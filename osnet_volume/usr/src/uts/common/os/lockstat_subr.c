/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lockstat_subr.c	1.2	99/07/27 SMI"

#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/time.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/cyclic.h>
#include <sys/lockstat.h>
#include <sys/spl.h>

/*
 * Resident support for the lockstat driver.
 */

uchar_t lockstat_event[LS_MAX_EVENTS];

void (*lockstat_enter_op)(uintptr_t, uintptr_t, uintptr_t) =
	lockstat_enter_nop;
void (*lockstat_exit_op)(uintptr_t, uintptr_t, uint32_t, uintptr_t, uintptr_t) =
	lockstat_exit_nop;
void (*lockstat_record_op)(uintptr_t, uintptr_t, uint32_t, uintptr_t,
    hrtime_t) = lockstat_record_nop;

static hrtime_t lockstat_intr_interval;

/* ARGSUSED */
void
lockstat_enter_nop(uintptr_t lp, uintptr_t caller, uintptr_t owner)
{
}

/* ARGSUSED */
void
lockstat_exit_nop(uintptr_t lp, uintptr_t caller, uint32_t event,
	uintptr_t refcnt, uintptr_t owner)
{
}

/* ARGSUSED */
void
lockstat_record_nop(uintptr_t lp, uintptr_t caller, uint32_t event,
	uintptr_t refcnt, hrtime_t duration)
{
}

int
lockstat_depth(void)
{
	return (curthread->t_lockstat);
}

int
lockstat_active_threads(void)
{
	kthread_t *tp;
	int active = 0;

	mutex_enter(&pidlock);
	tp = curthread;
	do {
		if (tp->t_lockstat)
			active++;
	} while ((tp = tp->t_next) != curthread);
	mutex_exit(&pidlock);
	return (active);
}

static void
lockstat_intr(cpu_t *cp)
{
	hrtime_t ilate;

	if ((ilate = cp->cpu_profile_ilate) == 0)
		ilate = gethrtime() - cp->cpu_profile_when;
	else
		scalehrtime(&ilate);

	cp->cpu_profile_when += lockstat_intr_interval;
	cp->cpu_profile_ilate = 0;

	if (lockstat_event[LS_PROFILE_INTR] & LSE_RECORD) {
		curthread->t_lockstat++;
		lockstat_record_op((uintptr_t)cp +
		    spltoipl(cp->cpu_profile_pil),
		    cp->cpu_profile_pc, LS_PROFILE_INTR, 1, ilate);
		curthread->t_lockstat--;
	}
	cp->cpu_profile_pc = 0;
}

void
lockstat_interrupt_on(hrtime_t interval)
{
	cpu_t *cp;
	cyc_handler_t hdlr;
	cyc_time_t when;
	cyclic_id_t cid;
	int i = 0;
	hrtime_t now = gethrtime();

	if (interval == 0)
		return;

	lockstat_intr_interval = interval;

	mutex_enter(&cpu_lock);
	cp = cpu_list;
	do {
		if (cp->cpu_flags & CPU_OFFLINE)
			continue;

		ASSERT(cp->cpu_profile_cyclic_id == CYCLIC_NONE);

		hdlr.cyh_func = (cyc_func_t)lockstat_intr;
		hdlr.cyh_arg = cp;
		hdlr.cyh_level = CY_HIGH_LEVEL;

		/*
		 * We stagger the interrupt start times to minimize
		 * contention for lockstat hash chains.
		 */
		when.cyt_when = now - interval * ++i / ncpus_online;
		when.cyt_interval = interval;

		cp->cpu_profile_when = when.cyt_when;
		cid = cyclic_add(&hdlr, &when);
		cyclic_bind(cid, cp, NULL);

		cp->cpu_profile_cyclic_id = cid;

	} while ((cp = cp->cpu_next) != cpu_list);
	mutex_exit(&cpu_lock);
}

void
lockstat_interrupt_off(void)
{
	cpu_t *cp;

	mutex_enter(&cpu_lock);
	cp = cpu_list;
	do {
		if (cp->cpu_profile_cyclic_id != CYCLIC_NONE) {
			cyclic_remove(cp->cpu_profile_cyclic_id);
			cp->cpu_profile_cyclic_id = CYCLIC_NONE;
		}
	} while ((cp = cp->cpu_next) != cpu_list);
	mutex_exit(&cpu_lock);
}
