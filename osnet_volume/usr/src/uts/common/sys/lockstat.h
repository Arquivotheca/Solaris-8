/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_LOCKSTAT_H
#define	_SYS_LOCKSTAT_H

#pragma ident	"@(#)lockstat.h	1.4	99/07/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The lockstat measurments in this driver are done in four forms:
 * basic, timing, histogram, and stack trace.
 *
 * LS_BASIC reports lock address, caller, event (e.g. spin on mutex,
 * block on rwlock, etc), and number of times the event occurred.
 *
 * LS_TIME provides everything in LS_BASIC plus cumulative event duration
 * in whatever units are convenient (for example, we use a loop count for
 * mutex spins but a sleep time for mutex blocks and hold times).
 *
 * LS_HIST provides everything in LS_TIME plus power-of-2 histogram buckets
 * for event durations.  This makes it possible to detect (or exclude)
 * extreme outliers and determine wait-time and hold-time distributions.
 *
 * LS_STACK provides everything in LS_HIST plus complete stack traces
 * indicating where the events took place.
 */

/*
 * Types of events the lockstat driver can record.  When the lockstat
 * driver is active the global lockstat_event[] table indicates which
 * events to record.  Note that hold times are *much* more expensive to
 * gather since the lockstat driver must interpose on white-hot code paths.
 */

/*
 * Contention events (lockstat -C)
 */
#define	LS_ADAPTIVE_MUTEX_SPIN		0
#define	LS_ADAPTIVE_MUTEX_BLOCK		1
#define	LS_SPIN_LOCK			2
#define	LS_THREAD_LOCK			3
#define	LS_RW_WRITER_BY_WRITER		4
#define	LS_RW_WRITER_BY_READERS		5
#define	LS_RW_READER_BY_WRITER		6
#define	LS_RW_READER_BY_WRITE_WANTED	7

/*
 * Hold-time events (lockstat -H)
 */
#define	LS_ADAPTIVE_MUTEX_HOLD		32
#define	LS_SPIN_LOCK_HOLD		33
#define	LS_RW_WRITER_HOLD		34
#define	LS_RW_READER_HOLD		35

/*
 * Interrupt events (lockstat -I)
 */
#define	LS_PROFILE_INTR			56

/*
 * Error events (lockstat -E)
 */
#define	LS_ERROR_BASE			60	/* error events from here up */
#define	LS_RECURSION_DETECTED		60
#define	LS_ENTER_FAILED			61
#define	LS_EXIT_FAILED			62
#define	LS_RECORD_FAILED		63

#define	LS_MAX_EVENTS			64

/*
 * Bits describing each event in the lockstat_event[] table
 */
#define	LSE_ENTER	0x01		/* interpose on lock enter */
#define	LSE_EXIT	0x02		/* interpose on lock exit */
#define	LSE_RECORD	0x04		/* record lock event */
#define	LSE_TRACE	0x08		/* trace (rather than sample) event */

#ifndef _ASM

#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/systm.h>

#define	LS_MAX_STACK_DEPTH	50

typedef struct lsrec {
	struct lsrec	*ls_next;	/* next in hash chain */
	uintptr_t	ls_lock;	/* lock address */
	uintptr_t	ls_caller;	/* caller address */
	uint32_t	ls_count;	/* cumulative event count */
	uint32_t	ls_event;	/* type of event */
	uintptr_t	ls_refcnt;	/* cumulative reference count */
	uint64_t	ls_time;	/* cumulative event duration */
	uint32_t	ls_hist[64];	/* log2(duration) histogram */
	uintptr_t	ls_stack[LS_MAX_STACK_DEPTH];
} lsrec_t;

typedef struct ls_pend {
	uintptr_t	lp_lock;	/* lock address */
	uintptr_t	lp_owner;	/* lock owner */
	hrtime_t	lp_start_time;	/* pending event's start time */
	uint32_t	lp_refcnt;
	uintptr_t	lp_caller;
	lock_t		lp_mylock;	/* in case platform code needs it */
} ls_pend_t;

/*
 * Definitions for the types of experiments which can be run.  They are
 * listed in increasing order of memory cost and processing time cost.
 * The numerical value of each type is the number of bytes needed per record.
 */
#define	LS_BASIC	offsetof(lsrec_t, ls_time)
#define	LS_TIME		offsetof(lsrec_t, ls_hist[0])
#define	LS_HIST		offsetof(lsrec_t, ls_stack[0])
#define	LS_STACK(depth)	offsetof(lsrec_t, ls_stack[depth])

/*
 * The lockstat driver can be programmed to watch only a certain set
 * of locks or functions to reduce its Heisenberg effect.  This number
 * can't be too large, however, or searching the watch list will become
 * more invasive than just recording all the data!
 */
#define	LS_MAX_WATCH	32

typedef struct lswatch {
	uintptr_t	lw_base;
	size_t		lw_size;
} lswatch_t;

/*
 * Control structure written to /dev/lockstat to start an experiment.
 */
typedef struct lsctl {
	size_t		lc_recsize;	/* type of lock recording to do */
	size_t		lc_nrecs;	/* number of records to allocate */
	hrtime_t	lc_interval;	/* profiling interrupt period */
	uchar_t		lc_event[LS_MAX_EVENTS]; /* which events to record */
	hrtime_t	lc_min_duration[LS_MAX_EVENTS]; /* duration filter */
	lswatch_t	lc_wlock[LS_MAX_WATCH + 1]; /* watched locks */
	lswatch_t	lc_wfunc[LS_MAX_WATCH + 1]; /* watched functions */
} lsctl_t;

#ifdef _KERNEL

/*
 * Platform-independent kernel support for the lockstat driver.
 */
extern uchar_t lockstat_event[LS_MAX_EVENTS];
extern void (*lockstat_enter_op)(uintptr_t, uintptr_t, uintptr_t);
extern void (*lockstat_exit_op)(uintptr_t, uintptr_t, uint32_t,
	uintptr_t, uintptr_t);
extern void (*lockstat_record_op)(uintptr_t, uintptr_t, uint32_t,
	uintptr_t, hrtime_t);
extern void lockstat_enter_nop(uintptr_t, uintptr_t, uintptr_t);
extern void lockstat_exit_nop(uintptr_t, uintptr_t, uint32_t,
	uintptr_t, uintptr_t);
extern void lockstat_record_nop(uintptr_t, uintptr_t, uint32_t,
	uintptr_t, hrtime_t);
extern int lockstat_active_threads(void);
extern int lockstat_depth(void);
extern void lockstat_interrupt_on(hrtime_t);
extern void lockstat_interrupt_off(void);

/*
 * Platform-specific lockstat support.
 */
extern int lockstat_event_start(uintptr_t, ls_pend_t *);
extern hrtime_t lockstat_event_end(ls_pend_t *);
extern void lockstat_hot_patch(void);

/*
 * Macros to record lockstat events.
 */
#define	LOCKSTAT_RECORD(event, lp, time, refcnt)			\
	if (lockstat_event[event] & LSE_RECORD)				\
		curthread->t_lockstat++,				\
		lockstat_record_op((uintptr_t)(lp), (uintptr_t)caller(), \
			(event), (refcnt), (time)),			\
		curthread->t_lockstat--;

#define	LOCKSTAT_ENTER(event, lp, owner)				\
	if (lockstat_event[event] & LSE_ENTER)				\
		curthread->t_lockstat++,				\
		lockstat_enter_op((uintptr_t)(lp), (uintptr_t)caller(),	\
			(uintptr_t)(owner)),				\
		curthread->t_lockstat--;

#define	LOCKSTAT_EXIT(event, lp, owner, refcnt)				\
	if (lockstat_event[event] & LSE_EXIT)				\
		curthread->t_lockstat++,				\
		lockstat_exit_op((uintptr_t)(lp), (uintptr_t)caller(),	\
			(event), (refcnt), (uintptr_t)(owner)),		\
		curthread->t_lockstat--;

#endif /* _KERNEL */

#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LOCKSTAT_H */
