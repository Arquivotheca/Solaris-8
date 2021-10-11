/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lwp.c	1.5	99/12/12 SMI"

#include "liblwp.h"
#include <ctype.h>
#include <procfs.h>
#include <sys/syscall.h>

int	liblwp_initialized;
int	nthreads = 1;
int	ndaemons = 0;

/*
 * concurrency is not used by the library.
 * It exists solely to hold and return the values set by
 * calls to thr_setconcurrency().  Protected by link_lock.
 */
static	int	concurrency;

mutex_t	link_lock = DEFAULTMUTEX;

mutex_t	stack_lock = DEFAULTMUTEX;
ulwp_t	*lwp_stacks = NULL;
ulwp_t	*lwp_laststack = NULL;
ulwp_t	*ulwp_freelist = NULL;
ulwp_t	*ulwp_lastfree = NULL;

pid_t _lpid;

ulwp_t	ulwp_one;
size_t	_lpagesize;
int	_lsemvaluemax;

#define	HASHTBLSZ	1024		/* must be a power of two */
#define	TIDHASH(tid)	(tid & hashmask)

/* initial allocations, just enough for one lwp */
int	hashtblsz = 1;			/* power of two (2**0) */
int	hashmask = 0;			/* hashtblsz - 1 */
mutex_t	init_hash_lock[1] = { DEFAULTMUTEX };
cond_t	init_hash_cond[1] = { DEFAULTCV };
ulwp_t	*init_hash_bucket[1] = { &ulwp_one };

/* hash table for all lwps */
hash_table_t hash_table = {
	init_hash_lock,			/* array of hash bucket locks */
	init_hash_cond,			/* array of hash bucket cvs */
	init_hash_bucket,		/* hash buckets */
	1				/* size of the hash table */
};

#define	ulwp_mutex(ulwp)	&hash_table.hash_lock[(ulwp)->ul_ix]
#define	ulwp_condvar(ulwp)	&hash_table.hash_cond[(ulwp)->ul_ix]

/* this space is allocated in finish_init() on the first thr_create() */
void *bigwad;
#define	WADSIZE	(HASHTBLSZ * \
	(sizeof (ulwp_t *) + sizeof (mutex_t) + sizeof (cond_t)))

ulwp_t	*all_lwps;

/* unimplemented TNF support */
void * (*thr_probe_getfunc_addr)(void) = NULL;

extern	void	finish_init(void);
extern	int	_thrp_continue(thread_t, int);

const char *panicstr;
ulwp_t *panic_thread;

static void
Abort(const char *msg)
{
	struct sigaction act;
	sigset_t sigmask;

	/* to help with core file debugging */
	panicstr = msg;
	panic_thread = curthread;

	/* set SIGABRT signal handler to SIG_DFL */
	(void) _memset(&act, 0, sizeof (act));
	act.sa_sigaction = SIG_DFL;
	(void) sigaction_internal(SIGABRT, &act, NULL);

	/* delete SIGABRT from the signal mask */
	(void) _sigemptyset(&sigmask);
	(void) _sigaddset(&sigmask, SIGABRT);
	(void) _sigprocmask(SIG_UNBLOCK, &sigmask, NULL);

	(void) __lwp_kill(curthread->ul_lwpid, SIGABRT); /* never returns */
	_exit(127);
}

/*
 * Write a panic message w/o grabbing any locks.
 * We have no idea what locks are held at this point.
 */
void
panic(const char *why)
{
	char msg[512];	/* no panic() message in the library is this long */
	size_t len1, len2;

	(void) _memset(msg, 0, sizeof (msg));
	(void) strcpy(msg, "*** libthread failure: ");
	len1 = strlen(msg);
	len2 = strlen(why);
	if (len1 + len2 >= sizeof (msg))
		len2 = sizeof (msg) - len1 - 1;
	(void) strncat(msg, why, len2);
	len1 = strlen(msg);
	if (msg[len1 - 1] != '\n')
		msg[len1++] = '\n';
	(void) _write(2, msg, len1);
	msg[len1] = '\0';
	Abort(msg);
}

/*
 * Insert the lwp into the hash table.
 */
void
hash_in_unlocked(ulwp_t *ulwp, int ix)
{
	ulwp->ul_hash = hash_table.hash_bucket[ix];
	hash_table.hash_bucket[ix] = ulwp;
	ulwp->ul_ix = ix;
}

void
hash_in(ulwp_t *ulwp)
{
	int ix = TIDHASH(ulwp->ul_lwpid);
	mutex_t *mp = &hash_table.hash_lock[ix];

	lmutex_lock(mp);
	hash_in_unlocked(ulwp, ix);
	lmutex_unlock(mp);
}

/*
 * Delete the lwp from the hash table.
 */
void
hash_out_unlocked(ulwp_t *ulwp, int ix)
{
	ulwp_t **ulwpp;

	for (ulwpp = &hash_table.hash_bucket[ix]; ulwp != *ulwpp;
	    ulwpp = &(*ulwpp)->ul_hash)
		;
	*ulwpp = ulwp->ul_hash;
	ulwp->ul_hash = NULL;
	ulwp->ul_ix = -1;
}

void
hash_out(ulwp_t *ulwp)
{
	int ix;

	if ((ix = ulwp->ul_ix) >= 0) {
		mutex_t *mp = &hash_table.hash_lock[ix];

		lmutex_lock(mp);
		hash_out_unlocked(ulwp, ix);
		lmutex_unlock(mp);
	}
}

void *
zmap(void *addr, size_t len, int prot, int flags)
{
	int fd;
	void *raddr;

#if defined(MAP_ANON)
	/* first try anonymous mapping */
	flags |= MAP_ANON;
	if ((raddr = _mmap(addr, len, prot, flags, -1, (off_t)0)) != MAP_FAILED)
		return (raddr);
	flags &= ~MAP_ANON;
#endif	/* MAP_ANON */

	/* fall back to mapping /dev/zero */
	if ((fd = _open("/dev/zero", O_RDWR, 0)) < 0)
		raddr = MAP_FAILED;
	else {
		raddr = _mmap(addr, len, prot, flags, fd, (off_t)0);
		(void) _close(fd);
	}

	return (raddr);
}

static void
ulwp_clean(ulwp_t *ulwp)
{
	ulwp->ul_rval = NULL;
	ulwp->ul_lwpid = 0;
	ulwp->ul_pri = 0;
	ulwp->ul_mappedpri = 0;
	ulwp->ul_policy = 0;
	ulwp->ul_pri_mapped = 0;
	ulwp->ul_mutator = 0;
	ulwp->ul_pleasestop = 0;
	ulwp->ul_stop = 0;
	ulwp->ul_wanted = 0;
	ulwp->ul_dead = 0;
	ulwp->ul_unwind = 0;
	ulwp->ul_detached = 0;
	ulwp->ul_was_detached = 0;
	ulwp->ul_stopping = 0;
	ulwp->ul_validregs = 0;
	ulwp->ul_critical = 0;
	ulwp->ul_cancelable = 0;
	ulwp->ul_cancel_pending = 0;
	ulwp->ul_cancel_disabled = 0;
	ulwp->ul_cancel_async = 0;
	ulwp->ul_save_async = 0;
	ulwp->ul_cursig = 0;
	ulwp->ul_created = 0;
	ulwp->ul_replace = 0;
	ulwp->ul_schedctl_called = 0;
	ulwp->ul_mutex = 0;
	ulwp->ul_rdlock = 0;
	ulwp->ul_wrlock = 0;
	ulwp->ul_errno = 0;
	ulwp->ul_clnup_hdr = NULL;
	ulwp->ul_schedctl = NULL;
	ulwp->ul_bindflags = 0;
	(void) _memset(&ulwp->ul_td_evbuf, 0, sizeof (ulwp->ul_td_evbuf));
	ulwp->ul_td_events_enable = 0;
	ulwp->ul_usropts = 0;
	ulwp->ul_startpc = NULL;
	ulwp->ul_wchan = NULL;
	ulwp->ul_epri = 0;
	ulwp->ul_emappedpri = 0;
	(void) _memset(ulwp->ul_resvtls, 0, sizeof (ulwp->ul_resvtls));
}

static int stackprot;

/*
 * Answer the question, "Is the lwp in question really dead?"
 * We must inquire of the operating system to be really sure
 * because the lwp may have called _lwp_exit() but it has not
 * yet completed the exit.
 */
static int
dead_and_buried(ulwp_t *ulwp)
{
	if (ulwp->ul_lwpid == (lwpid_t)(-1))
		return (1);
	if (ulwp->ul_dead && ulwp->ul_detached &&
	    __lwp_kill(ulwp->ul_lwpid, 0) == ESRCH) {
		ulwp->ul_lwpid = (lwpid_t)(-1);
		return (1);
	}
	if (ulwp->ul_dead && ulwp->ul_was_detached &&
	    __lwp_wait(ulwp->ul_lwpid, NULL) == 0) {
		ulwp->ul_was_detached = 0;
		ulwp->ul_detached = 1;
		ulwp->ul_lwpid = (lwpid_t)(-1);
		return (1);
	}
	return (0);
}

/*
 * Find an unused stack of the requested size
 * or create a new stack of the requested size.
 * Return a pointer to the ulwp_t structure referring to the stack, or NULL.
 * thr_exit() stores 1 in the ul_dead member.
 * thr_join() stores -1 in the ul_lwpid member.
 */
ulwp_t *
find_stack(size_t stksize, size_t guardsize)
{
	size_t mapsize;
	ulwp_t *prev;
	ulwp_t *ulwp;
	ulwp_t **ulwpp;
	void *stk;

	/*
	 * The stack is allocated PROT_READ|PROT_WRITE|PROT_EXEC
	 * unless overridden by the system's configuration.
	 */
	if (stackprot == 0) {	/* do this once */
		long lprot = _sysconf(_SC_STACK_PROT);
		if (lprot <= 0)
			lprot = (PROT_READ|PROT_WRITE|PROT_EXEC);
		stackprot = (int)lprot;
	}
	/*
	 * One megabyte stacks by default, but subtract off three pages,
	 * two for the system-created red zones and one for the ulwp_t.
	 */
	if (stksize == 0)
		stksize = DEFAULTSTACK - 3 * _lpagesize;

	/*
	 * Round up the mapping size to a multiple of pagesize
	 * and add a page for the ulwp_t structure.
	 * Note: mmap() provides at least one page of red zone
	 * so we deduct that from the value of guardsize.
	 */
	if (guardsize != 0)
		guardsize = ((guardsize + _lpagesize - 1) & -_lpagesize) -
			_lpagesize;
	mapsize = ((stksize + _lpagesize - 1) & -_lpagesize) +
		_lpagesize + guardsize;

	lmutex_lock(&stack_lock);
	for (prev = NULL, ulwpp = &lwp_stacks;
	    (ulwp = *ulwpp) != NULL;
	    prev = ulwp, ulwpp = &ulwp->ul_next) {
		if (ulwp->ul_mapsiz == mapsize &&
		    ulwp->ul_guardsize == guardsize &&
		    dead_and_buried(ulwp)) {
			/*
			 * The previous lwp is gone; reuse the stack.
			 * Remove it from the stack list.
			 */
			*ulwpp = ulwp->ul_next;
			ulwp->ul_next = NULL;
			if (ulwp == lwp_laststack)
				lwp_laststack = prev;
			hash_out(ulwp);
			lmutex_unlock(&stack_lock);
			ulwp_clean(ulwp);
			return (ulwp);
		}
	}
	lmutex_unlock(&stack_lock);

	/*
	 * Create a new stack.
	 */
	if ((stk = zmap(NULL, mapsize, stackprot, MAP_PRIVATE|MAP_NORESERVE))
	    != MAP_FAILED) {
		/*
		 * So now we've allocated our stack.
		 * We're going to reserve the top sizeof(ulwp_t) bytes.
		 */
		ASSERT(sizeof (ulwp_t) <= _lpagesize);
		ulwp = (ulwp_t *)((uintptr_t)stk + mapsize - _lpagesize);
		(void) _memset(ulwp, 0, _lpagesize);
		ulwp->ul_stk = stk;
		ulwp->ul_mapsiz = mapsize;
		ulwp->ul_guardsize = guardsize;
		ulwp->ul_stktop = (uintptr_t)stk + mapsize - _lpagesize;
		ulwp->ul_stksiz = stksize;
		ulwp->ul_ix = -1;
		if (guardsize) {	/* reserve the extra red zone */
			int fd = _open("/dev/null", O_RDWR, 0);
			if (fd >= 0) {
				(void) _mmap(stk, guardsize, PROT_NONE,
					MAP_PRIVATE, fd, (off_t)0);
				(void) _close(fd);
			}
		}
	}
	return (ulwp);
}

/*
 * Get a ulwp_t structure from the free list or allocate a new one.
 * Such ulwp_t's do not have a stack allocated by the library.
 */
ulwp_t *
ulwp_alloc()
{
	ulwp_t *prev;
	ulwp_t *ulwp;
	ulwp_t **ulwpp;

	lmutex_lock(&stack_lock);
	for (prev = NULL, ulwpp = &ulwp_freelist;
	    (ulwp = *ulwpp) != NULL;
	    prev = ulwp, ulwpp = &ulwp->ul_next) {
		if (dead_and_buried(ulwp)) {
			*ulwpp = ulwp->ul_next;
			ulwp->ul_next = NULL;
			if (ulwp == ulwp_lastfree)
				ulwp_lastfree = prev;
			hash_out(ulwp);
			lmutex_unlock(&stack_lock);
			ulwp_clean(ulwp);
			return (ulwp);
		}
	}
	lmutex_unlock(&stack_lock);

	if ((ulwp = malloc(sizeof (*ulwp))) != NULL)
		(void) _memset(ulwp, 0, sizeof (*ulwp));
	return (ulwp);
}

/*
 * Free a ulwp structure.
 * If there is an associated stack, put it on the stack list,
 * else put it on the ulwp free list.  Never call free() on it.
 */
void
ulwp_free(ulwp_t *ulwp)
{
	void *free_tsd;

	if (nthreads > 1)	/* when called from _postfork1_child() */
		lmutex_lock(&stack_lock);
	ulwp->ul_next = NULL;
	if (ulwp == &ulwp_one)
		/*EMPTY*/;
	else if (ulwp->ul_mapsiz != 0) {
		if (lwp_stacks == NULL)
			lwp_stacks = lwp_laststack = ulwp;
		else {
			lwp_laststack->ul_next = ulwp;
			lwp_laststack = ulwp;
		}
	} else {
		if (ulwp_freelist == NULL)
			ulwp_freelist = ulwp_lastfree = ulwp;
		else {
			ulwp_lastfree->ul_next = ulwp;
			ulwp_lastfree = ulwp;
		}
	}
	free_tsd = ulwp->ul_tsd.tsd;	/* remember for free()ing below */
	ulwp->ul_tsd.nkey = 0;
	ulwp->ul_tsd.tsd = NULL;
	if (nthreads > 1)
		lmutex_unlock(&stack_lock);
	/* we can call free() now; we are out of the critical section */
	if (free_tsd)
		liblwp_free(free_tsd);
}

void
liblwp_free(void *ptr)
{
	/*
	 * We cannot do free() while holding malloc_lock in fork1().
	 * If doing fork1(), keep track of it for later free().
	 */
	if (!doing_fork1)
		free(ptr);
	else {
		*(void **)ptr = fork1_freelist;
		fork1_freelist = ptr;
	}
}

/*
 * Find a named lwp and return a pointer to its hash list location.
 * On success, returns with the hash lock held.
 */
ulwp_t **
find_lwpp(thread_t tid)
{
	int ix = TIDHASH(tid);
	mutex_t *mp = &hash_table.hash_lock[ix];
	ulwp_t *ulwp;
	ulwp_t **ulwpp;

	if (tid == 0)
		return (NULL);

	lmutex_lock(mp);
	for (ulwpp = &hash_table.hash_bucket[ix]; (ulwp = *ulwpp) != NULL;
	    ulwpp = &ulwp->ul_hash) {
		if (ulwp->ul_lwpid == tid)
			return (ulwpp);
	}
	lmutex_unlock(mp);
	return (NULL);
}

/*
 * Grab the hash table lock for the specified lwp.
 */
void
ulwp_lock(ulwp_t *ulwp)
{
	lmutex_lock(ulwp_mutex(ulwp));
}

/*
 * Release the hash table lock for the specified lwp.
 */
void
ulwp_unlock(ulwp_t *ulwp)
{
	lmutex_unlock(ulwp_mutex(ulwp));
}

/*
 * Wake up all lwps waiting on this lwp for some reason.
 */
void
ulwp_broadcast(ulwp_t *ulwp)
{
	ASSERT(_mutex_held(ulwp_mutex(ulwp)));
	ulwp->ul_wanted = 0;
	(void) _cond_broadcast(ulwp_condvar(ulwp));
}

/*
 * Find a named lwp and return a pointer to it.
 * Returns with the hash lock held.
 */
ulwp_t *
find_lwp(thread_t tid)
{
	ulwp_t *ulwp = NULL;
	ulwp_t **ulwpp;

	if (curthread->ul_lwpid == tid)
		ulwp_lock(ulwp = curthread);
	else if ((ulwpp = find_lwpp(tid)) != NULL)
		ulwp = *ulwpp;

	if (ulwp && ulwp->ul_dead) {
		ulwp_unlock(ulwp);
		ulwp = NULL;
	}

	return (ulwp);
}

static int finish_init_called;

int
_thrp_create(void *stk, size_t stksize, void *(*func)(void *), void *arg,
	long flags, thread_t *new_thread, pri_t priority, int policy,
	size_t guardsize)
{
	ucontext_t ctx;
	uint_t lwp_flags = 0;
	thread_t tid;
	int ret;
	ulwp_t *ulwp;
	ulwp_t *self = curthread;

	if (finish_init_called == 0)
		finish_init();
	finish_init_called = 1;

	if (flags & (THR_DETACHED|THR_DAEMON))
		lwp_flags |= LWP_DETACHED;

	if (flags & THR_SUSPENDED)
		lwp_flags |= LWP_SUSPENDED;

	if (((stk || stksize) && stksize < MINSTACK) ||
	    priority < THREAD_MIN_PRIORITY || priority > THREAD_MAX_PRIORITY)
		return (EINVAL);

	if (stk == NULL) {
		if ((ulwp = find_stack(stksize, guardsize)) == NULL)
			return (ENOMEM);
		stksize = ulwp->ul_mapsiz - ulwp->ul_guardsize - _lpagesize;
	} else {
		/* initialize the private stack */
		if ((ulwp = ulwp_alloc()) == NULL)
			return (ENOMEM);
		ulwp->ul_stk = stk;
		ulwp->ul_stktop = (uintptr_t)stk + stksize;
		ulwp->ul_stksiz = stksize;
		ulwp->ul_ix = -1;
	}

	/* debugger support */
	ulwp->ul_usropts = flags;
	ulwp->ul_startpc = (void (*)())func;

	enter_critical();	/* setup_context() is critical on x86 (ldt) */

	setup_context(&ctx, (void(*)(void *))func, arg, ulwp,
		(caddr_t)ulwp->ul_stk + ulwp->ul_guardsize, stksize);
	(void) _sigprocmask(SIG_SETMASK, NULL, &ctx.uc_sigmask);
	if ((ret = __lwp_create(&ctx, lwp_flags | LWP_SUSPENDED, &tid)) != 0) {
		exit_critical();
		ulwp->ul_lwpid = (lwpid_t)(-1);
		ulwp->ul_dead = 1;
		ulwp->ul_detached = 1;
		ulwp_free(ulwp);
		return (ret);
	}
	if (new_thread)
		*new_thread = tid;
	if (lwp_flags & LWP_DETACHED)
		ulwp->ul_detached = 1;
	ulwp->ul_lwpid = tid;
	ulwp->ul_stop = TSTP_REGULAR;
	ulwp->ul_created = 1;
	ulwp->ul_policy = policy;
	ulwp->ul_pri = priority;

	lmutex_lock(&link_lock);
	ulwp->ul_forw = all_lwps;
	ulwp->ul_back = all_lwps->ul_back;
	ulwp->ul_back->ul_forw = ulwp;
	ulwp->ul_forw->ul_back = ulwp;
	hash_in(ulwp);
	nthreads++;
	if (flags & THR_DAEMON)
		ndaemons++;
	if (flags & THR_NEW_LWP)
		concurrency++;
	lmutex_unlock(&link_lock);

	if (__td_event_report(self, TD_CREATE)) {
		self->ul_td_evbuf.eventnum = TD_CREATE;
		self->ul_td_evbuf.eventdata = (void *)tid;
		tdb_event_create();
	}
	if (!(lwp_flags & LWP_SUSPENDED)) {
		ulwp->ul_validregs = 0;
		ulwp->ul_created = 0;
		(void) _thrp_continue(tid, TSTP_REGULAR);
	}

	exit_critical();
	return (0);
}

#pragma weak thr_create = _thr_create
#pragma weak _liblwp_thr_create = _thr_create
int
_thr_create(void *stk, size_t stksize, void *(*func)(void *), void *arg,
	long flags, thread_t *new_thread)
{
	return (_thrp_create(stk, stksize, func, arg, flags, new_thread,
		curthread->ul_pri, curthread->ul_policy, _lpagesize));
}

/*
 * A special cancelation cleanup hook for DCE.
 * cleanuphndlr, when it is not NULL, will contain a callback
 * function to be called before a thread is terminated in
 * _thr_exit() as a result of being cancelled.
 */
static void (*cleanuphndlr)(void) = NULL;

/*
 * _pthread_setcleanupinit: sets the cleanup hook.
 */
int
_pthread_setcleanupinit(void (*func)(void))
{
	cleanuphndlr = func;
	return (0);
}

void
_thrp_exit()
{
	ulwp_t *self = curthread;
	ulwp_t *replace = NULL;
	int do_exit = 0;

	tsd_exit();
	_destroy_resv_tls();

	ASSERT(self->ul_tsd.tsd == NULL && self->ul_tsd.nkey == 0);

	if (self->ul_mapsiz && !(self->ul_detached | self->ul_was_detached)) {
		/*
		 * We want to free the stack for reuse but must keep
		 * the ulwp_t struct for the benefit of thr_join().
		 * For this purpose we allocate a replacement ulwp_t.
		 * We won't need the trailing deferred signal members.
		 */
		replace = malloc(sizeof (*self) -
		    sizeof (self->ul_prevmask) - sizeof (self->ul_siginfo) -
		    sizeof (self->ul_resvtls));
	}

	if (__td_event_report(self, TD_DEATH)) {
		self->ul_td_evbuf.eventnum = TD_DEATH;
		tdb_event_death();
	}

	/* block all signals to finish exiting */
	(void) _sigprocmask(SIG_SETMASK, &fillset, NULL);
	enter_critical();
	lmutex_lock(&link_lock);
	ulwp_free(self);
	(void) ulwp_lock(self);
	/*
	 * Make all_lwps NULL when the last lwp exits.
	 * Debuggers examine all_lwps and nthreads via libthread_db.
	 */
	if (all_lwps == self)
		all_lwps = self->ul_forw;
	if (all_lwps == self)
		all_lwps = NULL;
	else {
		self->ul_forw->ul_back = self->ul_back;
		self->ul_back->ul_forw = self->ul_forw;
	}
	nthreads--;
	if (self->ul_usropts & THR_DAEMON)
		ndaemons--;
	if (self->ul_usropts & THR_NEW_LWP)
		concurrency--;
	self->ul_forw = self->ul_back = NULL;
	self->ul_dead = 1;
	/*
	 * Prevent any more references to the schedctl data.
	 * We are exiting and continue_fork() may not find us.
	 */
	self->ul_schedctl = NULL;
	self->ul_schedctl_called = 1;
	self->ul_pleasestop = 0;
	if (replace != NULL) {
		int ix = self->ul_ix;		/* the hash index */
		(void) _memcpy(replace, self, sizeof (*self) -
		    sizeof (self->ul_prevmask) - sizeof (self->ul_siginfo) -
		    sizeof (self->ul_resvtls));
		replace->ul_next = NULL;	/* clone not on stack list */
		replace->ul_mapsiz = 0;		/* allows clone to be freed */
		replace->ul_replace = 1;	/* requires clone to be freed */
		hash_out_unlocked(self, ix);
		hash_in_unlocked(replace, ix);
		self->ul_detached = 1;		/* this frees the stack */
		lwp_setprivate(self = replace);
		/*
		 * Having just changed the address of curthread, we
		 * must reset the ownership of the locks we hold so
		 * that assertions will not fire when we release them.
		 */
		set_mutex_owner(&link_lock);
		set_mutex_owner(ulwp_mutex(self));
	}
	if (self->ul_wanted)	/* notify everyone waiting for this thread */
		ulwp_broadcast(self);
	(void) ulwp_unlock(self);
	do_exit = (nthreads == ndaemons);
	lmutex_unlock(&link_lock);

	ASSERT(self->ul_critical == 1);
	if (do_exit)	/* last non-daemon thread is exiting */
		exit(0);
	_lwp_terminate();	/* never returns */
	panic("_thrp_exit(): _lwp_terminate() returned");
}

void
_thr_exit_common(void *status, int posix)
{
	ulwp_t *self = curthread;

	ASSERT(self->ul_critical == 0);

	/* cancellation is disabled */
	self->ul_cancel_disabled = 1;
	self->ul_cancel_async = 0;
	self->ul_save_async = 0;
	self->ul_cancelable = 0;

	/* special DCE cancellation cleanup hook */
	if (self->ul_cancel_pending &&
	    status == PTHREAD_CANCELED &&
	    cleanuphndlr != NULL)
		(*cleanuphndlr)();

	self->ul_rval = status;

	/*
	 * If thr_exit is being called from the places where
	 * C++ destructors are to be called such as cancellation
	 * points, then set this flag. It is checked in _t_cancel()
	 * to decide whether _ex_unwind() is to be called or not.
	 */
	if (posix)
		self->ul_unwind = 1;

	/*
	 * _tcancel_all() will eventually call _thrp_exit().
	 * It never returns.
	 */
	_tcancel_all(NULL);
	panic("_thr_exit_common(): _tcancel_all() returned");
}

/*
 * Called when a thread returns from its start function.
 */
void
_thr_terminate(void *status)
{
	_thr_exit_common(status, _libpthread_loaded);
}

#pragma weak thr_exit = _thr_exit
#pragma weak _liblwp_thr_exit = _thr_exit
void
_thr_exit(void *status)
{
	_thr_exit_common(status, 0);
}

#pragma weak pthread_exit = _pthread_exit
#pragma weak _liblwp_pthread_exit = _pthread_exit
void
_pthread_exit(void *status)
{
	_thr_exit_common(status, 1);
}

#pragma weak pthread_detach = _thr_detach
#pragma weak _pthread_detach = _thr_detach
#pragma weak _liblwp_pthread_detach = _thr_detach
int
_thr_detach(thread_t tid)
{
	ulwp_t **ulwpp;
	ulwp_t *ulwp;
	int error = 0;

	if ((ulwpp = find_lwpp(tid)) == NULL)
		error = ESRCH;
	else {
		ulwp = *ulwpp;
		if (ulwp->ul_detached | ulwp->ul_was_detached)
			error = EINVAL;
		else
			ulwp->ul_was_detached = 1;
		ulwp_unlock(ulwp);
	}
	return (error);
}

int
_thrp_join(thread_t tid, thread_t *departed, void **status)
{
	mutex_t *mp;
	void *rval;
	thread_t found;
	ulwp_t *ulwp;
	ulwp_t **ulwpp;
	int detached;
	int error;

	if (tid != 0) {
		if ((ulwpp = find_lwpp(tid)) == NULL)
			return (ESRCH);
		ulwp = *ulwpp;
		detached = (ulwp->ul_detached | ulwp->ul_was_detached);
		ulwp_unlock(ulwp);
		if (detached)
			return (EINVAL);
	}

again:
	error = lwp_wait(tid, &found);
	if (error)
		return (error);
	/*
	 * We must hold stack_lock to avoid a race condition with find_stack().
	 */
	lmutex_lock(&stack_lock);
	if ((ulwpp = find_lwpp(found)) == NULL) {
		lmutex_unlock(&stack_lock);
		goto again;
	}
	/*
	 * Remove ulwp from the hash table.
	 */
	ulwp = *ulwpp;
	*ulwpp = ulwp->ul_hash;
	ulwp->ul_hash = NULL;
	ASSERT(ulwp->ul_dead);
	/*
	 * We can't call ulwp_unlock(ulwp) after we set
	 * ulwp->ul_ix = -1 so we have to get a pointer to the
	 * ulwp's hash table mutex now in order to unlock it below.
	 */
	mp = ulwp_mutex(ulwp);
	ulwp->ul_ix = -1;
	rval = ulwp->ul_rval;
	ulwp->ul_lwpid = (lwpid_t)(-1);
	detached = ulwp->ul_was_detached;
	lmutex_unlock(mp);
	lmutex_unlock(&stack_lock);
	if (ulwp->ul_replace)
		liblwp_free(ulwp);
	if (tid == 0 && detached)
		goto again;

	if (departed != NULL)
		*departed = found;
	if (status != NULL)
		*status = rval;
	return (0);
}

#pragma weak	thr_join		= _thr_join
#pragma weak	_liblwp_thr_join	= _thr_join
int
_thr_join(thread_t tid, thread_t *departed, void **status)
{
	int error = _thrp_join(tid, departed, status);
	return ((error == EINVAL)? ESRCH : error);
}

/*
 * pthread_join() differs from Solaris thr_join():
 * It does not return the departed thread's id
 * and hence does not have a "departed" argument.
 * It returnd EINVAL if tid refers to a detached thread.
 */
#pragma weak	pthread_join 		= _pthread_join
#pragma weak	_liblwp_pthread_join 	= _pthread_join
int
_pthread_join(pthread_t tid, void **status)
{
	return ((tid == 0)? ESRCH : _thrp_join(tid, NULL, status));
}

void
_liblwp_prepare_atfork(void)
{
	/*
	 * This stops all threads but ourself, leaving them stopped
	 * outside of any critical regions in the library.
	 * Thus, we are assured that no library locks are held
	 * while we invoke fork1() from the current thread.
	 */
	suspend_fork();
}

void
_liblwp_parent_atfork(void)
{
	/*
	 * This restarts all threads that were stopped for fork1().
	 */
	continue_fork(1);
}

void
_liblwp_child_atfork(void)
{
	/*
	 * This resets the library's data structures to reflect one thread.
	 */
	_postfork1_child();
}

extern void (*__thr_door_server_func)(door_info_t *);
extern void (*__door_server_func)(door_info_t *);

void
door_create_server(door_info_t *dip)
{
	if (__door_server_func)
		(*__door_server_func)(dip);
	else
		_thr_exit(NULL);
}

extern void _set_libc_interface(void);
extern void _unset_libc_interface(void);
extern void _set_rtld_interface(void);
extern void _unset_rtld_interface(void);
extern void _libc_prepare_atfork(void);
extern void _libc_parent_atfork(void);
extern void _libc_child_atfork(void);

/*
 * We avoid using malloc() until the second phase of initialization,
 * in finish_init().  The application may have its own version of
 * malloc() which requires specialized initialization.  We hope that
 * everything we call here exists in libc and does not call malloc().
 */
#pragma init(_liblwp_init)
void
_liblwp_init()
{
	ucontext_t ctx;
	struct rlimit rl;

	__thr_door_server_func = door_create_server;
	_lpagesize = _sysconf(_SC_PAGESIZE);
	_lsemvaluemax = _sysconf(_SC_SEM_VALUE_MAX);
	_lpid = _getpid();
	signal_init();
	tsd_init();

	/*
	 * Perform minimal lwp initialization;
	 * just enough for things to work with one lwp.
	 */
	ulwp_one.ul_lwpid = 1;
	(void) _getcontext(&ctx);
	(void) _getrlimit(RLIMIT_STACK, &rl);
	ulwp_one.ul_stktop =
		(uintptr_t)ctx.uc_stack.ss_sp + ctx.uc_stack.ss_size;
	ulwp_one.ul_stksiz = rl.rlim_cur;
	ulwp_one.ul_stk = (caddr_t)(ulwp_one.ul_stktop - ulwp_one.ul_stksiz);
	ulwp_one.ul_forw = ulwp_one.ul_back = all_lwps = &ulwp_one;
	ulwp_one.ul_hash = NULL;
	ulwp_one.ul_ix = 0;
	nthreads = 1;

	/* This is what makes curthread != NULL for lwp #1 */
#if defined(__i386)
	ulwp_one.ul_gs = __setupgs(&ulwp_one);
	set_gs(ulwp_one.ul_gs);
#else
	lwp_setprivate(&ulwp_one);
#endif
	liblwp_initialized = 1;
	mutex_setup();

	_set_libc_interface();	/* notify libc */
}

static int
envvar(const char *ename, int limit)
{
	int val = -1;

	if ((ename = getenv(ename)) != NULL) {
		int c;
		for (val = 0; (c = *ename) != '\0'; ename++) {
			if (!isdigit(c)) {
				val = -1;
				break;
			}
			val = val * 10 + (c - '0');
			if (val > limit) {
				val = limit;
				break;
			}
		}
	}
	return (val);
}

/*
 * This is called when we are about to become multi-threaded,
 * that is, on the first call to thr_create().
 */
void
finish_init()
{
	static mutex_t def_mutex = DEFAULTMUTEX;
	static cond_t def_cond = DEFAULTCV;
	struct sigaction act;
	int i;

	if ((bigwad = zmap(NULL, WADSIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE))
	    == MAP_FAILED)
		panic("cannot allocate thread tables");

	/* the order here is important due to varying alignments */
	hash_table.hash_lock = (mutex_t *)bigwad;
	hash_table.hash_cond = (cond_t *)(hash_table.hash_lock + HASHTBLSZ);
	hash_table.hash_bucket = (ulwp_t **)(hash_table.hash_cond + HASHTBLSZ);
	hash_table.hash_size = HASHTBLSZ;

	hashtblsz = HASHTBLSZ;
	hashmask = HASHTBLSZ - 1;
	for (i = 0; i < HASHTBLSZ; i++) {
		hash_table.hash_lock[i] = def_mutex;
		hash_table.hash_cond[i] = def_cond;
		hash_table.hash_bucket[i] = NULL;
	}
	hash_in(&ulwp_one);

	_set_rtld_interface();		/* notify ld.so.1 */
	(void) _pthread_atfork(_libc_prepare_atfork,
	    _libc_parent_atfork, _libc_child_atfork);
	(void) _lpthread_atfork(_liblwp_prepare_atfork,
	    _liblwp_parent_atfork, _liblwp_child_atfork);

	/* set up the SIGCANCEL handler for cancellation */
	act.sa_flags = SA_SIGINFO;
	act.sa_mask = fillset;
	act.sa_sigaction = _sigcancel;
	(void) sigaction_internal(SIGCANCEL, &act, NULL);

	/* set SIGWAITING and SIGLWP signal handlers to SIG_DFL */
	(void) _memset(&act, 0, sizeof (act));
	act.sa_sigaction = SIG_DFL;
	(void) sigaction_internal(SIGWAITING, &act, NULL);
	(void) sigaction_internal(SIGLWP, &act, NULL);

	if ((ncpus = _sysconf(_SC_NPROCESSORS_ONLN)) <= 0)
		ncpus = 1;
	if ((i = envvar("MUTEX_ADAPTIVE_SPIN", 1000000)) >= 0)
		mutex_adaptive_spin = i;
}

/*
 * This is called from fork1() in the child.
 * Reset our data structures to reflect one lwp.
 */
void
_postfork1_child()
{
	ulwp_t *self = curthread;
	ulwp_t *next = self->ul_forw;
	ulwp_t *ulwp;
	int i;

	self->ul_forw = self->ul_back = all_lwps = self;
	nthreads = 1;
	for (i = 0; i < hashtblsz; i++)
		hash_table.hash_bucket[i] = NULL;
	self->ul_lwpid = 1;

	/* inline hash_in(self) */
	i = TIDHASH(1);
	self->ul_hash = NULL;
	hash_table.hash_bucket[i] = self;
	self->ul_ix = i;

	/*
	 * All lwps except ourself are gone.  Mark them so.
	 * Since we are single-threaded, no locks are required here.
	 */
	for (ulwp = next; ulwp != self; ulwp = next) {
		next = ulwp->ul_forw;
		ulwp->ul_dead = 1;
		ulwp->ul_lwpid = (lwpid_t)(-1);
		ulwp->ul_hash = NULL;
		ulwp->ul_forw = ulwp->ul_back = NULL;
		ulwp->ul_ix = -1;
		ulwp_free(ulwp);
	}
}

#ifdef EVER_DELETED
/*
 * This is called on exit() and when we are dlclose()d
 * We are never deleted so this is never called.
 */
#pragma fini(_liblwp_fini)
void
_liblwp_fini()
{
	ulwp_t *self = curthread;

	/*
	 * Destroy our data space only if we are single-threaded now.
	 */
	lmutex_lock(&link_lock);
	if (self->ul_forw != self)
		lmutex_unlock(&link_lock);
	else {
		lmutex_unlock(&link_lock);
		if (bigwad == NULL)	/* we never became multi-threaded */
			_unset_libc_interface();
		else {
			_unset_rtld_interface();
			_unset_libc_interface();
			(void) munmap(bigwad, WADSIZE);
			bigwad = NULL;
			hash_table.hash_lock = init_hash_lock;
			hash_table.hash_cond = init_hash_cond;
			hash_table.hash_bucket = init_hash_bucket;
			hash_table.hash_size = 1;
			hashtblsz = 1;
			hashmask = 0;
			hash_table.hash_bucket[0] = self;
			self->ul_hash = NULL;
			self->ul_ix = 0;
		}
	}
}
#endif	/* EVER_DELETED */

#pragma weak thr_setprio = _thr_setprio
#pragma weak _liblwp_thr_setprio = _thr_setprio
int
_thr_setprio(thread_t tid, int priority)
{
	ulwp_t *ulwp;
	int error = 0;

	if (priority < THREAD_MIN_PRIORITY ||
	    priority > THREAD_MAX_PRIORITY)
		error = EINVAL;
	else if ((ulwp = find_lwp(tid)) == NULL)
		error = ESRCH;
	else {
		ulwp->ul_policy = SCHED_OTHER;
		ulwp->ul_pri = (pri_t)priority;
		ulwp_unlock(ulwp);
	}
	return (error);
}

#pragma weak thr_getprio = _thr_getprio
#pragma weak _liblwp_thr_getprio = _thr_getprio
int
_thr_getprio(thread_t tid, int *priority)
{
	ulwp_t *ulwp;
	int error = 0;

	if ((ulwp = find_lwp(tid)) == NULL)
		error = ESRCH;
	else {
		*priority = ulwp->ul_pri;
		ulwp_unlock(ulwp);
	}
	return (error);
}

#pragma weak _liblwp_thr_errnop = _thr_errnop
int *
_thr_errnop()
{
	return (&curthread->ul_errno);
}

lwpid_t
lwp_self(void)
{
	return (__lwp_self());
}

#pragma weak thr_self = _thr_self
#pragma weak _liblwp_thr_self = _thr_self
#pragma weak pthread_self = _thr_self
#pragma weak _pthread_self = _thr_self
#pragma weak _liblwp_pthread_self = _thr_self
thread_t
_thr_self()
{
	if (!liblwp_initialized)
		return (1);
	return (curthread->ul_lwpid);
}

#pragma weak thr_main = _thr_main
#pragma weak _liblwp_thr_main = _thr_main
int
_thr_main()
{
	if (!liblwp_initialized)
		return (1);
	return (curthread->ul_lwpid == 1);
}

#pragma weak thr_sigsetmask = _thr_sigsetmask
#pragma weak _liblwp_thr_sigsetmask = _thr_sigsetmask
#pragma weak pthread_sigmask = _thr_sigsetmask
#pragma weak _pthread_sigmask = _thr_sigsetmask
#pragma weak _liblwp_pthread_sigmask = _thr_sigsetmask
int
_thr_sigsetmask(int how, const sigset_t *set, sigset_t *oset)
{
	sigset_t lset;
	sigset_t *lsetp = NULL;
	int rv;

	if (set != NULL) {
		lset = *set;
		lsetp = &lset;
		delete_reserved_signals(lsetp);
	}
	if ((rv = _sigprocmask(how, lsetp, oset)) == -1)
		rv = errno;
	if (oset != NULL)
		delete_reserved_signals(oset);
	return (rv);
}

/*
 * _thr_libthread() is used to identify the link order of libc.so vs.
 * libthread.so.  There is a copy of this routine in both libraries:
 *
 *	libc:_thr_libthread():		returns 0
 *	libthread:_thr_libthread():	returns 1
 *
 * A call to this routine can be used to determine whether the libc threads
 * interface needs to be initialized or not.
 */
int
_thr_libthread()
{
	return (1);
}

int
_thrp_stksegment(ulwp_t *ulwp, stack_t *stk)
{
	stk->ss_sp = (void *)ulwp->ul_stktop;
	stk->ss_size = ulwp->ul_stksiz;
	stk->ss_flags = 0;
	return (0);
}

#pragma weak thr_stksegment = _thr_stksegment
#pragma weak _liblwp_thr_stksegment = _thr_stksegment
int
_thr_stksegment(stack_t *stk)
{
	return (_thrp_stksegment(curthread, stk));
}

static void
lwp_suspend(thread_t tid)
{
	int error;

	while ((error = __lwp_suspend(tid)) != 0) {
		if (error != EINTR)
			panic("__lwp_suspend()");
		error = 0;
	}
}

int
force_continue(ulwp_t *ulwp)
{
	int error = 0;

	ASSERT(_mutex_held(ulwp_mutex(ulwp)));

	for (;;) {
		error = __lwp_continue(ulwp->ul_lwpid);
		if (error != 0 && error != EINTR)
			break;
		error = 0;
		if (ulwp->ul_stopping)		/* he is stopping himself */
			_yield();		/* give him a chance to run */
		if (!ulwp->ul_stopping)		/* he is running now */
			break;			/* so we are done */
		/*
		 * He is marked as being in the process of stopping
		 * himself.  Loop around and continue him again.
		 * He may not have been stopped the first time.
		 */
	}

	return (error);
}

/*
 * Move an lwp that has been stopped by lwp_suspend(), but has not yet
 * been marked as stopped, to a safe point, that is, to a point where
 * ul_critical is zero.
 * Side-effect:
 *	The ulwp_lock() is dropped as with ulwp_unlock().
 * If the function returns (1), then the link_lock is dropped as well.
 */
int
move_to_safe(ulwp_t *ulwp, uchar_t whystopped, int link_held)
{
	cond_t *cvp = ulwp_condvar(ulwp);
	mutex_t *mp = ulwp_mutex(ulwp);
	thread_t tid = ulwp->ul_lwpid;
	int ix = ulwp->ul_ix;
	int rv = 0;

	ASSERT(ulwp->ul_stop == 0);
	ASSERT(_mutex_held(mp));
	if (ulwp->ul_critical == 0) {	/* already safe */
		ulwp->ul_stop |= whystopped;
		if (ulwp->ul_wanted)
			ulwp_broadcast(ulwp);
	} else {
		if (link_held) {
			rv = 1;
			lmutex_unlock(&link_lock);
		}
		/*
		 * Setting ul_pleasestop causes the target thread to stop
		 * itself in _thrp_suspend(), below, after we drop its lock.
		 * However, it may disappear by calling thr_exit() so we
		 * cannot rely on the ulwp pointer after dropping the lock.
		 * Instead, we search the hash table to find it again.
		 */
		ulwp->ul_pleasestop |= whystopped;
		if (force_continue(ulwp) == 0) {
			while (ulwp && !ulwp->ul_dead && !ulwp->ul_stop) {
				ulwp->ul_wanted = 1;
				(void) _cond_wait(cvp, mp);
				for (ulwp = hash_table.hash_bucket[ix];
				    ulwp != NULL; ulwp = ulwp->ul_hash) {
					if (ulwp->ul_lwpid == tid)
						break;
				}
			}
		}
		ASSERT(ulwp == NULL || ulwp->ul_dead ||
		    (ulwp->ul_stop && ulwp->ul_stopping));
	}

	/*
	 * Stop using schedctl mappings while we fork.
	 * We will resume using them after fork.
	 */
	if (whystopped == TSTP_FORK && ulwp != NULL) {
		ulwp->ul_schedctl_called = 1;
		ulwp->ul_schedctl = NULL;
	}
	lmutex_unlock(mp);
	return (rv);
}

int
_thrp_suspend(thread_t tid, uchar_t whystopped)
{
	ulwp_t *ulwp;
	int error = 0;

	ASSERT((whystopped & (TSTP_REGULAR|TSTP_MUTATOR|TSTP_FORK)) != 0);
	ASSERT((whystopped & ~(TSTP_REGULAR|TSTP_MUTATOR|TSTP_FORK)) == 0);

	if ((ulwp = find_lwp(tid)) == NULL)
		error = ESRCH;
	else if (whystopped == TSTP_MUTATOR && !ulwp->ul_mutator) {
		ulwp_unlock(ulwp);
		error = ESRCH;
	} else {
		if (ulwp->ul_stop) {	/* already stopped */
			ulwp->ul_stop |= whystopped;
			if (ulwp->ul_wanted)
				ulwp_broadcast(ulwp);
			/*
			 * Stop using schedctl mappings while we fork.
			 * We will resume using them after fork.
			 */
			if (whystopped == TSTP_FORK) {
				ulwp->ul_schedctl_called = 1;
				ulwp->ul_schedctl = NULL;
			}
			ulwp_unlock(ulwp);
		} else {
			if (ulwp == curthread) {
				/*
				 * We must not take a signal until we return
				 * from lwp_suspend() and clear ul_stopping.
				 * This is to guard against siglongjmp().
				 */
				enter_critical();
				_save_nv_regs(&ulwp->ul_savedregs);
				_flush_windows();	/* sparc */
				ulwp->ul_validregs = 1;
				ulwp->ul_pleasestop = 0;
				ulwp->ul_stopping = 1;
				ulwp->ul_stop |= whystopped;
				if (ulwp->ul_wanted)
					ulwp_broadcast(ulwp);
				if (whystopped == TSTP_FORK) {
					ulwp->ul_schedctl_called = 1;
					ulwp->ul_schedctl = NULL;
				}
				ulwp_unlock(ulwp);
				lwp_suspend(tid);	/* stop ourself */
				/*
				 * Somebody else continued us.
				 * We can't grab ulwp_lock(curthread)
				 * until after clearing ul_stopping.
				 * force_continue() relies on this.
				 */
				ulwp->ul_stopping = 0;
				ulwp->ul_validregs = 0;
				ulwp_lock(ulwp);
				if (ulwp->ul_wanted)
					ulwp_broadcast(ulwp);
				ulwp_unlock(ulwp);
				exit_critical();
			} else {
				/*
				 * After suspending the other thread,
				 * move it out of a critical section.
				 * Also, deal with the schedctl mappings.
				 */
				lwp_suspend(tid);
				(void) move_to_safe(ulwp, whystopped, 0);
			}
		}
	}

	return (error);
}

/*
 * Suspend all lwps other than ourself in preparation for fork()/fork1().
 */
void
suspend_fork()
{
	ulwp_t *self = curthread;
	ulwp_t *ulwp;

top:
	lmutex_lock(&link_lock);

	for (ulwp = self->ul_forw; ulwp != self; ulwp = ulwp->ul_forw) {
		ulwp_lock(ulwp);
		if (ulwp->ul_stop) {	/* already stopped */
			ulwp->ul_stop |= TSTP_FORK;
			if (ulwp->ul_wanted)
				ulwp_broadcast(ulwp);
			/*
			 * Stop using schedctl mappings while we fork.
			 * We will resume using them after fork.
			 */
			ulwp->ul_schedctl_called = 1;
			ulwp->ul_schedctl = NULL;
			ulwp_unlock(ulwp);
		} else {
			lwp_suspend(ulwp->ul_lwpid);
			/*
			 * Move the stopped lwp out of a critical section.
			 * This also deals with the schedctl mappings.
			 */
			if (move_to_safe(ulwp, TSTP_FORK, 1))
				goto top;
		}
	}

	lmutex_unlock(&link_lock);
}

/* ARGSUSED */
void
continue_fork(int fork1)
{
	ulwp_t *self = curthread;
	ulwp_t *ulwp;

	lmutex_lock(&link_lock);

	for (ulwp = self->ul_forw; ulwp != self; ulwp = ulwp->ul_forw) {
		mutex_t *mp = ulwp_mutex(ulwp);
		lmutex_lock(mp);
		/*
		 * Reenable schedctl.  The thread will reestablish
		 * the schedctl mapping for itself when it needs to.
		 * A thread that is exiting must not use schedctl.
		 */
		ulwp->ul_schedctl_called = ulwp->ul_dead;
		ulwp->ul_schedctl = NULL;
		ASSERT(ulwp->ul_stop & TSTP_FORK);
		ulwp->ul_stop &= ~TSTP_FORK;
		if (ulwp->ul_wanted)
			ulwp_broadcast(ulwp);
		if (!ulwp->ul_stop)
			(void) force_continue(ulwp);
		lmutex_unlock(mp);
	}

	lmutex_unlock(&link_lock);
}

int
_thrp_continue(thread_t tid, int whystopped)
{
	ulwp_t *ulwp;
	mutex_t *mp;
	int error = 0;

	ASSERT(whystopped == TSTP_REGULAR ||
	    whystopped == TSTP_MUTATOR ||
	    whystopped == TSTP_FORK);

	if ((ulwp = find_lwp(tid)) == NULL)
		return (ESRCH);

	mp = ulwp_mutex(ulwp);
	if ((whystopped == TSTP_MUTATOR && !ulwp->ul_mutator)) {
		error = EINVAL;
	} else if (ulwp->ul_stop & whystopped) {
		ulwp->ul_stop &= ~whystopped;
		if (ulwp->ul_wanted)
			ulwp_broadcast(ulwp);
		if (!ulwp->ul_stop) {
			if (whystopped == TSTP_REGULAR && ulwp->ul_created) {
				ulwp->ul_validregs = 0;
				ulwp->ul_created = 0;
			}
			(void) force_continue(ulwp);
		}
	}

	lmutex_unlock(mp);
	return (error);
}

#pragma weak thr_suspend = _thr_suspend
#pragma weak _liblwp_thr_suspend = _thr_suspend
int
_thr_suspend(thread_t tid)
{
	return (_thrp_suspend(tid, TSTP_REGULAR));
}

#pragma weak thr_continue = _thr_continue
#pragma weak _liblwp_thr_continue = _thr_continue
int
_thr_continue(thread_t tid)
{
	return (_thrp_continue(tid, TSTP_REGULAR));
}

#pragma weak thr_yield = _thr_yield
#pragma weak _liblwp_thr_yield = _thr_yield
void
_thr_yield()
{
	_yield();
}

#pragma weak thr_kill = _thr_kill
#pragma weak _liblwp_thr_kill = _thr_kill
#pragma weak pthread_kill = _thr_kill
#pragma weak _pthread_kill = _thr_kill
#pragma weak _liblwp_pthread_kill = _thr_kill
int
_thr_kill(thread_t tid, int sig)
{
	switch (sig) {
	case SIGWAITING:
	case SIGLWP:
	case SIGCANCEL:
		return (EINVAL);
	}
	return (__lwp_kill(tid, sig));
}

/*
 * Exit a critical section, take deferred actions if necessary.
 */
void
exit_critical()
{
	ulwp_t *self = curthread;

	ASSERT(self->ul_critical > 0);
	if (--self->ul_critical == 0 && !self->ul_dead) {
		if (self->ul_cursig) {
			/*
			 * Clear ul_cursig before proceeding.
			 * This protects us from the dynamic linker's
			 * calls to bind_guard()/bind_clear() in the
			 * event that it is invoked to resolve a symbol
			 * like take_deferred_signal() below.
			 */
			int sig = self->ul_cursig;

			self->ul_cursig = 0;
			take_deferred_signal(sig);
			ASSERT(self->ul_cursig == 0);
		}
		if (self->ul_pleasestop)
			(void) _thrp_suspend(self->ul_lwpid,
				self->ul_pleasestop);
	}
}

int
_liblwp_bind_guard(int bindflag)
{
	ulwp_t *self = curthread;

	if ((self->ul_bindflags & bindflag) == bindflag)
		return (0);
	enter_critical();
	self->ul_bindflags |= bindflag;
	return (1);
}

int
_liblwp_bind_clear(int bindflag)
{
	ulwp_t *self = curthread;

	if ((self->ul_bindflags & bindflag) == 0)
		return (self->ul_bindflags);
	self->ul_bindflags &= ~bindflag;
	exit_critical();
	return (self->ul_bindflags);
}

/*
 * Internally to the library, almost all mutex lock/unlock actions
 * go through these lmutex_ functions, to protect critical regions.
 */
void
lmutex_lock(mutex_t *mp)
{
	enter_critical();
	(void) _mutex_lock(mp);
}

void
lmutex_unlock(mutex_t *mp)
{
	(void) _mutex_unlock(mp);
	exit_critical();
}

#pragma weak	pthread_getconcurrency		= _thr_getconcurrency
#pragma weak	_pthread_getconcurrency		= _thr_getconcurrency
#pragma weak	_liblwp_pthread_getconcurrency	= _thr_getconcurrency
#pragma weak	thr_getconcurrency		= _thr_getconcurrency
#pragma weak	_liblwp_thr_getconcurrency	= _thr_getconcurrency
int
_thr_getconcurrency()
{
	return (concurrency);
}

#pragma weak	pthread_setconcurrency		= _thr_setconcurrency
#pragma weak	_pthread_setconcurrency		= _thr_setconcurrency
#pragma weak	_liblwp_pthread_setconcurrency	= _thr_setconcurrency
#pragma weak	thr_setconcurrency		= _thr_setconcurrency
#pragma weak	_liblwp_thr_setconcurrency	= _thr_setconcurrency
int
_thr_setconcurrency(int new_level)
{
	if (new_level < 0)
		return (EINVAL);
	if (new_level > 65536)		/* 65536 is totally arbitrary */
		return (EAGAIN);
	lmutex_lock(&link_lock);
	concurrency = new_level;
	lmutex_unlock(&link_lock);
	return (0);
}

#pragma weak	thr_min_stack			= _thr_min_stack
#pragma weak	_liblwp_thr_min_stack		= _thr_min_stack
#pragma weak	__pthread_min_stack		= _thr_min_stack
size_t
_thr_min_stack(void)
{
	return (MINSTACK);
}

int
__thr_door_unbind(void)
{
	extern int __door_unbind(void);
	return (__door_unbind());
}

/*
 * All of the following, up to the __assert() stuff, implements
 * the private interfaces to java for garbage collection.
 * It is no longer used, at least by java 1.2.
 * It can all go away once all old JVMs have disappeared.
 */

int	suspendingallmutators;	/* when non-zero, suspending all mutators. */
int	suspendedallmutators;	/* when non-zero, all mutators suspended. */
int	mutatorsbarrier;	/* when non-zero, mutators barrier imposed. */
mutex_t	mutatorslock;		/* used to enforce mutators barrier. */
cond_t	mutatorscv;		/* where non-mutators sleep. */

/*
 * Get the available register state for the target thread.
 * Return non-volatile registers: TRS_NONVOLATILE
 */
#pragma weak	thr_getstate		= _thr_getstate
int
_thr_getstate(thread_t tid, int *flag, lwpid_t *lwp, stack_t *ss, gregset_t rs)
{
	ulwp_t **ulwpp;
	ulwp_t *ulwp;
	int error = 0;
	int trs_flag = TRS_LWPID;

	if (tid == 0 || curthread->ul_lwpid == tid)
		ulwp_lock(ulwp = curthread);
	else if ((ulwpp = find_lwpp(tid)) != NULL)
		ulwp = *ulwpp;
	else {
		if (flag)
			*flag = TRS_INVALID;
		return (ESRCH);
	}

	if (ulwp->ul_dead) {
		trs_flag = TRS_INVALID;
	} else if (!ulwp->ul_stop && !suspendedallmutators) {
		error = EINVAL;
		trs_flag = TRS_INVALID;
	} else if (ulwp->ul_validregs) {
		trs_flag = TRS_NONVOLATILE;
		getgregs(ulwp, rs);
	}

	if (flag)
		*flag = trs_flag;
	if (lwp)
		*lwp = tid;
	if (ss != NULL)
		(void) _thrp_stksegment(ulwp, ss);

	ulwp_unlock(ulwp);
	return (error);
}

/*
 * Set the appropiate register state for the target thread.
 * This is not used by java.  It exists solely for the MSTC test suite.
 */
#pragma weak	thr_setstate		= _thr_setstate
int
_thr_setstate(thread_t tid, int flag, gregset_t rs)
{
	ulwp_t *ulwp;
	int error = 0;

	if ((ulwp = find_lwp(tid)) == NULL)
		return (ESRCH);

	if (!ulwp->ul_stop && !suspendedallmutators)
		error = EINVAL;
	else if (rs != NULL) {
		switch (flag) {
		case TRS_NONVOLATILE:
			/* do /proc stuff here? */
			if (ulwp->ul_validregs)
				setgregs(ulwp, rs);
			else
				error = EINVAL;
			break;
		case TRS_LWPID:		/* do /proc stuff here? */
		default:
			error = EINVAL;
			break;
		}
	}

	ulwp_unlock(ulwp);
	return (error);
}

ulong_t
__gettsp(thread_t tid)
{
	char filename[100];
	lwpstatus_t status;
	int fd;
	ulwp_t *ulwp;
	ulong_t result;

	if ((ulwp = find_lwp(tid)) == NULL)
		return (0);

	if (ulwp->ul_stop && ulwp->ul_validregs) {
		result = (ulong_t)ulwp->ul_savedregs.rs_sp;
		ulwp_unlock(ulwp);
		return (result);
	}

	(void) sprintf(filename, "/proc/self/lwp/%u/lwpstatus", tid);
	if ((fd = _open(filename, O_RDONLY, 0)) < 0 ||
	    _read(fd, &status, sizeof (status)) != sizeof (status)) {
		(void) sprintf(filename,
			"__gettsp(%u): can't open lwpstatus", tid);
		panic(filename);
	}
	result = status.pr_reg[R_SP];
	(void) _close(fd);

	ulwp_unlock(ulwp);
	return (result);
}

/*
 * This tells java stack walkers how to find the ucontext
 * structure passed to signal handlers.
 */
#pragma weak	thr_sighndlrinfo	= _thr_sighndlrinfo
void
_thr_sighndlrinfo(void (**func)(), int *funcsize)
{
	*func = &__sighndlr;
	*funcsize = (char *)&__sighndlrend - (char *)&__sighndlr;
}

/*
 * mark a thread a mutator or reset a mutator to being a default,
 * non-mutator thread.
 */
#pragma weak	thr_setmutator	= _thr_setmutator
int
_thr_setmutator(thread_t tid, int enabled)
{
	ulwp_t *ulwp;
	int error;

top:
	if (tid == 0 || curthread->ul_lwpid == tid)
		ulwp_lock(ulwp = curthread);
	else if ((ulwp = find_lwp(tid)) == NULL)
		return (ESRCH);

	/*
	 * The target thread should be the caller itself or a suspended thread.
	 * This prevents the target from also changing its ul_mutator field.
	 */
	error = 0;
	if (ulwp != curthread && !ulwp->ul_stop)
		error = EINVAL;
	else if (!enabled)
		ulwp->ul_mutator = 0;
	else if (!ulwp->ul_mutator) {
		lmutex_lock(&mutatorslock);
		if (mutatorsbarrier) {
			ulwp_unlock(ulwp);
			while (mutatorsbarrier)
				(void) _cond_wait(&mutatorscv, &mutatorslock);
			lmutex_unlock(&mutatorslock);
			goto top;
		}
		ulwp->ul_mutator = 1;
		lmutex_unlock(&mutatorslock);
	}

	ulwp_unlock(ulwp);
	return (error);
}

/*
 * establish a barrier against new mutators. any non-mutator
 * trying to become a mutator is suspended until the barrier
 * is removed.
 */
#pragma weak	thr_mutators_barrier	= _thr_mutators_barrier
void
_thr_mutators_barrier(int enabled)
{
	int oldvalue;

	lmutex_lock(&mutatorslock);

	/*
	 * Wait if trying to set the barrier while it is already set.
	 */
	while (mutatorsbarrier && enabled)
		(void) _cond_wait(&mutatorscv, &mutatorslock);

	oldvalue = mutatorsbarrier;
	mutatorsbarrier = enabled;
	/*
	 * Wakeup any blocked non-mutators when barrier is removed.
	 */
	if (oldvalue && !enabled)
		(void) _cond_broadcast(&mutatorscv);
	lmutex_unlock(&mutatorslock);
}

/*
 * suspend the set of all mutators except for the caller. the list
 * of actively running threads is searched, and only the mutators
 * in this list are suspended. actively running non-mutators remain
 * running. any other thread is suspended. this makes the cost of
 * suspending all mutators proportional to the number of active
 * threads. since only the active threads are separated.
 */
#pragma weak	thr_suspend_allmutators	= _thr_suspend_allmutators
int
_thr_suspend_allmutators(void)
{
	ulwp_t *self = curthread;
	ulwp_t *ulwp;

top:
	lmutex_lock(&link_lock);

	if (suspendingallmutators || suspendedallmutators) {
		lmutex_unlock(&link_lock);
		return (EINVAL);
	}
	suspendingallmutators = 1;

	for (ulwp = self->ul_forw; ulwp != self; ulwp = ulwp->ul_forw) {
		ulwp_lock(ulwp);
		if (!ulwp->ul_mutator) {
			ulwp_unlock(ulwp);
		} else if (ulwp->ul_stop) {	/* already stopped */
			ulwp->ul_stop |= TSTP_MUTATOR;
			if (ulwp->ul_wanted)
				ulwp_broadcast(ulwp);
			ulwp_unlock(ulwp);
		} else {
			lwp_suspend(ulwp->ul_lwpid);
			/*
			 * Move the stopped lwp out of a critical section.
			 */
			if (move_to_safe(ulwp, TSTP_MUTATOR, 1)) {
				suspendingallmutators = 0;
				goto top;
			}
		}
	}

	suspendedallmutators = 1;
	suspendingallmutators = 0;
	lmutex_unlock(&link_lock);
	return (0);
}

/*
 * Suspend the target mutator. the caller is permitted to suspend
 * itself.  If a mutator barrier is enabled, the caller will suspend
 * itself as though it had been suspended by thr_suspend_allmutators().
 * When the barrier is removed, this thread will be resumed.  Any
 * suspended mutator, whether suspeded by thr_suspend_mutator(), or by
 * thr_suspend_allmutators(), can be resumed by thr_continue_mutator().
 */
#pragma weak	thr_suspend_mutator	= _thr_suspend_mutator
int
_thr_suspend_mutator(thread_t tid)
{
	if (tid == 0)
		tid = curthread->ul_lwpid;
	return (_thrp_suspend(tid, TSTP_MUTATOR));
}

/*
 * resume the set of all suspended mutators.
 */
#pragma weak	thr_continue_allmutators	= _thr_continue_allmutators
int
_thr_continue_allmutators()
{
	ulwp_t *self = curthread;
	ulwp_t *ulwp;

	lmutex_lock(&link_lock);
	if (!suspendedallmutators) {
		lmutex_unlock(&link_lock);
		return (EINVAL);
	}
	suspendedallmutators = 0;

	for (ulwp = self->ul_forw; ulwp != self; ulwp = ulwp->ul_forw) {
		mutex_t *mp = ulwp_mutex(ulwp);
		lmutex_lock(mp);
		if (ulwp->ul_stop & TSTP_MUTATOR) {
			ulwp->ul_stop &= ~TSTP_MUTATOR;
			if (ulwp->ul_wanted)
				ulwp_broadcast(ulwp);
			if (!ulwp->ul_stop)
				(void) force_continue(ulwp);
		}
		lmutex_unlock(mp);
	}

	lmutex_unlock(&link_lock);
	return (0);
}

/*
 * resume a suspended mutator.
 */
#pragma weak	thr_continue_mutator	= _thr_continue_mutator
int
_thr_continue_mutator(thread_t tid)
{
	return (_thrp_continue(tid, TSTP_MUTATOR));
}

#pragma weak	thr_wait_mutator	= _thr_wait_mutator
int
_thr_wait_mutator(thread_t tid, int dontwait)
{
	ulwp_t *ulwp;
	int error = 0;

top:
	if ((ulwp = find_lwp(tid)) == NULL)
		return (ESRCH);

	if (!ulwp->ul_mutator)
		error = EINVAL;
	else if (dontwait) {
		if (!(ulwp->ul_stop & TSTP_MUTATOR))
			error = EWOULDBLOCK;
	} else if (!(ulwp->ul_stop & TSTP_MUTATOR)) {
		cond_t *cvp = ulwp_condvar(ulwp);
		mutex_t *mp = ulwp_mutex(ulwp);

		ulwp->ul_wanted = 1;
		(void) _cond_wait(cvp, mp);
		(void) lmutex_unlock(mp);
		goto top;
	}

	ulwp_unlock(ulwp);
	return (error);
}

#ifdef DEBUG
/*
 * We define __assert() ourself because the libc version calls
 * gettext() which calls malloc() which grabs a mutex.
 * We do everything without calling standard i/o.
 */

/*
 * 'base' *must* be one of 10 or 16
 */
void
ultos(ulong_t n, int base, char *s)
{
	char lbuf[24];		/* 64 bits fits in 16 hex digits, 20 decimal */
	char *cp = lbuf;

	do {
		*cp++ = "0123456789abcdef"[n%base];
		n /= base;
	} while (n);
	if (base == 16) {
		*s++ = '0';
		*s++ = 'x';
	}
	do {
		*s++ = *--cp;
	} while (cp > lbuf);
	*s = '\0';
}

/*
 * Only one lwp can complete the call to __assert(); all others block.
 */
static mutex_t assert_lock = DEFAULTMUTEX;
static ulwp_t *assert_thread = NULL;

void
__assert(const char *assertion, const char *filename, int line_num)
{
	char buf[512];	/* no assert() message in the library is this long */

	/* avoid recursion deadlock */
	if (assert_thread == curthread)
		_exit(127);
	lmutex_lock(&assert_lock);
	assert_thread = curthread;

	(void) strcpy(buf, "libthread assertion failed for lwp ");
	ultos(curthread->ul_lwpid, 10, buf + strlen(buf));
	(void) strcat(buf, ", thread ");
	ultos((ulong_t)curthread, 16, buf + strlen(buf));
	(void) strcat(buf, ": ");
	(void) strcat(buf, assertion);
	(void) strcat(buf, ", file ");
	(void) strcat(buf, filename);
	(void) strcat(buf, ", line ");
	ultos(line_num, 10, buf + strlen(buf));
	(void) strcat(buf, "\n");
	(void) _write(2, buf, strlen(buf));
	/*
	 * We could replace the call to Abort() with the following code
	 * if we want just to issue a warning message and not die.
	 *	assert_thread = NULL;
	 *	lmutex_unlock(&assert_lock);
	 */
	Abort(buf);
}
#endif	/* DEBUG */

/*
 * This stuff is here to make all of the global names
 * in the original libthread exist in this version.
 * BADFUNC functions should never be called.
 */

void
print_die(const char *func)
{
	char c[100];

	(void) sprintf(c, "libthread:  unexpected call to %s()... aborting",
		func);
	panic(c);
}

#define	BADFUNC(func)		\
void func() {			\
	print_die(#func);	\
}

BADFUNC(thr_probe_setup)
BADFUNC(_sigoff)
BADFUNC(_sigon)
BADFUNC(_assfail)
BADFUNC(_resume)
BADFUNC(_resume_ret)

/*
 * Fix for bug 4250942 which is a workaround for C++ bug 4246986.
 * This will be removed after the C++ bug 4246986 has been fixed.
 */
void
__1cH__CimplKcplus_init6F_v_(void)
{
}

void
__1cH__CimplKcplus_fini6F_v_(void)
{
}
