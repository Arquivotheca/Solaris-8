/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpu_intr.c	1.7	99/06/05 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/var.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/kstat.h>
#include <sys/uadmin.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/procset.h>
#include <sys/processor.h>
#include <sys/debug.h>
#include <sys/cyclic.h>

/*
 * cpu_intr_on - determine whether the CPU is participating
 * in I/O interrupts.
 */
int
cpu_intr_on(cpu_t *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return ((cp->cpu_flags & CPU_ENABLE) != 0);
}

/*
 * Return the next on-line CPU handling interrupts.
 */
cpu_t *
cpu_intr_next(cpu_t *cp)
{
	cpu_t	*c;

	ASSERT(MUTEX_HELD(&cpu_lock));

	c = cp->cpu_next_onln;
	while (c != cp) {
		if (cpu_intr_on(c)) {
			return (c);
		}
		c = c->cpu_next_onln;
	}
	return (NULL);
}

/*
 * cpu_intr_count - count how many CPUs are handling I/O interrupts.
 */
int
cpu_intr_count(cpu_t *cp)
{
	cpu_t	*c;
	int	count = 0;

	ASSERT(MUTEX_HELD(&cpu_lock));
	c = cp;
	do {
		if (cpu_intr_on(c)) {
			++count;
		}
	} while ((c = c->cpu_next) != cp);
	return (count);
}

/*
 * Enable I/O interrupts on this CPU, if they are disabled.
 */
void
cpu_intr_enable(cpu_t *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	if (!cpu_intr_on(cp)) {
		cpu_enable_intr(cp);
	}
}

/*
 * cpu_intr_disable - redirect I/O interrupts targetted at this CPU.
 *
 * semantics: We check the count of CPUs that are accepting
 * interrupts, because it's stupid to take the last CPU out
 * of I/O interrupt participation. This also permits the
 * p_online syscall to fail gracefully in uniprocessor configurations
 * without having to perform any special platform-specific operations.
 */
int
cpu_intr_disable(cpu_t *cp)
{
	int	e = EBUSY;

	ASSERT(MUTEX_HELD(&cpu_lock));
	if ((cpu_intr_count(cp) > 1) && (cpu_intr_next(cp) != NULL)) {
		if (cpu_intr_on(cp)) {
			/*
			 * Juggle away cyclics, but don't fail if we don't
			 * manage to juggle all of them away; we want to allow
			 * CPU-bound cyclics to continue to fire on the
			 * sheltered CPU.
			 */
			(void) cyclic_juggle(cp);
			e = cpu_disable_intr(cp);
		}
	}
	return (e);
}

/*
 * update the pi_state of this CPU.
 */
void
cpu_setstate(cpu_t *cp, int state)
{
	cp->cpu_type_info.pi_state = state;
	cp->cpu_state_begin = hrestime.tv_sec;
}

/*
 * determine whether this CPU is scheduling threads.
 */
int
cpu_up(cpu_t *cp)
{
	int	state;

	state = cpu_status(cp);
	return ((state == P_ONLINE) || (state == P_NOINTR));
}

/*
 * determine whether this CPU is off-line.
 */
int
cpu_down(cpu_t *cp)
{
	return (cpu_status(cp) == P_OFFLINE);
}
