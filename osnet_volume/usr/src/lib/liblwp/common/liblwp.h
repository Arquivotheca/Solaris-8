/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Implemention-specific include file for liblwp/libthread.
 * This is not to be seen by the clients of liblwp/libthread.
 */

#pragma ident	"@(#)liblwp.h	1.4	99/12/06 SMI"

/* <assert.h> keys off of NDEBUG */
#ifdef DEBUG
#undef	NDEBUG
#else
#define	NDEBUG
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>
#include <thread.h>
#include <pthread.h>
#include <link.h>
#include <sys/resource.h>
#include <sys/lwp.h>
#include <errno.h>
#include <assert.h>
#include <sys/asm_linkage.h>
#include <sys/regset.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <synch.h>
#include <door.h>
#include <limits.h>
#include <sys/synch32.h>
#include <sys/schedctl.h>
#include <thread_db.h>
#include "tdb_agent.h"

#define	CVWAITERS(cvp)	((cvp)->cond_waiters)

#define	ASSERT(ex)	assert(ex)

extern	int	liblwp_initialized;
extern	size_t	_lpagesize;
extern	int	_lsemvaluemax;
extern	int	_libpthread_loaded;
extern	sigset_t fillset;
extern	int	mutex_adaptive_spin;
extern	int	ncpus;		/* number of on-line cpus */

extern	pid_t	_lpid;		/* the current process's pid */

/* double the default stack size for 64-bit processes */
#ifdef _LP64
#define	MINSTACK	(8 * 1024)
#define	DEFAULTSTACK	(2 * 1024 * 1024)
#else
#define	MINSTACK	(4 * 1024)
#define	DEFAULTSTACK	(1024 * 1024)
#endif
#define	TSD_NKEYS	_POSIX_THREAD_KEYS_MAX

#define	THREAD_MIN_PRIORITY	0
#define	THREAD_MAX_PRIORITY	127

#define	PRIO_SET	0
#define	PRIO_INHERIT	1
#define	PRIO_DISINHERIT	2

#define	MUTEX_TRY	0
#define	MUTEX_LOCK	1

#ifdef __i386
typedef struct {
	greg_t	rs_pc;
	greg_t	rs_bp;
	greg_t	rs_edi;
	greg_t	rs_esi;
	greg_t	rs_ebx;
	greg_t	rs_sp;
} savedregs_t;
#endif	/* __i386 */

#ifdef __ia64
typedef struct {
	greg_t	rs_pc;
	greg_t	rs_r1;
	greg_t	rs_r4;
	greg_t	rs_r8;
	greg_t	rs_r9;
	greg_t	rs_r10;
	greg_t	rs_r11;
	greg_t	rs_sp;
	greg_t	rs_bspstore;
	greg_t	rs_bsp;
	greg_t	rs_r13;
	greg_t	rs_r14;
	greg_t	rs_r15;
} savedregs_t;
#endif	/* __ia64 */

#ifdef __sparc
typedef struct {
	greg_t	rs_pc;
	greg_t	rs_sp;
	greg_t	rs_o7;
	greg_t	rs_g1;
	greg_t	rs_g2;
	greg_t	rs_g3;
	greg_t	rs_g4;
} savedregs_t;
#endif	/* __sparc */

/*
 * System-defined limit on number of reserved fast TLS slots.
 * (A Sun private contract interface.)
 */
#define	MAX_RESV_TLS	8

/*
 * Memory block for chain of owned ceiling mutexes.
 */
typedef struct mxchain {
	struct mxchain	*mxchain_next;
	mutex_t		*mxchain_mx;
} mxchain_t;

typedef struct ulwp {
	struct ulwp	*ul_forw;	/* forw, back all_lwps list, */
	struct ulwp	*ul_back;	/* protected by link_lock */
	struct ulwp	*ul_next;	/* list to keep track of stacks */
	struct ulwp	*ul_hash;	/* hash chain linked list */
	void		*ul_rval;	/* return value from thr_exit() */
	caddr_t		ul_stk;		/* mapping base of the stack */
	size_t		ul_mapsiz;	/* mapping size of the stack */
	size_t		ul_guardsize;	/* normally _lpagesize */
	uintptr_t	ul_stktop;	/* broken thr_stksegment() interface */
	size_t		ul_stksiz;	/* broken thr_stksegment() interface */
	int		ul_ix;		/* hash index */
	lwpid_t		ul_lwpid;	/* thread id, aka the lwp id */
	pri_t		ul_pri;		/* priority known to the library */
	pri_t		ul_mappedpri;	/* priority known to the application */
	char		ul_policy;	/* scheduling policy */
	char		ul_pri_mapped;	/* != 0 means ul_mappedpri is valid */
	char		ul_mutator;	/* lwp is a mutator (java interface) */
	char		ul_pleasestop;	/* lwp requested to stop itself */
	char		ul_stop;	/* reason for stopping */
	char		ul_wanted;	/* someone is waiting for lwp to stop */
	char		ul_dead;	/* this lwp has called thr_exit */
	char		ul_unwind;	/* posix: unwind C++ stack */
	char		ul_detached;	/* THR_DETACHED at thread_create() */
	char		ul_was_detached; /* pthread_detach() was called */
	char		ul_stopping;	/* set by curthread: stopping self */
	char		ul_validregs;	/* values in ul_savedregs are valid */
	volatile int	ul_critical;	/* non-zero == in a critical region */
	volatile int	ul_cancelable;	/* _cancelon()/_canceloff() */
	volatile char	ul_cancel_pending;  /* pthread_cancel() was called */
	volatile char	ul_cancel_disabled; /* PTHREAD_CANCEL_DISABLE */
	volatile char	ul_cancel_async;    /* PTHREAD_CANCEL_ASYNCHRONOUS */
	volatile char	ul_save_async;	/* saved copy of ul_cancel_async */
	volatile char	ul_cursig;	/* deferred signal number */
	char		ul_created;	/* created suspemded */
	char		ul_replace;	/* replacement; must be free()d */
	char		ul_schedctl_called; /* ul_schedctl has been set up */
	int		ul_mutex;	/* count of mutex locks held */
	int		ul_rdlock;	/* count of rw read locks held */
	int		ul_wrlock;	/* count of rw write locks held */
	int		ul_errno;	/* per-thread errno */
	_cleanup_t	*ul_clnup_hdr;	/* head of cleanup handlers list */
	sc_shared_t	*ul_schedctl;	/* schedctl data */
	savedregs_t	ul_savedregs;	/* registers for thr_getstate() */
	int		ul_bindflags;	/* bind_guard() interface to ld.so.1 */
	int		ul_gs;		/* x86 only: value of %gs */
	struct ul_tsd {		/* this is read as a unit by libthread_db */
		int	nkey;		/* number of elements in tsd */
		void	**tsd;		/* thread-specific data */
	} ul_tsd;
		/* debugger (libthread_db) support */
	td_evbuf_t	ul_td_evbuf;	/* event buffer */
	char		ul_td_events_enable;	/* event mechanism enabled */
	char		ul_pad[3];
	int		ul_usropts;	/* flags given to thr_create() */
	void		(*ul_startpc)(); /* start func given to thr_create() */
	uintptr_t	ul_wchan;	/* synch object when sleeping */
	mxchain_t	*ul_mxchain;	/* chain of owned ceiling mutexes */
	pri_t		ul_epri;	/* effective scheduling priority */
	pri_t		ul_emappedpri;	/* effective mapped priority */
		/* the following members *must* be last in the structure */
		/* they are discarded when ulwp is replaced on thr_exit() */
	sigset_t	ul_prevmask;	/* deferred previous level mask */
	siginfo_t	ul_siginfo;	/* deferred siginfo */
	void		*ul_resvtls[MAX_RESV_TLS]; /* reserved for fast TLS */
} ulwp_t;

extern	ulwp_t	*all_lwps;	/* circular ul_forw/ul_back list of all lwps */
extern	int	nthreads;	/* total number of threads/lwps */
extern	int	ndaemons;	/* total number of THR_DAEMON threads/lwps */

/*
 * Parameters of the mutex, condvar, and ulwp (thread) hash tables.
 * Also known to libthread_db.
 */
typedef struct {
	mutex_t	*hash_lock;	/* lock per bucket */
	cond_t	*hash_cond;	/* convar per bucket */
	ulwp_t	**hash_bucket;	/* array of hash buckets */
	size_t	hash_size;	/* number of entries in each of the above */
} hash_table_t;

extern	hash_table_t	hash_table;

#ifdef _SYSCALL32	/* needed by libthread_db */

#if defined(__i386) || defined(__ia64)
typedef struct {
	greg32_t	rs_pc;
	greg32_t	rs_bp;
	greg32_t	rs_edi;
	greg32_t	rs_esi;
	greg32_t	rs_ebx;
	greg32_t	rs_sp;
} savedregs32_t;
#endif	/* __i386 */

#ifdef __sparc
typedef struct {
	greg32_t	rs_pc;
	greg32_t	rs_sp;
	greg32_t	rs_o7;
	greg32_t	rs_g1;
	greg32_t	rs_g2;
	greg32_t	rs_g3;
	greg32_t	rs_g4;
} savedregs32_t;
#endif	/* __sparc */

typedef struct ulwp32 {
	caddr32_t	ul_forw;	/* forw, back all_lwps list, */
	caddr32_t	ul_back;	/* protected by link_lock */
	caddr32_t	ul_next;	/* list to keep track of stacks */
	caddr32_t	ul_hash;	/* hash chain linked list */
	caddr32_t	ul_rval;	/* return value from thr_exit() */
	caddr32_t	ul_stk;		/* mapping base of the stack */
	size32_t	ul_mapsiz;	/* mapping size of the stack */
	size32_t	ul_guardsize;	/* normally _lpagesize */
	caddr32_t	ul_stktop;	/* broken thr_stksegment() interface */
	size32_t	ul_stksiz;	/* broken thr_stksegment() interface */
	int		ul_ix;		/* hash index */
	lwpid_t		ul_lwpid;	/* thread id, aka the lwp id */
	pri_t		ul_pri;		/* priority known to the library */
	pri_t		ul_mappedpri;	/* priority known to the application */
	char		ul_policy;	/* scheduling policy */
	char		ul_pri_mapped;	/* != 0 means ul_mappedpri is valid */
	char		ul_mutator;	/* lwp is a mutator (java interface) */
	char		ul_pleasestop;	/* lwp requested to stop itself */
	char		ul_stop;	/* reason for stopping */
	char		ul_wanted;	/* someone is waiting for lwp to stop */
	char		ul_dead;	/* this lwp has called thr_exit */
	char		ul_unwind;	/* posix: unwind C++ stack */
	char		ul_detached;	/* THR_DETACHED at thread_create() */
	char		ul_was_detached; /* pthread_detach() was called */
	char		ul_stopping;	/* set by curthread: stopping self */
	char		ul_validregs;	/* values in ul_savedregs are valid */
	volatile int	ul_critical;	/* non-zero == in a critical region */
	volatile int	ul_cancelable;	/* _cancelon()/_canceloff() */
	volatile char	ul_cancel_pending;  /* pthread_cancel() was called */
	volatile char	ul_cancel_disabled; /* PTHREAD_CANCEL_DISABLE */
	volatile char	ul_cancel_async;    /* PTHREAD_CANCEL_ASYNCHRONOUS */
	volatile char	ul_save_async;	/* saved copy of ul_cancel_async */
	volatile char	ul_cursig;	/* deferred signal number */
	char		ul_created;	/* created suspemded */
	char		ul_replace;	/* replacement; must be free()d */
	char		ul_schedctl_called; /* ul_schedctl has been set up */
	int		ul_mutex;	/* count of mutex locks held */
	int		ul_rdlock;	/* count of rw read locks held */
	int		ul_wrlock;	/* count of rw write locks held */
	int		ul_errno;	/* per-thread errno */
	caddr32_t	ul_clnup_hdr;	/* head of cleanup handlers list */
	caddr32_t	ul_schedctl;	/* schedctl data */
	savedregs32_t	ul_savedregs;	/* registers for thr_getstate() */
	int		ul_bindflags;	/* bind_guard() interface to ld.so.1 */
	int		ul_gs;		/* x86 only: value of %gs */
	struct ul_tsd32 {	/* this is read as a unit by libthread_db */
		int	nkey;		/* number of elements in tsd */
		caddr32_t tsd;		/* thread-specific data */
	} ul_tsd;
		/* debugger (libthread_db) support */
	td_evbuf32_t	ul_td_evbuf;	/* event buffer */
	char		ul_td_events_enable;	/* event mechanism enabled */
	char		ul_pad[3];
	int		ul_usropts;	/* flags given to thr_create() */
	caddr32_t	ul_startpc;	/* start func given to thr_create() */
	caddr32_t	ul_wchan;	/* synch object when sleeping */
	caddr32_t	ul_mxchain;	/* chain of owned ceiling mutexes */
	int		ul_epri;	/* effective scheduling priority */
	int		ul_emappedpri;	/* effective mapped priority */
		/* the following members *must* be last in the structure */
		/* they are discarded when ulwp is replaced on thr_exit() */
	sigset32_t	ul_prevmask;	/* deferred previous level mask */
	siginfo32_t	ul_siginfo;	/* deferred siginfo */
	caddr32_t	ul_resvtls[MAX_RESV_TLS]; /* reserved for fast TLS */
} ulwp32_t;

typedef struct {
	caddr32_t	hash_lock;
	caddr32_t	hash_cond;
	caddr32_t	hash_bucket;
	size32_t	hash_size;
} hash_table32_t;

#endif	/* _SYSCALL32 */

extern	ulwp_t	ulwp_one;

/* ul_stop values */
#define	TSTP_REGULAR	0x01	/* Stopped by thr_suspend */
#define	TSTP_MUTATOR	0x08	/* stopped by thr_suspend_*mutator* */
#define	TSTP_FORK	0x20	/* stopped by thr_suspend_fork */

/*
 * global thread-specific data (TSD).
 */
typedef struct {
	int 	numkeys;	/* number of keys used by thr_key_create() */
	int	maxkeys;	/* number of tsd_destructors[] allocated */
	void 	(**destructors)(void *);
} tsd_common_t;

#ifdef _SYSCALL32
typedef struct {
	int 		numkeys;
	int		maxkeys;
	caddr32_t	destructors;
} tsd_common32_t;
#endif

extern	tsd_common_t	tsd_common;

/*
 * Implementation-specific attribute types for pthread_mutexattr_init() etc.
 */

typedef	struct	_cvattr {
	int	pshared;
} cvattr_t;

typedef	struct	_mattr {
	int	pshared;
	int	protocol;
	int 	prioceiling;
	int	type;
	int	robustness;
} mattr_t;

typedef	struct	_thrattr {
	size_t	stksize;
	void	*stkaddr;
	int	detachstate;
	int 	scope;
	int 	prio;
	int 	policy;
	int 	inherit;
	size_t	guardsize;
} thrattr_t;

typedef	struct	_rwlattr {
	int	pshared;
} rwlattr_t;

/* _curthread() is inline for speed */
extern	ulwp_t		*_curthread(void);
#define	curthread	(_curthread())

extern	int	doing_fork1;	/* set non-zero while doing fork1() */
extern	void	*fork1_freelist;
extern	void	liblwp_free(void *);

/*
 * Implementation specific interfaces to libc
 */
extern int _libc_nanosleep(const struct timespec *, struct timespec *);

/*
 * Implementation functions.  Not visible outside of the library itself.
 */
extern	void	_save_nv_regs(savedregs_t *);
extern	void	getgregs(ulwp_t *, gregset_t);
extern	void	setgregs(ulwp_t *, gregset_t);
extern	void	panic(const char *);
extern	ulwp_t	*find_lwp(thread_t);
extern	void	ulwp_lock(ulwp_t *);
extern	void	ulwp_unlock(ulwp_t *);
extern	void	tsd_init(void);
extern	void	tsd_exit(void);
extern	void	_destroy_resv_tls(void);
extern	void	signal_init(void);
extern	void	mutex_setup(void);
extern	void	take_deferred_signal(int);
extern	void	setup_context(ucontext_t *, void (*func)(void *), void *arg,
			ulwp_t *ulwp, caddr_t stk, size_t stksize);
extern	void	*zmap(void *addr, size_t len, int prot, int flags);
#if defined(__sparc)
extern	void	_flush_windows(void);
#else
#define	_flush_windows()
#endif
extern	void	lwp_setprivate(void *);
#if defined(__i386)
/*
 * We use __setupgs() from libc but we never call __freegs()
 * because of brokenness in old (pre Solaris 8) versions of libc.
 * Instead, we retain the allocated selector values in ul_gs
 * for every ulwp_t that is allocated for an lwp and we reuse
 * the selector values when we reuse the ulwp_t structures.
 */
extern	int	__setupgs(void *);
extern	void	set_gs(int);
#endif

#ifdef DEBUG
extern	void	ultos(ulong_t, int, char *);
extern	void	__assert(const char *, const char *, int);
#endif

/* enter a critical section */
#define	enter_critical()	(curthread->ul_critical++)

/* exit a critical section, take deferred actions if necessary */
extern	void	exit_critical(void);

extern	void	_lwp_start(void);
extern	void	_lwp_terminate(void);
extern	void	_set_libc_interface(void);
extern	void	_set_rtld_interface(void);
extern	void	_libc_set_threaded(void);
extern	void	lmutex_unlock(mutex_t *);
extern	void	lmutex_lock(mutex_t *);
extern	void	set_mutex_owner(mutex_t *);
extern	void	_lprefork_handler(void);
extern	void	_lpostfork_child_handler(void);
extern	void	_lpostfork_parent_handler(void);
extern	void	_prefork_handler(void);
extern	void	_postfork_parent_handler(void);
extern	void	_postfork_child_handler(void);
extern	void	_postfork1_child(void);
extern	void	suspend_fork(void);
extern	void	continue_fork(int);
extern	void	_sigcancel(int, siginfo_t *, void *);
extern	void	_cancelon(void);
extern	void	_canceloff(void);
extern	void	_canceloff_nocancel(void);

extern	void	_t_cancel(void *);
extern	void	_tcancel_all(void *);
extern	void	_ex_clnup_handler(void *, void (*)(void *), void (*)(void *));
extern	void	_ex_unwind_local(void (*)(void *), void *);
#pragma unknown_control_flow(_t_cancel)
#pragma unknown_control_flow(_tcancel_all)
#pragma unknown_control_flow(_ex_clnup_handler)
#pragma unknown_control_flow(_ex_unwind_local)

/*
 * Prototypes for the strong versions of the interface functions
 */
extern	pid_t	_fork(void);
extern	pid_t	__fork(void);
extern	pid_t	_fork1(void);
extern	pid_t	__fork1(void);
extern	pid_t	_getpid(void);
extern	uid_t	_geteuid(void);
extern	int	_open(const char *, int, mode_t);
extern	int	_close(int);
extern	ssize_t	_read(int, void *, size_t);
extern	ssize_t	_write(int, const void *, size_t);
extern	void	*_memcpy(void *, const void *, size_t);
extern	void	*_memset(void *, int, size_t);
extern	int	_sigfillset(sigset_t *);
extern	int	_sigemptyset(sigset_t *);
extern	int	_sigaddset(sigset_t *, int);
extern	int	_sigdelset(sigset_t *, int);
extern	void	_yield(void);
extern	void	*_mmap(void *, size_t, int, int, int, off_t);
extern	int	_gettimeofday(struct timeval *, void *);
extern	int	_getrlimit(int, struct rlimit *);
extern	int	__lwp_continue(lwpid_t);
extern	int	__lwp_create(ucontext_t *, uint_t, lwpid_t *);
extern	int	__lwp_kill(lwpid_t, int);
extern	lwpid_t	__lwp_self(void);
extern	int	__lwp_suspend(lwpid_t);
extern	int	lwp_wait(lwpid_t, lwpid_t *);
extern	int	__lwp_wait(lwpid_t, lwpid_t *);

extern int _setcontext(const ucontext_t *);
#pragma unknown_control_flow(_setcontext)
extern int _getcontext(ucontext_t *);
#pragma unknown_control_flow(_getcontext)
extern	int	sigaction_internal(int, const struct sigaction *,
			struct sigaction *);
extern	void	delete_reserved_signals(sigset_t *);
extern	int	_sigprocmask(int, const sigset_t *, sigset_t *);
extern	void	__sighndlr(int, siginfo_t *, ucontext_t *, void (*)());
extern	caddr_t	__sighndlrend;
#pragma unknown_control_flow(__sighndlr)

extern	int	_lpthread_atfork(void (*) (), void (*) (), void (*) ());
extern	int	_pthread_atfork(void (*)(void), void (*)(void), void (*)(void));

extern	void	_pthread_exit(void *);

extern	int	_mutex_init(mutex_t *, int, void *);
extern	int	_mutex_lock(mutex_t *);
extern	int	_mutex_unlock(mutex_t *);
extern	int	_mutex_trylock(mutex_t *);
extern	int	_mutex_destroy(mutex_t *);
extern	void	_mutex_set_typeattr(mutex_t *, int);
extern	int	_mutex_held(mutex_t *);
extern	uchar_t	_lock_held(mutex_t *);
extern	int	mutex_lock_internal(mutex_t *, rwlock_t *, int);

extern	int	_sema_post(sema_t *);

extern	int	_cond_init(cond_t *, int, void *);
extern	int	_cond_wait(cond_t *, mutex_t *);
extern	int	_cond_timedwait(cond_t *, mutex_t *, timestruc_t *);
extern	int	_cond_signal(cond_t *);
extern	int	_cond_broadcast(cond_t *);
extern	int	_cond_destroy(cond_t *);
extern	int	cond_wait_internal(cond_t *, mutex_t *, rwlock_t *);
extern	int	cond_signal_internal(cond_t *, rwlock_t *);
extern	int	cond_broadcast_internal(cond_t *, rwlock_t *);

extern	int	_rwlock_init(rwlock_t *, int, void *);

extern	int	_thr_continue(thread_t);
extern	int	_thrp_create(void *, size_t, void *(*func)(void *), void *,
			long, thread_t *, pri_t, int, size_t);
extern	int	*_thr_errnop(void);
extern	int	_thr_getprio(thread_t, int *);
extern	int	_thr_getspecific(thread_key_t, void **);
extern	int	_thr_join(thread_t, thread_t *, void **);
extern	int	_thr_keycreate(thread_key_t *, void (*)(void *));
extern	int	_thr_key_delete(thread_key_t);
extern	int	_thr_main(void);
extern	thread_t _thr_self(void);
extern	int	_thr_getconcurrency(void);
extern	int	_thr_setconcurrency(int);
extern	int	_thr_setprio(thread_t, int);
extern	int	_thr_setspecific(thread_key_t, void *);
extern	int	_thr_stksegment(stack_t *);
extern	int	_thr_suspend(thread_t);

extern	void	_thr_terminate(void *);
extern	void	_thr_exit(void *);
extern	void	_thrp_exit(void);

extern	int	_thread_setschedparam_main(pthread_t, int,
			const struct sched_param *, int);
extern	int	_validate_rt_prio(int, int);
extern	int	_thrp_setlwpprio(lwpid_t, int, int);
extern	pri_t	_map_rtpri_to_gp(pri_t);
