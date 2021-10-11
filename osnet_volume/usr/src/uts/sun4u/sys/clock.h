/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CLOCK_H
#define	_SYS_CLOCK_H

#pragma ident	"@(#)clock.h	1.42	99/10/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/spl.h>
#include <sys/time.h>

#ifndef _ASM

#ifdef	_KERNEL

extern int	Cpudelay;
extern void	setcpudelay(void);

extern uint_t	nsec_scale;
extern uint_t	nsec_per_cpu_tick;
extern uint_t	cpu_tick_freq;

extern int	use_stick;
extern uint_t	system_clock_freq;

extern void mon_clock_init(void);
extern void mon_clock_start(void);
extern void mon_clock_stop(void);
extern void mon_clock_share(void);
extern void mon_clock_unshare(void);

extern hrtime_t	gethrtime_initial(void);
extern hrtime_t hrtime_base;
extern hrtime_t hres_last_tick;
extern void hres_tick();
extern void	clkstart(void);
extern void cbe_level14();

typedef struct {
	uint32_t cbe_level1_inum;
	uint32_t cbe_level10_inum;
} cbe_data_t;

#define	CLK_WATCHDOG_DEFAULT	10	/* 10 seconds */

extern int	watchdog_enable;
extern int	watchdog_available;
extern int	watchdog_activated;
extern uint_t	watchdog_timeout_seconds;

/*
 * tod module name and operations
 */
struct tod_ops {
	timestruc_t	(*tod_get)(void);
	void		(*tod_set)(timestruc_t);
	uint_t		(*tod_set_watchdog_timer)(uint_t);
	uint_t		(*tod_clear_watchdog_timer)(void);
	void		(*tod_set_power_alarm)(timestruc_t);
	void		(*tod_clear_power_alarm)(void);
	uint_t		(*tod_get_cpufrequency)(void);
};

extern struct tod_ops	tod_ops;
extern char		*tod_module_name;

#endif	/* _KERNEL */

#endif	/* _ASM */

#define	CBE_LOW_PIL	1
#define	CBE_LOCK_PIL	LOCK_LEVEL
#define	CBE_HIGH_PIL	14

#define	ADJ_SHIFT	4	/* used in get_hrestime and _level10 */
#define	NSEC_SHIFT	4
#define	VTRACE_SHIFT	4

/*
 * Get the current high-resolution native (unscaled) time.  Uses
 * V9's %tick for UltraSparc I & II, and uses %stick thereafter.
 */
#if !defined(GET_NATIVE_TIME) /* overridden in cpu module code */
#define	GET_NATIVE_TIME(out)				\
	sethi	%hi(use_stick), out;			\
	lduw	[out + %lo(use_stick)], out;		\
	/* CSTYLED */					\
        brz,a	out, .+12;				\
	rdpr	%tick, out;				\
	rd	%asr24, out;
#endif

/*
 * Get the implementation from the version register.
 * This is used to determine if processor is UltraSPARC III
 */
#define	GET_CPU_IMPL(out)		\
	rdpr	%ver,	out;		\
	srlx	out, 32, out;		\
	sll	out, 16, out;		\
	srl	out, 16, out;

#define	CHEETAH_IMPL	0x14

/*
 * Convert hi-res native time (V9's %tick in our case) into nanoseconds.
 *
 * The challenge is to multiply a %tick value by (NANOSEC / cpu_tick_freq)
 * without using floating point and without overflowing 64-bit integers.
 * We assume that all sun4u systems will have a 16 nsec or better clock
 * (i.e. faster than 62.5 MHz), which means that (ticks << 4) has units
 * greater than one nanosecond, so converting from (ticks << 4) to nsec
 * requires multiplication by a rational number, R, between 0 and 1.
 * To avoid floating-point we precompute (R * 2^32) during boot and
 * stash this away in nsec_scale.  Thus we can compute (tick * R) as
 * (tick * nsec_scale) >> 32, which is accurate to about 1 part per billion.
 *
 * To avoid 64-bit overflow when multiplying (tick << 4) by nsec_scale,
 * we split (tick << 4) into its high and low 32-bit pieces, H and L,
 * multiply each piece separately, and add up the relevant bits of the
 * partial products.  Putting it all together we have:
 *
 * nsec = (tick << 4) * R
 *	= ((tick << 4) * nsec_scale) >> 32
 *	= ((H << 32) + L) * nsec_scale) >> 32
 *	= (H * nsec_scale) + ((L * nsec_scale) >> 32)
 *
 * The last line is the computation we actually perform: it requires no
 * floating point and all intermediate results fit in 64-bit registers.
 *
 * Note that we require that tick is less than (1 << (64 - NSEC_SHIFT));
 * greater values will result in overflow and misbehavior (not that this
 * is a serious problem; (1 << (64 - NSEC_SHIFT)) nanoseconds is over
 * thirty-six years).  Nonetheless, clients may wish to be aware of this
 * limitation; NATIVE_TIME_MAX() returns this maximum native time.
 *
 * We provide three versions of this macro: a "full-service" version that
 * just converts ticks to nanoseconds, a slightly higher-performance
 * version that expects the scaling factor nsec_scale as its second argument
 * (so that callers can distance the load of nsec_scale from its use),
 * and a still faster version that can be used when the number of ticks
 * is known to be less than 32 bits (e.g. the delta between %tick values
 * for successive clock interrupts).
 *
 * Note that in the 32-bit version we don't even bother clearing NPT.
 * We get away with this by making hardclk.c ensure than nsec_scale
 * is even, so we can take advantage of the associativity of modular
 * arithmetic: multiplying %tick by any even number, say 2*n, is
 * equivalent to multiplying %tick by 2, then by n.  Multiplication
 * by 2 is equivalent to shifting left by one, which clears NPT.
 */
#define	NATIVE_TIME_TO_NSEC_SCALE_32(out, scr1)				\
	mulx	out, scr1, out;						\
	srlx	out, 32 - NSEC_SHIFT, out;

#define	NATIVE_TIME_TO_NSEC_SCALE(out, scr1, scr2)			\
	sllx	out, NSEC_SHIFT, out;	/* clear NPT and pre-scale */	\
	srlx	out, 32, scr2;		/* scr2 = hi32(tick<<4) = H */	\
	mulx	scr2, scr1, scr2;	/* scr2 = (H*F) */		\
	srl	out, 0, out;		/* out = lo32(tick<<4) = L */	\
	mulx	out, scr1, scr1;	/* scr1 = (L*F) */		\
	srlx	scr1, 32, scr1;		/* scr1 = (L*F) >> 32 */	\
	add	scr1, scr2, out;	/* out = (H*F) + ((L*F) >> 32) */

#define	NATIVE_TIME_TO_NSEC(out, scr1, scr2)				\
	sethi	%hi(nsec_scale), scr1;	/* load scaling factor */	\
	ld	[scr1 + %lo(nsec_scale)], scr1;				\
	NATIVE_TIME_TO_NSEC_SCALE(out, scr1, scr2);

#define	NATIVE_TIME_MAX(out)						\
	mov	-1, out;						\
	srlx	out, NSEC_SHIFT, out

/*
 * Locking strategy for high-resolution timing services
 *
 * We generally construct timestamps from two or more components:
 * a hardware time source and one or more software time sources.
 * These components cannot all be loaded simultaneously, so we need
 * some sort of locking strategy to generate consistent timestamps.
 *
 * To minimize lock contention and cache thrashing we employ the
 * weakest possible synchronization model: writers (rare) serialize
 * on an acquisition-counting mutex, described below; readers (common)
 * execute in parallel with no synchronization at all -- they don't
 * exclude other readers, and they don't even exclude writers.  Instead,
 * readers just examine the writer lock's value before and after loading
 * all the components of a timestamp to detect writer intervention.
 * In the rare case when a writer does intervene, the reader will
 * detect it, discard the timestamp and try again.
 *
 * The writer lock, hres_lock, is a 32-bit integer consisting of an
 * 8-bit lock and a 24-bit acquisition count.  To acquire the lock we
 * set the lock field with ldstub, which sets the low-order 8 bits to
 * 0xff; to clear the lock, we increment it, which simultaneously clears
 * the lock field (0xff --> 0x00) and increments the acquisition count
 * (due to carry into bit 8).  Thus each acquisition transforms hres_lock
 * from N:0 to N:ff, and each release transforms N:ff into (N+1):0.
 *
 * Readers can detect writer intervention by loading hres_lock before
 * and after loading the time components they need; if either lock value
 * contains 0xff in the low-order bits (lock held), or if the lock values
 * are not equal (lock was acquired and released), a writer intervened
 * and the reader must try again.  If the lock values are equal and the
 * low-order 8 bits are clear, the timestamp must be valid.  We can check
 * both of these conditions with a single compare instruction by checking
 * whether old_hres_lock & ~1 == new_hres_lock, as illustrated by the
 * following table of all possible lock states:
 *
 *	initial	& ~1	final		result of compare
 *	------------	-----		-----------------
 *	now:00		now:00		valid
 *	now:00		now:ff		invalid
 *	now:00		later:00	invalid
 *	now:00		later:ff	invalid
 *	now:fe		now:ff		invalid
 *	now:fe		later:00	invalid
 *	now:fe		later:ff	invalid
 *
 * Implementation considerations:
 *
 * (1) Load buffering.
 *
 * On a CPU that does load buffering we must ensure that the load of
 * hres_lock completes before the load of any timestamp components.
 * This is essential *even on a CPU that does in-order loads* because
 * accessing the hardware time source may not involve a memory reference
 * (e.g. rd %tick).  A convenient way to address this is to clear the
 * lower bit (andn with 1) of the old lock value right away, since this
 * generates a dependency on the load of hres_lock.  We have to do this
 * anyway to perform the lock comparison described above.
 *
 * (2) Out-of-order loads.
 *
 * On a CPU that does out-of-order loads we must ensure that the loads
 * of all timestamp components have completed before we load the final
 * value of hres_lock.  This can be done either by generating load
 * dependencies on the timestamp components or by membar #LoadLoad.
 *
 * (3) Interaction with the high level cyclic handler, hres_tick().
 *
 * One unusual property of hres_lock is that it's acquired in a high
 * level cyclic handler, hres_tick().  Thus, hres_lock must be acquired at
 * CBE_HIGH_PIL or higher to prevent single-CPU deadlock.
 *
 * (4) Cross-calls.
 *
 * If a cross-call happens while one CPU has hres_lock and another is
 * trying to acquire it in the clock interrupt path, the system will
 * deadlock: the first CPU will never release hres_lock since it's
 * waiting to be released from the cross-call, and the cross-call can't
 * complete because the second CPU is spinning on hres_lock with traps
 * disabled.  Thus cross-calls must be blocked while holding hres_lock.
 *
 * Together, (3) and (4) imply that hres_lock should only be acquired
 * at PIL >= max(XCALL_PIL, CBE_HIGH_PIL), or while traps are disabled.
 */
#define	HRES_LOCK_OFFSET 3

#define	CLOCK_LOCK(oldsplp)	\
	lock_set_spl((lock_t *)&hres_lock + HRES_LOCK_OFFSET, \
		ipltospl(CBE_HIGH_PIL), oldsplp)

#define	CLOCK_UNLOCK(spl)	\
	membar_ldst_stst();	\
	hres_lock++;		\
	splx(spl);		\
	LOCKSTAT_EXIT(LS_SPIN_LOCK_HOLD,	\
		(lock_t *)&hres_lock + HRES_LOCK_OFFSET, curthread, 1);

/*
 * NOTE: the macros below assume that the various time-related variables
 * (hrestime, hrestime_adj, hres_last_tick, timedelta, nsec_scale, etc)
 * are all stored together on a 64-byte boundary.  The primary motivation
 * is cache performance, but we also take advantage of a convenient side
 * effect: these variables all have the same high 22 address bits, so only
 * one sethi is needed to access them all.
 */

/*
 * GET_HRESTIME() returns the value of hrestime, hrestime_adj and the
 * number of nanoseconds since the last clock tick ('nslt').  It also
 * sets 'nano' to the value NANOSEC (one billion).
 *
 * This macro assumes that it's safe to use label "5:", and that all
 * registers are globals or outs so they can safely contain 64-bit data.
 */
#define	GET_HRESTIME(hrestsec, hrestnsec, adj, nslt, nano, scr, hrlock)	\
5:	sethi	%hi(hres_lock), scr;					\
	lduw	[scr + %lo(hres_lock)], hrlock;	/* load clock lock */	\
	lduw	[scr + %lo(nsec_scale)], nano;	/* tick-to-ns factor */	\
	andn	hrlock, 1, hrlock;  	/* see comments above! */	\
	ldx	[scr + %lo(hres_last_tick)], nslt;			\
	ldn	[scr + %lo(hrestime)], hrestsec; /* load hrestime.sec */\
	add	scr, %lo(hrestime), hrestnsec;				\
	ldn	[hrestnsec + CLONGSIZE], hrestnsec;			\
	GET_NATIVE_TIME(adj);			/* get current %tick */	\
	sub	adj, nslt, nslt; /* nslt = ticks since last clockint */	\
	ldx	[scr + %lo(hrestime_adj)], adj; /* load hrestime_adj */	\
	/* membar #LoadLoad; (see comment (2) above) */			\
	lduw	[scr + %lo(hres_lock)], scr; /* load clock lock */	\
	NATIVE_TIME_TO_NSEC_SCALE_32(nslt, nano); /* ticks to nsecs */	\
	sethi	%hi(NANOSEC), nano;					\
	xor	hrlock, scr, scr;					\
/* CSTYLED */ 								\
	brnz,pn	scr, 5b;						\
	or	nano, %lo(NANOSEC), nano;

/*
 * Similar to above, but returns current gethrtime() value in 'base'.
 */
#define	GET_HRTIME(base, now, nslt, scale, scr, hrlock)			\
5:	sethi	%hi(hres_lock), scr;					\
	lduw	[scr + %lo(hres_lock)], hrlock;	/* load clock lock */	\
	lduw	[scr + %lo(nsec_scale)], scale;	/* tick-to-ns factor */	\
	andn	hrlock, 1, hrlock;  	/* see comments above! */	\
	ldx	[scr + %lo(hres_last_tick)], nslt;			\
	ldx	[scr + %lo(hrtime_base)], base;	/* load hrtime_base */	\
	GET_NATIVE_TIME(now);			/* get current %tick */	\
	sub	now, nslt, nslt; /* nslt = ticks since last clockint */	\
	/* membar #LoadLoad; (see comment (2) above) */			\
	ld	[scr + %lo(hres_lock)], scr; /* load clock lock */	\
	NATIVE_TIME_TO_NSEC_SCALE_32(nslt, scale); /* ticks to nsecs */	\
	xor	hrlock, scr, scr;					\
/* CSTYLED */ 								\
	brnz,pn	scr, 5b;						\
	add	base, nslt, base;

/*
 * Similar to above, but uses NATIVE_TIME_TO_NSEC_SCALE() since
 * the delta could be larger than 32bit. This is used in the first
 * and one time call to gethrtime_initial() to initialize hrtime_base
 * in clkstart().
 */
#define	GET_HRTIME_INITIAL(base, now, nslt, scale, scr, hrlock)		\
5:	sethi	%hi(hres_lock), scr;					\
	lduw	[scr + %lo(hres_lock)], hrlock; /* load clock lock */	\
	lduw	[scr + %lo(nsec_scale)], scale; /* tick-to-ns factor */	\
	andn	hrlock, 1, hrlock;	/* see comments above! */	\
	ldx	[scr + %lo(hres_last_tick)], nslt;			\
	ldx	[scr + %lo(hrtime_base)], base; /* load hrtime_base */	\
	GET_NATIVE_TIME(now);			/* get current %tick */	\
	sub	now, nslt, nslt; /* nslt = ticks since last clockint */	\
	/* membar #LoadLoad; (see comment (2) above) */			\
	ld	[scr + %lo(hres_lock)], scr; /* load clock lock */	\
	NATIVE_TIME_TO_NSEC_SCALE(nslt, scale, now); /* ticks2nsecs */	\
	xor	hrlock, scr, scr;					\
/* CSTYLED */								\
	brnz,pn	scr, 5b;						\
	add	base, nslt, base;

/*
 * Maximum-performance timestamp for kernel tracing.  We don't bother
 * clearing NPT because vtrace expresses everything in 32-bit deltas,
 * so only the low-order 32 bits matter.  We do shift down a few bits,
 * however, so that the trace framework doesn't emit a ridiculous number
 * of 32_bit_elapsed_time records (trace points are more expensive when
 * the time since the last trace point doesn't fit in a 16-bit delta).
 * We currently shift by 4 (divide by 16) on the grounds that (1) there's
 * no point making the timing finer-grained than the trace point latency,
 * which exceeds 16 cycles; and (2) the cost and probe effect of many
 * 32-bit time records far exceeds the cost of the 'srlx' instruction.
 */
#define	GET_VTRACE_TIME(out, scr1, scr2)				\
	GET_NATIVE_TIME(out);			/* get current %tick */	\
	srlx	out, VTRACE_SHIFT, out;

/*
 * Full 64-bit version for those truly rare occasions when you need it.
 * Currently this is only needed to generate the TR_START_TIME record.
 */
#define	GET_VTRACE_TIME_64(out, scr1, scr2)				\
	GET_NATIVE_TIME(out);			/* get current %tick */	\
	add	out, out, out;						\
	srlx	out, VTRACE_SHIFT + 1, out;

/*
 * Return the rate at which the vtrace clock runs.
 */
#define	GET_VTRACE_FREQUENCY(out, scr1, scr2)				\
	sethi	%hi(cpu_tick_freq), out;				\
	ld	[out + %lo(cpu_tick_freq)], out;			\
	srl	out, VTRACE_SHIFT, out;

#ifdef	__cplusplus
}
#endif

#endif	/* !_SYS_CLOCK_H */
