/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)cbe.c	1.2	99/06/30 SMI"

#include <sys/systm.h>
#include <sys/cyclic.h>
#include <sys/cyclic_impl.h>
#include <sys/spl.h>
#include <sys/x_call.h>
#include <sys/kmem.h>
#include <sys/machsystm.h>
#include <sys/smp_impldefs.h>
#include <sys/psm_types.h>
#include <sys/atomic.h>
#include <sys/clock.h>

static int cbe_vector;
static int cbe_ticks = 0;

static ksema_t cbe_xcall_sema;
static cyc_func_t cbe_xcall_func;
static void *cbe_xcall_farg;
static cpu_t *cbe_xcall_cpu;
static cpuset_t cbe_enabled;

int
cbe_softclock(void)
{
	cyclic_softint(CPU, CY_LOCK_LEVEL);
	return (1);
}

int
cbe_low_level(void)
{
	cpu_t *cpu = CPU;

	if (cbe_xcall_func != NULL && cbe_xcall_cpu == cpu) {
		(*cbe_xcall_func)(cbe_xcall_farg);
		cbe_xcall_func = NULL;
		cbe_xcall_cpu = NULL;
		sema_v(&cbe_xcall_sema);
	}

	cyclic_softint(cpu, CY_LOW_LEVEL);
	return (1);
}

int
cbe_fire(void)
{
	cpu_t *cpu = CPU;
	processorid_t me = cpu->cpu_id, i;

	if (me == 0) {
		hres_tick();

		if ((cbe_ticks % hz) == 0)
			(*hrtime_tick)();

		cbe_ticks++;
	}

	cyclic_fire(cpu);

	if (me == 0) {
		for (i = 0; i < NCPU; i++) {
			if (CPU_IN_SET(cbe_enabled, i))
				send_dirint(i, CBE_HIGH_PIL);
		}
	}

	if (cbe_xcall_func != NULL && cbe_xcall_cpu == cpu)
		(*setsoftint)(CBE_LOW_PIL);

	return (1);
}

/*ARGSUSED*/
void
cbe_softint(void *arg, cyc_level_t level)
{
	switch (level) {
	case CY_LOW_LEVEL:
		(*setsoftint)(CBE_LOW_PIL);
		break;
	case CY_LOCK_LEVEL:
		(*setsoftint)(CBE_LOCK_PIL);
		break;
	default:
		panic("cbe_softint: unexpected soft level %d", level);
	}
}

/*ARGSUSED*/
void
cbe_reprogram(void *arg, hrtime_t time)
{
}

/*ARGSUSED*/
cyc_cookie_t
cbe_set_level(void *arg, cyc_level_t level)
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
void
cbe_restore_level(void *arg, cyc_cookie_t cookie)
{
	splx(cookie);
}

/*ARGSUSED*/
void
cbe_xcall(void *arg, cpu_t *dest, cyc_func_t func, void *farg)
{

	kpreempt_disable();

	if (dest == CPU) {
		(*func)(farg);
		kpreempt_enable();
		return;
	}

	ASSERT(cbe_xcall_func == NULL);

	cbe_xcall_farg = farg;
	membar_producer();
	cbe_xcall_cpu = dest;
	cbe_xcall_func = func;

	send_dirint(dest->cpu_id, CBE_HIGH_PIL);

	kpreempt_enable();

	sema_p(&cbe_xcall_sema);
	ASSERT(cbe_xcall_func == NULL && cbe_xcall_cpu == NULL);
}

void *
cbe_configure(cpu_t *cpu)
{
	return (cpu);
}

void
cbe_enable(void *arg)
{
	processorid_t me = ((cpu_t *)arg)->cpu_id;

	if (me == 0)
		return;

	ASSERT(!CPU_IN_SET(cbe_enabled, me));
	CPUSET_ADD(cbe_enabled, me);
}

void
cbe_disable(void *arg)
{
	processorid_t me = ((cpu_t *)arg)->cpu_id;

	if (me == 0) {
		/*
		 * If this is the boot CPU, we'll quietly refuse to disable
		 * our clock interrupt.
		 */
		return;
	}

	ASSERT(CPU_IN_SET(cbe_enabled, me));
	CPUSET_DEL(cbe_enabled, me);
}

void
cbe_init(void)
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

	cbe_vector = (*psm_get_clockirq)(CBE_HIGH_PIL);

	CPUSET_ZERO(cbe_enabled);

	sema_init(&cbe_xcall_sema, 0, NULL, SEMA_DEFAULT, NULL);

	mutex_enter(&cpu_lock);
	cyclic_init(&cbe, nsec_per_tick);
	mutex_exit(&cpu_lock);

	(*clkinitf)();

	(void) add_avintr(NULL, CBE_HIGH_PIL,
	    (avfunc)cbe_fire, "cbe_fire_master", cbe_vector, 0);

	if (psm_get_ipivect != NULL) {
		(void) add_avintr(NULL, CBE_HIGH_PIL, (avfunc)cbe_fire,
		    "cbe_fire_slave",
		    (*psm_get_ipivect)(CBE_HIGH_PIL, PSM_INTR_IPI_HI), 0);
	}

	(void) add_avsoftintr(NULL, CBE_LOCK_PIL, (avfunc)cbe_softclock,
	    "softclock", NULL);

	(void) add_avsoftintr(NULL, CBE_LOW_PIL,
	    (avfunc)cbe_low_level, "low level", NULL);
}
