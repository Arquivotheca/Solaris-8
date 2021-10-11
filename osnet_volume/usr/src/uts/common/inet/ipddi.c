/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ipddi.c	1.40	99/10/21 SMI"

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
#include <netinet/ip6.h>
#include <inet/ip.h>

/*
 * This is really ip.  The minor number of 0 indicates that it's
 * IPv4.  (note: this is the only way to determine the ip version
 * since all other information between all the ip pseudo devices
 * is shared)
 */

#define	D_SD_COMMENT "IP Streams device"
#define	D_SD_INFO ipinfo
#define	D_SD_NAME "ip"
#define	D_SD_OPS_NAME ip_ops
/*
 * D_SD_FLAGS in the other tcp/ip modules have to these D_SD_FLAGS since they
 * are effectively clones of the ip driver with their module autopushed.
 */
#define	D_SD_FLAGS (D_MP|D_MTPERMOD|D_MTPUTSHARED|_D_MTOCSHARED| \
    _D_QNEXTLESS|_D_MTCBSHARED)
#define	D_SM_COMMENT "IP Streams module"
#define	D_SM_INFO ipinfo
#define	D_SM_NAME "ip"
#define	D_SM_OPS_NAME ip_mops
#define	D_SM_FLAGS (D_MP|D_MTPERMOD|D_MTPUTSHARED|_D_MTOCSHARED|_D_MTCBSHARED)

extern	void	ip_ddi_init(void);
extern	void	ip_ddi_destroy(void);

extern struct mod_ops mod_driverops;
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
	(void *) &modlstrmod,
	(void *) &modldrv,
	NULL
};

_init()
{
	int ret;

	/*
	 * Note: After mod_install succeeds, another thread can enter
	 * therefore all initialization is done before it an any
	 * de-initialization needed done if it fails.
	 */
	ip_ddi_init();
	ret = mod_install(&modlinkage);
	if (ret != 0) {
		ip_ddi_destroy();
	}
	return (ret);
}

int
_fini()
{
	int ret;

	ret = mod_remove(&modlinkage);
	if (ret == 0) {
		ip_ddi_destroy();
	}
	return (ret);
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

static dev_info_t	*_mi_driver_dev_info;

/* ARGSUSED */
static int
_mi_driver_attach(devi, cmd)
	dev_info_t	*devi;
	ddi_attach_cmd_t cmd;
{
	_mi_driver_dev_info = devi;

	/* create with minor of 0 so ip knows it's IPv4 */
	if (ddi_create_minor_node(devi, D_SD_NAME, S_IFCHR,
	    IPV4_MINOR, DDI_PSEUDO, 0) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (-1);
	}

	return (DDI_SUCCESS);
}
/* ARGSUSED */
static int
_mi_driver_detach(devi, cmd)
	dev_info_t	 *devi;
	ddi_detach_cmd_t cmd;
{
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
_mi_driver_identify(devi)
	dev_info_t *devi;
{
	if (strcmp(ddi_get_name(devi), D_SD_NAME) == 0)
		return (DDI_IDENTIFIED);
	return (DDI_NOT_IDENTIFIED);
}
