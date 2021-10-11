/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _UTRACE_H
#define	_UTRACE_H

#pragma ident	"@(#)utrace.h	1.15	97/08/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * this file contains definitions for user-level tracing.
 */

/*
 * Allocation of facility space
 * 128 - 159 libthread
 * 160 - 191 libc
 * 192 - 223 other libraries (libnsl, etc.)
 * 224 - 255 users
 */

#ifndef _ASM
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <synch.h>
#include <sys/lwp.h>
#endif /* _ASM */
#include <thread.h>
#include <synch.h>
#include <sys/stack.h>
#include <sys/vtrace.h>

#ifdef ITRACE
#include <sys/lwp_trace_temp.h>
#endif

#if defined(ITRACE) || defined(UTRACE)

#ifndef _ASM

extern u_char *event_map;

u_char trace_label(u_char, u_char, u_short, char *);

void trace_0(u_long);
void trace_1(u_long, u_long);
void trace_2(u_long, u_long, u_long);
void trace_3(u_long, u_long, u_long, u_long);
void trace_4(u_long, u_long, u_long, u_long, u_long);
void trace_5(u_long, u_long, u_long, u_long, u_long, u_long);
void trace_on(void);
void trace_off(void);
void trace_close(void);

void enable_tracepoint();
void disable_tracepoint();
void enable_all_tracepoints();
void disable_all_tracepoints();

#define	TRACE_N(fac, tag, name, len, func) \
	{ \
		u_char xvt_info = event_map[FT2EVENT(fac, tag)]; \
		if (xvt_info & VT_ENABLED) { \
			if (!(xvt_info & VT_USED)) \
				xvt_info = trace_label(fac, tag, len, name); \
			func; \
		} \
	}

#if defined(ITRACE)

#define	ITRACE_0(fac, tag, name) TRACE_0(fac, tag, name)
#define	ITRACE_1(fac, tag, name, d1) TRACE_1(fac, tag, name, d1)
#define	ITRACE_2(fac, tag, name, d1, d2) TRACE_2(fac, tag, name, d1, d2)
#define	ITRACE_3(fac, tag, name, d1, d2, d3) TRACE_3(fac, tag, name, d1, d2,\
    d3)
#define	ITRACE_4(fac, tag, name, d1, d2, d3, d4) TRACE_4(fac, tag, name, d1,\
    d2, d3, d4)
#define	ITRACE_5(fac, tag, name, d1, d2, d3, d4, d5) TRACE_5(fac, tag, name,\
    d1, d2, d3, d4, d5)

#else /* if defined(UTRACE) */

#define	ITRACE_0(fac, tag, name)
#define	ITRACE_1(fac, tag, name, d1)
#define	ITRACE_2(fac, tag, name, d1, d2)
#define	ITRACE_3(fac, tag, name, d1, d2, d3)
#define	ITRACE_4(fac, tag, name, d1, d2, d3, d4)
#define	ITRACE_5(fac, tag, name, d1, d2, d3, d4, d5)

#endif

#else /* _ASM */

/*
 * Notes on tracing from assembly files:
 *
 * 1. Usage: TRACE_ASM_<n> (scr, fac, tag, namep [, data_1, ..., data_n]);
 *
 *	scr	= scratch register (will be clobbered)
 *	fac	= facility
 *	tag	= tag
 *	namep	= address of name:format string (ascii-z)
 *	data_i	= any register or "simm13" (-4096 <= x <= 4095) constant
 *
 *    Example:
 *
 *		.global TR_intr_start;
 *	TR_intr_start:
 *		.asciz "interrupt_start:level %d";
 *		.align 4;
 *    ...
 *	TRACE_ASM_1 (%o2, TR_FAC_INTR, TR_INTR_START, TR_intr_start, %l4);
 *
 *    When you use TRACE_ASM_[1-5], the data words you specify will be
 *    copied into %o1-%o5, in that order.  Make sure that (1) you either
 *    save or don't need the affected out-registers, and (2) you don't
 *    step on yourself [e.g. TRACE_ASM_2(..., %o2, %o1) would cause %o1
 *    to be overwritten by %o2].  Also, note that %o0 is always used as a
 *    scratch register, but only *after* your data values have been
 *    copied into %o1-%o5.  Thus, for example, TRACE_ASM_1(..., %o0) will
 *    work just fine, because %o0 will be copied into %o1 before it is
 *    clobbered.
 *    In the example above, %l4 is copied into %o1, and %o0 and %o2 are
 *    clobbered.
 *
 * 2. Registers: TRACE_ASM_N destroys %o0-%oN and the scratch register,
 *    and leaves the rest intact.
 *
 * 3. You can't put trace points where traps are disabled.
 *
 * 4. (Obvious, but...) Don't put a trace macro in a branch delay slot.
 *
 * 5. "name" should be the *address* of an ascii-z string, not the string
 *    itself.
 *
 * 6. BE CAREFUL if you ever change this macro.  To avoid using local
 *    labels (which could collide with neighboring code), the two
 *    branches below are hand-computed.
 */

#define	TRACE_ASM_N(scr, fac, tag, name, len, func)			\
	set	event_map, scr;			/* %o0 = map addr */	\
	ld	[scr], %o0;						\
	set	FT2EVENT(fac, tag), scr;	/* scr = event */	\
	ldub	[%o0 + scr], %o0;		/* %o0 = event info */	\
	andcc	%o0, VT_ENABLED, %g0;		/* is event enabled? */	\
	bz	. + 29*4;			/* EVIL! DANGER! */	\
	andcc	%o0, VT_USED, %g0;		/* is event used? */	\
	bnz	. + 25*4;			/* EVIL! DANGER! */	\
	sll	scr, 16, scr;			/* scr = header word */	\
	/* save the world, go to C for event label */			\
	save	%sp, -SA(MINFRAME), %sp;	/* save ins, locals */	\
	mov	%g1, %l1;			/* save globals */	\
	mov	%g2, %l2;						\
	mov	%g3, %l3;						\
	mov	%g4, %l4;						\
	mov	%g5, %l5;						\
	mov	%g6, %l6;						\
	mov	%g7, %l7;						\
	or	%g0, (fac), %o0;		/* %o0 = facility */	\
	or	%g0, (tag), %o1;		/* %o1 = tag */		\
	or	%g0, (len), %o2;		/* %o2 = length */	\
	sethi	%hi(name), %o3;						\
	call	trace_label;						\
	or	%o3, %lo(name), %o3;		/* delay: %o3 = name */	\
	mov	%o0, %i0;			/* return vaule */	\
	mov	%l1, %g1;			/* restore globals */	\
	mov	%l2, %g2;						\
	mov	%l3, %g3;						\
	mov	%l4, %g4;						\
	mov	%l5, %g5;						\
	mov	%l6, %g6;						\
	mov	%l7, %g7;						\
	restore;							\
	/* event label done, state restored */				\
	call	func;				/* write the record */	\
	or	%o0, scr, %o0;			/* delay: event info */

#define	TRACE_ASM_0(scr, fac, tag, name) \
	TRACE_ASM_N(scr, fac, tag, name, 4, trace_0)

#define	TRACE_ASM_1(scr, fac, tag, name, d1) \
	mov d1, %o1;							\
	TRACE_ASM_N(scr, fac, tag, name, 8, trace_1)

#define	TRACE_ASM_2(scr, fac, tag, name, d1, d2) \
	mov d1, %o1; mov d2, %o2;					\
	TRACE_ASM_N(scr, fac, tag, name, 12, trace_2)

#define	TRACE_ASM_3(scr, fac, tag, name, d1, d2, d3) \
	mov d1, %o1; mov d2, %o2; mov d3, %o3;				\
	TRACE_ASM_N(scr, fac, tag, name, 16, trace_3)

#define	TRACE_ASM_4(scr, fac, tag, name, d1, d2, d3, d4) \
	mov d1, %o1; mov d2, %o2; mov d3, %o3; mov d4, %o4;		\
	TRACE_ASM_N(scr, fac, tag, name, 20, trace_4)

#define	TRACE_ASM_5(scr, fac, tag, name, d1, d2, d3, d4, d5) \
	mov d1, %o1; mov d2, %o2; mov d3, %o3; mov d4, %o4; mov d5, %o5; \
	TRACE_ASM_N(scr, fac, tag, name, 24, trace_5)

#if defined(ITRACE)

#define	ITRACE_ASM_0(scr, fac, tag, name) TRACE_ASM_0(scr, fac, tag, name)
#define	ITRACE_ASM_1(scr, fac, tag, name, d1) TRACE_ASM_1(scr, fac, tag, name,\
    d1)
#define	ITRACE_ASM_2(scr, fac, tag, name, d1, d2) TRACE_ASM_2(scr, fac, tag,\
    name, d1, d2)
#define	ITRACE_ASM_3(scr, fac, tag, name, d1, d2, d3) TRACE_ASM_3(scr, fac,\
    tag, name, d1, d2, d3)
#define	ITRACE_ASM_4(scr, fac, tag, name, d1, d2, d3, d4) TRACE_ASM_4(scr,\
    fac, tag, name, d1, d2, d3, d4)
#define	ITRACE_ASM_5(scr, fac, tag, name, d1, d2, d3, d4, d5) TRACE_ASM_5(scr,\
    fac, tag, name, d1, d2, d3, d4, d5)

#else /* if defined(UTRACE) */

#define	ITRACE_ASM_0(scr, fac, tag, name)
#define	ITRACE_ASM_1(scr, fac, tag, name, d1)
#define	ITRACE_ASM_2(scr, fac, tag, name, d1, d2)
#define	ITRACE_ASM_3(scr, fac, tag, name, d1, d2, d3)
#define	ITRACE_ASM_4(scr, fac, tag, name, d1, d2, d3, d4)
#define	ITRACE_ASM_5(scr, fac, tag, name, d1, d2, d3, d4, d5)

#endif

#endif /* _ASM */

#else /* if !defined(ITRACE) && !defined(UTRACE) */

#ifndef _ASM

#define	TRACE_N(fac, tag, name, len, func)
#define	ITRACE_0(fac, tag, name)
#define	ITRACE_1(fac, tag, name, d1)
#define	ITRACE_2(fac, tag, name, d1, d2)
#define	ITRACE_3(fac, tag, name, d1, d2, d3)
#define	ITRACE_4(fac, tag, name, d1, d2, d3, d4)
#define	ITRACE_5(fac, tag, name, d1, d2, d3, d4, d5)

#else /* _ASM */

#define	TRACE_ASM_0(scr, fac, tag, name)
#define	TRACE_ASM_1(scr, fac, tag, name, d1)
#define	TRACE_ASM_2(scr, fac, tag, name, d1, d2)
#define	TRACE_ASM_3(scr, fac, tag, name, d1, d2, d3)
#define	TRACE_ASM_4(scr, fac, tag, name, d1, d2, d3, d4)
#define	TRACE_ASM_5(scr, fac, tag, name, d1, d2, d3, d4, d5)

#define	ITRACE_ASM_0(scr, fac, tag, name)
#define	ITRACE_ASM_1(scr, fac, tag, name, d1)
#define	ITRACE_ASM_2(scr, fac, tag, name, d1, d2)
#define	ITRACE_ASM_3(scr, fac, tag, name, d1, d2, d3)
#define	ITRACE_ASM_4(scr, fac, tag, name, d1, d2, d3, d4)
#define	ITRACE_ASM_5(scr, fac, tag, name, d1, d2, d3, d4, d5)


#endif /* _ASM */

#endif /* ITRACE || UTRACE */

/*
 * Facility definitions
 */

#define	UTR_FAC_TRACE		128	/* user trace admin records */
#define	UTR_FAC_THREAD		129	/* thread interface except synch */
#define	UTR_FAC_THREAD_SYNC	130	/* thread synchronization interface */
#define	UTR_FAC_TLIB_SWTCH	131	/* swtch/resume */
#define	UTR_FAC_TLIB_DISP	132	/* disp/setrq/t_release/t_block */
#define	UTR_FAC_TLIB_MISC	140	/* misc internal libthread routines */

/*
 * UTR_FAC_TRACE tags
 */
#define	UTR_THR_LWP_MAP		0	/* maps thread id to lwpid at thread */
					/* switch and thread start time. */
					/* Needed for correct merging of */
					/* user/kernel trace files */
/*
 * UTR_FAC_THREAD tags
 */

#define		UTR_THR_CREATE_START	0
#define		UTR_THR_CREATE_END	1 /* end of thread_create()  */
#define		UTR_THR_CREATE_END2	2 /* start of created thread */
#define		UTR_THR_EXIT_START	3
#define		UTR_THR_EXIT_END	4
#define		UTR_THR_JOIN_START	5
#define		UTR_THR_JOIN_END	6
#define		UTR_THR_SELF_START	7
#define		UTR_THR_SELF_END	8


#define		UTR_THR_SUSPEND_START	9
#define		UTR_THR_SUSPEND_END	10
#define		UTR_THR_CONTINUE_START	11
#define		UTR_THR_CONTINUE_END	12
#define		UTR_THR_SETPRIO_START	13
#define		UTR_THR_SETPRIO_END	14
#define		UTR_THR_GETPRIO_START	15
#define		UTR_THR_GETPRIO_END	16
#define		UTR_THR_YIELD_START	17
#define		UTR_THR_YIELD_END	18
#define		UTR_THR_SETCONC_START	19
#define		UTR_THR_SETCONC_END	20
#define		UTR_THR_GETCONC_START	21
#define		UTR_THR_GETCONC_END	22

#define		UTR_THR_KILL_START	23
#define		UTR_THR_KILL_END	24
#define		UTR_THR_SIGSETMASK_START	25
#define		UTR_THR_SIGSETMASK_END	26

#define		UTR_THR_GETSPECIFIC_START	27
#define		UTR_THR_GETSPECIFIC_END	28
#define		UTR_THR_KEYCREATE_START	29
#define		UTR_THR_KEYCREATE_END	30
#define		UTR_THR_SETSPECIFIC_START	31
#define		UTR_THR_SETSPECIFIC_END	32


/*
 * UTR_FAC_THREAD_SYNC tags
 */

#define		UTR_MUTEX_INIT_START		0
#define		UTR_MUTEX_INIT_END		1
#define		UTR_MUTEX_LOCK_START		2
#define		UTR_MUTEX_LOCK_END		3
#define		UTR_CS_START			4
#define		UTR_CS_END			5
#define		UTR_MUTEX_UNLOCK_START		6
#define		UTR_MUTEX_UNLOCK_END		7
#define		UTR_MUTEX_TRYLOCK_START		8
#define		UTR_MUTEX_TRYLOCK_END		9

#define		UTR_COND_INIT_START		10
#define		UTR_COND_INIT_END		11
#define		UTR_COND_WAIT_START		12
#define		UTR_COND_WAIT_END		13
#define		UTR_COND_TIMEDWAIT_START	14
#define		UTR_COND_TIMEDWAIT_END		15
#define		UTR_COND_SIGNAL_START		16
#define		UTR_COND_SIGNAL_END		17
#define		UTR_COND_BCST_START		18
#define		UTR_COND_BCST_END		19

#define		UTR_SEMA_INIT_START		20
#define		UTR_SEMA_INIT_END		21
#define		UTR_SEMA_WAIT_START		22
#define		UTR_SEMA_WAIT_END		23
#define		UTR_SEMA_POST_START		24
#define		UTR_SEMA_POST_END		25
#define		UTR_SEMA_TRYWAIT_START		26
#define		UTR_SEMA_TRYWAIT_END		27

#define		UTR_RWLOCK_INIT_START		28
#define		UTR_RWLOCK_INIT_END		29
#define		UTR_RW_RDLOCK_START		30
#define		UTR_RW_RDLOCK_END		31
#define		UTR_RW_UNLOCK_START		32
#define		UTR_RW_UNLOCK_END		33
#define		UTR_RW_WRLOCK_START		34
#define		UTR_RW_WRLOCK_END		35

#define		UTR_RW_TRYRDLOCK_START		36
#define		UTR_RW_TRYRDLOCK_END		37
#define		UTR_RW_TRYWRLOCK_START		38
#define		UTR_RW_TRYWRLOCK_END		39

/*
 * UTR_FAC_TLIB_* tags.
 */

/* tags for UTR_FAC_TLIB_SWTCH */

#define		UTR_SWTCH_START	0
#define		UTR_SWTCH_END		1
#define		UTR_RESUME_START	2
#define		UTR_RESUME_FLUSH	3
#define		UTR_RESUME_END		4
#define		UTR_PARK_START		5
#define		UTR_PARK_END		6
#define		UTR_UNPARK_START	7
#define		UTR_UNPARK_END		8

/* tags for UTR_FAC_TLIB_DISP */

#define		UTR_DISP_START		0
#define		UTR_DISP_END		1
#define		UTR_T_RELEASE_START	2
#define		UTR_T_RELEASE_END	3
#define		UTR_SETRQ_START	4
#define		UTR_SETRQ_END		5

/* tags for UTR_FAC_TLIB_MISC */

#define		UTR_IDLE_CREATE_START	0
#define		UTR_IDLE_CREATE_END	1
#define		UTR_REAPER_START	2
#define		UTR_REAPER_END		3

#define		UTR_SC_LK_ENTER_START	4
#define		UTR_SC_LK_ENTER_END	5
#define		UTR_SC_LK_CS_START	6
#define		UTR_SC_LK_CS_END	7
#define		UTR_SC_LK_EXIT_START	8
#define		UTR_SC_LK_EXIT_END	9

#define		UTR_THC_STK		27
#define		UTR_THC_CONT1		28
#define		UTR_THC_CONT2		29
#define		UTR_THC_ALLOCSTK	30
#define		UTR_THC_MEMSET1		31
#define		UTR_THC_MEMSET2		32
#define		UTR_THC_BLWPC		33
#define		UTR_THC_ALWPC		34

#define		UTR_THCONT_BLWPC	35
#define		UTR_THCONT_ALWPC	36

#define		UTR_CACHE_MISS		37
#define		UTR_CACHE_HIT		38

#define		UTR_LWP_EXEC_START	39
#define		UTR_LWP_EXEC_END	40
#define		UTR_ALWP_MEMSET		41
#define		UTR_ALLOC_TLS_END	42

/* Temp tag for benchmarking trace */
#define		UTR_BM			18

#ifdef	__cplusplus
}
#endif

#endif	/* _UTRACE_H */
