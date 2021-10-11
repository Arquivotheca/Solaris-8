/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cbe.c	1.1	99/06/05 SMI"

#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/clock.h>
#include <sys/pte.h>
#include <sys/scb.h>
#include <sys/cpuvar.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/mman.h>
#include <sys/vmem.h>
#include <sys/promif.h>
#include <sys/sysmacros.h>
#include <sys/lockstat.h>
#include <vm/seg_kmem.h>
#include <sys/syserr.h>
#include <sys/stat.h>
#include <sys/sunddi.h>
#include <sys/cyclic_impl.h>
#include <sys/kmem.h>
#include <sys/machsystm.h>
#include <sys/atomic.h>
#include <sys/mon_clock.h>

extern int intr_prof_getlimit_local(void);

int cbe_spurious = 0;

static ksema_t cbe_xcall_sema;
static cyc_func_t cbe_xcall_func;
static void *cbe_xcall_farg;
static cpu_t *cbe_xcall_cpu;

static processorid_t timekeeper = -1;
static int timekeeper_quiesced = 0;

void
cbe_fire()
{
	processorid_t me = CPU->cpu_id;

	/*
	 * If we're the timekeeper, then we need to update the system's
	 * notion of high-resolution time.  If we've been previously disabled,
	 * then we don't need to do anything else.
	 */
	if (me == timekeeper) {
		hres_tick((void(*)(void))intr_prof_getlimit_local);

		if (timekeeper_quiesced)
			return;
	} else {
		/*
		 * Clear the interrupt.
		 */
		(void) intr_prof_getlimit_local();
	}

	if (timekeeper == -1) {
		cbe_spurious++;
		return;
	}

	if (CPU->cpu_cyclic != NULL)
		cyclic_fire(CPU);

	if (mon_clock_go && mon_clock_cpu == me)
		xmit_cpu_intr(me, CBE_HIGH_PIL);
}

void
cbe_fire_low()
{
	cpu_t *cpu = CPU;

	if (cbe_xcall_func != NULL && cbe_xcall_cpu == cpu) {
		(*cbe_xcall_func)(cbe_xcall_farg);
		cbe_xcall_func = NULL;
		cbe_xcall_cpu = NULL;
		sema_v(&cbe_xcall_sema);
	}

	if (cpu->cpu_cyclic != NULL)
		cyclic_softint(cpu, CY_LOW_LEVEL);
}

/*ARGSUSED*/
static void
cbe_softint(cyb_arg_t arg, cyc_level_t level)
{
	switch (level) {
	case CY_LOW_LEVEL:
		setsoftint(CBE_LOW_PIL);
		break;
	case CY_LOCK_LEVEL:
		setsoftint(CBE_LOCK_PIL);
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

	level14_enable(me, nsec_per_tick);
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

	level14_disable(me);
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
		(*func)(farg);
		kpreempt_enable();
		return;
	}

	ASSERT(cbe_xcall_func == NULL);

	/*
	 * This is a little weak.  sun4d has no cross call mechanism, so
	 * we use the level-1 softint.  We're guaranteed to have only one
	 * thread at a time calling into cbe_xcall, so we can just use
	 * global data.
	 *
	 * The function argument needs to hit global visibility before the
	 * function pointer.
	 */
	cbe_xcall_farg = farg;
	membar_producer();
	cbe_xcall_cpu = dest;
	cbe_xcall_func = func;

	xmit_cpu_intr(dest->cpu_id, CBE_LOW_PIL);

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
cbe_init()
{
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
		NULL,			/* cyb_suspend */
		NULL			/* cyb_resume */
	};

	sema_init(&cbe_xcall_sema, 0, NULL, SEMA_DEFAULT, NULL);

	mon_clock_stop();

	mutex_enter(&cpu_lock);
	cyclic_init(&cbe, nsec_per_tick);
	mutex_exit(&cpu_lock);

	mon_clock_share();
	mon_clock_start();
}
