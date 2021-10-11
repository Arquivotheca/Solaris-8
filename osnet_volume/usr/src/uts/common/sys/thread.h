/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_THREAD_H
#define	_SYS_THREAD_H

#pragma ident	"@(#)thread.h	1.103	99/11/20 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/klwp.h>
#include <sys/time.h>
#include <sys/signal.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The thread object, its states, and the methods by which it
 * is accessed.
 */

/*
 * Values that t_state may assume. Note that t_state cannot have more
 * than one of these flags set at a time.
 */
#define	TS_FREE		0x00	/* Thread at loose ends */
#define	TS_SLEEP	0x01	/* Awaiting an event */
#define	TS_RUN		0x02	/* Runnable, but not yet on a processor */
#define	TS_ONPROC	0x04	/* Thread is being run on a processor */
#define	TS_ZOMB		0x08	/* Thread has died but hasn't been reaped */
#define	TS_STOPPED	0x10	/* Stopped, initial state */

typedef struct ctxop {
	void	(*save_op)(void *);	/* function to invoke to save context */
	void	(*restore_op)(void *);	/* function to invoke to restore ctx */
	void	(*fork_op)(void *, void *);	/* invoke to fork context */
	void	(*lwp_create_op)(void *, void *);	/* lwp_create context */
	void	(*free_op)(void *, int); /* function which frees the context */
	void	*arg;		/* argument to above functions, ctx pointer */
	struct ctxop *next;	/* next context ops */
} ctxop_t;

/*
 * The active file descriptor table.
 * Each member of a_fd[] not equalling -1 represents an active fd.
 * The structure is initialized on first use; all zeros means uninitialized.
 */
typedef struct _afd {
	int	*a_fd;		/* pointer to list of fds */
	short	a_nfd;		/* number of entries in *a_fd */
	short	a_stale;	/* one of the active fds is being closed */
	int	a_buf[1];	/* buffer to which a_fd initially refers */
} afd_t;

/*
 * An lwpchan provides uniqueness when sleeping on user-level
 * synchronization primitives.  The lc_wchan member is used
 * for sleeping on kernel synchronization primitives.
 */
typedef struct {
	caddr_t lc_wchan0;
	caddr_t lc_wchan;
} lwpchan_t;

typedef struct _kthread	*kthread_id_t;

struct turnstile;
struct trap_info;
struct upimutex;

typedef struct _kthread {
	struct _kthread	*t_link; /* dispq, sleepq, and free queue link */

	caddr_t	t_stk;		/* base of stack (kernel sp value to use) */
#if defined(__ia64)
	caddr_t	t_regstk;	/* base of register stack (initial bsp value) */
#endif
	void	(*t_startpc)(void);	/* PC where thread started */
	struct cpu *t_bound_cpu; /* cpu bound to, or NULL if not bound */
	short	t_affinitycnt;	/* nesting level of kernel affinity-setting */
	short	t_bind_cpu;	/* user-specified CPU binding (-1 if none) */
	ushort_t t_flag;		/* modified only by current thread */
	ushort_t t_proc_flag;	/* modified holding ttproc(t)->p_lock */
	ushort_t t_schedflag;	/* modified holding thread_lock(t) */
	volatile char t_preempt;	/* don't preempt thread if set */
	volatile char t_preempt_lk;
	uint_t	t_state;	/* thread state	(protected by thread_lock) */
	pri_t	t_pri;		/* assigned thread priority */
	pri_t	t_epri;		/* inherited thread priority */
	label_t	t_pcb;		/* pcb, save area when switching */
	lwpchan_t t_lwpchan;	/* reason for blocking */
#define	t_wchan0	t_lwpchan.lc_wchan0
#define	t_wchan		t_lwpchan.lc_wchan
	struct _sobj_ops *t_sobj_ops;
	id_t	t_cid;		/* scheduling class id */
	struct thread_ops *t_clfuncs;	/* scheduling class ops vector */
	void	*t_cldata;	/* per scheduling class specific data */
	ctxop_t	*t_ctx;		/* thread context */
	uintptr_t t_lofault;	/* ret pc for failed page faults */
	label_t	*t_onfault;	/* on_fault() setjmp buf */
	void	*t_nofault;	/* nofault setjmp buf */
	caddr_t t_swap;		/* swappable thread storage */
	lock_t	t_lock;		/* used to resume() a thread */
	uint8_t	t_lockstat;	/* set while thread is in lockstat code */
	uint8_t	t_pil;		/* interrupt thread PIL */
	disp_lock_t	t_pi_lock;	/* lock protecting t_prioinv list */
	struct cpu	*t_cpu;	/* CPU that thread last ran on */
	struct _kthread	*t_intr; /* interrupted (pinned) thread */
	uint_t	t_did;		/* thread id for kernel debuggers */
	caddr_t t_tnf_tpdp;	/* Trace facility data pointer */

	/*
	 * non swappable part of the lwp state.
	 */
	id_t		t_tid;		/* lwp's id */
	id_t		t_waitfor;	/* target lwp id in lwp_wait() */
	timeout_id_t	t_alarmid;	/* alarm's timeout id */
	struct itimerval t_realitimer;	/* real interval timer */
	timeout_id_t	t_itimerid;	/* real interval timer's timeout id */
	struct sigqueue	*t_sigqueue;	/* queue of siginfo structs */
	k_sigset_t	t_sig;		/* signals pending to this process */
	k_sigset_t	t_hold;		/* hold signal bit mask */
	struct	_kthread *t_forw;	/* process's forward thread link */
	struct	_kthread *t_back;	/* process's backward thread link */
	klwp_t	*t_lwp;			/* thread's lwp pointer */
	struct	proc	*t_procp;	/* proc pointer */
	caddr_t t_audit_data;		/* per thread audit data */
	struct	_kthread *t_next;	/* doubly linked list of all threads */
	struct	_kthread *t_prev;
	struct	vnode	*t_trace;	/* pointer to /proc lwp vnode */
	ushort_t t_whystop;		/* reason for stopping */
	ushort_t t_whatstop;		/* more detailed reason */
	int	t_dslot;		/* index in proc's thread directory */
	struct	pollstate *t_pollstate;	/* state used during poll(2) */
	struct	pollcache *t_pollcache;	/* to pass a pcache ptr by /dev/poll */
	struct	cred	*t_cred;	/* pointer to current cred */
	time_t	t_start;		/* start time, seconds since epoch */
	clock_t	t_lbolt;		/* lbolt at last clock_tick() */
	hrtime_t t_stoptime;		/* timestamp at stop() */
	short	t_sysnum;		/* system call number */
	kcondvar_t	t_delay_cv;
	uint_t	t_pctcpu;		/* %cpu at last clock_tick(), binary */
					/* point at right of high-order bit */

	/*
	 * Pointer to the dispatcher lock protecting t_state and state-related
	 * flags.  This pointer can change during waits on the lock, so
	 * it should be grabbed only by thread_lock().
	 */
	disp_lock_t	*t_lockp;	/* pointer to the dispatcher lock */
	ushort_t 	t_oldspl;	/* spl level before dispatcher locked */
	volatile char	t_pre_sys;	/* pre-syscall work needed */
	lock_t		t_lock_flush;	/* for lock_mutex_flush() impl */
	struct _disp	*t_disp_queue;	/* run queue for chosen CPU */
	clock_t		t_disp_time;	/* last time this thread was running */
	uint_t		t_kpri_req;	/* kernel priority required */

	/*
	 * Post-syscall / post-trap flags.
	 * 	No lock is required to set these.
	 *	These must be cleared only by the thread itself.
	 *
	 *	t_astflag indicates that some post-trap processing is required,
	 *		possibly a signal or a preemption.  The thread will not
	 *		return to user with this set.
	 *	t_post_sys indicates that some unusualy post-system call
	 *		handling is required, such as an error or tracing.
	 *	t_sig_check indicates that some condition in ISSIG() must be
	 * 		checked, but doesn't prevent returning to user.
	 *	t_post_sys_ast is a way of checking whether any of these three
	 *		flags are set.
	 */
	union __tu {
		struct __ts {
			volatile char	_t_astflag;	/* AST requested */
			volatile char	_t_sig_check;	/* ISSIG required */
			volatile char	_t_post_sys;	/* post_syscall req */
			volatile char	_t_trapret;	/* call CL_TRAPRET */
		} _ts;
		volatile int	_t_post_sys_ast;	/* OR of these flags */
	} _tu;
#define	t_astflag	_tu._ts._t_astflag
#define	t_sig_check	_tu._ts._t_sig_check
#define	t_post_sys	_tu._ts._t_post_sys
#define	t_trapret	_tu._ts._t_trapret
#define	t_post_sys_ast	_tu._t_post_sys_ast

	/*
	 * Real time microstate profiling.
	 */
					/* possible 4-byte filler */
	hrtime_t t_waitrq;		/* timestamp for run queue wait time */
	int	t_mstate;		/* current microstate */
	struct rprof {
		int	rp_anystate;		/* set if any state non-zero */
		uint_t	rp_state[NMSTATES];	/* mstate profiling counts */
	} *t_rprof;

	/*
	 * There is a turnstile inserted into the list below for
	 * every priority inverted synchronization object that
	 * this thread holds.
	 */

	struct turnstile *t_prioinv;

	/*
	 * Pointer to the turnstile attached to the synchronization
	 * object where this thread is blocked.
	 */

	struct turnstile *t_ts;

	/*
	 * Opaque MMU context information, used by resume to coordinate
	 * handling of MMU context hardware with the hat layer.
	 */
	uint_t		t_mmuctx;


	/*
	 * kernel thread specific data
	 *	Borrowed from userland implementation of POSIX tsd
	 */
	struct tsd_thread {
		struct tsd_thread *ts_next;	/* threads with TSD */
		struct tsd_thread *ts_prev;	/* threads with TSD */
		uint_t		  ts_nkeys;	/* entries in value array */
		void		  **ts_value;	/* array of value/key */
	} *t_tsd;

	clock_t		t_stime;	/* time stamp used by the swapper */
	struct door_data *t_door;	/* door invocation data */
	kmutex_t	*t_plockp;	/* pointer to process's p_lock */

	struct _kthread	*t_handoff;	/* handoff scheduling */
	struct sc_data	*t_schedctl;	/* scheduler activations data */

	struct cpupart	*t_cpupart;	/* partition containing thread */
	int		t_bind_pset;	/* processor set binding */

	struct copyops	*t_copyops;	/* copy in/out ops vector */

	caddr_t		t_stkbase;	/* base of the the stack */
#if defined(__ia64)
	size_t		t_stksize;	/* size of the the stack */
#endif
	struct page	*t_red_pp;	/* if non-NULL, redzone is mapped */

	struct _afd	t_activefd;	/* active file descriptor table */

	struct _kthread	*t_priforw;	/* sleepq per-priority sublist */
	struct _kthread	*t_priback;

	struct sleepq	*t_sleepq;	/* sleep queue thread is waiting on */
	struct trap_info *t_panic_trap;	/* saved data from fatal trap */
	void 		*t_resv;	/* reserved for future use */
	struct upimutex	*t_upimutex;	/* list of upimutexes owned by thread */
	uint32_t	t_nupinest;	/* number of nested held upi mutexes */
} kthread_t;

/*
 * Thread flag (t_flag) definitions.
 *	These flags must be changed only for the current thread,
 * 	and not during preemption code, since the code being
 *	preempted could be modifying the flags.
 *
 *	For the most part these flags do not need locking.
 *	The following flags will only be changed while the thread_lock is held,
 *	to give assurrance that they are consistent with t_state:
 *		T_WAKEABLE
 */
#define	T_INTR_THREAD	0x0001	/* thread is an interrupt thread */
#define	T_WAKEABLE	0x0002	/* thread is blocked, signals enabled */
#define	T_TOMASK	0x0004	/* use lwp_sigoldmask on return from signal */
#define	T_TALLOCSTK	0x0008  /* thread structure allocated from stk */
#define	T_WOULDBLOCK	0x0020	/* for lockfs */
#define	T_DONTBLOCK	0x0040	/* for lockfs */
#define	T_DONTPEND	0x0080	/* for lockfs */
#define	T_SYS_PROF	0x0100	/* profiling on for duration of system call */
#define	T_WAITCVSEM	0x0200	/* waiting for a lwp_cv or lwp_sema on sleepq */
#define	T_WATCHPT	0x0400	/* thread undergoing a watchpoint emulation */
#define	T_PANIC		0x0800	/* thread initiated a system panic */

/*
 * Flags in t_proc_flag.
 *	These flags must be modified only when holding the p_lock
 *	for the associated process.
 */
#define	TP_HOLDLWP	0x0002	/* hold thread's lwp */
#define	TP_TWAIT	0x0004	/* wait to be freed by lwp_wait() */
#define	TP_LWPEXIT	0x0008	/* LWP has exited */
#define	TP_PRSTOP	0x0010	/* thread is being stopped via /proc */
#define	TP_CHKPT	0x0020	/* thread is being stopped via CPR checkpoint */
#define	TP_EXITLWP	0x0040	/* terminate this LWP */
#define	TP_PRVSTOP	0x0080	/* thread is virtually stopped via /proc */
#define	TP_MSACCT	0x0100	/* collect micro-state accounting information */
#define	TP_STOPPING	0x0200	/* thread is executing stop() */
#define	TP_WATCHPT	0x0400	/* process has watchpoints in effect */
#define	TP_PAUSE	0x0800	/* process is being stopped via pauselwps() */
#define	TP_CHANGEBIND	0x1000	/* thread has a new cpu/cpupart binding */

/*
 * Thread scheduler flag (t_schedflag) definitions.
 *	The thread must be locked via thread_lock() or equiv. to change these.
 */
#define	TS_LOAD		0x0001	/* thread is in memory */
#define	TS_DONT_SWAP	0x0002	/* thread/LWP should not be swapped */
#define	TS_SWAPENQ	0x0004	/* swap thread when it reaches a safe point */
#define	TS_ON_SWAPQ	0x0008	/* thread is on the swap queue */
#define	TS_CSTART	0x0100	/* setrun() by continuelwps() */
#define	TS_UNPAUSE	0x0200	/* setrun() by unpauselwps() */
#define	TS_XSTART	0x0400	/* setrun() by SIGCONT */
#define	TS_PSTART	0x0800	/* setrun() by /proc */
#define	TS_RESUME	0x1000	/* setrun() by CPR resume process */
#define	TS_CREATE	0x2000	/* setrun() by syslwp_create() */
#define	TS_ALLSTART	\
	(TS_CSTART|TS_UNPAUSE|TS_XSTART|TS_PSTART|TS_RESUME|TS_CREATE)

/*
 * No locking needed for AST field.
 */
#define	aston(t)		((t)->t_astflag = 1)
#define	astoff(t)		((t)->t_astflag = 0)

/* True if thread is stopped on an event of interest */
#define	ISTOPPED(t) ((t)->t_state == TS_STOPPED && \
			!((t)->t_schedflag & TS_PSTART))

/* similar to ISTOPPED except the event of interest is CPR */
#define	CPR_ISTOPPED(t) ((t)->t_state == TS_STOPPED && \
			!((t)->t_schedflag & TS_RESUME))

/*
 * True if thread is virtually stopped (is or was asleep in
 * one of the lwp_*() system calls and marked to stop by /proc.)
 */
#define	VSTOPPED(t)	((t)->t_proc_flag & TP_PRVSTOP)

/* similar to VSTOPPED except the point of interest is CPR */
#define	CPR_VSTOPPED(t)				\
	((t)->t_state == TS_SLEEP &&		\
	(t)->t_wchan0 != NULL &&		\
	((t)->t_flag & T_WAKEABLE) &&		\
	((t)->t_proc_flag & TP_CHKPT))

/* True if thread has been stopped by hold*() or was created stopped */
#define	SUSPENDED(t) ((t)->t_state == TS_STOPPED && \
	((t)->t_schedflag & (TS_CSTART|TS_UNPAUSE)) != (TS_CSTART|TS_UNPAUSE))

/* True if thread possesses an inherited priority */
#define	INHERITED(t)	((t)->t_epri != 0)

/* The dispatch priority of a thread */
#define	DISP_PRIO(t) (INHERITED(t) ? (t)->t_epri : (t)->t_pri)

/* The assigned priority of a thread */
#define	ASSIGNED_PRIO(t)	((t)->t_pri)

/*
 * Macros to determine whether a thread can be swapped.
 * If t_lock is held, the thread is either on a processor or being swapped.
 */
#define	SWAP_OK(t)	(!LOCK_HELD(&(t)->t_lock))

/*
 * proctot(x)
 *	convert a proc pointer to a thread pointer. this only works with
 *	procs that have only one lwp.
 *
 * proctolwp(x)
 *	convert a proc pointer to a lwp pointer. this only works with
 *	procs that have only one lwp.
 *
 * ttolwp(x)
 *	convert a thread pointer to its lwp pointer.
 *
 * ttoproc(x)
 *	convert a thread pointer to its proc pointer.
 *
 * lwptot(x)
 *	convert a lwp pointer to its thread pointer.
 *
 * lwptoproc(x)
 *	convert a lwp to its proc pointer.
 */
#define	proctot(x)	((x)->p_tlist)
#define	proctolwp(x)	((x)->p_tlist->t_lwp)
#define	ttolwp(x)	((x)->t_lwp)
#define	ttoproc(x)	((x)->t_procp)
#define	lwptot(x)	((x)->lwp_thread)
#define	lwptoproc(x)	((x)->lwp_procp)

#define	t_pc		t_pcb.val[0]
#define	t_sp		t_pcb.val[1]

#ifdef	_KERNEL

extern	kthread_t	*threadp(void);	/* inline, returns thread pointer */
#define	curthread	(threadp())	/* current thread pointer */
#define	curproc		(ttoproc(curthread))	/* current process */

extern	struct _kthread	t0;		/* the scheduler thread */
extern	kmutex_t	pidlock;	/* global process lock */

/*
 * thread_free_lock is used by the clock thread to keep a thread
 * from being freed while it is being examined.
 */
extern	kmutex_t	thread_free_lock;

/* Return thread ptr for given ID */
extern kthread_t	*idtot(kthread_t *, id_t);

/*
 * Routines to change the priority and effective priority
 * of a thread-locked thread, whatever its state.
 */
extern int	thread_change_pri(kthread_t *t, pri_t disp_pri, int front);
extern void	thread_change_epri(kthread_t *t, pri_t disp_pri);

/*
 * Routines that manipulate the dispatcher lock for the thread.
 * The locking heirarchy is as follows:
 *	cpu_lock > sleepq locks > run queue locks
 */
void	thread_transition(kthread_t *); /* move to transition lock */
void	thread_lock(kthread_t *);	/* lock thread and its queue */
void	thread_lock_high(kthread_t *);	/* lock thread and its queue */
void	thread_onproc(kthread_t *, struct cpu *); /* set onproc state lock */

#define	thread_unlock(t)		disp_lock_exit((t)->t_lockp)
#define	thread_unlock_high(t)		disp_lock_exit_high((t)->t_lockp)
#define	thread_unlock_nopreempt(t)	disp_lock_exit_nopreempt((t)->t_lockp)

#define	THREAD_LOCK_HELD(t)	(DISP_LOCK_HELD((t)->t_lockp))

extern disp_lock_t transition_lock;	/* lock protecting transiting threads */
extern disp_lock_t stop_lock;		/* lock protecting stopped threads */

caddr_t	thread_stk_init(caddr_t);	/* init thread stack */

#endif	/* _KERNEL */

/*
 * Macros to indicate that the thread holds resources that could be critical
 * to other kernel threads, so this thread needs to have kernel priority
 * if it blocks or is preempted.  Note that this is not necessary if the
 * resource is a mutex or a writer lock because of priority inheritance.
 *
 * The only way one thread may legally manipulate another thread's t_kpri_req
 * is to hold the target thread's thread lock while that thread is asleep.
 * (The rwlock code does this to implement direct handoff to waiting readers.)
 */
#define	THREAD_KPRI_REQUEST()	(curthread->t_kpri_req++)
#define	THREAD_KPRI_RELEASE()	(curthread->t_kpri_req--)
#define	THREAD_KPRI_RELEASE_N(n) (curthread->t_kpri_req -= (n))

/*
 * Macros to change thread state and the associated lock.
 */
#define	THREAD_SET_STATE(tp, state, lp) \
		((tp)->t_state = state, (tp)->t_lockp = lp)

/*
 * Point it at the transition lock, which is always held.
 * The previosly held lock is dropped.
 */
#define	THREAD_TRANSITION(tp) 	thread_transition(tp);
/*
 * Set the thread's lock to be the transition lock, without dropping
 * previosly held lock.
 */
#define	THREAD_TRANSITION_NOLOCK(tp) 	((tp)->t_lockp = &transition_lock)

/*
 * Put thread in run state, and set the lock pointer to the dispatcher queue
 * lock pointer provided.  This lock should be held.
 */
#define	THREAD_RUN(tp, lp)	THREAD_SET_STATE(tp, TS_RUN, lp)

/*
 * Put thread in run state, and set the lock pointer to the dispatcher queue
 * lock pointer provided (i.e., the "swapped_lock").  This lock should be held.
 */
#define	THREAD_SWAP(tp, lp)	THREAD_SET_STATE(tp, TS_RUN, lp)

/*
 * Put thread in stop state, and set the lock pointer to the stop_lock.
 * This effectively drops the lock on the thread, since the stop_lock
 * isn't held.
 * Eventually, stop_lock could be hashed if there is too much contention.
 */
#define	THREAD_STOP(tp) \
	{	disp_lock_t *lp = (tp)->t_lockp; \
		THREAD_SET_STATE(tp, TS_STOPPED, &stop_lock); \
		disp_lock_exit(lp); \
	}

/*
 * Put the thread in zombie state and set the lock pointer to NULL.
 * The NULL will catch anything that tries to lock a zombie.
 */
#define	THREAD_ZOMB(tp)		THREAD_SET_STATE(tp, TS_ZOMB, NULL)

/*
 * Set the thread into ONPROC state, and point the lock at the CPUs
 * lock for the onproc thread(s).  This lock should be held, so the
 * thread deoes not become unlocked, since these stores can be reordered.
 */
#define	THREAD_ONPROC(tp, cpu)	\
		THREAD_SET_STATE(tp, TS_ONPROC, &(cpu)->cpu_thread_lock)

/*
 * Set the thread into the TS_SLEEP state, and set the lock pointer to
 * to some sleep queue's lock.  The new lock should already be held.
 */
#define	THREAD_SLEEP(tp, lp)	{				\
			disp_lock_t	*tlp;			\
			tlp = (tp)->t_lockp;			\
			THREAD_SET_STATE(tp, TS_SLEEP, lp);	\
			disp_lock_exit_high(tlp);		\
			}

/*
 * Interrupt threads are created in TS_FREE state, and their lock
 * points at the associated CPU's lock.
 */
#define	THREAD_FREEINTR(tp, cpu)	\
		THREAD_SET_STATE(tp, TS_FREE, &(cpu)->cpu_thread_lock)


#ifdef	__cplusplus
}
#endif

#endif /* _SYS_THREAD_H */
