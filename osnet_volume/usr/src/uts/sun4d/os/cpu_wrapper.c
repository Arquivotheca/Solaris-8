/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ident	"@(#)cpu_wrapper.c	1.7	93/09/11 SMI"

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>
#include <sys/errno.h>

extern struct mod_ops mod_driverops;
extern struct dev_ops cpu_ops;		/* the actual driver */

static struct modldrv modldrv = {
	&mod_driverops,			/* modops */
	"SPARC Processor Driver",	/* linkinfo */
	&cpu_ops,			/* dev_ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_fini(void)
{
	return (EBUSY);
}

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

struct mod_ops cpu_module = {
	_init,			/* install */
	_fini,			/* remove */
	_info			/* status */
};
