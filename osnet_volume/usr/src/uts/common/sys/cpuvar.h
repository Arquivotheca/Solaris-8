/*
 * Copyright (c) 1989, 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CPUVAR_H
#define	_SYS_CPUVAR_H

#pragma ident	"@(#)cpuvar.h	1.72	99/10/08 SMI"

#include <sys/thread.h>
#include <sys/sysinfo.h>	/* has cpu_stat_t definition */
#include <sys/disp.h>
#include <sys/processor.h>

#if (defined(_KERNEL) || defined(_KMEMUSER)) && defined(_MACHDEP)
#include <sys/machcpuvar.h>
#endif

#include <sys/types.h>
#include <sys/file.h>
#include <sys/bitmap.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This struct is defined to implement tracing on MPs.
 */

typedef struct tracedata {
	char	*tbuf_start;		/* start of ring buffer */
	char	*tbuf_end;		/* end of ring buffer */
	char	*tbuf_wrap;		/* wrap-around threshold */
	char	*tbuf_head;		/* where data is written to */
	char	*tbuf_tail;		/* where data is flushed from */
	char	*tbuf_redzone;		/* red zone in the ring buffer */
	char	*tbuf_overflow;		/* set if red zone is entered */
	uchar_t	*real_event_map;	/* event enabled/used bitmap */
	uchar_t	*event_map;		/* either real or null event map */
	struct file *trace_file;	/* file to flush to */
	uint32_t last_hrtime_lo32;	/* low 32 bits of hrtime at last TP */
	kthread_id_t	last_thread;	/* TID of thread at last TP */
	ulong_t	scratch[4];		/* traps-off TP register save area */
} tracedata_t;

/*
 * For fast event tracing.
 */
struct ftrace_record;
typedef struct ftrace_data {
	int			ftd_state;	/* ftrace flags */
	kmutex_t		ftd_mutex;	/* ftrace buffer lock */
	struct ftrace_record	*ftd_cur;	/* current record */
	struct ftrace_record	*ftd_first;	/* first record */
	struct ftrace_record	*ftd_last;	/* last record */
} ftrace_data_t;

struct cyc_cpu;

/*
 * Per-CPU data.
 */
typedef struct cpu {
	processorid_t	cpu_id;		/* CPU number */
	processorid_t	cpu_seqid;	/* sequential CPU id (0..ncpus-1) */
	volatile ushort_t cpu_flags;	/* flags indicating CPU state */
	kthread_id_t	cpu_thread;		/* current thread */
	kthread_id_t	cpu_idle_thread; 	/* idle thread for this CPU */
	kthread_id_t	cpu_pause_thread;	/* pause thread for this CPU */
	klwp_id_t	cpu_lwp;		/* current lwp (if any) */
	klwp_id_t	cpu_fpowner;		/* currently loaded fpu owner */
	struct cpupart	*cpu_part;		/* partition with this CPU */
	int		cpu_cache_offset;	/* see kmem.c for details */

	/*
	 * Links - protected by cpu_lock.
	 */
	struct cpu	*cpu_next;	/* next existing CPU */
	struct cpu	*cpu_prev;

	struct cpu	*cpu_next_onln;	/* next online (enabled) CPU */
	struct cpu	*cpu_prev_onln;

	struct cpu	*cpu_next_part;	/* next CPU in partition */
	struct cpu	*cpu_prev_part;

	/*
	 * Scheduling variables.
	 */
	disp_t		cpu_disp;	/* dispatch queue data */
	char		cpu_runrun;	/* scheduling flag - set to preempt */
	char		cpu_kprunrun;	/* force kernel preemption */
	pri_t		cpu_chosen_level; /* priority level at which cpu */
					/* was chosen for scheduling */
	kthread_id_t	cpu_dispthread;	/* thread selected for dispatch */
	disp_lock_t	cpu_thread_lock; /* dispatcher lock on current thread */
	/*
	 * The following field is updated when ever the cpu_dispthread
	 * changes. Also in places, where the current thread(cpu_dispthread)
	 * priority changes. This is used in disp_lowpri_cpu()
	 */
	pri_t		cpu_dispatch_pri; /* priority of cpu_dispthread */
	clock_t		cpu_last_swtch;	/* last time switched to new thread */

	/*
	 * Interrupt data.
	 */
	caddr_t		cpu_intr_stack;	/* interrupt stack */
	int		cpu_on_intr;	/* on interrupt stack */
	kthread_id_t	cpu_intr_thread; /* interrupt thread list */
	uint_t		cpu_intr_actv;	/* interrupt levels active (bitmask) */
	int		cpu_base_spl;	/* priority for highest rupt active */

	/*
	 * Statistics.
	 */
	cpu_stat_t	cpu_stat;	/* per cpu statistics */
	struct kstat	*cpu_kstat;	/* kstat for this cpu's statistics */

	uintptr_t	cpu_profile_cyclic_id; /* profile cyclic id */
	uintptr_t	cpu_profile_pc;	/* trapped PC in profile interrupt */
	uintptr_t	cpu_profile_pil; /* PIL when profile interrupted */
	hrtime_t	cpu_profile_when; /* when next profile intr is due */
	hrtime_t	cpu_profile_ilate; /* interrupt latency (native time) */

	tracedata_t	cpu_trace;	/* per cpu trace data */
	ftrace_data_t	cpu_ftrace;	/* per cpu ftrace data */

	/*
	 * Configuration information for the processor_info system call.
	 */
	processor_info_t cpu_type_info;	/* config info */
	time_t	cpu_state_begin;	/* when CPU entered current state */
	char	cpu_cpr_flags;		/* CPR related info */
	struct cyc_cpu *cpu_cyclic;	/* per cpu cyclic subsystem data */

#if (defined(_KERNEL) || defined(_KMEMUSER)) && defined(_MACHDEP)
	/*
	 * XXX - needs to be fixed. Structure size should not change.
	 *	 probably needs to be a pointer to an opaque structure.
	 * XXX - this is OK as long as cpu structs aren't in an array.
	 *	 A user program will either read the first part,
	 *	 which is machine-independent, or read the whole thing.
	 */
	struct machcpu 	cpu_m;		/* per architecture info */
#endif
} cpu_t;

#define	INTR_STACK_SIZE	MAX(8192, PAGESIZE)

/* MEMBERS PROTECTED BY "atomicity": cpu_flags */

/*
 * Flags in the CPU structure.
 *
 * These are protected by cpu_lock (except during creation).
 *
 * Offlined-CPUs have three stages of being offline:
 *
 * CPU_ENABLE indicates that the CPU is participating in I/O interrupts
 * that can be directed at a number of different CPUs.  If CPU_ENABLE
 * is off, the CPU will not be given interrupts that can be sent elsewhere,
 * but will still get interrupts from devices associated with that CPU only,
 * and from other CPUs.
 *
 * CPU_OFFLINE indicates that the dispatcher should not allow any threads
 * other than interrupt threads to run on that CPU.  A CPU will not have
 * CPU_OFFLINE set if there are any bound threads (besides interrupts).
 *
 * CPU_QUIESCED is set if p_offline was able to completely turn idle the
 * CPU and it will not have to run interrupt threads.  In this case it'll
 * stay in the idle loop until CPU_QUIESCED is turned off.
 *
 * On some platforms CPUs can be individually powered off.
 * The following flags are set for powered off CPUs: CPU_QUIESCED,
 * CPU_OFFLINE, and CPU_POWEROFF.  The following flags are cleared:
 * CPU_RUNNING, CPU_READY, CPU_EXISTS, and CPU_ENABLE.
 */
#define	CPU_RUNNING	0x01		/* CPU running */
#define	CPU_READY	0x02		/* CPU ready for cross-calls */
#define	CPU_QUIESCED	0x04		/* CPU will stay in idle */
#define	CPU_EXISTS	0x08		/* CPU is configured */
#define	CPU_ENABLE	0x10		/* CPU enabled for interrupts */
#define	CPU_OFFLINE	0x20		/* CPU offline via p_online */
#define	CPU_POWEROFF	0x40		/* CPU is powered off */

#define	CPU_ACTIVE(cpu)	(((cpu)->cpu_flags & CPU_OFFLINE) == 0)

#if (defined(_KERNEL) || defined(_KMEMUSER)) && defined(_MACHDEP)

/*
 * Macros for manipulating sets of CPUs as a bitmap.  Note that this
 * bitmap may vary in size depending on the maximum CPU id a specific
 * platform supports.  This may be different than the number of CPUs
 * the platform supports, since CPU ids can be sparse.  We define two
 * sets of macros; one for platforms where the maximum CPU id is less
 * than the number of bits in a single word (32 in a 32-bit kernel,
 * 64 in a 64-bit kernel), and one for platforms that require bitmaps
 * of more than one word.
 */

#define	CPUSET_WORDS	BT_BITOUL(NCPU)

#if	CPUSET_WORDS > 1

typedef struct cpuset {
	ulong_t	cpub[CPUSET_WORDS];
} cpuset_t;

/*
 * Private functions for manipulating cpusets that do not fit in a
 * single word.  These should not be used directly; instead the
 * CPUSET_* macros should be used so the code will be portable
 * across different definitions of NCPU.
 */
extern	void	cpuset_all(cpuset_t *);
extern	void	cpuset_all_but(cpuset_t *, uint_t);
extern	int	cpuset_isnull(cpuset_t *);
extern	int	cpuset_cmp(cpuset_t *, cpuset_t *);
extern	void	cpuset_only(cpuset_t *, uint_t);

#define	CPUSET_ALL(set)			cpuset_all(&(set))
#define	CPUSET_ALL_BUT(set, cpu)	cpuset_all_but(&(set), cpu)
#define	CPUSET_ONLY(set)		cpuset_only(&(set), cpu)
#define	CPU_IN_SET(set, cpu)		BT_TEST((set).cpub, cpu)
#define	CPUSET_ADD(set, cpu)		BT_SET((set).cpub, cpu)
#define	CPUSET_DEL(set, cpu)		BT_CLEAR((set).cpub, cpu)
#define	CPUSET_ISNULL(set)		cpuset_isnull(&(set))
#define	CPUSET_ISEQUAL(set1, set2)	cpuset_cmp(&(set1), &(set2))

#define	CPUSET_OR(set1, set2)		{		\
	int _i;						\
	for (_i = 0; _i < CPUSET_WORDS; _i++)		\
		(set1).cpub[_i] |= (set2).cpub[_i];	\
}

#define	CPUSET_AND(set1, set2)		{		\
	int _i;						\
	for (_i = 0; _i < CPUSET_WORDS; _i++)		\
		(set1).cpub[_i] &= (set2).cpub[_i];	\
}

#define	CPUSET_ZERO(set)		{		\
	int _i;						\
	for (_i = 0; _i < CPUSET_WORDS; _i++)		\
		(set).cpub[_i] = 0;			\
}

#elif	CPUSET_WORDS == 1

typedef	ulong_t	cpuset_t;	/* a set of CPUs */

#define	CPUSET(cpu)			(1UL << (cpu))

#define	CPUSET_ALL(set)			((void)((set) = ~0UL))
#define	CPUSET_ALL_BUT(set, cpu)	((void)((set) = ~CPUSET(cpu)))
#define	CPUSET_ONLY(set, cpu)		((void)((set) = CPUSET(cpu)))
#define	CPU_IN_SET(set, cpu)		((set) & CPUSET(cpu))
#define	CPUSET_ADD(set, cpu)		((void)((set) |= CPUSET(cpu)))
#define	CPUSET_DEL(set, cpu)		((void)((set) &= ~CPUSET(cpu)))
#define	CPUSET_ISNULL(set)		((set) == 0)
#define	CPUSET_ISEQUAL(set1, set2)	((set1) == (set2))
#define	CPUSET_OR(set1, set2)   	((void)((set1) |= (set2)))
#define	CPUSET_AND(set1, set2)  	((void)((set1) &= (set2)))
#define	CPUSET_ZERO(set)		((void)((set) = 0))

#else	/* CPUSET_WORDS <= 0 */

#error NCPU is undefined or invalid

#endif	/* CPUSET_WORDS	*/

extern cpuset_t cpu_seqid_inuse;

#endif	/* (_KERNEL || _KMEMUSER) && _MACHDEP */

#define	CPU_CPR_ONLINE		0x1
#define	CPU_CPR_IS_OFFLINE(cpu)	(((cpu)->cpu_cpr_flags & CPU_CPR_ONLINE) == 0)
#define	CPU_SET_CPR_FLAGS(cpu, flag)	((cpu)->cpu_cpr_flags |= flag)

extern struct cpu	*cpu[];		/* indexed by CPU number */
extern cpu_t		*cpu_list;	/* list of CPUs */
extern int		ncpus;		/* number of CPUs present */
extern int		ncpus_online;	/* number of CPUs not quiesced */
extern int		max_ncpus;	/* max present before ncpus is known */
extern int		boot_max_ncpus;	/* like max_ncpus but for real */

#if defined(i386) || defined(__i386)
extern struct cpu *curcpup(void);
#define	CPU		(curcpup())	/* Pointer to current CPU */
#else
#define	CPU		(curthread->t_cpu)	/* Pointer to current CPU */
#endif

/*
 * CPU_CURRENT indicates to thread_affinity_set to use CPU->cpu_id
 * as the target and to grab cpu_lock instead of requiring the caller
 * to grab it.
 */
#define	CPU_CURRENT	-3

/*
 * Macros to update CPU statistics.
 *
 * CPU_STAT_ADD_K can be used when we want accurate counts, and we know
 * that an interrupt thread that could interrupt us will not try to
 * increment the same cpu stat.  The benefit of using these routines is
 * that we only increment t_kpreempt instead of acquiring a mutex.
 */

#define	CPU_STAT_ENTER_K()	kpreempt_disable()
#define	CPU_STAT_EXIT_K()	kpreempt_enable()

#define	CPU_STAT_ADD_K(thing, amount) \
	{	kpreempt_disable(); /* keep from switching CPUs */\
		CPU_STAT_ADDQ(CPU, thing, amount); \
		kpreempt_enable(); \
	}

#define	CPU_STAT_ADDQ(cpuptr, thing, amount) \
	cpuptr->cpu_stat.thing += amount

/*
 * CPU support routines.
 */
#if	defined(_KERNEL) && defined(__STDC__)	/* not for genassym.c */

void	cpu_list_init(cpu_t *);
void	cpu_add_unit(cpu_t *);
void	cpu_del_unit(int cpuid);
void	cpu_add_active(cpu_t *);
void	cpu_kstat_init(cpu_t *);

void	mbox_lock_init(void);	 /* initialize cross-call locks */
void	mbox_init(int cpun);	 /* initialize cross-calls */
void	poke_cpu(int cpun);	 /* interrupt another CPU (to preempt) */

void	pause_cpus(cpu_t *off_cp);
void	start_cpus(void);

void	cpu_pause_init(void);
cpu_t	*cpu_get(processorid_t cpun);	/* get the CPU struct associated */
int	cpu_status(cpu_t *cp);
int	cpu_online(cpu_t *cp);
int	cpu_offline(cpu_t *cp);
int	cpu_poweron(cpu_t *cp);		/* take powered-off cpu to off-line */
int	cpu_poweroff(cpu_t *cp);	/* take off-line cpu to powered-off */

void	cpu_setstate(cpu_t *cp, int state);
cpu_t	*cpu_intr_next(cpu_t *cp);	/* get next online CPU taking intrs */
int	cpu_intr_count(cpu_t *cp);	/* count # of CPUs handling intrs */
int	cpu_intr_on(cpu_t *cp);		/* CPU taking I/O interrupts? */
void	cpu_intr_enable(cpu_t *cp);	/* enable I/O interrupts */
int	cpu_intr_disable(cpu_t *cp);	/* disable I/O interrupts */
int	cpu_up(cpu_t *cp);		/* CPU scheduling threads */
int	cpu_down(cpu_t *cp);		/* CPU not scheduling threads */

int	cpu_configure(int);
int	cpu_unconfigure(int);
void	cpu_destroy_bound_threads(cpu_t *cp);

struct bind_arg {			/* args passed through dotoprocs */
	processorid_t		bind;
	processorid_t		obind;
	int			err;	/* non-zero error number if any */
};


int	cpu_bind_process(proc_t *pp, struct bind_arg *arg);
int	cpu_bind_thread(kthread_id_t tp, struct bind_arg *arg);

extern void thread_affinity_set(kthread_id_t t, int cpu_id);
extern void thread_affinity_clear(kthread_id_t t);
extern void affinity_set(int cpu_id);
extern void affinity_clear(void);

/*
 * The following routines affect the CPUs participation in interrupt processing,
 * if that is applicable on the architecture.  This only affects interrupts
 * which aren't directed at the processor (not cross calls).
 *
 * cpu_disable_intr returns non-zero if interrupts were previously enabled.
 */
int	cpu_disable_intr(struct cpu *cp); /* stop issuing interrupts to cpu */
void	cpu_enable_intr(struct cpu *cp); /* start issuing interrupts to cpu */

/*
 * The mutex cpu_lock protects cpu_flags for all CPUs, as well as the ncpus
 * and ncpus_online counts.
 */
extern kmutex_t	cpu_lock;	/* lock protecting CPU data */

typedef enum {
	CPU_CONFIG,
	CPU_UNCONFIG
} cpu_setup_t;

typedef int cpu_setup_func_t(cpu_setup_t, int, void *);

/*
 * Routines used to register interest in cpu's being added to or removed
 * from the system.
 */
extern void register_cpu_setup_func(cpu_setup_func_t *, void *);
extern void unregister_cpu_setup_func(cpu_setup_func_t *, void *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_CPUVAR_H */
