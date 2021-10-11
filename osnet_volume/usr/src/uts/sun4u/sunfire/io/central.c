/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)central.c	1.12	99/04/21 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_subrdefs.h>
#include <sys/obpdefs.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/autoconf.h>
#include <sys/modctl.h>

/*
 * module central.c
 *
 * This module is a nexus driver designed to support the fhc nexus driver
 * and all children below it. This driver does not handle any of the
 * DDI functions passed up to it by the fhc driver, but instead allows
 * them to bubble up to the root node. A consequence of this is that
 * the maintainer of this code must watch for changes in the sun4u
 * rootnexus driver to make sure they do not break this driver or any
 * of its children.
 */

/*
 * Function Prototypes
 */
static int
central_identify(dev_info_t *devi);

static int
central_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);

static int
central_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);

static int
central_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);

/*
 * Configuration Data Structures
 */
static struct bus_ops central_bus_ops = {
	BUSO_REV,
	ddi_bus_map,		/* map */
	i_ddi_get_intrspec,	/* get_intrspec */
	i_ddi_add_intrspec,	/* add_intrspec */
	i_ddi_remove_intrspec,	/* remove_intrspec */
	i_ddi_map_fault,	/* map_fault */
	ddi_no_dma_map,		/* dma_map */
	ddi_no_dma_allochdl,
	ddi_no_dma_freehdl,
	ddi_no_dma_bindhdl,
	ddi_no_dma_unbindhdl,
	ddi_no_dma_flush,
	ddi_no_dma_win,
	ddi_dma_mctl,		/* dma_ctl */
	central_ctlops,		/* ctl */
	ddi_bus_prop_op,	/* prop_op */
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0,	/* (*bus_post_event)();		*/
	i_ddi_intr_ctlops
};

static struct dev_ops central_ops = {
	DEVO_REV,		/* rev */
	0,			/* refcnt */
	ddi_no_info,		/* getinfo */
	central_identify,	/* identify */
	nulldev,		/* probe */
	central_attach,		/* attach */
	central_detach,		/* detach */
	nulldev,		/* reset */
	(struct cb_ops *)0,	/* cb_ops */
	&central_bus_ops,	/* bus_ops */
	nulldev			/* power */
};

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	"Central Nexus",	/* Name of module. */
	&central_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,		/* rev */
	(void *)&modldrv,
	NULL
};

/*
 * These are the module initialization routines.
 */

int
_init(void)
{
	return (mod_install(&modlinkage));
}   

int
_fini(void)
{
	int error;

	if ((error = mod_remove(&modlinkage)) != 0)
		return (error);

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
central_identify(dev_info_t *devi)
{
	char *name = ddi_get_name(devi);
	int rc = DDI_NOT_IDENTIFIED;

	if (strcmp(name, "central") == 0) {
		rc = DDI_IDENTIFIED;
	}

	return (rc);
}

static int
central_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	/* nothing to suspend/resume here */
	(void) ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
		"pm-hardware-state", (caddr_t)"no-suspend-resume",
		strlen("no-suspend-resume") + 1);

	ddi_report_dev(devi);
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
central_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_SUSPEND:
	case DDI_DETACH:
	default:
		return (DDI_FAILURE);
	}
}


static int
central_ctlops(dev_info_t *dip, dev_info_t *rdip, ddi_ctl_enum_t op,
    void *arg, void *result)
{
	int rval;

	if ((rval = ddi_ctlops(dip, rdip, op, arg, result)) != DDI_SUCCESS)
		return (rval);

	switch (op) {
	case DDI_CTLOPS_INITCHILD: {
		char name[MAXNAMELEN];
		struct regspec *rp = sparc_pd_getreg(rdip, 0);

		(void) sprintf(name, "%x,%x", (rp->regspec_bustype >> 1) & 0x1f,
			rp->regspec_addr);

		ddi_set_name_addr(rdip, name);
		break;

	}
	}

	return (rval);
}
