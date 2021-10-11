/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ebus.c	1.38	99/05/03 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_subrdefs.h>
#include <sys/pci.h>
#include <sys/pci/pci_nexus.h>
#include <sys/autoconf.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/ebus.h>

#ifdef DEBUG
static uint_t ebus_debug_flags = 0;
#endif

/*
 * The values of the following variables are used to initialize
 * the cache line size and latency timer registers in the ebus
 * configuration header.  Variables are used instead of constants
 * to allow tuning from the /etc/system file.
 */
static uint8_t ebus_cache_line_size = 0x10;	/* 64 bytes */
static uint8_t ebus_latency_timer = 0x40;	/* 64 PCI cycles */

/*
 * function prototypes for bus ops routines:
 */
static int
ebus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *addrp);
static int
ebus_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t op, void *arg, void *result);
static int
ebus_intr_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_intr_ctlop_t op, void *arg, void *result);

/*
 * function prototypes for dev ops routines:
 */
static int ebus_identify(dev_info_t *dip);
static int ebus_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int ebus_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);

/*
 * general function prototypes:
 */
static int ebus_config(ebus_devstate_t *ebus_p);
static int ebus_apply_range(ebus_devstate_t *ebus_p, dev_info_t *rdip,
    ebus_regspec_t *ebus_rp, pci_regspec_t *rp);
int get_ranges_prop(ebus_devstate_t *ebus_p);

#define	getprop(dip, name, addr, intp)		\
		ddi_getlongprop(DDI_DEV_T_NONE, (dip), DDI_PROP_DONTPASS, \
				(name), (caddr_t)(addr), (intp))

/*
 * bus ops and dev ops structures:
 */
static struct bus_ops ebus_bus_ops = {
	BUSO_REV,
	ebus_map,
	NULL,
	NULL,
	NULL,
	i_ddi_map_fault,
	ddi_dma_map,
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,
	ebus_ctlops,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0,	/* (*bus_post_event)();		*/
	ebus_intr_ctlops
};

static struct dev_ops ebus_ops = {
	DEVO_REV,
	0,
	ddi_no_info,
	ebus_identify,
	0,
	ebus_attach,
	ebus_detach,
	nodev,
	(struct cb_ops *)0,
	&ebus_bus_ops
};

/*
 * module definitions:
 */
#include <sys/modctl.h>
extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops, 	/* Type of module.  This one is a driver */
	"ebus nexus driver",	/* Name of module. */
	&ebus_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

/*
 * driver global data:
 */
static void *per_ebus_state;		/* per-ebus soft state pointer */


int
_init(void)
{
	int e;

	/*
	 * Initialize per-ebus soft state pointer.
	 */
	e = ddi_soft_state_init(&per_ebus_state, sizeof (ebus_devstate_t), 1);
	if (e != 0)
		return (e);

	/*
	 * Install the module.
	 */
	e = mod_install(&modlinkage);
	if (e != 0)
		ddi_soft_state_fini(&per_ebus_state);
	return (e);
}

int
_fini(void)
{
	int e;

	/*
	 * Remove the module.
	 */
	e = mod_remove(&modlinkage);
	if (e != 0)
		return (e);

	/*
	 * Free the soft state info.
	 */
	ddi_soft_state_fini(&per_ebus_state);
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/* device driver entry points */

/*
 * identify entry point:
 *
 * Identifies with nodes named "ebus".
 */
static int
ebus_identify(dev_info_t *dip)
{
	char *name = ddi_get_name(dip);
	int rc = DDI_NOT_IDENTIFIED;

	DBG1(D_IDENTIFY, NULL, "trying dip=%x\n", dip);
	if (strcmp(name, "ebus") == 0) {
		DBG1(D_IDENTIFY, NULL, "identified dip=%x\n", dip);
		rc = DDI_IDENTIFIED;
	}
	return (rc);
}

/*
 * attach entry point:
 *
 * normal attach:
 *
 *	create soft state structure (dip, reg, nreg and state fields)
 *	map in configuration header
 *	make sure device is properly configured
 *	report device
 */
static int
ebus_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	ebus_devstate_t *ebus_p;	/* per ebus state pointer */
	int instance;

	DBG1(D_ATTACH, NULL, "dip=%x\n", dip);
	switch (cmd) {
	case DDI_ATTACH:

		/*
		 * Allocate soft state for this instance.
		 */
		instance = ddi_get_instance(dip);
		if (ddi_soft_state_zalloc(per_ebus_state, instance)
				!= DDI_SUCCESS) {
			DBG(D_ATTACH, NULL, "failed to alloc soft state\n");
			return (DDI_FAILURE);
		}
		ebus_p = get_ebus_soft_state(instance);
		ebus_p->dip = dip;

		(void) ddi_prop_create(DDI_DEV_T_NONE, dip,
			DDI_PROP_CANSLEEP, "no-dma-interrupt-sync", NULL, 0);
		/* Get our ranges property for mapping child registers. */
		if (get_ranges_prop(ebus_p) != DDI_SUCCESS) {
			free_ebus_soft_state(instance);
			return (DDI_FAILURE);
		}

		/*
		 * Make sure the master enable and memory access enable
		 * bits are set in the config command register.
		 */
		if (!ebus_config(ebus_p)) {
			free_ebus_soft_state(instance);
			return (DDI_FAILURE);
		}

		/*
		 * Make the state as attached and report the device.
		 */
		ebus_p->state = ATTACHED;
		ddi_report_dev(dip);
		DBG(D_ATTACH, ebus_p, "returning\n");
		return (DDI_SUCCESS);

	case DDI_RESUME:

		instance = ddi_get_instance(dip);
		ebus_p = get_ebus_soft_state(instance);

		/*
		 * Make sure the master enable and memory access enable
		 * bits are set in the config command register.
		 */
		if (!ebus_config(ebus_p)) {
			free_ebus_soft_state(instance);
			return (DDI_FAILURE);
		}

		ebus_p->state = RESUMED;
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

/*
 * detach entry point:
 */
static int
ebus_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int instance = ddi_get_instance(dip);
	ebus_devstate_t *ebus_p = get_ebus_soft_state(instance);

	switch (cmd) {
	case DDI_DETACH:
		DBG1(D_DETACH, ebus_p, "DDI_DETACH dip=%x\n", dip);
		kmem_free(ebus_p->rangep, ebus_p->range_cnt *
		    sizeof (struct ebus_pci_rangespec));
		free_ebus_soft_state(instance);
		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		DBG1(D_DETACH, ebus_p, "DDI_SUSPEND dip=%x\n", dip);
		ebus_p->state = SUSPENDED;
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}


int
get_ranges_prop(ebus_devstate_t *ebus_p)
{
	struct ebus_pci_rangespec *rangep;
	int nrange, range_len;

	if (ddi_getlongprop(DDI_DEV_T_ANY, ebus_p->dip, DDI_PROP_DONTPASS,
	    "ranges", (caddr_t)&rangep, &range_len) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "Can't get %s ranges property\n",
		    ddi_get_name(ebus_p->dip));
		return (DDI_ME_REGSPEC_RANGE);
	}

	nrange = range_len / sizeof (struct ebus_pci_rangespec);

	if (nrange == 0)  {
		kmem_free(rangep, range_len);
		return (DDI_FAILURE);
	}

#ifdef	DEBUG
	/* */ {
	int i;

	for (i = 0; i < nrange; i++) {
		DBG5(D_MAP, ebus_p, "ebus range addr 0x%x.0x%x PCI range "
			"addr 0x%x.0x%x.0x%x ", rangep[i].ebus_phys_hi,
			    rangep[i].ebus_phys_low, rangep[i].pci_phys_hi,
			    rangep[i].pci_phys_mid, rangep[i].pci_phys_low);
		DBG1(D_MAP, ebus_p, "Size 0x%x\n", rangep[i].rng_size);
	}
	}
#endif /* DEBUG */

	ebus_p->rangep = rangep;
	ebus_p->range_cnt = nrange;

	return (DDI_SUCCESS);
}


/* bus driver entry points */

/*
 * bus map entry point:
 *
 * 	if map request is for an rnumber
 *		get the corresponding regspec from device node
 * 	build a new regspec in our parent's format
 *	build a new map_req with the new regspec
 *	call up the tree to complete the mapping
 */
static int
ebus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t off, off_t len, caddr_t *addrp)
{
	ebus_devstate_t *ebus_p = get_ebus_soft_state(ddi_get_instance(dip));
	ebus_regspec_t *ebus_rp, *ebus_regs;
	pci_regspec_t pci_reg;
	ddi_map_req_t p_map_request;
	int rnumber, i, n;
	int rval = DDI_SUCCESS;

	/*
	 * Handle the mapping according to its type.
	 */
	DBG4(D_MAP, ebus_p, "rdip=%s%d: off=%x len=%x\n",
	    ddi_get_name(rdip), ddi_get_instance(rdip), off, len);
	switch (mp->map_type) {
	case DDI_MT_REGSPEC:

		/*
		 * We assume the register specification is in ebus format.
		 * We must convert it into a PCI format regspec and pass
		 * the request to our parent.
		 */
		DBG3(D_MAP, ebus_p, "rdip=%s%d: REGSPEC - handlep=%x\n",
			ddi_get_name(rdip), ddi_get_instance(rdip),
			mp->map_handlep);
		ebus_rp = (ebus_regspec_t *)mp->map_obj.rp;
		break;

	case DDI_MT_RNUMBER:

		/*
		 * Get the "reg" property from the device node and convert
		 * it to our parent's format.
		 */
		rnumber = mp->map_obj.rnumber;
		DBG4(D_MAP, ebus_p, "rdip=%s%d: rnumber=%x handlep=%x\n",
			ddi_get_name(rdip), ddi_get_instance(rdip),
			rnumber, mp->map_handlep);

		if (getprop(rdip, "reg", &ebus_regs, &i) != DDI_SUCCESS) {
			DBG(D_MAP, ebus_p, "can't get reg property\n");
			return (DDI_ME_RNUMBER_RANGE);
		}
		n = i / sizeof (ebus_regspec_t);

		if (rnumber < 0 || rnumber >= n) {
			DBG(D_MAP, ebus_p, "rnumber out of range\n");
			return (DDI_ME_RNUMBER_RANGE);
		}
		ebus_rp = &ebus_regs[rnumber];
		break;

	default:
		return (DDI_ME_INVAL);

	}

	/* Adjust our reg property with offset and length */
	ebus_rp->addr_low += off;
	if (len)
		ebus_rp->size = len;

	/*
	 * Now we have a copy the "reg" entry we're attempting to map.
	 * Translate this into our parents PCI address using the ranges
	 * property.
	 */
	rval = ebus_apply_range(ebus_p, rdip, ebus_rp, &pci_reg);

	if (mp->map_type == DDI_MT_RNUMBER)
		kmem_free((caddr_t)ebus_regs, i);

	if (rval != DDI_SUCCESS)
		return (rval);

#ifdef DEBUG
	DBG5(D_MAP, ebus_p, "(%x,%x,%x)(%x,%x)\n",
		pci_reg.pci_phys_hi,
		pci_reg.pci_phys_mid,
		pci_reg.pci_phys_low,
		pci_reg.pci_size_hi,
		pci_reg.pci_size_low);
#endif

	p_map_request = *mp;
	p_map_request.map_type = DDI_MT_REGSPEC;
	p_map_request.map_obj.rp = (struct regspec *)&pci_reg;
	rval = ddi_map(dip, &p_map_request, 0, 0, addrp);
	DBG1(D_MAP, ebus_p, "parent returned %x\n", rval);
	return (rval);
}


static int
ebus_apply_range(ebus_devstate_t *ebus_p, dev_info_t *rdip,
    ebus_regspec_t *ebus_rp, pci_regspec_t *rp)
{
	int b;
	int rval = DDI_SUCCESS;
	struct ebus_pci_rangespec *rangep = ebus_p->rangep;
	int nrange = ebus_p->range_cnt;
	static char *out_of_range =
	    "Out of range register specification from device node <%s>\n";

	DBG3(D_MAP, ebus_p, "Range Matching Addr 0x%x.%x size 0x%x\n",
	    ebus_rp->addr_hi, ebus_rp->addr_low, ebus_rp->size);

	for (b = 0; b < nrange; ++b, ++rangep) {

		/* Check for the correct space */
		if (ebus_rp->addr_hi == rangep->ebus_phys_hi)
			/* See if we fit in this range */
			if ((ebus_rp->addr_low >=
			    rangep->ebus_phys_low) &&
			    ((ebus_rp->addr_low + ebus_rp->size - 1)
				<= (rangep->ebus_phys_low +
				    rangep->rng_size - 1))) {
				uint_t addr_offset = ebus_rp->addr_low -
				    rangep->ebus_phys_low;
				/*
				 * Use the range entry to translate
				 * the EBUS physical address into the
				 * parents PCI space.
				 */
				rp->pci_phys_hi =
				    rangep->pci_phys_hi;
				rp->pci_phys_mid = rangep->pci_phys_mid;
				rp->pci_phys_low =
				    rangep->pci_phys_low + addr_offset;
				rp->pci_size_hi = 0;
				rp->pci_size_low =
				    min(ebus_rp->size, (rangep->rng_size -
					addr_offset));

				DBG2(D_MAP, ebus_p, "Child hi0x%x lo0x%x ",
				    rangep->ebus_phys_hi,
				    rangep->ebus_phys_low);
				DBG4(D_MAP, ebus_p, "Parent hi0x%x "
					"mid0x%x lo0x%x size 0x%x\n",
					    rangep->pci_phys_hi,
					    rangep->pci_phys_mid,
					    rangep->pci_phys_low,
					    rangep->rng_size);

				break;
			}
	}

	if (b == nrange)  {
		cmn_err(CE_WARN, out_of_range, ddi_get_name(rdip));
		return (DDI_ME_REGSPEC_RANGE);
	}

	return (rval);
}


/*
 * control ops entry point:
 *
 * Requests handled completely:
 *	DDI_CTLOPS_INITCHILD
 *	DDI_CTLOPS_UNINITCHILD
 *	DDI_CTLOPS_REPORTDEV
 *	DDI_CTLOPS_REGSIZE
 *	DDI_CTLOPS_NREGS
 *
 * All others passed to parent.
 */
static int
ebus_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t op, void *arg, void *result)
{
#ifdef DEBUG
	ebus_devstate_t *ebus_p = get_ebus_soft_state(ddi_get_instance(dip));
#endif
	ebus_regspec_t *ebus_rp;
	int32_t reglen;
	int i, n;
	char name[10];

	switch (op) {
	case DDI_CTLOPS_INITCHILD: {
		dev_info_t *child = (dev_info_t *)arg;
		/*
		 * Set the address portion of the node name based on the
		 * address/offset.
		 */
		DBG2(D_CTLOPS, ebus_p, "DDI_CTLOPS_INITCHILD: rdip=%s%d\n",
		    ddi_get_name(child), ddi_get_instance(child));

		if (ddi_getlongprop(DDI_DEV_T_NONE, child, DDI_PROP_DONTPASS,
		    "reg", (caddr_t)&ebus_rp, &reglen) != DDI_SUCCESS) {

			DBG(D_CTLOPS, ebus_p, "can't get reg property\n");
			return (DDI_FAILURE);

		}

		(void) sprintf(name, "%x,%x", ebus_rp->addr_hi,
		    ebus_rp->addr_low);
		ddi_set_name_addr(child, name);

		kmem_free((caddr_t)ebus_rp, reglen);

		ddi_set_parent_data(child, NULL);

		return (DDI_SUCCESS);

	}

	case DDI_CTLOPS_UNINITCHILD:
		DBG2(D_CTLOPS, ebus_p, "DDI_CTLOPS_UNINITCHILD: rdip=%s%d\n",
			ddi_get_name((dev_info_t *)arg),
			ddi_get_instance((dev_info_t *)arg));
		ddi_set_name_addr((dev_info_t)arg, NULL);
		ddi_remove_minor_node((dev_info_t)arg, NULL);
		impl_rem_dev_props((dev_info_t)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REPORTDEV:

		DBG2(D_CTLOPS, ebus_p, "DDI_CTLOPS_REPORTDEV: rdip=%s%d\n",
			ddi_get_name(rdip), ddi_get_instance(rdip));
		cmn_err(CE_CONT, "?%s%d at %s%d: offset %s\n",
			ddi_driver_name(rdip), ddi_get_instance(rdip),
			ddi_driver_name(dip), ddi_get_instance(dip),
			ddi_get_name_addr(rdip));
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REGSIZE:

		DBG2(D_CTLOPS, ebus_p, "DDI_CTLOPS_REGSIZE: rdip=%s%d\n",
			ddi_get_name(rdip), ddi_get_instance(rdip));
		if (getprop(rdip, "reg", &ebus_rp, &i) != DDI_SUCCESS) {
			DBG(D_CTLOPS, ebus_p, "can't get reg property\n");
			return (DDI_FAILURE);
		}
		n = i / sizeof (ebus_regspec_t);
		if (*(int *)arg < 0 || *(int *)arg >= n) {
			DBG(D_MAP, ebus_p, "rnumber out of range\n");
			return (DDI_ME_RNUMBER_RANGE);
		}
		*((off_t *)result) = ebus_rp[*(int *)arg].size;
		kmem_free((caddr_t)ebus_rp, i);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_NREGS:

		DBG2(D_CTLOPS, ebus_p, "DDI_CTLOPS_NREGS: rdip=%s%d\n",
			ddi_get_name(rdip), ddi_get_instance(rdip));
		if (getprop(rdip, "reg", &ebus_rp, &i) != DDI_SUCCESS) {
			DBG(D_CTLOPS, ebus_p, "can't get reg property\n");
			return (DDI_FAILURE);
		}
		*((uint_t *)result) = i / sizeof (ebus_regspec_t);
		kmem_free((caddr_t)ebus_rp, i);
		return (DDI_SUCCESS);
	}

	/*
	 * Now pass the request up to our parent.
	 */
	DBG2(D_CTLOPS, ebus_p, "passing request to parent: rdip=%s%d\n",
		ddi_get_name(rdip), ddi_get_instance(rdip));
	return (ddi_ctlops(dip, rdip, op, arg, result));
}

struct ebus_string_to_pil {
	int8_t *string;
	uint32_t pil;
};

static struct ebus_string_to_pil ebus_name_to_pil[] = {{"SUNW,CS4231", 9},
						    {"fdthree", 8},
						    {"ecpp", 3},
						    {"su", 12},
						    {"se", 12},
						    {"power", 14}};

static struct ebus_string_to_pil ebus_device_type_to_pil[] = {{"serial", 12},
								{"block", 8}};

static int
ebus_intr_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_intr_ctlop_t op, void *arg, void *result)
{
#ifdef DEBUG
	ebus_devstate_t *ebus_p = get_ebus_soft_state(ddi_get_instance(dip));
#endif
	/*
	 * This is a hack to set the PIL for the devices under ebus.
	 * We first look up a device by it's specific name, if we can't
	 * match the name, we try and match it's device_type property.
	 * Lastly we default a PIL level of 1.
	 */
	switch (op) {
	case DDI_INTR_CTLOPS_ALLOC_ISPEC: {
		ddi_ispec_t *ispecp;

		i_ddi_alloc_ispec(rdip, (uint_t)arg,
		    (ddi_intrspec_t *)result);

		ispecp = *((ddi_ispec_t **)result);
		DBG1(D_INTR, ebus_p, "ispec 0x%x\n", ispecp);
		if (ispecp && ispecp->is_pil == 0) {
			int8_t *name, *device_type;
			int32_t i, max_children, max_device_types, len;

			name = ddi_get_name(rdip);
			max_children = sizeof (ebus_name_to_pil) /
			    sizeof (struct ebus_string_to_pil);

			for (i = 0; i < max_children; i++) {
				if (strcmp(ebus_name_to_pil[i].string, name) ==
				    0) {
					DBG2(D_INTR, ebus_p, "child name %s; "
					    "match PIL %d\n",
					    ebus_name_to_pil[i].string,
					    ebus_name_to_pil[i].pil);

					ispecp->is_pil =
					    ebus_name_to_pil[i].pil;
					goto done;
				}
			}

			if (ddi_getlongprop(DDI_DEV_T_NONE, rdip,
			    DDI_PROP_DONTPASS, "device_type",
			    (caddr_t)&device_type, &len) == DDI_SUCCESS) {

				max_device_types =
				    sizeof (ebus_device_type_to_pil) /
				    sizeof (struct ebus_string_to_pil);
				for (i = 0; i < max_device_types; i++) {
					if (strcmp(
					    ebus_device_type_to_pil[i].string,
					    device_type) == 0) {

						DBG2(D_INTR, ebus_p,
						    "Device type %s; match "
						    "PIL %d\n",
						    ebus_device_type_to_pil[i].
						    string,
						    ebus_device_type_to_pil[i].
						    pil);

						ispecp->is_pil =
						    ebus_device_type_to_pil[i].
						    pil;
						break;
					}
				}

				kmem_free(device_type, len);
			}

			/*
			 * If we get here, we need to set a default value
			 * for the PIL.
			 */
			if (ispecp->is_pil == 0) {
				ispecp->is_pil = 1;
				cmn_err(CE_WARN, "%s%d assigning default "
				    "interrupt level %d for device %s%d\n",
				    ddi_get_name(dip), ddi_get_instance(dip),
				    ispecp->is_pil, ddi_get_name(rdip),
				    ddi_get_instance(rdip));
			}
		}
done:

		return (DDI_SUCCESS);
	}
	default:
		return (i_ddi_intr_ctlops(dip, rdip, op, arg, result));
	}

}


static int
ebus_config(ebus_devstate_t *ebus_p)
{
	ddi_acc_handle_t conf_handle;
	uint16_t comm;

	/*
	 * Make sure the master enable and memory access enable
	 * bits are set in the config command register.
	 */
	if (pci_config_setup(ebus_p->dip, &conf_handle) != DDI_SUCCESS)
		return (0);

	comm = pci_config_get16(conf_handle, PCI_CONF_COMM),
#ifdef DEBUG
	    DBG1(D_MAP, ebus_p, "command register was 0x%x\n", comm);
#endif
	comm |= (PCI_COMM_ME|PCI_COMM_MAE|PCI_COMM_SERR_ENABLE|
	    PCI_COMM_PARITY_DETECT);
	pci_config_put16(conf_handle, PCI_CONF_COMM, comm),
#ifdef DEBUG
	    DBG1(D_MAP, ebus_p, "command register is now 0x%x\n", comm);
#endif
	pci_config_put8(conf_handle, PCI_CONF_CACHE_LINESZ,
	    (uchar_t)ebus_cache_line_size);
	pci_config_put8(conf_handle, PCI_CONF_LATENCY_TIMER,
	    (uchar_t)ebus_latency_timer);
	pci_config_teardown(&conf_handle);
	return (1);
}

#ifdef DEBUG
extern void prom_printf(const char *, ...);

static void
ebus_debug(uint_t flag, ebus_devstate_t *ebus_p, char *fmt,
	uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5)
{
	char *s;

	if (ebus_debug_flags & flag) {
		switch (flag) {
		case D_IDENTIFY:
			s = "identify"; break;
		case D_ATTACH:
			s = "attach"; break;
		case D_DETACH:
			s = "detach"; break;
		case D_MAP:
			s = "map"; break;
		case D_CTLOPS:
			s = "ctlops"; break;
		case D_INTR:
			s = "intr"; break;
		}
		if (ebus_p)
			cmn_err(CE_CONT, "%s%d: %s: ",
				ddi_get_name(ebus_p->dip),
				ddi_get_instance(ebus_p->dip), s);
		else
			cmn_err(CE_CONT, "ebus: ");
		cmn_err(CE_CONT, fmt, a1, a2, a3, a4, a5);
	}
}
#endif
