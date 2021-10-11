/*
 * Copyright (c) 1996-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)keysockddi.c 1.2	99/03/21 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stream.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/sunddi.h>
#include <sys/stat.h>
#include <sys/ddi.h>

#include <inet/common.h>
#include <inet/optcom.h>
#include <inet/keysock.h>

#define	D_SD_COMMENT "PF_KEY Key Management Socket STREAMS device"
#define	D_SD_INFO keysockinfo
#define	D_SD_NAME "keysock"
#define	D_SD_OPS_NAME keysock_ops
/* Q:  Do we wish to run this hot? */
#define	D_SD_FLAGS (D_NEW|D_MP|D_MTPERMOD|D_MTPUTSHARED)

extern  struct streamtab D_SD_INFO;

static int _mi_driver_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result);
static int _mi_driver_attach(dev_info_t *devi,  ddi_attach_cmd_t cmd);
static int _mi_driver_identify(dev_info_t *devi);
static int _mi_driver_detach(dev_info_t *devi,  ddi_detach_cmd_t cmd);

extern	int	nulldev();
extern	int	nodev();

extern boolean_t keysock_ddi_init(void);
extern void keysock_ddi_destroy(void);

static struct cb_ops _mi_driver_ops = {
	nulldev,		/* cb_open */
	nulldev,		/* cb_close */
	nodev,			/* cb_strategy */
	nodev,			/* cb_print */
	nodev,			/* cb_dump */
	nodev,			/* cb_read */
	nodev,			/* cb_write */
	nodev,			/* cb_ioctl */
	nodev,			/* cb_devmap */
	nodev,			/* cb_mmap */
	nodev,			/* cb_segmap */
	nochpoll,		/* cb_chpoll */
	ddi_prop_op,		/* cb_prop_op */
	&D_SD_INFO,		/* cb_stream */
	D_SD_FLAGS		/* cb_flag */
};

static struct dev_ops D_SD_OPS_NAME = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	_mi_driver_info,	/* devo_getinfo */
	_mi_driver_identify,	/* devo_identify */
	nulldev,		/* devo_probe */
	_mi_driver_attach,	/* devo_attach */
	_mi_driver_detach,	/* devo_detach */
	nodev,			/* devo_reset */
	&_mi_driver_ops,	/* devo_cb_ops */
	NULL			/* devo_bus_ops */
};

static struct modldrv modldrv = {
	&mod_driverops,	 /* Type of module */
	D_SD_COMMENT,
	&D_SD_OPS_NAME,		/* driver ops */
};

static struct fmodsw _mi_module_fmodsw = {
	"keysock",
	&keysockinfo,
	D_SD_FLAGS
};

static struct modlstrmod modlstrmod = {
	&mod_strmodops, "keysock module", &_mi_module_fmodsw
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlstrmod,
	&modldrv,
	NULL
};

_init()
{
	int rc;

	if (!keysock_ddi_init())
		return (ENOMEM);
	rc = mod_install(&modlinkage);
	if (rc != 0)
		keysock_ddi_destroy();
	return (rc);
}

_fini()
{
	int rc;

	rc = mod_remove(&modlinkage);
	if (rc == 0)
		keysock_ddi_destroy();
	return (rc);
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static dev_info_t *_mi_driver_dev_info;

/* ARGSUSED */
static int
_mi_driver_attach(devi, cmd)
	dev_info_t	*devi;
	ddi_attach_cmd_t cmd;
{
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);
	_mi_driver_dev_info = devi;

	/*
	 * XXX For now, create with minor number 0.  This isn't an auto-push
	 * module, so perhaps this doesn't matter.
	 */
	if (ddi_create_minor_node(devi, D_SD_NAME, S_IFCHR,
	    0, DDI_PSEUDO, 0) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
_mi_driver_detach(devi, cmd)
	dev_info_t	*devi;
	ddi_detach_cmd_t cmd;
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);
	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
_mi_driver_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result)
{
	register int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (_mi_driver_dev_info == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = (void *) _mi_driver_dev_info;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

static int
_mi_driver_identify(devi)
	dev_info_t *devi;
{
	if (strcmp((char *)ddi_get_name(devi), D_SD_NAME) == 0)
		return (DDI_IDENTIFIED);
	return (DDI_NOT_IDENTIFIED);
}
