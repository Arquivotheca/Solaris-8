/*
 * Copyright (c) 1990-1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pseudonex.c	1.21	98/02/14 SMI"	/* SVr4 5.0 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/ddi_impldefs.h>
#include <sys/autoconf.h>
#include <sys/systm.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

/*
 * #define PSEUDONEX_DEBUG 1
 */

/*
 * Config information
 */

static ddi_intrspec_t
pseudonex_get_intrspec(dev_info_t *, dev_info_t *, u_int);

static int
pseudonex_add_intrspec(dev_info_t *, dev_info_t *, ddi_intrspec_t,
	ddi_iblock_cookie_t *, ddi_idevice_cookie_t *, u_int (*)(caddr_t),
	caddr_t, int);

static void
pseudonex_remove_intrspec(dev_info_t *, dev_info_t *,
	ddi_intrspec_t, ddi_iblock_cookie_t);

static int
pseudonex_ctl(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);

static struct bus_ops pseudonex_bus_ops = {
	BUSO_REV,
	nullbusmap,
	pseudonex_get_intrspec,		/* NO OP */
	pseudonex_add_intrspec,		/* NO OP */
	pseudonex_remove_intrspec,	/* NO OP */
	i_ddi_map_fault,
	ddi_no_dma_map,
	ddi_no_dma_allochdl,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	pseudonex_ctl,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0	/* (*bus_post_event)();		*/
};

static int pseudonex_identify(dev_info_t *devi);
static int pseudonex_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int pseudonex_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);

static struct dev_ops pseudo_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	pseudonex_identify,	/* identify */
	nulldev,		/* probe */
	pseudonex_attach,	/* attach */
	pseudonex_detach,	/* detach */
	nodev,			/* reset */
	(struct cb_ops *)0,	/* driver operations */
	&pseudonex_bus_ops,	/* bus operations */
	nulldev			/* power */

};

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a pseudo driver */
	"nexus driver for 'pseudo'",
	&pseudo_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
pseudonex_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), DEVI_PSEUDO_NEXNAME) == 0) {
		return (DDI_IDENTIFIED);
	}
	return (DDI_NOT_IDENTIFIED);
}

/*ARGSUSED*/
static int
pseudonex_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		return (DDI_SUCCESS);

	case DDI_RESUME:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/*ARGSUSED*/
static int
pseudonex_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_DETACH:
		return (DDI_FAILURE);

	case DDI_SUSPEND:
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/*
 * pseudonex_get_intrspec: pseudonex convert an interrupt number to an
 *			   interrupt. NO OP for pseudo drivers.
 */
/*ARGSUSED*/
static ddi_intrspec_t
pseudonex_get_intrspec(dev_info_t *dip, dev_info_t *rdip, u_int inumber)
{
	return ((ddi_intrspec_t)0);
}

/*
 * pseudonex_add_intrspec:
 *
 *	Add an interrupt specification.
 *	NO OP for pseudo drivers.
 */
/*ARGSUSED*/
static int
pseudonex_add_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	u_int (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg, int kind)
{
	return (DDI_FAILURE);
}

/*
 * pseudonex_remove_intrspec:	remove an interrupt specification.
 *				NO OP for the pseudo drivers.
 */
/*ARGSUSED*/
static void
pseudonex_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookie)
{
}

static int
pseudonex_ctl(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	char	name[12];	/* enough for a decimal integer */
	char	*childname;
	ddi_prop_t	*propp;
	int		instance;
	dev_info_t	*tdip;
	kmutex_t	*dmp;

	switch (ctlop) {
	case DDI_CTLOPS_REPORTDEV:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		cmn_err(CE_CONT, "?pseudo-device: %s%d\n",
		    ddi_driver_name(rdip), ddi_get_instance(rdip));
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INITCHILD:
	{
		dev_info_t *child = (dev_info_t *)arg;

		/*
		 * Look up the "instance" property. If it does not exist,
		 * use instance 0.
		 */
		childname = ddi_get_name(child);
		for (propp = DEVI(child)->devi_sys_prop_ptr; propp != NULL;
		    propp = propp->prop_next) {
			if (strcmp(propp->prop_name, "instance") == 0)
				break;
		}
		if (propp == NULL) {
			/* not found */
			instance = 0;
		} else {
			if (propp->prop_len != sizeof (int)) {
				cmn_err(CE_WARN,
				    "\"instance\" property on node \"%s\" "
				    "is not an integer -- using zero.",
				    childname);
				instance = 0;
			} else
				instance = *(int *)propp->prop_val;
		}
		/*
		 * Determine if this instance is already in use.
		 */
		dmp = &devnamesp[ddi_name_to_major(childname)].dn_lock;
		LOCK_DEV_OPS(dmp);
		for (tdip = devnamesp[ddi_name_to_major(childname)].dn_head;
		    tdip != NULL; tdip = ddi_get_next(tdip)) {
			/* is this the current node? */
			if (tdip == child)
				continue;
			/* is this a duplicate instance? */
			if (instance == ddi_get_instance(tdip)) {
				cmn_err(CE_WARN,
				    "Duplicate instance %d of node \"%s\" "
					"ignored.",
				    instance, childname);
				UNLOCK_DEV_OPS(dmp);
				return (DDI_FAILURE);
			}
		}
		UNLOCK_DEV_OPS(dmp);

		/*
		 * Attach the instance number to the node. This allows
		 * us to have multiple instances of the same pseudo
		 * device, they will be named 'device@instance'. If this
		 * breaks programs, we may need to special-case instance 0
		 * into 'device'. Ick. devlinks appears to handle the
		 * new names ok, so if only names in /dev are used
		 * this may not be necessary.
		 */
		(void) sprintf(name, "%d", instance);
		DEVI(child)->devi_instance = instance;
		ddi_set_name_addr(child, name);
		return (DDI_SUCCESS);
	}

	case DDI_CTLOPS_UNINITCHILD:
	{
		dev_info_t *child = (dev_info_t *)arg;

		ddi_set_name_addr(child, NULL);
		return (DDI_SUCCESS);
	}

	/*
	 * These ops correspond to functions that "shouldn't" be called
	 * by a pseudo driver.  So we whinge when we're called.
	 */
	case DDI_CTLOPS_DMAPMAPC:
	case DDI_CTLOPS_REPORTINT:
	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
	case DDI_CTLOPS_NINTRS:
	case DDI_CTLOPS_SIDDEV:
	case DDI_CTLOPS_SLAVEONLY:
	case DDI_CTLOPS_AFFINITY:
	case DDI_CTLOPS_IOMIN:
	case DDI_CTLOPS_POKE_INIT:
	case DDI_CTLOPS_POKE_FLUSH:
	case DDI_CTLOPS_POKE_FINI:
	case DDI_CTLOPS_INTR_HILEVEL:
	case DDI_CTLOPS_XLATE_INTRS:
		cmn_err(CE_CONT, "%s%d: invalid op (%d) from %s%d\n",
			ddi_get_name(dip), ddi_get_instance(dip),
			ctlop, ddi_get_name(rdip), ddi_get_instance(rdip));
		return (DDI_FAILURE);

	/*
	 * Everything else (e.g. PTOB/BTOP/BTOPR requests) we pass up
	 */
	default:
		return (ddi_ctlops(dip, rdip, ctlop, arg, result));
	}
}
