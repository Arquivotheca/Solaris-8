/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#ident	"@(#)cpudelay.c	1.34	97/10/22 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/cmn_err.h>
#include <sys/syserr.h>
#include <sys/cpuvar.h>
#include <sys/systm.h>
#include <sys/promif.h>

/*
 * For sun4d/SuperSPARC we really should have POST provide us with a
 * "calibration" value which can be then put in an OBP/forth property.
 *
 * Also, as far as I can tell, the only use of this is busy-wait,
 * which could be done equally well by reading the hi-res timer.
 */

extern int Cpudelay;	/* delay loop count/usec */
extern struct cpu cpu0;
static int fastest_cpuid = -1;

/*
 * setcpudelay runs on the boot cpu and looks for the 'clock-frequency'
 * property of each cpu (not just the boot cpu). It saves the cpu_id
 * of the fastest one and bases Cpudelay on that clock speed. This
 * runs before the hires timer is available and for use by drivers that
 * run early on. The approximation is (5% decrease is for overhead in
 * drv_usecwait):
 *
 *		cpudelay = clock_frequency_cpu / 2;
 *		cpudelay -= cpudelay/20;
 *
 * If there's an error in any of the device tree lookups, it returns
 * leaving Cpudelay at the default value (preset to be the fastest
 * supported processor). This is necessary, since we can't be sure which
 * cpu is the fastest.
 */
void
setcpudelay(void)
{
	dnode_t id;		/* pointer to PROM device node */
	int pcpu_id;		/* PROM node's CPU id */
	int cpu_clock, tmp_cpuid, fastest_clock = 0;
	int ncpunode = 0;	/* ncpus in devtree */

	/* start at root node of PROM device tree */
	id = prom_nextnode(0);

	/* get the child of the root node */
	id = prom_childnode(id);

	while (id != OBP_BADNODE && id != OBP_NONODE) {
		dnode_t node;

		/* skip over nodes that are not "cpu-unit" nodes */
		if (!prom_getnode_byname(id, "cpu-unit")) {
			id = prom_nextnode(id);
			continue;
		}

		/*
		 * find the child of this 'cpu-unit'. There is only one child
		 * of a 'cpu-unit' with a property called 'cpu-id'. It is
		 * usually named TI,TMS390Z55, but we are not hard coding
		 * in this string. This name could change in the PROM.
		 */
		node = prom_childnode(id);
		ncpunode++;

		while (node != OBP_BADNODE && node != OBP_NONODE) {
			/* look for a cpu-id property in node */
			if (prom_getprop(node, "cpu-id", (caddr_t)
			    &pcpu_id) == sizeof (int)) {
				break;
			}
			node = prom_nextnode(node);
		}

		/*
		 * if we did not find a node with 'cpu-id', then
		 * we have a problem with the OBP device tree.
		 */
		if (node == OBP_BADNODE || node == OBP_NONODE) {
			id = prom_nextnode(id);
			continue;
		}

		/*
		 * If there's an error getting the 'clock-frequency' attr,
		 * return without updating Cpudelay, since there's no way to
		 * compare it to the other cpus. Use the default Cpudelay,
		 * which is based on the fastest supported processor.
		 */
		if ((prom_getprop(node, "clock-frequency",
				(caddr_t)&cpu_clock)) != sizeof (int)) {
#ifdef DEBUG
			prom_printf("WARNING: cpu%d has no "
				"'clock-frequency' property\n", pcpu_id);
#endif
			return;
		}
		if (cpu_clock <= 0) {
#ifdef DEBUG
			prom_printf("WARNING: cpu%d has invalid "
				"'clock-frequency' property\n", pcpu_id);
#endif
			return;
		}

		/*
		 * convert from Hz to MHz, making
		 * sure to round to the nearest
		 * MHz.
		 */
		cpu_clock = (cpu_clock + 500000)/1000000;
		/*
		 * To be sure that cpu0 has a clock freq.
		 * when cpu_attach() is called for all CPUs,
		 * set up cpu0 clock frequency here.
		 * Fix for BugID 1124644.
		 */
		if (pcpu_id == cpu0.cpu_id)
			cpu0.cpu_type_info.pi_clock = cpu_clock;

		if (cpu_clock > fastest_clock) {
			fastest_clock = cpu_clock;
			tmp_cpuid = pcpu_id;
		}

		/* look for next sibling in device tree */
		id = prom_nextnode(id);
	}
	if (fastest_clock != 0) {
		Cpudelay = fastest_clock/2;
		Cpudelay -= Cpudelay/20;

		fastest_cpuid = tmp_cpuid;
	}
	/* Update max_ncpus */
	if (ncpunode > 0 && ncpunode <= NCPU)
		max_ncpus = ncpunode;
}

#define	NS_PER_US	(NANOSEC / MICROSEC)

/*
 * resetcpudelay is called by each cpu as it starts up to see if it's
 * the one that should calibrate Cpudelay, based on hi-res timer values.
 * If it's cpu_id doesn't match fastest_cpuid, it returns. If there's
 * an error in the calibration, it leaves Cpudelay at its current value.
 */
void
resetcpudelay(void)
{
	unsigned	e;		/* delay time, us */
	unsigned	es;		/* delay time, ~ns */
	hrtime_t	t, f;		/* for time measurement */
	int		s;		/* saved PSR for inhibiting ints */
	int		cpudelay;	/* local copy of default setting */


	/* Don't calibrate, if this isn't the fastest cpu */
	if (CPU->cpu_id != fastest_cpuid)
		return;

	/*
	 * Figure out how big Cpudelay should be by iteratively
	 * measuring DELAY()'s with gethrtime() while at PIL 15.
	 * A caveat:  gethrtime() itself is drive by the level 10
	 * interrupt, so don't delay for more than usec_per_tick.
	 */

	cpudelay = Cpudelay;	/* save copy of default setting */
	e = usec_per_tick / 2;
	es = e * NS_PER_US;		/* adjusted target delay, in ns */

	Cpudelay = 1;	/* initial guess (better safe than sorry) */
	DELAY(1);	/* warm up the caches */
	do {
		Cpudelay <<= 1;	/* double until big enough */
		do {
			s = spl8();
			t = gethrtime();	/* get low word of time */
			DELAY(e);
			f = gethrtime();   /* get low word of time */
			splx(s);
		} while (f < t);
		t = f - t;
	} while (t < es);
	Cpudelay = (Cpudelay * es + t) / t;

	/* restore default setup if above code fails */
	if (Cpudelay < 0)
		Cpudelay = cpudelay;
}
