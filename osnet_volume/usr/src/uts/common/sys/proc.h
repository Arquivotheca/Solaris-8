/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_PROC_H
#define	_SYS_PROC_H

#pragma ident	"@(#)proc.h	1.133	99/11/20 SMI"	/* SVr4.0 11.65 */

#include <sys/time.h>

#include <sys/thread.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/watchpoint.h>
#include <sys/timer.h>
#if defined(i386) || defined(__i386) || defined(__ia64)
#include <sys/segment.h>
#endif
#include <sys/utrap.h>
#include <sys/model.h>
#include <sys/refstr.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Profile arguments.
 */
struct prof {
	void		*pr_base;	/* buffer base */
	uintptr_t	pr_off;		/* pc offset */
	size_t		pr_size;	/* buffer size */
	uint32_t	pr_scale;	/* pc scaling */
	long		pr_samples;	/* sample count */
};

/*
 * One structure allocated per active process.  It contains all
 * data needed about the process while the process may be swapped
 * out.  Other per-process data (user.h) is also inside the proc structure.
 * Lightweight-process data (lwp.h) and the kernel stack may be swapped out.
 */
typedef struct	proc {
	/*
	 * Fields requiring no explicit locking
	 */
	struct	vnode *p_exec;		/* pointer to a.out vnode */
	struct	as *p_as;		/* process address space pointer */
	struct	plock *p_lockp;		/* ptr to proc struct's mutex lock */
	kmutex_t p_crlock;		/* lock for p_cred */
	struct	cred	*p_cred;	/* process credentials */
	/*
	 * Fields protected by pidlock
	 */
	int	p_swapcnt;		/* number of swapped out lwps */
	char	p_stat;			/* status of process */
	char	p_wcode;		/* current wait code */
	ushort_t p_pidflag;		/* flags protected only by pidlock */
	int 	p_wdata;		/* current wait return value */
	pid_t	p_ppid;			/* process id of parent */
	struct	proc	*p_link;	/* forward link */
	struct	proc	*p_parent;	/* ptr to parent process */
	struct	proc	*p_child;	/* ptr to first child process */
	struct	proc	*p_sibling;	/* ptr to next sibling proc on chain */
	struct	proc	*p_psibling;	/* ptr to prev sibling proc on chain */
	struct	proc	*p_sibling_ns;	/* prt to siblings with new state */
	struct	proc	*p_child_ns;	/* prt to children with new state */
	struct	proc 	*p_next;	/* active chain link next */
	struct	proc 	*p_prev;	/* active chain link prev */
	struct	proc 	*p_nextofkin;	/* gets accounting info at exit */
	struct	proc 	*p_orphan;
	struct	proc 	*p_nextorph;
	struct	proc	*p_pglink;	/* process group hash chain link next */
	struct	proc	*p_ppglink;	/* process group hash chain link prev */
	struct	sess	*p_sessp;	/* session information */
	struct	pid 	*p_pidp;	/* process ID info */
	struct	pid 	*p_pgidp;	/* process group ID info */
	/*
	 * Fields protected by p_lock
	 */
	kcondvar_t p_cv;		/* proc struct's condition variable */
	kcondvar_t p_flag_cv;
	kcondvar_t p_lwpexit;		/* waiting for some lwp to exit */
	kcondvar_t p_holdlwps;		/* process is waiting for its lwps */
					/* to to be held.  */
	ushort_t p_pad1;		/* unused */
	uint_t	p_flag;			/* protected while set. */
					/* flags defined below */
	clock_t	p_utime;		/* user time, this process */
	clock_t	p_stime;		/* system time, this process */
	clock_t	p_cutime;		/* sum of children's user time */
	clock_t	p_cstime;		/* sum of children's system time */
	caddr_t *p_segacct;		/* segment accounting info */
	caddr_t	p_brkbase;		/* base address of heap */
	size_t	p_brksize;		/* heap size in bytes */
	/*
	 * Per process signal stuff.
	 */
	k_sigset_t p_sig;		/* signals pending to this process */
	k_sigset_t p_ignore;		/* ignore when generated */
	k_sigset_t p_siginfo;		/* gets signal info with signal */
	struct sigqueue *p_sigqueue;	/* queued siginfo structures */
	struct sigqhdr *p_sigqhdr;	/* hdr to sigqueue structure pool */
	struct sigqhdr *p_signhdr;	/* hdr to signotify structure pool */
	uchar_t	p_stopsig;		/* jobcontrol stop signal */

	/*
	 * Special per-process flag when set will fix misaligned memory
	 * references.
	 */
	char    p_fixalignment;

	/*
	 * Per process lwp and kernel thread stuff
	 */
	int	p_lwptotal;		/* total number of lwps created */
	int 	p_lwpcnt;		/* number of lwps in this process */
	int	p_lwprcnt;		/* number of not stopped lwps */
	int	p_lwpwait;		/* number of lwps in lwp_wait() */
	int	p_zombcnt;		/* number of zombie LWPs */
	int	p_zomb_max;		/* number of entries in p_zomb_tid */
	id_t	*p_zomb_tid;		/* array of zombie lwpids */
	kthread_t *p_tlist;		/* circular list of threads */
	/*
	 * /proc (process filesystem) debugger interface stuff.
	 */
	k_sigset_t p_sigmask;		/* mask of traced signals (/proc) */
	k_fltset_t p_fltmask;		/* mask of traced faults (/proc) */
	struct	vnode *p_trace;		/* pointer to primary /proc vnode */
	struct	vnode *p_plist;		/* list of /proc vnodes for process */
	kthread_t *p_agenttp;		/* thread ptr for /proc agent lwp */
	struct watched_area *p_warea;	/* list of watched areas */
	ulong_t	p_nwarea;		/* number of watched areas */
	struct watched_page *p_wpage;	/* remembered watched pages (vfork) */
	int	p_nwpage;		/* number of watched pages (vfork) */
	int	p_mapcnt;		/* number of active pr_mappage()s */

	struct	proc  *p_rlink;		/* linked list for server */
	kcondvar_t p_srwchan_cv;
	size_t	p_stksize;		/* process stack size in bytes */
	/*
	 * Microstate accounting, resource usage, and real-time profiling
	 */
	hrtime_t p_mstart;		/* hi-res process start time */
	hrtime_t p_mterm;		/* hi-res process termination time */
	hrtime_t p_mlreal;		/* elapsed time sum over defunct lwps */
	hrtime_t p_acct[NMSTATES];	/* microstate sum over defunct lwps */
	struct lrusage p_ru;		/* lrusage sum over defunct lwps */
	struct itimerval p_rprof_timer; /* ITIMER_REALPROF interval timer */
	timeout_id_t p_rprof_timerid;	/* interval timer's timeout id */
	uint_t	p_defunct;		/* number of defunct lwps */
	/*
	 * profiling. A lock is used in the event of multiple lwp's
	 * using the same profiling base/size.
	 */
	kmutex_t p_pflock;		/* protects user profile arguments */
	struct prof p_prof;		/* profile arguments */

	/*
	 * The user structure
	 */
	struct user p_user;		/* (see sys/user.h) */

	/*
	 * Doors.
	 */
	kthread_t		*p_server_threads;
	struct door_node	*p_door_list;	/* active doors */
	struct door_node	*p_unref_list;
	kcondvar_t		p_server_cv;
	char			p_unref_thread;	/* unref thread created */

	/*
	 * Kernel probes
	 */
	uchar_t			p_tnf_flags;

	/*
	 * C2 Security  (C2_AUDIT)
	 */
	caddr_t p_audit_data;		/* per process audit structure */
	kthread_t	*p_aslwptp;	/* thread ptr representing "aslwp" */
#if defined(i386) || defined(__i386) || defined(__ia64)
	/*
	 * LDT support.
	 */
	kmutex_t p_ldtlock;		/* protects the following fields */
	struct seg_desc *p_ldt;		/* Pointer to private LDT */
	struct seg_desc p_ldt_desc;	/* segment descriptor for private LDT */
	int p_ldtlimit;			/* highest selector used */
#endif
	size_t p_swrss;			/* resident set size before last swap */
	struct aio	*p_aio;		/* pointer to async I/O struct */
	struct itimer	**p_itimer;	/* interval timers */
	k_sigset_t	p_notifsigs;	/* signals in notification set */
	kcondvar_t	p_notifcv;	/* notif cv to synchronize with aslwp */
	timeout_id_t	p_alarmid;	/* alarm's timeout id */
	uint_t		p_sc_unblocked;	/* number of unblocked threads */
	struct vnode	*p_sc_door;	/* scheduler activations door */
	caddr_t		p_usrstack;	/* top of the process stack */
	uint_t		p_stkprot;	/* stack memory protection */
	model_t		p_model;	/* data model determined at exec time */
	struct lwpchan_data	*p_lcp;	/* lwpchan cache */
	/*
	 * protects unmapping and initilization of robust locks.
	 */
	kmutex_t	p_lcp_mutexinitlock;
	utrap_handler_t	*p_utraps;	/* pointer to user trap handlers */
	refstr_t	*p_corefile;	/* pattern for core file */
#if defined(__ia64)
	caddr_t		p_upstack;	/* base of the upward-growing stack */
	size_t		p_upstksize;	/* size of that stack, in bytes */
	uchar_t		p_isa;		/* which instruction set is utilized */
#endif
	void		*p_rce;		/* resource control extension data */
	void		*p_reserved[3]; /* for future use */
} proc_t;

#if defined(__ia64)
/*
 * Values for p_isa above
 */
#define	_ISA_IA32	0		/* Intel Architecture 32-bit */
#define	_ISA_IA64	1		/* Intel Architecture 64-bit */

#define	is_ia32_process(p)	((p)->p_isa == _ISA_IA32)
#endif /* defined(__ia64) */

#define	PROC_T				/* headers relying on proc_t are OK */

/* active process chain */

extern proc_t *practive;

/* Well known processes */

extern proc_t *proc_sched;		/* memory scheduler */
extern proc_t *proc_init;		/* init */
extern proc_t *proc_pageout;		/* pageout daemon */
extern proc_t *proc_fsflush;		/* filesystem sync-er */


/*
 * Stuff to keep track of the number of processes each uid has.
 *
 * A structure is allocated when a new uid shows up
 * There is a hash to find each uid's structure.
 */
struct	upcount	{
	struct	upcount	*up_next;
	uid_t		up_uid;
	uint_t		up_count;
};

/* process ID info */

struct pid {
	unsigned int pid_prinactive :1;
	unsigned int pid_pgorphaned :1;
	unsigned int pid_padding :6;	/* used to be pid_ref, now an int */
	unsigned int pid_prslot :24;
	pid_t pid_id;
	struct proc *pid_pglink;
	struct pid *pid_link;
	uint_t pid_ref;
};

#define	p_pgrp p_pgidp->pid_id
#define	p_pid  p_pidp->pid_id
#define	p_slot p_pidp->pid_prslot
#define	p_detached p_pgidp->pid_pgorphaned

#define	PID_HOLD(pidp)	ASSERT(MUTEX_HELD(&pidlock)); \
			++(pidp)->pid_ref;
#define	PID_RELE(pidp)	ASSERT(MUTEX_HELD(&pidlock)); \
			(pidp)->pid_ref > 1 ? \
				--(pidp)->pid_ref : pid_rele(pidp);

/*
 * Structure containing persistent process lock.  The structure and
 * macro allow "mutex_enter(&p->p_lock)" to continue working.
 */
struct plock {
	kmutex_t pl_lock;
};
#define	p_lock	p_lockp->pl_lock

extern proc_t p0;		/* process 0 */
extern struct plock p0lock;	/* p0's plock */
extern struct pid pid0;		/* p0's pid */

/* stat codes */

#define	SSLEEP	1		/* awaiting an event */
#define	SRUN	2		/* running */
#define	SZOMB	3		/* process terminated but not waited for */
#define	SSTOP	4		/* process stopped by debugger */
#define	SIDL	5		/* intermediate state in process creation */
#define	SONPROC	6		/* process is being run on a processor */

/* p_pidflag codes */
#define	CLDPEND	0x0001		/* have yet to post a SIGCLD to the parent */
#define	CLDCONT	0x0002		/* child has notified parent of CLD_CONTINUED */

/* p_flag codes */

/* XXX - several of these should be per-LWP. */

#define	SSYS	0x00000001	/* system (resident) process */
#define	STRC	0x00000002	/* ptrace() compatibility mode set by /proc */
#define	SLOAD	0x00000008	/* in core */
#define	SLOCK	0x00000010	/* process cannot be swapped */
#define	SPREXEC	0x00000020	/* process is in exec() (a flag for /proc) */
#define	SPROCTR	0x00000040	/* signal, fault or syscall tracing via /proc */
#define	SPRFORK	0x00000080	/* child inherits /proc tracing flags */
#define	SKILLED	0x00000100	/* SIGKILL has been posted to the process */
#define	SULOAD	0x00000200	/* u-block in core */
#define	SRUNLCL	0x00000400	/* set process running on last /proc close */
#define	SBPTADJ	0x00000800	/* adjust pc on breakpoint trap (/proc) */
#define	SKILLCL	0x00001000	/* kill process on last /proc close */
#define	SOWEUPC	0x00002000	/* owe process an addupc() call at next ast */
#define	SEXECED	0x00004000	/* this process has execed */
#define	SPASYNC	0x00008000	/* asynchronous stopping via /proc */
#define	SJCTL	0x00010000	/* SIGCLD sent when children stop/continue */
#define	SNOWAIT	0x00020000	/* children never become zombies */
#define	SVFORK	0x00040000	/* child of vfork that has not yet exec'd */
#define	SVFWAIT	0x00080000	/* parent of vfork waiting for child to exec */
#define	EXITLWPS  0x00100000	/* have lwps exit within the process */
#define	HOLDFORK  0x00200000	/* hold lwps where they're cloneable */
#define	SWAITSIG  0x00400000	/* SIGWAITING sent when all lwps block */
#define	HOLDFORK1 0x00800000	/* hold lwps in place (not cloning) */
#define	COREDUMP 0x01000000	/* process is dumping core */
#define	SMSACCT	0x02000000	/* process is keeping micro-state accounting */
#define	ASLWP 	0x04000000 	/* process uses an "aslwp" for async signals */
				/* also overloaded to mean it is a MT process */
#define	SPRLOCK	0x08000000	/* process locked by /proc */
#define	NOCD	0x10000000	/* new creds from VSxID, do not coredump */
#define	HOLDWATCH 0x20000000	/* hold lwps for watchpoint operation */
#define	SMSFORK	0x40000000	/* child inherits micro-state accounting */

/* Macro to convert proc pointer to a user block pointer */
#define	PTOU(p)		(&(p)->p_user)

#define	tracing(p, sig)	(sigismember(&(p)->p_sigmask, sig))

/* Macro to reduce unnecessary calls to issig() */

#define	ISSIG(t, why)	ISSIG_FAST(t, ttolwp(t), ttoproc(t), why)

/*
 * Fast version of ISSIG.
 *	1. uses register pointers to lwp and proc instead of reloading them.
 *	2. uses bit-wise OR of tests, since the usual case is that none of them
 *	   are true, this saves orcc's and branches.
 *	3. load the signal flags instead of using sigisempty() macro which does
 *	   a branch to convert to boolean.
 */
#define	ISSIG_FAST(t, lwp, p, why)		\
	(ISSIG_PENDING(t, lwp, p) && issig(why))

#define	ISSIG_PENDING(t, lwp, p)		\
	((lwp)->lwp_cursig |			\
	    (p)->p_sig.__sigbits[0] |		\
	    (p)->p_sig.__sigbits[1] |		\
	    (t)->t_sig.__sigbits[0] |		\
	    (t)->t_sig.__sigbits[1] |		\
	    (p)->p_stopsig |			\
	    ((t)->t_proc_flag & (TP_PRSTOP|TP_HOLDLWP|TP_CHKPT|TP_PAUSE)) | \
	    ((p)->p_flag & (EXITLWPS|SKILLED|HOLDFORK1|HOLDWATCH)))

#define	ISSTOP(sig)	 (u.u_signal[sig-1] == SIG_DFL && \
				sigismember(&stopdefault, sig))

#define	ISHOLD(p)	((p)->p_flag & HOLDFORK)

#define	MUSTRETURN(p, t)	(ISHOLD(p) | (t)->t_activefd.a_stale)

/*
 * Check for any anomolous conditions that may trigger a setrun
 */
#define	ISANOMALOUS(p)	(curthread->t_astflag | \
	((p)->p_flag & (EXITLWPS|SKILLED|HOLDFORK|HOLDFORK1|HOLDWATCH)) | \
	(curthread->t_proc_flag & TP_HOLDLWP))

/* Reasons for calling issig() */

#define	FORREAL		0	/* Usual side-effects */
#define	JUSTLOOKING	1	/* Don't stop the process */

/* 'what' values for stop(PR_SUSPENDED, what) */
#define	SUSPEND_NORMAL	0
#define	SUSPEND_PAUSE	1

/* pseudo-flag to lwp_create() */
#define	NOCLASS	(-1)

#ifdef _KERNEL

/* user profiling functions */

extern void profil_tick(uintptr_t);

/* process management functions */

extern caddr_t findvaddr(proc_t *);
extern int newproc(void (*)(), id_t, int);
extern void vfwait(pid_t);
extern void freeproc(proc_t *);
extern void setrun(kthread_t *);
extern void setrun_locked(kthread_t *);
extern void exit(int, int);
extern void relvm(void);
extern void add_ns(proc_t *, proc_t *);
extern void delete_ns(proc_t *, proc_t *);
extern void upcount_inc(uid_t);
extern void upcount_dec(uid_t);
extern int upcount_get(uid_t);
#if defined(i386) || defined(__i386) || defined(__ia64)
extern int ldt_dup(proc_t *, proc_t *);
#endif

extern void sigcld(proc_t *, sigqueue_t *);
extern void sigcld_delete(k_siginfo_t *);
extern void sigcld_repost(void);
extern int fsig(k_sigset_t *, kthread_t *);
extern void psig(void);
extern void stop(int, int);
extern int stop_on_fault(uint_t, k_siginfo_t *);
extern int issig(int);
extern int jobstopped(proc_t *);
extern void psignal(proc_t *, int);
extern void tsignal(kthread_t *, int);
extern void sigtoproc(proc_t *, kthread_t *, int);
extern void trapsig(k_siginfo_t *, int);
extern int eat_signal(kthread_t *t, int sig);

extern void pid_setmin(void);
extern pid_t pid_assign(proc_t *);
extern int pid_rele(struct pid *);
extern void pid_exit(proc_t *);
extern void proc_entry_free(struct pid *);
extern proc_t *prfind(pid_t);
extern proc_t *pgfind(pid_t);
extern proc_t *sprlock(pid_t);
extern void sprunlock(proc_t *);
extern void pid_init(void);
extern proc_t *pid_entry(int);
extern int pid_slot(proc_t *);
extern void signal(pid_t, int);
extern void prsignal(struct pid *, int);

extern void pgsignal(struct pid *, int);
extern void pgjoin(proc_t *, struct pid *);
extern void pgcreate(proc_t *);
extern void pgexit(proc_t *);
extern void pgdetach(proc_t *);
extern int pgmembers(pid_t);

extern	void	init_mstate(kthread_t *, int);
extern	int	new_mstate(kthread_t *, int);
extern	void	restore_mstate(kthread_t *);
extern	void	term_mstate(kthread_t *);
extern	void	estimate_msacct(kthread_t *, hrtime_t);
extern	void	disable_msacct(proc_t *);

extern	uint_t	cpu_decay(uint_t, clock_t);
extern	uint_t	cpu_grow(uint_t, clock_t);

extern void	set_proc_pre_sys(proc_t *p);
extern void	set_proc_post_sys(proc_t *p);
extern void	set_proc_sys(proc_t *p);
extern void	set_proc_ast(proc_t *p);
extern void	set_all_proc_sys(void);

/* thread function prototypes */

extern	kthread_t	*thread_create(
	caddr_t		stk,
	size_t		stksize,
	void		(*proc)(),
	caddr_t		arg,
	size_t		len,
	proc_t 		*pp,
	int		state,
	int		pri);
extern	void	thread_exit(void);
extern	void	thread_destroy(kthread_t *);
extern	void	thread_free(kthread_t *);
extern	int	reaper(void);
extern	void	installctx(kthread_t *, void *, void (*)(), void (*)(),
    void (*)(), void (*)(), void (*)());
extern	int	removectx(kthread_t *, void *, void (*)(), void (*)(),
    void (*)(), void (*)(), void (*)());
extern	void	savectx(kthread_t *);
extern	void	restorectx(kthread_t *);
extern	void	forkctx(kthread_t *, kthread_t *);
extern	void	lwp_createctx(kthread_t *, kthread_t *);
extern	void	freectx(kthread_t *, int);
extern	kthread_t *thread_unpin(void);
extern	void	release_interrupt(void);
extern	int	thread_create_intr(struct cpu *);
extern	void	thread_init(void);
extern	int	thread_load(kthread_t *, void (*)(), caddr_t, size_t);

extern	void	tsd_create(uint_t *, void (*)(void *));
extern	void	tsd_destroy(uint_t *);
extern	void	*tsd_getcreate(uint_t *, void (*)(void *), void *(*)(void));
extern	void	*tsd_get(uint_t);
extern	int	tsd_set(uint_t, void *);
extern	void	tsd_exit(void);
extern	void	*tsd_agent_get(kthread_t *, uint_t);
extern	int	tsd_agent_set(kthread_t *, uint_t, void *);

/* lwp function prototypes */

extern	klwp_t 		*lwp_create(
	void		(*proc)(),
	caddr_t		arg,
	size_t		len,
	proc_t		*p,
	int		state,
	int		pri,
	k_sigset_t	smask,
	int		cid);
extern	void	lwp_create_done(kthread_t *);
extern	void	lwp_exit(void);
extern	void	lwp_cleanup(void);
extern	int	lwp_suspend(kthread_t *, int *);
extern	void	lwp_continue(kthread_t *);
extern	void	holdlwp(void);
extern	void	stoplwp(void);
extern	int	holdlwps(int);
extern	int	holdwatch(void);
extern	void	pokelwps(proc_t *);
extern	void	continuelwps(proc_t *);
extern	int	exitlwps(int);
extern	klwp_t	*forklwp(klwp_t *, proc_t *);
extern	int	sigisheld(proc_t *, int);
extern	int	sigwaiting_send(kmutex_t *);
extern	void	lwp_load(klwp_t *, gregset_t);
extern	void	lwp_setrval(klwp_t *, int, int);
extern	void	lwp_forkregs(klwp_t *, klwp_t *);
extern	void	lwp_freeregs(klwp_t *, int);
extern	caddr_t	lwp_stk_init(klwp_t *, caddr_t);
extern	void	lwp_stk_fini(klwp_t *);
extern	void	lwp_rtt(void);

/*
 * Signal queue function prototypes. Must be here due to header ordering
 * dependencies.
 */
extern void sigqfree(proc_t *);
extern void siginfofree(sigqueue_t *);
extern int sigdeq(proc_t *, kthread_t *, int, sigqueue_t **);
extern void sigdelq(proc_t *, kthread_t *, int);
extern void sigaddq(proc_t *, kthread_t *, k_siginfo_t *, int);
extern void sigaddqa(proc_t *, kthread_t *, sigqueue_t *);
extern void sigdupq(proc_t *, proc_t *);
extern sigqueue_t *sigqalloc(sigqhdr_t **);
extern int sigqhdralloc(sigqhdr_t **, int, int);
extern void sigqhdrfree(sigqhdr_t **);
extern void sigqrel(sigqueue_t *);
extern sigqueue_t *sigappend(k_sigset_t *, sigqueue_t *,
	k_sigset_t *, sigqueue_t *);
extern sigqueue_t *sigprepend(k_sigset_t *, sigqueue_t *,
	k_sigset_t *, sigqueue_t *);
extern int csigtimedwait(sigset_t *, siginfo_t *, struct timespec *, int *);
extern void winfo(proc_t *, k_siginfo_t *, int);
extern int wstat(int, int);
extern int sendsig(int, k_siginfo_t *, void (*)());
#ifdef _SYSCALL32_IMPL
extern int sendsig32(int, k_siginfo_t *, void (*)());
#endif

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROC_H */
