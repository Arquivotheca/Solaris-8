/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hardclk.c	1.85	99/12/05 SMI"

#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/clock.h>
#include <sys/intreg.h>
#include <sys/x_call.h>
#include <sys/cpuvar.h>
#include <sys/promif.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/lockstat.h>
#include <vm/as.h>
#include <vm/hat.h>
#include <sys/intr.h>
#include <sys/ivintr.h>
#include <sys/sysiosbus.h>
#include <sys/machsystm.h>
#include <sys/reboot.h>
#include <sys/membar.h>
#include <sys/atomic.h>

int cpu_clock_mhz = 0;
uint_t cpu_tick_freq = 0;
uint_t scaled_clock_mhz = 0;
uint_t nsec_per_cpu_tick;
char clock_started = 0;

/*
 * Hardware watchdog parameters and knobs
 */
int watchdog_enable = 0;		/* user knob */
int watchdog_available = 0;		/* system has a watchdog */
int watchdog_activated = 0;		/* the watchdog is armed */
uint_t watchdog_timeout_seconds = CLK_WATCHDOG_DEFAULT;
int Cpudelay = 0;			/* delay loop count/usec */

/*
 * tod module name and operations
 */
struct tod_ops	tod_ops;
char		*tod_module_name;

extern uint_t find_cpufrequency(volatile uchar_t *);

void
clkstart(void)
{
	int ret;

	/*
	 * Now is a good time to activate hardware watchdog (if one exists).
	 */
	mutex_enter(&tod_lock);
	if (watchdog_enable)
		ret = tod_ops.tod_set_watchdog_timer(watchdog_timeout_seconds);
	else
		ret = 0;
	mutex_exit(&tod_lock);
	if (ret != 0)
		printf("Hardware watchdog enabled\n");
}

/*
 * preset the delay constant for drv_usecwait(). This is done for early
 * use of the le or scsi drivers in the kernel. The default contant
 * might be too high early on. We can get a pretty good approximation
 * of this by setting it as:
 *
 * 	cpu_clock_mhz = (cpu_tick_freq + 500000) / 1000000
 */
void
setcpudelay(void)
{
	/*
	 * We want to allow cpu_tick_freq to be tunable; we'll only set it
	 * if it hasn't been explicitly tuned.
	 */
	if (cpu_tick_freq == 0) {
		/*
		 * For UltraSPARC III and beyond we want to use the
		 * system clock rate as the basis for low level timing,
		 * due to support of mixed speed CPUs and power managment.
		 */
		if (use_stick) {
			if (system_clock_freq == 0)
				cmn_err(CE_PANIC,
				    "setcpudelay: invalid system_clock_freq");
			cpu_tick_freq = system_clock_freq;
		} else {
			/*
			 * Determine the cpu frequency by calling
			 * tod_get_cpufrequency. Use an approximate freqency
			 * value computed by the prom if the tod module
			 * is not initialized and loaded yet.
			 */
			if (tod_ops.tod_get_cpufrequency != NULL) {
				mutex_enter(&tod_lock);
				cpu_tick_freq = tod_ops.tod_get_cpufrequency();
				mutex_exit(&tod_lock);
			} else {
				cpu_tick_freq =
				    cpunodes[CPU->cpu_id].clock_freq;
			}
		}
	}

	/*
	 * See the comments in clock.h for a full description of
	 * nsec_scale.  The "& ~1" operation below ensures that
	 * nsec_scale is always even, so that for *any* value of
	 * %tick, multiplying by nsec_scale clears NPT for free.
	 */
	nsec_scale = (uint_t)(((u_longlong_t)NANOSEC << (32 - NSEC_SHIFT)) /
	    cpu_tick_freq) & ~1;

	/*
	 * scaled_clock_mhz is a more accurated (ie not rounded-off)
	 * version of cpu_clock_mhz that we used to program the tick
	 * compare register. Just in case cpu_tick_freq is like 142.5 Mhz
	 * instead of some whole number like 143
	 */

	scaled_clock_mhz = (cpu_tick_freq) / 1000;
	cpu_clock_mhz = (cpu_tick_freq + 500000) / 1000000;

	nsec_per_cpu_tick = NANOSEC / cpu_tick_freq;

	/*
	 * On Spitfire, because of the pipelining the 2 instruction
	 * loop in drvusec_wait() is grouped together and therefore
	 * only takes 1 cycle instead of 2. Because of this Cpudelay
	 * should be adjusted accordingly.
	 */

	if (cpu_clock_mhz > 0) {
		Cpudelay = cpu_clock_mhz;
		if (Cpudelay > 3) Cpudelay = Cpudelay - 3;


	} else {
		prom_printf("WARNING: Cpu node has invalid "
			"'clock-frequency' property\n");
	}
}

timestruc_t
tod_get(void)
{
	return (tod_ops.tod_get());
}

void
tod_set(timestruc_t ts)
{
	tod_ops.tod_set(ts);
}


/*
 * The following wrappers have been added so that locking
 * can be exported to platform-independent clock routines
 * (ie adjtime(), clock_setttime()), via a functional interface.
 */
int
hr_clock_lock(void)
{
	ushort_t s;

	CLOCK_LOCK(&s);
	return (s);
}

void
hr_clock_unlock(int s)
{
	CLOCK_UNLOCK(s);
}

/*
 * We don't share the trap table with the prom, so we don't need
 * to enable/disable its clock.
 */
void
mon_clock_init(void)
{}

void
mon_clock_start(void)
{}

void
mon_clock_stop(void)
{}

void
mon_clock_share(void)
{}

void
mon_clock_unshare(void)
{}
