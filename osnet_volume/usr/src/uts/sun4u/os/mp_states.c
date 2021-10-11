/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mp_states.c	1.3	99/04/13 SMI"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/membar.h>
#include <sys/machsystm.h>
#include <sys/cpuvar.h>
#include <sys/x_call.h>
#include <sys/platform_module.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>

#include <sys/cpu_sgnblk_defs.h>

static cpuset_t cpu_idle_set;
static kmutex_t cpu_idle_lock;

/*
 * flags to determine if the PROM routines
 * should be used to idle/resume/stop cpus
 */
int use_prom_stop = 0;
static int kern_idle[NCPU];		/* kernel's idle loop */
extern void debug_flush_windows();

/*
 * Initialize the idlestop mutex
 */
void
idlestop_init(void)
{
	mutex_init(&cpu_idle_lock, NULL, MUTEX_SPIN, (void *)ipltospl(PIL_15));
}

static void
cpu_idle_self(void)
{
	u_int s;
	label_t save;

	s = spl8();
	debug_flush_windows();

	CPU->cpu_m.in_prom = 1;
	membar_stld();

	save = curthread->t_pcb;
	(void) setjmp(&curthread->t_pcb);

	kern_idle[CPU->cpu_id] = 1;
	while (kern_idle[CPU->cpu_id])
		/* SPIN */;

	CPU->cpu_m.in_prom = 0;
	membar_stld();

	curthread->t_pcb = save;
	splx(s);
}

/*ARGSUSED*/
static void
cpu_stop_self(void)
{
	(void) spl8();
	debug_flush_windows();

	CPU->cpu_m.in_prom = 1;
	membar_stld();

	(void) setjmp(&curthread->t_pcb);

	if (use_prom_stop) {
		(void) prom_stop_self();
	} else {
		kern_idle[CPU->cpu_id] = 1;
		while (kern_idle[CPU->cpu_id])
			/* SPIN */;
	}
	/* shouldn't have gotten here */
	if (!panicstr) {
		cmn_err(CE_PANIC,
			"cpu_stop_self: return from prom_stop_self");
	} else {
		cmn_err(CE_WARN,
			"cpu_stop_self: return from prom_stop_self");
		/*CONSTCOND*/
		while (1)
			/* SPIN */;
	}
}

void
idle_other_cpus(void)
{
	int i, cpuid, ntries;
	int failed = 0;

	if (ncpus == 1)
		return;

	mutex_enter(&cpu_idle_lock);

	cpuid = CPU->cpu_id;
	ASSERT(cpuid < NCPU);

	cpu_idle_set = cpu_ready_set;
	CPUSET_DEL(cpu_idle_set, cpuid);

	if (CPUSET_ISNULL(cpu_idle_set))
		return;

	xt_some(cpu_idle_set, (xcfunc_t *)idle_stop_xcall,
	    (uint64_t)cpu_idle_self, NULL);

	for (i = 0; i < NCPU; i++) {
		if (!CPU_IN_SET(cpu_idle_set, i))
			continue;

		ntries = 0x10000;
		while (!cpu[i]->cpu_m.in_prom && ntries) {
			DELAY(50);
			ntries--;
		}

		/*
		 * A cpu failing to idle is an error condition, since
		 * we can't be sure anymore of its state.
		 */
		if (!cpu[i]->cpu_m.in_prom) {
			cmn_err(CE_WARN, "cpuid 0x%x failed to idle", i);
			failed++;
		}
	}

	if (failed) {
		mutex_exit(&cpu_idle_lock);
		cmn_err(CE_PANIC, "idle_other_cpus: not all cpus idled");
	}
}

void
resume_other_cpus(void)
{
	int i, ntries;
	int cpuid = CPU->cpu_id;
	boolean_t failed = B_FALSE;

	if (ncpus == 1)
		return;

	ASSERT(cpuid < NCPU);
	ASSERT(MUTEX_HELD(&cpu_idle_lock));

	for (i = 0; i < NCPU; i++) {
		if (!CPU_IN_SET(cpu_idle_set, i))
			continue;

		kern_idle[i] = 0;
		membar_stld();
	}

	for (i = 0; i < NCPU; i++) {
		if (!CPU_IN_SET(cpu_idle_set, i))
			continue;

		ntries = 0x10000;
		while (cpu[i]->cpu_m.in_prom && ntries) {
			DELAY(50);
			ntries--;
		}

		/*
		 * A cpu failing to resume is an error condition, since
		 * intrs may have been directed there.
		 */
		if (cpu[i]->cpu_m.in_prom) {
			cmn_err(CE_WARN, "cpuid 0x%x failed to resume", i);
			continue;
		}
		CPUSET_DEL(cpu_idle_set, i);
	}

	failed = !CPUSET_ISNULL(cpu_idle_set);

	mutex_exit(&cpu_idle_lock);

	/*
	 * Non-zero if a cpu failed to resume
	 */
	if (failed)
		cmn_err(CE_PANIC, "resume_other_cpus: not all cpus resumed");

}


void
stop_other_cpus(void)
{
	int i, cpuid, ntries;
	cpuset_t cpu_stop_set;
	boolean_t failed = B_FALSE;

	if (ncpus == 1)
		return;

	mutex_enter(&cpu_lock); /* for playing with cpu_flags */

	intr_redist_all_cpus(INTR_CURRENT_CPU);

	mutex_enter(&cpu_idle_lock);

	cpuid = CPU->cpu_id;
	ASSERT(cpuid < NCPU);

	cpu_stop_set = cpu_ready_set;
	CPUSET_DEL(cpu_stop_set, cpuid);

	if (CPUSET_ISNULL(cpu_stop_set)) {
		mutex_exit(&cpu_idle_lock);
		mutex_exit(&cpu_lock);
		return;
	}

	xt_some(cpu_stop_set, (xcfunc_t *)idle_stop_xcall,
	    (uint64_t)cpu_stop_self, NULL);

	for (i = 0; i < NCPU; i++) {
		if (!CPU_IN_SET(cpu_stop_set, i))
			continue;

		/*
		 * Make sure the stopped cpu looks quiesced  and is
		 * prevented from receiving further xcalls by removing
		 * it from the cpu_ready_set.
		 */
		cpu[i]->cpu_flags &= ~(CPU_READY | CPU_EXISTS);
		CPUSET_DEL(cpu_ready_set, i);

		ntries = 0x10000;
		while (!cpu[i]->cpu_m.in_prom && ntries) {
			DELAY(50);
			ntries--;
		}

		/*
		 * A cpu failing to stop is an error condition, since
		 * we can't be sure anymore of its state.
		 */
		if (!cpu[i]->cpu_m.in_prom) {
			cmn_err(CE_WARN, "cpuid 0x%x failed to stop", i);
			continue;
		}
		CPUSET_DEL(cpu_stop_set, i);
	}

	failed = !CPUSET_ISNULL(cpu_stop_set);

	mutex_exit(&cpu_idle_lock);
	mutex_exit(&cpu_lock);

	/*
	 * Non-zero if a cpu failed to stop
	 */
	if (failed && !panicstr)
		cmn_err(CE_PANIC, "stop_other_cpus: not all cpus stopped");
}

/*
 * set_idle_cpu is called from idle() when a CPU becomes idle.
 */
/*ARGSUSED*/
void
set_idle_cpu(int cpun)
{}

/*
 * unset_idle_cpu is called from idle() when a CPU is no longer idle.
 */
/*ARGSUSED*/
void
unset_idle_cpu(int cpun)
{}

/*
 * Start CPU on user request.
 */
/* ARGSUSED */
int
mp_cpu_start(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	/*
	 * StarFire requires the signature block update to indicate
	 * that this CPU is in OS now.
	 */
	SGN_UPDATE_CPU_OS_RUN_NULL(cp->cpu_id);
	return (0);			/* nothing special to do on this arch */
}

/*
 * Stop CPU on user request.
 */
/* ARGSUSED */
int
mp_cpu_stop(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	/*
	 * StarFire requires the signature block update to indicate
	 * that this CPU is in offlined now.
	 */
	SGN_UPDATE_CPU_OS_OFFLINE_NULL(cp->cpu_id);
	return (0);			/* nothing special to do on this arch */
}

/*
 * Power on CPU.
 */
int
mp_cpu_poweron(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (plat_cpu_poweron(cp));		/* platform-dependent hook */
}

/*
 * Power off CPU.
 */
int
mp_cpu_poweroff(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (plat_cpu_poweroff(cp));		/* platform-dependent hook */
}
