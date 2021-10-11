/*
 * Copyright (c) 1992, 1997, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tcpddi.c	1.43	99/10/21 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stream.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/kmem.h>
#include <inet/common.h>
#include <netinet/ip6.h>
#include <inet/ip.h>

#define	D_SD_COMMENT "TCP Streams device"
#define	D_SD_INFO tcpinfo
#define	D_SD_NAME "tcp"
#define	D_SD_OPS_NAME tcp_ops
/*
 * D_SD_FLAGS have to match D_SD_FLAGS in ipddi.c since this driver is
 * effectively a clone of the ip driver with the tcp module autopushed.
 */
#define	D_SD_FLAGS (D_MP|D_MTPERMOD|D_MTPUTSHARED|_D_MTOCSHARED| \
    _D_QNEXTLESS|_D_MTCBSHARED)
#define	D_SM_COMMENT "TCP Streams module"
#define	D_SM_INFO tcpinfo
#define	D_SM_NAME "tcp"
#define	D_SM_OPS_NAME tcp_mops
#define	D_SM_FLAGS (D_MP|D_MTQPAIR|D_SYNCSTR|_D_QNEXTLESS)

extern void    tcp_ddi_init(void);
extern void    tcp_ddi_destroy(void);

extern struct streamtab D_SD_INFO;

static int _mi_driver_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
			void **result);
static int	_mi_driver_attach(dev_info_t *devi,  ddi_attach_cmd_t cmd);
static int	_mi_driver_identify(dev_info_t *devi);
static  int	_mi_driver_detach(dev_info_t *devi,  ddi_detach_cmd_t cmd);

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
	&mod_driverops,	 /* Type of module */
	D_SD_COMMENT,
	&D_SD_OPS_NAME,		/* driver ops */
};

extern struct streamtab D_SM_INFO;

static struct fmodsw _mi_module_fmodsw = {
	D_SM_NAME,
	&D_SM_INFO,
	D_SM_FLAGS
};

static struct modlstrmod modlstrmod = {
	&mod_strmodops, D_SM_COMMENT, &_mi_module_fmodsw
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlstrmod,
	&modldrv,
	NULL
};

int
_init(void)
{
	extern struct streamtab ipinfo;
	int	error;

	/* Convert driver to ip */
	_mi_driver_ops.cb_str = &ipinfo;
	/*
	 * Note: After mod_install succeeds, another thread can enter
	 * therefore all initialization is done before it an any
	 * de-initialization needed done if it fails.
	 */
	tcp_ddi_init();
	error = mod_install(&modlinkage);
	if (error != 0) {
		tcp_ddi_destroy();
	}
	return (error);
}

int
_fini(void)
{
	int	error;

	error = mod_remove(&modlinkage);
	if (error != 0)
		return (error);
	tcp_ddi_destroy();
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static dev_info_t	*_mi_driver_dev_info;

static int
_mi_driver_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);
	_mi_driver_dev_info = devi;

	/* create with minor of 0 so ip can tell it's IPv4 */
	if (ddi_create_minor_node(devi, D_SD_NAME, S_IFCHR,
	    IPV4_MINOR, DDI_PSEUDO, 0) == DDI_FAILURE) {
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
		*result = NULL;
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
	if (strcmp((char *)ddi_get_name(devi), D_SD_NAME) == 0)
		return (DDI_IDENTIFIED);
	return (DDI_NOT_IDENTIFIED);
}
