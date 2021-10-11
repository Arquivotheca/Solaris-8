/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ifndef _LIBTHREAD_H
#define	_LIBTHREAD_H

#pragma ident	"@(#)libthread.h	1.157	99/12/06 SMI"

/*
 * libthread.h:
 *	struct thread and struct lwp definitions.
 */
#include <signal.h>
#include <siginfo.h>
#include <sys/types.h>
#include <string.h>
#include <sys/ucontext.h>
#include <sys/reg.h>
#include <sys/param.h>
#include <sys/asm_linkage.h>
#include <sys/frame.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <thread.h>
#include <sys/synch.h>
#include <sys/synch32.h>
#include <sys/lwp.h>
#include <utrace.h>
#include <debug.h>
#include <machlibthread.h>
#include <sys/psw.h>
#include <sys/schedctl.h>
#include <sys/resource.h>
#include <setjmp.h>
#include <underscore.h>
#include <unistd.h>

#include <thread_db.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * thread stack layout.
 *
 *	-----------------	high address
 *	|		|
 *	| struct thread |
 *	|		|
 *	|		|
 *	|		|
 *	|	tls	|
 *	|		|
 *	-----------------
 *	|		| <- 	thread stack bottom
 *	|		|
 *	|		|
 *	|		|
 *	|		|
 *	-----------------	low address
 */

/*
 * default stack allocation parameters
 *
 * DEFAULTSTACK is defined as 1MB for 32-bit applications,
 * and 2MB for those that are 64-bit.  The daemon threads also
 * have double the stack size in the 64-bit environment.
 */
#ifdef _LP64
#define	DEFAULTSTACK	0x200000	/* 2 MB stacks */
#define	DAEMON_STACK	0x4000		/* 16K stacks for daemons */
#else
#define	DEFAULTSTACK	0x100000	/* 1 MB stacks */
#define	DAEMON_STACK	0x2000		/* 8K stacks for daemons */
#endif /* _LP64 */
/*
 * Stack for door threads, which also transition into idle threads.
 */
#define	DOOR_STACK	(DAEMON_STACK * 2)

#define	BPW 32		/* number of bits per word */
#define	DEFAULTSTACKINCR	8
#define	MAXSTACKS		16
#ifdef TLS
extern int _etls;
#define	MINSTACK		(SA(MINFRAME + (int)&_etls))
#else
#define	MINSTACK		(SA(sizeof (struct thread) + 512))
#endif

/*
 * System-defined limit on number of reserved TLS slots which are
 * a private-contract interface.
 */
#define	MAX_RESV_TLS    8

/*
 * default stack cache definition.
 */
typedef struct _stkcache {
	int size;
	char *next;
	int busy;
	cond_t cv;
} stkcache_t;

/*
 * memory block for chain of owned ceiling mutexes
 */

typedef struct mxchain mxchain_t;

struct mxchain {
	mxchain_t *mxchain_next;
	mutex_t *mxchain_mx;
};

extern stkcache_t _defaultstkcache;
extern mutex_t _stkcachelock;

/*
 * thread priority range.
 */
#define	THREAD_MIN_PRIORITY	0		/* minimum scheduling pri */
#define	THREAD_MAX_PRIORITY	127		/* max scheduling priority */
#define	NPRI			(THREAD_MAX_PRIORITY - THREAD_MIN_PRIORITY+1)
#define	MAXRUNWORD		(NPRI/BPW)
#define	IDLE_THREAD_PRI		-1		/* idle thread's priority */

/*
 * Is LWP switching away from this thread?
 */
#define	ISSWITCHING(t)	((t)->t_ulflag & T_SWITCHING)

/*
 * Is thread temporarily bound to this LWP?
 */
#define	ISTEMPBOUND(t)	(((t)->t_ulflag & T_TEMPBOUND))

/*
 * Is the thread in _resetsig()?
 */
#define	ISINRESETSIG(t) (((t)->t_ulflag & T_INRESETSIG))

#define	ISTEMPRTBOUND(t)	(((t)->t_rtflag & T_TEMPRTBOUND))

/*
 * used to implement a callout mechanism.
 */
typedef struct callo {
	thread_t tid;
	char flag;
	char running;
	cond_t waiting;
	struct timeval time;
	void (*func)();
	uintptr_t arg;
	struct callo *forw;
	struct callo *backw;
} callo_t;

#ifdef _SYSCALL32_IMPL
typedef struct callo32 {
	thread_t tid;
	char flag;
	char running;
	cond_t waiting;
	struct timeval32 time;
	caddr32_t func;
	caddr32_t arg;
	caddr32_t forw;
	caddr32_t backw;
} callo32_t;
#endif /* _SYSCALL32_IMPL */

/* callout flags */
#define	CO_TIMER_OFF	0
#define	CO_TIMEDOUT	1
#define	CO_TIMER_ON	2

#define	ISTIMEDOUT(x)	((x)->flag == CO_TIMEDOUT)

/*
 * The structure containing per-thread event data.
 */
typedef struct {
	td_thr_events_t	eventmask;	/* Which events are enabled? */
	td_event_e	eventnum;	/* Most recent enabled event */
	void		*eventdata;	/* Param. for most recent event */
} td_evbuf_t;

#ifdef _SYSCALL32_IMPL
typedef struct {
	td_thr_events_t	eventmask;	/* Which events are enabled? */
	td_event_e	eventnum;	/* Most recent enabled event */
	caddr32_t	eventdata;	/* Param. for most recent event */
} td_evbuf32_t;
#endif /* _SYSCALL32_IMPL */

/*
 * The per-thread description of TSD.
 * Per thread TSD storage is allocated when required by the thread
 * not at the time of key creation.
 */
typedef struct tsd_thread {
	unsigned int	count;	/* number of allocated storage cells */
	void		**array; /* pointer to allocated storage cells */
} tsd_t;

#ifdef _SYSCALL32_IMPL
typedef struct tsd_thread32 {
	unsigned int	count;	/* number of allocated storage cells */
	caddr32_t	array;	/* pointer to allocated storage cells */
} tsd32_t;
#endif /* _SYSCALL32_IMPL */

typedef	char	thstate_t;

/*
 * thread internal structure
 *
 * READ THIS NOTE IF YOU'RE PLANNING ON ADDING ANYTHING TO THIS
 * STRUCTURE:
 *	libaio is dependent on the size of struct thread. if you
 *	are enlarging its size, always make sure that the "structure
 *	aio_worker" contains enough padding to hold a thread struct.
 *	you'll see that the first field of this structure is a character
 *	array of some number of pad bytes. you'll find the aio_worker
 *	structure in lib/libaio/common/libaio.h.
 */
typedef struct thread {
	struct thread	*t_link;	/* run/sleep queue */
	caddr_t		t_stk;		/* stack base */
	size_t		t_stksize;	/* size of stack */
	size_t		t_stkguardsize; /* redzone size */
	caddr_t		t_tls;		/* pointer to thread local storage */
	resumestate_t	t_resumestate;	/* any extra state needed by resume */
	void		(*t_startpc)();	/* start func called by thr_create() */
	thread_t	t_tid;		/* thread id */
	lwpid_t		t_lwpid;	/* lwp id */
	int		t_usropts;	/* usr options, (THR_BOUND, ...) */
	int		t_flag;		/* flags, (T_ALLOCSTK, T_PARK) */
	int		t_ulflag;	/* flags that do not need a lock */
	_cleanup_t	*t_clnup_hdr;	/* head to cleanup handlers list */
	int		t_pri;		/* scheduling priority */
	int		t_mappedpri;	/* mapped prio */
	int		t_policy;	/* scheduling policy */
	/* Keep following 8 fields together. See _clean_thread() */
	thstate_t	t_state;	/* thread state */
	char		t_nosig;	/* block signal handlers */
	char		t_stop;		/* stop thread when set */
	char		t_preempt;	/* preempt thread when set */
	char		t_schedlocked;	/* flag set, thread holding schedlock */
	char		t_bdirpend;	/* pending directed bounced signals */
	char		t_pending;	/* set when t_psig is not empty */
	char		t_sig;		/* signal rcvd in critical section */
	/* cancel stuff - keep following 4 fields together */
	signed char	t_can_pending;
	signed char	t_can_state;
	signed char	t_can_type;
	signed char	t_cancelable;
	/* cancel stuff word finish */
	sigset_t	t_hold;		/* per thread signal mask */
	sigset_t	t_psig;		/* pending signals */
	sigset_t	t_ssig;		/* signals sent, still pending */
	sigset_t	t_bsig;		/* sigs bounced to this thread's lwp */
	caddr_t		t_wchan;	/* sleep wchan */
	void		*t_exitstat;	/* exit status - non-detached threads */
	mutex_t		*t_handoff;	/* mutex hand off for cv_signal() */
	lwp_sema_t	t_park;		/* used to park threads */
	sigset_t	t_olmask;	/* lwp mask when deferred sig taken */
	siginfo_t	t_si;		/* siginfo for deferred signal */
	struct thread	*t_idle;	/* pointer to an idle thread */
	struct callo	t_itimer_callo;	/* alarm callout (per thread) */
	struct callo	t_cv_callo;	/* cv_timedwait callout (per thread) */
	struct itimerval t_realitimer;	/* real time interval timer */
	struct thread	*t_next;	/* circular queue of all threads */
	struct thread	*t_prev;
	/* only multiplexing threads use the following fields */
	mutex_t		t_lock;		/* locked when loaded into a LWP */
	struct thread	*t_iforw;	/* circular queue of idling threads */
	struct thread	*t_ibackw;
	struct thread	*t_forw;	/* circular queue of ONPROC threads */
	struct thread	*t_backw;
	int		t_errno;	/* thread specific errno */
	int		t_rtldbind;	/* dynamic linking flags */
	/* PROBE_SUPPORT begin */
	void		*t_tpdp;	/* thread probe data pointer */
	/* PROBE_SUPPORT end */
	/* libthread_db support */
	td_evbuf_t	t_td_evbuf;	/* libthread_db event buffer */
	char		t_td_events_enable;	/* event mechanism enabled */
	char		t_mutator;	/* mutator flag: synchronous GC */
	char		t_mutatormask;	/* mask mutator: synchronous GC */
	char		t_ssflg;	/* suspended self on srunq */
	ucontext_t	*t_savedstate;	/* preempted or suspended state */
	lwp_cond_t	t_suspndcv;	/* cv_signal when thread suspended */
	sc_shared_t	*t_lwpdata;	/* scheduler activations data */
	struct thread	*t_scforw;	/* scheduler activations thread list */
	struct thread	*t_scback;
	/*
	 * t_resvtls is used to define a contract-private interface
	 */
	void		*t_resvtls[MAX_RESV_TLS]; /* reserved for fast TLS */
	int		t_npinest;	/* number of nested PI locks */
	int		t_nceilnest;	/* number of nested ceiling locks */
	int		t_rtflag;	/* flags related to RT */
	mxchain_t	*t_mxchain;	/* chain of owned ceiling mutexes */
	int		t_epri;		/* effective scheduling priority */
	int		t_emappedpri;	/* effective mapped prio */
} uthread_t;

#ifdef _SYSCALL32_IMPL

typedef struct thread32 {
	caddr32_t	t_link;		/* run/sleep queue */
	caddr32_t	t_stk;		/* stack base */
	size32_t	t_stksize;	/* size of stack */
	size32_t	t_stkguardsize; /* redzone size */
	caddr32_t	t_tls;		/* pointer to thread local storage */
	resumestate32_t	t_resumestate;	/* any extra state needed by resume */
	caddr32_t	t_startpc;	/* start func called by thr_create() */
	thread_t	t_tid;		/* thread id */
	lwpid_t		t_lwpid;	/* lwp id */
	int		t_usropts;	/* usr options, (THR_BOUND, ...) */
	int		t_flag;		/* flags, (T_ALLOCSTK, T_PARK) */
	int		t_ulflag;	/* flags that do not need a lock */
	caddr32_t	t_clnup_hdr;	/* head to cleanup handlers list */
	int		t_pri;		/* scheduling priority */
	int		t_mappedpri;	/* mapped prio */
	int		t_policy;	/* scheduling policy */
	/* Keep following 8 fields together. See _clean_thread() */
	thstate_t	t_state;	/* thread state */
	char		t_nosig;	/* block signal handlers */
	char		t_stop;		/* stop thread when set */
	char		t_preempt;	/* preempt thread when set */
	char		t_schedlocked;	/* flag set, thread holding schedlock */
	char		t_bdirpend;	/* pending directed bounced signals */
	char		t_pending;	/* set when t_psig is not empty */
	char		t_sig;		/* signal rcvd in critical section */
	/* cancel stuff - keep following 4 fields togather */
	signed char	t_can_pending;
	signed char	t_can_state;
	signed char	t_can_type;
	signed char	t_cancelable;
	/* cancel stuff word finish */
	sigset32_t	t_hold;		/* per thread signal mask */
	sigset32_t	t_psig;		/* pending signals */
	sigset32_t	t_ssig;		/* signals sent, still pending */
	sigset32_t	t_bsig;		/* sigs bounced to this thread's lwp */
	caddr32_t	t_wchan;	/* sleep wchan */
	caddr32_t	t_exitstat;	/* exit status - non-detached threads */
	caddr32_t	t_handoff;	/* mutex hand off for cv_signal() */
	lwp_sema_t	t_park;		/* used to park threads */
	sigset32_t	t_olmask;	/* lwp mask when deferred sig taken */
	siginfo32_t	t_si;		/* siginfo for deferred signal */
	caddr32_t	t_idle;		/* pointer to an idle thread */
	struct callo32	t_itimer_callo;	/* alarm callout (per thread) */
	struct callo32	t_cv_callo;	/* cv_timedwait callout (per thread) */
	struct itimerval32 t_realitimer;	/* real time interval timer */
	caddr32_t	t_next;		/* circular queue of all threads */
	caddr32_t	t_prev;
	/* only multiplexing threads use the following fields */
	mutex_t		t_lock;		/* locked when loaded into a LWP */
	caddr32_t	t_iforw;	/* circular queue of idling threads */
	caddr32_t	t_ibackw;
	caddr32_t	t_forw;		/* circular queue of ONPROC threads */
	caddr32_t	t_backw;
	int		t_errno;	/* thread specific errno */
	int		t_rtldbind;	/* dynamic linking flags */
	/* PROBE_SUPPORT begin */
	caddr32_t	t_tpdp;		/* thread probe data pointer */
	/* PROBE_SUPPORT end */
	/* libthread_db support */
	td_evbuf32_t	t_td_evbuf;	/* libthread_db event buffer */
	char		t_td_events_enable;	/* event mechanism enabled */
	char		t_mutator;	/* mutator flag: synchronous GC */
	char		t_mutatormask;	/* mask mutator: synchronous GC */
	char		t_ssflg;	/* suspended self on srunq */
	caddr32_t	t_savedstate;	/* preempted or suspended state */
	lwp_cond_t	t_suspndcv;	/* cv_signal when thread suspended */
	caddr32_t	t_lwpdata;	/* scheduler activations data */
	caddr32_t	t_scforw;	/* scheduler activations thread list */
	caddr32_t	t_scback;
	/*
	 * t_resvtls is used to define a contract-private interface
	 */
	caddr32_t	t_resvtls[MAX_RESV_TLS]; /* reserved for fast TLS */
	int		t_npinest;	/* number of nested PI locks */
	int		t_nceilnest;	/* number of nested ceiling locks */
	int		t_rtflag;	/* flags related to RT */
	mxchain_t	*t_mxchain;	/* chain of owned ceiling mutexes */
	int		t_epri;		/* effective scheduling priority */
	int		t_emappedpri;	/* effective mapped prio */
} uthread32_t;

#endif /* _SYSCALL32_IMPL */

/*
 * thread states
 */
#define	TS_SLEEP	1
#define	TS_RUN		2
#define	TS_DISP		3
#define	TS_ONPROC	4
#define	TS_STOPPED	5
#define	TS_ZOMB		6
#define	TS_REAPED	7

/*
 * t_flag values
 */
#define	T_ALLOCSTK	0x1	 /* thread library allocated thread's stack */
#define	T_LWPDIRSIGS	0x2	 /* thread has called setitimer(2) VIRT, PROF */
#define	T_PARKED	0x4	 /* thread is parked on its LWP */
#define	T_PREEMPT	0x8	 /* thread has a pending preemption */
#define	T_DONTPREEMPT	0x10	 /* suspend pre-emption until cleared */
#define	T_INTR		0x20	 /* sleep interrupted by an unmasked signal */
#define	T_IDLETHREAD	0x40	 /* thread is an idle thread */
#define	T_INSIGLWP	0x80	 /* thread is in siglwp handler */
#define	T_IDLE		0x100	 /* thread is idle */
#define	T_OFFPROC	0x200	 /* thread is dispatchable */
#define	T_ZOMBIE	0x400	 /* thread is on zombie queue */
#define	T_SIGWAIT	0x800	 /* thread is in a sigwait(2) */
#define	T_INTERNAL	0x2000	 /* an internal libthread daemon thread */
#define	T_2BZOMBIE	0x4000	 /* thread is on the way to zombie queue */
#define	T_BSSIG		0x8000	 /* thread's lwp has pending bounced signals */
#define	T_WAITCV	0x10000	 /* thread is/was asleep for a condition var */
#define	T_EXUNWIND	0x20000	 /* thread is exiting due to cancellation */
#define	T_DOORSERVER	0x40000	 /* thread is a door server thread/lwp */
#define	T_LWPSUSPENDED	0x80000  /* thread is suspended via lwp_suspend() */
#define	T_DEFAULTSTK	0x100000 /* T_ALLOCSTK & default size stack */

/*
 * t_ulflag values
 */

#define	T_TEMPBOUND	0x20	 /* thread is temporarily bound to the lwp */
#define	T_INRESETSIG	0x40	 /* thread has _resetsig() on its stack */
#define	T_SWITCHING	0x800	 /* lwp is switching from this thread */

/*
 * t_rtflag values
 */

#define	T_TEMPRTBOUND	0x1	 /* temporarily bound to the lwp for RT */


/*  t_can_state values */
#define	TC_DISABLE		-1	/* disable cancellation */
#define	TC_ENABLE		00	/* enable cancellation */

/*  t_can_type values */
#define	TC_ASYNCHRONOUS		-1	/* async cancelable */
#define	TC_DEFERRED		00	/* deferred cancelable */

/*  t_cancelable values */
#define	TC_CANCELABLE		-1	/* thread is in cancellation point */

/*  t_can_pending values */
#define	TC_PENDING		-1	/* cancellation pending on thread */

/* t_stop values */
#define	TSTP_REGULAR		1	/* Stopped by thr_suspend */
#define	TSTP_EXTERNAL		2	/* Stopped by debugger */
#define	TSTP_INTERNAL		4	/* Stopped by libthread */
#define	TSTP_MUTATOR		0x8	/* stopped by thr_suspend_*mutator* */
#define	TSTP_ALLMUTATORS	0x10	/* suspending all mutators */

/* RT mutex values */
#define	RTMUTEX_TRY		0
#define	RTMUTEX_BLOCK		1

/*
 * Checks if thread was created with the DETACHED flag set.
 */
#define	DETACHED(t)	((t)->t_usropts & THR_DETACHED)

/*
 * Is thread permanently bound to a LWP?
 */
#define	ISBOUND(t)	((t)->t_usropts & THR_BOUND)

/*
 * Is thread parked on its LWP?
 */
#define	ISPARKED(t)	((t)->t_flag & T_PARKED)

/*
 * Is thread suspended via lwp_suspend()?
 */
#define	ISLWPSUSPENDED(t)	((t)->t_flag & T_LWPSUSPENDED)

/*
 * Does this thread have a preemption pending ?
 */
#define	PREEMPTED(t)	((t)->t_flag & T_PREEMPT)

/*
 * Is this thread stopped ?
 */
#define	STOPPED(t)	((t)->t_stop & TSTP_REGULAR)

/*
 * Is this thread stopped by an external agent (debugger)?
 */
#define	DBSTOPPED(t)	((t)->t_stop & TSTP_EXTERNAL)

/*
 * Was thread created to be an idle thread?
 */
#define	IDLETHREAD(t)	((t)->t_flag & T_IDLETHREAD)

/*
 * Is thread on idle queue?
 */
#define	ON_IDLE_Q(t)	((t)->t_iforw)

/*
 * Is thread at some cancellation point?
 */
#define	CANCELABLE(t)		((t)->t_cancelable == TC_CANCELABLE)

/*
 * Is thread cancellation pending?
 */
#define	CANCELPENDING(t)	((t)->t_can_pending == TC_PENDING)

/*
 * Is thread cancellation enabled/disable?
 */
#define	CANCELENABLE(t)		((t)->t_can_state == TC_ENABLE)
#define	CANCELDISABLE(t)	((t)->t_can_state == TC_DISABLE)

/*
 * Is thread deferred/async cancelable?
 */
#define	CANCELDEFERED(t)	((t)->t_can_type == TC_DEFERRED)
#define	CANCELASYNC(t)		((t)->t_can_type ==  TC_ASYNCHRONOUS)

/*
 * Is thread on/off the _onprocq ?
 */
#define	ONPROCQ(t) ((t)->t_forw != NULL && (t)->t_backw != NULL)
#define	OFFPROCQ(t) ((t)->t_forw == NULL && (t)->t_backw == (t)->t_forw)

/* Assumes p >= 0 */
#define	HASH_PRIORITY(p, index) index = p - THREAD_MIN_PRIORITY; \

#define	PRIO_SET	0
#define	PRIO_INHERIT	1
#define	PRIO_DISINHERIT	2

/*
 * True if thread possesses a priority inherited from a ceiling mutex.
 * Can also be used for user-level priority inheritance, if we ever do this.
 * Basically, t_epri, if set, is the priority to be used by the dispatcher.
 */
#define	INHERITED(t)	((t)->t_epri != 0)

/* The dispatch priority of a thread */
#define	DISP_PRIO(t)	(INHERITED(t) ? (t)->t_epri : (t)->t_pri)

/* Is the inherited priority mapped ? */
#define	INHERITED_MAPPED(t)	((t)->t_emappedpri != 0)

/* Return the "real" (or prio passed to setschedparam()) inherited priority */
#define	INHERITED_RPRIO(t)\
	(INHERITED_MAPPED(t) ? (t)->t_emappedpri : (t)->t_epri)

/* Is the priority mapped ? */
#define	PRIO_MAPPED(t)	((t)->t_mappedpri != 0)

/* Return the "real" (or prio passed to setschedparam()) priority */
#define	RPRIO(t)\
	(PRIO_MAPPED(t) ? (t)->t_mappedpri : (t)->t_pri)

/* The "real" dispatch priority of a thread */
#define	DISP_RPRIO(t)	(INHERITED(t) ? INHERITED_RPRIO(t) : RPRIO(t))

#define	_lock_bucket(ix) \
if (ix != -1) _lmutex_lock(&(_allthreads[ix].lock)); else

#define	_unlock_bucket(ix) \
if (ix != -1) _lmutex_unlock(&(_allthreads[ix].lock)); else

/* convert a thread_t to a struct thread pointer */
#define	THREAD(x)	(((x) == 0) ? curthread : (uthread_t *)_idtot_nolock(x))
/*
 * A relative time increment used to set the absolute time "cond_eot" to be
 * sufficiently far into the future. cond_eot is used to validate the timeout
 * argument to cond_timedwait().
 */
#define	COND_REL_EOT 100000000

/* convert a thread to its lwpid */
#define	LWPID(t)	(t)->t_lwpid

/*
 * Is signal ignored
 */
#define	ISIGNORED(s)	(_tsiguhandler[(s)-1] == SIG_IGN || \
			(_tsiguhandler[(s)-1] == SIG_DFL && \
			_sigismember(&_ignoredefault, (s))))
/*
 * Is this a robust lock?
 */
#define	ROBUSTLOCK(type)	((type) & USYNC_PROCESS_ROBUST)

/*
 * round up x to the  pagesize
 */
#define	ROUNDUP_PAGESIZE(x)	(((x)+ _lpagesize - 1) & ~(_lpagesize - 1))

/*
 * is this a RT signal
 */
#define	ISRTSIGNAL(x)		((((x) >= _SIGRTMIN) && ((x) <= _SIGRTMAX)))

#define	RTMUTEXTYPE	(PTHREAD_PRIO_INHERIT|PTHREAD_PRIO_PROTECT)

/*
 * Is this a RT mutex
 */
#define	ISRTMUTEX(x)		((x) & RTMUTEXTYPE)

#define	ALLTHR_TBLSIZ 509
#define	HASH_TID(tid) ((tid) == 0 ? -1 : (tid) % ALLTHR_TBLSIZ)
#define	CHECK_PRIO(x) ((x) < THREAD_MIN_PRIORITY || (x) > THREAD_MAX_PRIORITY)

typedef struct thrtab {
	uthread_t	*first;
	mutex_t		lock;
} thrtab_t;

#ifdef _SYSCALL32_IMPL
typedef struct thrtab32 {
	caddr32_t	first;
	mutex_t		lock;
} thrtab32_t;
#endif /* _SYSCALL32_IMPL */

/*
 * global mutexes, read-write and condition locks.
 * _sighandlerlock should be grabbed before bucket lock and _schedlock.
 * See thread.c:_thrp_kill()
 */
extern mutex_t _schedlock;	/* protects runq and sleepq */
extern lwp_mutex_t _sighandlerlock;	/* protects signal handlers */
extern rwlock_t _lrw_lock;

extern thrtab_t _allthreads[]; 	/* doubly linked list of all threads */
extern thread_t _lasttid;	/* monotonically increasing global tid count */
extern mutex_t _tidlock;	/* protects access to _lasttid */
extern int _totalthreads;	/* total number of threads created */
extern int _userthreads;	/* number of user created threads */
extern int _u2bzombies;		/* u threads on their way 2b zombies */
extern int _d2bzombies;		/* daemon threads on their way 2b zombies */
extern long _idlethread;	/* address of IDLETHREAD */
extern uthread_t *_nidle;	/* list of idling threads */
extern int _nidlecnt;		/* number of threads idling */
extern int _onprocq_size;	/* size of onproc Q */
extern int _nagewakecnt;	/* number of awakened aging threads */
extern int _naging;		/* number of aging threads running */
extern int _minlwps;		/* min number of idle lwps */
extern int _nlwps;		/* number of lwps in this pool. */
extern int _ndie;		/* number of lwps to delete from this pool. */
extern int _nrunnable;		/* number of threads on the run queue */
extern int _nthreads;		/* number of unbound threads */
extern int _sigwaitingset;	/* 1 if sigwaiting enabled; 0 if disabled */
extern struct thread *_lowestpri_th;	/* the lowest priority running thread */
extern lwp_mutex_t _schedlock;	/* protects runqs and sleepqs */
extern uthread_t *_onprocq;	/* circular queue of ONPROC threads */
extern uthread_t *_zombies;	/* circular queue of zombie threads */
extern uthread_t *_deathrow;	/* circular queue of dead threads */
extern cond_t _zombied;		/* waiting for zombie threads */
extern int _zombiecnt;		/* nunber of zombied threads */
extern int _reapcnt;		/* number of threads to be reaped */
extern lwp_mutex_t _reaplock;	/* reaper thread's lock */
extern lwp_cond_t _untilreaped;	/* wait until _reapcnt < high mark */
extern int _lpagesize;		/* libthread pagesize initialized in _t0init */
extern int _lsemvaluemax;	/* maximum value for a semaphore */
extern sigset_t _allmasked;	/* all maskable signals except SIGLWP masked */
extern sigset_t _totalmasked;	/* all maskable signals masked */
extern int _maxpriq;		/* index of highest priority dispq */
extern uint_t _dqactmap[];	/* bit map of priority queues */

extern int _timerset;		/* interval timer is set if timerset == 1 */
extern thread_t _co_tid;	/* thread that does the callout processing */
extern int _co_set;		/* create only one thread to run callouts */
extern int _calloutcnt;		/* number of pending callouts */
extern callo_t *_calloutp;	/* pointer to the callout queue */
extern mutex_t _calloutlock;	/* protects queue of callouts */
extern thread_t __dynamic_tid;	/* bound thread that handles SIGWAITING */
extern lwpid_t __dynamic_lwpid;	/* lwpid of thread that handles SIGWAITING */
extern lwp_cond_t _aging;	/* condition on which threads age */

extern uthread_t *_sched_owner;
extern uintptr_t _sched_ownerpc;
extern struct thread _thread;
extern struct	thread *_t0;	/* the initial thread */

extern uint8_t _libpthread_loaded;	/* indicates libpthread is loaded */
extern struct sigaction __alarm_sigaction; /* global sigaction for SIGALRM */
extern int _first_thr_create;
extern int _uconcurrency;
extern mutex_t _concurrencylock;

extern int _suspendingallmutators; /* when non-zero, suspending all mutators. */
extern int _suspendedallmutators; /* when non-zero, all mutators suspended. */
extern int _samcnt;		/* mutators signalled to suspend themselves */
extern lwp_cond_t _samcv;	/* wait for all mutators to be suspended */

extern caddr_t __sighndlrend;

#ifdef TLS
#ifndef NOTHREAD
#pragma unshared(_thread);
#endif
#define	curthread (&_thread)
#else
extern uthread_t *_curthread(void);
#define	curthread (_curthread())
#endif


/*
 * sleep-wakeup hashing:  Each entry in slpq[] points
 * to the front and back of a linked list of sleeping processes.
 * Processes going to sleep go to the back of the appropriate
 * sleep queue and wakeprocs wakes them from front to back (so the
 * first process to go to sleep on a given channel will be the first
 * to run after a wakeup on that channel).
 */

#define	NSLEEPQ		509
#define	slpqhash(X)	(&_slpq[((uintptr_t)X) % (NSLEEPQ)])

struct slpq {
	struct thread	*sq_first;
	struct thread	*sq_last;
};

extern struct slpq _slpq[];

/*
 * Reserve one bucket for all threads with priorities > THREAD_MAX_PRIORITY
 * or less than THREAD_MIN_PRIORITY.
 * NOTE : Currently, thread_priority() returns an error if there is an
 * attempt to set a thread's priority out of this range, so the extra
 * bucket is not really used.
 */
#define	DISPQ_SIZE	(NPRI + 1)

typedef struct dispq {
	struct thread *dq_first;
	struct thread *dq_last;
} dispq_t;

extern dispq_t _dispq[];

typedef	void	(*PFrV) (void *);	/* pointer to function returning void */

/*
* Information common to all threads' TSD.
*/
struct tsd_common {
	unsigned int	nkeys;		/* number of used keys */
	unsigned int	max_keys;	/* number of allocated keys */
	PFrV		*destructors;	/* array of per-key destructor funcs */
	rwlock_t	lock;		/* lock for the above */
};

#ifdef _SYSCALL32_IMPL
struct tsd_common32 {
	unsigned int	nkeys;		/* number of used keys */
	unsigned int	max_keys;	/* number of allocated keys */
	caddr32_t	destructors;	/* array of per-key destructor funcs */
	rwlock_t	lock;		/* lock for the above */
};
#endif /* _SYSCALL32_IMPL */

extern struct tsd_common tsd_common;

/*
 * The following two data structures are used to define contract-private
 * interfaces and hold global information on the reserved TLS slots.
 */

struct resv_tls_common {
	unsigned int	nslots;			/* number of allocated slots */
	unsigned char	slot_map[MAX_RESV_TLS];	/* bit map of resv TLS slots */
	/* array of per slot destructor function pointers */
	PFrV		destructors[MAX_RESV_TLS];
	/* array of per slot pass-through parameter pointers */
	void		*pass_through_param[MAX_RESV_TLS];
	mutex_t		lock;			/* lock for the above */
};

#ifdef _SYSCALL32_IMPL
struct resv_tls_common32 {
	unsigned int	nslots;			/* number of allocated slots */
	unsigned char	slot_map[MAX_RESV_TLS];	/* bit map of resv TLS slots */
	/* array of per slot destructor function pointers */
	caddr32_t	destructors[MAX_RESV_TLS];
	/* array of per slot pass-through parameter pointers */
	caddr32_t	pass_through_param[MAX_RESV_TLS];
	mutex_t		lock;			/* lock for the above */
};
#endif /* _SYSCALL32_IMPL */

/*
 * The following type is used in the definition of reserved TLS which is
 * contract-private
 */
typedef ptrdiff_t thr_slot_handle_t;

extern struct resv_tls_common _resv_tls_common;



extern sigset_t _allunmasked;
extern sigset_t _cantmask;
extern sigset_t _lcantmask;
extern sigset_t _cantreset;
extern sigset_t _pmask;		/* virtual process signal mask */
extern mutex_t  _pmasklock;	/* lock for v. process signal mask */
extern sigset_t _bpending;	/* signals pending reassignment */
extern mutex_t	_bpendinglock;	/* mutex protecting _bpending */
extern cond_t	_sigwait_cv;
extern mutex_t _tsslock;
extern sigset_t	_ignoredset;	/* ignored signals */

#ifdef i386
extern void __freegs_lock(void);
extern void __freegs_unlock(void);
extern void __ldt_lock(void);
extern void __ldt_unlock(void);
#endif	/* i386 */

extern sigset_t _ignoredefault;

extern uint_t	_sc_dontfork;	/* block forks while calling _lwp_schedctl */
extern cond_t	_sc_dontfork_cv;
extern mutex_t	_sc_lock;
extern uthread_t *_sc_list;

/*
 * Signal test and manipulation macros.
 */
#define	sigdiffset(s1, s2)\
	(s1)->__sigbits[0] &= ~((s2)->__sigbits[0]); \
	(s1)->__sigbits[1] &= ~((s2)->__sigbits[1]); \
	(s1)->__sigbits[2] &= ~((s2)->__sigbits[2]); \
	(s1)->__sigbits[3] &= ~((s2)->__sigbits[3])

/* Mask all signal except - SIGSTOP/SIGKILL/SILWP/SIGCANCEL */
#define	maskallsigs(s) _sigfillset((s)); \
	sigdiffset((s), &_cantmask)

/* Mask all signal except - SIGSTOP/SIGKILL */
#define	masktotalsigs(s) _sigfillset((s)); \
	sigdiffset((s), &_lcantmask)

#define	sigcmpset(x, y)	(((x)->__sigbits[0] ^ (y)->__sigbits[0]) || \
			((x)->__sigbits[1] ^ (y)->__sigbits[1]) || \
			((x)->__sigbits[2] ^ (y)->__sigbits[2]) || \
			((x)->__sigbits[3] ^ (y)->__sigbits[3]))

/*
 * Are signals in "s" being manipulated in the mask (o = oldmask; n = newmask)?
 * Return true if yes, otherwise false.
 */
#define	changesigs(o, n, s)\
	((((o)->__sigbits[0] ^ (n)->__sigbits[0]) & (s)->__sigbits[0]) || \
	(((o)->__sigbits[1] ^ (n)->__sigbits[1]) & (s)->__sigbits[1]) || \
	(((o)->__sigbits[2] ^ (n)->__sigbits[2]) & (s)->__sigbits[2]) || \
	(((o)->__sigbits[3] ^ (n)->__sigbits[3]) & (s)->__sigbits[3]))

#define	sigorset(s1, s2)	(s1)->__sigbits[0] |= (s2)->__sigbits[0]; \
				(s1)->__sigbits[1] |= (s2)->__sigbits[1]; \
				(s1)->__sigbits[2] |= (s2)->__sigbits[2]; \
				(s1)->__sigbits[3] |= (s2)->__sigbits[3];

#define	sigisempty(s)\
	((s)->__sigbits[0] == 0 && (s)->__sigbits[1] == 0 &&\
	(s)->__sigbits[2] == 0 && (s)->__sigbits[3] == 0)


/*
 * following macro copied from sys/signal.h since inside #ifdef _KERNEL there.
 */
#define	sigmask(n)	((unsigned int)1 << (((n) - 1) & (32 - 1)))

/*
 * masksmaller(sigset_t *m1, sigset_t *m2)
 * return true if m1 is smaller (less restrictive) than m2
 */
#define	masksmaller(m1, m2) \
	((~((m1)->__sigbits[0]) & (m2)->__sigbits[0]) ||\
	    (~((m1)->__sigbits[1]) & (m2)->__sigbits[1]) ||\
	    (~((m1)->__sigbits[2]) & (m2)->__sigbits[2]) ||\
	    (~((m1)->__sigbits[3]) & (m2)->__sigbits[3]))

#define	sigand(x, y) (\
	((x)->__sigbits[0] & (y)->__sigbits[0]) ||\
	    ((x)->__sigbits[1] & (y)->__sigbits[1]) ||\
	    ((x)->__sigbits[2] & (y)->__sigbits[2]) ||\
	    ((x)->__sigbits[3] & (y)->__sigbits[3]))

#define	sigandset(a, x, y) \
	(a)->__sigbits[0] = (x)->__sigbits[0] & (y)->__sigbits[0]; \
	    (a)->__sigbits[1] = (x)->__sigbits[1] & (y)->__sigbits[1]; \
	    (a)->__sigbits[2] = (x)->__sigbits[2] & (y)->__sigbits[2]; \
	    (a)->__sigbits[3] = (x)->__sigbits[3] & (y)->__sigbits[3]

#define	maskreverse(s1, s2) {\
	int i; \
		for (i = 0; i < 4; i++)\
			(s2)->__sigbits[i] =  ~(s1)->__sigbits[i]; \
	}

/*
 * sparc v7 has no native swap instruction. It is emulated on the ss1s, ipcs,
 * etc. So, use the SIGSEGV interpositioning solution to solve the
 * mutex_unlock() problem of reading the waiter bit after the lock is freed.
 * Add other architectures (say, arch_foo) here which do not have atomic swap
 * instructions. This would result in the conditional define changing to:
 * "#if defined (__sparc) || defined(sparc) || defined(arch_foo)"
 */

#if !defined(__sparcv9) && (defined(__sparc) || defined(sparc))
extern __wrd; /* label in _mutex_unlock_asm() needed for SEGV handler */
extern __wrds; /* label in _mutex_unlock() needed for SEGV handler */
#define	NO_SWAP_INSTRUCTION
extern int __advance_pc_required(ucontext_t *, siginfo_t *);
extern int __munlock_segv(int, struct thread *, ucontext_t *);
void  __libthread_segvhdlr(int sig, siginfo_t *sip, ucontext_t *uap);
extern struct sigaction __segv_sigaction; /* global sigaction for SEGV */
#endif

extern sigset_t __lwpdirsigs;
extern sigset_t _null_sigset;
sigset_t _tpmask;

#define	dbg_sigaddset(m, s)
#define	dbg_delset(m, s)
#define	pmaskok(tm, pm) (1)

extern void (*_tsiguhandler[])(int, siginfo_t *, ucontext_t *);

/*
 * ANSI PROTOTYPES of internal global functions
 */


uthread_t *_idtot(thread_t tid);
uthread_t *_idtot_nolock(thread_t tid);
void	_dynamiclwps(void);
void	_tdb_agent(void);
void	_tdb_agent_check(void);
pid_t	_fork(void);
pid_t	_fork1(void);
uintptr_t _manifest_thread_state(void);
void	_mutex_sema_unlock(mutex_t *mp);
void	_lmutex_unlock(mutex_t *mp);
int	_lmutex_trylock(mutex_t *mp);
void	_lmutex_lock(mutex_t *mp);
void	_lprefork_handler(void);
void	_lpostfork_child_handler(void);
void	_lpostfork_parent_handler(void);
int	_lpthread_atfork(void (*) (), void (*) (), void (*) ());
int	_lrw_rdlock(rwlock_t *rwlp);
int	_lrw_wrlock(rwlock_t *rwlp);
int	_lrw_unlock(rwlock_t *rwlp);
int	_thrp_kill_unlocked(uthread_t *t, int ix,
					int sig, lwpid_t *lwpidp);
int	_thr_main(void);
int	_setcallout(callo_t *cop, thread_t tid, const struct timeval *tv,
					void (*func)(), uintptr_t arg);
int	_rmcallout(callo_t *cop);
void	_callin(int sig, siginfo_t *sip, ucontext_t *uap);
void	_t_block(caddr_t chan);
int	_t_release(caddr_t chan, uchar_t *waiters, int);
void	_t_release_all(caddr_t chan);
void	_unsleep(struct thread *t);
void	_setrun(struct thread *t);
void	_dopreempt(ucontext_t *);
void	_preempt(uthread_t *t, pri_t pri);
struct	thread *_choose_thread(pri_t pri);
int 	_lwp_exec(struct thread *t, uintptr_t npc, caddr_t sp, void (*fcn)(),
					int flags, lwpid_t *retlwpid);
int 	_new_lwp(struct thread *t, void (*func)(void), int);
int	_alloc_stack(size_t size, caddr_t *sp, size_t guardsize);
int	_alloc_stack_fromreapq(caddr_t *sp);
int	_alloc_chunk(caddr_t at, size_t size, caddr_t *cp);
void	_free_stack(caddr_t addr, size_t size, int cache, size_t guardsize);
void	_swtch(int dontsave);
void	_qswtch(void);
void	_age(void);
void	_onproc_deq(uthread_t *t);
void	_onproc_enq(uthread_t *t);
void	_unpark(uthread_t *t);
void	_setrq(uthread_t *t);
void	_setsrq(uthread_t *t);
void	_suspend_rq();
void	_resume_rq();
int	_rqdeq(uthread_t *t);
int	_srqdeq(uthread_t *t);
uthread_t *_idle_thread_create(int size, void (*)());
void	_thread_destroy(uthread_t *t, int ix);
void	_thread_delete(uthread_t *t, int ix);
void	_thread_free(uthread_t *t);
int	_alloc_thread(caddr_t stk, size_t stksize, struct thread **tp,
				size_t guardsize);
int 	_thread_call(uthread_t *t, void (*fcn)(void), void *arg);
void 	_thread_ret(struct thread *t, void (*fcn)(void));
void 	_thread_start(void);
void	_reapq_add(uthread_t *t);
void	_reaper_create(void);
void	_reap_wait(cond_t *cvp);
void	_reap_wait_cancel(cond_t *cvp);
void	_reap_lock(void);
void	_reap_unlock(void);
void	_sigon(void);
void	_sigoff(void);
void	_t0init(void);
void	_deliversigs(const sigset_t *sigs);
int	_sigredirect(int sig);
void	_siglwp(int sig, siginfo_t *sip, ucontext_t *uap);
void	_sigcancel(int sig, siginfo_t *sip, ucontext_t *uap);
void	__sighndlr(int sig, siginfo_t *sip, ucontext_t *uap, void (*func)());
void	_sigwaiting_enabled(void);
void	_sigwaiting_disabled(void);
int	_hibit(uint_t i);
int	_fsig(sigset_t *s);
void	_sigmaskset(sigset_t *s1, sigset_t *s2, sigset_t *s3);
int	_blocking(sigset_t *sent, sigset_t *old, const sigset_t *new,
					sigset_t *resend);
void	_resume_ret(uthread_t *oldthread);
void	_destroy_tsd(void);
void	_destroy_resv_tls(void);
void	_resetlib(void);
int	_assfail(char *, char *, int);
int	_setsighandler(int sig, const struct sigaction *nact,
						struct sigaction *oact);
void	__sighandler_lock(void);
void	__sighandler_unlock(void);
void	_initsigs(void);
void	_sys_thread_create(void (*entry)(void), int flags);
void	_cancel(uthread_t *t);
void	_canceloff(void);
void	_cancelon(void);
void	_prefork_handler(void);
void	_postfork_parent_handler(void);
void	_postfork_child_handler(void);
void	_tcancel_all(void *arg);
void	_thrp_exit(void);
void	_thr_exit_common(void *sts, int ex);
extern int (*__sigsuspend_trap)(const sigset_t *);
extern int (*__kill_trap)(pid_t, int);
int	_sigaction(int, const struct sigaction *, struct sigaction *);
int	_sigwait(const sigset_t *);
int	_sigprocmask(int, sigset_t *, sigset_t *);
void	_sigunblock(sigset_t *, sigset_t *, sigset_t *);
int	__sigtimedwait(const sigset_t *, siginfo_t *, const struct timespec *);
unsigned	_sleep(unsigned);
void	_sched_lock(void);
void	_sched_unlock(void);
void	_sched_lock_nosig(void);
void	_sched_unlock_nosig(void);
void	_panic(const char *);
int	_getcaller();
psw_t	_getpsr();
void	_copyfsr(long *);

void	_sc_init(void);
void	_sc_add(uthread_t *);
void	_sc_setup(int, int);
void	_sc_switch(uthread_t *);
void	_sc_exit(void);
void	_sc_cleanup(int);
void	_lwp_start(void);
int	_lock_try_adaptive(mutex_t *);
int	_lock_clear_adaptive(mutex_t *);
uchar_t _lock_held(mutex_t *);
void	_mutex_set_typeattr(mutex_t *, int);
int	_mutex_lock_robust(mutex_t *);
int	_mutex_trylock_robust(mutex_t *);
int	_mutex_unlock_robust(mutex_t *);
int	_mutex_wakeup(mutex_t *);
int	_validate_rt_prio(int, int);
int	_thrp_setlwpprio(lwpid_t, int, int);
int	_map_rtpri_to_gp(pri_t);
int	_getscheduler();

void _get_resumestate(uthread_t *, gregset_t);
void _set_resumestate(uthread_t *, gregset_t);

void	tsd_init(uthread_t *);

/*
 * prototype of all the exported functions of internal
 * version of libthread calls. We need this since there
 * is no synonyms_mt.h any more.
 */

int	_cond_init(cond_t *cvp, int type, void *arg);
int	_cond_destroy(cond_t *cvp);
int	_cond_timedwait(cond_t *cvp, mutex_t *mp, timestruc_t *ts);
int	_cond_timedwait_cancel(cond_t *cvp, mutex_t *mp, timestruc_t *ts);
int	_cond_wait(cond_t *cvp, mutex_t *mp);
int	_cond_wait_cancel(cond_t *cvp, mutex_t *mp);
int	_cond_signal(cond_t *cvp);
int	_cond_broadcast(cond_t *cvp);

int	_mutex_init(mutex_t *mp, int type, void *arg);
int	_mutex_destroy(mutex_t *mp);
int	_mutex_lock(mutex_t *mp);
int	_mutex_unlock(mutex_t *mp);
int	_mutex_trylock(mutex_t *mp);
void	_mutex_op_lock(mutex_t *mp);
void	_mutex_op_unlock(mutex_t *mp);

int	_pthread_atfork(void (*)(void), void (*)(void), void (*)(void));

int	_rwlock_init(rwlock_t *rwlp, int type, void *arg);
int	_rwlock_destroy(rwlock_t *rwlp);
int	_rw_rdlock(rwlock_t *rwlp);
int	_rw_wrlock(rwlock_t *rwlp);
int	_rw_unlock(rwlock_t *rwlp);
int	_rw_tryrdlock(rwlock_t *rwlp);
int	_rw_trywrlock(rwlock_t *rwlp);

int	_sema_init(sema_t *sp, unsigned int count, int type, void *arg);
int	_sema_destroy(sema_t *sp);
int	_sema_wait(sema_t *sp);
int	_sema_trywait(sema_t *sp);
int	_sema_post(sema_t *sp);
int	_sema_wait_cancel(sema_t *sp);

int	_thr_create(void *stk, size_t stksize, void *(*func)(void *),
		void *arg, long flags, thread_t *new_thread);
int	_thrp_create(void *stk, size_t stksize, void *(*func)(void *),
		void *arg, long flags, thread_t *new_thread, int prio,
		int policy, size_t guardsize);
int	*_thr_errnop(void);
int	_thr_join(thread_t tid, thread_t *departed, void **status);
int	_thr_setconcurrency(int n);
int	_thr_getconcurrency(void);
void	_thr_exit(void *status);
thread_t _thr_self(void);
int	_thr_sigsetmask(int how, const sigset_t *set, sigset_t *oset);
int	__thr_sigsetmask(int how, const sigset_t *set, sigset_t *oset);
int	__thr_sigredirect(uthread_t *, int, int, lwpid_t *);
int	_thr_kill(thread_t tid, int sig);
int	_thr_suspend(thread_t tid);
int	_thr_stksegment(stack_t *);
int	_thr_continue(thread_t tid);
void	_thr_yield(void);
int	_thr_setprio(thread_t tid, int newpri);
int	_thr_getprio(thread_t tid, int *pri);
int	_thr_keycreate(thread_key_t *pkey, void (*destructor)(void *));
int	_thr_key_delete(thread_key_t key);
int	_thr_getspecific(thread_key_t key, void **valuep);
int	_thr_setspecific(unsigned int key, void *value);

/*
 * The following four interfaces are contract-private
 */
int	thr_slot_sync_allocate(thr_slot_handle_t *, void(*)(void *), void *);
void	thr_slot_set(thr_slot_handle_t, void *);
void	*thr_slot_get(thr_slot_handle_t);
int	thr_slot_sync_deallocate(thr_slot_handle_t);

size_t	_thr_min_stack(void);
int	__thr_continue(thread_t);
int	_thr_dbsuspend(thread_t);
int	_thr_dbcontinue(thread_t);
int	_thrp_join(thread_t, thread_t *, void **);
void	_thread_free_all();
int	_thrp_setprio(uthread_t *, int, int, int, int);
int	_thread_setschedparam_main(pthread_t, int, const struct sched_param *,
		int);
int	setcontext(const ucontext_t *);
void	_fpinherit(uthread_t *);
int	_blocksent(sigset_t *, sigset_t *, const sigset_t *);
void	_bsigs(sigset_t *);
void	_init_cpu(struct thread *);
void	_flush_store(void);
void	_ex_clnup_handler(void *, void (*clnup)(void *),
		void (*tcancel)(void *));
void	_ex_unwind_local(void (*func)(void *), void *);
void	_whereami(volatile caddr_t *);
void	__advance_pc_if_munlock_segv(int, siginfo_t *, ucontext_t *);

void	_pthread_exit(void *);

void	_preempt_off(void);
void	_preempt_on(void);

void	_set_libc_interface(void);
void	_unset_libc_interface(void);
void	_set_rtld_interface(void);
void	_unset_rtld_interface(void);

int	__alarm(int);
int	__lwp_alarm(int);
int	__lwp_cond_wait(cond_t *cv, mutex_t *mp);
void	_lwp_terminate(uthread_t *);

void	__sig_resethand(int);

void	_fcntl_cancel(int, int, int);

void	_tdb_update_stats();
void	tdb_event_catchsig();
void	tdb_event_create();
void	tdb_event_death();
void	tdb_event_lock_try();
void	tdb_event_preempt();
void	tdb_event_ready();
void	tdb_event_sleep();
void	_resume(uthread_t *, caddr_t, int);

extern	int	_libthread_sema_wait(sema_t *);
extern	int	_pthread_temp_rt_bind(void);
extern	void	_pthread_temp_rt_unbind(void);


/*
 * Specially defined 'weak' symbols for initilialization of
 * the Thr_interface table.
 */
int	_ti_alarm(unsigned);
int	_ti_mutex_lock(mutex_t *);
int	_ti_mutex_unlock(mutex_t *);
int	_ti_thr_self(void);
int	_ti_cond_broadcast(cond_t *);
int	_ti_cond_destroy(cond_t *);
int	_ti_cond_init(cond_t *, int, void *);
int	_ti_cond_signal(cond_t *);
int	_ti_cond_timedwait(cond_t *, mutex_t *, timestruc_t *);
int	_ti_cond_wait(cond_t *, mutex_t *);
int	_ti_fork(void);
int	_ti_fork1(void);
int	_ti_mutex_destroy(mutex_t *);
int	_ti_mutex_held(mutex_t *);
int	_ti_mutex_init(mutex_t *, int, void *);
int	_ti_mutex_trylock(mutex_t *);
int	_ti_pthread_atfork(void (*)(void), void(*)(void), void (*)(void));
int	_ti_pthread_cond_broadcast(cond_t *);
int	_ti_pthread_cond_destroy(cond_t *);
int	_ti_pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
int	_ti_pthread_cond_signal(cond_t *);
int	_ti_pthread_cond_timedwait(pthread_cond_t *, pthread_mutex_t *,
					struct timespec *);
int	_ti_pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
int	_ti_pthread_condattr_destroy(pthread_condattr_t *);
int	_ti_pthread_condattr_getpshared(const pthread_condattr_t *, int *);
int	_ti_pthread_condattr_init(pthread_condattr_t *);
int	_ti_pthread_condattr_setpshared(pthread_condattr_t *, int);
int	_ti_pthread_mutex_destroy(mutex_t *);
int	_ti_pthread_mutex_getprioceiling(const pthread_mutex_t *, int *);
int	_ti_pthread_mutex_init(pthread_mutex_t *, pthread_mutexattr_t *);
int	_ti_pthread_mutex_lock(mutex_t *);
int	_ti_pthread_mutex_setprioceiling(pthread_mutex_t *, int, int *);
int	_ti_pthread_mutex_trylock(mutex_t *);
int	_ti_pthread_mutex_unlock(mutex_t *);
int	_ti_pthread_mutexattr_destroy(pthread_mutexattr_t *);
int	_ti_pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *,
						int *);
int	_ti_pthread_mutexattr_getprotocol(const pthread_mutexattr_t *, int *);
int	_ti_pthread_mutexattr_getpshared(const pthread_mutexattr_t *, int *);
int	_ti_pthread_mutexattr_init(pthread_mutexattr_t *);
int	_ti_pthread_mutexattr_setprioceiling(pthread_mutexattr_t *, int);
int	_ti_pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
int	_ti_pthread_mutexattr_setpshared(pthread_mutexattr_t *, int);
int	_ti_pthread_self(void);
int	_ti_pthread_sigmask(int, const sigset_t *, sigset_t *);
int	_ti_pthread_attr_destroy(pthread_attr_t *);
int	_ti_pthread_attr_getdetachstate(pthread_attr_t *, int);
int	_ti_pthread_attr_getinheritsched(pthread_attr_t *, int);
int	_ti_pthread_attr_getschedparam(pthread_attr_t *,
					const struct sched_param *);
int	_ti_pthread_attr_getschedpolicy(pthread_attr_t *, int);
int	_ti_pthread_attr_getscope(pthread_attr_t *, int);
int	_ti_pthread_attr_getstackaddr(const pthread_attr_t *, void **);
int	_ti_pthread_attr_getstacksize(pthread_attr_t *, size_t);
int	_ti_pthread_attr_init(pthread_attr_t *);
int	_ti_pthread_attr_setdetachstate(const pthread_attr_t *, void **);
int	_ti_pthread_attr_setinheritsched(const pthread_attr_t *, int *);
int	_ti_pthread_attr_setschedparam(const pthread_attr_t *, int *);
int	_ti_pthread_attr_setschedpolicy(const pthread_attr_t *, int *);
int	_ti_pthread_attr_setscope(const pthread_attr_t *, int *);
int	_ti_pthread_attr_setstackaddr(const pthread_attr_t *, size_t *);
int	_ti_pthread_attr_setstacksize(pthread_attr_t *);
int	_ti_pthread_cancel(thread_t);
int	_ti__pthread_cleanup_pop(int, _cleanup_t *);
int	_ti__pthread_cleanup_push(void (*routine)(void *), void *,
					caddr_t, _cleanup_t *);
int	_ti_pthread_create(pthread_t *, const pthread_attr_t *,
				void * (*start_routine)(void *), void *);
int	_ti_pthread_detach(thread_t);
int	_ti_pthread_equal(pthread_t, pthread_t);
int	_ti_pthread_exit(void *);
int	_ti_pthread_getschedparam(pthread_t, int *, struct sched_param *);
int	_ti_pthread_getspecific(pthread_key_t);
int	_ti_pthread_join(pthread_t, void **);
int	_ti_pthread_key_create(thread_key_t *, void (*destructor)(void *));
int	_ti_pthread_key_delete(thread_key_t);
int	_ti_pthread_kill(thread_t, int);
int	_ti_pthread_once(pthread_once_t *, void (*init_routine)(void));
int	_ti_pthread_setcancelstate(int, int *);
int	_ti_pthread_setcanceltype(int, int *);
int	_ti_pthread_setschedparam(pthread_t, int, const struct sched_param *);
int	_ti_pthread_setspecific(unsigned int, void *);
int	_ti_pthread_testcancel(void);
int	_ti_rw_read_held(rwlock_t *);
int	_ti_rw_rdlock(rwlock_t *);
int	_ti_rw_wrlock(rwlock_t *);
int	_ti_rw_unlock(rwlock_t *);
int	_ti_rw_tryrdlock(rwlock_t *);
int	_ti_rw_trywrlock(rwlock_t *);
int	_ti_rw_write_held(rwlock_t *);
int	_ti_rwlock_init(rwlock_t *, int, void *);
int	_ti_rwlock_destroy(rwlock_t *);
int	_ti_sema_held(sema_t *);
int	_ti_sema_init(sema_t *, uint_t, int, void *);
int	_ti_sema_post(sema_t *);
int	_ti_sema_trywait(sema_t *);
int	_ti_sema_wait(sema_t *);
int	_ti_sema_destroy(sema_t *);
int	_ti_setitimer(int, const struct itimerval *, struct itimerval *);
int	_ti_sigaction(int, const struct sigaction *, struct sigaction *);
int	_ti_siglongjmp(sigjmp_buf, int);
int	_ti_sigpending(sigset_t *);
int	_ti_sigprocmask(int, sigset_t *, sigset_t *);
int	_ti_sigsetjmp(sigjmp_buf, int);
int	_ti_sigsuspend(const sigset_t *);
int	_ti_sigwait(const sigset_t *);
int	_ti_sigtimedwait(const sigset_t *, siginfo_t *,
				const struct timespec *);
int	_ti_sleep(unsigned);
int	_ti_thr_continue(thread_t);
int	_ti_thr_create(void *, size_t, void *(*func)(void *), void *,
			long, thread_t *);
int	_ti_thr_errnop(void);
int	_ti_thr_exit(void *);
int	_ti_thr_getconcurrency(void);
int	_ti_thr_getprio(thread_t, int *);
int	_ti_thr_getspecific(thread_key_t, void **);
int	_ti_thr_join(thread_t, thread_t *, void **);
int	_ti_thr_keycreate(thread_key_t *, void (*destructor)(void *));
int	_ti_thr_kill(thread_t, int);
int	_ti_thr_main(void);
int	_ti_thr_min_stack(void);
int	_ti_thr_setconcurrency(int);
int	_ti_thr_setprio(thread_t, int);
int	_ti_thr_setspecific(unsigned int, void *);
int	_ti_thr_sigsetmask(int, const sigset_t *, sigset_t *);
int	_ti_thr_stksegment(stack_t *);
int	_ti_thr_suspend(thread_t);
int	_ti_thr_yield(void);
int	_ti_close();
int	_ti_creat();
int	_ti_creat64();
int	_ti_fcntl();
int	_ti_fsync();
int	_ti_msync();
int	_ti_open();
int	_ti_open64();
int	_ti_pause();
int	_ti_read();
int	_ti_tcdrain();
int	_ti_wait();
int	_ti_waitpid();
int	_ti_write();
int	_ti__nanosleep();
int	_ti_kill();
int	_ti_pthread_attr_getguardsize();
int	_ti_pthread_attr_setguardsize();
int	_ti_pthread_getconcurrency();
int	_ti_pthread_setconcurrency();
int	_ti_pthread_mutexattr_settype();
int	_ti_pthread_mutexattr_gettype();
int	_ti_pthread_rwlock_init();
int	_ti_pthread_rwlock_destroy();
int	_ti_pthread_rwlock_rdlock();
int	_ti_pthread_rwlock_tryrdlock();
int	_ti_pthread_rwlock_wrlock();
int	_ti_pthread_rwlock_trywrlock();
int	_ti_pthread_rwlock_unlock();
int	_ti_pthread_rwlockattr_init();
int	_ti_pthread_rwlockattr_destroy();
int	_ti_pthread_rwlockattr_getpshared();
int	_ti_pthread_rwlockattr_setpshared();
int	_ti_getmsg();
int	_ti_getpmsg();
int	_ti_putmsg();
int	_ti_putpmsg();
int	_ti_lockf();
int	_ti_lockf64();
int	_ti_msgrcv();
int	_ti_msgsnd();
int	_ti_poll();
int	_ti_pread();
int	_ti_pread64();
int	_ti_readv();
int	_ti_pwrite();
int	_ti_pwrite64();
int	_ti_writev();
int	_ti_select();
int	_ti_sigpause();
int	_ti_usleep();
int	_ti_wait3();
int	_ti_waitid();
int	_ti_xpg4_putmsg();
int	_ti_xpg4_putpmsg();
#ifdef	__cplusplus
}
#endif

#endif /* _LIBTHREAD_H */
