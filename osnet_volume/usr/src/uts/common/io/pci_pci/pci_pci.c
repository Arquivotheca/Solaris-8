/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_pci.c	1.43	99/04/26 SMI"

/*
 *	PCI to PCI bus bridge nexus driver
 */

#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/modctl.h>
#include <sys/autoconf.h>
#include <sys/ddi_impldefs.h>
#include <sys/pci.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>

#define	PCI_HOTPLUG

#if	defined(PCI_HOTPLUG)
#include <sys/hotplug/pci/pcihp.h>
#endif

#if	defined(PCI_HOTPLUG)
/*
 * For PCI Hotplug support, the misc/pcihp module provides devctl control
 * device and cb_ops functions to support hotplug operations.
 */
char _depends_on[] = "misc/pcihp";
#endif

/*
 * The variable controls the default setting of the command register
 * for pci devices.  See ppb_initchild() for details.
 */
#if defined(i386)
static u_short ppb_command_default = PCI_COMM_ME |
					PCI_COMM_MAE |
					PCI_COMM_IO;
#else
static u_short ppb_command_default = PCI_COMM_SERR_ENABLE |
					PCI_COMM_WAIT_CYC_ENAB |
					PCI_COMM_PARITY_DETECT |
					PCI_COMM_ME |
					PCI_COMM_MAE |
					PCI_COMM_IO;
#endif


static int ppb_bus_map(dev_info_t *, dev_info_t *, ddi_map_req_t *,
	off_t, off_t, caddr_t *);
static int ppb_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t,
	void *, void *);

struct bus_ops ppb_bus_ops = {
	BUSO_REV,
	ppb_bus_map,
	i_ddi_get_intrspec,
	i_ddi_add_intrspec,
	i_ddi_remove_intrspec,
	i_ddi_map_fault,
	ddi_dma_map,
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,
	ppb_ctlops,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0	/* (*bus_post_event)();		*/
};

static int ppb_identify(dev_info_t *devi);
static int ppb_probe(dev_info_t *);
static int ppb_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int ppb_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);
static int ppb_info(dev_info_t *dip, ddi_info_cmd_t infocmd,
	void *arg, void **result);

#if	!defined(PCI_HOTPLUG)
static struct cb_ops ppb_cb_ops = {
	nulldev,			/* open */
	nulldev,			/* close */
	nulldev,			/* strategy */
	nulldev,			/* print */
	nulldev,			/* dump */
	nulldev,			/* read */
	nulldev,			/* write */
	nulldev,			/* ioctl */
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
#endif

struct dev_ops ppb_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt  */
	ppb_info,		/* info */
	ppb_identify,		/* identify */
	ppb_probe,		/* probe */
	ppb_attach,		/* attach */
	ppb_detach,		/* detach */
	nulldev,		/* reset */
#if	defined(PCI_HOTPLUG)
	&pcihp_cb_ops,		/* driver operations */
#else
	&ppb_cb_ops,		/* driver operations */
#endif
	&ppb_bus_ops		/* bus operations */

};

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module */
	"PCI to PCI bridge nexus driver",
	&ppb_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

/*
 * soft state pointer and structure template:
 */
static void *ppb_state;

typedef struct {

	dev_info_t *dip;

#ifdef sparc
	/*
	 * configuration register state for the bus:
	 */
	u_char ppb_cache_line_size;
	u_char ppb_latency_timer;
#endif

	/*
	 * cpr support:
	 */
#define	PCI_MAX_DEVICES		32
#define	PCI_MAX_FUNCTIONS	8
#define	PCI_MAX_CHILDREN	PCI_MAX_DEVICES * PCI_MAX_FUNCTIONS
	u_int config_state_index;
	struct {
		dev_info_t *dip;
		u_short command;
		u_char cache_line_size;
		u_char latency_timer;
		u_char header_type;
		u_char sec_latency_timer;
		u_short bridge_control;
	} config_state[PCI_MAX_CHILDREN];
} ppb_devstate_t;

#ifdef sparc
/*
 * The following variable enables a workaround for the following obp bug:
 *
 *	1234181 - obp should set latency timer registers in pci
 *		configuration header
 *
 * Until this bug gets fixed in the obp, the following workaround should
 * be enabled.
 */
static u_int ppb_set_latency_timer_register = 1;

/*
 * The following variable enables a workaround for an obp bug to be
 * submitted.  A bug requesting a workaround fof this problem has
 * been filed:
 *
 *	1235094 - need workarounds on positron nexus drivers to set cache
 *		line size registers
 *
 * Until this bug gets fixed in the obp, the following workaround should
 * be enabled.
 */
static u_int ppb_set_cache_line_size_register = 1;
#endif


/*
 * forward function declarations:
 */
static void ppb_removechild(dev_info_t *);
static int ppb_initchild(dev_info_t *child);
static int ppb_create_pci_prop(dev_info_t *child, uint_t *, uint_t *);
static void ppb_save_config_regs(ppb_devstate_t *ppb_p);
static void ppb_restore_config_regs(ppb_devstate_t *ppb_p);

extern int impl_ddi_merge_child(dev_info_t *child);


int
_init(void)
{
	int e;

	if ((e = ddi_soft_state_init(&ppb_state, sizeof (ppb_devstate_t),
	    1)) == 0 && (e = mod_install(&modlinkage)) != 0)
		ddi_soft_state_fini(&ppb_state);
	return (e);
}

int
_fini(void)
{
	int e;

	if ((e = mod_remove(&modlinkage)) == 0)
		ddi_soft_state_fini(&ppb_state);
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*ARGSUSED*/
static int
ppb_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
static int
ppb_identify(dev_info_t *dip)
{
	return (DDI_IDENTIFIED);
}

/*ARGSUSED*/
static int
ppb_probe(register dev_info_t *devi)
{
	return (DDI_PROBE_SUCCESS);
}

/*ARGSUSED*/
static int
ppb_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int instance;
	ppb_devstate_t *ppb;
	ddi_acc_handle_t config_handle;

	switch (cmd) {
	case DDI_ATTACH:

		/*
		 * Make sure the "device_type" property exists.
		 */
		(void) ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
			"device_type", (caddr_t)"pci", 4);

		/*
		 * Allocate and get soft state structure.
		 */
		instance = ddi_get_instance(devi);
		if (ddi_soft_state_zalloc(ppb_state, instance) != DDI_SUCCESS)
			return (DDI_FAILURE);
		ppb = (ppb_devstate_t *)ddi_get_soft_state(ppb_state, instance);
		ppb->dip = devi;
		if (pci_config_setup(devi, &config_handle) != DDI_SUCCESS) {
			ddi_soft_state_free(ppb_state, instance);
			return (DDI_FAILURE);
		}
#ifdef sparc
		ppb->ppb_cache_line_size =
			pci_config_get8(config_handle, PCI_CONF_CACHE_LINESZ);
		ppb->ppb_latency_timer =
			pci_config_get8(config_handle, PCI_CONF_LATENCY_TIMER);
#endif
		pci_config_teardown(&config_handle);

#if	defined(PCI_HOTPLUG)
		/*
		 * Initialize hotplug support on this bus. At minimum
		 * (for non hotplug bus) this would create ":devctl" minor
		 * node to support DEVCTL_DEVICE_* and DEVCTL_BUS_* ioctls
		 * to this bus.
		 */
		if (pcihp_init(devi) != DDI_SUCCESS)
		    cmn_err(CE_WARN, "pci: Failed to setup hotplug framework");
#endif
		ddi_report_dev(devi);
		return (DDI_SUCCESS);

	case DDI_RESUME:

		/*
		 * Get the soft state structure for the bridge.
		 */
		ppb = (ppb_devstate_t *)
			ddi_get_soft_state(ppb_state, ddi_get_instance(devi));
		ppb_restore_config_regs(ppb);
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

/*ARGSUSED*/
static int
ppb_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	ppb_devstate_t *ppb;

	switch (cmd) {
	case DDI_DETACH:
		(void) ddi_prop_remove(DDI_DEV_T_NONE, devi, "device_type");
		/*
		 * And finally free the per-pci soft state.
		 */
		ddi_soft_state_free(ppb_state, ddi_get_instance(devi));
#if	defined(PCI_HOTPLUG)
		/*
		 * Uninitialize hotplug support on this bus.
		 */
		(void) pcihp_uninit(devi);
#endif
		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		ppb = (ppb_devstate_t *)
			ddi_get_soft_state(ppb_state, ddi_get_instance(devi));
		ppb_save_config_regs(ppb);
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

/*ARGSUSED*/
static int
ppb_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *vaddrp)
{
	register dev_info_t *pdip;

	pdip = (dev_info_t *)DEVI(dip)->devi_parent;
	return ((DEVI(pdip)->devi_ops->devo_bus_ops->bus_map)(pdip,
			rdip, mp, offset, len, vaddrp));
}

/*ARGSUSED*/
static int
ppb_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	pci_regspec_t *drv_regp;
	int	reglen;
	int	rn;
	int	totreg;

	switch (ctlop) {
	case DDI_CTLOPS_REPORTDEV:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		cmn_err(CE_CONT, "?PCI-device: %s@%s, %s%d\n",
		    ddi_node_name(rdip), ddi_get_name_addr(rdip),
		    ddi_driver_name(rdip),
		    ddi_get_instance(rdip));
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INITCHILD:
		return (ppb_initchild((dev_info_t *)arg));

	case DDI_CTLOPS_UNINITCHILD:
		ppb_removechild((dev_info_t *)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_NINTRS:
		if (ddi_get_parent_data(rdip))
			*(int *)result = 1;
		else
			*(int *)result = 0;
		return (DDI_SUCCESS);

	case DDI_CTLOPS_XLATE_INTRS:
		return (DDI_SUCCESS);

	case DDI_CTLOPS_SIDDEV:
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		break;

	default:
		return (ddi_ctlops(dip, rdip, ctlop, arg, result));
	}

	*(int *)result = 0;
	if (ddi_getlongprop(DDI_DEV_T_NONE, rdip,
		DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, "reg",
		(caddr_t)&drv_regp, &reglen) != DDI_SUCCESS)
		return (DDI_FAILURE);

	totreg = reglen / sizeof (pci_regspec_t);
	if (ctlop == DDI_CTLOPS_NREGS)
		*(int *)result = totreg;
	else if (ctlop == DDI_CTLOPS_REGSIZE) {
		rn = *(int *)arg;
		if (rn > totreg)
			return (DDI_FAILURE);
		*(off_t *)result = drv_regp[rn].pci_size_low;
	}
	return (DDI_SUCCESS);
}

static int
ppb_initchild(dev_info_t *child)
{
	struct ddi_parent_private_data *pdptr;
	char name[MAXNAMELEN];
	int ret;
	uint_t slot, func;
	ddi_acc_handle_t config_handle;
	u_short command_preserve, command;
	char **unit_addr;
	u_int n;
#if !defined(i386)
	u_short bcr;
	u_char header_type;
#endif
#ifdef sparc
	u_char min_gnt, latency_timer;
	ppb_devstate_t *ppb;
#endif

	/*
	 * Pseudo nodes indicate a prototype node with per-instance
	 * properties to be merged into the real h/w device node.
	 * The interpretation of the unit-address is DD[,F]
	 * where DD is the device id and F is the function.
	 */
	if (ndi_dev_is_persistent_node(child) == 0) {
		if (ddi_prop_lookup_string_array(DDI_DEV_T_ANY, child,
		    DDI_PROP_DONTPASS, "unit-address", &unit_addr, &n) !=
		    DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN,
			    "cannot merge prototype from %s.conf",
			    ddi_driver_name(child));
			return (DDI_NOT_WELL_FORMED);
		}
		if (n != 1 || *unit_addr == NULL || **unit_addr == 0) {
			cmn_err(CE_WARN, "unit-address property in %s.conf"
			    " not well-formed", ddi_driver_name(child));
			ddi_prop_free(unit_addr);
			return (DDI_NOT_WELL_FORMED);
		}
		ddi_set_name_addr(child, *unit_addr);
		ddi_set_parent_data(child, NULL);
		ddi_prop_free(unit_addr);

		/*
		 * Try to merge the properties from this prototype
		 * node into real h/w nodes.
		 */
		if (impl_ddi_merge_child(child) != DDI_SUCCESS) {
			/*
			 * Merged ok - return failure to remove the node.
			 */
			return (DDI_FAILURE);
		}
		/*
		 * The child was not merged into a h/w node,
		 * but there's not much we can do with it other
		 * than return failure to cause the node to be removed.
		 */
		cmn_err(CE_WARN, "!%s@%s: %s.conf properties not merged",
		    ddi_driver_name(child), ddi_get_name_addr(child),
		    ddi_driver_name(child));
		return (DDI_NOT_WELL_FORMED);
	}

	if ((ret = ppb_create_pci_prop(child, &slot, &func)) != DDI_SUCCESS)
		return (ret);
	if (func != 0)
		(void) sprintf(name, "%x,%x", slot, func);
	else
		(void) sprintf(name, "%x", slot);

	ddi_set_name_addr(child, name);

	if (ddi_getprop(DDI_DEV_T_NONE, child, DDI_PROP_DONTPASS, "interrupts",
		-1) != -1) {
		pdptr = (struct ddi_parent_private_data *)
			kmem_zalloc((sizeof (struct ddi_parent_private_data) +
			sizeof (struct intrspec)), KM_SLEEP);
		pdptr->par_intr = (struct intrspec *)(pdptr + 1);
		pdptr->par_nintr = 1;
		ddi_set_parent_data(child, (caddr_t)pdptr);
	} else
		ddi_set_parent_data(child, NULL);

	if (pci_config_setup(child, &config_handle) != DDI_SUCCESS)
		return (DDI_FAILURE);

#if !defined(i386)
	/*
	 * Determine the configuration header type.
	 */
	header_type = pci_config_get8(config_handle, PCI_CONF_HEADER);
#endif

	/*
	 * Support for the "command-preserve" property.
	 */
	command_preserve = ddi_prop_get_int(DDI_DEV_T_ANY, child,
						DDI_PROP_DONTPASS,
						"command-preserve", 0);
	command = pci_config_get16(config_handle, PCI_CONF_COMM);
	command &= (command_preserve | PCI_COMM_BACK2BACK_ENAB);
	command |= (ppb_command_default & ~command_preserve);
	pci_config_put16(config_handle, PCI_CONF_COMM, command);

#if !defined(i386)
	/*
	 * If the device has a bus control register then program it
	 * based on the settings in the command register.
	 */
	if ((header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE) {
		/*
		 * These flags should be moved to uts/common/sys/pci.h:
		 */
#ifndef PCI_BCNF_BCNTRL
#define	PCI_BCNF_BCNTRL			0x3e
#define	PCI_BCNF_BCNTRL_PARITY_ENABLE	0x0001
#define	PCI_BCNF_BCNTRL_SERR_ENABLE	0x0002
#define	PCI_BCNF_BCNTRL_MAST_AB_MODE	0x0020
#endif
		bcr = pci_config_get8(config_handle, PCI_BCNF_BCNTRL);
		if (ppb_command_default & PCI_COMM_PARITY_DETECT)
			bcr |= PCI_BCNF_BCNTRL_PARITY_ENABLE;
		if (ppb_command_default & PCI_COMM_SERR_ENABLE)
			bcr |= PCI_BCNF_BCNTRL_SERR_ENABLE;
		bcr |= PCI_BCNF_BCNTRL_MAST_AB_MODE;
		pci_config_put8(config_handle, PCI_BCNF_BCNTRL, bcr);
	}
#endif

#ifdef sparc
	/*
	 * Initialize cache-line-size configuration register if needed.
	 */
	if (ppb_set_cache_line_size_register &&
			ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
					"cache-line-size", 0) == 0) {
		ppb = (ppb_devstate_t *)
			ddi_get_soft_state(ppb_state,
				    ddi_get_instance(ddi_get_parent(child)));
		pci_config_put8(config_handle, PCI_CONF_CACHE_LINESZ,
				ppb->ppb_cache_line_size);
		n = pci_config_get8(config_handle, PCI_CONF_CACHE_LINESZ);
		if (n != 0) {
			(void) ndi_prop_update_int(DDI_DEV_T_NONE, child,
					"cache-line-size", n);
		}
	}

	/*
	 * Initialize latency timer configuration registers if needed.
	 */
	if (ppb_set_latency_timer_register &&
			ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
					"latency-timer", 0) == 0) {

		if ((header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE) {
			latency_timer = ppb->ppb_latency_timer;
		/*
		 * This flags should be moved to uts/common/sys/pci.h:
		 */
#ifndef	PCI_BCNF_LATENCY_TIMER
#define	PCI_BCNF_LATENCY_TIMER		0x1b
#endif
			pci_config_put8(config_handle, PCI_BCNF_LATENCY_TIMER,
					ppb->ppb_latency_timer);
		} else {
			min_gnt = pci_config_get8(config_handle,
						    PCI_CONF_MIN_G);
			latency_timer = min_gnt * 8;
		}
		pci_config_put8(config_handle, PCI_CONF_LATENCY_TIMER,
				latency_timer);
		n = pci_config_get8(config_handle, PCI_CONF_LATENCY_TIMER);
		if (n != 0) {
			(void) ndi_prop_update_int(DDI_DEV_T_NONE, child,
					"latency-timer", n);
		}
	}
#endif

	pci_config_teardown(&config_handle);
	return (DDI_SUCCESS);
}

static void
ppb_removechild(dev_info_t *dip)
{
	register struct ddi_parent_private_data *pdptr;

	pdptr = (struct ddi_parent_private_data *)ddi_get_parent_data(dip);
	if (pdptr != NULL) {
		kmem_free(pdptr, (sizeof (*pdptr) + sizeof (struct intrspec)));
		ddi_set_parent_data(dip, NULL);
	}
	ddi_set_name_addr(dip, NULL);

	/*
	 * Strip the node to properly convert it back to prototype form
	 */
	ddi_remove_minor_node(dip, NULL);

	impl_rem_dev_props(dip);
}

static int
ppb_create_pci_prop(dev_info_t *child, uint_t *foundslot, uint_t *foundfunc)
{
	pci_regspec_t *pci_rp;
	int	length;
	int	value;

	/* get child "reg" property */
	value = ddi_getlongprop(DDI_DEV_T_NONE, child, DDI_PROP_CANSLEEP,
		"reg", (caddr_t)&pci_rp, &length);
	if (value != DDI_SUCCESS)
		return (value);

	(void) ndi_prop_update_byte_array(DDI_DEV_T_NONE, child, "reg",
		(u_char *)pci_rp, length);

	/* copy the device identifications */
	*foundslot = PCI_REG_DEV_G(pci_rp->pci_phys_hi);
	*foundfunc = PCI_REG_FUNC_G(pci_rp->pci_phys_hi);

	/*
	 * free the memory allocated by ddi_getlongprop ().
	 */
	kmem_free(pci_rp, length);

	/* assign the basic PCI Properties */

	value = ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_CANSLEEP,
		"vendor-id", -1);
	if (value != -1)
		(void) ndi_prop_update_int(DDI_DEV_T_NONE, child,
		"vendor-id", value);

	value = ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_CANSLEEP,
		"device-id", -1);
	if (value != -1)
		(void) ndi_prop_update_int(DDI_DEV_T_NONE, child,
		"device-id", value);

	value = ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_CANSLEEP,
		"interrupts", -1);
	if (value != -1)
		(void) ndi_prop_update_int(DDI_DEV_T_NONE, child,
		"interrupts", value);
	return (DDI_SUCCESS);
}


/*
 * ppb_save_config_regs
 *
 * This routine saves the state of the configuration registers of all
 * the child nodes of each PBM.
 *
 * used by: ppb_detach() on suspends
 *
 * return value: none
 */
static void
ppb_save_config_regs(ppb_devstate_t *ppb_p)
{
	int i;
	dev_info_t *dip;
	ddi_acc_handle_t config_handle;

	for (i = 0, dip = ddi_get_child(ppb_p->dip); dip != NULL;
			i++, dip = ddi_get_next_sibling(dip)) {

		if (pci_config_setup(dip, &config_handle) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "%s%d: can't config space for %s%d\n",
				ddi_driver_name(ppb_p->dip),
				ddi_get_instance(ppb_p->dip),
				ddi_driver_name(dip),
				ddi_get_instance(dip));
			continue;
		}

		ppb_p->config_state[i].dip = dip;
		ppb_p->config_state[i].command =
			pci_config_get16(config_handle, PCI_CONF_COMM);
#if !defined(i386)
		ppb_p->config_state[i].header_type =
			pci_config_get8(config_handle, PCI_CONF_HEADER);
		if ((ppb_p->config_state[i].header_type & PCI_HEADER_TYPE_M) ==
				PCI_HEADER_ONE)
			ppb_p->config_state[i].bridge_control =
				pci_config_get16(config_handle,
						PCI_BCNF_BCNTRL);
#endif
#ifdef sparc
		ppb_p->config_state[i].cache_line_size =
			pci_config_get8(config_handle, PCI_CONF_CACHE_LINESZ);
		ppb_p->config_state[i].latency_timer =
			pci_config_get8(config_handle, PCI_CONF_LATENCY_TIMER);
		if ((ppb_p->config_state[i].header_type &
				PCI_HEADER_TYPE_M) == PCI_HEADER_ONE)
			ppb_p->config_state[i].sec_latency_timer =
				pci_config_get8(config_handle,
						PCI_BCNF_LATENCY_TIMER);
#endif
		pci_config_teardown(&config_handle);
	}
	ppb_p->config_state_index = i;
}


/*
 * ppb_restore_config_regs
 *
 * This routine restores the state of the configuration registers of all
 * the child nodes of each PBM.
 *
 * used by: ppb_attach() on resume
 *
 * return value: none
 */
static void
ppb_restore_config_regs(ppb_devstate_t *ppb_p)
{
	int i;
	dev_info_t *dip;
	ddi_acc_handle_t config_handle;

	for (i = 0; i < ppb_p->config_state_index; i++) {
		dip = ppb_p->config_state[i].dip;
		if (pci_config_setup(dip, &config_handle) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "%s%d: can't config space for %s%d\n",
				ddi_driver_name(ppb_p->dip),
				ddi_get_instance(ppb_p->dip),
				ddi_driver_name(dip),
				ddi_get_instance(dip));
			continue;
		}
		pci_config_put16(config_handle, PCI_CONF_COMM,
				ppb_p->config_state[i].command);
#if !defined(i386)
		if ((ppb_p->config_state[i].header_type & PCI_HEADER_TYPE_M) ==
				PCI_HEADER_ONE)
			pci_config_put16(config_handle, PCI_BCNF_BCNTRL,
					ppb_p->config_state[i].bridge_control);
#endif
#ifdef sparc
		pci_config_put8(config_handle, PCI_CONF_CACHE_LINESZ,
				ppb_p->config_state[i].cache_line_size);
		pci_config_put8(config_handle, PCI_CONF_LATENCY_TIMER,
				ppb_p->config_state[i].latency_timer);
		if ((ppb_p->config_state[i].header_type &
				PCI_HEADER_TYPE_M) == PCI_HEADER_ONE)
			pci_config_put8(config_handle, PCI_BCNF_LATENCY_TIMER,
				ppb_p->config_state[i].sec_latency_timer);
#endif
		pci_config_teardown(&config_handle);
	}
}
