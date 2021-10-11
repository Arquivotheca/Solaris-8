/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_devctl.c	1.4	99/04/23 SMI"

/*
 * PCI nexus HotPlug devctl interface
 */
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/async.h>
#include <sys/sysmacros.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi_impldefs.h>
#include <sys/pci/pci_obj.h>
#include <sys/open.h>
#include <sys/errno.h>
#include <sys/file.h>

/*LINTLIBRARY*/

static int pci_open(dev_t *devp, int flags, int otyp, cred_t *credp);
static int pci_close(dev_t dev, int flags, int otyp, cred_t *credp);
static int pci_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
						cred_t *credp, int *rvalp);
struct cb_ops pci_cb_ops = {
	pci_open,			/* open */
	pci_close,			/* close */
	nodev,				/* strategy */
	nodev,				/* print */
	nodev,				/* dump */
	nodev,				/* read */
	nodev,				/* write */
	pci_ioctl,			/* ioctl */
	nodev,				/* devmap */
	nodev,				/* mmap */
	nodev,				/* segmap */
	nochpoll,			/* poll */
	ddi_prop_op,			/* cb_prop_op */
	NULL,				/* streamtab */
	D_NEW | D_MP | D_HOTPLUG,	/* Driver compatibility flag */
	CB_REV,				/* rev */
	nodev,				/* int (*cb_aread)() */
	nodev				/* int (*cb_awrite)() */
};

/* ARGSUSED3 */
static int
pci_open(dev_t *devp, int flags, int otyp, cred_t *credp)
{
	pci_t *pci_p;

	/*
	 * Make sure the open is for the right file type.
	 */
	if (otyp != OTYP_CHR)
		return (EINVAL);

	/*
	 * Get the soft state structure for the device.
	 */
	pci_p = get_pci_soft_state(getminor(*devp));
	if (pci_p == NULL)
		return (ENXIO);

	/*
	 * Handle the open by tracking the device state.
	 */
	DEBUG2(DBG_OPEN, pci_p->pci_dip, "devp=%x: flags=%x\n", devp, flags);
	mutex_enter(&pci_p->pci_mutex);
	if (flags & FEXCL) {
		if (pci_p->pci_soft_state != PCI_SOFT_STATE_CLOSED) {
			mutex_exit(&pci_p->pci_mutex);
			DEBUG0(DBG_OPEN, pci_p->pci_dip, "busy\n");
			return (EBUSY);
		}
		pci_p->pci_soft_state = PCI_SOFT_STATE_OPEN_EXCL;
	} else
		pci_p->pci_soft_state = PCI_SOFT_STATE_OPEN;
	pci_p->pci_open_count++;
	mutex_exit(&pci_p->pci_mutex);
	return (0);
}


/* ARGSUSED */
static int
pci_close(dev_t dev, int flags, int otyp, cred_t *credp)
{
	pci_t *pci_p;

	if (otyp != OTYP_CHR)
		return (EINVAL);

	pci_p = get_pci_soft_state(getminor(dev));
	if (pci_p == NULL)
		return (ENXIO);

	DEBUG2(DBG_CLOSE, pci_p->pci_dip, "dev=%x: flags=%x\n", dev, flags);
	mutex_enter(&pci_p->pci_mutex);
	pci_p->pci_soft_state &=
	    ~(PCI_SOFT_STATE_OPEN|PCI_SOFT_STATE_OPEN_EXCL);
	pci_p->pci_open_count = 0;
	mutex_exit(&pci_p->pci_mutex);
	return (0);
}


/*
 * pci_ioctl: devctl hotplug controls
 */
/* ARGSUSED */
static int
pci_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp, int *rvalp)
{
	pci_t *pci_p;
	dev_info_t *self;
	dev_info_t *child_dip = NULL;
	char *name, *addr;
	struct devctl_iocdata *dcp;
	uint_t bus_state;
	int rv = 0;
	int nrv = 0;
	uint_t cmd_flags = 0;

	pci_p = get_pci_soft_state(getminor(dev));
	if (pci_p == NULL)
		return (ENXIO);

	self = pci_p->pci_dip;
	DEBUG2(DBG_IOCTL, self, "dev=%x: cmd=%x\n", dev, cmd);

	/*
	 * read devctl ioctl data
	 */
	if (ndi_dc_allochdl((void *)arg, &dcp) != NDI_SUCCESS)
		return (EFAULT);

	switch (cmd) {
	case DEVCTL_DEVICE_GETSTATE:
		name = ndi_dc_getname(dcp);
		addr = ndi_dc_getaddr(dcp);
		if (name == NULL || addr == NULL) {
			rv = EINVAL;
			break;
		}
		DEBUG2(DBG_IOCTL, self,
		    "DEVCTL_DEVICE_GETSTATE name=%s addr=%s\n", name, addr);

		/*
		 * lookup and hold child device
		 */
		child_dip = ndi_devi_find(self, name, addr);
		if (child_dip == NULL) {
			rv = ENXIO;
			break;
		}

		if (ndi_dc_return_dev_state(child_dip, dcp) != NDI_SUCCESS)
			rv = EFAULT;
		break;

	case DEVCTL_DEVICE_ONLINE:
		name = ndi_dc_getname(dcp);
		addr = ndi_dc_getaddr(dcp);
		if (name == NULL || addr == NULL) {
			rv = EINVAL;
			break;
		}
		DEBUG2(DBG_IOCTL, self,
		    "DEVCTL_DEVICE_ONLINE name=%s addr=%s\n", name, addr);

		/*
		 * lookup and hold child device
		 */
		child_dip = ndi_devi_find(self, name, addr);
		if (child_dip == NULL) {
			rv = ENXIO;
			break;
		}

		if (ndi_devi_online(child_dip, 0) != NDI_SUCCESS)
			rv = EIO;

		break;

	case DEVCTL_DEVICE_OFFLINE:
		name = ndi_dc_getname(dcp);
		addr = ndi_dc_getaddr(dcp);
		if (name == NULL || addr == NULL) {
			rv = EINVAL;
			break;
		}
		DEBUG2(DBG_IOCTL, self,
		    "DEVCTL_DEVICE_OFFLINE name=%s addr=%s\n", name, addr);

		/*
		 * lookup child device
		 */
		child_dip = ndi_devi_find(self, name, addr);
		if (child_dip == NULL) {
			rv = ENXIO;
			break;
		}

		nrv = ndi_devi_offline(child_dip, cmd_flags);
		if (nrv == NDI_BUSY)
			rv = EBUSY;
		else if (nrv == NDI_FAILURE)
			rv = EIO;
		break;

	case DEVCTL_DEVICE_RESET:
		DEBUG0(DBG_IOCTL, self, "DEVCTL_DEVICE_RESET\n");
		rv = ENOTSUP;
		break;


	case DEVCTL_BUS_QUIESCE:
		DEBUG0(DBG_IOCTL, self, "DEVCTL_BUS_QUIESCE\n");
		if (ndi_get_bus_state(self, &bus_state) == NDI_SUCCESS)
			if (bus_state == BUS_QUIESCED)
				break;
		(void) ndi_set_bus_state(self, BUS_QUIESCED);
		break;

	case DEVCTL_BUS_UNQUIESCE:
		DEBUG0(DBG_IOCTL, self, "DEVCTL_BUS_UNQUIESCE\n");
		if (ndi_get_bus_state(self, &bus_state) == NDI_SUCCESS)
			if (bus_state == BUS_ACTIVE)
				break;
		(void) ndi_set_bus_state(self, BUS_ACTIVE);
		break;

	case DEVCTL_BUS_RESET:
		DEBUG0(DBG_IOCTL, self, "DEVCTL_BUS_RESET\n");
		rv = ENOTSUP;
		break;

	case DEVCTL_BUS_RESETALL:
		DEBUG0(DBG_IOCTL, self, "DEVCTL_BUS_RESETALL\n");
		rv = ENOTSUP;
		break;

	case DEVCTL_BUS_GETSTATE:
		DEBUG0(DBG_IOCTL, self, "DEVCTL_BUS_GETSTATE\n");
		if (ndi_dc_return_bus_state(self, dcp) != NDI_SUCCESS)
			rv = EFAULT;
		break;

	default:
		rv = ENOTTY;
	}

	ndi_dc_freehdl(dcp);
	return (rv);
}
