/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)common_asm.s	1.7	99/09/13 SMI"

/*
 * General assembly language routines.
 * It is the intent of this file to contain routines that are
 * specific to cpu architecture.
 */

/*
 * Override GET_NATIVE_TIME for the cpu module code.  Must expand to
 * exactly one machine instruction as this macro may be used in delay
 * slots.
 */
#if defined(CHEETAH) && !defined(CHEETAH_BRINGUP) /* XXX */
#define	GET_NATIVE_TIME(out)	\
	rd	STICK, out;
#else
#define	GET_NATIVE_TIME(out)	\
	rdpr	%tick, out;
#endif

#include <sys/clock.h>

#if defined(lint)
#include <sys/types.h>
#include <sys/scb.h>
#include <sys/systm.h>
#include <sys/regset.h>
#include <sys/sunddi.h>
#include <sys/lockstat.h>
#endif	/* lint */


#include <sys/atomic_prim.h>
#include <sys/asm_linkage.h>
#include <sys/privregs.h>
#include <sys/machparam.h>	/* To get SYSBASE and PAGESIZE */
#include <sys/machthread.h>
#include <sys/clock.h>
#include <sys/intreg.h>
#include <sys/psr_compat.h>
#include <sys/isa_defs.h>
#include <sys/dditypes.h>

#if !defined(lint)
#include "assym.h"
#endif

#if defined(lint)

uint_t
get_impl(void)
{ return (0); }

#else   /* lint */

	ENTRY(get_impl)
	GET_CPU_IMPL(%o0)
	retl
	nop
	SET_SIZE(get_impl)

#endif  /* lint */

/*
 * XXX time routines go here after fixing them to use tick/stick as
 * appropriate
 */

/*
 * This gethrtime_initial() is only specific to sun4u
 * It will give the correct hrtime in the first initial
 * call when hrtime_base is zero. This is used to initialize
 * hrtime_base in clkstart()
 */
#if defined(lint) || defined(__lint)
hrtime_t
gethrtime_initial(void)
{
	return ((hrtime_t)0);
}
#else /* lint */

	ENTRY_NP(gethrtime_initial)
	GET_HRTIME_INITIAL(%g1, %o0, %o1, %o2, %o3, %o4) ! %g1 = hrtime
#ifdef __sparcv9
	retl
	mov	%g1, %o0
#else
	srlx	%g1, 32, %o0				! %o0 = hi32(%g1)
	retl
	srl	%g1, 0, %o1				! %o1 = lo32(%g1)
#endif
	SET_SIZE(gethrtime_initial)
#endif /* lint */


/*
 * Get current tick
 */
#if defined(lint)

u_longlong_t
gettick(void)
{ return (0); }

#else   /* lint */

	ENTRY(gettick)
#ifdef __sparcv9
	retl
	GET_NATIVE_TIME(%o0)
#else
	GET_NATIVE_TIME(%o1)
	srlx    %o1, 32, %o0	! put the high 32 bits in low part of o0
	retl
	srl     %o1, 0, %o1	! put lower 32 bits in o1, clear upper 32 bits
#endif
	SET_SIZE(gettick)

#endif  /* lint */


/*
 * Provide a C callable interface to the trap that reads the hi-res timer.
 * Returns 64-bit nanosecond timestamp in %o0 and %o1.
 */

#if defined(lint)

hrtime_t
gethrtime(void)
{
	return ((hrtime_t)0);
}

hrtime_t
gethrtime_unscaled(void)
{
	return ((hrtime_t)0);
}

hrtime_t
gethrtime_max(void)
{
	return ((hrtime_t)0);
}

void
scalehrtime(hrtime_t *hrt)
{
	*hrt = 0;
}

void
gethrestime(timespec_t *tp)
{
	tp->tv_sec = 0;
	tp->tv_nsec = 0;
}

/*ARGSUSED*/
int
lockstat_event_start(uintptr_t lp, ls_pend_t *lpp)
{
	return (0);
}

/*ARGSUSED*/
hrtime_t
lockstat_event_end(ls_pend_t *lpp)
{
	return ((hrtime_t)0);
}

void
hres_tick(void)
{
}

#else	/* lint */

	ENTRY_NP(gethrtime)
	GET_HRTIME(%g1, %o0, %o1, %o2, %o3, %o4)	! %g1 = hrtime
#ifdef __sparcv9
	retl
	mov	%g1, %o0
#else
	srlx	%g1, 32, %o0				! %o0 = hi32(%g1)
	retl
	srl	%g1, 0, %o1				! %o1 = lo32(%g1)
#endif
	SET_SIZE(gethrtime)

	ENTRY_NP(gethrtime_unscaled)
	GET_NATIVE_TIME(%g1)				! %g1 = native time
#ifdef __sparcv9
	retl
	mov	%g1, %o0
#else
	srlx	%g1, 32, %o0				! %o0 = hi32(%g1)
	retl
	srl	%g1, 0, %o1				! %o1 = lo32(%g1)
#endif
	SET_SIZE(gethrtime_unscaled)

	ENTRY(gethrtime_max)
	NATIVE_TIME_MAX(%g1)
	NATIVE_TIME_TO_NSEC(%g1, %o0, %o1)
#ifdef __sparcv9
	retl
	mov	%g1, %o0
#else
	srlx	%g1, 32, %o0
	retl
	srl	%g1, 0, %o1
#endif

	ENTRY(scalehrtime)
	ldx	[%o0], %o1
	NATIVE_TIME_TO_NSEC(%o1, %o2, %o3)
	retl
	stx	%o1, [%o0]
	SET_SIZE(scalehrtime)

	!
	! %o0 = lp, %o1 = ls_pend_t
	!
	! returns zero on success, non-zero on failure
	!
	ENTRY_NP(lockstat_event_start)
	casn	[%o1], %g0, %o0			! lpp->lp_lock = lp (on success)
	brnz,pn	%o0, 1f				! if we didn't get it, return
	GET_NATIVE_TIME(%g1)
	retl
	stx	%g1, [%o1 + LP_START_TIME]	! lpp->lp_start_time = now
1:
	retl
	nop
	SET_SIZE(lockstat_event_start)

	!
	! %o0 = ls_pend_t
	!
	ENTRY_NP(lockstat_event_end)
	ldx	[%o0 + LP_START_TIME], %o5	! %o5 = starting native time
	sethi	%hi(nsec_scale), %o2
	GET_NATIVE_TIME(%g1)			! %g1 = current native time
	lduh	[%o2 + %lo(nsec_scale)], %o2	! for cheap 16-bit conversion
	sub	%g1, %o5, %g1			! %g1 = native time delta
	stn	%g0, [%o0 + LP_LOCK]		! clear lock
	mulx	%g1, %o2, %o3
#ifdef __sparcv9
	retl
	srlx	%o3, 16 - NSEC_SHIFT, %o0	! %o0 = nsec delta to ~100 ppm
#else
	srlx	%o3, 16 - NSEC_SHIFT, %g1	! %g1 = nsec delta to ~100 ppm
	srlx	%g1, 32, %o0			! %o0 = hi32(duration)
	retl
	srl	%g1, 0, %o1			! %o1 = lo32(duration)
#endif
	SET_SIZE(lockstat_event_end)

/*
 * Fast trap to return a timestamp, uses trap window, leaves traps
 * disabled.  Returns a 64-bit nanosecond timestamp in %o0 and %o1.
 *
 * This is the handler for the ST_GETHRTIME trap.
 */

	ENTRY_NP(get_timestamp)
	GET_HRTIME(%g1, %g2, %g3, %g4, %g5, %o0)	! %g1 = hrtime
	srlx	%g1, 32, %o0				! %o0 = hi32(%g1)
	srl	%g1, 0, %o1				! %o1 = lo32(%g1)
	done
	SET_SIZE(get_timestamp)

/*
 * Macro to convert GET_HRESTIME() bits into a timestamp.
 *
 * We use two separate macros so that the platform-dependent GET_HRESTIME()
 * can be as small as possible; CONV_HRESTIME() implements the generic part.
 */
#define	CONV_HRESTIME(hrestsec, hrestnsec, adj, nslt, nano) \
	brz,pt	adj, 3f;		/* no adjustments, it's easy */	\
	add	hrestnsec, nslt, hrestnsec; /* hrest.tv_nsec += nslt */	\
	brlz,pn	adj, 2f;		/* if hrestime_adj negative */	\
	srl	nslt, ADJ_SHIFT, nslt;	/* delay: nslt >>= 4 */		\
	subcc	adj, nslt, %g0;		/* hrestime_adj - nslt/16 */	\
	movg	%xcc, nslt, adj;	/* adj by min(adj, nslt/16) */	\
	ba	3f;			/* go convert to sec/nsec */	\
	add	hrestnsec, adj, hrestnsec; /* delay: apply adjustment */ \
2:	addcc	adj, nslt, %g0;		/* hrestime_adj + nslt/16 */	\
	bge,a,pt %xcc, 3f;		/* is adj less negative? */	\
	add	hrestnsec, adj, hrestnsec; /* yes: hrest.nsec += adj */	\
	sub	hrestnsec, nslt, hrestnsec; /* no: hrest.nsec -= nslt/16 */ \
3:	cmp	hrestnsec, nano;	/* more than a billion? */	\
	bl,pt	%xcc, 4f;		/* if not, we're done */	\
	nop;				/* delay: do nothing :( */	\
	add	hrestsec, 1, hrestsec;	/* hrest.tv_sec++; */		\
	sub	hrestnsec, nano, hrestnsec; /* hrest.tv_nsec -= NANOSEC; */	\
4:

	ENTRY_NP(gethrestime)
	GET_HRESTIME(%o1, %o2, %o3, %o4, %o5, %g1, %g2)
	CONV_HRESTIME(%o1, %o2, %o3, %o4, %o5)
	stn	%o1, [%o0]
	retl
	stn	%o2, [%o0 + CLONGSIZE]
	SET_SIZE(gethrestime)

/*
 * Fast trap for gettimeofday().  Returns a timestruc_t in %o0 and %o1.
 *
 * This is the handler for the ST_GETHRESTIME trap.
 */

	ENTRY_NP(get_hrestime)
	GET_HRESTIME(%o0, %o1, %g1, %g2, %g3, %g4, %g5)
	CONV_HRESTIME(%o0, %o1, %g1, %g2, %g3)
	done
	SET_SIZE(get_hrestime)

/*
 * Fast trap to return lwp virtual time, uses trap window, leaves traps
 * disabled.  Returns a 64-bit number in %o0:%o1, which is the number
 * of nanoseconds consumed.
 *
 * This is the handler for the ST_GETHRVTIME trap.
 *
 * Register usage:
 *	%o0, %o1 = return lwp virtual time
 * 	%o2 = CPU/thread
 * 	%o3 = lwp
 * 	%g1 = scratch
 * 	%g5 = scratch
 */
	ENTRY_NP(get_virtime)
	GET_HRTIME(%g5, %g1, %g2, %g3, %g4, %o0)	! %g5 = hrtime
	CPU_ADDR(%g2, %g3)			! CPU struct ptr to %g2
	ldn	[%g2 + CPU_THREAD], %g2		! thread pointer to %g2
	lduh	[%g2 + T_PROC_FLAG], %g3
	btst	TP_MSACCT, %g3			! test for MSACCT on
	bz	1f				! not on - do estimate
	ldn	[%g2 + T_LWP], %g3		! lwp pointer to %g3

	/*
	 * Subtract start time of current microstate from time
	 * of day to get increment for lwp virtual time.
	 */
	ldx	[%g3 + LWP_STATE_START], %g1	! ms_state_start
	sub	%g5, %g1, %g5

	/*
	 * Add current value of ms_acct[LMS_USER]
	 */
	ldx	[%g3 + LWP_ACCT_USER], %g1	! ms_acct[LMS_USER]
	add	%g5, %g1, %g5

	srl	%g5, 0, %o1			! %o1 = lo32(%g5)
	srlx	%g5, 32, %o0			! %o0 = hi32(%g5)

	done

	/*
	 * Microstate accounting times are not available.
	 * Estimate based on tick samples.
	 * Convert from ticks (100 Hz) to nanoseconds (mult by nsec_per_tick).
	 */
1:
	ldn	[%g3 + LWP_UTIME], %g4		! utime = lwp->lwp_utime;
	sethi	%hi(nsec_per_tick), %g1
	ld	[%g1 + %lo(nsec_per_tick)], %g1	! multiplier
	mulx	%g4, %g1, %g1

	/*
	 * Sanity-check estimate.
	 * If elapsed real time for process is smaller, use that instead.
	 */
	ldx	[%g3 + LWP_MS_START], %g4	! starting real time for LWP
	sub	%g5, %g4, %g5			! subtract start time
	cmp	%g1, %g5			! compare to estimate
	movleu	%xcc, %g1, %g5			! use estimate

	srl	%g5, 0, %o1			! %o1 = lo32(%g5)
	srlx	%g5, 32, %o0			! %o0 = hi32(%g5)

	done
	SET_SIZE(get_virtime)



	.seg	".text"
hrtime_base_panic:
	.asciz	"hrtime_base stepping back"
	.align	4


	ENTRY_NP(hres_tick)
	save	%sp, -SA(MINFRAME), %sp	! get a new window

	sethi	%hi(hrestime), %l4
	ldstub	[%l4 + %lo(hres_lock + HRES_LOCK_OFFSET)], %l5	! try locking
7:	tst	%l5
	bz,pt	%xcc, 8f			! if we got it, drive on
	ld	[%l4 + %lo(nsec_scale)], %l5	! delay: %l5 = scaling factor
	ldub	[%l4 + %lo(hres_lock + HRES_LOCK_OFFSET)], %l5
9:	tst	%l5
	bz,a,pn	%xcc, 7b
	ldstub	[%l4 + %lo(hres_lock + HRES_LOCK_OFFSET)], %l5
	ba,pt	%xcc, 9b
	ldub	[%l4 + %lo(hres_lock + HRES_LOCK_OFFSET)], %l5
8:
	membar	#StoreLoad|#StoreStore

	!
	! update hres_last_tick.  %l5 has the scaling factor (nsec_scale).
	!
	ldx	[%l4 + %lo(hrtime_base)], %g1	! load current hrtime_base
	GET_NATIVE_TIME(%l0)			! current native time
	stx	%l0, [%l4 + %lo(hres_last_tick)]! prev = current
	NATIVE_TIME_TO_NSEC_SCALE(%l0, %l5, %l2) ! convert native time to nsecs

	sub	%l0, %g1, %i1			! get accurate nsec delta

	ldx	[%l4 + %lo(hrtime_base)], %l1	
	cmp	%l1, %l0
	bg,pn	%xcc, 9f
	nop

	stx	%l0, [%l4 + %lo(hrtime_base)]	! update hrtime_base

	!
	! apply adjustment, if any
	!
	ldx	[%l4 + %lo(hrestime_adj)], %l0	! %l0 = hrestime_adj
	brz	%l0, 2f
						! hrestime_adj == 0 ?
						! yes, skip adjustments
	clr	%l5				! delay: set adj to zero
	tst	%l0				! is hrestime_adj >= 0 ?
	bge,pt	%xcc, 1f			! yes, go handle positive case
	srl	%i1, ADJ_SHIFT, %l5		! delay: %l5 = adj

	addcc	%l0, %l5, %g0			! hrestime_adj < -adj ?
	bl,pt	%xcc, 2f			! yes, use current adj
	neg	%l5				! delay: %l5 = -adj
	ba,pt	%xcc, 2f
	mov	%l0, %l5			! no, so set adj = hrestime_adj
1:
	subcc	%l0, %l5, %g0			! hrestime_adj < adj ?
	bl,a,pt	%xcc, 2f			! yes, set adj = hrestime_adj
	mov	%l0, %l5			! delay: adj = hrestime_adj
2:
	ldx	[%l4 + %lo(timedelta)], %l0	! %l0 = timedelta
	sub	%l0, %l5, %l0			! timedelta -= adj

	stx	%l0, [%l4 + %lo(timedelta)]	! store new timedelta
	stx	%l0, [%l4 + %lo(hrestime_adj)]	! hrestime_adj = timedelta

	or	%l4, %lo(hrestime), %l2
	ldn	[%l2], %i2			! %i2:%i3 = hrestime sec:nsec
	ldn	[%l2 + CLONGSIZE], %i3
	add	%i3, %l5, %i3			! hrestime.nsec += adj
	add	%i3, %i1, %i3			! hrestime.nsec += nslt

	set	NANOSEC, %l5			! %l5 = NANOSEC
	cmp	%i3, %l5
	bl,pt	%xcc, 5f			! if hrestime.tv_nsec < NANOSEC
	sethi	%hi(one_sec), %i1		! delay
	add	%i2, 0x1, %i2			! hrestime.tv_sec++
	sub	%i3, %l5, %i3			! hrestime.tv_nsec - NANOSEC
	mov	0x1, %l5
	st	%l5, [%i1 + %lo(one_sec)]
5:
	stn	%i2, [%l2]
	stn	%i3, [%l2 + CLONGSIZE]		! store the new hrestime

	membar	#StoreStore

	ld	[%l4 + %lo(hres_lock)], %i1
	inc	%i1				! release lock
	st	%i1, [%l4 + %lo(hres_lock)]	! clear hres_lock

	ret
	restore

9:
	sethi	%hi(hrtime_base_panic), %o0
	call	panic
	or	%o0, %lo(hrtime_base_panic), %o0

	SET_SIZE(hres_tick)

#endif	/* lint */
#if !defined(lint) && !defined(__lint)

	.seg	".data"
kstat_q_panic_msg:
	.asciz	"kstat_q_exit: qlen == 0"

	ENTRY(kstat_q_panic)
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(kstat_q_panic_msg), %o0
	call	panic
	or	%o0, %lo(kstat_q_panic_msg), %o0
	/*NOTREACHED*/
	SET_SIZE(kstat_q_panic)

#define	BRZPN	brz,pn
#define	BRZPT	brz,pt

#define	KSTAT_Q_UPDATE(QOP, QBR, QZERO, QRETURN, QTYPE) \
	ld	[%o0 + QTYPE/**/CNT], %o1;	/* %o1 = old qlen */	\
	QOP	%o1, 1, %o2;			/* %o2 = new qlen */	\
	QBR	%o1, QZERO;			/* done if qlen == 0 */	\
	st	%o2, [%o0 + QTYPE/**/CNT];	/* delay: save qlen */	\
	ldx	[%o0 + QTYPE/**/LASTUPDATE], %o3;			\
	ldx	[%o0 + QTYPE/**/TIME], %o4;	/* %o4 = old time */	\
	ldx	[%o0 + QTYPE/**/LENTIME], %o5;	/* %o5 = old lentime */	\
	sub	%g1, %o3, %o2;			/* %o2 = time delta */	\
	mulx	%o1, %o2, %o3;			/* %o3 = cur lentime */	\
	add	%o4, %o2, %o4;			/* %o4 = new time */	\
	add	%o5, %o3, %o5;			/* %o5 = new lentime */	\
	stx	%o4, [%o0 + QTYPE/**/TIME];	/* save time */		\
	stx	%o5, [%o0 + QTYPE/**/LENTIME];	/* save lentime */	\
QRETURN;								\
	stx	%g1, [%o0 + QTYPE/**/LASTUPDATE]; /* lastupdate = now */

	.align 16
	ENTRY(kstat_waitq_enter)
	GET_NATIVE_TIME(%g1)
	KSTAT_Q_UPDATE(add, BRZPT, 1f, 1:retl, KSTAT_IO_W)
	SET_SIZE(kstat_waitq_enter)

	.align 16
	ENTRY(kstat_waitq_exit)
	GET_NATIVE_TIME(%g1)
	KSTAT_Q_UPDATE(sub, BRZPN, kstat_q_panic, retl, KSTAT_IO_W)
	SET_SIZE(kstat_waitq_exit)

	.align 16
	ENTRY(kstat_runq_enter)
	GET_NATIVE_TIME(%g1)
	KSTAT_Q_UPDATE(add, BRZPT, 1f, 1:retl, KSTAT_IO_R)
	SET_SIZE(kstat_runq_enter)

	.align 16
	ENTRY(kstat_runq_exit)
	GET_NATIVE_TIME(%g1)
	KSTAT_Q_UPDATE(sub, BRZPN, kstat_q_panic, retl, KSTAT_IO_R)
	SET_SIZE(kstat_runq_exit)

	.align 16
	ENTRY(kstat_waitq_to_runq)
	GET_NATIVE_TIME(%g1)
	KSTAT_Q_UPDATE(sub, BRZPN, kstat_q_panic, 1:, KSTAT_IO_W)
	KSTAT_Q_UPDATE(add, BRZPT, 1f, 1:retl, KSTAT_IO_R)
	SET_SIZE(kstat_waitq_to_runq)

	.align 16
	ENTRY(kstat_runq_back_to_waitq)
	GET_NATIVE_TIME(%g1)
	KSTAT_Q_UPDATE(sub, BRZPN, kstat_q_panic, 1:, KSTAT_IO_R)
	KSTAT_Q_UPDATE(add, BRZPT, 1f, 1:retl, KSTAT_IO_W)
	SET_SIZE(kstat_runq_back_to_waitq)

#endif	/* lint */

#ifdef lint	

int64_t timedelta;
hrtime_t hres_last_tick;
timestruc_t hrestime;
int64_t hrestime_adj;
int hres_lock;
uint_t nsec_scale;
hrtime_t hrtime_base;
int use_stick;

#else
	/*
	 *  -- WARNING --
	 *
	 * The following variables MUST be together on a 128-byte boundary.
	 * In addition to the primary performance motivation (having them all
	 * on the same cache line(s)), code here and in the GET*TIME() macros
	 * assumes that they all have the same high 22 address bits (so
	 * there's only one sethi).
	 */
	.seg	".data"
	.global	timedelta, hres_last_tick, hrestime, hrestime_adj
	.global	hres_lock, nsec_scale, hrtime_base, use_stick

	/* XXX - above comment claims 128-bytes is necessary */
	.align	64
timedelta:
	.word	0, 0		/* int64_t */
hres_last_tick:
	.word	0, 0		/* hrtime_t */
hrestime:
	.nword	0, 0		/* 2 longs */
hrestime_adj:
	.word	0, 0		/* int64_t */
hres_lock:
	.word	0
nsec_scale:
	.word	0
hrtime_base:
	.word	0, 0
use_stick:
	.word	0

#endif
