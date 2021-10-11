
#pragma ident "@(#)ipdptp.c 1.9 98/06/11 Copyright 1992 SMI"

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
#include <sys/kstat.h>
#include <sys/socket.h>
#ifdef ISERE_TREE
#include <ipd_ioctl.h>
#include <ipd_sys.h>
#include <ipd_extern.h>
#else
#include <sys/ipd_ioctl.h>
#include <sys/ipd_sys.h>
#include <sys/ipd_extern.h>
#endif

static int		ipdptp_info(dev_info_t *, ddi_info_cmd_t, void *,
				void **);
static int		ipdptp_identify(dev_info_t *);
static int		ipdptp_attach(dev_info_t *, ddi_attach_cmd_t);
static int		ipdptp_detach(dev_info_t *, ddi_detach_cmd_t);

/*
 * IP/Dialup point-to-point pseudo device driver
 *
 * Please refer to the ISERE architecture document for more
 * details.
 */


extern struct streamtab ipdptp_tab;

DDI_DEFINE_STREAM_OPS(ipdptp_ops, \
	ipdptp_identify, nulldev, ipdptp_attach, ipdptp_detach, nodev, \
	ipdptp_info, D_NEW | D_MP, &ipdptp_tab);


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
	"IP/Dialup ptp interface v1.9",
	&ipdptp_ops,
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
 * point-to-point device information pointer
 */
extern dev_info_t	*ipd_ptp_dip;


/*
 * Identify point-to-point device.
 */
static int
ipdptp_identify(dev_info_t *dip)
{
	IPD_DDIDEBUG("ipdptp_identify: called\n");

	if (strcmp(ddi_get_name(dip), IPD_PTP_NAME) == 0) {
		return (DDI_IDENTIFIED);
	} else {
		return (DDI_NOT_IDENTIFIED);
	}
}


/*
 * ipdptp_attach()
 *
 * Attach a point-to-point interface to the system
 */
static int
ipdptp_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	struct ipd_softc	*ifp;

	IPD_DDIDEBUG("ipdptp_attach: called\n");

	if (cmd != DDI_ATTACH) {
		return (DDI_FAILURE);
	}

	ipd_init();

	if (ddi_create_minor_node(dip, IPD_PTP_NAME, S_IFCHR,
		ddi_get_instance(dip), DDI_PSEUDO, CLONE_DEV) == DDI_FAILURE) {

		ddi_remove_minor_node(dip, NULL);
		return (DDI_FAILURE);
	}

	ipd_ptp_dip = dip;

	/*
	 * attach the IP point-to-point pseudo interfaces to this dev_info node
	 */
	for (ifp = ipd_ifs; ifp; ifp = ifp->if_next) {

		if (ifp->if_type == IPD_PTP) {

			/*
			 * musn't already be attached
			 */
			ASSERT(ifp->if_dip == NULL);
			ifp->if_dip = ipd_ptp_dip;
		}
	}

	IPD_DDIDEBUG("ipdptp_attach: DDI_SUCCESS\n");

	return (DDI_SUCCESS);

}

/*
 * ipdptp_detach()
 *
 * Detach an interface to the system
 */
static int
ipdptp_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	struct ipd_softc	*ifp;

	IPD_DDIDEBUG("ipdptp_detach: called\n");

	if (cmd != DDI_DETACH) {
		return (DDI_FAILURE);
	}

	ddi_remove_minor_node(dip, NULL);

	ipd_ptp_dip = NULL;

	for (ifp = ipd_ifs; ifp; ifp = ifp->if_next) {
		if (ifp->if_type == IPD_PTP) {
			ASSERT(ifp->if_dip == dip);
			ifp->if_dip = NULL;
		}
	}

	return (DDI_SUCCESS);
}

/*
 * ipdptp_info()
 *
 * Translate "dev_t" to a pointer to the associated "dev_info_t".
 */
/* ARGSUSED */
static int
ipdptp_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t 			dev = (dev_t)arg;
	struct ipd_str		*ptp;
	minor_t			instance;
	int			rc;

	IPD_DDIDEBUG1("ipdptp_info: cmd %x\n", infocmd);

	instance = getminor(dev);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		dip = NULL;
		for (ptp = ipd_strup; ptp; ptp = ptp->st_next) {

			/*
			 * looking here for the dip corresponding to the
			 * specified ptp minor device
			 */
			if (ptp->st_type != IPD_PTP) {
				continue;
			}

			if (ptp->st_minor == instance) {
				break;
			}
		}
		if (ptp && ptp->st_ifp) {
			dip = ptp->st_ifp->if_dip;
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
