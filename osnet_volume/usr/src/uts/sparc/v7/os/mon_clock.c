/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mon_clock.c	1.2	99/10/04 SMI"

/*
 * The sun4d/sun4m monitor clock
 * -----------------------------------
 *
 * On these architectures, the level-14 counter/timer is used by the PROM to
 * implement its notion of time, including an internal alarm mechanism.  The
 * PROM uses these alarms to (among other things) poll the UART and support
 * the prom_gettime() entry point.  Therefore, to assure that L1-A/BRKs are
 * not lost, the kernel must not shut off the PROM's level-14 interrupt source
 * until the console is enabled.
 *
 * A problem arises:  the level-14 interrupt source is also used as the
 * interrupt source for the cyclic subsystem (and thus drives clock());
 * the cyclic subsystem is initialized before the console has been
 * configured.  If the cyclic subsystem were to simply pirate the level-14,
 * the system would not respond to L1-A/BRK until console initialization.
 *
 * Thus, from cyclic initialization until console initialization, the
 * level-14 must be shared between the PROM and the cyclic subsystem.
 *
 * Sharing is complicated by the fact that the PROM performs the rett on
 * the level-14 trap (that is, the kernel doesn't call into the PROM from
 * the trap handler; it branches there).  Configurable hz further complicates
 * matters; if the user has increased hz by hand, the level-14 under the
 * cyclic subsystem will be firing at a higher rate than the PROM expects.
 * Such a rate descrepency could effect strange behavior.
 *
 * We solve the latter problem by implementing the monitor clock as a
 * CY_HIGH_LEVEL (level-14) cyclic with an interval determined by reading
 * the level-14 counter/timer's limit register on boot.  While implementing
 * the monitor clock as a CY_HIGH_LEVEL cyclic addresses the rate discrepency,
 * we still have the problem of the PROM's rett; if we were to branch into
 * the PROM from a CY_HIGH_LEVEL cyclic handler, the PROM would
 * return-from-trap with the kernel in an inconsistent state.  To allow the
 * PROM to see a "raw" level-14, we have the monitor clock cyclic set the
 * boolean "mon_clock_go"; when cbe_fire() sees that mon_clock_go is set,
 * it will refire the level-14.  On sun4m, refiring is achieved by
 * explicitly not reading the counter/timer's limit register in cbe_fire()
 * (and thus not signaling end-of-interrupt).  On sun4d, refiring is achieved
 * by triggering a level-14 soft interrupt from cbe_fire().
 *
 * The monitor clock can be in one of four states:
 *
 *  MON_CLK_EXCLUSIVE <-- The PROM is using the level-14 exclusively.
 *
 *  MON_CLK_EXCLDISBL <-- The level-14 is currently not being used by the PROM;
 *                        a subsequent mon_clock_start() will put the monitor
 *                        clock into MON_CLK_EXCLUSIVE.
 *
 *  MON_CLK_DISABLED  <-- The level-14 is currently not being used by the PROM;
 *                        a subsequent mon_clock_start() will put the monitor
 *                        clock into into MON_CLK_SHARED.
 *
 *  MON_CLK_SHARED    <-- The level-14 is currently being shared between the
 *                        cyclic subsystem and the PROM.  The PROM will receive
 *                        its level-14 via the monitor clock cyclic and
 *                        cbe_fire().
 *
 * A state diagram:
 *
 *  +-------------------+    mon_clock_stop()    +-------------------+
 *  |                   |----------------------->|                   |
 *  | MON_CLK_EXCLUSIVE |                        | MON_CLK_EXCLDISBL |
 *  |                   |    mon_clock_start()   |                   |
 *  |                   |<-----------------------|                   |
 *  +-------------------+                        +-------------------+
 *                                                  |   ^
 *                                                  |   |
 *                                mon_clock_share() |   | mon_clock_unshare()
 *                                                  |   |
 *                                                  |   |
 *                                                  v   |
 *  +-------------------+    mon_clock_stop()    +-------------------+
 *  |                   |----------------------->|                   |
 *  |  MON_CLK_SHARED   |                        | MON_CLK_DISABLED  |
 *  |                   |    mon_clock_start()   |                   |
 *  |                   |<-----------------------|                   |
 *  +-------------------+                        +-------------------+
 */

#include <sys/mon_clock.h>
#include <sys/cyclic.h>
#include <sys/cmn_err.h>

char mon_clock = MON_CLK_EXCLUSIVE;
char mon_clock_go = 0;
processorid_t mon_clock_cpu = -1;

static hrtime_t mon_clock_nsec;
static cyclic_id_t mon_clock_cyclic = 0;
static int mon_clock_disable = 0;

void
mon_clock_init(void)
{
	mon_clock_cpu = CPU->cpu_id;
	mon_clock_nsec = level14_nsec(mon_clock_cpu);
}

/*ARGSUSED*/
void
mon_clock_tick(processorid_t me)
{
	if (mon_clock_disable)
		return;

	mon_clock_go = 1;
}

void
mon_clock_start(void)
{
	cyc_handler_t hdlr;
	cyc_time_t when;

	mutex_enter(&cpu_lock);

	switch (mon_clock) {
	case MON_CLK_EXCLDISBL:
		mon_clock_cpu = CPU->cpu_id;
		mon_clock = MON_CLK_EXCLUSIVE;
		/*FALLTHROUGH*/

	case MON_CLK_EXCLUSIVE:
		ASSERT(mon_clock_cpu != -1);
		level14_enable(mon_clock_cpu, mon_clock_nsec);
		break;

	case MON_CLK_SHARED:
		ASSERT(mon_clock_cyclic != 0);
		break;

	case MON_CLK_DISABLED:
		/*
		 * We can be MON_CLK_DISABLED only after mon_clock_share() has
		 * been called.  mon_clock_share(), in turn, cannot be called
		 * until the cyclic subsystem has initialized; we know that
		 * we can add a cyclic here.
		 */
		ASSERT(mon_clock_cyclic == 0);
		ASSERT(mon_clock_cpu == -1);

		mon_clock_go = 0;

		hdlr.cyh_func = (cyc_func_t)mon_clock_tick;
		hdlr.cyh_level = CY_HIGH_LEVEL;
		hdlr.cyh_arg = (void *)CPU->cpu_id;

		when.cyt_when = 0;
		when.cyt_interval = mon_clock_nsec;

		mon_clock = MON_CLK_SHARED;
		mon_clock_cpu = CPU->cpu_id;
		mon_clock_cyclic = cyclic_add(&hdlr, &when);
		cyclic_bind(mon_clock_cyclic, CPU, NULL);
		break;
	default:
		panic("unknown mon_clock state %d\n", mon_clock);
	}

	mutex_exit(&cpu_lock);
}

void
mon_clock_stop(void)
{
	mutex_enter(&cpu_lock);

	switch (mon_clock) {
	case MON_CLK_EXCLDISBL:
		ASSERT(mon_clock_cpu == -1);
		break;

	case MON_CLK_DISABLED:
		ASSERT(mon_clock_cyclic == 0 && mon_clock_cpu == -1);
		break;

	case MON_CLK_EXCLUSIVE:
		ASSERT(mon_clock_cpu != -1);
		mon_clock = MON_CLK_EXCLDISBL;
		level14_disable(mon_clock_cpu);
		mon_clock_cpu = -1;
		break;

	case MON_CLK_SHARED:
		ASSERT(mon_clock_cyclic != 0);
		ASSERT(mon_clock_cpu != -1);
		mon_clock = MON_CLK_DISABLED;

		cyclic_remove(mon_clock_cyclic);
		mon_clock_cyclic = 0;
		mon_clock_go = 0;
		mon_clock_cpu = -1;
		break;
	}

	mutex_exit(&cpu_lock);
}

void
mon_clock_share(void)
{
	ASSERT(mon_clock == MON_CLK_DISABLED || mon_clock == MON_CLK_EXCLDISBL);
	mon_clock = MON_CLK_DISABLED;
}

void
mon_clock_unshare(void)
{
	ASSERT(mon_clock == MON_CLK_DISABLED || mon_clock == MON_CLK_EXCLDISBL);
	mon_clock = MON_CLK_EXCLDISBL;
}
