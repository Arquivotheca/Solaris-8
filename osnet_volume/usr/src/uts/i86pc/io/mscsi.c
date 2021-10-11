/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mscsi.c	1.2	99/03/04 SMI"

/*
 *	HBA to MSCSI BUS nexus driver
 */

/*
 * Many newer hba drivers must support multiple scsi-busses. This
 * simple, generic nexus driver can be used to create separate instances
 * for each scsi-bus, using the following procedure:
 *
 * 1) hba-parent (mscsi bus driver child attaches to hba driver parent)
 * -------------
 *
 * a) Set the class of the parent hba driver to "mscsi" (note that
 *       PSARC approval of the mscsi class must precede this usage).
 * b) Set the class of the mscsi bus driver to "scsi"
 * c) Place entries in mscsi.conf for each scsi-bus
 *
 * name="mscsi" class="mscsi" reg=N,0,0 mscsi-bus=N
 *
 * where N is the required scsi-bus number.
 *
 * 4) Place special mscsi_hba_* routines in the parent hba driver
 * to properly initialize these mscsi_bus nodes to passthru
 * SCSA requests. The following properties control the operation
 * of the mscsi bus nexus driver.
 *
 * MSCSI_BUSPROP	When set on devinfo node, indicates which
 *			scsi bus is attaching to an hba-parent.
 *
 * MSCSI_CALLPROP	When set on parent devinfo node, indicates the
 *			hba-parent requests callbacks through parent
 *			dev_ops entries	to perform initialization, etc.
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/debug.h>
#include <sys/modctl.h>

#include <sys/dktp/hba.h>
#include <sys/dktp/mscsi.h>

char _depends_on[] = "misc/scsi";

int mscsi_forceload = 0;

static int mscsi_identify(dev_info_t *devi);
static int mscsi_probe(dev_info_t *);
static int mscsi_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int mscsi_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);
static int mscsi_reset(dev_info_t *devi, ddi_reset_cmd_t cmd);
static int mscsi_info(dev_info_t *dip, ddi_info_cmd_t infocmd,
	void *arg, void **result);

struct dev_ops mscsi_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt  */
	mscsi_info,		/* info */
	mscsi_identify,		/* identify */
	mscsi_probe,		/* probe */
	mscsi_attach,		/* attach */
	mscsi_detach,		/* detach */
	mscsi_reset,		/* reset */
	(struct cb_ops *)0,	/* driver operations */
	NULL,			/* bus operations */
	NULL			/* power operations */
};

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module */
	"scsi mscsi_bus nexus driver",
	&mscsi_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
_init(void)
{
	int status;

	if ((status = scsi_hba_init(&modlinkage)) != 0)
		return (status);

	if ((status = mod_install(&modlinkage)) != 0) {
		scsi_hba_fini(&modlinkage);
		return (status);
	}
	return (status);
}

int
_fini(void)
{
	int status;

	if ((status = mod_remove(&modlinkage)) == 0)
		scsi_hba_fini(&modlinkage);

	return (status);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*ARGSUSED*/
static int
mscsi_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
static int
mscsi_identify(dev_info_t *dip)
{
	return (DDI_IDENTIFIED);
}

static int
mscsi_callback(register dev_info_t *devi)
{
	int	mscsi_call;
	int	proplen;

	/*
	 * Check if MSCSI_CALLPROP property is set on parent.
	 * And if so prepare to call parent ops entries.
	 */
	proplen = sizeof (mscsi_call);
	if (ddi_getlongprop_buf(DDI_DEV_T_ANY, devi,
	    DDI_PROP_DONTPASS, MSCSI_CALLPROP, (caddr_t)&mscsi_call,
	    &proplen) != DDI_PROP_SUCCESS)
		return (DDI_FAILURE);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
mscsi_probe(register dev_info_t *devi)
{
	/* check for forced probe failure */
	if (mscsi_forceload < 0)
		return (DDI_PROBE_FAILURE);

	return (DDI_PROBE_SUCCESS);
}

/*ARGSUSED*/
static int
mscsi_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	dev_info_t *pdevi = ddi_get_parent(devi);

	/*
	 * Callback parent dev_ops if parent requests it.
	 */
	if (mscsi_callback(pdevi) == DDI_SUCCESS)
		return ((DEVI(pdevi)->devi_ops->devo_attach)(devi, cmd));

	(void) ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
		"mbus_type", (caddr_t)MSCSI_NAME, strlen(MSCSI_NAME)+1);
	ddi_report_dev(devi);
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
mscsi_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	dev_info_t *pdevi = ddi_get_parent(devi);

	if (mscsi_callback(pdevi) == DDI_SUCCESS)
		return ((DEVI(pdevi)->devi_ops->devo_detach)(devi, cmd));

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
mscsi_reset(dev_info_t *devi, ddi_reset_cmd_t cmd)
{
	dev_info_t *pdevi = ddi_get_parent(devi);

	if (mscsi_callback(pdevi) == DDI_SUCCESS)
		return ((DEVI(pdevi)->devi_ops->devo_reset)(devi, cmd));

	return (DDI_SUCCESS);
}
