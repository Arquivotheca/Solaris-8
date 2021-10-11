/*
 * Copyright (c) 1992, 1994, 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)p_online.c	1.12	98/08/12 SMI"

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


/*
 * p_online(2) - get/change processor operational status.
 */
int
p_online(processorid_t cpun, int flag)
{
	cpu_t	*cp;
	int	status;
	int	e = 0;

	/*
	 * Try to get a pointer to the requested CPU
	 * structure.
	 */
	mutex_enter(&cpu_lock);		/* protects CPU states */
	if ((cp = cpu_get(cpun)) == NULL) {
		mutex_exit(&cpu_lock);
		return (set_errno(EINVAL));
	}

	status = cpu_status(cp);	/* get the processor status */

	/*
	 * Perform credentials check.
	 */
	switch (flag) {
	case P_STATUS:
		goto out;

	case P_ONLINE:
	case P_OFFLINE:
	case P_NOINTR:
		if (!suser(CRED())) {
			status = set_errno(EPERM);
			goto out;
		}
		break;

	default:
		status = set_errno(EINVAL);
		goto out;
	}

	/*
	 * return if the CPU is already in the desired new state.
	 */
	if (status == flag) {
		goto out;
	}

	ASSERT(flag == P_ONLINE || flag == P_OFFLINE || flag == P_NOINTR);

	switch (flag) {
	case P_ONLINE:
		switch (status) {
		case P_POWEROFF:
			/*
			 * if CPU is powered off, power it on.
			 */
			if (e = cpu_poweron(cp)) {
				status = set_errno(e);
				goto out;
			}
			ASSERT(cpu_status(cp) == P_OFFLINE);
			/* FALLTHROUGH */

		case P_OFFLINE:
			if (e = cpu_online(cp)) {
				status = set_errno(e);
				goto out;
			}
			break;

		case P_NOINTR:
			cpu_intr_enable(cp);
			cpu_setstate(cp, P_ONLINE);
			break;
		}
		break;

	case P_OFFLINE:
		switch (status) {
		case P_NOINTR:
			/*
			 * Before we bring it offline, we first
			 * enable I/O interrupts.
			 */
			cpu_intr_enable(cp);
			cpu_setstate(cp, P_ONLINE);
			/* FALLTHROUGH */
		case P_ONLINE:
			/*
			 * CPU is online. Take if offline.
			 */
			if (e = cpu_offline(cp)) {
				status = set_errno(e);
				goto out;
			}
			break;
		case P_POWEROFF:
			/*
			 * if CPU is powered off, power it on.
			 */
			if (e = cpu_poweron(cp)) {
				status = set_errno(e);
				goto out;
			}
			ASSERT(cpu_status(cp) == P_OFFLINE);
			break;
		}
		break;

	case P_NOINTR:
		switch (status) {
		case P_POWEROFF:
			/*
			 * if CPU is powered off, power it on.
			 */
			if (e = cpu_poweron(cp)) {
				status = set_errno(e);
				goto out;
			}
			ASSERT(cpu_status(cp) == P_OFFLINE);
			/* FALLTHROUGH */
		case P_OFFLINE:
			/*
			 * First, bring the CPU online.
			 */
			if (e = cpu_online(cp)) {
				status = set_errno(e);
				goto out;
			}
			/* FALLTHROUGH */
		case P_ONLINE:
			/*
			 * CPU is now online. Disable interrupts.
			 */
			(void) cpu_intr_disable(cp);
			/*
			 * check whether the cpu_intr_disable operation
			 * succeeded. If not, we should return an error.
			 */
			if (cpu_intr_on(cp)) {
				status = set_errno(EBUSY);
				goto out;
			}
			cpu_setstate(cp, P_NOINTR);
			break;
		}

	}
out:
	mutex_exit(&cpu_lock);
	return (status);
}
