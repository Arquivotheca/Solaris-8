/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cbe.c	1.1	99/06/05 SMI"

#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/clock.h>
#include <sys/sysmacros.h>
#include <sys/lockstat.h>
#include <vm/as.h>
#include <sys/stat.h>
#include <sys/sunddi.h>
#include <sys/machsystm.h>
#include <sys/cyclic_impl.h>
#include <sys/clock.h>
#include <sys/kmem.h>
#include <sys/mon_clock.h>
#include <sys/atomic.h>

static ksema_t cbe_xcall_sema;
static cyc_func_t cbe_xcall_func;
static void *cbe_xcall_farg;
static cpu_t *cbe_xcall_cpu;

static processorid_t timekeeper = -1;
static int timekeeper_quiesced = 0;

static void
cbe_intr_clear()
{
	/*LINTED*/
	volatile int clear = v_counter_addr[CPU->cpu_id]->timer_msw;
}

void
cbe_fire(void)
{
	processorid_t me = CPU->cpu_id;

	if (me == timekeeper && timekeeper_quiesced)
		goto update;

	cyclic_fire(CPU);

	/*
	 * If the monitor clock is to fire, we'll update the time but won't
	 * clear the limit bit.  This will cause another level-14 to be
	 * immediately generated.
	 */
	if (mon_clock_go && mon_clock_cpu == me) {
		if (me == timekeeper)
			hres_tick(NULL);
		return;
	}

	if (me != timekeeper) {
		cbe_intr_clear();
		return;
	}
update:
	hres_tick(cbe_intr_clear);
}

void
cbe_fire_low(cpu_t *cpu)
{
	if (cbe_xcall_func != NULL && cbe_xcall_cpu == cpu) {
		(*cbe_xcall_func)(cbe_xcall_farg);
		cbe_xcall_func = NULL;
		cbe_xcall_cpu = NULL;
		sema_v(&cbe_xcall_sema);
	}

	if (cpu->cpu_cyclic != NULL)
		cyclic_softint(cpu, CY_LOW_LEVEL);
}

static void
cbe_softint(cyb_arg_t arg, cyc_level_t level)
{
	cpu_t *cpu = (cpu_t *)arg;

	switch (level) {
	case CY_LOW_LEVEL:
		send_dirint(cpu->cpu_id, CBE_LOW_PIL);
		break;
	case CY_LOCK_LEVEL:
		send_dirint(cpu->cpu_id, CBE_LOCK_PIL);
		break;
	default:
		panic("cbe_softint: unexpected soft level %d", level);
	}
}

/*ARGSUSED*/
static void
cbe_reprogram(cyb_arg_t arg, hrtime_t time)
{
}

static void
cbe_enable(cyb_arg_t arg)
{
	processorid_t me = ((cpu_t *)arg)->cpu_id;

	/*
	 * If we're currently the timekeeper, then the level-14 on this
	 * CPU was never actually disabled; we don't need to re-enable it.
	 */
	if (timekeeper == me) {
		timekeeper_quiesced = 0;
		return;
	}

	/*
	 * If there isn't currently a timekeeper, we'll volunteer.
	 */
	if (timekeeper == -1)
		timekeeper = me;

	v_counter_addr[me]->timer_msw =
	    ((usec_per_tick << CTR_USEC_SHIFT) & CTR_USEC_MASK);
}

static void
cbe_disable(cyb_arg_t arg)
{
	processorid_t me = ((cpu_t *)arg)->cpu_id;

	/*
	 * If we're currently the timekeeper, then we'll quietly refuse to
	 * disable our level-14.
	 */
	if (timekeeper == me) {
		timekeeper_quiesced = 1;
		return;
	}

	v_counter_addr[me]->timer_msw = 0;
}

static void
cbe_suspend(cyb_arg_t arg)
{
	processorid_t me = ((cpu_t *)arg)->cpu_id;

	/*
	 * If we're not the timekeeper, then this CPU should already be
	 * disabled.  Otherwise, we need to disable our level-14 (time
	 * will stop after this point).
	 */
	if (timekeeper != me)
		return;

	v_counter_addr[me]->timer_msw = 0;
}

static void
cbe_resume(cyb_arg_t arg)
{
	processorid_t me = ((cpu_t *)arg)->cpu_id;

	/*
	 * If we're not the timekeeper, then this CPU will be lit up by a
	 * later call to cbe_enable().  If we're the timekeeper, we need to
	 * start the level-14 again.
	 */
	if (timekeeper != me)
		return;

	v_counter_addr[me]->timer_msw =
	    ((usec_per_tick << CTR_USEC_SHIFT) & CTR_USEC_MASK);
}

/*ARGSUSED*/
static cyc_cookie_t
cbe_set_level(cyb_arg_t arg, cyc_level_t level)
{
	int ipl;

	switch (level) {
	case CY_LOW_LEVEL:
		ipl = CBE_LOW_PIL;
		break;
	case CY_LOCK_LEVEL:
		ipl = CBE_LOCK_PIL;
		break;
	case CY_HIGH_LEVEL:
		ipl = CBE_HIGH_PIL;
		break;
	default:
		panic("cbe_set_level: unexpected level %d", level);
	}

	return (splr(ipltospl(ipl)));
}

/*ARGSUSED*/
static void
cbe_restore_level(cyb_arg_t arg, cyc_cookie_t cookie)
{
	splx(cookie);
}

/*ARGSUSED*/
static void
cbe_xcall(cyb_arg_t arg, cpu_t *dest, cyc_func_t func, void *farg)
{
	kpreempt_disable();

	if (dest == CPU) {
		ASSERT(dest == (cpu_t *)arg);
		(*func)(farg);
		kpreempt_enable();
		return;
	}

	ASSERT(cbe_xcall_func == NULL);

	cbe_xcall_farg = farg;
	membar_producer();
	cbe_xcall_cpu = dest;
	cbe_xcall_func = func;

	send_dirint(dest->cpu_id, CBE_LOW_PIL);

	kpreempt_enable();

	sema_p(&cbe_xcall_sema);
	ASSERT(cbe_xcall_func == NULL && cbe_xcall_cpu == NULL);
}

static cyb_arg_t
cbe_configure(cpu_t *cpu)
{
	return (cpu);
}

void
cbe_init(void)
{
	int i;

	cyc_backend_t cbe = {
		cbe_configure,		/* cyb_configure */
		NULL,			/* cyb_unconfigure */
		cbe_enable,		/* cyb_enable */
		cbe_disable,		/* cyb_disable */
		cbe_reprogram,		/* cyb_reprogram */
		cbe_softint,		/* cyb_softint */
		cbe_set_level,		/* cyb_set_level */
		cbe_restore_level,	/* cyb_restore_level */
		cbe_xcall,		/* cyb_xcall */
		cbe_suspend,		/* cyb_suspend */
		cbe_resume		/* cyb_resume */
	};

	sema_init(&cbe_xcall_sema, 0, NULL, SEMA_DEFAULT, NULL);

	mon_clock_stop();

	for (i = 0; i < NCPU; i++) {
		if (cpu[i] != NULL && (cpu[i]->cpu_flags & CPU_EXISTS))
			v_counter_addr[i]->timer_msw = 0;
	}

	v_level10clk_addr->config = 0;
	clock_addr = (uintptr_t)v_counter_addr[CPU->cpu_id];

	mutex_enter(&cpu_lock);
	cyclic_init(&cbe, nsec_per_tick);
	mutex_exit(&cpu_lock);

	mon_clock_share();
	mon_clock_start();
}
