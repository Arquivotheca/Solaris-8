/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ddi_impl.c	1.87	99/10/22 SMI"

/*
 * sun4u specific DDI implementation
 */
#include <sys/types.h>
#include <sys/autoconf.h>
#include <sys/avintr.h>
#include <sys/bootconf.h>
#include <sys/conf.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/ddi_subrdefs.h>
#include <sys/ethernet.h>
#include <sys/idprom.h>
#include <sys/instance.h>
#include <sys/kmem.h>
#include <sys/machsystm.h>
#include <sys/mmu.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/sunndi.h>
#include <sys/ndi_impldefs.h>
#include <sys/sysmacros.h>
#include <sys/systeminfo.h>
#include <sys/fpu/fpusystm.h>
#include <sys/vm.h>
#include <vm/seg_kmem.h>


/*
 * Boot Configuration
 */

idprom_t idprom;

/*
 * Favoured drivers of this implementation
 * architecture.  These drivers MUST be present for
 * the system to boot at all.
 */
char *impl_module_list[] = {
	"rootnex",
	"options",
	"dma",	/* needed for old scsi cards, do not remove */
	"sad",		/* Referenced via init_tbl[] */
	(char *)0
};

/*
 * List of drivers to do multi-threaded probe/attach
 */
char *default_mta_drivers = "sd ssd ses st";


/*
 * These strings passed to not_serviced in locore.s
 */
const char busname_ovec[] = "onboard ";
const char busname_svec[] = "SBus ";
const char busname_vec[] = "";

/*
 * Forward declarations
 */
static int getlongprop_buf();
static int get_boardnum(int nid, dev_info_t *par);

/*
 * Configure the hardware on the system.
 * Called before the rootfs is mounted
 */
void
configure(void)
{
	major_t major;
	dev_info_t *dip;

	/* We better have released boot by this time! */

	ASSERT(!bootops);

	/*
	 * Determine whether or not to use the fpu, V9 SPARC cpus
	 * always have one. Could check for existence of a fp queue,
	 * Ultra I, II and IIa do not have a fp queue.
	 */
	if (fpu_exists)
		fpu_probe();
	else
		cmn_err(CE_CONT, "FPU not in use\n");

	/*
	 * This following line fixes bugid 1041296; we need to do a
	 * prom_nextnode(0) because this call ALSO patches the DMA+
	 * bug in Campus-B and Phoenix. The prom uncaches the traptable
	 * page as a side-effect of devr_next(0) (which prom_nextnode calls),
	 * so this *must* be executed early on. (XXX This is untrue for sun4u)
	 */
	(void) prom_nextnode((dnode_t)0);

	/*
	 * Initialize devices on the machine.
	 * Uses configuration tree built by the PROMs to determine what
	 * is present, and builds a tree of prototype dev_info nodes
	 * corresponding to the hardware which identified itself.
	 */

	/*
	 * Record that devinfos have been made for "rootnex."
	 */
	major = ddi_name_to_major("rootnex");
	devnamesp[major].dn_flags |= DN_DEVI_MADE;

	/*
	 * Set the name part of the address to make the root conform
	 * to canonical form 1.  (Eliminates special cases later).
	 */
	dip = ddi_root_node();
	if (impl_ddi_sunbus_initchild(dip) != DDI_SUCCESS)
		cmn_err(CE_PANIC, "Could not initialize root nexus");

#ifdef	DDI_PROP_DEBUG
	(void) ddi_prop_debug(1);	/* Enable property debugging */
#endif	DDI_PROP_DEBUG
}

/*
 * The "status" property indicates the operational status of a device.
 * If this property is present, the value is a string indicating the
 * status of the device as follows:
 *
 *	"okay"		operational.
 *	"disabled"	not operational, but might become operational.
 *	"fail"		not operational because a fault has been detected,
 *			and it is unlikely that the device will become
 *			operational without repair. no additional details
 *			are available.
 *	"fail-xxx"	not operational because a fault has been detected,
 *			and it is unlikely that the device will become
 *			operational without repair. "xxx" is additional
 *			human-readable information about the particular
 *			fault condition that was detected.
 *
 * The absense of this property means that the operational status is
 * unknown or okay.
 *
 * This routine checks the status property of the specified device node
 * and returns 0 if the operational status indicates failure, and 1 otherwise.
 *
 * The property may exist on plug-in cards the existed before IEEE 1275-1994.
 * And, in that case, the property may not even be a string. So we carefully
 * check for the value "fail", in the beginning of the string, noting
 * the property length.
 */
int
status_okay(int id, char *buf, int buflen)
{
	char status_buf[OBP_MAXPROPNAME];
	char *bufp = buf;
	int len = buflen;
	int proplen;
	static const char *status = "status";
	static const char *fail = "fail";
	size_t fail_len = strlen(fail);

	/*
	 * Get the proplen ... if it's smaller than "fail",
	 * or doesn't exist ... then we don't care, since
	 * the value can't begin with the char string "fail".
	 *
	 * NB: proplen, if it's a string, includes the NULL in the
	 * the size of the property, and fail_len does not.
	 */
	proplen = prom_getproplen((dnode_t)id, (caddr_t)status);
	if (proplen <= fail_len)	/* nonexistant or uninteresting len */
		return (1);

	/*
	 * if a buffer was provided, use it
	 */
	if ((buf == (char *)NULL) || (buflen <= 0)) {
		bufp = status_buf;
		len = sizeof (status_buf);
	}
	*bufp = (char)0;

	/*
	 * Get the property into the buffer, to the extent of the buffer,
	 * and in case the buffer is smaller than the property size,
	 * NULL terminate the buffer. (This handles the case where
	 * a buffer was passed in and the caller wants to print the
	 * value, but the buffer was too small).
	 */
	(void) prom_bounded_getprop((dnode_t)id, (caddr_t)status,
	    (caddr_t)bufp, len);
	*(bufp + len - 1) = (char)0;

	/*
	 * If the value begins with the char string "fail",
	 * then it means the node is failed. We don't care
	 * about any other values. We assume the node is ok
	 * although it might be 'disabled'.
	 */
	if (strncmp(bufp, fail, fail_len) == 0)
		return (0);

	return (1);
}

/*
 * Check the status of the device node passed as an argument.
 *
 *	if ((status is OKAY) || (status is DISABLED))
 *		return DDI_SUCCESS
 *	else
 *		print a warning and return DDI_FAILURE
 */
/*ARGSUSED*/
int
check_status(int id, char *buf, dev_info_t *parent)
{
	char status_buf[64];
	char devtype_buf[OBP_MAXPROPNAME];
	char board_buf[32];
	char path[OBP_MAXPATHLEN];
	int boardnum;
	int retval = DDI_FAILURE;

	/*
	 * is the status okay?
	 */
	if (status_okay(id, status_buf, sizeof (status_buf)))
		return (DDI_SUCCESS);

	/*
	 * a status property indicating bad memory will be associated
	 * with a node which has a "device_type" property with a value of
	 * "memory-controller". in this situation, return DDI_SUCCESS
	 */
	if (getlongprop_buf(id, OBP_DEVICETYPE, devtype_buf,
	    sizeof (devtype_buf)) > 0) {
		if (strcmp(devtype_buf, "memory-controller") == 0)
			retval = DDI_SUCCESS;
	}

	/*
	 * get the full OBP pathname of this node
	 */
	if (prom_phandle_to_path((phandle_t)id, path, sizeof (path)) < 0)
		cmn_err(CE_WARN, "prom_phandle_to_path(%d) failed\n", id);

	/*
	 * get the board number, if one exists
	 */
	if ((boardnum = get_boardnum(id, parent)) >= 0)
		(void) sprintf(board_buf, " on board %d", boardnum);
	else
		board_buf[0] = '\0';

	/*
	 * print the status property information
	 */
	cmn_err(CE_WARN, "status '%s' for '%s'%s",
		status_buf, path, board_buf);
	return (retval);
}

/*
 * determine the board number associated with this nodeid
 */
static int
get_boardnum(int nid, dev_info_t *par)
{
	int board_num;

	if (prom_getprop((dnode_t)nid, OBP_BOARDNUM,
	    (caddr_t)&board_num) != -1)
		return (board_num);

	/*
	 * Look at current node and up the parent chain
	 * till we find a node with an OBP_BOARDNUM.
	 */
	while (par) {
		nid = ddi_get_nodeid(par);

		if (prom_getprop((dnode_t)nid, OBP_BOARDNUM,
		    (caddr_t)&board_num) != -1)
			return (board_num);

		par = ddi_get_parent(par);
	}
	return (-1);
}

/*
 * We set the cpu type from the idprom, if we can.
 * Note that we just read out the contents of it, for the most part.
 */
void
setcputype(void)
{
	/*
	 * We cache the idprom info early on so that we don't
	 * rummage thru the NVRAM unnecessarily later.
	 */
	(void) prom_getidprom((caddr_t)&idprom, sizeof (idprom));
}

/*
 *  Here is where we actually infer meanings to the members of idprom_t
 */
void
parse_idprom(void)
{
	if (idprom.id_format == IDFORM_1) {
		uint_t i;

		(void) localetheraddr((struct ether_addr *)idprom.id_ether,
		    (struct ether_addr *)NULL);

		i = idprom.id_machine << 24;
		i = i + idprom.id_serial;
		numtos((ulong_t)i, hw_serial);
	} else
		prom_printf("Invalid format code in IDprom.\n");
}

/*
 * Allow for implementation specific correction of PROM property values.
 */
/*ARGSUSED*/
void
impl_fix_props(dev_info_t *dip, dev_info_t *ch_dip, char *name, int len,
    caddr_t buffer)
{
	/*
	 * There are no adjustments needed in this implementation.
	 */
}

/*
 * Wrapper for ddi_prop_lookup_int_array().
 * This is handy because it returns the prop length in
 * bytes which is what most of the callers require.
 */

static int
get_prop_int_array(dev_info_t *di, char *pname, int **pval, uint_t *plen)
{
	int ret;

	if ((ret = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, di,
	    DDI_PROP_DONTPASS, pname, pval, plen)) == DDI_PROP_SUCCESS) {
		*plen = (*plen) * (uint_t)sizeof (int);
	}
	return (ret);
}

/*
 * Note that this routine does not take into account the endianness
 * of the host or the device (or PROM) when retrieving properties.
 */
static int
getlongprop_buf(int id, char *name, char *buf, int maxlen)
{
	int size;

	size = prom_getproplen((dnode_t)id, name);
	if (size <= 0 || (size > maxlen - 1))
		return (-1);

	if (-1 == prom_getprop((dnode_t)id, name, buf))
		return (-1);

	/*
	 * Workaround for bugid 1085575 - OBP may return a "name" property
	 * without null terminating the string with '\0'.  When this occurs,
	 * append a '\0' and return (size + 1).
	 */
	if (strcmp("name", name) == 0) {
		if (buf[size - 1] != '\0') {
			buf[size] = '\0';
			size += 1;
		}
	}

	return (size);
}


/*
 * SECTION: DDI Node Configuration
 */

extern int impl_ddi_merge_child(dev_info_t *child);

/*
 * impl_ddi_merge_wildcard:
 *
 * Framework function to merge .conf file 'wildcard' nodes into all
 * previous named hw nodes of the same "name", with the same parent.
 *
 * This path is a bit different from impl_ddi_merge_child.  merge_child
 * merges a single child with the same exact name and address; this
 * function can merge into several nodes with the same driver name, only.
 *
 * This function may be used by buses which export a wildcarding mechanism
 * in .conf files, such as the "registers" mechanism, in sbus .conf files.
 * (The registers applies to all prev named nodes' reg properties.)
 *
 * This function always returns DDI_FAILURE as an indication to the
 * caller's caller, that the wildcard node has been uninitialized
 * and should be removed. (Presumably, it's properties have been copied
 * into the "merged" children of the same parent.)
 */
static char *cantmerge = "Cannot merge %s devinfo node %s@%s";

int
impl_ddi_merge_wildcard(dev_info_t *child)
{
	major_t major;
	dev_info_t *parent, *och;

	parent = ddi_get_parent(child);

	/*
	 * If the wildcard node has no properties, there is nothing to do...
	 */
	if ((DEVI(child)->devi_drv_prop_ptr == NULL) &&
	    (DEVI(child)->devi_sys_prop_ptr == NULL))  {
		/* Compensate for extra release done in ddi_uninitchild() */
		(void) ddi_hold_devi(parent);
		(void) ddi_uninitchild(child);
		return (DDI_FAILURE);
	}

	/*
	 * Find all previously defined h/w children with the same name
	 * and same parent and copy the property lists from the
	 * prototype node into the h/w nodes and re-inititialize them.
	 */
	if ((major = ddi_name_to_major(ddi_get_name(child))) == -1)  {
		/* Compensate for extra release done in ddi_uninitchild() */
		(void) ddi_hold_devi(parent);
		(void) ddi_uninitchild(child);
		return (DDI_FAILURE);
	}

	for (och = devnamesp[major].dn_head;
	    (och != NULL) && (och != child);
	    och = ddi_get_next(och))  {

		if ((ddi_get_parent(och) != parent) ||
		    (ndi_dev_is_persistent_node(och) == 0))
			continue;
		if (strcmp(ddi_get_name(och), ddi_get_name(child)) != 0)
			continue;

		if (DEVI(och)->devi_sys_prop_ptr ||
		    DEVI(och)->devi_drv_prop_ptr || DDI_CF2(och)) {
			cmn_err(CE_WARN, cantmerge, "wildcard",
			    ddi_get_name(och), ddi_get_name_addr(och));
			continue;
		}

		(void) ddi_uninitchild(och);
		mutex_enter(&(DEVI(child)->devi_lock));
		mutex_enter(&(DEVI(och)->devi_lock));
		(void) copy_prop(DEVI(child)->devi_drv_prop_ptr,
		    &(DEVI(och)->devi_drv_prop_ptr));
		(void) copy_prop(DEVI(child)->devi_sys_prop_ptr,
		    &(DEVI(och)->devi_sys_prop_ptr));
		mutex_exit(&(DEVI(och)->devi_lock));
		mutex_exit(&(DEVI(child)->devi_lock));
		(void) ddi_initchild(parent, och);
	}

	/*
	 * We can toss the wildcard node. Note that we do an extra
	 * hold on the parent to compensate for the extra release
	 * done in ddi_uninitchild(). [ plus the release already done
	 * by returning "failure" from this function. ]
	 */
	(void) ddi_hold_devi(parent);
	(void) ddi_uninitchild(child);
	return (DDI_FAILURE);
}

/*
 * Create a ddi_parent_private_data structure from the ddi properties of
 * the dev_info node.
 *
 * The "reg" and "interrupts" properties are required
 * if the driver wishes to create mappings or field interrupts on behalf
 * of the device.
 *
 * The "reg" property is assumed to be a list of at least one triple
 *
 *	<bustype, address, size>*1
 *
 * The "interrupts" property is assumed to be a list of at least one
 * n-tuples that describes the interrupt capabilities of the bus the device
 * is connected to.  For SBus, this looks like
 *
 *	<SBus-level>*1
 *
 * The "ranges" property describes the mapping of child addresses to parent
 * addresses.
 */
static void
make_ddi_ppd(dev_info_t *child, struct ddi_parent_private_data **ppd)
{
	struct ddi_parent_private_data *pdptr;
	int n;
	int *reg_prop, *rng_prop;
	uint_t reg_len, rng_len;

	*ppd = pdptr = kmem_zalloc(sizeof (*pdptr), KM_SLEEP);

	/*
	 * "reg" property ...
	 */

	if (get_prop_int_array(child, OBP_REG, &reg_prop, &reg_len)
	    != DDI_PROP_SUCCESS) {
		reg_len = 0;
	}

	if ((n = reg_len) != 0)  {
		pdptr->par_nreg = n / (int)sizeof (struct regspec);
		pdptr->par_reg = (struct regspec *)reg_prop;
	}

	/*
	 * "ranges" property ...
	 */
	if (get_prop_int_array(child, OBP_RANGES, &rng_prop,
	    &rng_len) == DDI_PROP_SUCCESS) {
		pdptr->par_nrng = rng_len / (int)(sizeof (struct rangespec));
		pdptr->par_rng = (struct rangespec *)rng_prop;
	}
}

/*
 * Called from the bus_ctl op of some drivers.
 * to implement the DDI_CTLOPS_INITCHILD operation.  That is, it names
 * the children of sun busses based on the reg spec.
 *
 * Handles the following properties:
 *
 *	Property		value
 *	  Name			type
 *
 *	reg		register spec
 *	interrupts	new (bus-oriented) interrupt spec
 *	ranges		range spec
 *
 * NEW drivers should NOT use this function, but should declare
 * there own initchild/uninitchild handlers. (This function assumes
 * the layout of the parent private data and the format of "reg",
 * "ranges", "interrupts" properties and that #address-cells and
 * #size-cells of the parent bus are defined to be default values.)
 */
int
impl_ddi_sunbus_initchild(dev_info_t *child)
{
	struct ddi_parent_private_data *pdptr;
	char name[MAXNAMELEN];
	dev_info_t *parent;
	int portid;
	extern uint_t root_phys_addr_lo_mask;

	/*
	 * Fill in parent-private data and this function returns to us
	 * an indication if it used "registers" to fill in the data.
	 */
	make_ddi_ppd(child, &pdptr);
	ddi_set_parent_data(child, (caddr_t)pdptr);
	parent = ddi_get_parent(child);

	name[0] = '\0';
	if (sparc_pd_getnreg(child) > 0) {
		struct regspec *rp = sparc_pd_getreg(child, 0);
		/*
		 * On sun4u, the 'name' of children of the root node
		 * is foo@<upa-mid>,<offset>, which is derived from,
		 * but not identical to the physical address.
		 */
		if (parent == ddi_root_node()) {
			portid = ddi_prop_get_int(DDI_DEV_T_ANY, child,
			    DDI_PROP_DONTPASS, "upa-portid", -1);
			if (portid == -1)
				portid = ddi_prop_get_int(DDI_DEV_T_ANY, child,
				    DDI_PROP_DONTPASS, "portid", -1);
			if (portid == -1)
				cmn_err(CE_WARN, "could not find portid "
				    "property in %s\n",
				    DEVI(child)->devi_node_name);
			(void) sprintf(name, "%x,%x", portid,
			    rp->regspec_addr & root_phys_addr_lo_mask);
		} else {
			(void) sprintf(name, "%x,%x",
			    rp->regspec_bustype,
			    rp->regspec_addr);
		}
	}

	ddi_set_name_addr(child, name);
	return (impl_ddi_merge_child(child));
}

/*
 * A better name for this function would be impl_ddi_sunbus_uninitchild()
 * It does not remove the child, it uninitializes it, reclaiming the
 * resources taken by impl_ddi_sunbus_initchild.
 */
void
impl_ddi_sunbus_removechild(dev_info_t *dip)
{
	register struct ddi_parent_private_data *pdptr;

	if ((pdptr = (struct ddi_parent_private_data *)
	    ddi_get_parent_data(dip)) != NULL)  {
		if (pdptr->par_nrng != 0)
			ddi_prop_free((void *)pdptr->par_rng);

		if (pdptr->par_nreg != 0)
			ddi_prop_free((void *)pdptr->par_reg);

		kmem_free(pdptr, sizeof (*pdptr));
		ddi_set_parent_data(dip, NULL);
	}
	ddi_set_name_addr(dip, NULL);
	/*
	 * Strip the node to properly convert it back to prototype form
	 */
	ddi_remove_minor_node(dip, NULL);
	impl_rem_dev_props(dip);
}

/*
 * SECTION: DDI Interrupt
 */

static void
cells_1275_copy(prop_1275_cell_t *from, prop_1275_cell_t *to, int32_t len)
{
	int i;
	for (i = 0; i < len; i++)
		*to = *from;
}

static prop_1275_cell_t *
cells_1275_cmp(prop_1275_cell_t *cell1, prop_1275_cell_t *cell2, int32_t len)
{
	prop_1275_cell_t *match_cell = 0;
	int32_t i;

	for (i = 0; i < len; i++)
		if (cell1[i] != cell2[i]) {
			match_cell = &cell1[i];
			break;
		}

	return (match_cell);
}


/*
 * get_intr_parent() is a generic routine that process a 1275 interrupt
 * map (imap) property.  This function returns a dev_info_t structure
 * which claims ownership of the interrupt domain.
 * It also returns the new interrupt translation within this new domain.
 * If an interrupt-parent or interrupt-map property are not found,
 * then we fallback to using the device tree's parent.
 *
 * imap entry format:
 * <reg>,<interrupt>,<phandle>,<translated interrupt>
 * reg - The register specificaton in the interrupts domain
 * interrupt - The interrupt specification
 * phandle - PROM handle of the device that owns the xlated interrupt domain
 * translated interrupt - interrupt specifier in the parents domain
 * note: <reg>,<interrupt> - The reg and interrupt can be combined to create
 *	a unique entry called a unit interrupt specifier.
 *
 * Here's the processing steps:
 * step1 - If the interrupt-parent property exists, create the ispec and
 *	return the dip of the interrupt parent.
 * step2 - Extract the interrupt-map property and the interrupt-map-mask
 *	If these don't exist, just return the device tree parent.
 * step3 - build up the unit interrupt specifier to match against the
 *	interrupt map property
 * step4 - Scan the interrupt-map property until a match is found
 * step4a - Extract the interrupt parent
 * step4b - Compare the unit interrupt specifier
 */
static dev_info_t *
get_intr_parent(dev_info_t *pdip, dev_info_t *dip,
    ddi_ispec_t *child_ispecp, ddi_ispec_t **new_ispecp)
{
	prop_1275_cell_t *imap, *imap_mask, *scan, *reg_p, *match_req;
	int32_t imap_sz, imap_cells, imap_scan_cells, imap_mask_sz,
	    addr_cells, intr_cells, reg_len, i, j;
	int32_t match_found = 0;
	dev_info_t *intr_parent_dip = NULL;
	ddi_ispec_t *ispecp;
	uint32_t *intr = child_ispecp->is_intr;
	uint32_t nodeid;
	static ddi_ispec_t *dup_ispec(ddi_ispec_t *ispecp);
#ifdef DEBUG
	static int debug = 0;
#endif

	*new_ispecp = (ddi_ispec_t *)NULL;

	/*
	 * step1
	 * If we have an interrupt-parent property, this property represents
	 * the nodeid of our interrupt parent.
	 */
	if ((nodeid = ddi_getprop(DDI_DEV_T_ANY, dip, 0,
	    "interrupt-parent", -1)) != -1) {
		intr_parent_dip =
		    e_ddi_nodeid_to_dip(ddi_root_node(), nodeid);
		/*
		 * If we have an interrupt parent whose not in our device tree
		 * path, we need to hold and install that driver.
		 */
		if (ddi_hold_installed_driver(ddi_name_to_major(ddi_get_name(
		    intr_parent_dip))) == NULL) {
			intr_parent_dip = (dev_info_t *)NULL;
			goto exit1;
		}

		/* Create a new interrupt info struct and initialize it. */
		ispecp = dup_ispec(child_ispecp);

		*new_ispecp = ispecp;
		return (intr_parent_dip);
	}

	/*
	 * step2
	 * Get interrupt map structure from PROM property
	 */
	if (ddi_getlongprop(DDI_DEV_T_ANY, pdip, DDI_PROP_DONTPASS,
	    "interrupt-map", (caddr_t)&imap, &imap_sz)
	    != DDI_PROP_SUCCESS) {
		/*
		 * If we don't have an imap property, default to using the
		 * device tree.
		 */
		/* Create a new interrupt info struct and initialize it. */
		ispecp = dup_ispec(child_ispecp);

		*new_ispecp = ispecp;
		return (pdip);
	}

	/* Get the interrupt mask property */
	if (ddi_getlongprop(DDI_DEV_T_ANY, pdip, DDI_PROP_DONTPASS,
	    "interrupt-map-mask", (caddr_t)&imap_mask, &imap_mask_sz)
	    != DDI_PROP_SUCCESS)
		/*
		 * If we don't find this property, we have to fail the request
		 * because the 1275 imap property wasn't defined correctly.
		 */
		goto exit2;

	/* Get the address cell size */
	addr_cells = ddi_getprop(DDI_DEV_T_ANY, pdip, 0,
	    "#address-cells", 2);

	/* Get the interrupts cell size */
	intr_cells = ddi_getprop(DDI_DEV_T_ANY, pdip, 0,
	    "#interrupt-cells", 1);

	/*
	 * step3
	 * Now lets build up the unit interrupt specifier e.g. reg,intr
	 * and apply the imap mask.  match_req will hold this when we're
	 * through.
	 */
	if (ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS, "reg",
	    (caddr_t)&reg_p, &reg_len) != DDI_SUCCESS)
		goto exit3;

	match_req = kmem_alloc(CELLS_1275_TO_BYTES(addr_cells) +
	    CELLS_1275_TO_BYTES(intr_cells), KM_SLEEP);

	for (i = 0; i < addr_cells; i++)
		match_req[i] = (reg_p[i] & imap_mask[i]);

	for (j = 0; j < intr_cells; i++, j++)
		match_req[i] = (intr[j] & imap_mask[i]);

	/* Calculate the imap size in cells */
	imap_cells = BYTES_TO_1275_CELLS(imap_sz);

#ifdef DEBUG
	if (debug)
		prom_printf("reg cell size 0x%x, intr cell size 0x%x, "
		    "match_request 0x%x, imap 0x%x\n", addr_cells, intr_cells,
		    match_req, imap);
#endif
	/*
	 * Scan the imap property looking for a match of the interrupt unit
	 * specifier.  This loop is rather complex since the data within the
	 * imap property may vary in size.
	 */
	for (scan = imap, imap_scan_cells = i = 0;
	    imap_scan_cells < imap_cells; scan += i, imap_scan_cells += i) {
		int new_intr_cells;

		/* Set the index to the nodeid field */
		i = addr_cells + intr_cells;
		/*
		 * step4a
		 * Translate the nodeid field to a dip
		 */
		intr_parent_dip = e_ddi_nodeid_to_dip(ddi_root_node(),
		    (uint_t)scan[i++]);

		ASSERT(intr_parent_dip != 0);
		/*
		 * If we have an imap parent whose not in our device
		 * tree path, we need to hold and install that driver.
		 */
		if (ddi_hold_installed_driver(ddi_name_to_major(
		    ddi_get_name(intr_parent_dip))) == NULL) {
			intr_parent_dip = (dev_info_t *)NULL;
			goto exit4;
		}
#ifdef DEBUG
		if (debug)
			prom_printf("scan 0x%x\n", scan);
#endif
		/*
		 * The tmp_dip describes the new domain, get it's interrupt
		 * cell size
		 */
		new_intr_cells = ddi_getprop(DDI_DEV_T_ANY, intr_parent_dip, 0,
		    "#interrupts-cells", 1);

		/*
		 * step4b
		 * See if we have a match on the interrupt unit specifier
		 */
		if (cells_1275_cmp(match_req, scan, addr_cells + intr_cells)
		    == 0) {
			ddi_ispec_t ispec;
			uint32_t *intr;

			/*
			 * Copy The childs ispec info excluding the interrupt
			 */
			ispec = *child_ispecp;

			match_found = 1;

			/*
			 * We need to handcraft an ispec along with a bus
			 * interrupt value, so we can dup it into our
			 * standard ispec structure.
			 */
			/* Extract the translated interrupt information */
			intr = kmem_alloc(
			    CELLS_1275_TO_BYTES(new_intr_cells), KM_SLEEP);

			for (j = 0; j < new_intr_cells; j++, i++)
				intr[j] = scan[i];

			ispec.is_intr_sz =
			    CELLS_1275_TO_BYTES(new_intr_cells);
			ispec.is_intr = intr;

			ispecp = dup_ispec(&ispec);

			kmem_free(intr, CELLS_1275_TO_BYTES(new_intr_cells));

#ifdef DEBUG
			if (debug)
				prom_printf("dip 0x%x, intr info 0x%x\n",
				    intr_parent_dip, ispecp);
#endif

			break;
		} else {
#ifdef DEBUG
			if (debug)
				prom_printf("dip 0x%x\n", intr_parent_dip);
#endif
			i += new_intr_cells;
		}
	}

	/*
	 * If we haven't found our interrupt parent at this point, fallback
	 * to using the device tree.
	 */
	if (!match_found) {
		/* Create a new interrupt info struct and initialize it. */
		ispecp = dup_ispec(child_ispecp);

		intr_parent_dip = pdip;
	}

	ASSERT(ispecp != NULL);
	ASSERT(intr_parent_dip != NULL);
	*new_ispecp = ispecp;

exit4:
	kmem_free(reg_p, reg_len);
	kmem_free(match_req, CELLS_1275_TO_BYTES(addr_cells) +
	    CELLS_1275_TO_BYTES(intr_cells));

exit3:
	kmem_free(imap_mask, imap_mask_sz);

exit2:
	kmem_free(imap, imap_sz);

exit1:
	return (intr_parent_dip);
}

/*
 * i_ddi_intr_ctlops is used to process The consolidated interrupt ctlops.
 * On 4u and later systems, we also process interrupts utilizing the interrupt
 * map proposal of the 1275 specification.
 */
int
i_ddi_intr_ctlops(dev_info_t *dip, dev_info_t *rdip, ddi_intr_ctlop_t op,
    void *arg, void *val)
{
	ddi_ispec_t *sav_ispecp, *ispecp = NULL;
	struct bus_ops *bus_ops;
	int (*fp)();
	dev_info_t *pdip = ddi_get_parent(dip);
	int ret = DDI_FAILURE;

	switch (op) {
	case DDI_INTR_CTLOPS_ADD:
	case DDI_INTR_CTLOPS_REMOVE:
	case DDI_INTR_CTLOPS_HILEVEL:
		/*
		 * If we have an ispec struct, try and determine our
		 * parent and possibly an interrupt translation.
		 */
		/* save the ispec */
		sav_ispecp =
		    (ddi_ispec_t *)(((ddi_intr_info_t *)arg)->ii_ispec);

		if ((pdip = get_intr_parent(pdip, dip, sav_ispecp,
		    &ispecp)) != NULL) {
			/* Insert the interrupt info sructure */
			((ddi_intr_info_t *)arg)->ii_ispec =
			    (ddi_intrspec_t)ispecp;
		} else
			goto done;
	}

	bus_ops = DEVI(pdip)->devi_ops->devo_bus_ops;
	fp = bus_ops->bus_intr_ctl;

	/*
	 * Sanity check the version of the nexus driver to see that it
	 * suports the new interrupt model and that it defined a vector.
	 */
	if (bus_ops->busops_rev >= BUSO_REV_4 && bus_ops->bus_intr_ctl != 0)
		/* Process the interrupt op via the interrupt parent */
		ret = (*fp)(pdip, rdip, op, arg, val);
	else
		cmn_err(CE_WARN, "Failed to process interrupt for %s%d "
		    "due to down-rev nexus driver %s%d\n", ddi_get_name(rdip),
		    ddi_get_instance(rdip), ddi_get_name(pdip),
		    ddi_get_instance(pdip));

done:
	switch (op) {
	case DDI_INTR_CTLOPS_ADD:
	case DDI_INTR_CTLOPS_REMOVE:
	case DDI_INTR_CTLOPS_HILEVEL:
		if (ispecp) {
			/* Set the PIL according to what the parent did */
			sav_ispecp->is_pil = ispecp->is_pil;
			/* Free the stacked ispec structure */
			i_ddi_free_ispec((ddi_intrspec_t)ispecp);
		}
		/* Restore the interrupt info */
		((ddi_intr_info_t *)arg)->ii_ispec =
		    (ddi_intrspec_t)sav_ispecp;
	}

	return (ret);
}

/*
 * Support routine for allocating and initializing an interrupt specification.
 * The bus interrupt value will be allocated at the end of this structure, so
 * the corresponding routine i_ddi_free_ispec() should be used to free the
 * interrupt specification.
 */
void
i_ddi_alloc_ispec(dev_info_t *dip, uint_t inumber, ddi_intrspec_t *intrspecp)
{
	int32_t intrlen, intr_cells, max_intrs;
	prop_1275_cell_t *ip;
	prop_1275_cell_t intr_sz;
	ddi_ispec_t **ispecp = (ddi_ispec_t **)intrspecp;

	*ispecp = NULL;
	if (ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS |
	    DDI_PROP_CANSLEEP,
	    "interrupts", (caddr_t)&ip, &intrlen) == DDI_SUCCESS) {

		intr_cells = ddi_getprop(DDI_DEV_T_ANY, dip, 0,
		    "#interrupt-cells", 1);

		/* adjust for number of bytes */
		intr_sz = CELLS_1275_TO_BYTES(intr_cells);

		/* Calculate the number of interrupts */
		max_intrs = intrlen / intr_sz;

		if (inumber < max_intrs) {
			prop_1275_cell_t *intrp = ip;

			*ispecp = kmem_zalloc(
			    (sizeof (ddi_ispec_t) + intr_sz), KM_SLEEP);

			(*ispecp)->is_intr =
			    (uint32_t *)(*ispecp + 1);

			/* Index into interrupt property */
			intrp += (inumber * intr_cells);

			cells_1275_copy(intrp,
			    (*ispecp)->is_intr, intr_cells);

			(*ispecp)->is_intr_sz = intr_sz;

			(*ispecp)->is_pil = i_ddi_get_intr_pri(dip, inumber);
		}

		kmem_free((caddr_t)ip, intrlen);
	}
}


/*
 * Support routine for duplicating and initializing an interrupt specification.
 * The bus interrupt value will be allocated at the end of this structure, so
 * the corresponding routine i_ddi_free_ispec() should be used to free the
 * interrupt specification.
 */
static ddi_ispec_t *
dup_ispec(ddi_ispec_t *ispecp)
{
	ddi_ispec_t *new_ispecp;

	new_ispecp = kmem_alloc(sizeof (ddi_ispec_t) + ispecp->is_intr_sz,
	    KM_SLEEP);

	/* Copy the contents of the ispec */
	*new_ispecp = *ispecp;

	/* Reset the intr pointer to the one just created */
	new_ispecp->is_intr = (uint32_t *)(new_ispecp + 1);

	cells_1275_copy(ispecp->is_intr, new_ispecp->is_intr,
	    BYTES_TO_1275_CELLS(ispecp->is_intr_sz));

	return (new_ispecp);
}

/*
 * Analog routine to i_ddi_alloc_ispec() used to free the interrupt
 * specification and the associated bus interrupt value.
 */
void
i_ddi_free_ispec(ddi_intrspec_t intrspecp)
{
	ddi_ispec_t *ispecp = (ddi_ispec_t *)intrspecp;

	kmem_free(ispecp, sizeof (ddi_ispec_t) + (ispecp->is_intr_sz));
}


int
i_ddi_get_nintrs(dev_info_t *dip)
{
	int32_t intrlen;
	prop_1275_cell_t intr_sz;
	prop_1275_cell_t *ip;
	int32_t ret = 0;

	if (ddi_getlongprop(DDI_DEV_T_NONE, dip, DDI_PROP_DONTPASS |
	    DDI_PROP_CANSLEEP,
	    "interrupts", (caddr_t)&ip, &intrlen) == DDI_SUCCESS) {

		intr_sz = ddi_getprop(DDI_DEV_T_ANY, dip, 0,
		    "#interrupt-cells", 1);
		/* adjust for number of bytes */
		intr_sz = CELLS_1275_TO_BYTES(intr_sz);

		ret = intrlen / intr_sz;

		kmem_free(ip, intrlen);
	}

	return (ret);
}



/*
 * i_ddi_get_intr_pri - Get the interrupt-priorities property from
 * the specified device.
 */
uint32_t
i_ddi_get_intr_pri(dev_info_t *dip, uint_t inumber)
{
	uint32_t *intr_prio_p;
	uint32_t pri = 0;
	int32_t i;

	/*
	 * Use the "interrupt-priorities" property to determine the
	 * the pil/ipl for the interrupt handler.
	 */
	if (ddi_getlongprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "interrupt-priorities", (caddr_t)&intr_prio_p,
	    &i) == DDI_SUCCESS) {
		if (inumber < (i / sizeof (int32_t)))
			pri = intr_prio_p[inumber];
		kmem_free((caddr_t)intr_prio_p, i);
	}

	return (pri);
}


/*
 * Soft interrupt priorities.  patterned after 4M.
 * The entries of this table correspond to system level PIL's.  I'd
 * prefer this table lived in the sbus nexus, but it didn't seem
 * like an easy task with the DDI soft interrupt framework.
 * XXX The 4m priorities are probably not a good choice here. (RAZ)
 */
static int soft_interrupt_priorities[] = {
	4,				/* Low soft interrupt */
	4,				/* Medium soft interrupt */
	6};				/* High soft interrupt */

/*
 * i_ddi_add_softintr - add a soft interrupt to the system
 */
int
i_ddi_add_softintr(dev_info_t *rdip, int preference, ddi_softintr_t *idp,
    ddi_iblock_cookie_t *iblock_cookiep,
    ddi_idevice_cookie_t *idevice_cookiep,
    uint_t (*int_handler)(caddr_t int_handler_arg),
    caddr_t int_handler_arg)
{
	dev_info_t *rp;
	ddi_intr_info_t *intr_info;
	ddi_softispec_t *ispecp;
	int r = DDI_SUCCESS;

	if (idp == NULL)
		return (DDI_FAILURE);
	intr_info = kmem_zalloc(sizeof (*intr_info), KM_SLEEP);
	ispecp = intr_info->ii_ispec = kmem_zalloc(sizeof (ddi_softispec_t),
	    KM_SLEEP);

	if (preference <= DDI_SOFTINT_LOW)
		ispecp->sis_pil = soft_interrupt_priorities[0];

	else if (preference > DDI_SOFTINT_LOW && preference <= DDI_SOFTINT_MED)
		ispecp->sis_pil = soft_interrupt_priorities[1];
	else
		ispecp->sis_pil = soft_interrupt_priorities[2];

	rp = ddi_root_node();

	ispecp->sis_rdip = rdip;
	intr_info->ii_iblock_cookiep = iblock_cookiep;
	intr_info->ii_idevice_cookiep = idevice_cookiep;
	intr_info->ii_int_handler = int_handler;
	intr_info->ii_int_handler_arg = int_handler_arg;
	intr_info->ii_kind = IDDI_INTR_TYPE_SOFT;

	r = (*(DEVI(rp)->devi_ops->devo_bus_ops->bus_intr_ctl))(rp, rdip,
	    DDI_INTR_CTLOPS_ADD, (void *)intr_info, (void *) NULL);

	if (r != DDI_SUCCESS) {
		kmem_free(ispecp, sizeof (*ispecp));
		kmem_free(intr_info, sizeof (*intr_info));
		return (r);
	}
	*idp = (ddi_softintr_t)intr_info;

	return (r);
}


void
i_ddi_trigger_softintr(ddi_softintr_t id)
{
	ddi_intr_info_t *intr_info = (ddi_intr_info_t *)id;
	ddi_softispec_t *ispecp = (ddi_softispec_t *)intr_info->ii_ispec;

	uint_t pri = ispecp->sis_softint_id;
	/* ICK! */
	setsoftint(pri);
}

void
i_ddi_remove_softintr(ddi_softintr_t id)
{
	ddi_intr_info_t *intr_info = (ddi_intr_info_t *)id;
	ddi_softispec_t *ispecp = (ddi_softispec_t *)intr_info->ii_ispec;
	dev_info_t *rp = ddi_root_node();
	dev_info_t *rdip = ispecp->sis_rdip;

	(*(DEVI(rp)->devi_ops->devo_bus_ops->bus_intr_ctl))(rp,
	    rdip, DDI_INTR_CTLOPS_REMOVE, (void *)intr_info, (void *)NULL);
	kmem_free(intr_info->ii_ispec, sizeof (ddi_softispec_t));
	kmem_free(intr_info, sizeof (*intr_info));
}

/*
 * process_intr_op
 * This routine is used to guarantee the call sequence of interrupt
 * operations that percolate through the device tree.  Any op must
 * first allocate an ipsec, perform the operation on the ispec, then free
 * the ispec.
 */
static int
process_intr_op(dev_info_t *dip, ddi_intr_ctlop_t op, uint_t inumber,
    ddi_intr_info_t *intr_info, ddi_intrspec_t *ispecp, void *result)
{
	int ret = DDI_SUCCESS;

	switch (op) {
	case DDI_INTR_CTLOPS_ADD:
	case DDI_INTR_CTLOPS_REMOVE:
	case DDI_INTR_CTLOPS_HILEVEL:
		if (i_ddi_intr_ctlops(dip, dip, DDI_INTR_CTLOPS_ALLOC_ISPEC,
		    (void *) inumber, (void *) ispecp) == DDI_FAILURE ||
		    *ispecp == NULL)
			return (DDI_FAILURE);

		intr_info->ii_ispec = *ispecp;
		break;

	default:
		break;
	}

	ret = i_ddi_intr_ctlops(dip, dip, op, (void *)intr_info, result);

	switch (op) {
	case DDI_INTR_CTLOPS_ADD:
	case DDI_INTR_CTLOPS_REMOVE:
	case DDI_INTR_CTLOPS_HILEVEL: {
		(void) i_ddi_intr_ctlops(dip, dip, DDI_INTR_CTLOPS_FREE_ISPEC,
		    (void *) *ispecp, (void *) NULL);
		break;
	}
	default:
		break;
	}

	return (ret);
}


int
i_ddi_intr_hilevel(dev_info_t *dip, uint_t inumber)
{
	ddi_intrspec_t ispec;
	int	r = 0;
	ddi_intr_info_t intr_info;

	intr_info.ii_kind = IDDI_INTR_TYPE_NORMAL;
	intr_info.ii_inum = inumber;

	if (process_intr_op(dip, DDI_INTR_CTLOPS_HILEVEL, inumber,
	    &intr_info, &ispec, (void *) &r) == DDI_FAILURE)
		r = 0;

	return (r);
}

int
i_ddi_add_intr(dev_info_t *dip, uint_t inumber,
    ddi_iblock_cookie_t *iblock_cookiep,
    ddi_idevice_cookie_t *idevice_cookiep,
    uint_t (*int_handler)(caddr_t int_handler_arg),
    caddr_t int_handler_arg)
{
	ddi_intrspec_t ispec;
	ddi_intr_info_t intr_info;

	intr_info.ii_iblock_cookiep = iblock_cookiep;
	intr_info.ii_idevice_cookiep = idevice_cookiep;
	intr_info.ii_int_handler = int_handler;
	intr_info.ii_int_handler_arg = int_handler_arg;
	intr_info.ii_kind = IDDI_INTR_TYPE_NORMAL;
	intr_info.ii_inum = inumber;

	return (process_intr_op(dip, DDI_INTR_CTLOPS_ADD, inumber,
	    &intr_info, &ispec, NULL));
}

int
i_ddi_add_fastintr(dev_info_t *dip, uint_t inumber,
    ddi_iblock_cookie_t *iblock_cookiep,
    ddi_idevice_cookie_t *idevice_cookiep,
    uint_t (*hi_int_handler)(void))
{
	ddi_intrspec_t ispec;
	ddi_intr_info_t intr_info;

	intr_info.ii_iblock_cookiep = iblock_cookiep;
	intr_info.ii_idevice_cookiep = idevice_cookiep;
	intr_info.ii_int_handler =
	    (uint_t (*)(caddr_t))hi_int_handler;
	intr_info.ii_int_handler_arg = NULL;
	intr_info.ii_kind = IDDI_INTR_TYPE_FAST;
	intr_info.ii_inum = inumber;

	return (process_intr_op(dip, DDI_INTR_CTLOPS_ADD, inumber,
	    &intr_info, &ispec, NULL));
}

void
i_ddi_remove_intr(dev_info_t *dip, uint_t inumber,
    ddi_iblock_cookie_t iblock_cookie)
{
	ddi_intrspec_t ispec;
	ddi_intr_info_t intr_info;

	intr_info.ii_kind = IDDI_INTR_TYPE_NORMAL;
	intr_info.ii_iblock_cookiep = &iblock_cookie;
	intr_info.ii_inum = inumber;

	(void) process_intr_op(dip, DDI_INTR_CTLOPS_REMOVE, inumber,
	    &intr_info, &ispec, (void *) NULL);
}

int
i_ddi_dev_nintrs(dev_info_t *dev, int *result)
{
	return (process_intr_op(dev, DDI_INTR_CTLOPS_NINTRS,
	    (uint_t)0, (ddi_intr_info_t *)0, (ddi_intrspec_t *)0,
	    (void *)result));
}


static uint64_t *intr_map_reg[32];
/*
 * Routines to set/get UPA slave only device interrupt mapping registers.
 * set_intr_mapping_reg() is called by the UPA master to register the address
 * of an interrupt mapping register. The upa id is that of the master. If
 * this routine is called on behalf of a slave device, the framework
 * determines the upa id of the slave based on that supplied by the master.
 *
 * get_intr_mapping_reg() is called by the UPA nexus driver on behalf
 * of a child device to get and program the interrupt mapping register of
 * one of it's child nodes.  It uses the upa id of the child device to
 * index into a table of mapping registers.  If the routine is called on
 * behalf of a slave device and the mapping register has not been set,
 * the framework determines the devinfo node of the corresponding master
 * nexus which owns the mapping register of the slave and installs that
 * driver.  The device driver which owns the mapping register must call
 * set_intr_mapping_reg() in its attach routine to register the slaves
 * mapping register with the system.
 */
void
set_intr_mapping_reg(int upaid, uint64_t *addr, int slave)
{
	int affin_upaid;

	/* For UPA master devices, set the mapping reg addr and we're done */
	if (slave == 0) {
		intr_map_reg[upaid] = addr;
		return;
	}

	/*
	 * If we get here, we're adding an entry for a UPA slave only device.
	 * The UPA id of the device which has affinity with that requesting,
	 * will be the device with the same UPA id minus the slave number.
	 */
	affin_upaid = upaid - slave;

	/*
	 * Load the address of the mapping register in the correct slot
	 * for the slave device.
	 */
	intr_map_reg[affin_upaid] = addr;
}

uint64_t *
get_intr_mapping_reg(int upaid, int slave)
{
	int affin_upaid;
	dev_info_t *affin_dip;
	uint64_t *addr = intr_map_reg[upaid];

	/* If we're a UPA master, or we have a valid mapping register. */
	if (!slave || addr != NULL)
		return (addr);

	/*
	 * We only get here if we're a UPA slave only device whose interrupt
	 * mapping register has not been set.
	 * We need to try and install the nexus whose physical address
	 * space is where the slaves mapping register resides.  They
	 * should call set_intr_mapping_reg() in their xxattach() to register
	 * the mapping register with the system.
	 */

	/*
	 * We don't know if a single- or multi-interrupt proxy is fielding
	 * our UPA slave interrupt, we must check both cases.
	 * Start out by assuming the multi-interrupt case.
	 * We assume that single- and multi- interrupters are not
	 * overlapping in UPA portid space.
	 */

	affin_upaid = upaid | 3;

	/*
	 * We start looking for the multi-interrupter affinity node.
	 * We know it's ONLY a child of the root node since the root
	 * node defines UPA space.
	 */
	for (affin_dip = ddi_get_child(ddi_root_node()); affin_dip;
	    affin_dip = ddi_get_next_sibling(affin_dip))
		if (ddi_prop_get_int(DDI_DEV_T_ANY, affin_dip,
		    DDI_PROP_DONTPASS, "upa-portid", -1) == affin_upaid)
			break;

	if (affin_dip) {
		if (ddi_install_driver(ddi_get_name(affin_dip))
		    == DDI_SUCCESS) {
			/* try again to get the mapping register. */
			addr = intr_map_reg[upaid];
		}
	}

	/*
	 * If we still don't have a mapping register try single -interrupter
	 * case.
	 */
	if (addr == NULL) {

		affin_upaid = upaid | 1;

		for (affin_dip = ddi_get_child(ddi_root_node()); affin_dip;
		    affin_dip = ddi_get_next_sibling(affin_dip))
			if (ddi_prop_get_int(DDI_DEV_T_ANY, affin_dip,
			    DDI_PROP_DONTPASS, "upa-portid", -1) == affin_upaid)
				break;

		if (affin_dip) {
			if (ddi_install_driver(ddi_get_name(affin_dip))
			    == DDI_SUCCESS) {
				/* try again to get the mapping register. */
				addr = intr_map_reg[upaid];
			}
		}
	}
	return (addr);
}

/*ARGSUSED*/
uint_t
softlevel1(caddr_t arg)
{
	extern int siron_pending;

	siron_pending = 0;
	softint();
	return (1);
}

/*
 * indirection table, to save us some large switch statements
 * NOTE: This must agree with "INTLEVEL_foo" constants in
 *	<sys/avintr.h>
 */
struct autovec *const vectorlist[] = { 0 };

/*
 * This value is exported here for the functions in avintr.c
 */
const uint_t maxautovec = (sizeof (vectorlist) / sizeof (vectorlist[0]));

/*
 * Check for machine specific interrupt levels which cannot be reasigned by
 * settrap(), sun4u version.
 *
 * sun4u does not support V8 SPARC "fast trap" handlers.
 */
/*ARGSUSED*/
int
exclude_settrap(int lvl)
{
	return (1);
}

/*
 * Check for machine specific interrupt levels which cannot have interrupt
 * handlers added. We allow levels 1 thru 15; level 0 is nonsense.
 */
/*ARGSUSED*/
int
exclude_level(int lvl)
{
	return ((lvl < 1) || (lvl > 15));
}


/*
 * SECTION: DDI Memory/DMA
 */

static vmem_t *little_endian_arena;

static void *
segkmem_alloc_le(vmem_t *vmp, size_t size, int flag)
{
	return (segkmem_xalloc(vmp, NULL, size, flag, HAT_STRUCTURE_LE,
	    segkmem_page_create, NULL));
}

void
ka_init(void)
{
	little_endian_arena = vmem_create("little_endian", NULL, 0, 1,
	    segkmem_alloc_le, segkmem_free, heap_arena, 0, VM_SLEEP);
}

/*
 * Allocate from the system, aligned on a specific boundary.
 * The alignment, if non-zero, must be a power of 2.
 */
static void *
kalloca(size_t size, size_t align, int cansleep, uint_t endian_flags)
{
	size_t *addr, *raddr, rsize;
	size_t hdrsize = 4 * sizeof (size_t);	/* must be power of 2 */

	align = MAX(align, hdrsize);
	ASSERT((align & (align - 1)) == 0);
	rsize = P2ROUNDUP(size, align) + align + hdrsize;

	if (endian_flags == DDI_STRUCTURE_LE_ACC)
		raddr = vmem_alloc(little_endian_arena, rsize,
		    cansleep ? VM_SLEEP : VM_NOSLEEP);
	else
		raddr = kmem_alloc(rsize, cansleep ? KM_SLEEP : KM_NOSLEEP);
	if (raddr == NULL)
		return (NULL);
	addr = (size_t *)P2ROUNDUP((uintptr_t)raddr + hdrsize, align);
	addr[-3] = (size_t)endian_flags;
	addr[-2] = (size_t)raddr;
	addr[-1] = rsize;

	return (addr);
}

static void
kfreea(void *addr)
{
	size_t *saddr = addr;

	if (saddr[-3] == DDI_STRUCTURE_LE_ACC)
		vmem_free(little_endian_arena, (void *)saddr[-2], saddr[-1]);
	else
		kmem_free((void *)saddr[-2], saddr[-1]);
}

int
i_ddi_mem_alloc(dev_info_t *dip, ddi_dma_attr_t *attr,
    size_t length, int cansleep, int streaming,
    ddi_device_acc_attr_t *accattrp,
    caddr_t *kaddrp, size_t *real_length, ddi_acc_hdl_t *handlep)
{
	caddr_t a;
	int iomin, align;
	uint_t endian_flags = DDI_NEVERSWAP_ACC;

#if defined(lint)
	*handlep = *handlep;
#endif

	/*
	 * Check legality of arguments
	 */
	if (length == 0 || kaddrp == NULL || attr == NULL) {
		return (DDI_FAILURE);
	}
	if (attr->dma_attr_minxfer == 0 || attr->dma_attr_align == 0 ||
	    (attr->dma_attr_align & (attr->dma_attr_align - 1)) ||
	    (attr->dma_attr_minxfer & (attr->dma_attr_minxfer - 1))) {
		return (DDI_FAILURE);
	}

	/*
	 * Drivers for 64-bit capable SBus devices will encode
	 * the burtsizes for 64-bit xfers in the upper 16-bits.
	 * For DMA alignment, we use the most restrictive
	 * alignment of 32-bit and 64-bit xfers.
	 */
	iomin = (attr->dma_attr_burstsizes & 0xffff) |
	    ((attr->dma_attr_burstsizes >> 16) & 0xffff);
	/*
	 * If a driver set burtsizes to 0, we give him byte alignment.
	 * Otherwise align at the burtsizes boundary.
	 */
	if (iomin == 0)
		iomin = 1;
	else
		iomin = 1 << (ddi_fls(iomin) - 1);
	iomin = maxbit(iomin, attr->dma_attr_minxfer);
	iomin = maxbit(iomin, attr->dma_attr_align);
	iomin = ddi_iomin(dip, iomin, streaming);
	if (iomin == 0)
		return (DDI_FAILURE);

	ASSERT((iomin & (iomin - 1)) == 0);
	ASSERT(iomin >= attr->dma_attr_minxfer);
	ASSERT(iomin >= attr->dma_attr_align);

	length = P2ROUNDUP(length, iomin);
	align = iomin;

	if (accattrp != NULL)
		endian_flags = accattrp->devacc_attr_endian_flags;
	a = kalloca(length, align, cansleep, endian_flags);
	if ((*kaddrp = a) == 0) {
		return (DDI_FAILURE);
	} else {
		if (real_length) {
			*real_length = length;
		}
		if (handlep) {
			/*
			 * assign handle information
			 */
			impl_acc_hdl_init(handlep);
		}
		return (DDI_SUCCESS);
	}
}

/*
 * covert old DMA limits structure to DMA attribute structure
 * and continue
 */
int
i_ddi_mem_alloc_lim(dev_info_t *dip, ddi_dma_lim_t *limits,
    uint_t length, int cansleep, int streaming,
    ddi_device_acc_attr_t *accattrp, caddr_t *kaddrp,
    uint_t *real_length, ddi_acc_hdl_t *ap)
{
	ddi_dma_attr_t dma_attr, *attrp;
	size_t rlen;
	int ret;

	ASSERT(limits);
	attrp = &dma_attr;
	attrp->dma_attr_version = DMA_ATTR_V0;
	attrp->dma_attr_addr_lo = (uint64_t)limits->dlim_addr_lo;
	attrp->dma_attr_addr_hi = (uint64_t)limits->dlim_addr_hi;
	attrp->dma_attr_count_max = (uint64_t)-1;
	attrp->dma_attr_align = 1;
	attrp->dma_attr_burstsizes = (uint_t)limits->dlim_burstsizes;
	attrp->dma_attr_minxfer = (uint32_t)limits->dlim_minxfer;
	attrp->dma_attr_maxxfer = (uint64_t)-1;
	attrp->dma_attr_seg = (uint64_t)limits->dlim_cntr_max;
	attrp->dma_attr_sgllen = 1;
	attrp->dma_attr_granular = 1;
	attrp->dma_attr_flags = 0;

	ret = i_ddi_mem_alloc(dip, attrp, (size_t)length, cansleep, streaming,
	    accattrp, kaddrp, &rlen, ap);
	if (ret == DDI_SUCCESS) {
		if (real_length)
			*real_length = (uint_t)rlen;
	}
	return (ret);
}

/* ARGSUSED */
void
i_ddi_mem_free(caddr_t kaddr, int stream)
{
	kfreea(kaddr);
}

static struct upa_dma_pfns {
	uint_t hipfn;
	uint_t lopfn;
} upa_dma_pfn_array[MAX_UPA];

static int upa_dma_pfn_ndx = 0;

/*
 * Certain UPA busses cannot accept dma transactions from any other source
 * except for memory due to livelock conditions in their hardware. (e.g. sbus
 * and PCI). These routines allow devices or busses on the UPA to register
 * a physical address block within it's own register space where DMA can be
 * performed.  Currently, the FFB is the only such device which supports
 * device DMA on the UPA.
 */
void
pf_set_dmacapable(uint_t hipfn, uint_t lopfn)
{
	int i = upa_dma_pfn_ndx;

	upa_dma_pfn_ndx++;

	upa_dma_pfn_array[i].hipfn = hipfn;
	upa_dma_pfn_array[i].lopfn = lopfn;
}

void
pf_unset_dmacapable(uint_t pfn)
{
	int i;

	for (i = 0; i < upa_dma_pfn_ndx; i++) {
		if (pfn <= upa_dma_pfn_array[i].hipfn &&
		    pfn >= upa_dma_pfn_array[i].lopfn) {
			upa_dma_pfn_array[i].hipfn =
			    upa_dma_pfn_array[upa_dma_pfn_ndx - 1].hipfn;
			upa_dma_pfn_array[i].lopfn =
			    upa_dma_pfn_array[upa_dma_pfn_ndx - 1].lopfn;
			upa_dma_pfn_ndx--;
			break;
		}
	}
}

/*
 * This routine should only be called using a pfn that is known to reside
 * in IO space.  The function pf_is_memory() can be used to determine this.
 */
int
pf_is_dmacapable(pfn_t pfn)
{
	int i, j;

	/* If the caller passed in a memory pfn, return true. */
	if (pf_is_memory(pfn))
		return (1);

	for (i = upa_dma_pfn_ndx, j = 0; j < i; j++)
		if (pfn <= upa_dma_pfn_array[j].hipfn &&
		    pfn >= upa_dma_pfn_array[j].lopfn)
			return (1);

	return (0);
}


/*
 * SECTION: DDI Data Access
 */

static uintptr_t impl_acc_hdl_id = 0;

/*
 * access handle allocator
 */
ddi_acc_hdl_t *
impl_acc_hdl_get(ddi_acc_handle_t hdl)
{
	/*
	 * Extract the access handle address from the DDI implemented
	 * access handle
	 */
	return (&((ddi_acc_impl_t *)hdl)->ahi_common);
}

ddi_acc_handle_t
impl_acc_hdl_alloc(int (*waitfp)(caddr_t), caddr_t arg)
{
	ddi_acc_impl_t *hp;
	int sleepflag;

	sleepflag = ((waitfp == (int (*)())KM_SLEEP) ? KM_SLEEP : KM_NOSLEEP);

	/*
	 * Allocate and initialize the data access handle.
	 */
	hp = kmem_zalloc(sizeof (ddi_acc_impl_t), sleepflag);
	if (!hp) {
		if ((waitfp != (int (*)())KM_SLEEP) &&
		    (waitfp != (int (*)())KM_NOSLEEP))
			ddi_set_callback(waitfp, arg, &impl_acc_hdl_id);
		return (NULL);
	}
	hp->ahi_common.ah_platform_private = (void *)hp;
	return ((ddi_acc_handle_t)hp);
}

void
impl_acc_hdl_free(ddi_acc_handle_t handle)
{
	ddi_acc_impl_t *hp;

	/*
	 * The supplied (ddi_acc_handle_t) is actually a (ddi_acc_impl_t *),
	 * because that's what we allocated in impl_acc_hdl_alloc() above.
	 */
	hp = (ddi_acc_impl_t *)handle;
	if (hp) {
		kmem_free(hp, sizeof (*hp));
		if (impl_acc_hdl_id)
			ddi_run_callback(&impl_acc_hdl_id);
	}
}

void
impl_acc_hdl_init(ddi_acc_hdl_t *handlep)
{
	ddi_acc_impl_t *hp;

	ASSERT(handlep);

	hp = (ddi_acc_impl_t *)handlep;

	/*
	 * check for SW byte-swapping
	 */
	hp->ahi_get8 = i_ddi_get8;
	hp->ahi_put8 = i_ddi_put8;
	hp->ahi_rep_get8 = i_ddi_rep_get8;
	hp->ahi_rep_put8 = i_ddi_rep_put8;
	if (handlep->ah_acc.devacc_attr_endian_flags & DDI_STRUCTURE_LE_ACC) {
		hp->ahi_get16 = i_ddi_swap_get16;
		hp->ahi_get32 = i_ddi_swap_get32;
		hp->ahi_get64 = i_ddi_swap_get64;
		hp->ahi_put16 = i_ddi_swap_put16;
		hp->ahi_put32 = i_ddi_swap_put32;
		hp->ahi_put64 = i_ddi_swap_put64;
		hp->ahi_rep_get16 = i_ddi_swap_rep_get16;
		hp->ahi_rep_get32 = i_ddi_swap_rep_get32;
		hp->ahi_rep_get64 = i_ddi_swap_rep_get64;
		hp->ahi_rep_put16 = i_ddi_swap_rep_put16;
		hp->ahi_rep_put32 = i_ddi_swap_rep_put32;
		hp->ahi_rep_put64 = i_ddi_swap_rep_put64;
	} else {
		hp->ahi_get16 = i_ddi_get16;
		hp->ahi_get32 = i_ddi_get32;
		hp->ahi_get64 = i_ddi_get64;
		hp->ahi_put16 = i_ddi_put16;
		hp->ahi_put32 = i_ddi_put32;
		hp->ahi_put64 = i_ddi_put64;
		hp->ahi_rep_get16 = i_ddi_rep_get16;
		hp->ahi_rep_get32 = i_ddi_rep_get32;
		hp->ahi_rep_get64 = i_ddi_rep_get64;
		hp->ahi_rep_put16 = i_ddi_rep_put16;
		hp->ahi_rep_put32 = i_ddi_rep_put32;
		hp->ahi_rep_put64 = i_ddi_rep_put64;
	}

	hp->ahi_fault_check = i_ddi_acc_fault_check;
	hp->ahi_fault_notify = i_ddi_acc_fault_notify;
}	

void
i_ddi_acc_set_fault(ddi_acc_handle_t handle)
{
	ddi_acc_impl_t *hp = (ddi_acc_impl_t *)handle;

	if (!hp->ahi_fault) {
		hp->ahi_fault = 1;
		(*hp->ahi_fault_notify)(hp);
	}
}

void
i_ddi_acc_clr_fault(ddi_acc_handle_t handle)
{
	ddi_acc_impl_t *hp = (ddi_acc_impl_t *)handle;

	if (hp->ahi_fault) {
		hp->ahi_fault = 0;
		(*hp->ahi_fault_notify)(hp);
	}
}

/* ARGSUSED */
void
i_ddi_acc_fault_notify(ddi_acc_impl_t *hp)
{
	/* Default version, does nothing */
}


#ifdef	DEBUG
int	peekfault_cnt = 0;
int	pokefault_cnt = 0;
#endif	/* DEBUG */

/*ARGSUSED*/
int
i_ddi_peek(dev_info_t *devi, size_t size, void *addr, void *value_p)
{
	int err = DDI_SUCCESS;
	longlong_t trash;
	ddi_nofault_data_t nofault_data;
	void *saved_nofault;
	extern void membar_sync();
	extern int peek_fault();
	extern int do_peek(size_t, void *, void *);

	/*
	 * arrange that peeking to a nil destination pointer silently succeeds
	 */
	if (value_p == (void *)0)
		value_p = &trash;

	saved_nofault = curthread->t_nofault;

	/* Set up the nofault data */
	curthread->t_nofault = &nofault_data;
	nofault_data.op_type = PEEK_START;
	nofault_data.pfn = hat_getkpfnum(addr);
	nofault_data.pc = (caddr_t)peek_fault;

	/* Make sure all the nofault_data is set before we peek */
	membar_sync();

	err = do_peek(size, addr, value_p);

#ifdef	DEBUG
	if (err == DDI_FAILURE)
		peekfault_cnt++;
#endif

	curthread->t_nofault = saved_nofault;
	return (err);
}

int
i_ddi_poke(dev_info_t *devi, size_t size, void *addr, void *value_p)
{
	void *saved_nofault;
	ddi_nofault_data_t nofault_data;
	int err = DDI_SUCCESS;

	extern void membar_sync();
	extern int poke_fault();
	extern int do_poke(size_t, void *, void *);

	saved_nofault = curthread->t_nofault;

	curthread->t_nofault = &nofault_data;
	/* Set up the nofault data */
	nofault_data.op_type = POKE_START;
	nofault_data.pfn = hat_getkpfnum(addr);
	ASSERT(nofault_data.pfn != (unsigned int) -1);
	nofault_data.pc = (caddr_t)poke_fault;

	/*
	 * Inform our parent nexi what we're about to do, giving them
	 * an early opportunity to tell us not to even try.
	 */
	if (devi && ddi_ctlops(devi, devi, DDI_CTLOPS_POKE_INIT,
	    (void *)&nofault_data, (void *)0) != DDI_SUCCESS) {
		curthread->t_nofault = saved_nofault;
		return (DDI_FAILURE);
	}

	/* Make sure all the nofault_data is set before we poke */
	membar_sync();

	err = do_poke(size, addr, value_p);

	/*
	 * Now give our parent(s) a chance to ensure that what we
	 * did really propagated through any intermediate buffers,
	 * returning failure if we detected any problems
	 */
	if (devi && (ddi_ctlops(devi, devi, DDI_CTLOPS_POKE_FLUSH,
	    (void *)&nofault_data, (void *)0) == DDI_FAILURE))
		err = DDI_FAILURE;

	/*
	 * Give our parents a chance to tidy up after us.  If
	 * 'tidying up' causes faults, we crash and burn.
	 */
	if (devi)
		(void) ddi_ctlops(devi, devi, DDI_CTLOPS_POKE_FINI,
		    (void *)&nofault_data, (void *)0);

#ifdef	DEBUG
	if (err == DDI_FAILURE)
		pokefault_cnt++;
#endif

	curthread->t_nofault = saved_nofault;

	return (err);
}


/*
 * SECTION: Misc functions
 */

/*
 * instance wrappers
 */
/*ARGSUSED*/
uint_t
impl_assign_instance(dev_info_t *dip)
{
	return ((uint_t)-1);
}

/*ARGSUSED*/
int
impl_keep_instance(dev_info_t *dip)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
int
impl_free_instance(dev_info_t *dip)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
int
impl_check_cpu(dev_info_t *devi)
{
	return (DDI_SUCCESS);
}

dev_info_t *
e_ddi_nodeid_to_dip(dev_info_t *dip, uint_t nodeid)
{
	dev_info_t *result;

	if (!dip)
		return (NULL);

	if (ddi_get_nodeid(dip) == nodeid)
		return (dip);

	if (result = e_ddi_nodeid_to_dip(ddi_get_child(dip), nodeid))
		return (result);
	else
		return (e_ddi_nodeid_to_dip(ddi_get_next_sibling(dip),
		    nodeid));
}
