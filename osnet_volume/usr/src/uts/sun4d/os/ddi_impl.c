/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ddi_impl.c	1.74	99/10/22 SMI"

/*
 * Sun4d-specific DDI implementation
 */
#include <sys/types.h>
#include <sys/autoconf.h>
#include <sys/avintr.h>
#include <sys/bootconf.h>
#include <sys/conf.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
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
#include <sys/syserr.h>
#include <sys/sysmacros.h>
#include <sys/systeminfo.h>
#include <sys/fpu/fpusystm.h>

/*
 * External Functions (should be in header files)
 */
extern int mem_unit_good(char *status);
extern void halt(char *);	/* fix syserr.h - should be void */

/*
 * DDI Boot Configuration
 */

/*
 * Favoured drivers of this implementation
 * architecture.  These drivers MUST be present for
 * the system to boot at all.
 */
char *impl_module_list[] = {
	"rootnex",
	"options",
	"dma",
	"sad",
	(char *)0
};

/*
 * No platform pm drivers on this platform
 */
char *platform_pm_module_list[] = {
	(char *)0
};

/*
 * List of drivers to do multi-threaded probe/attach
 */
char *default_mta_drivers = "sd st ssd ses";

#if !defined(SAS) && !defined(MPSAS)

/*
 * Forward declarations
 */
static uint_t softlevel1(caddr_t);
static int get_boardnum(int, dev_info_t *);

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
idprom_t idprom;

int sun4d_model = MODEL_UNKNOWN;

#define	NAME_SC2000 "SUNW,SPARCcenter-2000"
#define	NAME_SC1000 "SUNW,SPARCserver-1000"
#define	BUFLEN 24

/*
 * The Presto driver relies on n_xdbus.
 */
uint_t n_xdbus;				/* number XDbuses on system */
int good_xdbus = -1;

#endif	/* !SAS && !MPSAS */

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
	 * Determine if an FPU is attached
	 */

#ifndef	MPSAS	/* no fpu module yet in MPSAS */
	fpu_probe();
#endif
	if (!fpu_exists) {
		printf("No FPU in configuration\n");
	}

	/*
	 * This following line fixes bugid 1041296; we need to do a
	 * prom_nextnode(0) because this call ALSO patches the DMA+
	 * bug in Campus-B and Phoenix. The prom uncaches the traptable
	 * page as a side-effect of devr_next(0) (which prom_nextnode calls),
	 * so this *must* be executed early on.
	 */
	(void) prom_nextnode((dnode_t)0);

	/*
	 * Initialize devices on the machine.
	 * Uses configuration tree built by the PROMs to determine what
	 * is present, and builds a tree of prototype dev_info nodes
	 * corresponding to the hardware which identified itself.
	 */
#if !defined(SAS) && !defined(MPSAS)
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

#endif	/* !SAS && !MPSAS */
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

	/*
	 * set global n_xdbus from root node property
	 */
	if (-1 == prom_getprop((dnode_t)0, "n-xdbus", (caddr_t)&n_xdbus)) {
		n_xdbus = 0;
		sun4d_model = MODEL_UNKNOWN;
	} else if (n_xdbus == 2) {
		sun4d_model = MODEL_SC2000;
	} else if (n_xdbus == 1) {

		char name[BUFLEN];

		if (-1 == prom_getprop((dnode_t)0, "name", name)) {
			sun4d_model = MODEL_UNKNOWN;
			return;
		}
		name[BUFLEN -1] = '\0';

		if (strncmp(name, NAME_SC1000, BUFLEN) == 0) {
			sun4d_model = MODEL_SC1000;
		} else if (strncmp(name, NAME_SC2000, BUFLEN) == 0) {

			int badbus;
			sun4d_model = MODEL_SC2000;

			if (-1 == prom_getprop((dnode_t)0, "disabled-xdbus",
			    (caddr_t)&badbus))
				good_xdbus = -1;
			else if (badbus == 0)
				good_xdbus = 1;
			else if (badbus == 1)
				good_xdbus = 0;
		} else
			halt(name);
	}
}

/*
 *  Here is where we actually infer meanings to the members of idprom_t
 */
void
parse_idprom(void)
{
	if (idprom.id_format == IDFORM_1) {
		register int i;

		(void) localetheraddr((struct ether_addr *)idprom.id_ether,
		    (struct ether_addr *)NULL);

		i = idprom.id_machine << 24;
		i = i + idprom.id_serial;
		numtos((ulong_t)i, hw_serial);
	} else
		prom_printf("Invalid format code in IDprom.\n");
}

/*
 * Check the status property of the device node passed as an argument.
 *
 *	if (status property does not begin with "fail" or does not exist)
 *		return DDI_SUCCESS
 *	else
 *		print a warning
 *
 *	if (good mem-unit)
 *		return DDI_SUCCESS
 *	else
 *		return DDI_FAILURE
 */
int
check_status(int id, char *name, dev_info_t *parent)
{
	char	status_buf[OBP_MAXPATHLEN];
	char	path[OBP_MAXPATHLEN];
	int boardnum;
	int retval = DDI_FAILURE;
	char *partially = "";
	char board_buf[24];
	int proplen;
	static const char *status = "status";
	static const char *fail = "fail";
	int fail_len = (int)strlen(fail);
	int size;

	/*
	 * If the property doesn't exist, or does not begin with
	 * the characters "fail", then it's not a failed node.
	 * This is done for plug-in cards that might have a 'status'
	 * property of their own, that may not even be a char string.
	 */
	proplen = prom_getproplen((dnode_t)id, (caddr_t)status);
	if (proplen <= fail_len)
		return (DDI_SUCCESS);


	size = prom_getproplen((dnode_t)id, (char *)status);
	if (size <= 0 || (size > OBP_MAXPATHLEN - 1))
		return (DDI_SUCCESS);

	if (-1 == prom_getprop((dnode_t)id, (char *)status, status_buf))
		return (DDI_SUCCESS);

	if (strncmp(status_buf, fail, fail_len) != 0)
		return (DDI_SUCCESS);

	/*
	 * otherwise we print a warning
	 */

	/*
	 * get board number
	 */
	board_buf[0] = (char)0;
	boardnum = get_boardnum(id, parent);
	if (boardnum != -1)
		(void) sprintf(board_buf, " on board %d", boardnum);

	/*
	 * is this a 'memory' node with a bad simm, perhaps?
	 */
	if (strncmp(name, "mem-unit", 8) == 0 &&
	    mem_unit_good(status_buf)) {
		retval = DDI_SUCCESS;
		partially = " partially";
	}

	path[0] = (char)0;
	prom_dnode_to_pathname((dnode_t)id, path);

	cmn_err(CE_WARN, "device '%s'%s%s unavailable.\n\tstatus: '%s'",
		path, board_buf, partially, status_buf);

	return (retval);
}

static int
get_boardnum(int nid, dev_info_t *par)
{
	int board_num;

	if (-1 != prom_getprop((dnode_t)nid, OBP_BOARDNUM,
	    (char *)&board_num))
		return (board_num);

	/*
	 * Look at current node and up the parent chain
	 * till we find a node with an OBP_BOARDNUM.
	 */
	while (par) {

		nid = ddi_get_nodeid(par);

		if (-1 != prom_getprop((dnode_t)nid, OBP_BOARDNUM,
		    (char *)&board_num))
			return (board_num);

		par = ddi_get_parent(par);
	}
	return (-1);
}


/*
 * SECTION: DDI Properties
 */

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
get_prop_int_array(dev_info_t *di, char *pname, uint_t flags,
    int **pval, uint_t *plen)
{
	int ret;

	if ((ret = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, di,
	    flags | DDI_PROP_DONTPASS, pname, pval, plen))
	    == DDI_PROP_SUCCESS) {
		*plen = (*plen) * (sizeof (int));
	}
	return (ret);
}


/*
 * SECTION: DDI Node Configuration
 */

struct prop_ispec {
	uint_t	pri, vec;
};

/*
 * Create a ddi_parent_private_data structure from the ddi properties of
 * the dev_info node.
 *
 * The "reg" and either an "intr" or "interrupts" properties are required
 * if the driver wishes to create mappings or field interrupts on behalf
 * of the device.
 *
 * The "reg" property is assumed to be a list of at least one triple
 *
 *	<bustype, address, size>*1
 *
 * The "intr" property is assumed to be a list of at least one duple
 *
 *	<SPARC ipl, vector#>*1
 *
 * The "interrupts" property is assumed to be a list of at least one
 * n-tuples that describes the interrupt capabilities of the bus the device
 * is connected to.  For SBus, this looks like
 *
 *	<SBus-level>*1
 *
 * (This property obsoletes the 'intr' property).
 *
 * The OBP_RANGES property is optional.
 */
static int
make_ddi_ppd(dev_info_t *child, struct ddi_parent_private_data **ppd,
    int use_registers)
{
	register struct ddi_parent_private_data *pdptr;
	register int n;
	int *reg_prop, *rgstr_prop, *rng_prop, *intr_prop, *irupts_prop;
	uint_t reg_len, rgstr_len, rng_len, intr_len, irupts_len;
	int has_registers = 0;

	*ppd = pdptr = (struct ddi_parent_private_data *)
			kmem_zalloc(sizeof (*pdptr), KM_SLEEP);

	/*
	 * Handle the 'reg'/'registers' properties.
	 * "registers" overrides "reg", but requires that "reg" be exported,
	 * so we can handle wildcard specifiers.  "registers" implies an
	 * sbus style device.  "registers" implies that we insert the
	 * correct value in the regspec_bustype field of each spec for a real
	 * (non-pseudo) device node.  "registers" is a s/w only property, so
	 * we inhibit the prom search for this property.
	 */

	if (get_prop_int_array(child, OBP_REG, 0, &reg_prop, &reg_len)
	    != DDI_PROP_SUCCESS) {
		reg_len = 0;
	}

	rgstr_len = 0;
	if (use_registers)
		(void) get_prop_int_array(child, "registers", DDI_PROP_NOTPROM,
		    &rgstr_prop, &rgstr_len);

	if (rgstr_len != 0)  {

		if (ndi_dev_is_persistent_node(child) && (reg_len != 0))  {

			/*
			 * Convert wildcard "registers" for a real node...
			 * (Else, this is the wildcard prototype node)
			 */
			struct regspec *rp = (struct regspec *)reg_prop;
			uint_t slot = rp->regspec_bustype;
			int i;

			rp = (struct regspec *)rgstr_prop;
			n = rgstr_len / sizeof (struct regspec);
			for (i = 0; i < n; ++i, ++rp)
				rp->regspec_bustype = slot;
		}

		if (reg_len != 0)
			ddi_prop_free((void *)reg_prop);

		reg_prop = rgstr_prop;
		reg_len = rgstr_len;
		++has_registers;
	}

	if ((n = reg_len) != 0)  {
		pdptr->par_nreg = n / (int)sizeof (struct regspec);
		pdptr->par_reg = (struct regspec *)reg_prop;
	}

	/*
	 * See if I have ranges.
	 */
	if (get_prop_int_array(child, OBP_RANGES, 0, &rng_prop,
	    &rng_len) == DDI_PROP_SUCCESS) {
		pdptr->par_nrng = rng_len / (int)(sizeof (struct rangespec));
		pdptr->par_rng = (struct rangespec *)rng_prop;
	}

	/*
	 * Handle the 'intr' and 'interrupts' properties
	 */

	/*
	 * For backwards compatibility with the zillion old SBus cards in
	 * the world, we first look for the 'intr' property for the device.
	 */
	if (get_prop_int_array(child, OBP_INTR, 0, &intr_prop,
	    &intr_len) != DDI_PROP_SUCCESS) {
		intr_len = 0;
	}

	/*
	 * If we're to support bus adapters and future platforms cleanly,
	 * we need to support the generalized 'interrupts' property.
	 */
	if (get_prop_int_array(child, OBP_INTERRUPTS, 0, &irupts_prop,
	    &irupts_len) != DDI_PROP_SUCCESS) {
		irupts_len = 0;
	} else if (intr_len != 0) {
		/*
		 * If both 'intr' and 'interrupts' are defined,
		 * then 'interrupts' wins and we toss the 'intr' away.
		 */
		ddi_prop_free((void *)intr_prop);
		intr_len = 0;
	}

	if (intr_len != 0) {

		/*
		 * Translate the 'intr' property into an array
		 * an array of struct intrspec's.  There's not really
		 * very much to do here except copy what's out there.
		 */

		struct intrspec *new;
		struct prop_ispec *l;

		n = pdptr->par_nintr =
			intr_len / sizeof (struct prop_ispec);
		l = (struct prop_ispec *)intr_prop;
		new = pdptr->par_intr = (struct intrspec *)
		    kmem_zalloc(n * sizeof (struct intrspec), KM_SLEEP);
		while (n--) {
			new->intrspec_pri = l->pri;
			new->intrspec_vec = l->vec;
			new++;
			l++;
		}
		ddi_prop_free((void *)intr_prop);

	} else if ((n = irupts_len) != 0) {
		size_t size;
		int *out;

		/*
		 * Translate the 'interrupts' property into an array
		 * of intrspecs for the rest of the DDI framework to
		 * toy with.  Only our ancestors really know how to
		 * do this, so ask 'em.  We massage the 'interrupts'
		 * property so that it is pre-pended by a count of
		 * the number of integers in the argument.
		 */
		size = sizeof (int) + n;
		out = kmem_alloc(size, KM_SLEEP);
		*out = n / sizeof (int);
		bcopy((caddr_t)irupts_prop, (caddr_t)(out + 1), (size_t)n);
		ddi_prop_free((void *)irupts_prop);
		if (ddi_ctlops(child, child, DDI_CTLOPS_XLATE_INTRS,
		    out, pdptr) != DDI_SUCCESS) {
			cmn_err(CE_CONT,
			    "Unable to translate 'interrupts' for %s%d\n",
			    DEVI(child)->devi_binding_name,
			    DEVI(child)->devi_instance);
		}
		kmem_free(out, size);
	}
	return (has_registers);
}


/*
 * Called from the bus_ctl op of sunbus (sbus, obio, etc) nexus drivers
 * to implement the DDI_CTLOPS_INITCHILD operation.  That is, it names
 * the children of sun busses based on the reg spec.
 *
 * Handles the following properties:
 *
 *	Property		value
 *	  Name			type
 *
 *	reg		register spec
 *	registers	wildcard s/w sbus register spec (.conf file property)
 *	intr		old-form interrupt spec
 *	interrupts	new (bus-oriented) interrupt spec
 *	ranges		range spec
 */

static char *cantmerge = "Cannot merge %s devinfo node %s@%s";

static int
i_impl_ddi_sunbus_initchild(dev_info_t *child, int use_registers)
{
	struct ddi_parent_private_data *pdptr;
	char name[MAXNAMELEN];
	int has_registers;
	dev_info_t *parent, *och;
	void impl_ddi_sunbus_removechild(dev_info_t *);

	/*
	 * Fill in parent-private data and this function returns to us
	 * an indication if it used "registers" to fill in the data.
	 */
	has_registers = make_ddi_ppd(child, &pdptr, use_registers);
	ddi_set_parent_data(child, (caddr_t)pdptr);
	parent = ddi_get_parent(child);

	/*
	 * If this is a s/w node defined with the "registers" property,
	 * this means that this is an "sbus" style device and that this
	 * is a wildcard specifier, whose properties get applied to all
	 * previously defined h/w nodes with the same name and same parent.
	 *
	 * XXX: This functionality is "sbus" class nexus specific...
	 * XXX: and should be a function of that nexus driver only!
	 */

	if ((has_registers) && (ndi_dev_is_persistent_node(child) == 0)) {

		major_t major;
		int need_init;

		/*
		 * Find all previously defined h/w children with the same name
		 * and same parent and copy the property lists from the
		 * prototype node into the h/w nodes and re-inititialize them.
		 */

		if ((major = ddi_name_to_major(ddi_get_name(child))) == -1)  {
			impl_ddi_sunbus_removechild(child);
			return (DDI_NOT_WELL_FORMED);
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

			/*
			 * If the node we're merging into is already init-ed
			 * (CF1), we need to un-init it and later, re-init it.
			 */
			need_init = DDI_CF1(och) ? 1 : 0;
			if (need_init)
				(void) ddi_uninitchild(och);

			mutex_enter(&(DEVI(child)->devi_lock));
			mutex_enter(&(DEVI(och)->devi_lock));
			copy_prop(DEVI(child)->devi_drv_prop_ptr,
			    &(DEVI(och)->devi_drv_prop_ptr));
			copy_prop(DEVI(child)->devi_sys_prop_ptr,
			    &(DEVI(och)->devi_sys_prop_ptr));
			mutex_exit(&(DEVI(och)->devi_lock));
			mutex_exit(&(DEVI(child)->devi_lock));

			if (need_init)
				(void) ddi_initchild(parent, och);
		}

		/*
		 * We can toss the wildcard node...
		 */
		impl_ddi_sunbus_removechild(child);
		return (DDI_NOT_WELL_FORMED);
	}

	/*
	 * Force the name property to be generated from the "reg" property...
	 * (versus the "registers" property, so we always match the obp
	 * namespace no matter what the .conf file said.)
	 */

	name[0] = '\0';
	if ((has_registers) && ndi_dev_is_persistent_node(child)) {

		int *reg_prop;
		uint_t reg_len;

		if (get_prop_int_array(child, OBP_REG, 0, &reg_prop,
		    &reg_len) == DDI_PROP_SUCCESS)  {

			struct regspec *rp = (struct regspec *)reg_prop;

			(void) sprintf(name, "%x,%x", rp->regspec_bustype,
			    rp->regspec_addr);
			ddi_prop_free((void *)reg_prop);
		}

	} else if (sparc_pd_getnreg(child) > 0) {
		(void) sprintf(name, "%x,%x",
		    (uint_t)sparc_pd_getreg(child, 0)->regspec_bustype,
		    (uint_t)sparc_pd_getreg(child, 0)->regspec_addr);
	}

	ddi_set_name_addr(child, name);
	/*
	 * If another child already exists by this name,
	 * merge these properties onto that one.
	 * NOTE - This property override/merging depends on several things:
	 * 1) That hwconf nodes are 'named' (ddi_initchild()) before prom
	 *	devinfo nodes.
	 * 2) That ddi_findchild() will call ddi_initchild() for all
	 *	siblings with a matching devo_name field.
	 * 3) That hwconf devinfo nodes come "after" prom devinfo nodes.
	 *
	 * Then "och" should be a prom node with no attached properties.
	 */
	if ((och = ddi_findchild(parent, ddi_get_name(child), name)) != NULL &&
	    och != child) {
		if ((ndi_dev_is_persistent_node(och) == 0) ||
		    ndi_dev_is_persistent_node(child) ||
		    DEVI(och)->devi_sys_prop_ptr ||
		    DEVI(och)->devi_drv_prop_ptr || DDI_CF2(och)) {
			cmn_err(CE_WARN, cantmerge, "hwconf",
			    ddi_get_name(child), name);
			impl_ddi_sunbus_removechild(child);
			return (DDI_NOT_WELL_FORMED);
		}
		/*
		 * Move "child"'s properties to "och." and allow the node
		 * to be init-ed (this handles 'reg' and 'intr/interrupts'
		 * in hwconf files overriding those in a hw node)
		 *
		 * Note that 'och' is not yet in canonical form 2, so we
		 * can happily transform it to prototype form and recreate it.
		 */
		(void) ddi_uninitchild(och);
		mutex_enter(&(DEVI(child)->devi_lock));
		mutex_enter(&(DEVI(och)->devi_lock));
		DEVI(och)->devi_sys_prop_ptr = DEVI(child)->devi_sys_prop_ptr;
		DEVI(och)->devi_drv_prop_ptr = DEVI(child)->devi_drv_prop_ptr;
		DEVI(child)->devi_sys_prop_ptr = NULL;
		DEVI(child)->devi_drv_prop_ptr = NULL;
		mutex_exit(&(DEVI(och)->devi_lock));
		mutex_exit(&(DEVI(child)->devi_lock));
		(void) ddi_initchild(parent, och);
		/*
		 * To get rid of this child...
		 */
		impl_ddi_sunbus_removechild(child);
		return (DDI_NOT_WELL_FORMED);
	}
	return (DDI_SUCCESS);
}

/*
 * non-sbus adaptors use this function, (until we can change the rest
 * of the nexi to use a common function that passes in requesting parent
 * dip, or break this entire mess up.
 */
int
impl_ddi_sunbus_initchild(dev_info_t *child)
{
	return (i_impl_ddi_sunbus_initchild(child, 0));
}

/*
 * sbus adaptors (that support s/w registers wildcarding) use this function
 */
int
impl_ddi_sbus_initchild(dev_info_t *child)
{
	return (i_impl_ddi_sunbus_initchild(child, 1));
}

void
impl_ddi_sunbus_removechild(dev_info_t *dip)
{
	register struct ddi_parent_private_data *pdptr;
	register size_t n;

	if ((pdptr = (struct ddi_parent_private_data *)
	    ddi_get_parent_data(dip)) != NULL)  {
		if ((n = (size_t)pdptr->par_nintr) != 0)
			/*
			 * Note that kmem_free is used here (instead of
			 * ddi_prop_free) because the contents of the
			 * property were placed into a separate buffer and
			 * mucked with a bit before being stored in par_intr.
			 * The actual return value from the prop lookup
			 * was freed with ddi_prop_free previously.
			 */

			kmem_free(pdptr->par_intr, n *
			    sizeof (struct intrspec));

		if ((n = (size_t)pdptr->par_nrng) != 0)
			ddi_prop_free((void *)pdptr->par_rng);

		if ((n = pdptr->par_nreg) != 0)
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

int
i_ddi_intr_hilevel(dev_info_t *dip, uint_t inumber)
{
	ddi_intrspec_t ispec;
	int	r = 0;

	/*
	 * Get the named interrupt specification.  If found, perform
	 * the bus op to find out whether it is hilevel or not.
	 */
	ispec = i_ddi_get_intrspec(dip, dip, inumber);

	if (ispec != NULL)
		(void) ddi_ctlops(dip, dip, DDI_CTLOPS_INTR_HILEVEL,
		    (void *)ispec, (void *)&r);

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

	/* get the named interrupt specification */
	if ((ispec = i_ddi_get_intrspec(dip, dip, inumber)) == NULL)
		return (DDI_INTR_NOTFOUND);

	/* request the parent node to add it */
	return (i_ddi_add_intrspec(dip, dip, ispec, iblock_cookiep,
	    idevice_cookiep, int_handler, int_handler_arg,
	    IDDI_INTR_TYPE_NORMAL));
}

int
i_ddi_add_fastintr(dev_info_t *dip, uint_t inumber,
    ddi_iblock_cookie_t *iblock_cookiep,
    ddi_idevice_cookie_t *idevice_cookiep,
    uint_t (*hi_int_handler)(void))
{
	ddi_intrspec_t ispec;

	/* get the named interrupt specification */
	if ((ispec = i_ddi_get_intrspec(dip, dip, inumber)) == NULL) {
		return (DDI_INTR_NOTFOUND);
	}

	/* request the parent node to add it */
	return (i_ddi_add_intrspec(dip, dip, ispec, iblock_cookiep,
	    idevice_cookiep, (uint_t (*)(caddr_t))hi_int_handler, 0,
	    IDDI_INTR_TYPE_FAST));
}

void
i_ddi_remove_intr(dev_info_t *dip, uint_t inum,
    ddi_iblock_cookie_t iblock_cookie)
{
	ddi_intrspec_t ispec;

	/* get the named interrupt specification */
	if ((ispec = i_ddi_get_intrspec(dip, dip, inum)) != NULL) {
		/* request the parent node to remove it */
		i_ddi_remove_intrspec(dip, dip, ispec, iblock_cookie);
	}
}

int
i_ddi_dev_nintrs(dev_info_t *dev, int *result)
{
	return (ddi_ctlops(dev, dev, DDI_CTLOPS_NINTRS,
	    (void *)0, (void *)result));
}

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
	struct soft_intrspec *sspec;
	struct intrspec *ispec;
	int r;

	if (idp == NULL)
		return (DDI_FAILURE);
	sspec = (struct soft_intrspec *)kmem_zalloc(sizeof (*sspec), KM_SLEEP);
	sspec->si_devi = (struct dev_info *)rdip;
	ispec = &sspec->si_intrspec;
	if (preference > DDI_SOFTINT_MED) {
		ispec->intrspec_pri = 6;
	} else {
		ispec->intrspec_pri = 4;
	}
	rp = ddi_root_node();
	r = (*(DEVI(rp)->devi_ops->devo_bus_ops->bus_add_intrspec))(rp,
	    rdip, (ddi_intrspec_t)ispec, iblock_cookiep, idevice_cookiep,
	    int_handler, int_handler_arg, IDDI_INTR_TYPE_SOFT);
	if (r != DDI_SUCCESS) {
		kmem_free((caddr_t)sspec, sizeof (*sspec));
		return (r);
	}
	*idp = (ddi_softintr_t)sspec;
	return (DDI_SUCCESS);
}

void
i_ddi_trigger_softintr(ddi_softintr_t id)
{
	uint_t pri = ((struct soft_intrspec *)id)->si_intrspec.intrspec_pri;
	/* ICK! */
	setsoftint(INT_IPL(pri));
}

void
i_ddi_remove_softintr(ddi_softintr_t id)
{
	struct soft_intrspec *sspec = (struct soft_intrspec *)id;
	struct intrspec *ispec = &sspec->si_intrspec;
	dev_info_t *rp = ddi_root_node();
	dev_info_t *rdip = (dev_info_t *)sspec->si_devi;

	(*(DEVI(rp)->devi_ops->devo_bus_ops->bus_remove_intrspec))(rp,
	    rdip, (ddi_intrspec_t)ispec, (ddi_iblock_cookie_t)0);
	kmem_free((caddr_t)sspec, sizeof (*sspec));
}


/* New interrupt architecture not implemented on 4d machines. */
/*ARGSUSED*/
int
i_ddi_intr_ctlops(dev_info_t *dip, dev_info_t *rdip, ddi_intr_ctlop_t op,
    void *arg, void *val)
{
	return (DDI_FAILURE);
}


#define	XVECTOR(n)		\
int	xlvl##n##_spurious;	\
struct autovec xlvl##n[NVECT]

#define	VECTOR(n)		\
int	level##n##_spurious;	\
struct autovec level##n[NVECT]

/*ARGSUSED*/
static uint_t
softlevel1(caddr_t arg)
{
	softint();
	return (1);	/* so locore believes we handled it */
}

/*
 * These structures are used in locore.s to jump to device interrupt routines.
 * They also provide vmstat assistance.
 * They will index into the string table generated by autoconfig
 * but in the exact order addintr sees them. This allows IOINTR to quickly
 * find the right counter to increment.
 * (We use the fact that the arrays are initialized to 0 by default).
 */

/*
 * Initial interrupt vector information.
 * Each of these macros defines both the "spurious-int" counter and
 * the list of autovec structures that will be used by locore.s
 * to distribute interrupts to the interrupt requestors.
 * Each list is terminated by a null.
 * Lists are scanned only as needed: hard ints
 * stop scanning when the int is claimed; soft ints
 * scan the entire list. If nobody on the list claims the
 * interrupt, then a spurious interrupt is reported.
 *
 * These should all be initialized to zero, except for the
 * few interrupts that we have handlers for built into the
 * kernel that are not installed by calling "addintr".
 * I would like to eventually get everything going through
 * the "addintr" path.
 * It might be a good idea to remove VECTORs that are not
 * actually processed by locore.s
 */

/*
 * software vectored interrupts:
 *
 * Level1 is special (softcall handler), so we initialize it to always
 * call softlevel1 first.
 */

XVECTOR(1) = {	{syserr_handler, (caddr_t)1},
		{softlevel1},		/* time-scheduled tasks */
		{0}};
XVECTOR(2) = {{0}};
XVECTOR(3) = {{0}};
XVECTOR(4) = {{syserr_handler, (caddr_t)4}, {0}};
XVECTOR(5) = {{0}};
XVECTOR(6) = {{0}};
XVECTOR(7) = {{0}};
XVECTOR(8) = {{syserr_handler, (caddr_t)8}, {0}};
XVECTOR(9) = {{0}};
XVECTOR(10) = {{0}};
XVECTOR(11) = {{0}};
XVECTOR(12) = {{0}};
XVECTOR(13) = {{0}};
XVECTOR(14) = {{0}};
XVECTOR(15) = {{0}};

/*
 * For the sun4m, these are "otherwise unclaimed sparc interrupts", but for
 * us, they're all hardware interrupts
 */

VECTOR(1) = {{0}};
VECTOR(2) = {{0}};
VECTOR(3) = {{0}};
VECTOR(4) = {{0}};
VECTOR(5) = {{0}};
VECTOR(6) = {{0}};
VECTOR(7) = {{0}};
VECTOR(8) = {{0}};
VECTOR(9) = {{0}};
VECTOR(10) = {{0}};
VECTOR(11) = {{0}};
VECTOR(12) = {{0}};
VECTOR(13) = {{0}};
VECTOR(14) = {{0}};
VECTOR(15) = {{0}};

/*
 * indirection table, to save us some large switch statements
 * And so we can share avintr.c with sun4m, which actually uses large tables.
 * NOTE: This must agree with "INTLEVEL_foo" constants in
 *	<sun/autoconf.h>
 */
struct autovec *const vectorlist[] = {
/*
 * otherwise unidentified interrupts at SPARC levels 1..15
 */
	0,	level1,	level2,  level3,  level4,  level5,  level6,  level7,
	level8,	level9,	level10, level11, level12, level13, level14, level15,
/*
 * interrupts identified as "soft"
 */
	0,	xlvl1,	xlvl2,	xlvl3,	xlvl4,	xlvl5,	xlvl6,	xlvl7,
	xlvl8,	xlvl9,	xlvl10,	xlvl11,	xlvl12,	xlvl13,	xlvl14,	xlvl15,
};

/*
 * This string is pased to not_serviced() from locore.
 */
const char busname_vec[] = "iobus ";	/* only bus we know */

/*
 * This value is exported here for the functions in avintr.c
 */
const uint_t maxautovec = (sizeof (vectorlist) / sizeof (vectorlist[0]));

/*
 * NOTE: if a device can generate interrupts on more than
 * one level, or if a driver services devices that interrupt
 * on more than one level, then the driver should install
 * itself on each of those levels.
 *
 * On Hard-ints, order of evaluation of the chains is:
 *   scan "unspecified" chain; if nobody claims,
 *	report spurious interrupt.
 * Scanning terminates with the first driver that claims it has
 * serviced the interrupt.
 *
 * On Soft-ints, order of evaulation of the chains is:
 *   scan the "unspecified" chain
 *   scan the "soft" chain
 * Scanning continues until some driver claims the interrupt (all softint
 * routines get called if no hardware int routine claims the interrupt and
 * if the software interrupt bit is on in the interrupt register).  If there
 * is no pending software interrupt, we report a spurious hard interrupt.
 * If soft int bit in interrupt register is on and nobody claims the interrupt,
 * report a spurious soft interrupt.
 */

/*
 * Check for machine specific interrupt levels which cannot be reasigned by
 * settrap(), sun4d version.
 */
/*ARGSUSED*/
int
exclude_settrap(int lvl)
{
	return (1);	/* i.e. we don't allow any! */
}

/*
 * Check for machine specific interrupt levels which cannot be set.
 */
/*ARGSUSED*/
int
exclude_level(int lvl)
{
	return (0);	/* in theory we can set any */
}


/*
 * SECTION: DDI Memory/DMA
 */

int
i_ddi_mem_alloc(dev_info_t *dip, ddi_dma_attr_t *attr,
	size_t length, int cansleep, int streaming,
	ddi_device_acc_attr_t *accattrp, caddr_t *kaddrp,
	size_t *real_length, ddi_acc_hdl_t *ap)
{
	caddr_t a;
	uint_t iomin, align;

#if defined(lint)
	accattrp = accattrp;
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

	a = kalloca(length, align, (streaming)? 0 : 1, cansleep);
	if ((*kaddrp = a) == 0) {
		return (DDI_FAILURE);
	} else {
		if (real_length) {
			*real_length = length;
		}
		if (ap) {
			/*
			 * assign handle information
			 */
			impl_acc_hdl_init(ap);
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

	/*
	 * set up DMA attribute structure to pass to i_ddi_mem_alloc()
	 */
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

void
i_ddi_mem_free(caddr_t kaddr, int stream)
{
	kfreea(kaddr, (stream)? 0 : 1);
}


/*
 * SECTION: DDI Data Access
 */

/*
 * Implementation wrapper functions
 */
int
i_ddi_poke(dev_info_t *dip, size_t size, void *addr, void *val)
{
	return (common_ddi_poke(dip, size, addr, val));
}

int
i_ddi_peek(dev_info_t *dip, size_t size, void *addr, void *val_p)
{
	return (common_ddi_peek(dip, size, addr, val_p));
}


/*
 * SECTION: Misc functions
 */

/*
 * On sun4d, we have an interesting situation -- because the system
 * boards include a bootbus with zs devices, and the boards are pluggable,
 * and the bootbus path can change if we remove the 'A' cpu module, the
 * instance number code gets confused and assigns incorrect and different
 * zs instance numbers.  We make out own assignments based on board# and
 * zs instance # within the board (0 or 1).  If the inherited property
 * board# is not defined, we can infer it from the device-id property.
 *
 * Note use of ddi_prop_get_int to bypass unattached drivers prop_op(9e)
 * function.
 *
 * Note:  The algortithm used fixes ttya/b to board#0, ttyc/d to board#1, ...
 */

/*
 * Turn this off (before FCS) when "board#" property is propagated to all proms.
 */
#define	PROM_BOARD_NUM_WORKAROUND	1


#define	ZS_SLAVE	"slave"
#define	BOARD		"board#"
#define	DEVICE_ID	"device-id"	/* Only used with board# workaraond */

static int is_cpu_device(dev_info_t *);
static int is_zs_device(dev_info_t *);

uint_t
impl_assign_instance(dev_info_t *dev)
{
	int	board, slave;
	static char *pmsg = "Unable to determine board# for <%s>";

#ifdef	PROM_BOARD_NUM_WORKAROUND
	int	deviceid;
#endif	PROM_BOARD_NUM_WORKAROUND

	/* cpu case */
	if (is_cpu_device(dev)) {
		return (0);
	}

	/* normal case */
	if (!is_zs_device(dev)) {
		return ((uint_t)-1);	/* let framework handle it */
	}

	/* zs case */

	/*
	 * Get inherited properties for slave (zs local instance # 0 or 1),
	 * board# from PROM, and default to using f(device-id) property
	 * value to compute board#.  We expect slave to be defined locally,
	 * and default to value zero if we don't find it, but this is
	 * probably a reasonable assumption to expect it to be defined.
	 *
	 * Software zs nodes should have already been merged into h/w nodes.
	 */

	ASSERT(strcmp(ddi_node_name(dev), "zs") == 0);
	ASSERT(ndi_dev_is_persistent_node(dev));

	slave = ddi_prop_get_int(DDI_DEV_T_ANY, dev, DDI_PROP_DONTPASS,
	    ZS_SLAVE, 0);
	board = ddi_prop_get_int(DDI_DEV_T_ANY, dev, 0, BOARD, -1);

#ifdef	PROM_BOARD_NUM_WORKAROUND
	if (board == -1)  {
		deviceid = ddi_prop_get_int(DDI_DEV_T_ANY, dev, 0,
		    "device-id", -1);
		if (deviceid != -1)  {
			/* From architecture specification */
			board = deviceid >> 4;
		}
	}
#endif	PROM_BOARD_NUM_WORKAROUND

	if (board == -1)  {
		cmn_err(CE_PANIC, pmsg, ddi_get_name_addr(dev));
		/*NOTREACHED*/
	}

	/*
	 * Instance# = board# * (number of zs per board) + duart instance
	 * Instance# = (board * 2) + slave;
	 */
	return ((board << 1) + slave);
}
#undef	ZS_SLAVE
#undef	BOARD
#undef	DEVICE_ID


static int
is_cpu_device(dev_info_t *dev)
{
	/*
	 * 1140626: For sun4d cpu devices, don't let the ddi instance
	 * number code assign the instance #, as it is assigned based
	 * on the cpu-id in the attach routine.
	 */
	return ((strcmp(DEVI(dev)->devi_node_name, "cpu") == 0) ||
	    (strcmp(DEVI(dev)->devi_node_name, "TI,TMS390Z55") == 0));
}

static int
is_zs_device(dev_info_t *dev)
{
	/*
	 * 1100913: For sun4d (ONLY), zs devices, don't let the ddi instance
	 * number code assign the instance #, we'll do it based on board#.
	 */
	return (strcmp(DEVI(dev)->devi_node_name, "zs") == 0);
}

int
impl_keep_instance(dev_info_t *dev)
{
	if (is_cpu_device(dev) || is_zs_device(dev)) {
		/* override framework */
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

int
impl_free_instance(dev_info_t *dev)
{
	if (is_cpu_device(dev) || is_zs_device(dev)) {
		/* override framework */
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

/*ARGSUSED*/
int
impl_check_cpu(dev_info_t *devi)
{
	return (DDI_SUCCESS);
}

/*
 * Called from common/cpr_driver.c: Power off machine
 * Let the firmware remove power, if it knows how to.
 */
void
arch_power_down()
{
	int is_defined = 0;
	char *wordexists = "p\" power-off\" find nip swap ! ";

	/*
	 * is_defined has value -1 when defined
	 */
	prom_interpret(wordexists, (int)(&is_defined), 0, 0, 0, 0);
	if (is_defined)
		prom_interpret("power-off", 0, 0, 0, 0, 0);
}
