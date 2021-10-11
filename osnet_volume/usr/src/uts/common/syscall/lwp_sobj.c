/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lwp_sobj.c	1.46	99/12/16 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/prsystm.h>
#include <sys/kmem.h>
#include <sys/sobject.h>
#include <sys/fault.h>
#include <sys/procfs.h>
#include <sys/watchpoint.h>
#include <sys/time.h>
#include <sys/cmn_err.h>
#include <sys/machlock.h>
#include <sys/debug.h>
#include <sys/synch.h>
#include <sys/synch32.h>
#include <sys/mman.h>
#include <sys/class.h>
#include <sys/schedctl.h>
#include <sys/sleepq.h>
#include <sys/tnf_probe.h>
#include <sys/lwpchan_impl.h>
#include <sys/turnstile.h>
#include <sys/lwp_upimutex_impl.h>
#include <vm/as.h>

static kthread_t *lwpsobj_owner(caddr_t);
static void lwp_unsleep(kthread_t *t);
static void lwp_change_pri(kthread_t *t, pri_t pri, pri_t *t_prip);
static void lwp_mutex_cleanup(lwpchan_entry_t *ent, uint16_t lockflg);
static void lwp_release_all(lwpchan_t *lwpchan);
/*
 * Maximum number of user prio inheritance locks that can be held by a thread.
 * Used to limit kmem for each thread. This is a per-thread limit that
 * can be administered on a system wide basis (using /etc/system).
 *
 * Also, when a limit, say maxlwps is added for numbers of lwps within a
 * process, the per-thread limit automatically becomes a process-wide limit
 * of maximum number of held upi locks within a process:
 *      maxheldupimx = maxnestupimx * maxlwps;
 */
static uint32_t maxnestupimx = 2000;

/*
 * The sobj_ops vector exports a set of functions needed when a thread
 * is asleep on a synchronization object of this type.
 */
static sobj_ops_t lwp_sobj_ops = {
	SOBJ_USER, lwpsobj_owner, lwp_unsleep, lwp_change_pri
};

static kthread_t *lwpsobj_pi_owner(upimutex_t *up);

static sobj_ops_t lwp_sobj_pi_ops = {
	SOBJ_USER_PI, lwpsobj_pi_owner, turnstile_unsleep,
	turnstile_change_pri
};

static sleepq_head_t	lwpsleepq[NSLEEPQ];
static kmutex_t		lwpchanlock[LWPCHAN_ENTRIES];
upib_t 			upimutextab[UPIMUTEX_TABSIZE];

#define	LWPCHAN_LOCAL	USYNC_THREAD
#define	LWPCHAN_GLOBAL	USYNC_PROCESS
#define	LWPCHAN_ROBUST	USYNC_PROCESS_ROBUST

#define	LOCKADDR(lname)	((lwp_mutex_t *) \
		((caddr_t)(lname) - (caddr_t)&(((lwp_mutex_t *)0)->mutex_type)))
/* is this a robust lock? */
#define	ROBUSTLOCK(type)	((type) & USYNC_PROCESS_ROBUST)

/*
 * Is this a POSIX threads user-level lock requiring priority inheritance?
 */
#define	UPIMUTEX(type)	((type) & LOCK_PRIO_INHERIT)

static sleepq_head_t *
lwpsqhash(lwpchan_t *lwpchan)
{
	return (&lwpsleepq[SQHASHINDEX(lwpchan->lc_wchan)]);
}

/*
 * Lock an lwpchan.
 * This will make an operation for this lwpchan atomic.
 */
static void
lwpchan_lock(lwpchan_t *lwpchan, int type, int pool)
{
	kmutex_t *lp;
	int i;

	ASSERT(curproc->p_lcp != NULL);

	i = LWPCHAN_HASH(lwpchan->lc_wchan, pool);
	if (type == LWPCHAN_LOCAL)
		lp = &curproc->p_lcp->lwpchan_cache[i].lwpchan_lock;
	else
		lp = &lwpchanlock[i];
	mutex_enter(lp);
}

/*
 * Unlock an lwpchan.
 */
static void
lwpchan_unlock(lwpchan_t *lwpchan, int type, int pool)
{
	kmutex_t *lp;
	int i;

	ASSERT(curproc->p_lcp != NULL);

	i = LWPCHAN_HASH(lwpchan->lc_wchan, pool);
	if (type == LWPCHAN_LOCAL)
		lp = &curproc->p_lcp->lwpchan_cache[i].lwpchan_lock;
	else
		lp = &lwpchanlock[i];
	mutex_exit(lp);
}

/*
 * Delete mappings from the lwpchan cache for pages that are being
 * unmapped by as_unmap().  Given a range of addresses, "start" to "end",
 * all mappings within this range will be deleted from the cache.
 */
void
lwpchan_delete_mapping(lwpchan_data_t *lcp, caddr_t start, caddr_t end)
{
	lwpchan_entry_t *ent, **prev;
	caddr_t a;
	int i;

	ASSERT(lcp != NULL);

	for (i = 0; i < LWPCHAN_ENTRIES; i++) {
		mutex_enter(&lcp->lwpchan_cache[i].lwpchan_lock);
		prev = &(lcp->lwpchan_cache[i].lwpchan_chain);
		/* check entire chain */
		while ((ent = *prev) != NULL) {
			a = ent->lwpchan_addr;
			if ((((caddr_t)LOCKADDR(a) +
				sizeof (lwp_mutex_t) - 1) >= start) &&
				a < end) {
				*prev = ent->lwpchan_next;
				lwp_mutex_cleanup(ent, LOCK_UNMAPPED);
				kmem_free(ent, sizeof (*ent));
			} else
				prev = &ent->lwpchan_next;
		}
		mutex_exit(&lcp->lwpchan_cache[i].lwpchan_lock);
	}
}

/*
 * Allocate a per-process lwpchan cache.
 * If memory can't be allocated, the cache remains disabled.
 */
static void
lwpchan_alloc_cache(void)
{
	proc_t *p = curproc;
	lwpchan_data_t *p_lcp;

	if (p->p_lcp == NULL) {
		p_lcp = kmem_zalloc(sizeof (lwpchan_data_t), KM_SLEEP);
		/*
		 * typically, the lwp cache is allocated by a single thread,
		 * however there is nothing preventing two or more threads from
		 * doing this. when this happens, only one thread will be
		 * allowed to create the cache.
		 */
		mutex_enter(&p->p_lock);
		if (p->p_lcp == NULL) {
			p->p_lcp = p_lcp;
			mutex_exit(&p->p_lock);
			return;
		}
		mutex_exit(&p->p_lock);
		kmem_free(p_lcp, sizeof (lwpchan_data_t));
	}
}

/*
 * Deallocate the lwpchan cache, and any dynamically allocated mappings.
 * Called when the process exits, all lwps except one have exited.
 */
void
lwpchan_destroy_cache(void)
{
	lwpchan_entry_t *ent, *next;
	proc_t *p = curproc;
	int i;

	if (p->p_lcp != NULL) {
		for (i = 0; i < LWPCHAN_ENTRIES; i++) {
			ent = p->p_lcp->lwpchan_cache[i].lwpchan_chain;
			while (ent != NULL) {
				next = ent->lwpchan_next;
				if (ent->lwpchan_type == LWPCHAN_ROBUST)
					lwp_mutex_cleanup(ent, LOCK_OWNERDEAD);
				kmem_free(ent, sizeof (*ent));
				ent = next;
			}
		}
		kmem_free(p->p_lcp, sizeof (*p->p_lcp));
		p->p_lcp = NULL;
	}
}

/*
 * An lwpchan cache is allocated for the calling process the first time
 * the cache is referenced.  This function returns 1 when the mapping
 * is in the cache and a zero when it is not.
 * The caller holds the bucket lock.
 */
static int
lwpchan_cache_mapping(caddr_t addr, lwpchan_t *lwpchan,
			lwpchan_hashbucket_t *hashbucket)
{
	lwpchan_entry_t *ent;
	int cachehit = 0;

	/*
	 * check cache for mapping.
	 */
	ent = hashbucket->lwpchan_chain;
	while (ent != NULL) {
		if (ent->lwpchan_addr == addr) {
			*lwpchan = ent->lwpchan_lwpchan;
			cachehit = 1;
			break;
		}
		ent = ent->lwpchan_next;
	}
	return (cachehit);
}

/*
 * Return the cached mapping if cached, otherwise insert a virtual address
 * to lwpchan mapping into the cache.  If the bucket for this mapping is
 * not unique, allocate an entry, and add it to the hash chain.
 */
static int
lwpchan_get_mapping(struct as *as, caddr_t addr, lwpchan_t *lwpchan,
		int type, int pool)
{
	lwpchan_hashbucket_t *hashbucket;
	lwpchan_entry_t *ent, *new_ent;
	memid_t	memid;

	hashbucket = &(curproc->p_lcp->lwpchan_cache[LWPCHAN_HASH(addr, pool)]);
	mutex_enter(&hashbucket->lwpchan_lock);
	if (lwpchan_cache_mapping(addr, lwpchan, hashbucket)) {
		mutex_exit(&hashbucket->lwpchan_lock);
		return (1);
	}
	mutex_exit(&hashbucket->lwpchan_lock);
	if (as_getmemid(as, addr, &memid) != 0)
		return (0);
	lwpchan->lc_wchan0 = (caddr_t)memid.val[0];
	lwpchan->lc_wchan = (caddr_t)memid.val[1];
	new_ent = kmem_alloc(sizeof (lwpchan_entry_t), KM_SLEEP);
	mutex_enter(&hashbucket->lwpchan_lock);
	if (lwpchan_cache_mapping(addr, lwpchan, hashbucket)) {
		mutex_exit(&hashbucket->lwpchan_lock);
		kmem_free(new_ent, sizeof (*new_ent));
		return (1);
	}
	new_ent->lwpchan_addr = addr;
	new_ent->lwpchan_type = type;
	new_ent->lwpchan_lwpchan = *lwpchan;
	ent = hashbucket->lwpchan_chain;
	if (ent == NULL) {
		hashbucket->lwpchan_chain = new_ent;
		new_ent->lwpchan_next = NULL;
	} else {
		new_ent->lwpchan_next = ent->lwpchan_next;
		ent->lwpchan_next = new_ent;
	}
	mutex_exit(&hashbucket->lwpchan_lock);
	return (1);
}

/*
 * Return a unique pair of identifiers (usually vnode/offset)
 * that corresponds to 'addr'.
 */
static int
get_lwpchan(struct as *as, caddr_t addr, int type, lwpchan_t *lwpchan, int pool)
{
	/*
	 * initialize the lwpchan cache.
	 */
	if (curproc->p_lcp == NULL)
		lwpchan_alloc_cache();

	/*
	 * If the lwp synch object was defined to be local to this
	 * process, its type field is set to zero.  The first
	 * field of the lwpchan is curproc and the second field
	 * is the synch object's virtual address.
	 */
	if (type == LWPCHAN_LOCAL) {
		lwpchan->lc_wchan0 = (caddr_t)curproc;
		lwpchan->lc_wchan = addr;
		return (1);
	}
	/* check lwpchan cache for mapping */
	return (lwpchan_get_mapping(as, addr, lwpchan, type, pool));
}

static void
lwp_block(lwpchan_t *lwpchan)
{
	klwp_t *lwp = ttolwp(curthread);
	sleepq_head_t *sqh;

	thread_lock(curthread);
	curthread->t_flag |= T_WAKEABLE;
	curthread->t_lwpchan = *lwpchan;
	curthread->t_sobj_ops = &lwp_sobj_ops;
	sqh = lwpsqhash(lwpchan);
	disp_lock_enter_high(&sqh->sq_lock);
	CL_SLEEP(curthread, 0);
	THREAD_SLEEP(curthread, &sqh->sq_lock);
	sleepq_insert(&sqh->sq_queue, curthread);
	thread_unlock(curthread);
	lwp->lwp_asleep = 1;
	lwp->lwp_sysabort = 0;
	lwp->lwp_ru.nvcsw++;
	if (curthread->t_proc_flag & TP_MSACCT)
		(void) new_mstate(curthread, LMS_SLEEP);
}

static kthread_t *
lwpsobj_pi_owner(upimutex_t *up)
{
	return (up->upi_owner);
}

static struct upimutex *
upi_get(upib_t *upibp, lwpchan_t *lcp)
{
	struct upimutex *upip;

	for (upip = upibp->upib_first; upip != NULL;
	    upip = upip->upi_nextchain) {
		if (upip->upi_lwpchan.lc_wchan0 == lcp->lc_wchan0 &&
		    upip->upi_lwpchan.lc_wchan == lcp->lc_wchan)
			break;
	}
	return (upip);
}

static void
upi_chain_add(upib_t *upibp, struct upimutex *upimutex)
{
	ASSERT(MUTEX_HELD(&upibp->upib_lock));

	/*
	 * Insert upimutex at front of list. Maybe a bit unfair
	 * but assume that not many lwpchans hash to the same
	 * upimutextab bucket, i.e. the list of upimutexes from
	 * upib_first is not too long.
	 */
	upimutex->upi_nextchain = upibp->upib_first;
	upibp->upib_first = upimutex;
}

static void
upi_chain_del(upib_t *upibp, struct upimutex *upimutex)
{
	struct upimutex **prev;

	ASSERT(MUTEX_HELD(&upibp->upib_lock));

	prev = &upibp->upib_first;
	while (*prev != upimutex) {
		prev = &(*prev)->upi_nextchain;
	}
	*prev = upimutex->upi_nextchain;
	upimutex->upi_nextchain = NULL;
}

/*
 * Add upimutex to chain of upimutexes held by curthread.
 * Returns number of upimutexes held by curthread.
 */
static uint32_t
upi_mylist_add(struct upimutex *upimutex)
{
	kthread_t *t = curthread;

	/*
	 * Insert upimutex at front of list of upimutexes owned by t. This
	 * would match typical LIFO order in which nested locks are acquired
	 * and released.
	 */
	upimutex->upi_nextowned = t->t_upimutex;
	t->t_upimutex = upimutex;
	t->t_nupinest++;
	ASSERT(t->t_nupinest > 0);
	return (t->t_nupinest);
}

/*
 * Delete upimutex from list of upimutexes owned by curthread.
 */
static void
upi_mylist_del(struct upimutex *upimutex)
{
	kthread_t *t = curthread;
	struct upimutex **prev;

	/*
	 * Since the order in which nested locks are acquired and released,
	 * is typically LIFO, and typical nesting levels are not too deep, the
	 * following should not be expensive in the general case.
	 */
	prev = &t->t_upimutex;
	while (*prev != upimutex) {
		prev = &(*prev)->upi_nextowned;
	}
	*prev = upimutex->upi_nextowned;
	upimutex->upi_nextowned = NULL;
	ASSERT(t->t_nupinest > 0);
	t->t_nupinest--;
}

/*
 * Returns true if upimutex is owned. Should be called only when upim points
 * to kmem which cannot disappear from underneath.
 */
static int
upi_owned(upimutex_t *upim)
{
	return (upim->upi_owner == curthread);
}

/*
 * Returns pointer to kernel object (upimutex_t *) if lp is owned.
 */
static struct upimutex *
lwp_upimutex_owned(lwp_mutex_t *lp, uint8_t type)
{
	lwpchan_t lwpchan;
	upib_t *upibp;
	struct upimutex *upimutex;

	if (!get_lwpchan(curproc->p_as, (caddr_t)lp, type, &lwpchan,
							LWPCHAN_MPPOOL)) {
		return (NULL);
	}
	upibp = &UPI_CHAIN(lwpchan);
	mutex_enter(&upibp->upib_lock);
	upimutex = upi_get(upibp, &lwpchan);
	if (upimutex == NULL || upimutex->upi_owner != curthread) {
		mutex_exit(&upibp->upib_lock);
		return (NULL);
	}
	mutex_exit(&upibp->upib_lock);
	return (upimutex);
}

/*
 * Unlocks upimutex, waking up waiters if any. upimutex kmem is freed if
 * no lock hand-off occurrs.
 */
static void
upimutex_unlock(struct upimutex *upimutex, uint16_t flag)
{
	turnstile_t *ts;
	upib_t *upibp;
	kthread_t *newowner;

	upi_mylist_del(upimutex);
	upibp = upimutex->upi_upibp;
	mutex_enter(&upibp->upib_lock);
	if (upimutex->upi_waiter != 0) { /* if waiters */
		ts = turnstile_lookup(upimutex);
		if (ts != NULL && !(flag & LOCK_NOTRECOVERABLE)) {
			/* hand-off lock to highest prio waiter */
			newowner = ts->ts_sleepq[TS_WRITER_Q].sq_first;
			upimutex->upi_owner = newowner;
			if (ts->ts_waiters == 1)
				upimutex->upi_waiter = 0;
			turnstile_wakeup(ts, TS_WRITER_Q, 1, newowner);
			mutex_exit(&upibp->upib_lock);
			return;
		} else if (ts != NULL) {
			/* LOCK_NOTRECOVERABLE: wakeup all */
			turnstile_wakeup(ts, TS_WRITER_Q, ts->ts_waiters, NULL);
		} else {
			/*
			 * Misleading w bit. Waiters might have been
			 * interrupted. No need to clear the w bit (upimutex
			 * will soon be freed). Re-calculate PI from existing
			 * waiters.
			 */
			turnstile_exit(upimutex);
			turnstile_pi_recalc();
		}
	}
	/*
	 * no waiters, or LOCK_NOTRECOVERABLE.
	 * remove from the bucket chain of upi mutexes.
	 * de-allocate kernel memory (upimutex).
	 */
	upi_chain_del(upimutex->upi_upibp, upimutex);
	mutex_exit(&upibp->upib_lock);
	kmem_free(upimutex, sizeof (upimutex_t));
}

static int
lwp_upimutex_lock(lwp_mutex_t *lp, uint8_t type, int try)
{
	label_t ljb;
	int error = 0;
	lwpchan_t lwpchan;
	uint16_t flag;
	upib_t *upibp;
	volatile struct upimutex *upimutex = NULL;
	int tsrc;
	turnstile_t *ts;
	int scblock;
	uint32_t nupinest;
	volatile int upilocked = 0;

	if (on_fault(&ljb)) {
		if (upilocked)
			upimutex_unlock((upimutex_t *)upimutex, 0);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * The apparent assumption made in implementing other _lwp_* synch
	 * primitives, is that get_lwpchan() does not return a unique cookie
	 * for the case where 2 processes (one forked from the other) point
	 * at the same underlying object, which is typed USYNC_PROCESS, but
	 * mapped MAP_PRIVATE, since the object has not yet been written to,
	 * in the child process.
	 *
	 * Since get_lwpchan() has been fixed, it is not necessary to do the
	 * dummy writes to force a COW fault as in other places (which should
	 * be fixed).
	 */
	if (!get_lwpchan(curproc->p_as, (caddr_t)lp, type, &lwpchan,
							LWPCHAN_MPPOOL)) {
		error = set_errno(EFAULT);
		goto out;
	}
	upibp = &UPI_CHAIN(lwpchan);
retry:
	mutex_enter(&upibp->upib_lock);
	upimutex = upi_get(upibp, &lwpchan);
	if (upimutex == NULL)  {
		/* lock available since lwpchan has no upimutex */
		upimutex = kmem_zalloc(sizeof (upimutex_t), KM_SLEEP);
		upi_chain_add(upibp, (upimutex_t *)upimutex);
		upimutex->upi_owner = curthread; /* grab lock */
		upimutex->upi_upibp = upibp;
		upimutex->upi_vaddr = lp;
		upimutex->upi_lwpchan = lwpchan;
		mutex_exit(&upibp->upib_lock);
		nupinest = upi_mylist_add((upimutex_t *)upimutex);
		upilocked = 1;
		fuword16_noerr(&lp->mutex_flag, &flag);
		if (nupinest > maxnestupimx && !suser(CRED())) {
			upimutex_unlock((upimutex_t *)upimutex, flag);
			error = set_errno(ENOMEM);
			goto out;
		}
		if (flag & LOCK_OWNERDEAD) {
			/*
			 * Return with upimutex held.
			 */
			error = set_errno(EOWNERDEAD);
		} else if (flag & LOCK_NOTRECOVERABLE) {
			/*
			 * Since the setting of LOCK_NOTRECOVERABLE
			 * was done under the high-level upi mutex,
			 * in lwp_upimutex_unlock(), this flag needs to
			 * be checked while holding the upi mutex.
			 * If set, this thread should  return without
			 * the lock held, and with the right error
			 * code.
			 */
			upimutex_unlock((upimutex_t *)upimutex, flag);
			upilocked = 0;
			error = set_errno(ENOTRECOVERABLE);
		}
		goto out;
	}
	/*
	 * If a upimutex object exists, it must have an owner.
	 * This is due to lock hand-off, and release of upimutex when no
	 * waiters are present at unlock time,
	 */
	ASSERT(upimutex->upi_owner != NULL);
	if (upimutex->upi_owner == curthread) {
		/*
		 * The user wrapper can check if the mutex type is
		 * ERRORCHECK: if not, it should stall at user-level.
		 * If so, it should return the error code.
		 */
		mutex_exit(&upibp->upib_lock);
		error = set_errno(EDEADLK);
		goto out;
	}
	if (try == UPIMUTEX_TRY) {
		mutex_exit(&upibp->upib_lock);
		error = set_errno(EBUSY);
		goto out;
	}
	/*
	 * Block for the lock.
	 * Put the lwp in an orderly state for debugging
	 * before calling schedctl_block(NULL).
	 */
	prstop(PR_REQUESTED, 0);
	if ((scblock = schedctl_check(curthread, SC_BLOCK)) != 0)
		(void) schedctl_block(NULL);
	/*
	 * Now, set the waiter bit and block for the lock in turnstile_block().
	 * No need to preserve the previous wbit since a lock try is not
	 * attempted after setting the wait bit. Wait bit is set under
	 * the upib_lock, which is not released until the turnstile lock
	 * is acquired. Say, the upimutex is L:
	 *
	 * 1. upib_lock is held so the waiter does not have to retry L after
	 *    setting the wait bit: since the owner has to grab the upib_lock
	 *    to unlock L, it will certainly see the wait bit set.
	 * 2. upib_lock is not released until the turnstile lock is acquired.
	 *    This is the key to preventing a missed wake-up. Otherwise, the
	 *    owner could acquire the upib_lock, and the tc_lock, to call
	 *    turnstile_wakeup(). All this, before the waiter gets tc_lock
	 *    to sleep in turnstile_block(). turnstile_wakeup() will then not
	 *    find this waiter, resulting in the missed wakeup.
	 * 3. The upib_lock, being a kernel mutex, cannot be released while
	 *    holding the tc_lock (since mutex_exit() could need to acquire
	 *    the same tc_lock)...and so is held when calling turnstile_block().
	 *    The address of upib_lock is passed to turnstile_block() which
	 *    releases it after releasing all turnstile locks, and before going
	 *    to sleep in swtch().
	 * 4. The waiter value cannot be a count of waiters, because a waiter
	 *    can be interrupted. The interrupt occurs under the tc_lock, at
	 *    which point, the upib_lock cannot be locked, to decrement waiter
	 *    count. So, just treat the waiter state as a bit, not a count.
	 */
	ts = turnstile_lookup((upimutex_t *)upimutex);
	upimutex->upi_waiter = 1;
	tsrc = turnstile_block(ts, TS_WRITER_Q, (upimutex_t *)upimutex,
	    &lwp_sobj_pi_ops, &upibp->upib_lock);
	if (scblock)
		schedctl_unblock();
	/*
	 * Hand-off implies that we wakeup holding the lock, except when:
	 *	- deadlock is detected
	 *	- lock is not recoverable
	 * Use lwp_upimutex_owned() to check if we do hold the lock.
	 */
	if (tsrc > 0) {
		if (tsrc == EINTR &&
		    (upimutex = lwp_upimutex_owned(lp, type))) {
			/*
			 * The lock could be held, due to lock hand-off!
			 * To check, use "lp". "upimutex" could be invalid.
			 * Unlock and return - the re-startable syscall will
			 * try the lock again.
			 */
			(void) upi_mylist_add((upimutex_t *)upimutex);
			upimutex_unlock((upimutex_t *)upimutex, 0);
		}
		/*
		 * Only other possible error is EDEADLK. If so, "upimutex" is
		 * valid, since its owner is deadlocked with curthread.
		 */
		ASSERT(tsrc == EINTR || (tsrc == EDEADLK &&
		    upi_owned((upimutex_t *)upimutex) == 0));
		ASSERT(!lwp_upimutex_owned(lp, type));
		error = set_errno(tsrc);
		goto out;
	}
	if (lwp_upimutex_owned(lp, type)) {
		ASSERT(lwp_upimutex_owned(lp, type) == upimutex);
		nupinest = upi_mylist_add((upimutex_t *)upimutex);
		upilocked = 1;
	} else {
		/*
		 * If lock has not been handed-off, the action to be taken
		 * depends on the return code from turnstile_block().
		 */
		if (tsrc == 0) {
			/*
			 * If return code is 0, it must be due to one of the
			 * following scenarios:
			 * 1. mutex_flag was set to LOCK_NOTERECOVERABLE.
			 * 2. A upimutex owner died, but was unable to udpate
			 *    the user mutex_flag  with LOCK_OWNERDEAD (see
			 *    upimutex_cleanup()).
			 * Return the error without reading the flag (which is
			 * not necessary).
			 */
			error = set_errno(ENOTRECOVERABLE);
			goto out;
		} else {
			/*
			 * If return code is -1, turnstile_block() has reported
			 * a spurious wake-up! Lock is not held. Try again.
			 * Cannot return success without lock held.
			 */
			ASSERT(tsrc == -1);
			goto retry;
		}
	}
	fuword16_noerr(&lp->mutex_flag, &flag);
	/*
	 * If LOCK_NOTRECOVERABLE is set in flag, it would have exited out
	 * of the above check which returns ENOTRECOVERABLE, since lock would
	 * not be held. Hence, the following ASSERT.
	 */
	ASSERT(!(flag & LOCK_NOTRECOVERABLE));
	ASSERT(upilocked);
	if (nupinest > maxnestupimx && !suser(CRED())) {
		upimutex_unlock((upimutex_t *)upimutex, flag);
		upilocked = 0;
		error = set_errno(ENOMEM);
	} else if (flag & LOCK_OWNERDEAD) {
		error = set_errno(EOWNERDEAD);
	}
out:
	no_fault();
	return (error);
}


static int
lwp_upimutex_unlock(lwp_mutex_t *lp, uint8_t type)
{
	label_t ljb;
	int error = 0;
	lwpchan_t lwpchan;
	uint16_t flag;
	upib_t *upibp;
	volatile struct upimutex *upimutex = NULL;
	volatile int upilocked = 0;

	if (on_fault(&ljb)) {
		if (upilocked)
			upimutex_unlock((upimutex_t *)upimutex, 0);
		error = set_errno(EFAULT);
		goto out;
	}
	if (!get_lwpchan(curproc->p_as, (caddr_t)lp, type, &lwpchan,
						    LWPCHAN_MPPOOL)) {
		error = set_errno(EFAULT);
		goto out;
	}
	upibp = &UPI_CHAIN(lwpchan);
	mutex_enter(&upibp->upib_lock);
	upimutex = upi_get(upibp, &lwpchan);
	/*
	 * If the lock is not held, or the owner is not curthread, return
	 * error. The user-level wrapper can return this error or stall,
	 * depending on whether mutex is of ERRORCHECK type or not.
	 */
	if (upimutex == NULL || upimutex->upi_owner != curthread) {
		mutex_exit(&upibp->upib_lock);
		error = set_errno(EPERM);
		goto out;
	}
	mutex_exit(&upibp->upib_lock); /* release for user memory access */
	upilocked = 1;
	fuword16_noerr(&lp->mutex_flag, &flag);
	if (flag & LOCK_OWNERDEAD) {
		/*
		 * transition mutex to the LOCK_NOTRECOVERABLE state.
		 */
		flag &= ~LOCK_OWNERDEAD;
		flag |= LOCK_NOTRECOVERABLE;
		suword16_noerr(&(lp->mutex_flag), flag);
	}
	upimutex_unlock((upimutex_t *)upimutex, flag);
	upilocked = 0;
out:
	no_fault();
	return (error);
}

/*
 * Mark user mutex state, corresponding to kernel upimutex, as LOCK_OWNERDEAD.
 */
static int
upi_dead(upimutex_t *upip)
{
	label_t ljb;
	int error = 0;
	lwp_mutex_t *lp;
	uint16_t flag;

	if (on_fault(&ljb)) {
		error = EFAULT;
		goto out;
	}

	lp = upip->upi_vaddr;
	fuword16_noerr(&lp->mutex_flag, &flag);
	flag |= LOCK_OWNERDEAD;
	suword16_noerr(&(lp->mutex_flag), flag);
out:
	no_fault();
	return (error);
}

/*
 * Unlock all upimutexes held by curthread, since curthread is dying.
 * For each upimutex, attempt to mark its corresponding user mutex object as
 * dead.
 */
void
upimutex_cleanup()
{
	kthread_t *t = curthread;
	struct upimutex *upip;

	while ((upip = t->t_upimutex) != NULL) {
		if (upi_dead(upip) != 0) {
			/*
			 * If the user object associated with this upimutex is
			 * unmapped, unlock upimutex with the
			 * LOCK_NOTRECOVERABLE flag, so that all waiters are
			 * woken up. Since user object is unmapped, it could
			 * not be marked as dead or notrecoverable.
			 * The waiters will now all wake up and return
			 * ENOTRECOVERABLE, since they would find that the lock
			 * has not been handed-off to them.
			 * See lwp_upimutex_lock().
			 */
			upimutex_unlock(upip, LOCK_NOTRECOVERABLE);
		} else {
			/*
			 * The user object has been updated as dead.
			 * Unlock the upimutex: if no waiters, upip kmem will
			 * be freed. If there is a waiter, the lock will be
			 * handed off. If exit() is in progress, each existing
			 * waiter will successively get the lock, as owners
			 * die, and each new owner will call this routine as
			 * it dies. The last owner will free kmem, since
			 * it will find the upimutex has no waiters. So,
			 * eventually, the kmem is guaranteed to be freed.
			 */
			upimutex_unlock(upip, 0);
		}
		/*
		 * Note that the call to upimutex_unlock() above will delete
		 * upimutex from the t_upimutexes chain. And so the
		 * while loop will eventually terminate.
		 */
	}
}

/*
 * A lwp blocks when the mutex is set.
 */
int
lwp_mutex_lock(lwp_mutex_t *lp)
{
	kthread_t *t = curthread;
	proc_t *p = ttoproc(t);
	klwp_t *lwp = ttolwp(t);
	int error = 0;
	uchar_t waiters;
	volatile int locked = 0;
	volatile int mapped = 0;
	label_t ljb;
	volatile uint8_t type = 0;
	caddr_t lname;		/* logical name of mutex (vnode + offset) */
	lwpchan_t lwpchan;
	sleepq_head_t *sqh;
	static int iswanted();
	int scblock;
	uint16_t flag;

	if ((caddr_t)lp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));
	/*
	 * Although LMS_USER_LOCK implies "asleep waiting for user-mode lock",
	 * this micro state is really a run state. If the thread indeed blocks,
	 * this state becomes valid. If not, the state is converted back to
	 * LMS_SYSTEM. So, it is OK to set the mstate here, instead of just
	 * when blocking.
	 */
	if (t->t_proc_flag & TP_MSACCT)
		(void) new_mstate(t, LMS_USER_LOCK);
	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
		error = set_errno(EFAULT);
		goto out;
	}
	fuword8_noerr(&lp->mutex_type, (uint8_t *)&type);
	if (UPIMUTEX(type)) {
		no_fault();
		error = lwp_upimutex_lock(lp, type, UPIMUTEX_BLOCK);
		return (error);
	}
	/*
	 * Force Copy-on-write fault if lwp_mutex_t object is
	 * defined to be MAP_PRIVATE and it was initialized to
	 * USYNC_PROCESS.
	 */
	suword8_noerr(&lp->mutex_type, type);
	lname = (caddr_t)&lp->mutex_type;
	if (!get_lwpchan(curproc->p_as, lname, type, &lwpchan,
							LWPCHAN_MPPOOL)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type, LWPCHAN_MPPOOL);

	locked = 1;
	fuword8_noerr(&lp->mutex_waiters, &waiters);
	suword8_noerr(&lp->mutex_waiters, 1);
	if (ROBUSTLOCK(type)) {
		fuword16_noerr(&lp->mutex_flag, &flag);
		if (flag & LOCK_NOTRECOVERABLE) {
			lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
			error = set_errno(ENOTRECOVERABLE);
			goto out;
		}
	}

	/*
	 * If watchpoints are set, they need to be restored, since
	 * atomic accesses of memory such as the call to ulock_try()
	 * below cannot be watched.
	 */

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);

	while (!ulock_try(&lp->mutex_lockw)) {
		if (mapped) {
			pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
			mapped = 0;
		}
		/*
		 * Put the lwp in an orderly state for debugging
		 * before calling schedctl_block(NULL).
		 */
		prstop(PR_REQUESTED, 0);
		if ((scblock = schedctl_check(t, SC_BLOCK)) != 0)
			(void) schedctl_block(NULL);
		lwp_block(&lwpchan);
		/*
		 * Nothing should happen to cause the lwp to go to
		 * sleep again until after it returns from swtch().
		 */
		locked = 0;
		lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
		if (ISSIG(t, JUSTLOOKING) || MUSTRETURN(p, t))
			setrun(t);
		swtch();
		t->t_flag &= ~T_WAKEABLE;
		if (scblock)
			schedctl_unblock();
		setallwatch();
		if (ISSIG(t, FORREAL) ||
		    lwp->lwp_sysabort || MUSTRETURN(p, t)) {
			error = set_errno(EINTR);
			lwp->lwp_asleep = 0;
			lwp->lwp_sysabort = 0;
			if (p->p_warea)
				mapped = pr_mappage((caddr_t)lp,
					sizeof (*lp), S_WRITE, 1);
			/*
			 * Need to re-compute waiters bit. The waiters field in
			 * the lock is not reliable. Either of two things
			 * could have occurred: no lwp may have called
			 * lwp_release() for me but I have woken up due to a
			 * signal. In this case, the waiter bit is incorrect
			 * since it is still set to 1, set above.
			 * OR an lwp_release() did occur for some other lwp
			 * on the same lwpchan. In this case, the waiter bit is
			 * correct. But which event occurred, one can't tell.
			 * So, recompute.
			 */
			lwpchan_lock(&lwpchan, type, LWPCHAN_MPPOOL);
			locked = 1;
			sqh = lwpsqhash(&lwpchan);
			disp_lock_enter(&sqh->sq_lock);
			waiters = iswanted(sqh->sq_queue.sq_first, &lwpchan);
			disp_lock_exit(&sqh->sq_lock);
			break;
		}
		lwp->lwp_asleep = 0;
		if (p->p_warea)
			mapped = pr_mappage((caddr_t)lp, sizeof (*lp),
				S_WRITE, 1);
		lwpchan_lock(&lwpchan, type, LWPCHAN_MPPOOL);
		locked = 1;
		fuword8_noerr(&lp->mutex_waiters, &waiters);
		suword8_noerr(&lp->mutex_waiters, 1);
		if (ROBUSTLOCK(type)) {
			fuword16_noerr(&lp->mutex_flag, &flag);
			if (flag & LOCK_NOTRECOVERABLE) {
				error = set_errno(ENOTRECOVERABLE);
				break;
			}
		}
	}
	if (!error && ROBUSTLOCK(type)) {
		suword32_noerr((uint32_t *)&(lp->mutex_ownerpid),
		    p->p_pidp->pid_id);
		fuword16_noerr(&lp->mutex_flag, &flag);
		if (flag & LOCK_OWNERDEAD) {
			error = set_errno(EOWNERDEAD);
		} else if (flag & LOCK_UNMAPPED) {
			error = set_errno(ELOCKUNMAPPED);
		}
	}
	suword8_noerr(&lp->mutex_waiters, waiters);
	locked = 0;
	lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
	return (error);
}

static int
iswanted(kthread_t *t, lwpchan_t *lwpchan)
{
	/*
	 * The caller holds the dispatcher lock on the sleep queue.
	 */
	while (t != NULL) {
		if (t->t_lwpchan.lc_wchan0 == lwpchan->lc_wchan0 &&
		    t->t_lwpchan.lc_wchan == lwpchan->lc_wchan)
			return (1);
		t = t->t_link;
	}
	return (0);
}

static int
lwp_release(lwpchan_t *lwpchan, uchar_t *waiters, int sync_type)
{
	sleepq_head_t *sqh;
	kthread_t *tp;
	kthread_t **tpp;

	sqh = lwpsqhash(lwpchan);
	disp_lock_enter(&sqh->sq_lock);		/* lock the sleep queue */
	tpp = &sqh->sq_queue.sq_first;
	while ((tp = *tpp) != NULL) {
		if (tp->t_lwpchan.lc_wchan0 == lwpchan->lc_wchan0 &&
		    tp->t_lwpchan.lc_wchan == lwpchan->lc_wchan) {
			/*
			 * The following is typically false. It could be true
			 * only if lwp_release() is called from
			 * lwp_mutex_wakeup() after reading the waiters field
			 * from memory in which the lwp lock used to be, but has
			 * since been re-used to hold a lwp cv or lwp semaphore.
			 * The thread "tp" found to match the lwp lock's wchan
			 * is actually sleeping for the cv or semaphore which
			 * now has the same wchan. In this case, lwp_release()
			 * should return failure.
			 */
			if (sync_type != (tp->t_flag & T_WAITCVSEM)) {
				ASSERT(sync_type == 0);
				/*
				 * assert that this can happen only for mutexes
				 * i.e. sync_type == 0, for correctly written
				 * user programs.
				 */
				disp_lock_exit(&sqh->sq_lock);
				return (0);
			}
			*waiters = iswanted(tp->t_link, lwpchan);
			sleepq_unlink(tpp, tp);
			tp->t_wchan0 = NULL;
			tp->t_wchan = NULL;
			tp->t_sobj_ops = NULL;
			THREAD_TRANSITION(tp);	/* drops sleepq lock */
			CL_WAKEUP(tp);
			thread_unlock(tp);	/* drop run queue lock */
			return (1);
		}
		tpp = &tp->t_link;
	}
	*waiters = 0;
	disp_lock_exit(&sqh->sq_lock);
	return (0);
}

static void
lwp_release_all(lwpchan_t *lwpchan)
{
	sleepq_head_t	*sqh;
	kthread_t *tp;
	kthread_t **tpp;

	sqh = lwpsqhash(lwpchan);
	disp_lock_enter(&sqh->sq_lock);		/* lock sleep q queue */
	tpp = &sqh->sq_queue.sq_first;
	while ((tp = *tpp) != NULL) {
		if (tp->t_lwpchan.lc_wchan0 == lwpchan->lc_wchan0 &&
		    tp->t_lwpchan.lc_wchan == lwpchan->lc_wchan) {
			sleepq_unlink(tpp, tp);
			tp->t_wchan0 = NULL;
			tp->t_wchan = NULL;
			tp->t_sobj_ops = NULL;
			CL_WAKEUP(tp);
			thread_unlock_high(tp);	/* release run queue lock */
		} else {
			tpp = &tp->t_link;
		}
	}
	disp_lock_exit(&sqh->sq_lock);		/* drop sleep q lock */
}

/*
 * unblock a lwp that is trying to acquire this mutex. the blocked
 * lwp resumes and retries to acquire the lock.
 */
int
lwp_mutex_wakeup(lwp_mutex_t *lp)
{
	proc_t *p = ttoproc(curthread);
	lwpchan_t lwpchan;
	uchar_t waiters;
	volatile int locked = 0;
	volatile int mapped = 0;
	volatile uint8_t type = 0;
	caddr_t lname;		/* logical name of mutex (vnode + offset) */
	label_t ljb;
	int error = 0;

	if ((caddr_t)lp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_mutex_t object is
	 * defined to be MAP_PRIVATE, and type is USYNC_PROCESS
	 */
	fuword8_noerr(&lp->mutex_type, (uint8_t *)&type);
	suword8_noerr(&lp->mutex_type, type);
	lname = (caddr_t)&lp->mutex_type;
	if (!get_lwpchan(curproc->p_as, lname, type, &lwpchan,
							LWPCHAN_MPPOOL)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type, LWPCHAN_MPPOOL);
	locked = 1;
	/*
	 * Always wake up an lwp (if any) waiting on lwpchan. The woken lwp will
	 * re-try the lock in _lwp_mutex_lock(). The call to lwp_release() may
	 * fail.  If it fails, do not write into the waiter bit.
	 * The call to lwp_release() might fail due to one of three reasons:
	 *
	 * 	1. due to the thread which set the waiter bit not actually
	 *	   sleeping since it got the lock on the re-try. The waiter
	 *	   bit will then be correctly updated by that thread. This
	 *	   window may be closed by reading the wait bit again here
	 *	   and not calling lwp_release() at all if it is zero.
	 *	2. the thread which set the waiter bit and went to sleep
	 *	   was woken up by a signal. This time, the waiter recomputes
	 *	   the wait bit in the return with EINTR code.
	 *	3. the waiter bit read by lwp_mutex_wakeup() was in
	 *	   memory that has been re-used after the lock was dropped.
	 *	   In this case, writing into the waiter bit would cause data
	 *	   corruption.
	 */
	if (lwp_release(&lwpchan, &waiters, 0) == 1) {
		suword8_noerr(&lp->mutex_waiters, waiters);
	}
	lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
	return (error);
}

/*
 * lwp_cond_wait() has three arguments, a pointer to a condition variable,
 * a pointer to a mutex, and a pointer to a timeval for a timed wait.
 * The kernel puts the lwp to sleep on a unique pair of caddr_t's called
 * an lwpchan, returned by get_lwpchan().
 */
int
lwp_cond_wait(lwp_cond_t *cv, lwp_mutex_t *mp, timestruc_t *tsp)
{
	timestruc_t ts;			/* timed wait value */
	kthread_t *t = curthread;
	klwp_t *lwp = ttolwp(t);
	proc_t *p = ttoproc(t);
	lwpchan_t cv_lwpchan, m_lwpchan;
	caddr_t timedwait;
	volatile uint16_t type = 0;
	uint8_t mtype;
	caddr_t cv_lname;		/* logical name of lwp_cond_t */
	caddr_t mp_lname;		/* logical name of lwp_mutex_t */
	uchar_t waiters, wt;
	int error = 0;
	volatile timeout_id_t id = 0;	/* timeout's id */
	clock_t tim, runtime;
	volatile int locked = 0;
	volatile int m_locked = 0;
	volatile int cvmapped = 0;
	volatile int mpmapped = 0;
	label_t ljb;
	int scblock;
	volatile int no_lwpchan = 1;
	int imm_timeout;

	if ((caddr_t)cv >= p->p_as->a_userlimit ||
	    (caddr_t)mp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (t->t_proc_flag & TP_MSACCT)
		(void) new_mstate(t, LMS_USER_LOCK);

	if (on_fault(&ljb)) {
		if (no_lwpchan) {
			error = set_errno(EFAULT);
			goto out;
		}
		if (m_locked) {
			m_locked = 0;
			lwpchan_unlock(&m_lwpchan, type, LWPCHAN_MPPOOL);
		}
		if (locked) {
			locked = 0;
			lwpchan_unlock(&cv_lwpchan, type, LWPCHAN_CVPOOL);
		}
		/*
		 * set up another on_fault() for a possible fault
		 * on the user lock accessed at "efault"
		 */
		if (on_fault(&ljb)) {
			if (m_locked) {
				m_locked = 0;
				lwpchan_unlock(&m_lwpchan, type,
							LWPCHAN_MPPOOL);
			}
			goto out;
		}
		error = set_errno(EFAULT);
		goto efault;
	}


	/*
	 * Force Copy-on-write fault if lwp_cond_t and lwp_mutex_t
	 * objects are defined to be MAP_PRIVATE, and are USYNC_PROCESS
	 */
	fuword8_noerr(&mp->mutex_type, (uint8_t *)&mtype);
	if (UPIMUTEX(mtype) == 0) {
		suword8_noerr(&mp->mutex_type, mtype);
		mp_lname = (caddr_t)&mp->mutex_type;
		/* convert user level mutex, "mp", to a unique lwpchan */
		/* check if mtype is ok to use below, instead of type from cv */
		if (!get_lwpchan(p->p_as, mp_lname, mtype, &m_lwpchan,
		    LWPCHAN_MPPOOL)) {
			error = set_errno(EFAULT);
			goto out;
		}
	}
	fuword16_noerr(&cv->cond_type, (uint16_t *)&type);
	suword16_noerr(&cv->cond_type, type);
	cv_lname = (caddr_t)&cv->cond_type;
	/* convert user level condition variable, "cv", to a unique lwpchan */
	if (!get_lwpchan(p->p_as, cv_lname, type, &cv_lwpchan,
							LWPCHAN_CVPOOL)) {
		error = set_errno(EFAULT);
		goto out;
	}
	no_lwpchan = 0;
	if ((timedwait = (caddr_t)tsp) != NULL) {
		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (copyin(timedwait, &ts, sizeof (ts))) {
				error = set_errno(EFAULT);
				goto out;
			}
		} else {
			timestruc32_t ts32;
			if (copyin(timedwait, &ts32, sizeof (ts32))) {
				error = set_errno(EFAULT);
				goto out;
			}
			TIMESPEC32_TO_TIMESPEC(&ts, &ts32);
		}
		tim = TIMESTRUC_TO_TICK(&ts);
		runtime = tim + lbolt;
		id = timeout((void (*)(void *))setrun, t, tim);
	}
	if (p->p_warea) {
		cvmapped = pr_mappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);
		if (UPIMUTEX(mtype) == 0) {
			mpmapped = pr_mappage((caddr_t)mp, sizeof (*mp),
			    S_WRITE, 1);
		}
	}
	/*
	 * lwpchan_lock ensures that the calling lwp is put to sleep atomically
	 * with respect to a possible wakeup which is a result of either
	 * an lwp_cond_signal() or an lwp_cond_broadcast().
	 *
	 * What's misleading, is that the lwp is put to sleep after the
	 * condition variable's mutex is released.  This is OK as long as
	 * the release operation is also done while holding lwpchan_lock.
	 * The lwp is then put to sleep when the possibility of pagefaulting
	 * or sleeping is completely eliminated.
	 */
	lwpchan_lock(&cv_lwpchan, type, LWPCHAN_CVPOOL);
	locked = 1;
	if (UPIMUTEX(mtype) == 0) {
		lwpchan_lock(&m_lwpchan, type, LWPCHAN_MPPOOL);
		m_locked = 1;
		suword8_noerr(&cv->cond_waiters, 1);
		/*
		 * unlock the condition variable's mutex. (pagefaults are
		 * possible here.)
		 */
		ulock_clear(&mp->mutex_lockw);
		fuword8_noerr(&mp->mutex_waiters, &wt);
		if (wt != 0) {
			/*
			 * Given the locking of lwpchan_lock around the release
			 * of the mutex and checking for waiters, the following
			 * call to lwp_release() can fail ONLY if the lock
			 * acquirer is interrupted after setting the waiter bit,
			 * calling lwp_block() and releasing lwpchan_lock.
			 * In this case, it could get pulled off the lwp sleep
			 * q (via setrun()) before the following call to
			 * lwp_release() occurs. In this case, the lock
			 * requestor will update the waiter bit correctly by
			 * re-evaluating it.
			 */
			if (lwp_release(&m_lwpchan, &waiters, 0) > 0)
				suword8_noerr(&mp->mutex_waiters, waiters);
		}
		m_locked = 0;
		lwpchan_unlock(&m_lwpchan, type, LWPCHAN_MPPOOL);
	} else {
		suword8_noerr(&cv->cond_waiters, 1);
		error = lwp_upimutex_unlock(mp, mtype);
		if (error) { /* if the upimutex unlock failed */
			lwpchan_unlock(&cv_lwpchan, type, LWPCHAN_CVPOOL);
			goto out;
		}
	}
	if (mpmapped) {
		pr_unmappage((caddr_t)mp, sizeof (*mp), S_WRITE, 1);
		mpmapped = 0;
	}
	if (cvmapped) {
		pr_unmappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);
		cvmapped = 0;
	}
	/*
	 * Put the lwp in an orderly state for debugging
	 * before calling schedctl_block(NULL).
	 */
	prstop(PR_REQUESTED, 0);
	if ((scblock = schedctl_check(t, SC_BLOCK)) != 0)
		(void) schedctl_block(NULL);
	t->t_flag |= T_WAITCVSEM;
	lwp_block(&cv_lwpchan);
	/*
	 * Nothing should happen to cause the lwp to go to sleep
	 * until after it returns from swtch().
	 */
	lwpchan_unlock(&cv_lwpchan, type, LWPCHAN_CVPOOL);
	locked = 0;
	no_fault();
	imm_timeout = 0;
	if (ISSIG(t, JUSTLOOKING) || MUSTRETURN(p, t)) {
		setrun(t);
	} else if (timedwait && runtime - lbolt <= 0) {
		imm_timeout = 1;
		setrun(t);
	}
	swtch();
	t->t_flag &= ~(T_WAITCVSEM | T_WAKEABLE);
	if (timedwait)
		tim = untimeout(id);
	if (scblock)
		schedctl_unblock();
	setallwatch();
	if (ISSIG(t, FORREAL) || lwp->lwp_sysabort || MUSTRETURN(p, t))
		error = set_errno(EINTR);
	else if (timedwait && (tim == -1 || imm_timeout))
		error = set_errno(ETIME);
	lwp->lwp_asleep = 0;
	lwp->lwp_sysabort = 0;
	/* mutex is re-acquired by caller */
	return (error);

efault:
	/*
	 * make sure that the user level lock is dropped before
	 * returning to caller, since the caller always re-acquires it.
	 */
	if (UPIMUTEX(mtype) == 0) {
		lwpchan_lock(&m_lwpchan, type, LWPCHAN_MPPOOL);
		m_locked = 1;
		ulock_clear(&mp->mutex_lockw);
		fuword8_noerr(&mp->mutex_waiters, &wt);
		if (wt != 0) {
			/*
			 * See comment above on lock clearing and lwp_release()
			 * success/failure.
			 */
			if (lwp_release(&m_lwpchan, &waiters, 0) > 0)
				suword8_noerr(&mp->mutex_waiters, waiters);
		}
		m_locked = 0;
		lwpchan_unlock(&m_lwpchan, type, LWPCHAN_MPPOOL);
	} else {
		(void) lwp_upimutex_unlock(mp, mtype);
	}
out:
	/* Cancel outstanding timeout */
	if (id != (timeout_id_t)0)
		tim = untimeout(id);
	no_fault();
	if (mpmapped)
		pr_unmappage((caddr_t)mp, sizeof (*mp), S_WRITE, 1);
	if (cvmapped)
		pr_unmappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);
	return (error);
}

/*
 * wakeup one lwp that's blocked on this condition variable.
 */
int
lwp_cond_signal(lwp_cond_t *cv)
{
	proc_t *p = ttoproc(curthread);
	lwpchan_t lwpchan;
	uchar_t waiters, wt;
	volatile uint16_t type = 0;
	caddr_t cv_lname;		/* logical name of lwp_cond_t */
	volatile int locked = 0;
	volatile int mapped = 0;
	label_t ljb;
	int error = 0;

	if ((caddr_t)cv >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type, LWPCHAN_CVPOOL);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_cond_t object is
	 * defined to be MAP_PRIVATE, and is USYNC_PROCESS.
	 */
	fuword16_noerr(&cv->cond_type, (uint16_t *)&type);
	suword16_noerr(&cv->cond_type, type);
	cv_lname = (caddr_t)&cv->cond_type;
	if (!get_lwpchan(curproc->p_as, cv_lname, type, &lwpchan,
							LWPCHAN_CVPOOL)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type, LWPCHAN_CVPOOL);
	locked = 1;
	fuword8_noerr(&cv->cond_waiters, &wt);
	if (wt != 0) {
		/*
		 * The following call to lwp_release() might fail but it is
		 * OK to write into the waiters bit below, since the memory
		 * could not have been re-used or unmapped (for correctly
		 * written user programs) as in the case of lwp_mutex_wakeup().
		 * For an incorrect program, we should not care about data
		 * corruption since this is just one instance of other places
		 * where corruption can occur for such a program. Of course
		 * if the memory is unmapped, normal fault recovery occurs.
		 */
		(void) lwp_release(&lwpchan, &waiters, T_WAITCVSEM);
		suword8_noerr(&cv->cond_waiters, waiters);
	}
	lwpchan_unlock(&lwpchan, type, LWPCHAN_CVPOOL);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);
	return (error);
}

/*
 * wakeup every lwp that's blocked on this condition variable.
 */
int
lwp_cond_broadcast(lwp_cond_t *cv)
{
	proc_t *p = ttoproc(curthread);
	lwpchan_t lwpchan;
	volatile uint16_t type = 0;
	caddr_t cv_lname;		/* logical name of lwp_cond_t */
	volatile int locked = 0;
	volatile int mapped = 0;
	label_t ljb;
	uchar_t wt;
	int error = 0;

	if ((caddr_t)cv >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type, LWPCHAN_CVPOOL);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_cond_t object is
	 * defined to be MAP_PRIVATE, and is USYNC_PROCESS.
	 */
	fuword16_noerr(&cv->cond_type, (uint16_t *)&type);
	suword16_noerr(&cv->cond_type, type);
	cv_lname = (caddr_t)&cv->cond_type;
	if (!get_lwpchan(curproc->p_as, cv_lname, type, &lwpchan,
							LWPCHAN_CVPOOL)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type, LWPCHAN_CVPOOL);
	locked = 1;
	fuword8_noerr(&cv->cond_waiters, &wt);
	if (wt != 0) {
		lwp_release_all(&lwpchan);
		suword8_noerr(&cv->cond_waiters, 0);
	}
	lwpchan_unlock(&lwpchan, type, LWPCHAN_CVPOOL);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);
	return (error);
}

int
lwp_sema_trywait(volatile lwp_sema_t *sp)
{
	kthread_t *t = curthread;
	proc_t *p = ttoproc(t);
	label_t ljb;
	volatile int locked = 0;
	volatile int mapped = 0;
	volatile uint16_t type = 0;
	int tmpcnt;
	caddr_t sp_lname;		/* logical name of lwp_sema_t */
	lwpchan_t lwpchan;
	int error = 0;

	if ((caddr_t)sp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)sp, sizeof (*sp), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type, LWPCHAN_CVPOOL);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_sema_t object is
	 * defined to be MAP_PRIVATE, and is USYNC_PROCESS.
	 */
	fuword16_noerr((void *)&sp->sema_type, (uint16_t *)&type);
	suword16_noerr((void *)&sp->sema_type, type);
	sp_lname = (caddr_t)&sp->sema_type;
	if (!get_lwpchan(p->p_as, sp_lname, type, &lwpchan,
							LWPCHAN_CVPOOL)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type, LWPCHAN_CVPOOL);
	locked = 1;
	fuword32_noerr((void *)&sp->sema_count, (uint32_t *)&tmpcnt);
	if (tmpcnt > 0) {
		suword32_noerr((void *)&sp->sema_count, --tmpcnt);
		error = 0;
	} else {
		error = set_errno(EBUSY);
	}
	lwpchan_unlock(&lwpchan, type, LWPCHAN_CVPOOL);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)sp, sizeof (*sp), S_WRITE, 1);
	return (error);
}

int
lwp_sema_wait(volatile lwp_sema_t *sp)
{
	kthread_t *t = curthread;
	klwp_t *lwp = ttolwp(t);
	proc_t *p = ttoproc(t);
	label_t ljb;
	volatile int locked = 0;
	volatile int mapped = 0;
	volatile uint16_t type = 0;
	int tmpcnt;
	caddr_t sp_lname;		/* logical name of lwp_sema_t */
	lwpchan_t lwpchan;
	int scblock;
	int error = 0;

	if ((caddr_t)sp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)sp, sizeof (*sp), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type, LWPCHAN_CVPOOL);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_sema_t object is
	 * defined to be MAP_PRIVATE, and is USYNC_PROCESS.
	 */
	fuword16_noerr((void *)&sp->sema_type, (uint16_t *)&type);
	suword16_noerr((void *)&sp->sema_type, type);
	sp_lname = (caddr_t)&sp->sema_type;
	if (!get_lwpchan(p->p_as, sp_lname, type, &lwpchan,
							LWPCHAN_CVPOOL)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type, LWPCHAN_CVPOOL);
	locked = 1;
	fuword32_noerr((void *)&sp->sema_count, (uint32_t *)&tmpcnt);
	while (tmpcnt == 0) {
		suword8_noerr((void *)&sp->sema_waiters, 1);
		if (mapped) {
			pr_unmappage((caddr_t)sp, sizeof (*sp),
				S_WRITE, 1);
			mapped = 0;
		}
		/*
		 * Put the lwp in an orderly state for debugging
		 * before calling schedctl_block(NULL).
		 */
		prstop(PR_REQUESTED, 0);
		if ((scblock = schedctl_check(t, SC_BLOCK)) != 0)
			(void) schedctl_block(NULL);
		t->t_flag |= T_WAITCVSEM;
		lwp_block(&lwpchan);
		/*
		 * Nothing should happen to cause the lwp to sleep
		 * again until after it returns from swtch().
		 */
		lwpchan_unlock(&lwpchan, type, LWPCHAN_CVPOOL);
		locked = 0;
		if (ISSIG(t, JUSTLOOKING) || MUSTRETURN(p, t))
			setrun(t);
		swtch();
		t->t_flag &= ~(T_WAITCVSEM | T_WAKEABLE);
		if (scblock)
			schedctl_unblock();
		setallwatch();
		if (ISSIG(t, FORREAL) ||
		    lwp->lwp_sysabort || MUSTRETURN(p, t)) {
			lwp->lwp_asleep = 0;
			lwp->lwp_sysabort = 0;
			no_fault();
			return (set_errno(EINTR));
		}
		lwp->lwp_asleep = 0;
		if (p->p_warea)
			mapped = pr_mappage((caddr_t)sp,
				sizeof (*sp), S_WRITE, 1);
		lwpchan_lock(&lwpchan, type, LWPCHAN_CVPOOL);
		locked = 1;
		fuword32_noerr((void *)&sp->sema_count, (uint32_t *)&tmpcnt);
	}
	suword32_noerr((void *)&sp->sema_count, --tmpcnt);
	lwpchan_unlock(&lwpchan, type, LWPCHAN_CVPOOL);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)sp, sizeof (*sp), S_WRITE, 1);
	return (error);
}

int
lwp_sema_post(lwp_sema_t *sp)
{
	proc_t *p = ttoproc(curthread);
	uchar_t waiters;
	label_t ljb;
	volatile int locked = 0;
	volatile int mapped = 0;
	volatile uint16_t type = 0;
	caddr_t sp_lname;		/* logical name of lwp_sema_t */
	int tmpcnt;
	lwpchan_t lwpchan;
	uchar_t wt;
	int error = 0;

	if ((caddr_t)sp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)sp, sizeof (*sp), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type, LWPCHAN_CVPOOL);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_sema_t object is
	 * defined to be MAP_PRIVATE, and is USYNC_PROCESS.
	 */
	fuword16_noerr(&sp->sema_type, (uint16_t *)&type);
	suword16_noerr(&sp->sema_type, type);
	sp_lname = (caddr_t)&sp->sema_type;
	if (!get_lwpchan(curproc->p_as, sp_lname, type, &lwpchan,
							LWPCHAN_CVPOOL)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type, LWPCHAN_CVPOOL);
	locked = 1;
	/*
	 * sp->waiters is only a hint. lwp_release() does nothing
	 * if there is no one waiting. The value of waiters is
	 * then set to zero.
	 */
	fuword8_noerr(&sp->sema_waiters, &wt);
	if (wt != 0) {
		(void) lwp_release(&lwpchan, &waiters, T_WAITCVSEM);
		suword8_noerr(&sp->sema_waiters, waiters);
	}
	fuword32_noerr(&sp->sema_count, (uint32_t *)&tmpcnt);
	if (tmpcnt == _SEM_VALUE_MAX) {
		error = set_errno(EOVERFLOW);
	} else {
		suword32_noerr(&sp->sema_count, ++tmpcnt);
	}
	lwpchan_unlock(&lwpchan, type, LWPCHAN_CVPOOL);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)sp, sizeof (*sp), S_WRITE, 1);
	return (error);
}
/*
 * Return the owner of the user-level s-object.
 * Since we can't really do this, return NULL.
 */
/* ARGSUSED */
static kthread_t *
lwpsobj_owner(caddr_t sobj)
{
	return ((kthread_t *)NULL);
}

/*
 * Wake up a thread asleep on a user-level synchronization
 * object.
 */
static void
lwp_unsleep(kthread_t *t)
{
	ASSERT(THREAD_LOCK_HELD(t));
	if (t->t_wchan0 != NULL) {
		sleepq_head_t *sqh;
		sleepq_t *sqp = t->t_sleepq;

		if (sqp != NULL) {
			sqh = lwpsqhash(&t->t_lwpchan);
			ASSERT(&sqh->sq_queue == sqp);
			sleepq_unsleep(t);
			disp_lock_exit_high(&sqh->sq_lock);
			CL_SETRUN(t);
			return;
		}
	}
	cmn_err(CE_PANIC, "lwp_unsleep: thread %p not on sleepq", (void *)t);
}

/*
 * Change the priority of a thread asleep on a user-level
 * synchronization object. To maintain proper priority order,
 * we:
 *	o dequeue the thread.
 *	o change its priority.
 *	o re-enqueue the thread.
 * Assumption: the thread is locked on entry.
 */
static void
lwp_change_pri(kthread_t *t, pri_t pri, pri_t *t_prip)
{
	ASSERT(THREAD_LOCK_HELD(t));
	if (t->t_wchan0 != NULL) {
		sleepq_t   *sqp = t->t_sleepq;

		sleepq_dequeue(t);
		*t_prip = pri;
		sleepq_insert(sqp, t);
	} else
		panic("lwp_change_pri: %p not on a sleep queue", (void *)t);
}

/*
 * Clean up a locked a robust mutex
 */
static void
lwp_mutex_cleanup(lwpchan_entry_t *ent, uint16_t lockflg)
{
	uint16_t flag;
	uchar_t waiters;
	label_t ljb;
	pid_t owner_pid;
	lwp_mutex_t *lp;
	volatile int locked = 0;
	volatile int mapped = 0;
	proc_t *p = curproc;

	lp = LOCKADDR(ent->lwpchan_addr);
	if (p->p_warea)
		mapped = pr_mappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&ent->lwpchan_lwpchan,
					LWPCHAN_GLOBAL, LWPCHAN_MPPOOL);
		goto out;
	}
	fuword32_noerr((pid_t *)&(lp->mutex_ownerpid), (uint32_t *)&owner_pid);
	if (!(ent->lwpchan_type & USYNC_PROCESS_ROBUST) ||
		(owner_pid != curproc->p_pidp->pid_id)) {
		goto out;
	}
	lwpchan_lock(&ent->lwpchan_lwpchan, LWPCHAN_GLOBAL, LWPCHAN_MPPOOL);
	locked = 1;
	fuword16_noerr(&(lp->mutex_flag), &flag);
	if ((flag & (LOCK_OWNERDEAD | LOCK_UNMAPPED)) == 0) {
		flag |= lockflg;
		suword16_noerr(&(lp->mutex_flag), flag);
	}
	suword32_noerr(&(lp->mutex_ownerpid), NULL);
	ulock_clear(&lp->mutex_lockw);
	fuword8_noerr(&(lp->mutex_waiters), &waiters);
	if (waiters && lwp_release(&ent->lwpchan_lwpchan, &waiters, 0)) {
		suword8_noerr(&lp->mutex_waiters, waiters);
	}
	lwpchan_unlock(&ent->lwpchan_lwpchan, LWPCHAN_GLOBAL, LWPCHAN_MPPOOL);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
}

/*
 * Register the mutex and initialize the mutex if it is not already
 */
int
lwp_mutex_init(lwp_mutex_t *lp, int type)
{
	proc_t *p = curproc;
	int error = 0;
	volatile int locked = 0;
	volatile int mapped = 0;
	label_t ljb;
	uint16_t flag;
	caddr_t lname;		/* logical name of mutex (vnode + offset) */
	lwpchan_t lwpchan;

	if ((caddr_t)lp >= (caddr_t)USERLIMIT)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_mutex_t object is
	 * defined to be MAP_PRIVATE and it was initialized to
	 * USYNC_PROCESS.
	 */
	suword8_noerr(&lp->mutex_type, type);
	lname = (caddr_t)&lp->mutex_type;
	mutex_enter(&p->p_lcp_mutexinitlock);
	if (!get_lwpchan(curproc->p_as, lname, type, &lwpchan,
							LWPCHAN_MPPOOL)) {
		mutex_exit(&p->p_lcp_mutexinitlock);
		error = set_errno(EFAULT);
		goto out;
	}
	mutex_exit(&p->p_lcp_mutexinitlock);
	lwpchan_lock(&lwpchan, type, LWPCHAN_MPPOOL);
	locked = 1;
	fuword16_noerr(&(lp->mutex_flag), &flag);
	if (flag & LOCK_INITED) {
		if (flag & (LOCK_OWNERDEAD | LOCK_UNMAPPED)) {
			flag &= ~(LOCK_OWNERDEAD | LOCK_UNMAPPED);
			suword16_noerr(&(lp->mutex_flag), flag);
			locked = 0;
			lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
			goto out;
		} else
			error = set_errno(EBUSY);
	} else {
		suword8_noerr(&(lp->mutex_waiters), NULL);
		suword8_noerr(&(lp->mutex_lockw), NULL);
		suword16_noerr(&(lp->mutex_flag), LOCK_INITED);
		suword32_noerr(&(lp->mutex_ownerpid), NULL);
	}
	locked = 0;
	lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
	return (error);
}

/*
 * non-blocking lwp_mutex_lock, returns EBUSY if can get the lock
 */
int
lwp_mutex_trylock(lwp_mutex_t *lp)
{
	kthread_t *t = curthread;
	proc_t *p = ttoproc(t);
	int error = 0;
	volatile int locked = 0;
	volatile int mapped = 0;
	label_t ljb;
	volatile uint8_t type = 0;
	uint16_t flag;
	caddr_t lname;		/* logical name of mutex (vnode + offset) */
	lwpchan_t lwpchan;

	if ((caddr_t)lp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (t->t_proc_flag & TP_MSACCT)
		(void) new_mstate(t, LMS_USER_LOCK);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
		error = set_errno(EFAULT);
		goto out;
	}
	fuword8_noerr(&lp->mutex_type, (uint8_t *)&type);
	if (UPIMUTEX(type)) {
		no_fault();
		error = lwp_upimutex_lock(lp, type, UPIMUTEX_TRY);
		return (error);
	}
	/*
	 * Force Copy-on-write fault if lwp_mutex_t object is
	 * defined to be MAP_PRIVATE and it was initialized to
	 * USYNC_PROCESS.
	 */
	suword8_noerr(&lp->mutex_type, type);
	lname = (caddr_t)&lp->mutex_type;
	if (!get_lwpchan(curproc->p_as, lname, type, &lwpchan,
							LWPCHAN_MPPOOL)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type, LWPCHAN_MPPOOL);
	locked = 1;
	fuword8_noerr(&lp->mutex_type, (uint8_t *)&type);
	if (ROBUSTLOCK(type)) {
		fuword16_noerr((uint16_t *)(&lp->mutex_flag), &flag);
		if (flag & LOCK_NOTRECOVERABLE) {
			lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
			error =  set_errno(ENOTRECOVERABLE);
			goto out;
		}
	}

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);

	if (!ulock_try(&lp->mutex_lockw)) {
		error = set_errno(EBUSY);
		locked = 0;
		lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
		goto out;
	}
	if (ROBUSTLOCK(type)) {
		suword32_noerr((uint32_t *)&(lp->mutex_ownerpid),
		    p->p_pidp->pid_id);
		if (flag & LOCK_OWNERDEAD) {
			error = set_errno(EOWNERDEAD);
		} else if (flag & LOCK_UNMAPPED) {
			error = set_errno(ELOCKUNMAPPED);
		}
	}
	locked = 0;
	lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
	return (error);
}

/*
 * unlock the mutex and unblock lwps that is trying to acquire this mutex.
 * the blocked lwp resumes and retries to acquire the lock.
 */
int
lwp_mutex_unlock(lwp_mutex_t *lp)
{
	proc_t *p = ttoproc(curthread);
	lwpchan_t lwpchan;
	uchar_t waiters;
	volatile int locked = 0;
	volatile int mapped = 0;
	volatile uint8_t type = 0;
	caddr_t lname;		/* logical name of mutex (vnode + offset) */
	label_t ljb;
	uint16_t flag;
	int error = 0;

	if ((caddr_t)lp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
		error = set_errno(EFAULT);
		goto out;
	}
	fuword8_noerr(&lp->mutex_type, (uint8_t *)&type);
	if (UPIMUTEX(type)) {
		no_fault();
		error = lwp_upimutex_unlock(lp, type);
		return (error);
	}

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);

	/*
	 * Force Copy-on-write fault if lwp_mutex_t object is
	 * defined to be MAP_PRIVATE, and type is USYNC_PROCESS
	 */
	suword8_noerr(&lp->mutex_type, type);
	lname = (caddr_t)&lp->mutex_type;
	if (!get_lwpchan(curproc->p_as, lname, type, &lwpchan,
							LWPCHAN_MPPOOL)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type, LWPCHAN_MPPOOL);
	locked = 1;
	if (ROBUSTLOCK(type)) {
		fuword16_noerr(&(lp->mutex_flag), &flag);
		if (flag & (LOCK_OWNERDEAD |flag & LOCK_UNMAPPED)) {
			flag &= ~(LOCK_OWNERDEAD | LOCK_UNMAPPED);
			flag |= LOCK_NOTRECOVERABLE;
			suword16_noerr(&(lp->mutex_flag), flag);
		}
		suword32_noerr(&(lp->mutex_ownerpid), NULL);
	}
	ulock_clear(&lp->mutex_lockw);
	/*
	 * Always wake up an lwp (if any) waiting on lwpchan. The woken lwp will
	 * re-try the lock in _lwp_mutex_lock(). The call to lwp_release() may
	 * fail.  If it fails, do not write into the waiter bit.
	 * The call to lwp_release() might fail due to one of three reasons:
	 *
	 * 	1. due to the thread which set the waiter bit not actually
	 *	   sleeping since it got the lock on the re-try. The waiter
	 *	   bit will then be correctly updated by that thread. This
	 *	   window may be closed by reading the wait bit again here
	 *	   and not calling lwp_release() at all if it is zero.
	 *	2. the thread which set the waiter bit and went to sleep
	 *	   was woken up by a signal. This time, the waiter recomputes
	 *	   the wait bit in the return with EINTR code.
	 *	3. the waiter bit read by lwp_mutex_wakeup() was in
	 *	   memory that has been re-used after the lock was dropped.
	 *	   In this case, writing into the waiter bit would cause data
	 *	   corruption.
	 */
	fuword8_noerr(&(lp->mutex_waiters), &waiters);
	if (waiters) {
		if (ROBUSTLOCK(type) && (flag & LOCK_NOTRECOVERABLE)) {
			lwp_release_all(&lwpchan);
			suword8_noerr(&lp->mutex_waiters, 0);
		} else {
			if (lwp_release(&lwpchan, &waiters, 0) == 1) {
				suword8_noerr(&lp->mutex_waiters, waiters);

			}
		}
	}

	lwpchan_unlock(&lwpchan, type, LWPCHAN_MPPOOL);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
	return (error);
}
