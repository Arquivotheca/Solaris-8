
#pragma	ident	"@(#)ipd.c	1.9	98/06/11	Copyright 1992	SMI"

/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <sys/strlog.h>
#include <sys/conf.h>
#include <sys/sunddi.h>
#include <sys/ksynch.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/kstat.h>
#ifdef ISERE_TREE
#include <ipd_ioctl.h>
#include <ipd_sys.h>
#include <ipd_extern.h>
#else
#include <sys/ipd_ioctl.h>
#include <sys/ipd_sys.h>
#include <sys/ipd_extern.h>
#endif


/*
 * IP/Dialup point-to-multipoint pseudo device driver
 *
 * Please refer to the ISERE architecture document for more
 * details.
 */

static int	ipd_info(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int	ipd_identify(dev_info_t *);
static int	ipd_attach(dev_info_t *, ddi_attach_cmd_t);
static int	ipd_detach(dev_info_t *, ddi_detach_cmd_t);

extern struct streamtab ipd_tab;

DDI_DEFINE_STREAM_OPS(ipd_ops, \
	ipd_identify, nulldev, ipd_attach, ipd_detach, nodev, \
	ipd_info, D_NEW | D_MP, &ipd_tab);


char _depends_on[] = "drv/ipdcm";
extern int	ipd_debug;

#ifndef BUILD_STATIC

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern	struct mod_ops	mod_driverops;

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,
	"IP/Dialup mtp interface v1.9",
	&ipd_ops,
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

_init(void)
{
	return (mod_install(&modlinkage));
}

_fini(void)
{
	return (mod_remove(&modlinkage));
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
#endif BUILD_STATIC


/*
 * list of ipd_softc structures - one per pseudo device.
 */
extern struct ipd_softc *ipd_ifs;


/*
 * list of upper streams
 */
extern struct ipd_str	*ipd_strup;


/*
 * multipoint device information pointer
 */
extern dev_info_t	*ipd_mtp_dip;


/*
 * Identify multipoint pseudo device.
 */
static int
ipd_identify(dev_info_t *dip)
{
	IPD_DDIDEBUG("ipd_identify: called\n");

	if (strcmp(ddi_get_name(dip), IPD_MTP_NAME) == 0) {
		return (DDI_IDENTIFIED);
	} else {
		return (DDI_NOT_IDENTIFIED);
	}
}


/*
 * ipd_attach()
 *
 * Attach a point-to-multipoint interface to the system
 */
static int
ipd_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	struct ipd_softc	*ifp;

	IPD_DDIDEBUG("ipd_attach: called\n");

	if (cmd != DDI_ATTACH) {
		return (DDI_FAILURE);
	}

	ipd_init();

	if (ddi_create_minor_node(dip, IPD_MTP_NAME, S_IFCHR,
		ddi_get_instance(dip), DDI_PSEUDO, CLONE_DEV) == DDI_FAILURE) {

		ddi_remove_minor_node(dip, NULL);
		return (DDI_FAILURE);
	}

	ipd_mtp_dip = dip;

	for (ifp = ipd_ifs; ifp; ifp = ifp->if_next) {
		if (ifp->if_type == IPD_MTP) {

			/*
			 * musn't already be attached
			 */
			ASSERT(ifp->if_dip == NULL);
			ifp->if_dip = ipd_mtp_dip;
		}
	}
	IPD_DDIDEBUG("ipd_attach: DDI_SUCCESS\n");

	return (DDI_SUCCESS);

}

/*
 * ipd_detach()
 *
 * Detach an interface to the system
 */
static int
ipd_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	struct ipd_softc	*ifp;

	IPD_DDIDEBUG("ipd_detach: called\n");

	if (cmd != DDI_DETACH) {
		return (DDI_FAILURE);
	}

	ddi_remove_minor_node(dip, NULL);

	ipd_mtp_dip = NULL;

	for (ifp = ipd_ifs; ifp; ifp = ifp->if_next) {
		if (ifp->if_type == IPD_MTP) {
			ASSERT(ifp->if_dip == dip);
			ifp->if_dip = NULL;
		}
	}

	return (DDI_SUCCESS);
}

/*
 * ipd_info()
 *
 * Translate "dev_t" to a pointer to the associated "dev_info_t".
 */
/* ARGSUSED */
static int
ipd_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t			dev = (dev_t)arg;
	struct ipd_str		*mtp;
	minor_t			instance;
	int			rc;

	IPD_DDIDEBUG("ipd_info: called\n");

	instance = getminor(dev);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		dip = NULL;
		for (mtp = ipd_strup; mtp; mtp = mtp->st_next) {

			/*
			 * looking here for the dip corresponding to the
			 * specified mtp minor device
			 */
			if (mtp->st_type != IPD_MTP) {
				continue;
			}

			if (mtp->st_minor == instance) {
				break;
			}
		}
		if (mtp && mtp->st_ifp) {
			dip = mtp->st_ifp->if_dip;
		}

		if (dip) {
			*result = (void *) dip;
			rc = DDI_SUCCESS;
		} else
			rc = DDI_FAILURE;
		break;

	case DDI_INFO_DEVT2INSTANCE:

		*result = (void *)instance;
		rc = DDI_SUCCESS;
		break;

	default:
		rc = DDI_FAILURE;
		break;
	}

	return (rc);
}
