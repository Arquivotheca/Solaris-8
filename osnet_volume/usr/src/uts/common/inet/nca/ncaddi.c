/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ncaddi.c	1.1	99/08/06 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stream.h>
#include <sys/modctl.h>
#include <sys/sunddi.h>
#include <sys/stat.h>

#define	D_SD_CHAR_MAJOR 97
#define	D_SD_COMMENT "NCA Streams device"
#define	D_SD_INFO ncainfo
#define	D_SD_NAME "nca"
#define	D_SD_OPS_NAME nca_ops

#define	D_SD_FLAGS (D_MP)

#define	D_SM_COMMENT "NCA Streams module"
#define	D_SM_INFO ncainfo
#define	D_SM_NAME "nca"
#define	D_SM_OPS_NAME nca_mops
#define	D_SM_FLAGS (D_MP)

extern int	nulldev();
extern int	nodev();

extern void    nca_ddi_init(void);
extern void    nca_ddi_fini(void);

#ifdef D_SD_INFO
extern struct streamtab D_SD_INFO;

static int _mi_driver_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
			void **result);
static int _mi_driver_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int _mi_driver_identify(dev_info_t *devi);
static int _mi_driver_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);

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
	(int)D_SD_FLAGS		/* cb_flag */
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
	(struct bus_ops *)NULL	/* devo_bus_ops */
};

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module */
	D_SD_COMMENT,
	&D_SD_OPS_NAME,		/* driver ops */
};
#endif

#ifdef D_SM_INFO
extern struct streamtab D_SM_INFO;

static struct fmodsw _mi_module_fmodsw = {
	D_SM_NAME,
	&D_SM_INFO,
	D_SM_FLAGS
};

static struct modlstrmod modlstrmod = {
	&mod_strmodops, D_SM_COMMENT, &_mi_module_fmodsw
};
#endif

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlstrmod,
	&modldrv,
	NULL
};

/* Need access to TCP/IP mibs, so ... */
char _depends_on[] = "drv/ip drv/tcp";

int
_init()
{
	int	error;

	/* Initialize before any other thread can enter */
	nca_ddi_init();
	error = mod_install(&modlinkage);
	if (error != 0) {
		/* Failure of some sort, cleanup */
		nca_ddi_fini();
	}
	return (error);
}

int
_fini()
{
	int	error;

	error = (mod_remove(&modlinkage));
	if (error == 0) {
		/* No more threads can enter, so cleanup */
		nca_ddi_fini();
	}
	return (error);
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

#ifdef D_SD_INFO
static dev_info_t	*_mi_driver_dev_info;

static int
_mi_driver_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);
	_mi_driver_dev_info = devi;
	if (ddi_create_minor_node(devi, D_SD_NAME, S_IFCHR, 0, NULL,
	    CLONE_DEV) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

static int
_mi_driver_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);
	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

static int
/*ARGSUSED*/
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
_mi_driver_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), D_SD_NAME) == 0)
		return (DDI_IDENTIFIED);
	return (DDI_NOT_IDENTIFIED);
}
#endif
