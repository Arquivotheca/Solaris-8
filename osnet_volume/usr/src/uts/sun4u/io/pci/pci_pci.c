/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_pci.c	1.48	99/04/26 SMI"

/*
 *	Sun4u PCI to PCI bus bridge nexus driver
 */

#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/modctl.h>
#include <sys/autoconf.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_subrdefs.h>
#include <sys/pci.h>
#include <sys/pci/pci_nexus.h>
#include <sys/pci/pci_regs.h>
#include <sys/ddi.h>
#include <sys/sunndi.h>

/*
 * The variable controls the default setting of the command register
 * for pci devices.  See ppb_initchild() for details.
 */
static ushort_t ppb_command_default = PCI_COMM_SERR_ENABLE |
					PCI_COMM_WAIT_CYC_ENAB |
					PCI_COMM_PARITY_DETECT |
					PCI_COMM_ME |
					PCI_COMM_MAE |
					PCI_COMM_IO;

static int ppb_bus_map(dev_info_t *, dev_info_t *, ddi_map_req_t *,
	off_t, off_t, caddr_t *);
static int ppb_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t,
	void *, void *);
static int
ppb_intr_ctlop(dev_info_t *dip, dev_info_t *rdip, ddi_intr_ctlop_t ctlop,
	void *arg, void *result);

struct bus_ops ppb_bus_ops = {
	BUSO_REV,
	ppb_bus_map,
	0,
	0,
	0,
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
	0,	/* (*bus_post_event)();		*/
	ppb_intr_ctlop
};

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

static int ppb_identify(dev_info_t *devi);
static int ppb_probe(dev_info_t *);
static int ppb_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int ppb_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);
static int ppb_info(dev_info_t *dip, ddi_info_cmd_t infocmd,
	void *arg, void **result);

struct dev_ops ppb_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt  */
	ppb_info,		/* info */
	ppb_identify,		/* identify */
	ppb_probe,		/* probe */
	ppb_attach,		/* attach */
	ppb_detach,		/* detach */
	nulldev,		/* reset */
	&ppb_cb_ops,		/* driver operations */
	&ppb_bus_ops		/* bus operations */

};

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module */
	"Standard PCI to PCI bridge nexus driver",
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

static struct ppb_cfg_state {
	dev_info_t *dip;
	ushort_t command;
	uchar_t cache_line_size;
	uchar_t latency_timer;
	uchar_t header_type;
	uchar_t sec_latency_timer;
	ushort_t bridge_control;
};

typedef struct {

	dev_info_t *dip;

	/*
	 * configuration register state for the bus:
	 */
	uchar_t ppb_cache_line_size;
	uchar_t ppb_latency_timer;

	/*
	 * cpr support:
	 */
	uint_t config_state_index;
	struct ppb_cfg_state *ppb_config_state_p;
} ppb_devstate_t;

/*
 * The following variable enables a workaround for the following obp bug:
 *
 *	1234181 - obp should set latency timer registers in pci
 *		configuration header
 *
 * Until this bug gets fixed in the obp, the following workaround should
 * be enabled.
 */
static uint_t ppb_set_latency_timer_register = 1;

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
static uint_t ppb_set_cache_line_size_register = 1;

/*
 * forward function declarations:
 */
static void ppb_removechild(dev_info_t *);
static int ppb_initchild(dev_info_t *child);
static int ppb_create_pci_prop(dev_info_t *child, uint_t *, uint_t *);
static void ppb_save_config_regs(ppb_devstate_t *ppb_p);
static void ppb_restore_config_regs(ppb_devstate_t *ppb_p);
static dev_info_t *get_my_childs_dip(dev_info_t *dip, dev_info_t *rdip);

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

		ppb->ppb_cache_line_size =
			pci_config_get8(config_handle, PCI_CONF_CACHE_LINESZ);
		ppb->ppb_latency_timer =
			pci_config_get8(config_handle, PCI_CONF_LATENCY_TIMER);

		pci_config_teardown(&config_handle);
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
	return ((DEVI(pdip)->devi_ops->devo_bus_ops->bus_map)
	    (pdip, rdip, mp, offset, len, vaddrp));
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


static dev_info_t *
get_my_childs_dip(dev_info_t *dip, dev_info_t *rdip)
{
	dev_info_t *cdip = rdip;

	for (; ddi_get_parent(cdip) != dip; cdip = ddi_get_parent(cdip))
		;

	return (cdip);
}


static int
ppb_intr_ctlop(dev_info_t *dip, dev_info_t *rdip,
	ddi_intr_ctlop_t op, void *arg, void *result)
{
	int ret;
	ddi_ispec_t *ispecp = NULL;
	ddi_intr_info_t *intr_info = (ddi_intr_info_t *)arg;

	switch (op) {
	case DDI_INTR_CTLOPS_ADD:
	case DDI_INTR_CTLOPS_REMOVE:
	case DDI_INTR_CTLOPS_HILEVEL: {
		switch (intr_info->ii_kind) {
		case IDDI_INTR_TYPE_NORMAL: {
			dev_info_t *cdip = rdip;
			pci_regspec_t *pci_rp;
			int reglen;
			uint32_t d, intr;

			/*
			 * If the interrupt-map property is defined at this
			 * node, it will have performed the interrupt
			 * translation as part of the property, so no
			 * rotation needs to be done.
			 */
			{
				int len;

				if (ddi_getproplen(DDI_DEV_T_ANY, dip,
				    DDI_PROP_DONTPASS, "interrupt-map",
				    &len) == DDI_PROP_SUCCESS)
					break;
			}

			cdip = get_my_childs_dip(dip, rdip);

			/*
			 * Use the devices reg property to determine it's
			 * PCI bus number and device number.
			 */
			if (ddi_getlongprop(DDI_DEV_T_NONE, cdip,
			    DDI_PROP_DONTPASS, "reg", (caddr_t)&pci_rp,
			    &reglen) != DDI_SUCCESS)
				return (DDI_FAILURE);

			ispecp = (ddi_intrspec_t)intr_info->ii_ispec;

			intr = *ispecp->is_intr;

			/* spin the interrupt */
			d = PCI_REG_DEV_G(pci_rp[0].pci_phys_hi);
			if ((intr >= PCI_INTA) && (intr <= PCI_INTD))
				*ispecp->is_intr =
				    ((intr - 1 + (d % 4)) % 4 + 1);
			else
				cmn_err(CE_WARN, "%s%d: %s: "
				    "PCI intr=%x out of range\n",
				    ddi_driver_name(rdip),
				    ddi_get_instance(rdip),
				    ddi_driver_name(dip), intr);

			kmem_free(pci_rp, reglen);
			break;
		}

		default:
			/* We only handle normal interrupts */
			return (DDI_FAILURE);
		}

		break;
	}

	/*
	 * This is a special case of the ALLOC/FREE, NINTRS ctlop.  Normally a
	 * nexus driver should handle this request immediately on behalf of
	 * it's child, however, the PCI_PCI bridge is of the same interrupt
	 * domain as that of it's parent, so these ops will be resolved at
	 * the highest PCI parent claiming ownership of the domain.
	 */
	case DDI_INTR_CTLOPS_ALLOC_ISPEC:
	case DDI_INTR_CTLOPS_FREE_ISPEC:
	case DDI_INTR_CTLOPS_NINTRS:
	default:
		break;
	}

	/* Pass up the request to our parent. */
	ret = i_ddi_intr_ctlops(dip, rdip, op, arg, result);

	return (ret);
}


static int
ppb_initchild(dev_info_t *child)
{
	char name[MAXNAMELEN];
	int ret;
	uint_t slot, func;
	ddi_acc_handle_t config_handle;
	ushort_t command_preserve, command;
	char **unit_addr;
	uint_t n;
	ushort_t bcr;
	uchar_t header_type;
	uchar_t min_gnt, latency_timer;
	ppb_devstate_t *ppb;

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

	ddi_set_parent_data(child, NULL);

	if (pci_config_setup(child, &config_handle) != DDI_SUCCESS)
		return (DDI_FAILURE);

	/*
	 * Determine the configuration header type.
	 */
	header_type = pci_config_get8(config_handle, PCI_CONF_HEADER);

	/*
	 * Support for the "command-preserve" property.
	 */
	command_preserve = ddi_prop_get_int(DDI_DEV_T_ANY, child,
		DDI_PROP_DONTPASS, "command-preserve", 0);
	command = pci_config_get16(config_handle, PCI_CONF_COMM);
	command &= (command_preserve | PCI_COMM_BACK2BACK_ENAB);
	command |= (ppb_command_default & ~command_preserve);
	pci_config_put16(config_handle, PCI_CONF_COMM, command);

	/*
	 * If the device has a bus control register then program it
	 * based on the settings in the command register.
	 */
	if ((header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE) {
		bcr = pci_config_get8(config_handle, PCI_BCNF_BCNTRL);
		if (ppb_command_default & PCI_COMM_PARITY_DETECT)
			bcr |= PCI_BCNF_BCNTRL_PARITY_ENABLE;
		if (ppb_command_default & PCI_COMM_SERR_ENABLE)
			bcr |= PCI_BCNF_BCNTRL_SERR_ENABLE;
		bcr |= PCI_BCNF_BCNTRL_MAST_AB_MODE;
		pci_config_put8(config_handle, PCI_BCNF_BCNTRL, bcr);
	}

	ppb = (ppb_devstate_t *)ddi_get_soft_state(ppb_state,
	    ddi_get_instance(ddi_get_parent(child)));

	/*
	 * Initialize cache-line-size configuration register if needed.
	 */
	if (ppb_set_cache_line_size_register &&
	    ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
		"cache-line-size", 0) == 0) {
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

	pci_config_teardown(&config_handle);
	return (DDI_SUCCESS);
}

static void
ppb_removechild(dev_info_t *dip)
{
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
		(uchar_t *)pci_rp, length);

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
	struct ppb_cfg_state *statep;


	for (i = 0, dip = ddi_get_child(ppb_p->dip); dip != NULL;
		dip = ddi_get_next_sibling(dip)) {
		if (DDI_CF2(dip))
			i++;
	}
	ppb_p->config_state_index = i;

	if (!i) {
		ppb_p->ppb_config_state_p = NULL;
		return;
	}

	ppb_p->ppb_config_state_p =
		kmem_zalloc(i * sizeof (struct ppb_cfg_state), KM_NOSLEEP);
	if (!ppb_p->ppb_config_state_p) {
		cmn_err(CE_WARN, "not enough mem to save %d ppb child\n", i);
		return;
	}

	for (statep = ppb_p->ppb_config_state_p,
		dip = ddi_get_child(ppb_p->dip);
		dip != NULL;
		dip = ddi_get_next_sibling(dip)) {

		if (!DDI_CF2(dip))
			continue;

		if (pci_config_setup(dip, &config_handle) != DDI_SUCCESS) {
			cmn_err(CE_WARN,
				"%s%d: can't config space for %s%d\n",
				ddi_driver_name(ppb_p->dip),
				ddi_get_instance(ppb_p->dip),
				ddi_driver_name(dip),
				ddi_get_instance(dip));
			continue;
		}

		statep->dip = dip;
		statep->command =
			pci_config_get16(config_handle, PCI_CONF_COMM);
		statep->header_type =
			pci_config_get8(config_handle, PCI_CONF_HEADER);
		if ((statep->header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE)
			statep->bridge_control =
			    pci_config_get16(config_handle, PCI_BCNF_BCNTRL);
		statep->cache_line_size =
			pci_config_get8(config_handle, PCI_CONF_CACHE_LINESZ);
		statep->latency_timer =
			pci_config_get8(config_handle, PCI_CONF_LATENCY_TIMER);
		if ((statep->header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE)
			statep->sec_latency_timer =
			    pci_config_get8(config_handle,
				PCI_BCNF_LATENCY_TIMER);
		pci_config_teardown(&config_handle);
		statep++;
	}
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
	struct ppb_cfg_state *statep = ppb_p->ppb_config_state_p;

	for (i = 0; i < ppb_p->config_state_index; i++, statep++) {
		dip = statep->dip;
		if (!dip) {
			cmn_err(CE_WARN,
				"%s%d: skipping bad dev info (%d)\n",
				ddi_driver_name(ppb_p->dip),
				ddi_get_instance(ppb_p->dip),
				i);
			continue;
		}
		if (pci_config_setup(dip, &config_handle) != DDI_SUCCESS) {
			cmn_err(CE_WARN,
				"%s%d: can't config space for %s%d\n",
				ddi_driver_name(ppb_p->dip),
				ddi_get_instance(ppb_p->dip),
				ddi_driver_name(dip),
				ddi_get_instance(dip));
			continue;
		}
		pci_config_put16(config_handle, PCI_CONF_COMM, statep->command);
		if ((statep->header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE)
			pci_config_put16(config_handle, PCI_BCNF_BCNTRL,
				statep->bridge_control);
		pci_config_put8(config_handle, PCI_CONF_CACHE_LINESZ,
			statep->cache_line_size);
		pci_config_put8(config_handle, PCI_CONF_LATENCY_TIMER,
			statep->latency_timer);
		if ((statep->header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE)
			pci_config_put8(config_handle, PCI_BCNF_LATENCY_TIMER,
				statep->sec_latency_timer);
		pci_config_teardown(&config_handle);
	}

	kmem_free(ppb_p->ppb_config_state_p,
		ppb_p->config_state_index * sizeof (struct ppb_cfg_state));
	ppb_p->ppb_config_state_p = NULL;
	ppb_p->config_state_index = 0;
}
