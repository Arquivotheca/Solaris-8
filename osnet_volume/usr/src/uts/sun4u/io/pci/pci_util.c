/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_util.c	1.14	99/07/21 SMI"

/*
 * PCI nexus utility routines:
 *	property and config routines for attach()
 *	reg/intr/range/assigned-address property routines for bus_map()
 *	init_child()
 *	fault handling
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/async.h>
#include <sys/sysmacros.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi_impldefs.h>
#include <sys/pci/pci_obj.h>

/*LINTLIBRARY*/

static int pci_apply_range(pci_t *pci_p, dev_info_t *rdip,
	pci_regspec_t *pci_rp, struct regspec *rp);
static int get_addr(pci_t *pci_p, dev_info_t *dip, pci_regspec_t *pci_rp,
	pci_regspec_t *augmented_rp);

/*
 * get_pci_properties
 *
 * This function is called from the attach routine to get the key
 * properties of the pci nodes.
 *
 * used by: pci_attach()
 *
 * return value: DDI_FAILURE on failure
 */
static uint32_t javelin_prom_fix[] = {0xfff800, 0, 0, 0x3f};
int
get_pci_properties(pci_t *pci_p, dev_info_t *dip)
{
	int id;
	int i;
	int is_psycho = 0;
	extern char *platform;

	/*
	 * Get the device's port id.
	 */
	id = ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
				"upa-portid", -1);
	if (id != -1) {
		pci_p->pci_id = (uint_t)id;
		is_psycho = 1;
	} else {
		id = ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
			"portid", -1);
		if (id != -1)
			pci_p->pci_id = id;
		else {
			cmn_err(CE_WARN, "%s%d: no portid property\n",
				ddi_driver_name(dip), ddi_get_instance(dip));
			return (DDI_FAILURE);
		}
	}

	/*
	 * This is a hack to fix a broken imap entry in the javelin PROM.
	 * see bugid XXX
	 */
	if (strcmp((const char *)&platform, "SUNW,Ultra-250") == 0)
		(void) ddi_prop_create(DDI_DEV_T_NONE, dip, DDI_PROP_CANSLEEP,
		    "interrupt-map-mask", (caddr_t)javelin_prom_fix,
		    sizeof (javelin_prom_fix));

	/*
	 * Get the bus-ranges property.
	 */
	i = sizeof (pci_p->pci_bus_range);
	if (ddi_getlongprop_buf(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS,
	    "bus-range", (caddr_t)&pci_p->pci_bus_range, &i) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "%s%d: no bus-range property\n",
		    ddi_driver_name(dip), ddi_get_instance(dip));
		return (DDI_FAILURE);
	}
	DEBUG2(DBG_ATTACH, dip, "get_pci_properties: bus-range (%x,%x)\n",
	    pci_p->pci_bus_range.lo, pci_p->pci_bus_range.hi);

	if (is_psycho) {
		if (pci_p->pci_bus_range.lo == 0)
			pci_p->pci_side = B;
		else
			pci_p->pci_side = A;
		DEBUG1(DBG_ATTACH, dip, "get_pci_properties: side %c\n",
		    pci_p->pci_side == A ? 'A' : 'B');
	}

	/*
	 * Get the interrupts property.
	 */
	if (ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS,
	    "interrupts", (caddr_t)&pci_p->pci_interrupts,
	    &pci_p->pci_interrupts_length) !=
		DDI_SUCCESS) {

		cmn_err(CE_WARN, "%s%d: no interrupts property\n",
		    ddi_driver_name(dip), ddi_get_instance(dip));
		return (DDI_FAILURE);
	}
#ifdef DEBUG
	DEBUG0(DBG_ATTACH, dip, "get_pci_properties: interrupts:");
	for (i = 0; i < pci_p->pci_interrupts_length / sizeof (uint_t); i++)
		DEBUG1(DBG_ATTACH|DBG_CONT, dip, " %x",
		    pci_p->pci_interrupts[i]);
	DEBUG0(DBG_ATTACH|DBG_CONT, dip, "\n");
#endif

	/*
	 * disable streaming cache if necessary, this must be done
	 * before PBM is configured.
	 */
	if (ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
			"no-streaming-cache", -1) == 1) {
		pci_stream_buf_enable = 0;
		pci_stream_buf_exists = 0;
	}

	/*
	 * Get the ranges property.
	 */
	if (ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS,
	    "ranges", (caddr_t)&pci_p->pci_ranges, &pci_p->pci_ranges_length) !=
		DDI_SUCCESS) {

		cmn_err(CE_WARN, "%s%d: no ranges property\n",
			ddi_driver_name(dip), ddi_get_instance(dip));
		kmem_free(pci_p->pci_interrupts, pci_p->pci_interrupts_length);
		return (DDI_FAILURE);
	}
#ifdef DEBUG
	DEBUG0(DBG_ATTACH, dip, "get_pci_properties: ranges:");
	for (i = 0; i < pci_p->pci_ranges_length / sizeof (pci_ranges_t); i++) {
		DEBUG5(DBG_ATTACH|DBG_CONT, dip, "(%x,%x,%x)(%x,%x)",
		    pci_p->pci_ranges[i].child_high,
		    pci_p->pci_ranges[i].child_mid,
		    pci_p->pci_ranges[i].child_low,
		    pci_p->pci_ranges[i].parent_high,
		    pci_p->pci_ranges[i].parent_low);
		DEBUG2(DBG_ATTACH|DBG_CONT, dip, "(%x,%x)",
		    pci_p->pci_ranges[i].size_high,
		    pci_p->pci_ranges[i].size_low);
	}
	DEBUG0(DBG_ATTACH|DBG_CONT, dip, "\n");
#endif

	/*
	 * Determine the number upa slot interrupts.
	 */
	pci_p->pci_numproxy = pci_get_numproxy(pci_p->pci_dip);

	DEBUG1(DBG_ATTACH, dip, "get_pci_properties: numproxy=%d\n",
	    pci_p->pci_numproxy);

	pci_p->pci_thermal_interrupt =
		ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
				"thermal-interrupt", -1);
	DEBUG1(DBG_ATTACH, dip, "get_pci_properties: thermal_interrupt=%d\n",
	    pci_p->pci_thermal_interrupt);

	return (1);
}


/*
 * free_pci_properties:
 *
 * This routine frees the memory used to cache the "interrupts", "address",
 * and "ranges" properties of the pci bus device node.
 *
 * used by: pci_detach()
 *
 * return value: none
 */
void
free_pci_properties(pci_t *pci_p)
{
	kmem_free(pci_p->pci_interrupts, pci_p->pci_interrupts_length);
	kmem_free(pci_p->pci_ranges, pci_p->pci_ranges_length);
}


/*
 * save_config_regs
 *
 * This routine saves the state of the configuration registers of all
 * the child nodes of each PBM.
 *
 * used by: pci_detach() on suspends
 *
 * return value: none
 */
void
save_config_regs(pci_t *pci_p)
{
	int i;
	dev_info_t *dip;
	ddi_acc_handle_t config_handle;
	config_header_state_t *chs_p;

	/*
	 * Obtain a count of the number of children and allocate
	 * enough space to save their configuration states.
	 */
	for (i = 0, dip = ddi_get_child(pci_p->pci_dip); dip != NULL;
			i++, dip = ddi_get_next_sibling(dip))
		;
	pci_p->pci_config_state_p =
		kmem_alloc(i * sizeof (config_header_state_t), KM_SLEEP);
	pci_p->pci_config_state_entries = i;
	DEBUG1(DBG_DETACH, pci_p->pci_dip, "save_config_regs: %d entries\n",
	    pci_p->pci_config_state_entries);

	/*
	 * Now scan the child nodes again, this time saving their
	 * configuration state.
	 */
	chs_p = pci_p->pci_config_state_p;
	for (dip = ddi_get_child(pci_p->pci_dip); dip != NULL;
			dip = ddi_get_next_sibling(dip)) {
		DEBUG2(DBG_DETACH, pci_p->pci_dip, "save_config_regs: %s%d\n",
		    ddi_driver_name(dip), ddi_get_instance(dip));
		if (pci_config_setup(dip, &config_handle) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "%s%d: can't config space for %s%d\n",
				ddi_driver_name(pci_p->pci_dip),
				ddi_get_instance(pci_p->pci_dip),
				ddi_driver_name(dip),
				ddi_get_instance(dip));
			continue;
		}
		chs_p->chs_dip = dip;
		chs_p->chs_command =
		    pci_config_get16(config_handle, PCI_CONF_COMM);
		chs_p->chs_header_type =
		    pci_config_get8(config_handle, PCI_CONF_HEADER);
		if ((chs_p->chs_header_type & PCI_HEADER_TYPE_M) ==
		    PCI_HEADER_ONE)
			chs_p->chs_bridge_control =
			    pci_config_get16(config_handle, PCI_BCNF_BCNTRL);
		chs_p->chs_cache_line_size =
		    pci_config_get8(config_handle, PCI_CONF_CACHE_LINESZ);
		chs_p->chs_latency_timer =
		    pci_config_get8(config_handle, PCI_CONF_LATENCY_TIMER);
		if ((chs_p->chs_header_type & PCI_HEADER_TYPE_M) ==
		    PCI_HEADER_ONE)
			chs_p->chs_sec_latency_timer =
			    pci_config_get8(config_handle,
				PCI_BCNF_LATENCY_TIMER);
		pci_config_teardown(&config_handle);
		chs_p++;
	}
}


/*
 * restore_config_regs
 *
 * This routine restores the state of the configuration registers of all
 * the child nodes of each PBM.
 *
 * used by: pci_attach() on resume
 *
 * return value: none
 */
void
restore_config_regs(pci_t *pci_p)
{
	int i;
	dev_info_t *dip;
	ddi_acc_handle_t config_handle;
	config_header_state_t *chs_p;

	chs_p = pci_p->pci_config_state_p;
	DEBUG1(DBG_ATTACH, pci_p->pci_dip, "restore_config_regs: %d entries\n",
	    pci_p->pci_config_state_entries);
	for (i = 0; i < pci_p->pci_config_state_entries; i++) {
		dip = chs_p->chs_dip;
		DEBUG2(DBG_ATTACH, pci_p->pci_dip,
		    "restore_config_regs: %s%d\n",
		    ddi_driver_name(dip), ddi_get_instance(dip));
		if (pci_config_setup(dip, &config_handle) != DDI_SUCCESS) {
			cmn_err(CE_WARN,
				"%s%d: can't config space for %s%d\n",
				ddi_driver_name(pci_p->pci_dip),
				ddi_get_instance(pci_p->pci_dip),
				ddi_driver_name(dip),
				ddi_get_instance(dip));
			continue;
		}
		pci_config_put16(config_handle, PCI_CONF_COMM,
		    chs_p->chs_command);
		if ((chs_p->chs_header_type & PCI_HEADER_TYPE_M) ==
		    PCI_HEADER_ONE)
			pci_config_put16(config_handle, PCI_BCNF_BCNTRL,
			    chs_p->chs_bridge_control);
		pci_config_put8(config_handle, PCI_CONF_CACHE_LINESZ,
		    chs_p->chs_cache_line_size);
		pci_config_put8(config_handle, PCI_CONF_LATENCY_TIMER,
		    chs_p->chs_latency_timer);
		if ((chs_p->chs_header_type & PCI_HEADER_TYPE_M) ==
		    PCI_HEADER_ONE)
			pci_config_put8(config_handle, PCI_BCNF_LATENCY_TIMER,
			    chs_p->chs_sec_latency_timer);
		pci_config_teardown(&config_handle);
		chs_p++;
	}
}


/*
 * get_reg_set
 *
 * The routine will get an IEEE 1275 PCI format regspec for a given
 * device node and register number.
 *
 * used by: pci_map()
 *
 * return value:
 *
 *	DDI_SUCCESS		- on success
 *	DDI_ME_INVAL		- regspec is invalid
 *	DDI_ME_RNUMBER_RANGE	- rnumber out of range
 */
int
get_reg_set(pci_t *pci_p, dev_info_t *child, int rnumber,
	off_t off, off_t len, struct regspec *rp)
{
	pci_regspec_t *pci_rp;
	int i, n, rval;

	/*
	 * Get the reg property for the device.
	 */
	if (ddi_getlongprop(DDI_DEV_T_NONE, child, DDI_PROP_DONTPASS, "reg",
	    (caddr_t)&pci_rp, &i) != DDI_SUCCESS)
		return (DDI_ME_RNUMBER_RANGE);

	n = i / (int)sizeof (pci_regspec_t);
	if (rnumber >= n)
		rval = DDI_ME_RNUMBER_RANGE;
	else {

		/*
		 * Convert each the pci format register specification to a
		 * UPA format register specification.
		 */
		rval = xlate_reg_prop(pci_p, child, &pci_rp[rnumber], off,
			len, rp);
	}
	kmem_free(pci_rp, i);
	return (rval);
}

/*
 * xlate_reg_prop
 *
 * This routine converts an IEEE 1275 PCI format regspec to a standard
 * regspec containing the corresponding system address.
 *
 * used by: pci_map()
 *
 * return value:
 *
 *	DDI_SUCCESS		- on success
 *	DDI_ME_INVAL		- regspec is invalid
 */
int
xlate_reg_prop(pci_t *pci_p, dev_info_t *rdip, pci_regspec_t *pci_rp,
	off_t off, off_t len, struct regspec *rp)
{
	uint_t bus, phys_hi;
	pci_regspec_t augmented_reg;
	int rval = DDI_SUCCESS;

	/*
	 * Make sure the bus number is valid.
	 */
	DEBUG5(DBG_MAP, pci_p->pci_dip, "pci regspec - ((%x,%x,%x) (%x,%x))\n",
	    pci_rp->pci_phys_hi, pci_rp->pci_phys_mid, pci_rp->pci_phys_low,
	    pci_rp->pci_size_hi, pci_rp->pci_size_low);
	phys_hi = pci_rp->pci_phys_hi;
	bus = PCI_REG_BUS_G(phys_hi);
	if (bus < pci_p->pci_bus_range.lo || bus > pci_p->pci_bus_range.hi) {
		DEBUG1(DBG_MAP | DBG_CONT, pci_p->pci_dip,
			"invalid bus number (%x)\n", bus);
		return (DDI_ME_INVAL);
	}

	/*
	 * Regardless of type code, phys_mid must always be zero.
	 * XXX This needs to change when we support 64 bit memory space.
	 */
	if (pci_rp->pci_phys_mid != 0 || pci_rp->pci_size_hi != 0) {
		DEBUG0(DBG_MAP | DBG_CONT, pci_p->pci_dip,
			"phys_mid or size_hi not 0\n");
		return (DDI_ME_INVAL);
	}

	/*
	 * If the "reg" property specifies relocatable, get and interpret the
	 * "assigned-addresses" property.
	 */
	if (((phys_hi & PCI_RELOCAT_B) == 0) && (phys_hi & PCI_ADDR_MASK)) {
		if (get_addr(pci_p, rdip, pci_rp, &augmented_reg)
		    != DDI_SUCCESS)
			return (DDI_ME_INVAL);
	} else
		augmented_reg = *pci_rp;

	/* Adjust the mapping request for the length and offset parameters. */
	augmented_reg.pci_phys_low += off;
	if (len)
		augmented_reg.pci_size_low = len;

	/*
	 * Build the regspec using the ranges property.
	 */
	rval = pci_apply_range(pci_p, rdip, &augmented_reg, rp);

	DEBUG3(DBG_MAP | DBG_CONT, pci_p->pci_dip, "regspec (%x,%x,%x)\n",
		rp->regspec_bustype, rp->regspec_addr, rp->regspec_size);
	return (rval);
}


/*
 * pci_apply_range:
 * Apply range of dip to struct regspec *rp.
 *
 * Config space algorithm:
 *	Match the space field of the hi order pci address field to the space
 *	field of the hi order child range entry.  When
 *	a match occurs, the bus,device,function of the hi order PCI address
 *	is used as an index into the parent range entry.
 *
 * IO, MEM32, and MEM64 algorithm:
 *	Match the space field of the hi order pci address field to the space
 *	field of the hi order child range entry.  When
 *	a match occurs, determine if the PCI address falls into the range
 *	specified by the child range entry.  If the
 *	address fits, determine the offset of the PCI address, within that
 *	of the child range, and apply that offset to the parent range
 *	entry.  If the PCI address doesn't fit the child range entry, get
 *	the next range entry and restart the match process.
 */
static int
pci_apply_range(pci_t *pci_p, dev_info_t *rdip, pci_regspec_t *pci_rp,
    struct regspec *rp)
{
	int b;
	pci_ranges_t *rangep = pci_p->pci_ranges;
	int nrange = pci_p->pci_ranges_length / sizeof (pci_ranges_t);
	uint32_t space_type = PCI_REG_ADDR_G(pci_rp->pci_phys_hi);
	uint32_t off, reg_begin, reg_end;

	if (space_type == PCI_REG_ADDR_G(PCI_ADDR_CONFIG)) {
		uint32_t sz_lo = pci_rp->pci_size_low;
		off = pci_rp->pci_phys_hi & PCI_CONF_ADDR_MASK;

		/* No n,p,t bits can be set in the address */
		if (pci_rp->pci_phys_hi != off)
			return (DDI_ME_INVAL);
		if (pci_rp->pci_phys_low > PCI_CONF_HDR_SIZE)
			return (DDI_ME_INVAL);

		/* deal with config space reg size lo = 0 */
		if (sz_lo) {
			if (pci_config_space_size_zero == 0)
				return (DDI_ME_INVAL);
			pci_rp->pci_size_low = MIN(sz_lo, PCI_CONF_HDR_SIZE);
		} else
			pci_rp->pci_size_low = PCI_CONF_HDR_SIZE;
	} else {
		reg_begin = pci_rp->pci_phys_low;
		reg_end = reg_begin + pci_rp->pci_size_low - 1;
	}

	for (b = 0; b < nrange; ++b, ++rangep) {
		if (space_type != PCI_REG_ADDR_G(rangep->child_high))
			continue;	/* not the same space type */
		if (space_type == PCI_REG_ADDR_G(PCI_ADDR_CONFIG)) {
			DEBUG0(DBG_MAP | DBG_CONT, pci_p->pci_dip,
				"Applying Configuration ranges\n");
			rp->regspec_addr = rangep->parent_low +
				off + pci_rp->pci_phys_low;
		} else {
			uint32_t rng_begin = rangep->child_low;
			uint32_t rng_end = rng_begin + rangep->size_low - 1;
			if ((reg_begin < rng_begin) || (reg_end > rng_end))
				continue;	/* not overlapping */

			DEBUG1(DBG_MAP | DBG_CONT, pci_p->pci_dip,
				"Applying %x ranges\n", space_type);
			off = reg_begin - rng_begin;
			rp->regspec_addr = rangep->parent_low + off;
		}
		break;
	}

	if (b >= nrange)  {
		cmn_err(CE_WARN, "Bad register specification from device %s\n",
			ddi_driver_name(rdip));
		return (DDI_ME_REGSPEC_RANGE);
	}
	rp->regspec_bustype = rangep->parent_high;
	rp->regspec_size = pci_rp->pci_size_low;

	DEBUG3(DBG_MAP | DBG_CONT, pci_p->pci_dip,
		"Child hi=0x%x mid=0x%x lo=0x%x\n",
		rangep->child_high, rangep->child_mid, rangep->child_low);
	DEBUG4(DBG_MAP | DBG_CONT, pci_p->pci_dip,
		"Parent hi=0x%x lo=0x%x Size hi=0x%x lo=0x%x\n",
		rangep->parent_high, rangep->parent_low,
		rangep->size_high, rangep->size_low);
	return (DDI_SUCCESS);
}


/*
 * get_addr
 *
 * This routine interprets the "assigned-addresses" property for a given
 * IEEE 1275 PCI format regspec.
 *
 * used by: xlate_reg_prop()
 *
 * return value:
 *
 *	DDI_SUCCESS	- on success
 *	DDI_ME_INVAL	- on failure
 */
/*ARGSUSED*/
static int
get_addr(pci_t *pci_p, dev_info_t *dip, pci_regspec_t *pci_rp,
	pci_regspec_t *augmented_rp)
{
	pci_regspec_t *assigned_addr, *save_addr;
	int assigned_addr_len = 0;
	int i;

	/*
	 * Initialize the physical address with the offset contained
	 * in the specified "reg" property entry.
	 */
	*augmented_rp = *pci_rp;

	/*
	 * Attempt to get the "assigned-addresses" property for the
	 * requesting device.
	 */
	if (ddi_getlongprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "assigned-addresses", (caddr_t)&assigned_addr,
	    &assigned_addr_len) != DDI_SUCCESS) {
		DEBUG2(DBG_MAP | DBG_CONT, pci_p->pci_dip,
		    "%s%d has no assigned-addresses property\n",
		    ddi_driver_name(dip), ddi_get_instance(dip));
		return (DDI_ME_INVAL);
	}

	save_addr = assigned_addr;

	/*
	 * Scan the "assigned-addresses" for one that matches the specified
	 * "reg" property entry.
	 */
	DEBUG1(DBG_MAP | DBG_CONT, pci_p->pci_dip, "get_addr - matching %x\n",
		augmented_rp->pci_phys_hi & PCI_CONF_ADDR_MASK);
	for (i = 0; i < assigned_addr_len / sizeof (pci_regspec_t); i++,
	    assigned_addr++) {
		DEBUG5(DBG_MAP | DBG_CONT, pci_p->pci_dip,
		    "get addr - checking ((%x,%x,%x) (%x,%x))\n",
		    assigned_addr->pci_phys_hi,
		    assigned_addr->pci_phys_mid,
		    assigned_addr->pci_phys_low,
		    assigned_addr->pci_size_hi,
		    assigned_addr->pci_size_low);
		if ((assigned_addr->pci_phys_hi & PCI_CONF_ADDR_MASK) ==
		    (augmented_rp->pci_phys_hi & PCI_CONF_ADDR_MASK)) {
			augmented_rp->pci_phys_low +=
			    assigned_addr->pci_phys_low;
			DEBUG1(DBG_MAP | DBG_CONT, pci_p->pci_dip,
			    "match - phys_lo=%x\n",
			    augmented_rp->pci_phys_low);
			break;
		}
	}

	/*
	 * Free the memory taken by the "assigned-addresses" property.
	 */
	if (assigned_addr_len) {
		kmem_free(save_addr, assigned_addr_len);
	}

	if (i == assigned_addr_len / sizeof (pci_regspec_t)) {
		return (DDI_ME_INVAL);
	}

	DEBUG5(DBG_MAP | DBG_CONT, pci_p->pci_dip,
	    "get addr - PCI physical addr ((%x,%x,%x) (%x,%x))\n",
	    augmented_rp->pci_phys_hi, augmented_rp->pci_phys_mid,
	    augmented_rp->pci_phys_low,
	    augmented_rp->pci_size_hi, augmented_rp->pci_size_low);

	return (DDI_SUCCESS);
}


/*
 * report_dev
 *
 * This function is called from our control ops routine on a
 * DDI_CTLOPS_REPORTDEV request.
 *
 * The display format is
 *
 *	<name><inst> at <pname><pinst> device <dev> function <func>
 *
 * where
 *
 *	<name>		this device's name property
 *	<inst>		this device's instance number
 *	<name>		parent device's name property
 *	<inst>		parent device's instance number
 *	<dev>		this device's device number
 *	<func>		this device's function number
 */
int
report_dev(dev_info_t *dip)
{
	if (dip == (dev_info_t *)0)
		return (DDI_FAILURE);
	cmn_err(CE_CONT, "?PCI-device: %s@%s, %s%d\n",
	    ddi_node_name(dip), ddi_get_name_addr(dip),
	    ddi_driver_name(dip),
	    ddi_get_instance(dip));
	return (DDI_SUCCESS);
}


/*
 * reg property for pcimem nodes that covers the entire address
 * space for the node:  config, io, or memory.
 */
pci_regspec_t pci_pcimem_reg[3] =
{
	{PCI_ADDR_CONFIG,			0, 0, 0, 0x800000	},
	{(uint_t)(PCI_ADDR_IO|PCI_RELOCAT_B),	0, 0, 0, PCI_IO_SIZE	},
	{(uint_t)(PCI_ADDR_MEM32|PCI_RELOCAT_B), 0, 0, 0, PCI_MEM_SIZE	}
};

/*
 * init_child
 *
 * This function is called from our control ops routine on a
 * DDI_CTLOPS_INITCHILD request.  It builds and sets the device's
 * parent private data area.
 *
 * used by: pci_ctlops()
 *
 * return value: none
 */
int
init_child(pci_t *pci_p, dev_info_t *child)
{
	pci_regspec_t *pci_rp;
	char name[10];
	int i, reglen;
	uint_t func;
	ddi_acc_handle_t config_handle;
	uint16_t command_preserve, command;
	uint8_t bcr;
	uint8_t header_type, min_gnt, latency_timer;
	char **unit_addr;
	uint_t n;

	/*
	 * The following is a special case for pcimem nodes.
	 * For these nodes we create a reg property with a
	 * single entry that covers the entire address space
	 * for the node (config, io or memory).
	 */
	if (strcmp(ddi_driver_name(child), "pcimem") == 0) {
		(void) ddi_prop_create(DDI_DEV_T_NONE, child,
		    DDI_PROP_CANSLEEP, "reg", (caddr_t)pci_pcimem_reg,
		    sizeof (pci_pcimem_reg));
		ddi_set_name_addr(child, "0");
		ddi_set_parent_data(child, NULL);
		return (DDI_SUCCESS);
	}

	/*
	 * Pseudo nodes indicate a prototype node with per-instance
	 * properties to be merged into the real h/w device node.
	 * The interpretation of the unit-address is DD[,F]
	 * where DD is the device id and F is the function.
	 */
	if (ndi_dev_is_persistent_node(child) == 0) {
		if (ddi_getlongprop(DDI_DEV_T_ANY, child,
		    DDI_PROP_DONTPASS, "reg", (caddr_t)&pci_rp, &i) ==
		    DDI_SUCCESS) {
			cmn_err(CE_WARN,
			    "cannot merge prototype from %s.conf",
			    ddi_driver_name(child));
			kmem_free(pci_rp, i);
			return (DDI_NOT_WELL_FORMED);
		}
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

	/*
	 * Set the address portion of the node name based on
	 * the function and device number.
	 */
	if (ddi_getlongprop(DDI_DEV_T_NONE, child, DDI_PROP_DONTPASS, "reg",
	    (caddr_t)&pci_rp, &reglen) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}
	func = PCI_REG_FUNC_G(pci_rp[0].pci_phys_hi);
	if (func != 0)
		(void) sprintf(name, "%x,%x",
			PCI_REG_DEV_G(pci_rp[0].pci_phys_hi), func);
	else
		(void) sprintf(name, "%x",
			PCI_REG_DEV_G(pci_rp[0].pci_phys_hi));
	ddi_set_name_addr(child, name);

	kmem_free(pci_rp, reglen);

	/*
	 * Map the child configuration space to for initialization.
	 * We assume the obp will do the following in the devices
	 * config space:
	 *
	 *	Set the latency-timer register to values appropriate
	 *	for the devices on the bus (based on other devices
	 *	MIN_GNT and MAX_LAT registers.
	 *
	 *	Set the fast back-to-back enable bit in the command
	 *	register if it's supported and all devices on the bus
	 *	have the capability.
	 *
	 */
	if (pci_config_setup(child, &config_handle) != DDI_SUCCESS)
		return (DDI_FAILURE);

	/*
	 * Determine the configuration header type.
	 */
	header_type = pci_config_get8(config_handle, PCI_CONF_HEADER);
	DEBUG2(DBG_INIT_CLD, pci_p->pci_dip, "%s: header_type=%x\n",
	    ddi_driver_name(child), header_type);

	/*
	 * Support for "command-preserve" property.  Note that we
	 * add PCI_COMM_BACK2BACK_ENAB to the bits to be preserved
	 * since the obp will set this if the device supports and
	 * all targets on the same bus support it.  Since psycho
	 * doesn't support PCI_COMM_BACK2BACK_ENAB, it will never
	 * be set.  This is just here in case future revs do support
	 * PCI_COMM_BACK2BACK_ENAB.
	 */
	command_preserve =
	    ddi_prop_get_int(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
		"command-preserve", 0);
	DEBUG2(DBG_INIT_CLD, pci_p->pci_dip, "%s: command-preserve=%x\n",
	    ddi_driver_name(child), command_preserve);
	command = pci_config_get16(config_handle, PCI_CONF_COMM);
	command &= (command_preserve | PCI_COMM_BACK2BACK_ENAB);
	command |= (pci_command_default & ~command_preserve);
	pci_config_put16(config_handle, PCI_CONF_COMM, command);
	command = pci_config_get16(config_handle, PCI_CONF_COMM);
	DEBUG2(DBG_INIT_CLD, pci_p->pci_dip, "%s: command=%x\n",
	    ddi_driver_name(child),
	    pci_config_get16(config_handle, PCI_CONF_COMM));

	/*
	 * If the device has a bus control register then program it
	 * based on the settings in the command register.
	 */
	if ((header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE) {
		bcr = pci_config_get8(config_handle, PCI_BCNF_BCNTRL);
		if (pci_command_default & PCI_COMM_PARITY_DETECT)
			bcr |= PCI_BCNF_BCNTRL_PARITY_ENABLE;
		if (pci_command_default & PCI_COMM_SERR_ENABLE)
			bcr |= PCI_BCNF_BCNTRL_SERR_ENABLE;
		bcr |= PCI_BCNF_BCNTRL_MAST_AB_MODE;
		pci_config_put8(config_handle, PCI_BCNF_BCNTRL, bcr);
	}

	/*
	 * Initialize cache-line-size configuration register if needed.
	 */
	if (pci_set_cache_line_size_register &&
	    ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
		"cache-line-size", 0) == 0) {

		pci_config_put8(config_handle, PCI_CONF_CACHE_LINESZ,
		    PCI_CACHE_LINE_SIZE);
		n = pci_config_get8(config_handle, PCI_CONF_CACHE_LINESZ);
		if (n != 0)
			(void) ndi_prop_update_int(DDI_DEV_T_NONE, child,
			    "cache-line-size", n);
	}

	/*
	 * Initialize latency timer registers if needed.
	 */
	if (pci_set_latency_timer_register &&
	    ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
		"latency-timer", 0) == 0) {

		if ((header_type & PCI_HEADER_TYPE_M) == PCI_HEADER_ONE) {
			latency_timer = pci_latency_timer;
			pci_config_put8(config_handle, PCI_BCNF_LATENCY_TIMER,
			    latency_timer);
		} else {
			min_gnt = pci_config_get8(config_handle,
			    PCI_CONF_MIN_G);
			DEBUG2(DBG_INIT_CLD, pci_p->pci_dip, "%s: min_gnt=%x\n",
			    ddi_driver_name(child), min_gnt);
			if (min_gnt != 0) {
				switch (pci_p->pci_pbm_p->pbm_speed) {
				case PBM_SPEED_33MHZ:
					latency_timer = min_gnt * 8;
					break;
				case PBM_SPEED_66MHZ:
					latency_timer = min_gnt * 4;
					break;
				}
			}
		}
		pci_config_put8(config_handle, PCI_CONF_LATENCY_TIMER,
		    latency_timer);
		n = pci_config_get8(config_handle, PCI_CONF_LATENCY_TIMER);
		if (n != 0)
			(void) ndi_prop_update_int(DDI_DEV_T_NONE, child,
			    "latency-timer", n);
	}

	pci_config_teardown(&config_handle);

	return (DDI_SUCCESS);
}


/*
 * get_reg_set_size
 *
 * Given a dev info pointer to a pci child and a register number, this
 * routine returns the size element of that reg set property.
 *
 * used by: pci_ctlops() - DDI_CTLOPS_REGSIZE
 *
 * return value: size of reg set on success, zero on error
 */
uint_t
get_reg_set_size(dev_info_t *child, int rnumber)
{
	pci_regspec_t *pci_rp;
	uint_t size;
	int i;

	if (rnumber < 0)
		return (0);

	/*
	 * Get the reg property for the device.
	 */
	if (ddi_getlongprop(DDI_DEV_T_NONE, child, DDI_PROP_DONTPASS, "reg",
	    (caddr_t)&pci_rp, &i) != DDI_SUCCESS)
		return (0);

	if (rnumber >= (i / (int)sizeof (pci_regspec_t)))
		return (0);

	size = pci_rp[rnumber].pci_size_low;
	kmem_free(pci_rp, i);
	return (size);
}


/*
 * get_nreg_set
 *
 * Given a dev info pointer to a pci child, this routine returns the
 * number of sets in its "reg" property.
 *
 * used by: pci_ctlops() - DDI_CTLOPS_NREGS
 *
 * return value: # of reg sets on success, zero on error
 */
uint_t
get_nreg_set(dev_info_t *child)
{
	pci_regspec_t *pci_rp;
	int i, n;

	/*
	 * Get the reg property for the device.
	 */
	if (ddi_getlongprop(DDI_DEV_T_NONE, child, DDI_PROP_DONTPASS, "reg",
	    (caddr_t)&pci_rp, &i) != DDI_SUCCESS)
		return (0);

	n = i / (int)sizeof (pci_regspec_t);
	kmem_free(pci_rp, i);
	return (n);
}


/*
 * get_nintr
 *
 * Given a dev info pointer to a pci child, this routine returns the
 * number of items in its "interrupts" property.
 *
 * used by: pci_ctlops() - DDI_CTLOPS_NREGS
 *
 * return value: # of interrupts on success, zero on error
 */
uint_t
get_nintr(dev_info_t *child)
{
	int *pci_ip;
	int i, n;

	if (ddi_getlongprop(DDI_DEV_T_NONE, child, DDI_PROP_DONTPASS,
	    "interrupts", (caddr_t)&pci_ip, &i) != DDI_SUCCESS)
		return (0);

	n = i / (int)sizeof (uint_t);
	kmem_free(pci_ip, i);
	return (n);
}


/*
 * These conveniece wrappers relies on map_pci_registers() to setup
 * pci_address[0-2] correctly at first.
 */
/* The psycho+ reg base is at 1fe.0000.0000 */
uintptr_t
get_reg_base(pci_t *pci_p)
{
	return ((uintptr_t)pci_p->pci_address[pci_stream_buf_exists ? 2 : 0]);
}

/* The psycho+ config reg base is always the 2nd reg entry */
uintptr_t
get_config_reg_base(pci_t *pci_p)
{
	return ((uintptr_t)(pci_p->pci_address[1]));
}

void
fault_init(pci_t *pci_p)
{
	/*
	 * Allocate our fault handling dispatch list.  We place the
	 * fault handler for the primary bus at the end of the list
	 * so that we can detect peek/poke faults on the secondary
	 * bus before detecting SERR at the primary bus.
	 */
	mutex_init(&pci_p->pci_fh_lst_mutex,
		"PCI Fault Handler List Mutex", MUTEX_DRIVER, NULL);
	pci_p->pci_fh_lst =
		kmem_alloc(sizeof (struct pci_fault_handle), KM_NOSLEEP);
	pci_p->pci_fh_lst->fh_f = pci_fault;
	pci_p->pci_fh_lst->fh_arg = pci_p;
	pci_p->pci_fh_lst->fh_dip = pci_p->pci_dip;
	pci_p->pci_fh_lst->fh_next = NULL;
}


void
fault_fini(pci_t *pci_p)
{
	/* XXX de-allocate memory of all the fault handler structures */
#ifdef lint
	pci_p = pci_p;
#endif
}

/*
 * decodes standard PCI config space 16bit error status reg
 * bridge_secondary:	0 primary bus status register
 *			1 PCI to PCI bridge secondary status register
 */
int
log_pci_cfg_err(ushort_t e, int bridge_secondary)
{
	int nerr = 0;
	if (e & PCI_STAT_PERROR) {
		nerr++;
		cmn_err(CE_CONT, "detected parity error.\n");
	}
	if (e & PCI_STAT_S_SYSERR) {
		nerr++;
		if (bridge_secondary)
			cmn_err(CE_CONT, "received system error.\n");
		else
			cmn_err(CE_CONT, "signalled system error.\n");
	}
	if (e & PCI_STAT_R_MAST_AB) {
		nerr++;
		cmn_err(CE_CONT, "received master abort.\n");
	}
	if (e & PCI_STAT_R_TARG_AB)
		cmn_err(CE_CONT, "received target abort.\n");
	if (e & PCI_STAT_S_TARG_AB)
		cmn_err(CE_CONT, "signalled target abort\n");
	if (e & PCI_STAT_S_PERROR) {
		nerr++;
		cmn_err(CE_CONT, "signalled parity error\n");
	}
	return (nerr);
}
