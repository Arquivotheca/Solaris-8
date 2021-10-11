/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pcihp.c	1.13	99/07/26 SMI"

/*
 * **********************************************************************
 * Extension module for PCI nexus drivers to support PCI Hot Plug feature.
 *
 * DESCRIPTION:
 *    This module basically implements "devctl" and Attachment Point device
 *    nodes for hot plug operations. The cb_ops functions needed for access
 *    to these devcice nodes are also implemented. For hotplug operations
 *    on Attachment Points it interacts with the hotplug services (HPS)
 *    framework. A pci nexus driver would simply call pcihp_init() in its
 *    attach() function and pcihp_uninit() call in its detach() function.
 * **********************************************************************
 */

#define	CPCI_ENUM

#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/modctl.h>
#include <sys/autoconf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi_impldefs.h>
#include <sys/ndi_impldefs.h>
#include <sys/ddipropdefs.h>
#include <sys/open.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/pci.h>
#include <sys/pci_impl.h>
#include <sys/devctl.h>
#include <sys/hotplug/hpcsvc.h>
#include <sys/hotplug/pci/pcicfg.h>
#include <sys/hotplug/pci/pcihp.h>

/*
 * NOTE:
 * This module depends on PCI Configurator module (misc/pcicfg),
 * Hot Plug Services framework module (misc/hpcsvc) and Bus Resource
 * Allocator module (misc/busra).
 */

/*
 * ************************************************************************
 * *** Implementation specific data structures/definitions.		***
 * ************************************************************************
 */

/* soft state */
typedef enum { PCIHP_SOFT_STATE_CLOSED, PCIHP_SOFT_STATE_OPEN,
		PCIHP_SOFT_STATE_OPEN_EXCL } pcihp_soft_state_t;

#define	PCI_MAX_DEVS	32	/* max. number of devices on a pci bus */

/*
 * Soft state structure associated with each hot plug pci bus instance.
 */
typedef struct pcihp {

	/* instance number of the hot plug bus */
	int			instance;

	/* soft state flags: PCIHP_SOFT_STATE_* */
	pcihp_soft_state_t	soft_state;

	/* global mutex to serialize exclusive access to the bus */
	kmutex_t		mutex;

	/* devinfo pointer to the pci bus node */
	dev_info_t		*dip;

	/* slot information structure */
	struct pcihp_slotinfo {
		hpc_slot_t	slot_hdl;	/* HPS slot handle */
		ap_rstate_t	rstate;		/* state of Receptacle */
		ap_ostate_t	ostate;		/* state of the Occupant */
		ap_condition_t	condition;	/* condition of the occupant */
		time32_t	last_change;	/* XXX needed? */
		uint32_t	event_mask;	/* last event mask registerd */
		char		*name;		/* slot logical name */
		uint_t		slot_flags;
		uint16_t	slot_type;	/* slot type: pci or cpci */
		uint16_t	slot_capabilities; /* 64bit, etc. */
		uint_t		hs_csr_location; /* Location of HS_CSR */
		kmutex_t	slot_mutex;	/* mutex to serialize hotplug */
						/* operations on the slot */
	} slotinfo[PCI_MAX_DEVS];

	/* misc. bus attributes */
	uint_t			bus_flags;
} pcihp_t;

/*
 * Bit definitions for slot_flags field:
 *
 *	PCIHP_SLOT_AUTO_CFG_EN	This flags is set if nexus can do auto
 *				configuration of hot plugged card on this slot
 *				if the hardware reports the hot plug events.
 *
 *	PCIHP_SLOT_DISABLED	Slot is disabled for hotplug operations.
 *
 *	PCIHP_SLOT_NOT_HEALTHY	HEALTHY# signal is not OK on this slot.
 */
#define	PCIHP_SLOT_AUTO_CFG_EN		0x1
#define	PCIHP_SLOT_DISABLED		0x2
#define	PCIHP_SLOT_NOT_HEALTHY		0x4

/*
 * Bit definitions for bus_flags field:
 *
 *	PCIHP_BUS_66MHZ	Bus is running at 66Mhz.
 */
#define	PCIHP_BUS_66MHZ		0x1
#define	PCIHP_BUS_ENUM_RADIAL	0x2

#define	PCIHP_DEVCTL_MINOR	255

#define	AP_MINOR_NUM_TO_PCIHP_INSTANCE(x)  ((x) >> 8)
#define	AP_MINOR_NUM_TO_PCI_DEVNUM(x)	   ((x) & 0xFF)
#define	AP_MINOR_NUM(x, y)		   (((uint_t)(x) << 8) | ((y) & 0xFF))

/*
 * control structure for tree walk during configure/unconfigure operation.
 */
struct pcihp_config_ctrl {
	int	pci_dev;	/* PCI device number for the slot */
	uint_t	flags;		/* control flags (see below) */
	int	op;		/* operation: PCIHP_ONLINE or PCIHP_OFFLINE */
	int	rv;		/* return error code */
	dev_info_t *dip;	/* dip at which the (first) error occured */
};

/*
 * control flags for configure/unconfigure operations on the tree.
 *
 * PCIHP_CFG_CONTINUE	continue the operation ignoring errors
 */
#define	PCIHP_CFG_CONTINUE	0x1

#define	PCIHP_ONLINE	1
#define	PCIHP_OFFLINE	0


/* Leaf ops (hotplug controls for target devices) */
static int pcihp_open(dev_t *, int, int, cred_t *);
static int pcihp_close(dev_t, int, int, cred_t *);
static int pcihp_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);

#ifdef DEBUG
static int pcihp_debug;
#define	PCIHP_DEBUG(args)	if (pcihp_debug >= 1) cmn_err args
#define	PCIHP_DEBUG2(args)	if (pcihp_debug >= 2) cmn_err args
#else
#define	PCIHP_DEBUG(args)
#define	PCIHP_DEBUG2(args)
#endif

#ifdef CPCI_ENUM_DEBUG
int pcihp_hs_csr_debug = 0x40;
static int pcihp_debug_scan_all = 0;
#endif /* CPCI_ENUM_DEBUG */

/* static functions */
static int pcihp_new_slot_state(dev_info_t *, hpc_slot_t,
	hpc_slot_info_t *, int);
static int pcihp_configure(dev_info_t *, void *);
static int pcihp_unconfigure_node(dev_info_t *);
static int pcihp_event_handler(caddr_t, uint_t);
static dev_info_t *pcihp_devi_find(dev_info_t *dip, uint_t dev, uint_t func);
static int pcihp_match_dev(dev_info_t *dip, void *hdl);
static int pcihp_get_hs_csr(struct pcihp_slotinfo *, ddi_acc_handle_t,
	uint8_t *);
static void pcihp_set_hs_csr(struct pcihp_slotinfo *, ddi_acc_handle_t,
	uint8_t *);
static void pcihp_handle_enum(pcihp_t *, int);
static int pcihp_enum_slot(pcihp_t *, struct pcihp_slotinfo *, int);
static void pcihp_handle_enum_extraction(pcihp_t *, int,
	ddi_acc_handle_t config_handle);
static void pcihp_handle_enum_insertion(pcihp_t *, int,
	ddi_acc_handle_t config_handle);
static void pcihp_turn_on_blue_led(pcihp_t *, int);
static void pcihp_turn_off_blue_led(pcihp_t *, int);
static void pcihp_clear_EXT(struct pcihp_slotinfo *, ddi_acc_handle_t);
static void pcihp_clear_INS(struct pcihp_slotinfo *, ddi_acc_handle_t);
static int pcihp_add_dummy_reg_property(dev_info_t *, uint_t, uint_t, uint_t);

extern int pcicfg_configure(dev_info_t *, uint_t);
extern int pcicfg_unconfigure(dev_info_t *, uint_t);

#if 0
static void pcihp_probe_slot_state(dev_info_t *, int, hpc_slot_state_t *);
#endif

struct cb_ops pcihp_cb_ops = {
	pcihp_open,			/* open */
	pcihp_close,			/* close */
	nodev,				/* strategy */
	nodev,				/* print */
	nodev,				/* dump */
	nodev,				/* read */
	nodev,				/* write */
	pcihp_ioctl,			/* ioctl */
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

/*
 * local data
 */

int pcihp_autocfg_enabled = 0; /* auto config is disabled by default */

static void *pcihp_state;
static kmutex_t pcihp_mutex; /* mutex to protect the following data */
static int pcihp_next_instance = 0;
static int pcihp_count = 0;

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_miscops;
static struct modlmisc modlmisc = {
	&mod_miscops,
	"PCI nexus hotplug support module version 1.13",
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlmisc,
	NULL
};

int
_init(void)
{
	int error;

	mutex_init(&pcihp_mutex, NULL, MUTEX_DRIVER, NULL);
	(void) ddi_soft_state_init(&pcihp_state, sizeof (struct pcihp), 0);
	if ((error = mod_install(&modlinkage)) != 0) {
		mutex_destroy(&pcihp_mutex);
		ddi_soft_state_fini(&pcihp_state);
	}

	return (error);
}

int
_fini(void)
{
	int error;

	if ((error = mod_remove(&modlinkage)) == 0) {
		mutex_destroy(&pcihp_mutex);
		ASSERT(pcihp_count == 0);
		ddi_soft_state_fini(&pcihp_state);
	}

	return (error);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/* ARGSUSED3 */
static int
pcihp_open(dev_t *devp, int flags, int otyp, cred_t *credp)
{
	pcihp_t *pcihp_p;
	int minor;
	int pci_dev;

	/*
	 * Make sure the open is for the right file type.
	 */
	if (otyp != OTYP_CHR)
		return (EINVAL);

	/*
	 * Get the soft state structure for the 'devctl' device.
	 */
	minor = getminor(*devp);
	pcihp_p = (pcihp_t *)ddi_get_soft_state(pcihp_state,
		AP_MINOR_NUM_TO_PCIHP_INSTANCE(minor));
	if (pcihp_p == NULL)
		return (ENXIO);

	mutex_enter(&pcihp_p->mutex);

	/*
	 * check if the slot is enabled. If the pci_dev is
	 * valid then the minor device is an AP. Otherwise
	 * it is ":devctl" minor device.
	 */
	pci_dev = AP_MINOR_NUM_TO_PCI_DEVNUM(minor);
	if (pci_dev < PCI_MAX_DEVS) {
		struct pcihp_slotinfo *slotinfop;

		slotinfop = &pcihp_p->slotinfo[pci_dev];
		if ((slotinfop->slot_hdl == NULL) ||
		    (slotinfop->slot_flags & PCIHP_SLOT_DISABLED)) {
			mutex_exit(&pcihp_p->mutex);
			return (ENXIO);
		}
	}

	/*
	 * Handle the open by tracking the device state.
	 *
	 * Note: Needs review w.r.t exclusive access to AP or the bus.
	 * Currently in the pci plug-in we don't use EXCL open at all
	 * so the code below implements EXCL access on the bus.
	 */

	/* enforce exclusive access to the bus */
	if ((pcihp_p->soft_state == PCIHP_SOFT_STATE_OPEN_EXCL) ||
	    ((flags & FEXCL) &&
	    (pcihp_p->soft_state != PCIHP_SOFT_STATE_CLOSED))) {
		mutex_exit(&pcihp_p->mutex);
		return (EBUSY);
	}

	if (flags & FEXCL)
		pcihp_p->soft_state = PCIHP_SOFT_STATE_OPEN_EXCL;
	else
		pcihp_p->soft_state = PCIHP_SOFT_STATE_OPEN;

	mutex_exit(&pcihp_p->mutex);
	return (0);
}

/* ARGSUSED */
static int
pcihp_close(dev_t dev, int flags, int otyp, cred_t *credp)
{
	pcihp_t *pcihp_p;

	if (otyp != OTYP_CHR)
		return (EINVAL);

	pcihp_p = (pcihp_t *)ddi_get_soft_state(pcihp_state,
		AP_MINOR_NUM_TO_PCIHP_INSTANCE(getminor(dev)));
	if (pcihp_p == NULL)
		return (ENXIO);

	mutex_enter(&pcihp_p->mutex);
	pcihp_p->soft_state = PCIHP_SOFT_STATE_CLOSED;
	mutex_exit(&pcihp_p->mutex);
	return (0);
}

/*
 * pcihp_ioctl: devctl hotplug controls
 */
/* ARGSUSED */
static int
pcihp_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp,
	int *rvalp)
{
	pcihp_t *pcihp_p;
	dev_info_t *self;
	dev_info_t *child_dip = NULL;
	char *name, *addr;
	struct devctl_iocdata *dcp;
	uint_t bus_state;
	int rv = 0;
	int nrv = 0;
	uint_t cmd_flags = 0;
	int ap_minor, pci_dev;
	struct pcihp_slotinfo *slotinfop;
	hpc_slot_state_t rstate;
	struct pcihp_config_ctrl ctrl;
	uint_t circular_count;
	devctl_ap_state_t ap_state;
	struct hpc_control_data hpc_ctrldata;
	struct hpc_led_info led_info;

	ap_minor = getminor(dev);
	pcihp_p = (pcihp_t *)ddi_get_soft_state(pcihp_state,
		AP_MINOR_NUM_TO_PCIHP_INSTANCE(ap_minor));
	if (pcihp_p == NULL)
		return (ENXIO);

	self = pcihp_p->dip;

	/*
	 * read devctl ioctl data
	 */
	if ((cmd != DEVCTL_AP_CONTROL) &&
	    ndi_dc_allochdl((void *)arg, &dcp) != NDI_SUCCESS)
		return (EFAULT);

	switch (cmd) {
	case DEVCTL_DEVICE_GETSTATE:
		name = ndi_dc_getname(dcp);
		addr = ndi_dc_getaddr(dcp);
		if (name == NULL || addr == NULL) {
			rv = EINVAL;
			break;
		}

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
		rv = ENOTSUP;
		break;


	case DEVCTL_BUS_QUIESCE:
		if (ndi_get_bus_state(self, &bus_state) == NDI_SUCCESS)
			if (bus_state == BUS_QUIESCED)
				break;
		(void) ndi_set_bus_state(self, BUS_QUIESCED);
		break;

	case DEVCTL_BUS_UNQUIESCE:
		if (ndi_get_bus_state(self, &bus_state) == NDI_SUCCESS)
			if (bus_state == BUS_ACTIVE)
				break;
		(void) ndi_set_bus_state(self, BUS_ACTIVE);
		break;

	case DEVCTL_BUS_RESET:
		rv = ENOTSUP;
		break;

	case DEVCTL_BUS_RESETALL:
		rv = ENOTSUP;
		break;

	case DEVCTL_BUS_GETSTATE:
		if (ndi_dc_return_bus_state(self, dcp) != NDI_SUCCESS)
			rv = EFAULT;
		break;

	case DEVCTL_AP_CONNECT:
	case DEVCTL_AP_DISCONNECT:
		/*
		 * CONNECT(DISCONNECT) the hot plug slot to(from) the bus.
		 *
		 * For cPCI slots this operation is a nop so the HPC
		 * driver may return success if it is a valid operation.
		 */
	case DEVCTL_AP_INSERT:
	case DEVCTL_AP_REMOVE:
		/*
		 * Prepare the slot for INSERT/REMOVE operation.
		 */

		/*
		 * check for valid request:
		 *	1. It is a hotplug slot.
		 *	2. The slot has no occupant that is in
		 *	   the 'configured' state.
		 *
		 * The lower 8 bits of the minor number is the PCI
		 * device number for the slot.
		 */
		pci_dev = AP_MINOR_NUM_TO_PCI_DEVNUM(ap_minor);
		if (pci_dev >= PCI_MAX_DEVS) {
			rv = ENXIO;
			break;
		}
		slotinfop = &pcihp_p->slotinfo[pci_dev];
		if ((slotinfop->slot_hdl == NULL) ||
		    (slotinfop->slot_flags & PCIHP_SLOT_DISABLED)) {
			rv = ENXIO;
			break;
		}

		/* the slot occupant must be in the UNCONFIGURED state */
		if (slotinfop->ostate != AP_OSTATE_UNCONFIGURED) {
			rv = EINVAL;
			break;
		}

		/*
		 * Call the HPC driver to perform the operation on the slot.
		 */
		mutex_enter(&slotinfop->slot_mutex);
		switch (cmd) {
		case DEVCTL_AP_INSERT:
			rv = hpc_nexus_insert(slotinfop->slot_hdl, NULL, 0);
			break;
		case DEVCTL_AP_REMOVE:
			rv = hpc_nexus_remove(slotinfop->slot_hdl, NULL, 0);
			break;
		case DEVCTL_AP_CONNECT:
			if ((rv = hpc_nexus_connect(slotinfop->slot_hdl,
				NULL, 0)) == 0)
				slotinfop->rstate = AP_RSTATE_CONNECTED;
			break;
		case DEVCTL_AP_DISCONNECT:
			if ((rv = hpc_nexus_disconnect(slotinfop->slot_hdl,
				NULL, 0)) == 0)
				slotinfop->rstate = AP_RSTATE_DISCONNECTED;
			break;
		}
		mutex_exit(&slotinfop->slot_mutex);

		switch (rv) {
		case HPC_ERR_INVALID:
			rv = ENXIO;
			break;
		case HPC_ERR_NOTSUPPORTED:
			rv = ENOTSUP;
			break;
		case HPC_ERR_FAILED:
			rv = EIO;
			break;
		}

		break;

	case DEVCTL_AP_CONFIGURE:
		/*
		 * **************************************
		 * CONFIGURE the occupant in the slot.
		 * **************************************
		 */

		/*
		 * check for valid request:
		 *	1. It is a hotplug slot.
		 *	2. The receptacle is in the CONNECTED state.
		 *
		 * The lower 8 bits of the minor number is the PCI
		 * device number for the slot.
		 */
		pci_dev = AP_MINOR_NUM_TO_PCI_DEVNUM(ap_minor);
		slotinfop = &pcihp_p->slotinfo[pci_dev];
		if ((pci_dev >= PCI_MAX_DEVS) ||
		    (slotinfop->slot_hdl == NULL) ||
		    (slotinfop->slot_flags & PCIHP_SLOT_DISABLED)) {
			rv = ENXIO;
			break;
		}

		mutex_enter(&slotinfop->slot_mutex);

		/*
		 * If the occupant is already in (partially?) configured
		 * state then call the ndi_devi_online() on the device
		 * subtree(s) for this attachment point.
		 */

		if (slotinfop->ostate == AP_OSTATE_CONFIGURED) {
			ctrl.flags = PCIHP_CFG_CONTINUE;
			ctrl.rv = NDI_SUCCESS;
			ctrl.dip = NULL;
			ctrl.pci_dev = pci_dev;
			ctrl.op = PCIHP_ONLINE;

			i_ndi_block_device_tree_changes(&circular_count);
			ddi_walk_devs(ddi_get_child(self), pcihp_configure,
				(void *)&ctrl);
			i_ndi_allow_device_tree_changes(circular_count);

			mutex_exit(&slotinfop->slot_mutex);

			if (ctrl.rv != NDI_SUCCESS) {
				/*
				 * one or more of the devices are not
				 * onlined. How is this to be reported?
				 */
				cmn_err(CE_WARN,
					"pcihp (%s%d): failed to attach one or"
					" more drivers for the card in"
					" the slot %s",
					ddi_driver_name(self),
					ddi_get_instance(self),
					slotinfop->name);
				/* rv = EFAULT; */
			}
			break;
		}

		/*
		 * Occupant is in the UNCONFIGURED state.
		 */

		/* Check if the receptacle is in the CONNECTED state. */
		if (hpc_nexus_control(slotinfop->slot_hdl,
			HPC_CTRL_GET_SLOT_STATE, (caddr_t)&rstate) != 0) {
			rv = ENXIO;
			mutex_exit(&slotinfop->slot_mutex);
			break;
		}

#if 0	/* XXX NOT SUPPORTED? */
		/*
		 * If the receptacle state is undeterminable then
		 * we need to probe the configuration space of the slot
		 * to determine the state.
		 */
		if (rstate == HPC_SLOT_UNKNOWN) {
			/*
			 * HPC driver can not determine the receptacle state.
			 * So, probe for the configuration space of the slot.
			 */
			pcihp_probe_slot_state(self, pci_dev, &rstate);
		}
#endif

		if (rstate == HPC_SLOT_EMPTY) {
			/* error. slot is empty */
			rv = ENXIO;
			mutex_exit(&slotinfop->slot_mutex);
			break;
		}

		if (rstate != HPC_SLOT_CONNECTED) {
			/* error. either the slot is empty or connect failed */
			rv = ENXIO;
			mutex_exit(&slotinfop->slot_mutex);
			break;
		}

		slotinfop->rstate = AP_RSTATE_CONNECTED; /* record rstate */

		/*
		 * Call the configurator to configure the card.
		 */
		if (pcicfg_configure(self, pci_dev) != PCICFG_SUCCESS) {
			rv = EIO;
			mutex_exit(&slotinfop->slot_mutex);
			break;
		}

		/* record the occupant state as CONFIGURED */
		slotinfop->ostate = AP_OSTATE_CONFIGURED;
		slotinfop->condition = AP_COND_OK;

		/* now, online all the devices in the AP */
		ctrl.flags = PCIHP_CFG_CONTINUE;
		ctrl.rv = NDI_SUCCESS;
		ctrl.dip = NULL;
		ctrl.pci_dev = pci_dev;
		ctrl.op = PCIHP_ONLINE;

		i_ndi_block_device_tree_changes(&circular_count);
		ddi_walk_devs(ddi_get_child(self), pcihp_configure,
			(void *)&ctrl);
		i_ndi_allow_device_tree_changes(circular_count);

		if (ctrl.rv != NDI_SUCCESS) {
			/*
			 * one or more of the devices are not
			 * ONLINE'd. How is this to be
			 * reported?
			 */
			cmn_err(CE_WARN,
				"pcihp (%s%d): failed to attach one or"
				" more drivers for the card in"
				" the slot %s",
				ddi_driver_name(pcihp_p->dip),
				ddi_get_instance(pcihp_p->dip),
				slotinfop->name);
			/* rv = EFAULT; */
		}

		/* tell HPC driver that the occupant is configured */
		(void) hpc_nexus_control(slotinfop->slot_hdl,
			HPC_CTRL_DEV_CONFIGURED, NULL);

		mutex_exit(&slotinfop->slot_mutex);

		break;

	case DEVCTL_AP_UNCONFIGURE:
		/*
		 * **************************************
		 * UNCONFIGURE the occupant in the slot.
		 * **************************************
		 */

		/*
		 * check for valid request:
		 *	1. It is a hotplug slot.
		 *	2. The occupant is in the CONFIGURED state.
		 *
		 * The lower 8 bits of the minor number is the PCI
		 * device number for the slot.
		 */
		pci_dev = AP_MINOR_NUM_TO_PCI_DEVNUM(ap_minor);
		slotinfop = &pcihp_p->slotinfo[pci_dev];
		if ((pci_dev >= PCI_MAX_DEVS) ||
		    (slotinfop->slot_hdl == NULL) ||
		    (slotinfop->slot_flags & PCIHP_SLOT_DISABLED)) {
			rv = ENXIO;
			break;
		}

		mutex_enter(&slotinfop->slot_mutex);

		/*
		 * If the occupant is in the CONFIGURED state then
		 * call the configurator to unconfigure the slot.
		 */
		if (slotinfop->ostate == AP_OSTATE_CONFIGURED) {
			/*
			 * Detach all the drivers for the devices in the
			 * slot. Call pcihp_configure() to do this.
			 */
			ctrl.flags = 0;
			ctrl.rv = NDI_SUCCESS;
			ctrl.dip = NULL;
			ctrl.pci_dev = pci_dev;
			ctrl.op = PCIHP_OFFLINE;
			i_ndi_block_device_tree_changes(&circular_count);
			ddi_walk_devs(ddi_get_child(self), pcihp_configure,
				(void *)&ctrl);
			i_ndi_allow_device_tree_changes(circular_count);

			if (ctrl.rv != NDI_SUCCESS) {
				/*
				 * Failed to detach one or more drivers
				 * Restore the state of drivers which
				 * are offlined during this operation.
				 */
				ctrl.flags = 0;
				ctrl.rv = NDI_SUCCESS;
				ctrl.dip = NULL;
				ctrl.pci_dev = pci_dev;
				ctrl.op = PCIHP_ONLINE;
				i_ndi_block_device_tree_changes(
							&circular_count);
				ddi_walk_devs(ddi_get_child(self),
					pcihp_configure, (void *)&ctrl);
				i_ndi_allow_device_tree_changes(circular_count);
				rv = EBUSY;
			} else if (pcicfg_unconfigure(self,
				pci_dev) == PCICFG_SUCCESS) {
				slotinfop->ostate = AP_OSTATE_UNCONFIGURED;
				slotinfop->condition = AP_COND_UNKNOWN;
				/*
				 * send the notification of state change
				 * to the HPC driver.
				 */
				(void) hpc_nexus_control(slotinfop->slot_hdl,
					HPC_CTRL_DEV_UNCONFIGURED, NULL);
			} else {
				rv = EIO;
			}
		}

		mutex_exit(&slotinfop->slot_mutex);

		break;

	case DEVCTL_AP_GETSTATE:
	    {
		int mutex_held;

		/*
		 * return the state of Attachment Point.
		 *
		 * If the occupant is in UNCONFIGURED state then
		 * we should get the receptacle state from the
		 * HPC driver because the receptacle state
		 * maintained in the nexus may not be accurate.
		 */

		/*
		 * check for valid request:
		 *	1. It is a hotplug slot.
		 *
		 * The lower 8 bits of the minor number is the PCI
		 * device number for the slot.
		 */
		pci_dev = AP_MINOR_NUM_TO_PCI_DEVNUM(ap_minor);
		slotinfop = &pcihp_p->slotinfo[pci_dev];
		if (pci_dev >= PCI_MAX_DEVS || slotinfop->slot_hdl == NULL) {
			rv = ENXIO;
			break;
		}

		/* try to acquire the slot mutex */
		mutex_held = mutex_tryenter(&slotinfop->slot_mutex);

		if (slotinfop->ostate == AP_OSTATE_UNCONFIGURED) {
		    if (hpc_nexus_control(slotinfop->slot_hdl,
			HPC_CTRL_GET_SLOT_STATE, (caddr_t)&rstate) != 0) {
			rv = ENXIO;
			mutex_exit(&slotinfop->slot_mutex);
			break;
		    }
		    slotinfop->rstate = (ap_rstate_t)rstate;
		}

		ap_state.ap_rstate = slotinfop->rstate;
		ap_state.ap_ostate = slotinfop->ostate;
		ap_state.ap_condition = slotinfop->condition;
		ap_state.ap_last_change = 0; /* XXX */
		ap_state.ap_error_code = 0; /* XXX */
		if (mutex_held)
			ap_state.ap_in_transition = 0; /* AP is not busy */
		else
			ap_state.ap_in_transition = 1; /* AP is busy */

		if (mutex_held)
			mutex_exit(&slotinfop->slot_mutex);

		/* copy the return-AP-state information to the user space */
		if (ndi_dc_return_ap_state(&ap_state, dcp) != NDI_SUCCESS)
			rv = ENXIO;

		break;

	    }
	case DEVCTL_AP_CONTROL:
		/*
		 * HPC control functions:
		 *	HPC_CTRL_ENABLE_SLOT/HPC_CTRL_DISABLE_SLOT
		 *		Changes the state of the slot and preserves
		 *		the state across the reboot.
		 *	HPC_CTRL_ENABLE_AUTOCFG/HPC_CTRL_DISABLE_AUTOCFG
		 *		Enables or disables the auto configuration
		 *		of hot plugged occupant if the hardware
		 *		supports notification of the hot plug
		 *		events.
		 *	HPC_CTRL_GET_LED_STATE/HPC_CTRL_SET_LED_STATE
		 *		Controls the state of an LED.
		 *	HPC_CTRL_GET_SLOT_INFO
		 *		Get slot information data structure
		 *		(hpc_slot_info_t).
		 *	HPC_CTRL_GET_BOARD_TYPE
		 *		Get board type information (hpc_board_type_t).
		 *	HPC_CTRL_GET_CARD_INFO
		 *		Get card information (hpc_card_info_t).
		 *
		 * These control functions are used by the cfgadm plug-in
		 * to implement "-x" and "-v" options.
		 */

		/* copy user ioctl data first */
#ifdef _MULTI_DATAMODEL
		if (ddi_model_convert_from(mode & FMODELS) == DDI_MODEL_ILP32) {
			struct hpc_control32_data hpc_ctrldata32;

			if (copyin((void *)arg, (void *)&hpc_ctrldata32,
				sizeof (struct hpc_control32_data)) != 0) {
				rv = EFAULT;
				break;
			}
			hpc_ctrldata.cmd = hpc_ctrldata32.cmd;
			hpc_ctrldata.data = (void *)hpc_ctrldata32.data;
		}
#else
		if (copyin((void *)arg, (void *)&hpc_ctrldata,
			sizeof (struct hpc_control_data)) != 0) {
			rv = EFAULT;
			break;
		}
#endif

		/*
		 * check for valid request:
		 *	1. It is a hotplug slot.
		 *
		 * The lower 8 bits of the minor number is the PCI
		 * device number for the slot.
		 */
		pci_dev = AP_MINOR_NUM_TO_PCI_DEVNUM(ap_minor);
		slotinfop = &pcihp_p->slotinfo[pci_dev];
		if (pci_dev >= PCI_MAX_DEVS || slotinfop->slot_hdl == NULL) {
			rv = ENXIO;
			break;
		}

		mutex_enter(&slotinfop->slot_mutex);

		switch (hpc_ctrldata.cmd) {

		case HPC_CTRL_GET_LED_STATE:
			/* copy the led info from the user space */
			if (copyin(hpc_ctrldata.data, (void *)&led_info,
			    sizeof (hpc_led_info_t)) != 0) {
			    rv = ENXIO;
			    break;
			}

			/* get the state of LED information */
			if (hpc_nexus_control(slotinfop->slot_hdl,
			    HPC_CTRL_GET_LED_STATE, (caddr_t)&led_info) != 0) {
			    rv = ENXIO;
			    break;
			}

			/* copy the led info to the user space */
			if (copyout((void *)&led_info,
			    hpc_ctrldata.data,
			    sizeof (hpc_led_info_t)) != 0) {
			    rv = ENXIO;
			    break;
			}

			break;

		case HPC_CTRL_SET_LED_STATE:
			/* copy the led info from the user space */
			if (copyin(hpc_ctrldata.data, (void *)&led_info,
			    sizeof (hpc_led_info_t)) != 0) {
			    rv = ENXIO;
			    break;
			}

			/* set the state of an LED */
			if (hpc_nexus_control(slotinfop->slot_hdl,
			    HPC_CTRL_SET_LED_STATE, (caddr_t)&led_info) != 0) {
			    rv = ENXIO;
			    break;
			}

			break;

		case HPC_CTRL_ENABLE_SLOT:
			/*
			 * Enable the slot for hotplug operations.
			 */
			slotinfop->slot_flags &= ~PCIHP_SLOT_DISABLED;

			/* tell the HPC driver also */
			(void) hpc_nexus_control(slotinfop->slot_hdl,
			    HPC_CTRL_ENABLE_SLOT, NULL);

			/* XXX need to preserve this state across reboot? */

			break;

		case HPC_CTRL_DISABLE_SLOT:
			/*
			 * Disable the slot for hotplug operations.
			 */
			slotinfop->slot_flags |= PCIHP_SLOT_DISABLED;

			/* tell the HPC driver also */
			(void) hpc_nexus_control(slotinfop->slot_hdl,
			    HPC_CTRL_DISABLE_SLOT, NULL);

			/* XXX need to preserve this state across reboot? */

			break;

		case HPC_CTRL_ENABLE_AUTOCFG:
			/*
			 * Enable auto configuration on this slot.
			 */
			slotinfop->slot_flags |= PCIHP_SLOT_AUTO_CFG_EN;

			/* tell the HPC driver also */
			(void) hpc_nexus_control(slotinfop->slot_hdl,
			    HPC_CTRL_ENABLE_AUTOCFG, NULL);

			break;

		case HPC_CTRL_DISABLE_AUTOCFG:
			/*
			 * Disable auto configuration on this slot.
			 */
			slotinfop->slot_flags &= ~PCIHP_SLOT_AUTO_CFG_EN;

			/* tell the HPC driver also */
			(void) hpc_nexus_control(slotinfop->slot_hdl,
			    HPC_CTRL_DISABLE_AUTOCFG, NULL);

			break;

		case HPC_CTRL_GET_BOARD_TYPE:
		    {
			hpc_board_type_t board_type;

			/*
			 * Get board type data structure, hpc_board_type_t.
			 */
			if (hpc_nexus_control(slotinfop->slot_hdl,
			    HPC_CTRL_GET_BOARD_TYPE,
			    (caddr_t)&board_type) != 0) {
			    rv = ENXIO;
			    break;
			}

			/* copy the board type info to the user space */
			if (copyout((void *)&board_type, hpc_ctrldata.data,
			    sizeof (hpc_board_type_t)) != 0) {
			    rv = ENXIO;
			    break;
			}

			break;
		    }

		case HPC_CTRL_GET_SLOT_INFO:
		    {
			hpc_slot_info_t slot_info;

			/*
			 * Get slot information structure, hpc_slot_info_t.
			 */
			slot_info.version = HPC_SLOT_INFO_VERSION;
			slot_info.slot_type = slotinfop->slot_type;
			slot_info.pci_slot_capabilities =
					slotinfop->slot_capabilities;
			slot_info.pci_dev_num = (uint16_t)pci_dev;
			(void) strcpy(slot_info.pci_slot_name, slotinfop->name);

			/* copy the slot info structure to the user space */
			if (copyout((void *)&slot_info, hpc_ctrldata.data,
			    sizeof (hpc_slot_info_t)) != 0) {
			    rv = ENXIO;
			    break;
			}

			break;
		    }

		case HPC_CTRL_GET_CARD_INFO:
		    {
			hpc_card_info_t card_info;
			ddi_acc_handle_t handle;
			dev_info_t *cdip;

			/*
			 * Get card information structure, hpc_card_info_t.
			 */

			/* verify that the card is configured */
			if ((slotinfop->ostate != AP_OSTATE_CONFIGURED) ||
			    ((cdip = pcihp_devi_find(self, pci_dev,
							0)) == NULL)) {
			    /* either the card is not present or */
			    /* it is not configured.		 */
			    rv = ENXIO;
			    break;
			}

			/* get the information from the PCI config header */
			/* for the function 0.				  */
			(void) pci_config_setup(cdip, &handle);
			card_info.prog_class = pci_config_get8(handle,
						PCI_CONF_PROGCLASS);
			card_info.base_class = pci_config_get8(handle,
						PCI_CONF_BASCLASS);
			card_info.sub_class = pci_config_get8(handle,
						PCI_CONF_SUBCLASS);
			card_info.header_type = pci_config_get8(handle,
						PCI_CONF_HEADER);
			pci_config_teardown(&handle);

			/* copy the card info structure to the user space */
			if (copyout((void *)&card_info, hpc_ctrldata.data,
			    sizeof (hpc_card_info_t)) != 0) {
			    rv = ENXIO;
			    break;
			}

			break;
		    }

		default:
			rv = EINVAL;
			break;
		}

		mutex_exit(&slotinfop->slot_mutex);

		break;

	default:
		rv = ENOTTY;
	}

	if (cmd != DEVCTL_AP_CONTROL)
		ndi_dc_freehdl(dcp);

	return (rv);
}

/*
 * Setup function to initialize hot plug feature. Returns DDI_SUCCESS
 * for successful initialization, otherwise it returns DDI_FAILURE.
 *
 * It is assumed that this this function is called from the attach()
 * entry point of the PCI nexus driver.
 */

int
pcihp_init(dev_info_t *dip)
{
	int pcihp_instance;
	pcihp_t *pcihp_p;
	int i;
	caddr_t enum_data;
	int enum_size;

	mutex_enter(&pcihp_mutex);

	/*
	 * Make sure that it is not already initialized.
	 */
	if (ddi_prop_exists(DDI_DEV_T_ANY, dip,
		DDI_PROP_NOTPROM | DDI_PROP_DONTPASS,
		"pcihp-instance") == 1) {
	    cmn_err(CE_WARN, "%s%d: pcihp instance already initialized!\n",
		    ddi_driver_name(dip), ddi_get_instance(dip));
	    goto cleanup;
	}

	/*
	 * initialize soft state structure for the bus instance.
	 */
	pcihp_instance = pcihp_next_instance++;
	if (ddi_soft_state_zalloc(pcihp_state, pcihp_instance) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "%s%d: can't allocate pcihp soft state\n",
		    ddi_driver_name(dip), ddi_get_instance(dip));
		return (DDI_FAILURE);
	}
	pcihp_p = (pcihp_t *)ddi_get_soft_state(pcihp_state, pcihp_instance);
	pcihp_p->instance = pcihp_instance;
	pcihp_p->dip = dip;
	mutex_init(&pcihp_p->mutex, NULL, MUTEX_DRIVER, NULL);
	pcihp_p->soft_state = PCIHP_SOFT_STATE_CLOSED;
	/* XXX if bus is running at 66Mhz then set PCI_BUS_66MHZ bit */
	pcihp_p->bus_flags = 0;	/* XXX FIX IT */

	if (ddi_getlongprop(DDI_DEV_T_ANY, dip, 0, "enum-impl",
	    (caddr_t)&enum_data, &enum_size) == DDI_PROP_SUCCESS) {
		if (strcmp(enum_data, "radial") == 0) {
			pcihp_p->bus_flags |= PCIHP_BUS_ENUM_RADIAL;
		}
		kmem_free(enum_data, enum_size);
	}

	for (i = 0; i < PCI_MAX_DEVS; i++) {
		/* initialize slot mutex */
		mutex_init(&pcihp_p->slotinfo[i].slot_mutex, NULL,
						MUTEX_DRIVER, NULL);
	}

	/*
	 *  register the bus instance with the HPS framework.
	 */
	if (hpc_nexus_register_bus(dip, pcihp_new_slot_state, 0) != 0) {
		cmn_err(CE_WARN, "%s%d: failed to register the bus with HPS\n",
		    ddi_driver_name(dip), ddi_get_instance(dip));
		goto cleanup1;
	}

	/*
	 * Save the instance number of the soft state structure for
	 * this bus as a devinfo property.
	 */
	if (ddi_prop_create(DDI_DEV_T_NONE, dip, DDI_PROP_CANSLEEP,
		"pcihp-instance", (caddr_t)&pcihp_instance,
		sizeof (pcihp_instance)) != DDI_SUCCESS) {
		cmn_err(CE_WARN,
		    "%s%d: failed to add the property 'pcihp-instance'",
		    ddi_driver_name(dip), ddi_get_instance(dip));
		goto cleanup2;
	}

	/*
	 * Now, create the "devctl" minor for hot plug support. The minor
	 * number for "devctl" node is in the same format as the AP
	 * minor nodes.
	 */
	if (ddi_create_minor_node(dip, "devctl", S_IFCHR,
	    AP_MINOR_NUM(pcihp_instance, PCIHP_DEVCTL_MINOR),
	    DDI_NT_NEXUS, 0) != DDI_SUCCESS)
		goto cleanup3;

	pcihp_count++;	/* XXX no check for overflow on this. */

	mutex_exit(&pcihp_mutex);

	/*
	 * Setup resource maps for this bus node. (Note: This can
	 * be done from the attach(9E) of the nexus itself.)
	 */
	(void) pci_resource_setup(dip);

	return (DDI_SUCCESS);

cleanup3:
	(void) ddi_prop_remove(DDI_DEV_T_NONE, dip, "pcihp-instance");
cleanup2:
	(void) hpc_nexus_unregister_bus(dip);
cleanup1:
	ddi_soft_state_free(pcihp_state, pcihp_instance);
cleanup:
	mutex_exit(&pcihp_mutex);
	return (DDI_FAILURE);
}

/*
 * pcihp_uninit()
 *
 * The bus instance is going away, cleanup any data associated with
 * the management of hot plug slots. It is assumed that this function
 * is called from detach() routine of the PCI nexus driver. Also,
 * it is assumed that no devices on the bus are in the configured state.
 */
void
pcihp_uninit(dev_info_t *dip)
{
	int pcihp_instance;
	pcihp_t *pcihp_p;
	int i;

	mutex_enter(&pcihp_mutex);

	/* get the instance number for the pcihp soft state data */
	pcihp_instance = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
			DDI_PROP_DONTPASS, "pcihp-instance", -1);
	if (pcihp_instance < 0) {
		mutex_exit(&pcihp_mutex);
		return; /* no pcihp instance is setup for this bus */
	}

	/* get the pointer to the soft state structure */
	pcihp_p = (pcihp_t *)ddi_get_soft_state(pcihp_state, pcihp_instance);

	/*
	 * Unregister the bus with the HPS.
	 *
	 * (Note: It is assumed that the HPS framework uninstalls
	 *  event handlers for all the hot plug slots on this bus.)
	 */
	(void) hpc_nexus_unregister_bus(dip);

	/* Free up any kmem_alloc'd memory for slot info table. */
	for (i = 0; i < PCI_MAX_DEVS; i++) {
		/* free up slot name strings */
		if (pcihp_p->slotinfo[i].name != NULL)
			kmem_free(pcihp_p->slotinfo[i].name,
				strlen(pcihp_p->slotinfo[i].name) + 1);
	}

	/* free up the soft state structure */
	ddi_soft_state_free(pcihp_state, pcihp_instance);

	/* remove the 'pcihp-instance' property from the devinfo node */
	(void) ddi_prop_remove(DDI_DEV_T_ANY, dip, "pcihp-instance");

	ASSERT(pcihp_count != 0);
	--pcihp_count;

	mutex_exit(&pcihp_mutex);

	/*
	 * Destroy resource maps for this bus node. (Note: This can
	 * be done from the detach(9E) of the nexus itself.)
	 */
	(void) pci_resource_destroy(dip);
}

/*
 * pcihp_new_slot_state()
 *
 * This function is called by the HPS when it finds a hot plug
 * slot is added or being removed from the hot plug framework.
 * It returns 0 for success and HPC_ERR_FAILED for errors.
 */
static int
pcihp_new_slot_state(dev_info_t *dip, hpc_slot_t hdl,
	hpc_slot_info_t *slot_info, int slot_state)
{
	int pcihp_instance;
	pcihp_t *pcihp_p;
	struct pcihp_slotinfo *slotinfop;
	int pci_dev;
	int ap_minor;
	int rv = 0;

	/*
	 * get the soft state structure for the bus instance.
	 */
	pcihp_instance = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
			DDI_PROP_DONTPASS, "pcihp-instance", -1);
	ASSERT(pcihp_instance >= 0);

	pcihp_p = (pcihp_t *)ddi_get_soft_state(pcihp_state, pcihp_instance);
	pci_dev = slot_info->pci_dev_num;
	slotinfop = &pcihp_p->slotinfo[pci_dev];

	mutex_enter(&slotinfop->slot_mutex);

	switch (slot_state) {

	case HPC_SLOT_ONLINE:

		/*
		 * Make sure the slot is not already ONLINE (paranoia?).
		 * (Note: Should this be simply an ASSERTION?)
		 */
		if (slotinfop->slot_hdl != NULL) {
		    PCIHP_DEBUG((CE_WARN,
			"pcihp (%s%d): pci slot (dev %x) already ONLINE!!\n",
			ddi_driver_name(dip), ddi_get_instance(dip), pci_dev));
			rv = HPC_ERR_FAILED;
			break;
		}

		/*
		 * Add the hot plug slot to the bus.
		 */

		/* create the AP minor node */
		ap_minor = AP_MINOR_NUM(pcihp_instance, pci_dev);
		if (ddi_create_minor_node(dip, slot_info->pci_slot_name,
			S_IFCHR, ap_minor,
			DDI_NT_PCI_ATTACHMENT_POINT, 0) == DDI_FAILURE) {
		    cmn_err(CE_WARN,
			"pcihp (%s%d): ddi_create_minor_node failed"
			" for pci dev %x", ddi_driver_name(dip),
			ddi_get_instance(dip), pci_dev);
		    rv = HPC_ERR_FAILED;
		    break;
		}

		/* save the slot handle */
		slotinfop->slot_hdl = hdl;

		/* setup event handler for all hardware events on the slot */
		if (hpc_install_event_handler(hdl, -1, pcihp_event_handler,
			(caddr_t)ap_minor) != 0) {
		    cmn_err(CE_WARN,
			"pcihp (%s%d): install event handler failed"
			" for pci dev %x", ddi_driver_name(dip),
			ddi_get_instance(dip), pci_dev);
		    rv = HPC_ERR_FAILED;
		    break;
		}
		slotinfop->event_mask = (uint32_t)0xFFFFFFFF;

		/* set default auto configuration enabled flag for this slot */
		slotinfop->slot_flags = pcihp_autocfg_enabled;

		/* copy the slot information */
		slotinfop->name =
		    (char *)kmem_alloc(strlen(slot_info->pci_slot_name) + 1,
					KM_SLEEP);
		(void) strcpy(slotinfop->name, slot_info->pci_slot_name);
		slotinfop->slot_type = slot_info->slot_type;
		slotinfop->slot_capabilities = slot_info->pci_slot_capabilities;

		PCIHP_DEBUG((CE_NOTE,
		    "pcihp (%s%d): pci slot (dev %x) ONLINE\n",
		    ddi_driver_name(dip), ddi_get_instance(dip), pci_dev));

		/*
		 * The slot may have an occupant that was configured
		 * at boot time. If we find a devinfo node in the tree
		 * for this slot (i.e pci device number) then we
		 * record the occupant state as CONFIGURED.
		 */
		if (pcihp_devi_find(dip, pci_dev, 0) != NULL) {
			/* we have a configured occupant */
			slotinfop->ostate = AP_OSTATE_CONFIGURED;
			slotinfop->rstate = AP_RSTATE_CONNECTED;
			slotinfop->condition = AP_COND_OK;
			/* tell HPC driver that the occupant is configured */
			(void) hpc_nexus_control(slotinfop->slot_hdl,
				HPC_CTRL_DEV_CONFIGURED, NULL);
		} else {
			struct pcihp_config_ctrl ctrl;
			uint_t circular_count;

			slotinfop->ostate = AP_OSTATE_UNCONFIGURED;
			slotinfop->rstate = AP_RSTATE_EMPTY;

			/*
			 * We enable power to the slot and try to
			 * configure if there is any card present.
			 *
			 * Note: This case is possible if the BIOS or
			 * firmware doesn't enable the slots during
			 * soft reboot.
			 */

			if (hpc_nexus_connect(slotinfop->slot_hdl,
				NULL, 0) != HPC_SUCCESS)
				break;

			/*
			 * Call the configurator to configure the card.
			 */
			if (pcicfg_configure(dip, pci_dev) != PCICFG_SUCCESS) {
				/*
				 * call HPC driver to turn off the power for
				 * the slot.
				 */
				(void) hpc_nexus_disconnect(slotinfop->slot_hdl,
							NULL, 0);
			} else {
			    /* record the occupant state as CONFIGURED */
			    slotinfop->ostate = AP_OSTATE_CONFIGURED;
			    slotinfop->rstate = AP_RSTATE_CONNECTED;
			    slotinfop->condition = AP_COND_OK;

			    /* now, online all the devices in the AP */
			    ctrl.flags = PCIHP_CFG_CONTINUE;
			    ctrl.rv = NDI_SUCCESS;
			    ctrl.dip = NULL;
			    ctrl.pci_dev = pci_dev;
			    ctrl.op = PCIHP_ONLINE;

			    i_ndi_block_device_tree_changes(&circular_count);
			    ddi_walk_devs(ddi_get_child(dip), pcihp_configure,
				(void *)&ctrl);
			    i_ndi_allow_device_tree_changes(circular_count);

			    if (ctrl.rv != NDI_SUCCESS) {
				/*
				 * one or more of the devices are not
				 * ONLINE'd. How is this to be
				 * reported?
				 */
				cmn_err(CE_WARN,
					"pcihp (%s%d): failed to attach one or"
					" more drivers for the card in"
					" the slot %s",
					ddi_driver_name(dip),
					ddi_get_instance(dip),
					slotinfop->name);
			    }

			    /* tell HPC driver about the configured occupant */
			    (void) hpc_nexus_control(slotinfop->slot_hdl,
				HPC_CTRL_DEV_CONFIGURED, NULL);
			}
		}

		break;

	case HPC_SLOT_OFFLINE:
		/*
		 * A hot plug slot is being removed from the bus.
		 * Make sure there is no occupant configured on the
		 * slot before removing the AP minor node.
		 */
		if (slotinfop->ostate != AP_OSTATE_UNCONFIGURED) {
		    cmn_err(CE_WARN, "pcihp (%s%d): Card is still in configured"
			" state for pci dev %x",
			ddi_driver_name(dip), ddi_get_instance(dip), pci_dev);
		    rv = HPC_ERR_FAILED;
		    break;
		}

		/*
		 * If the AP device is in open state then return
		 * error.
		 */
		if (pcihp_p->soft_state != PCIHP_SOFT_STATE_CLOSED) {
		    rv = HPC_ERR_FAILED;
		    break;
		}

		/* remove the minor node */
		ddi_remove_minor_node(dip, slotinfop->name);

		/* free up the memory for the name string */
		kmem_free(slotinfop->name, strlen(slotinfop->name) + 1);

		/* update the slot info data */
		slotinfop->name = NULL;
		slotinfop->slot_hdl = NULL;

		PCIHP_DEBUG((CE_NOTE,
		    "pcihp (%s%d): pci slot (dev %x) OFFLINE\n",
		    ddi_driver_name(dip), ddi_get_instance(dip),
		    slot_info->pci_dev_num));

		break;
	default:
		cmn_err(CE_WARN,
			"pcihp_new_slot_state: unknown slot_state %d\n",
			slot_state);
		rv = HPC_ERR_FAILED;
	}

	mutex_exit(&slotinfop->slot_mutex);

	return (rv);
}

/*
 * Event handler. It is assumed that this function is called from
 * a kernel context only.
 *
 * Parameters:
 *	slot_arg	AP minor number.
 *	event_mask	Event that occured.
 */

static int
pcihp_event_handler(caddr_t slot_arg, uint_t event_mask)
{
	int ap_minor = (int)slot_arg;
	pcihp_t *pcihp_p;
	int pcihp_instance;
	int pci_dev;
	int rv = HPC_EVENT_CLAIMED;
	struct pcihp_slotinfo *slotinfop;
	struct pcihp_config_ctrl ctrl;
	uint_t circular_count;

	/*
	 * get the soft state structure for the bus instance.
	 */
	pcihp_instance = AP_MINOR_NUM_TO_PCIHP_INSTANCE(ap_minor);
	pcihp_p = (pcihp_t *)ddi_get_soft_state(pcihp_state, pcihp_instance);

	/* get the PCI device number for the slot */
	pci_dev = AP_MINOR_NUM_TO_PCI_DEVNUM(ap_minor);

	slotinfop = &pcihp_p->slotinfo[pci_dev];

	mutex_enter(&slotinfop->slot_mutex);

	switch (event_mask) {

	case HPC_EVENT_SLOT_INSERTION:
		/*
		 * A card is inserted in the slot. Just report this
		 * event and return.
		 */
		cmn_err(CE_NOTE, "pcihp (%s%d): card is inserted"
			" in the slot %s (pci dev %x)",
			ddi_driver_name(pcihp_p->dip),
			ddi_get_instance(pcihp_p->dip),
			slotinfop->name, pci_dev);

		/* +++ HOOK for RCM to report this hotplug event? +++ */

		break;

	case HPC_EVENT_SLOT_CONFIGURE:
		/*
		 * Configure the occupant that is just inserted in the slot.
		 * The receptacle may or may not be in the connected state. If
		 * the receptacle is not connected and the auto configuration
		 * is enabled on this slot then connect the slot. If auto
		 * configuration is enabled then configure the card.
		 */
		if ((slotinfop->slot_flags & PCIHP_SLOT_AUTO_CFG_EN) == 0) {
			/*
			 * auto configuration is disabled. Tell someone
			 * like RCM about this hotplug event?
			 */
			cmn_err(CE_NOTE, "pcihp (%s%d): SLOT_CONFIGURE event"
				" occured for pci dev %x (slot %s)",
				ddi_driver_name(pcihp_p->dip),
				ddi_get_instance(pcihp_p->dip), pci_dev,
				slotinfop->name);

			/* +++ HOOK for RCM to report this hotplug event? +++ */

			break;
		}

		ASSERT(slotinfop->ostate == AP_OSTATE_UNCONFIGURED);

		/*
		 * Auto configuration is enabled. First, make sure the
		 * receptacle is in the CONNECTED state.
		 */
		if ((rv = hpc_nexus_connect(slotinfop->slot_hdl,
		    NULL, 0)) == HPC_SUCCESS) {
		    slotinfop->rstate = AP_RSTATE_CONNECTED; /* record rstate */
		}

		/*
		 * Call the configurator to configure the card.
		 */
		if (pcicfg_configure(pcihp_p->dip, pci_dev) != PCICFG_SUCCESS) {
			/* failed to configure the card */
			cmn_err(CE_WARN, "pcihp (%s%d): failed to configure"
				" the card in the slot %s",
				ddi_driver_name(pcihp_p->dip),
				ddi_get_instance(pcihp_p->dip),
				slotinfop->name);
			/* failed to configure; disconnect the slot */
			if (hpc_nexus_disconnect(slotinfop->slot_hdl,
			    NULL, 0) == HPC_SUCCESS) {
			    slotinfop->rstate = AP_RSTATE_DISCONNECTED;
			}
		} else {
			/* record the occupant state as CONFIGURED */
			slotinfop->ostate = AP_OSTATE_CONFIGURED;
			slotinfop->condition = AP_COND_OK;

			/* now, online all the devices in the AP */
			ctrl.flags = PCIHP_CFG_CONTINUE;
			ctrl.rv = NDI_SUCCESS;
			ctrl.dip = NULL;
			ctrl.pci_dev = pci_dev;
			ctrl.op = PCIHP_ONLINE;

			i_ndi_block_device_tree_changes(&circular_count);
			ddi_walk_devs(ddi_get_child(pcihp_p->dip),
				pcihp_configure, (void *)&ctrl);
			i_ndi_allow_device_tree_changes(circular_count);

			if (ctrl.rv != NDI_SUCCESS) {
				/*
				 * one or more of the devices are not
				 * ONLINE'd. How is this to be
				 * reported?
				 */
				cmn_err(CE_WARN,
					"pcihp (%s%d): failed to attach one or"
					" more drivers for the card in"
					" the slot %s",
					ddi_driver_name(pcihp_p->dip),
					ddi_get_instance(pcihp_p->dip),
					slotinfop->name);
			}

			/* tell HPC driver that the occupant is configured */
			(void) hpc_nexus_control(slotinfop->slot_hdl,
				HPC_CTRL_DEV_CONFIGURED, NULL);

			cmn_err(CE_NOTE, "pcihp (%s%d): card is CONFIGURED"
				" in the slot %s (pci dev %x)",
				ddi_driver_name(pcihp_p->dip),
				ddi_get_instance(pcihp_p->dip),
				slotinfop->name, pci_dev);
		}

		break;

	case HPC_EVENT_SLOT_UNCONFIGURE:
		/*
		 * Unconfigure the occupant in this slot.
		 */
		if ((slotinfop->slot_flags & PCIHP_SLOT_AUTO_CFG_EN) == 0) {
			/*
			 * auto configuration is disabled. Tell someone
			 * like RCM about this hotplug event?
			 */
			cmn_err(CE_NOTE, "pcihp (%s%d): SLOT_UNCONFIGURE event"
				" occured for pci dev %x (slot %s)",
				ddi_driver_name(pcihp_p->dip),
				ddi_get_instance(pcihp_p->dip), pci_dev,
				slotinfop->name);

			/* +++ HOOK for RCM to report this hotplug event? +++ */

			break;
		}

		/*
		 * If the occupant is in the CONFIGURED state then
		 * call the configurator to unconfigure the slot.
		 */
		if (slotinfop->ostate == AP_OSTATE_CONFIGURED) {
			/*
			 * Detach all the drivers for the devices in the
			 * slot. Call pcihp_configure() to offline the
			 * devices.
			 */
			ctrl.flags = 0;
			ctrl.rv = NDI_SUCCESS;
			ctrl.dip = NULL;
			ctrl.pci_dev = pci_dev;
			ctrl.op = PCIHP_OFFLINE;

			i_ndi_block_device_tree_changes(&circular_count);
			ddi_walk_devs(ddi_get_child(pcihp_p->dip),
				pcihp_configure, (void *)&ctrl);
			i_ndi_allow_device_tree_changes(circular_count);

			if (ctrl.rv != NDI_SUCCESS) {
				/*
				 * Failed to detach one or more drivers.
				 * Restore the status for the drivers
				 * which are offlined during this step.
				 */
				ctrl.flags = PCIHP_CFG_CONTINUE;
				ctrl.rv = NDI_SUCCESS;
				ctrl.dip = NULL;
				ctrl.pci_dev = pci_dev;
				ctrl.op = PCIHP_ONLINE;
				i_ndi_block_device_tree_changes(
							&circular_count);
				ddi_walk_devs(ddi_get_child(pcihp_p->dip),
					pcihp_configure, (void *)&ctrl);
				i_ndi_allow_device_tree_changes(circular_count);
				rv = HPC_ERR_FAILED;
			} else if (pcicfg_unconfigure(pcihp_p->dip,
				pci_dev) == PCICFG_SUCCESS) {
				slotinfop->ostate = AP_OSTATE_UNCONFIGURED;
				slotinfop->condition = AP_COND_UNKNOWN;
				/*
				 * send the notification of state change
				 * to the HPC driver.
				 */
				(void) hpc_nexus_control(slotinfop->slot_hdl,
					HPC_CTRL_DEV_UNCONFIGURED, NULL);
				/* disconnect the slot */
				if (hpc_nexus_disconnect(slotinfop->slot_hdl,
				    NULL, 0) == HPC_SUCCESS) {
				    slotinfop->rstate = AP_RSTATE_DISCONNECTED;
				}

				cmn_err(CE_NOTE,
					"pcihp (%s%d): card is UNCONFIGURED"
					" in the slot %s (pci dev %x)",
					ddi_driver_name(pcihp_p->dip),
					ddi_get_instance(pcihp_p->dip),
					slotinfop->name, pci_dev);
			} else {
				rv = HPC_ERR_FAILED;
			}
		}

		/* +++ HOOK for RCM to report this hotplug event? +++ */

		break;

	case HPC_EVENT_SLOT_REMOVAL:
		/*
		 * Card is removed from the slot. The card must have been
		 * unconfigured before this event.
		 */
		if (slotinfop->ostate != AP_OSTATE_UNCONFIGURED) {
			cmn_err(CE_PANIC, "pcihp (%s%d): card is removed from"
				" the slot %s before doing unconfigure!!",
				ddi_driver_name(pcihp_p->dip),
				ddi_get_instance(pcihp_p->dip),
				slotinfop->name);
			/* any error recovery? */
			break;
		}

		cmn_err(CE_NOTE, "pcihp (%s%d): card is removed"
			" from the slot %s",
			ddi_driver_name(pcihp_p->dip),
			ddi_get_instance(pcihp_p->dip),
			slotinfop->name);

		/* +++ HOOK for RCM to report this hotplug event? +++ */

		break;

	case HPC_EVENT_SLOT_POWER_ON:
		/*
		 * Slot is connected to the bus. i.e the card is powered
		 * on. Are there any error conditions to be checked?
		 */
		cmn_err(CE_NOTE, "pcihp (%s%d): card is powered"
			" on in the slot %s",
			ddi_driver_name(pcihp_p->dip),
			ddi_get_instance(pcihp_p->dip),
			slotinfop->name);

		slotinfop->rstate = AP_RSTATE_CONNECTED; /* record rstate */

		/* +++ HOOK for RCM to report this hotplug event? +++ */

		break;

	case HPC_EVENT_SLOT_POWER_OFF:
		/*
		 * Slot is disconnected from the bus. i.e the card is powered
		 * off. Are there any error conditions to be checked?
		 */
		cmn_err(CE_NOTE, "pcihp (%s%d): card is powered"
			" off in the slot %s",
			ddi_driver_name(pcihp_p->dip),
			ddi_get_instance(pcihp_p->dip),
			slotinfop->name);

		slotinfop->rstate = AP_RSTATE_DISCONNECTED; /* record rstate */

		/* +++ HOOK for RCM to report this hotplug event? +++ */

		break;

	case HPC_EVENT_SLOT_LATCH_SHUT:
		/*
		 * Latch on the slot is closed.
		 */
		cmn_err(CE_NOTE, "pcihp (%s%d): latch is shut"
			" for the slot %s",
			ddi_driver_name(pcihp_p->dip),
			ddi_get_instance(pcihp_p->dip),
			slotinfop->name);

		/* +++ HOOK for RCM to report this hotplug event? +++ */

		break;

	case HPC_EVENT_SLOT_LATCH_OPEN:
		/*
		 * Latch on the slot is open.
		 */
		cmn_err(CE_NOTE, "pcihp (%s%d): latch is open"
			" for the slot %s",
			ddi_driver_name(pcihp_p->dip),
			ddi_get_instance(pcihp_p->dip),
			slotinfop->name);

		/* +++ HOOK for RCM to report this hotplug event? +++ */

		break;

	case HPC_EVENT_SLOT_ENUM:
		/*
		 * ENUM signal occured on the bus. It may be from this
		 * slot or any other hotplug slot on the bus.
		 */
		PCIHP_DEBUG((CE_NOTE, "pcihp (%s%d): ENUM# is generated"
			" on the bus (for slot %s ??)",
			ddi_driver_name(pcihp_p->dip),
			ddi_get_instance(pcihp_p->dip),
			slotinfop->name));

		mutex_exit(&slotinfop->slot_mutex);
		pcihp_handle_enum(pcihp_p, pci_dev);
		mutex_enter(&slotinfop->slot_mutex);

		/* +++ HOOK for RCM to report this hotplug event? +++ */

		break;

	case HPC_EVENT_SLOT_BLUE_LED_ON:

		/*
		 * Request to turn Hot Swap Blue LED on.
		 */
		PCIHP_DEBUG((CE_NOTE, "pcihp (%s%d): Request To Turn On Blue "
			"LED on the bus (for slot %s ??)",
			ddi_driver_name(pcihp_p->dip),
			ddi_get_instance(pcihp_p->dip),
			slotinfop->name));

		pcihp_turn_on_blue_led(pcihp_p, pci_dev);

		break;

	case HPC_EVENT_SLOT_BLUE_LED_OFF:

		/*
		 * Request to turn Hot Swap Blue LED off.
		 */
		PCIHP_DEBUG((CE_NOTE, "pcihp (%s%d): Request To Turn Off Blue "
			"LED on the bus (for slot %s ??)",
			ddi_driver_name(pcihp_p->dip),
			ddi_get_instance(pcihp_p->dip),
			slotinfop->name));

		pcihp_turn_off_blue_led(pcihp_p, pci_dev);

		break;

	case HPC_EVENT_SLOT_NOT_HEALTHY:
		/*
		 * HEALTHY# signal on this slot is not OK.
		 */
		cmn_err(CE_NOTE, "pcihp (%s%d): HEALTHY# signal is not OK"
			" for this slot %s",
			ddi_driver_name(pcihp_p->dip),
			ddi_get_instance(pcihp_p->dip),
			slotinfop->name);

		/* record the state in slot_flags field */
		slotinfop->slot_flags |= PCIHP_SLOT_NOT_HEALTHY;
		slotinfop->condition = AP_COND_FAILING;

		/* +++ HOOK for RCM to report this hotplug event? +++ */

		break;

	case HPC_EVENT_SLOT_HEALTHY_OK:
		/*
		 * HEALTHY# signal on this slot is OK now.
		 */
		cmn_err(CE_NOTE, "pcihp (%s%d): HEALTHY# signal is OK now"
			" for this slot %s",
			ddi_driver_name(pcihp_p->dip),
			ddi_get_instance(pcihp_p->dip),
			slotinfop->name);

		/* update the state in slot_flags field */
		slotinfop->slot_flags &= ~PCIHP_SLOT_NOT_HEALTHY;
		slotinfop->condition = AP_COND_OK;

		/* +++ HOOK for RCM to report this hotplug event? +++ */

		break;

	default:
		cmn_err(CE_NOTE, "pcihp (%s%d): unknown event %x"
			" for this slot %s",
			ddi_driver_name(pcihp_p->dip),
			ddi_get_instance(pcihp_p->dip), event_mask,
			slotinfop->name);

		/* +++ HOOK for RCM to report this hotplug event? +++ */

		break;
	}

	mutex_exit(&slotinfop->slot_mutex);

	return (rv);
}

/*
 * This function is called to online or offline the devices for an
 * attachment point. If the PCI device number of the node matches
 * with the device number of the specified hot plug slot then
 * the operation is performed.
 */
static int
pcihp_configure(dev_info_t *dip, void *hdl)
{
	int pci_dev;
	struct pcihp_config_ctrl *ctrl = (struct pcihp_config_ctrl *)hdl;
	int rv;
	pci_regspec_t *pci_rp;
	int length;

	/*
	 * Get the PCI device number information from the devinfo
	 * node. Since the node may not have the address field
	 * setup (this is done in the DDI_INITCHILD of the parent)
	 * we look up the 'reg' property to decode that information.
	 */
	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
		DDI_PROP_DONTPASS, "reg", (int **)&pci_rp,
		(uint_t *)&length) != DDI_PROP_SUCCESS) {
		ctrl->rv = DDI_FAILURE;
		ctrl->dip = dip;
		return (DDI_WALK_TERMINATE);
	}

	/* get the pci device id information */
	pci_dev = PCI_REG_DEV_G(pci_rp->pci_phys_hi);

	/*
	 * free the memory allocated by ddi_prop_lookup_int_array
	 */
	ddi_prop_free(pci_rp);

	/*
	 * Match the node for the device number of the slot.
	 */
	if (pci_dev == ctrl->pci_dev) {	/* node is a match */
		if (ctrl->op == PCIHP_ONLINE) {
			/* it is CONFIGURE operation */
			rv = ndi_devi_online(dip, NDI_ONLINE_ATTACH|NDI_CONFIG);
		} else {
			/*
			 * it is UNCONFIGURE operation.
			 * (Note: we restrict the use of NDI_DEVI_FORCE flag
			 * only for network devices.)
			 */
			rv = pcihp_unconfigure_node(dip);
		}
		if (rv != NDI_SUCCESS) {
			/* failed to attach/detach the driver(s) */
			ctrl->rv = rv;
			ctrl->dip = dip;
			/* terminate the search if specified */
			if (!(ctrl->flags & PCIHP_CFG_CONTINUE))
				return (DDI_WALK_TERMINATE);
		}
	}

	/*
	 * continue the walk to the next sibling to look for a match
	 * or to find other nodes if this card is a multi-function card.
	 */
	return (DDI_WALK_PRUNECHILD);
}

/*
 * Unconfigure/Offline the device node in a bottom up fashion by
 * calling ndi_devi_offline(). If a device node is a streams device
 * then pass NDI_DEVI_FORCE flag.
 */
static int
pcihp_unconfigure_node(dev_info_t *dip)
{
	dev_info_t *child;
	int rv;

	child = ddi_get_child(dip);

	while (child) { /* offline the children first */
		rv = pcihp_unconfigure_node(child);
		if (rv != NDI_SUCCESS)
			return (rv);
		child = ddi_get_next_sibling(child);
	}

	if (ddi_streams_driver(dip) == DDI_SUCCESS)
		rv = ndi_devi_offline(dip, NDI_DEVI_FORCE);
	else
		rv = ndi_devi_offline(dip, 0);

	return (rv);
}

/* control structure used to find a device in the devinfo tree */
struct pcihp_find_ctrl {
	uint_t		device;
	uint_t		function;
	dev_info_t	*dip;
};

static dev_info_t *
pcihp_devi_find(dev_info_t *dip, uint_t device, uint_t function)
{
	struct pcihp_find_ctrl ctrl;
	uint_t count;

	ctrl.device = device;
	ctrl.function = function;
	ctrl.dip = NULL;

	i_ndi_block_device_tree_changes(&count);
	ddi_walk_devs(ddi_get_child(dip), pcihp_match_dev, (void *)&ctrl);
	i_ndi_allow_device_tree_changes(count);

	return (ctrl.dip);
}

static int
pcihp_match_dev(dev_info_t *dip, void *hdl)
{
	struct pcihp_find_ctrl *ctrl = (struct pcihp_find_ctrl *)hdl;
	pci_regspec_t *pci_rp;
	int length;
	int pci_dev;
	int pci_func;

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
		DDI_PROP_DONTPASS, "reg", (int **)&pci_rp,
		(uint_t *)&length) != DDI_PROP_SUCCESS) {
		ctrl->dip = NULL;
		return (DDI_WALK_TERMINATE);
	}

	/* get the PCI device address info */
	pci_dev = PCI_REG_DEV_G(pci_rp->pci_phys_hi);
	pci_func = PCI_REG_FUNC_G(pci_rp->pci_phys_hi);

	/*
	 * free the memory allocated by ddi_prop_lookup_int_array
	 */
	ddi_prop_free(pci_rp);


	if ((pci_dev == ctrl->device) && (pci_func == ctrl->function)) {
		/* found the match for the specified device address */
		ctrl->dip = dip;
		return (DDI_WALK_TERMINATE);
	}

	/*
	 * continue the walk to the next sibling to look for a match.
	 */
	return (DDI_WALK_PRUNECHILD);
}

#if 0
/*
 * Probe the configuration space of the slot to determine the receptacle
 * state. There may not be any devinfo tree created for this slot.
 */
static void
pcihp_probe_slot_state(dev_info_t *dip, int dev, hpc_slot_state_t *rstatep)
{
	/* XXX FIX IT */
}
#endif

/*
 * This routine is called when a ENUM# assertion is detected for a bus.
 * Since ENUM# may be bussed, the slot that asserted ENUM# may not be known.
 * The HPC Driver passes the handle of a slot that is its best guess.
 * If the best guess slot is the one that asserted ENUM#, the proper handling
 * will be done.  If its not, all possible slots will be lokked at until
 * one that is asserting ENUM is found.
 */
static void
pcihp_handle_enum(pcihp_t *pcihp_p, int favorite_pci_dev)
{
	struct pcihp_slotinfo *slotinfop;
	int pci_dev;

	/*
	 * Handle ENUM# condition for the "favorite" slot first.
	 */
	slotinfop = &pcihp_p->slotinfo[favorite_pci_dev];
	mutex_enter(&slotinfop->slot_mutex);

	/*
	 * First try the "favorite" pci device.  This is the device
	 * associated with the handle passed by the HPC Driver.
	 */
	if ((pcihp_enum_slot(pcihp_p, slotinfop, favorite_pci_dev)) ==
	    DDI_WALK_TERMINATE) {
		mutex_exit(&slotinfop->slot_mutex);
		return;
	}

	/*
	 * If ENUM# is implemented as a radial signal, then there is no
	 * need to further poll the slots.
	 */
	if (pcihp_p->bus_flags & PCIHP_BUS_ENUM_RADIAL) {
		if (hpc_nexus_control(slotinfop->slot_hdl,
		    HPC_CTRL_DISABLE_ENUM, NULL) != HPC_SUCCESS) {
			cmn_err(CE_NOTE, "pcihp (%s%d): Can not disable "
			    "ENUM# on the bus (for slot %s ??)",
			    ddi_driver_name(pcihp_p->dip),
			    ddi_get_instance(pcihp_p->dip), slotinfop->name);
		}
		mutex_exit(&slotinfop->slot_mutex);
		return;
	}

	mutex_exit(&slotinfop->slot_mutex);

	/*
	 * If the "favorite" pci device didn't assert ENUM#, then
	 * try the rest.  Once we find and handle a device that asserted
	 * ENUM#, then we will terminate the walk by returning.
	 */
	for (pci_dev = 0; pci_dev < PCI_MAX_DEVS; pci_dev++) {
		if (pci_dev != favorite_pci_dev) {
			slotinfop = &pcihp_p->slotinfo[pci_dev];
			if (slotinfop == NULL) {
				continue;
			}
			mutex_enter(&slotinfop->slot_mutex);
			if ((pcihp_enum_slot(pcihp_p, slotinfop, pci_dev)) ==
			    DDI_WALK_TERMINATE) {
#ifdef CPIC_ENUM_DEBUG
				if (pcihp_debug_scan_all == 0) {
					mutex_exit(&slotinfop->slot_mutex);
					return;
				}
#else
				mutex_exit(&slotinfop->slot_mutex);
				return;
#endif
			}
			mutex_exit(&slotinfop->slot_mutex);
		}
	}

	/*
	 * Can not find the slot that asserted ENUM#, so we assume
	 * that the responsible adapter is implemented with Non
	 * Hot Swap Firiendly Silicon.  Since we can not support
	 * these adapters we must disable the ENUM# interrupt
	 * for the whole bus segment.
	 */
	slotinfop = &pcihp_p->slotinfo[favorite_pci_dev];
	mutex_enter(&slotinfop->slot_mutex);

	PCIHP_DEBUG((CE_NOTE, "pcihp (%s%d): Non Hot Swap Friendly Device "
	    "on the bus (for slot %s ??)", ddi_driver_name(pcihp_p->dip),
	    ddi_get_instance(pcihp_p->dip),  slotinfop->name));

	if (hpc_nexus_control(slotinfop->slot_hdl,
	    HPC_CTRL_DISABLE_ENUM, NULL) != 0) {

		cmn_err(CE_NOTE, "pcihp (%s%d): Can not disable ENUM# "
		    "on the bus (for slot %s ??)",
		    ddi_driver_name(pcihp_p->dip),
		    ddi_get_instance(pcihp_p->dip), slotinfop->name);
	}
	mutex_exit(&slotinfop->slot_mutex);
}

/*
 * This routine attempts to handle a possible ENUM# assertion case for a
 * specified slot.  This only works for adapters that implement Hot Swap
 * Friendly Silicon.  If the slot's HS_CSR is read and it specifies ENUM#
 * has been asserted, either the insertion or removal handlers will be
 * called.
 */
static int
pcihp_enum_slot(pcihp_t *pcihp_p, struct pcihp_slotinfo *slotinfop, int pci_dev)
{
	ddi_acc_handle_t handle;
	dev_info_t *dip, *new_child = NULL;
	int result, bus, len, rv;
	uint8_t hs_csr;
	struct bus_range {
		uint32_t lo;
		uint32_t hi;
	} pci_bus_range;

	dip = pcihp_devi_find(pcihp_p->dip, pci_dev, 0);

	if (dip) {
		if (pci_config_setup(dip, &handle) != DDI_SUCCESS) {
			return (DDI_WALK_CONTINUE);
		}
	} else {
		/*
		 * If there is no dip then we need to see if an
		 * adapter has just been hot plugged.
		 */
		len = sizeof (struct bus_range);
		if (ddi_getlongprop_buf(DDI_DEV_T_NONE, pcihp_p->dip,
		    DDI_PROP_DONTPASS, "bus-range",
		    (caddr_t)&pci_bus_range, &len) != DDI_SUCCESS) {

			return (DDI_WALK_CONTINUE);
		}

		/* primary bus number of this bus node */
		bus = pci_bus_range.lo;

		if (ndi_devi_alloc(pcihp_p->dip, DEVI_PSEUDO_NEXNAME,
		    (dnode_t)DEVI_SID_NODEID, &new_child) != NDI_SUCCESS) {

			PCIHP_DEBUG((CE_NOTE,
			    "Failed to alloc test node\n"));

			return (DDI_WALK_CONTINUE);
		}

		if (pcihp_add_dummy_reg_property(new_child, bus,
		    pci_dev, 0) != DDI_SUCCESS) {

			return (DDI_WALK_CONTINUE);
		}

		if (pci_config_setup(new_child, &handle) != DDI_SUCCESS) {
			return (DDI_WALK_CONTINUE);
		}

		/*
		 * See if there is any PCI HW at this location
		 * by reading the Vendor ID.  If it returns with 0xffff
		 * then there is no hardware at this location.
		 */
		if (pci_config_get16(handle, 0) == 0xffff) {
			pci_config_teardown(&handle);
			return (DDI_WALK_CONTINUE);
		}

		/* We found some new PCI HW */
	}

	rv = DDI_WALK_CONTINUE;

	/*
	 * Read the device's HS_CSR.
	 */
	result = pcihp_get_hs_csr(slotinfop, handle, (uint8_t *)&hs_csr);

	if (result == PCIHP_SUCCESS) {

		/*
		 * This device supports Full Hot Swap and implements
		 * the Hot Swap Control and Status Resigter.
		 */
		if (hs_csr & HS_CSR_INS) {
			/* handle insertion ENUM */
			PCIHP_DEBUG((CE_NOTE, "pcihp (%s%d): "
			    "Handle Insertion ENUM (INS) "
			    "on the bus (for slot %s ??)",
			    ddi_driver_name(pcihp_p->dip),
			    ddi_get_instance(pcihp_p->dip),
			    slotinfop->name));

			pcihp_handle_enum_insertion(pcihp_p,
			    pci_dev, handle);

			rv = DDI_WALK_TERMINATE;

		} else if (hs_csr & HS_CSR_EXT) {
			/* handle extraction ENUM */
			PCIHP_DEBUG((CE_NOTE, "pcihp (%s%d): "
			    "Handle Extraction ENUM (EXT) "
			    "on the bus (for slot %s ??)",
			    ddi_driver_name(pcihp_p->dip),
			    ddi_get_instance(pcihp_p->dip),
			    slotinfop->name));

			pcihp_handle_enum_extraction(pcihp_p,
			    pci_dev, handle);

			rv = DDI_WALK_TERMINATE;
		}
	}

	pci_config_teardown(&handle);

	/*
	 * If new_child is not NULL then a dummy node was created
	 * for a device that does not yet have a device node in
	 * order to do PCI configuration cycles.  Since we no longer
	 * need this node, it can be freed.
	 */
	if (new_child) {
		(void) ndi_devi_free(new_child);
		new_child = NULL;
	}

	return (rv);
}

/*
 * This routine is called when a ENUM# caused by lifting the lever
 * is detected.  If the occupant is configured, it will be unconfigured.
 * If the occupant is already unconfigured or is succesfully unconfigured,
 * the blue LED on the adapter is illuminated which means its OK to remove.
 */
static void
pcihp_handle_enum_extraction(pcihp_t *pcihp_p, int pci_dev,
    ddi_acc_handle_t config_handle)
{
	struct pcihp_config_ctrl ctrl;
	struct pcihp_slotinfo *slotinfop;
	uint_t circular_count;

	slotinfop = &pcihp_p->slotinfo[pci_dev];

	if (slotinfop->ostate == AP_OSTATE_CONFIGURED) {
		/*
		 * Detach all the drivers for the devices in the
		 * slot. Call pcihp_configure() to offline the
		 * devices.
		 */
		ctrl.flags = PCIHP_CFG_CONTINUE;
		ctrl.rv = NDI_SUCCESS;
		ctrl.dip = NULL;
		ctrl.pci_dev = pci_dev;
		ctrl.op = PCIHP_OFFLINE;

		i_ndi_block_device_tree_changes(&circular_count);
		ddi_walk_devs(ddi_get_child(pcihp_p->dip),
			pcihp_configure, (void *)&ctrl);
		i_ndi_allow_device_tree_changes(circular_count);

		if (ctrl.rv != NDI_SUCCESS) {
			return;
		}
		if (pcicfg_unconfigure(pcihp_p->dip,
		    pci_dev) == PCICFG_SUCCESS) {

			/*
			 * Clear ENUM# Condition.
			 */
			pcihp_clear_EXT(slotinfop, config_handle);

			slotinfop->ostate = AP_OSTATE_UNCONFIGURED;
			slotinfop->condition = AP_COND_UNKNOWN;
			/*
			 * send the notification of state change
			 * to the HPC driver.
			 */
			(void) hpc_nexus_control(slotinfop->slot_hdl,
				HPC_CTRL_DEV_UNCONFIGURED, NULL);
		} else {
			cmn_err(CE_WARN, "pcihp (%s%d): failed to unconfigure"
				" the card in the slot %s",
				ddi_driver_name(pcihp_p->dip),
				ddi_get_instance(pcihp_p->dip),
				slotinfop->name);

			return;
		}
	}
}

/*
 * This routine is called when a ENUM# caused by whwn an adapter insertion
 * is detected.  If the occupant is succefully configured (i.e. PCI resources
 * successfully assigned, the blue LED is left off, otherwise if configuration
 * is not successful, the blue LED is illuminated.
 */
static void
pcihp_handle_enum_insertion(pcihp_t *pcihp_p, int pci_dev,
    ddi_acc_handle_t config_handle)
{
	struct pcihp_config_ctrl ctrl;
	struct pcihp_slotinfo *slotinfop;
	uint_t circular_count;

	slotinfop = &pcihp_p->slotinfo[pci_dev];

	slotinfop->hs_csr_location = 0;

	if (slotinfop->ostate == AP_OSTATE_UNCONFIGURED) {

		/*
		 * Call the configurator to configure the card.
		 */
		if (pcicfg_configure(pcihp_p->dip, pci_dev) != PCICFG_SUCCESS) {
			/* failed to configure the card */
			cmn_err(CE_WARN, "pcihp (%s%d): failed to configure"
				" the card in the slot %s",
				ddi_driver_name(pcihp_p->dip),
				ddi_get_instance(pcihp_p->dip),
				slotinfop->name);

			return;
		} else {
			/*
			 * Clear ENUM# Condition.
			 */
			pcihp_clear_INS(slotinfop, config_handle);

			/* record the occupant state as CONFIGURED */
			slotinfop->ostate = AP_OSTATE_CONFIGURED;
			slotinfop->condition = AP_COND_OK;

			/* now, online all the devices in the AP */
			ctrl.flags = PCIHP_CFG_CONTINUE;
			ctrl.rv = DDI_SUCCESS;
			ctrl.dip = NULL;
			ctrl.pci_dev = pci_dev;
			ctrl.op = PCIHP_ONLINE;

			i_ndi_block_device_tree_changes(&circular_count);
			ddi_walk_devs(ddi_get_child(pcihp_p->dip),
				pcihp_configure, (void *)&ctrl);
			i_ndi_allow_device_tree_changes(circular_count);

			if (ctrl.rv == DDI_FAILURE) {
				/*
				 * one or more of the devices are not
				 * ONLINE'd. How is this to be
				 * reported?
				 */
				cmn_err(CE_WARN,
					"pcihp (%s%d): failed to attach one or"
					" more drivers for the card in"
					" the slot %s",
					ddi_driver_name(pcihp_p->dip),
					ddi_get_instance(pcihp_p->dip),
					slotinfop->name);
				/* any error recovery? */
			}

			/* tell HPC driver that the occupant is configured */
			(void) hpc_nexus_control(slotinfop->slot_hdl,
				HPC_CTRL_DEV_CONFIGURED, NULL);
		}
	}
}

/*
 * Read the Hot Swap Control and Status Register (HS_CSR) and
 * place the result in the location pointed to be hs_csr.
 */
static int
pcihp_get_hs_csr(struct pcihp_slotinfo *slotinfop,
    ddi_acc_handle_t config_handle, uint8_t *hs_csr)
{
#ifndef CPCI_ENUM_DEBUG
	uint8_t	capbilities_ptr, capability_id, next;

	if (slotinfop->hs_csr_location == 0) {
		next = PCI_CONF_EXTCAP;

		/*
		 * Walk the list of capabilities, but don't walk past the end
		 * of the Configuration Space Header.
		 */
		while (next < PCI_CONF_HDR_SIZE) {

			capbilities_ptr = pci_config_get8(config_handle, next);

			if ((capbilities_ptr == NULL) ||
			    (capbilities_ptr == 0xff)) {
				return (PCIHP_FAILURE);
			}

			capability_id = pci_config_get8(config_handle,
			    capbilities_ptr + PCI_ECP_CAPID);

			if (capability_id == CPCI_HOTSWAP_CAPID) {
				slotinfop->hs_csr_location = capbilities_ptr +
				    PCI_ECP_HS_CSR;

				*hs_csr = pci_config_get8(config_handle,
				    slotinfop->hs_csr_location);

				return (PCIHP_SUCCESS);
			}
			next = pci_config_get8(config_handle, capbilities_ptr +
			    PCI_ECP_NEXT);
		}

		return (PCIHP_FAILURE); /* We should never hit this */
	} else {
		*hs_csr = pci_config_get8(config_handle,
		    slotinfop->hs_csr_location);
	}
	return (PCIHP_SUCCESS);
#else /* CPCI_ENUM_DEBUG */
#ifdef lint
	config_handle = config_handle;
#endif
	/*
	 * Allows us to test ENUM handling without Full Hot Swap HW.
	 */
	*hs_csr = (uint8_t)pcihp_hs_csr_debug;

	return (PCIHP_SUCCESS);

#endif /* CPCI_ENUM_DEBUG */
}

/*
 * Write the Hot Swap Control and Status Register (HS_CSR) with
 * the value being pointed at by hs_csr.
 */
static void
pcihp_set_hs_csr(struct pcihp_slotinfo *slotinfop,
    ddi_acc_handle_t config_handle, uint8_t *hs_csr)
{
	uint8_t	capbilities_ptr, capability_id, next;

	if (slotinfop->hs_csr_location == 0) {
		next = PCI_CONF_EXTCAP;

		/*
		 * Walk the list of capabilities, but don't walk past the end
		 * of the Configuration Space Header.
		 */
		while (next < PCI_CONF_HDR_SIZE) {
			capbilities_ptr = pci_config_get8(config_handle, next);

			if ((capbilities_ptr == NULL) ||
			    (capbilities_ptr == 0xff)) {
				return;
			}

			capability_id = pci_config_get8(config_handle,
			    capbilities_ptr + PCI_ECP_CAPID);

			if (capability_id == CPCI_HOTSWAP_CAPID) {
				slotinfop->hs_csr_location =
				    capbilities_ptr + PCI_ECP_HS_CSR;

				pci_config_put8(config_handle,
				    slotinfop->hs_csr_location, *hs_csr);

				return;
			}
			next = pci_config_get8(config_handle, capbilities_ptr +
			    PCI_ECP_NEXT);
		}
	} else {
		pci_config_put8(config_handle, slotinfop->hs_csr_location,
		    *hs_csr);
	}
}

/*
 * Writes a one to the EXT bit of the HS_CSR to clear the Pending Extraction
 * of board ENUM#.
 */
static void
pcihp_clear_EXT(struct pcihp_slotinfo *slotinfop,
    ddi_acc_handle_t config_handle)
{
	uint8_t hs_csr;
	int result;

	result = pcihp_get_hs_csr(slotinfop, config_handle, (uint8_t *)&hs_csr);
	if (result == PCIHP_SUCCESS) {
		hs_csr |= HS_CSR_EXT;	/* clears the extraction condition */
		pcihp_set_hs_csr(slotinfop, config_handle, (uint8_t *)&hs_csr);
	}
}

/*
 * Writes a one to the INS bit of the HS_CSR to clear the Freshly
 * Inserted Board ENUM#.
 */
static void
pcihp_clear_INS(struct pcihp_slotinfo *slotinfop,
    ddi_acc_handle_t config_handle)
{
	uint8_t hs_csr;
	int result;

	result = pcihp_get_hs_csr(slotinfop, config_handle, (uint8_t *)&hs_csr);
	if (result == PCIHP_SUCCESS) {
		hs_csr |= HS_CSR_INS;	/* clears the insertion condition */
		pcihp_set_hs_csr(slotinfop, config_handle, (uint8_t *)&hs_csr);
	}
}

/*
 * Writes a one to the LOO bit of the HS_CSR to illuminate the
 * adapter's blue LED.
 */
static void
pcihp_turn_on_blue_led(pcihp_t *pcihp_p, int pci_dev)
{
	struct pcihp_slotinfo *slotinfop;
	ddi_acc_handle_t config_handle;
	dev_info_t *dip;
	uint8_t hs_csr;
	int result;

	slotinfop = &pcihp_p->slotinfo[pci_dev];

	dip = pcihp_devi_find(pcihp_p->dip, pci_dev, 0);

	if (dip) {
		if (pci_config_setup(dip, &config_handle) != DDI_SUCCESS) {
			return;
		}

		result = pcihp_get_hs_csr(slotinfop, config_handle,
		    (uint8_t *)&hs_csr);
		if (result == PCIHP_SUCCESS) {
			hs_csr |= HS_CSR_LOO; /* illuminates blue LED */
			pcihp_set_hs_csr(slotinfop, config_handle,
			    (uint8_t *)&hs_csr);
		}
		pci_config_teardown(&config_handle);
	}
}

/*
 * Writes a zero to the LOO bit of the HS_CSR to unilluminate the
 * adapter's blue LED.
 */
static void
pcihp_turn_off_blue_led(pcihp_t *pcihp_p, int pci_dev)
{
	struct pcihp_slotinfo *slotinfop;
	ddi_acc_handle_t config_handle;
	dev_info_t *dip;
	uint8_t hs_csr;
	int result;

	slotinfop = &pcihp_p->slotinfo[pci_dev];

	dip = pcihp_devi_find(pcihp_p->dip, pci_dev, 0);

	if (dip) {
		if (pci_config_setup(dip, &config_handle) != DDI_SUCCESS) {
			return;
		}

		result = pcihp_get_hs_csr(slotinfop, config_handle,
		    (uint8_t *)&hs_csr);
		if (result == PCIHP_SUCCESS) {
			hs_csr &= ~HS_CSR_LOO; /* unilluminates blue LED */
			pcihp_set_hs_csr(slotinfop, config_handle,
			    (uint8_t *)&hs_csr);
		}
		pci_config_teardown(&config_handle);
	}
}

static int
pcihp_add_dummy_reg_property(dev_info_t *dip,
    uint_t bus, uint_t device, uint_t func)
{
	pci_regspec_t dummy_reg;

	bzero((void *)&dummy_reg, sizeof (dummy_reg));

	dummy_reg.pci_phys_hi = PCIHP_MAKE_REG_HIGH(bus, device, func, 0);

	return (ndi_prop_update_int_array(DDI_DEV_T_NONE, dip,
	    "reg", (int *)&dummy_reg, sizeof (pci_regspec_t)/sizeof (int)));
}
