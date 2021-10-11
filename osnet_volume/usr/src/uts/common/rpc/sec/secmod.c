/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)secmod.c	1.11	99/07/14 SMI"	/* SVr4.0 1.7	*/

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/errno.h>

char _depends_on[] = "strmod/rpcmod misc/tlimod";

/*
 * Module linkage information for the kernel.
 */

static struct modlmisc modlmisc = {
	&mod_miscops, "kernel RPC security module."
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modlmisc,
	NULL
};

extern void sec_subrinit();
extern void sec_subrfini();
extern void svcauthdes_init();
extern void svcauthdes_fini();

_init()
{
	int retval = 0;

	sec_subrinit();
	svcauthdes_init();

	if ((retval = mod_install(&modlinkage)) != 0) {
		/*
		 * Failed to load module, cleanup previous
		 * initialization work.
		 */
		sec_subrfini();
		svcauthdes_fini();
	}

	return (retval);
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
