/*
 * Copyright (c) 1996-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)platmod.c	1.7	99/10/22 SMI"

#include <sys/cpuvar.h>
#include <sys/systm.h>
#include <sys/platform_module.h>
#include <sys/errno.h>

int
set_platform_tsb_spares()
{
	return (0);
}

void
set_platform_defaults(void)
{
}

void
load_platform_drivers(void)
{
}

/*ARGSUSED*/
int
plat_cpu_poweron(struct cpu *cp)
{
	return (ENOTSUP);	/* not supported on this platform */
}

/*ARGSUSED*/
int
plat_cpu_poweroff(struct cpu *cp)
{
	return (ENOTSUP);	/* not supported on this platform */
}

/*ARGSUSED*/
void
plat_freelist_process(int mnode)
{
}

/*
 * No platform pm drivers on this platform
 */
char *platform_pm_module_list[] = {
	(char *)0
};

/*ARGSUSED*/
void
plat_tod_fault(enum tod_fault_type tod_bad)
{
}
