/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)timestamp.c	1.8	99/10/25 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/disp.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/x86_archext.h>
#include <sys/archsystm.h>
#include <sys/cpuvar.h>
#include <sys/psm_defs.h>
#include <sys/clock.h>
#include <sys/atomic.h>
#include <sys/lockstat.h>
#include <sys/smp_impldefs.h>

/*
 * Using the Pentium's TSC register for gethrtime()
 * ------------------------------------------------
 *
 * The Pentium family, like many chip architectures, has a high-resolution
 * timestamp counter ("TSC") which increments once per CPU cycle.  The contents
 * of the timestamp counter are read with the RDTSC instruction.
 *
 * As with its UltraSPARC equivalent (the %tick register), TSC's cycle count
 * must be translated into nanoseconds in order to implement gethrtime().
 * We avoid inducing floating point operations in this conversion by
 * implementing the same nsec_scale algorithm as that found in the sun4u
 * platform code.  The sun4u NATIVE_TIME_TO_NSEC_SCALE block comment contains
 * a detailed description of the algorithm; the comment is not reproduced
 * here.  This implementation differs only in its value for NSEC_SHIFT:
 * we implement an NSEC_SHIFT of 5 (instead of sun4u's 4) to allow for
 * 60 MHz Pentiums.
 *
 * While TSC and %tick are both cycle counting registers, TSC's functionality
 * falls short in several critical ways:
 *
 *  (a)	TSCs on different CPUs are not guaranteed to be in sync.  While in
 *	practice they often _are_ in sync, this isn't guaranteed by the
 *	architecture.
 *
 *  (b)	The TSC cannot be reliably set to an arbitrary value.  The architecture
 *	only supports writing the low 32-bits of TSC, making it impractical
 *	to rewrite.
 *
 *  (c)	The architecture doesn't have the capacity to interrupt based on
 *	arbitrary values of TSC; there is no TICK_CMPR equivalent.
 *
 * Together, (a) and (b) imply that software must track the skew between
 * TSCs and account for it (it is assumed that while there may exist skew,
 * there does not exist drift).  To determine the skew between CPUs, we
 * have newly onlined CPUs call tsc_sync_slave(), while the CPU performing
 * the online operation (i.e. the boot CPU), calls tsc_sync_master().
 * Once both CPUs are ready, the master sets a shared flag, and each reads
 * their TSC register.  The master compares the values, and, if skew is found,
 * changes the gethrtimef function pointer to point to a gethrtime()
 * implementation which will take the discovered skew into consideration.
 *
 * In the absence of time-of-day clock adjustments, gethrtime() must stay in
 * sync with gettimeofday().  This is problematic; given (c), the software
 * cannot drive its time-of-day source from TSC, and yet they must somehow be
 * kept in sync.  We implement this by having a routine, tsc_tick(), which
 * is called once per second from the interrupt which drives time-of-day.
 * tsc_tick() recalculates nsec_scale based on the number of the CPU cycles
 * since boot versus the number of seconds since boot.  This algorithm
 * becomes more accurate over time and converges quickly; the error in
 * nsec_scale is typically under 1 ppm less than 10 seconds after boot, and
 * is less than 100 ppb 1 minute after boot.
 *
 * Note that the hrtime base for gethrtime, tsc_hrtime_base, is modified
 * atomically with nsec_scale under CLOCK_LOCK.  This assures that time
 * monotonically increases.
 */

#define	NSEC_SHIFT 5

static u_int nsec_scale;

/*
 * To minimize strange effects, we want the entire tsc_sync_flag to live
 * on the same cache line.
 */
#pragma align 32(tsc_sync_flag)
static struct {
	volatile int tsc_ready;
	volatile int tsc_sync_go;
} tsc_sync_flag;

/*
 * Used as indices into the tsc_sync_snaps[] array.
 */
#define	TSC_MASTER		0
#define	TSC_SLAVE		1

/*
 * Used in the tsc_master_sync()/tsc_slave_sync() rendezvous.
 */
#define	TSC_SYNC_STOP		1
#define	TSC_SYNC_GO		2

#define	TSC_CONVERT_AND_ADD(tsc, hrt) { \
	unsigned long *_l = (unsigned long *)&(tsc); \
	(hrt) += mul32(_l[1], nsec_scale) << NSEC_SHIFT; \
	(hrt) += mul32(_l[0], nsec_scale) >> (32 - NSEC_SHIFT); \
}

static int	tsc_max_delta;
static hrtime_t tsc_sync_snaps[2];
static hrtime_t tsc_sync_delta[NCPU];
static hrtime_t	tsc_last = 0;
static hrtime_t	tsc_first = 0;
static uint64_t	tsc_seconds = 0;
static hrtime_t	tsc_hrtime_base = 0;
static uint_t	tsc_hz;

/*
 * Called by the master after the sync operation is complete.  If the
 * slave is discovered to lag, gethrtimef will be changed to point to
 * tsc_gethrtime_delta().
 */
static void
tsc_digest(processorid_t target)
{
	hrtime_t tdelta, hdelta = 0;
	int max = tsc_max_delta;

	if (tsc_sync_snaps[TSC_SLAVE] - tsc_sync_snaps[TSC_MASTER] > max) {
		tdelta = tsc_sync_snaps[TSC_SLAVE] - tsc_sync_snaps[TSC_MASTER];
		TSC_CONVERT_AND_ADD(tdelta, hdelta);
		tsc_sync_delta[target] = -hdelta;
	}

	if (tsc_sync_snaps[TSC_MASTER] - tsc_sync_snaps[TSC_SLAVE] > max) {
		tdelta = tsc_sync_snaps[TSC_MASTER] - tsc_sync_snaps[TSC_SLAVE];
		TSC_CONVERT_AND_ADD(tdelta, hdelta);
		tsc_sync_delta[target] = hdelta;
	}

	if (tsc_sync_delta[target] != 0)
		gethrtimef = tsc_gethrtime_delta;
}

/*
 * Called by a CPU which has just performed an online operation on another
 * CPU.  It is expected that the newly onlined CPU will call tsc_sync_slave().
 */
void
tsc_sync_master(processorid_t slave)
{
	int flags;

	ASSERT(tsc_sync_flag.tsc_sync_go != TSC_SYNC_GO);

	flags = clear_int_flag();

	/*
	 * Wait for the slave CPU to arrive.
	 */
	while (tsc_sync_flag.tsc_ready != TSC_SYNC_GO)
		continue;

	/*
	 * Tell the slave CPU to begin reading its TSC, read our own, and wait
	 * for the slave to report completion.
	 */
	tsc_sync_flag.tsc_sync_go = TSC_SYNC_GO;
	tsc_sync_snaps[TSC_MASTER] = tsc_read();

	while (tsc_sync_flag.tsc_ready != TSC_SYNC_STOP)
		continue;

	/*
	 * By here, both CPUs have performed their tsc_read().  We'll digest
	 * it now, before letting the slave CPU return.
	 */
	tsc_digest(slave);
	tsc_sync_flag.tsc_sync_go = TSC_SYNC_STOP;

	restore_int_flag(flags);
}

/*
 * Called by a CPU which has just been onlined.  It is expected that the CPU
 * performing the online operation will call tsc_sync_master().
 */
void
tsc_sync_slave()
{
	int flags;

	ASSERT(tsc_sync_flag.tsc_sync_go != TSC_SYNC_GO);

	flags = clear_int_flag();

	/*
	 * Tell the master CPU that we're ready, and wait for the master to
	 * tell us to begin reading our TSC.
	 */
	tsc_sync_flag.tsc_ready = TSC_SYNC_GO;

	while (tsc_sync_flag.tsc_sync_go != TSC_SYNC_GO)
		continue;

	/*
	 * Read our TSC, notify the master CPU that we're done, and
	 * wait for the master to dismiss us.
	 */
	tsc_sync_snaps[TSC_SLAVE] = tsc_read();
	tsc_sync_flag.tsc_ready = TSC_SYNC_STOP;

	while (tsc_sync_flag.tsc_sync_go != TSC_SYNC_STOP)
		continue;

	restore_int_flag(flags);
}

void
tsc_hrtimeinit(uint32_t cpu_freq)
{
	ulong_t cpu_hz;
	longlong_t tsc;
	int flags;

	/*
	 * We'll start with a very crude estimate of the actual tick frequency.
	 */
	cpu_hz = (ulong_t)cpu_freq * 1000000;

	/*
	 * We can't accommodate CPUs slower than 31.25 MHz.
	 */
	ASSERT(cpu_hz > NANOSEC / (1 << NSEC_SHIFT));
	nsec_scale =
	    (u_int)(((u_longlong_t)NANOSEC << (32 - NSEC_SHIFT)) / cpu_hz);

	flags = clear_int_flag();
	tsc = tsc_read();
	(void) tsc_gethrtime();
	tsc_max_delta = tsc_read() - tsc;
	restore_int_flag(flags);
}

/*
 * Called once per second on CPU 0 from the cyclic subsystem's CY_HIGH_LEVEL
 * interrupt.
 */
void
tsc_tick()
{
	hrtime_t now, delta;
	u_short spl;

	CLOCK_LOCK(&spl);

	now = tsc_read();

	/*
	 * Determine the number of TSC ticks since the last clock tick, and
	 * add that to the hrtime base.
	 */
	delta = now - tsc_last;
	TSC_CONVERT_AND_ADD(delta, tsc_hrtime_base);
	tsc_last = now;

	if (tsc_seconds == 0) {
		tsc_first = now;
		goto out;
	}

	/*
	 * Now we need to update nsec_scale.  This, too, must be done under
	 * CLOCK_LOCK to assure that time monotonically increases.
	 */
	delta = now - tsc_first;

	tsc_hz = delta / tsc_seconds;
	nsec_scale = (u_int)(((hrtime_t)NANOSEC << (32 - NSEC_SHIFT)) / tsc_hz);

out:
	tsc_seconds++;

	CLOCK_UNLOCK(spl);
}

hrtime_t
tsc_gethrtime()
{
	uint32_t old_hres_lock;
	hrtime_t tsc, hrt;

	do {
		old_hres_lock = hres_lock;

		tsc = tsc_read() - tsc_last;
		hrt = tsc_hrtime_base;

		TSC_CONVERT_AND_ADD(tsc, hrt);
	} while ((old_hres_lock & ~1) != hres_lock);

	return (hrt);
}

hrtime_t
tsc_gethrtime_delta()
{
	hrtime_t hrt;

	/*
	 * We need to disable preemption here to assure that we don't migrate
	 * between the call to tsc_gethrtime() and adding the CPU's hrtime
	 * delta.
	 */
	kpreempt_disable();
	hrt = tsc_gethrtime() + tsc_sync_delta[CPU->cpu_id];
	kpreempt_enable();

	return (hrt);
}
