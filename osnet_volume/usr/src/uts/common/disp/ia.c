
/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)ia.c	1.24	99/07/29 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/class.h>
#include <sys/errno.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern pri_t ia_init();

static sclass_t csw = {
	"IA",
	ia_init,
	0
};

extern struct mod_ops mod_schedops;

/*
 * Module linkage information for the kernel.
 */
static struct modlsched modlsched = {
	&mod_schedops, "interactive scheduling class", &csw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlsched, NULL
};

#ifndef lint
static char _depends_on[] = "sched/TS";
#endif

_init()
{
	int error;

	if ((error = mod_install(&modlinkage)) == 0) {
		return (0);
	} else {
		return (error);
	}
}


_fini()
{
	return (EBUSY);
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

/* Rest of IA code is in ts.c. */
