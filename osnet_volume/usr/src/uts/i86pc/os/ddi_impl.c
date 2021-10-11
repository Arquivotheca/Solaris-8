/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ddi_impl.c	1.107	99/11/08 SMI"

/*
 * PC specific DDI implementation
 */
#include <sys/types.h>
#include <sys/autoconf.h>
#include <sys/avintr.h>
#include <sys/bootconf.h>
#include <sys/conf.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/ddi_subrdefs.h>
#include <sys/eisarom.h>
#include <sys/ethernet.h>
#include <sys/fp.h>
#include <sys/instance.h>
#include <sys/kmem.h>
#include <sys/machsystm.h>
#include <sys/modctl.h>
#include <sys/nvm.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/sunndi.h>
#include <sys/ndi_impldefs.h>
#include <sys/sysmacros.h>
#include <sys/systeminfo.h>
#include <sys/utsname.h>
#include <sys/atomic.h>
#include <vm/seg_kmem.h>

/*
 * DDI Boot Configuration
 */

/*
 * Machine type we are running on.
 */
short cputype;

/*
 * Favoured drivers of this implementation
 * architecture.  These drivers MUST be present for
 * the system to boot at all.
 */
char *impl_module_list[] = {
	"rootnex",
	"options",
	"sad",
	"objmgr",
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
char *default_mta_drivers = "sd st";

#if !defined(SAS) && !defined(MPSAS)

int envm_check(void);

/*
 * Forward declarations
 */
static int getlongprop_buf();
static void get_boot_properties(void);

static int eisa_getfunc(regs *);
static int eisa_getslot(regs *);
static int eisa_read_func(int, int, char *);
static int eisa_getnvm(regs *);

#endif	/* !SAS && !MPSAS */

#define	CTGENTRIES	15

static struct ctgas {
	struct ctgas	*ctg_next;
	int		ctg_index;
	void		*ctg_addr[CTGENTRIES];
	size_t		ctg_size[CTGENTRIES];
} ctglist;

static kmutex_t		ctgmutex;
#define	CTGLOCK()	mutex_enter(&ctgmutex)
#define	CTGUNLOCK()	mutex_exit(&ctgmutex)

/*
 * Configure the hardware on the system.
 * Called before the rootfs is mounted
 */
void
configure(void)
{
	major_t major;
	dev_info_t *dip;
	extern int fpu_pentium_fdivbug;
	extern int fpu_ignored;

	/*
	 * Determine if an FPU is attached
	 */

#ifndef	MPSAS	/* no fpu module yet in MPSAS */
	fpu_probe();
#endif
	if (fpu_pentium_fdivbug) {
		printf("\
FP hardware exhibits Pentium floating point divide problem\n");
		if (!fpu_ignored)
			printf("\
If you wish to disable the FPU, edit /etc/system and append:\n\
\tset use_pentium_fpu_fdivbug = 0\n");
	}
	if (fpu_ignored) {
		printf("FP hardware will not be used\n");
	} else if (!fpu_exists) {
		printf("No FPU in configuration\n");
	}

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
	 * Read in the properties from the boot.
	 */
	get_boot_properties();

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
	int fail_len = (int)strlen(fail);

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
/*ARGSUSED1*/
int
check_status(int id, char *name, dev_info_t *parent)
{
	char status_buf[64];
	char devtype_buf[OBP_MAXPROPNAME];
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
	 * print the status property information
	 */
	cmn_err(CE_WARN, "status '%s' for '%s'", status_buf, name);
	return (retval);
}

/*ARGSUSED*/
uint_t
softlevel1(caddr_t arg)
{
	softint();
	return (1);
}

/*
 * We set the cpu type from the idprom, if we can.
 * Note that we just read out the contents of it, for the most part.
 * Except for cputype, sigh.
 */

void
setcputype(void)
{
	cputype |= I86_PC;
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

static int
getlongprop_buf(int id, char *name, char *buf, int maxlen)
{
	int size;

	size = prom_getproplen((dnode_t)id, name);
	if (size <= 0 || (size > maxlen - 1))
		return (-1);

	if (-1 == prom_getprop((dnode_t)id, name, buf))
		return (-1);

	if (strcmp("name", name) == 0) {
		if (buf[size - 1] != '\0') {
			buf[size] = '\0';
			size += 1;
		}
	}

	return (size);
}

static int
get_prop_int_array(dev_info_t *di, char *pname, int **pval, uint_t *plen)
{
	int ret;

	if ((ret = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, di,
	    DDI_PROP_DONTPASS, pname, pval, plen))
	    == DDI_PROP_SUCCESS) {
		*plen = (*plen) * (sizeof (int));
	}
	return (ret);
}


/*
 * Node Configuration
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
 * The "ranges" property is optional.
 */
static int
make_ddi_ppd(dev_info_t *child, struct ddi_parent_private_data **ppd)
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
	 * (non-pseudo) device node.
	 */

	if (get_prop_int_array(child, "reg", &reg_prop, &reg_len)
	    != DDI_PROP_SUCCESS) {
		reg_len = 0;
	}
	if (get_prop_int_array(child, "registers", &rgstr_prop,
	    &rgstr_len) != DDI_PROP_SUCCESS) {
		rgstr_len = 0;
	}

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
	 * See if I have a range (adding one where needed - this
	 * means to add one for sbus node in sun4c, when romvec > 0,
	 * if no range is already defined in the PROM node.
	 * (Currently no sun4c PROMS define range properties,
	 * but they should and may in the future.)  For the SBus
	 * node, the range is defined by the SBus reg property.
	 */
	if (get_prop_int_array(child, "ranges", &rng_prop, &rng_len)
	    == DDI_PROP_SUCCESS) {
		pdptr->par_nrng = rng_len / (int)(sizeof (struct rangespec));
		pdptr->par_rng = (struct rangespec *)rng_prop;
	}

	/*
	 * Handle the 'intr' and 'interrupts' properties
	 */

	/*
	 * For backwards compatibility
	 * we first look for the 'intr' property for the device.
	 */
	if (get_prop_int_array(child, "intr", &intr_prop, &intr_len)
	    != DDI_PROP_SUCCESS) {
		intr_len = 0;
	}

	/*
	 * If we're to support bus adapters and future platforms cleanly,
	 * we need to support the generalized 'interrupts' property.
	 */
	if (get_prop_int_array(child, "interrupts", &irupts_prop,
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
 *	Property		value
 *	  Name			type
 *	on_cpu		cpu type flag (defined in cpu.h)
 *	not_on_cpu	cpu type flag (defined in cpu.h)
 *	reg		register spec
 *	registers	wildcard s/w sbus register spec (.conf file property)
 *	intr		old-form interrupt spec
 *	interrupts	new (bus-oriented) interrupt spec
 *	ranges		range spec
 */

static char *cantmerge = "Cannot merge %s devinfo node %s@%s";

int
impl_ddi_sunbus_initchild(dev_info_t *child)
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
	has_registers = make_ddi_ppd(child, &pdptr);
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

		if (get_prop_int_array(child, "reg", &reg_prop, &reg_len) ==
		    DDI_PROP_SUCCESS)  {

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

void
impl_ddi_sunbus_removechild(dev_info_t *dip)
{
	register struct ddi_parent_private_data *pdptr;
	register size_t n;

	if ((pdptr = (struct ddi_parent_private_data *)
	    ddi_get_parent_data(dip)) != NULL)  {
		if ((n = (size_t)pdptr->par_nintr) != 0) {
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
		}

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
 * DDI Interrupt
 */

/*
 * turn this on to force isa, eisa, and mca device to ignore the new
 * hardware nodes in the device tree (normally turned on only for
 * drivers that need it by setting the property "ignore-hardware-nodes"
 * in their driver.conf file).
 *
 * 7/31/96 -- Turned off globally.  Leaving variable in for the moment
 *		as safety valve.
 */
int ignore_hardware_nodes = 0;

/*
 * Local data
 */
static struct impl_bus_promops *impl_busp;

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

static char	*chosen_intr = "chosen-interrupt";

int
i_ddi_add_intr(dev_info_t *dip, uint_t inumber,
    ddi_iblock_cookie_t *iblock_cookiep,
    ddi_idevice_cookie_t *idevice_cookiep,
    uint_t (*int_handler)(caddr_t int_handler_arg),
    caddr_t int_handler_arg)
{
	ddi_intrspec_t ispec;
	struct {
		int	ipl;
		int	irq;
	} intr, *intrlist;
	uint_t	length;
	int	rc;

	/* get the named interrupt specification */
	if ((ispec = i_ddi_get_intrspec(dip, dip, inumber)) == NULL)
		return (DDI_INTR_NOTFOUND);

	/*
	 * get the 'interrupts' or the 'intr' property.
	 * treat the interrupts property as an array of int's.
	 */
	rc = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
			DDI_PROP_DONTPASS, "interrupts",
			(int **)&intrlist, &length);
	if (rc != DDI_PROP_SUCCESS)
		rc = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
				DDI_PROP_DONTPASS, "intr",
				(int **)&intrlist, &length);
	if (rc == DDI_PROP_SUCCESS) {
		/*
		 * point to the required entry.
		 */
		intr = intrlist[inumber];

		/*
		 * make a new property containing ONLY the required tuple.
		 */
		if (ddi_prop_update_int_array(DDI_DEV_T_NONE, dip,
		    chosen_intr, (int *)&intr,
		    (sizeof (intr)/sizeof (int))) != DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN, "%s%d: cannot create '%s' "
			    "property", DEVI(dip)->devi_name,
			    DEVI(dip)->devi_instance, chosen_intr);
		}
		/*
		 * free the memory allocated by
		 * ddi_prop_lookup_int_array ().
		 */
		ddi_prop_free((void *)intrlist);
	}

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
		(void) ddi_prop_remove(DDI_DEV_T_NONE, dip,
		    chosen_intr);
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
		kmem_free(sspec, sizeof (*sspec));
		return (r);
	}
	*idp = (ddi_softintr_t)sspec;
	return (DDI_SUCCESS);
}

extern void (*setsoftint)(int);
void
i_ddi_trigger_softintr(ddi_softintr_t id)
{
	struct soft_intrspec *sspec = (struct soft_intrspec *)id;
	struct intrspec *ip;

	ip = &sspec->si_intrspec;
	(*setsoftint)(ip->intrspec_pri);
}

void
i_ddi_remove_softintr(ddi_softintr_t id)
{
	struct soft_intrspec *sspec = (struct soft_intrspec *)id;
	struct intrspec *ispec = &sspec->si_intrspec;

	(void) rem_avsoftintr((void *)ispec, ispec->intrspec_pri,
		ispec->intrspec_func);
	kmem_free(sspec, sizeof (*sspec));
}


/* New interrupt architecture not implemented on x86 machines. */
/*ARGSUSED*/
int
i_ddi_intr_ctlops(dev_info_t *dip, dev_info_t *rdip, ddi_intr_ctlop_t op,
    void *arg, void *val)
{
	return (DDI_FAILURE);
}


/*
 * DDI Memory/DMA
 */

/*
 * Support for allocating DMAable memory to implement
 * ddi_dma_mem_alloc(9F) interface.
 */

#define	MAX_MEM_RANGES	4
#define	KA_ALIGN_SHIFT	7
#define	KA_ALIGN	(1 << KA_ALIGN_SHIFT)
#define	KA_NCACHE	(PAGESHIFT + 1 - KA_ALIGN_SHIFT)

/*
 * Dummy DMA attributes.  We only care about addr_lo, addr_hi, and align.
 */
static ddi_dma_attr_t segkmem_io_attrs[MAX_MEM_RANGES] = {
	{
		DMA_ATTR_V0,
		0x0000000000000000ULL,		/* dma_attr_addr_lo */
		0x00000fffffffffffULL,		/* dma_attr_addr_hi */
		0x00ffffff,
		0x1000,				/* dma_attr_align */
		1, 1, 0xffffffffULL, 0xffffffffULL, 0x1, 1, 0
	},
	{
		DMA_ATTR_V0,
		0x0000000000000000ULL,		/* dma_attr_addr_lo */
		0x00000000ffffffffULL,		/* dma_attr_addr_hi */
		0x00ffffff,
		0x1000,				/* dma_attr_align */
		1, 1, 0xffffffffULL, 0xffffffffULL, 0x1, 1, 0
	},
	{
		DMA_ATTR_V0,
		0x0000000000000000ULL,		/* dma_attr_addr_lo */
		0x000000007fffffffULL,		/* dma_attr_addr_hi */
		0x00ffffff,
		0x1000,				/* dma_attr_align */
		1, 1, 0xffffffffULL, 0xffffffffULL, 0x1, 1, 0
	},
	{
		DMA_ATTR_V0,
		0x0000000000000000ULL,		/* dma_attr_addr_lo */
		0x0000000000ffffffULL,		/* dma_attr_addr_hi */
		0x00ffffff,
		0x1000,				/* dma_attr_align */
		1, 1, 0xffffffffULL, 0xffffffffULL, 0x1, 1, 0
	}
};

static vmem_t *kmem_io_arena[MAX_MEM_RANGES];
static kmem_cache_t *kmem_io_cache[MAX_MEM_RANGES][KA_NCACHE];

/*
 * Return the index of the highest memory range for addr.
 */
static int
kmem_io_index(uint64_t addr)
{
	int n;

	for (n = 0; n < MAX_MEM_RANGES; n++)
		if (segkmem_io_attrs[n].dma_attr_addr_hi <= addr)
			return (n);
	return (0);
}

static page_t *
page_create_io_wrapper(void *addr, size_t len, int vmflag, void *arg)
{
	extern page_t *page_create_io(vnode_t *, u_offset_t, uint_t,
	    uint_t, struct as *, caddr_t, ddi_dma_attr_t *);

	return (page_create_io(&kvp, (u_offset_t)(uintptr_t)addr, len,
	    PG_EXCL | ((vmflag & VM_NOSLEEP) ? 0 : PG_WAIT), &kas, addr, arg));
}

static void *
segkmem_alloc_io_16T(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &segkmem_io_attrs[0]));
}

static void *
segkmem_alloc_io_4G(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &segkmem_io_attrs[1]));
}

static void *
segkmem_alloc_io_2G(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &segkmem_io_attrs[2]));
}

static void *
segkmem_alloc_io_16M(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &segkmem_io_attrs[3]));
}

struct {
	char	*name;
	void	*(*alloc)(vmem_t *, size_t, int);
} io_arena_params[MAX_MEM_RANGES] = {
	{ "kmem_io_16T",	segkmem_alloc_io_16T	},
	{ "kmem_io_4G",		segkmem_alloc_io_4G	},
	{ "kmem_io_2G",		segkmem_alloc_io_2G	},
	{ "kmem_io_16M",	segkmem_alloc_io_16M	}
};

void
ka_init(void)
{
	int a, c;
	char name[40];

	for (a = 0; a < MAX_MEM_RANGES; a++) {
		kmem_io_arena[a] = vmem_create(io_arena_params[a].name,
		    NULL, 0, PAGESIZE, io_arena_params[a].alloc,
		    segkmem_free, heap_arena, 0, VM_SLEEP);
		for (c = 0; c < KA_NCACHE; c++) {
			size_t size = KA_ALIGN << c;
			(void) sprintf(name, "%s_%lu",
			    io_arena_params[a].name, size);
			kmem_io_cache[a][c] = kmem_cache_create(name,
			    size, size, NULL, NULL, NULL, NULL,
			    kmem_io_arena[a], 0);
		}
	}
}

/*
 * put contig address/size
 */
static void *
putctgas(void *addr, size_t size)
{
	struct ctgas	*ctgp = &ctglist;
	int		i;

	CTGLOCK();
	do {
		if ((i = ctgp->ctg_index) < CTGENTRIES) {
			ctgp->ctg_addr[i] = addr;
			ctgp->ctg_size[i] = size;
			ctgp->ctg_index++;
			break;
		}
		if (!ctgp->ctg_next)
			ctgp->ctg_next = kmem_zalloc(sizeof (struct ctgas),
						KM_NOSLEEP);
		ctgp = ctgp->ctg_next;
	} while (ctgp);

	CTGUNLOCK();
	return (ctgp);
}

/*
 * get contig size by addr
 */
static size_t
getctgsz(void *addr)
{
	struct ctgas	*ctgp = &ctglist;
	int		i, j;
	size_t		sz;

	ASSERT(addr);
	CTGLOCK();

	while (ctgp) {
		for (i = 0; i < ctgp->ctg_index; i++) {
			if (addr != ctgp->ctg_addr[i])
				continue;

			sz = ctgp->ctg_size[i];
			j = --ctgp->ctg_index;
			if (i != j) {
				ctgp->ctg_size[i] = ctgp->ctg_size[j];
				ctgp->ctg_addr[i] = ctgp->ctg_addr[j];
			}
			CTGUNLOCK();
			return (sz);
		}
		ctgp = ctgp->ctg_next;
	}

	CTGUNLOCK();
	return (0);
}

/*
 * contig_alloc:
 *
 *	allocates contiguous memory to satisfy the 'size' and dma attributes
 *	specified in 'attr'.
 *
 *	Not all of memory need to be physically contiguous if the
 *	scatter-gather list length is greater than 1.
 */

void *
contig_alloc(size_t size, ddi_dma_attr_t *attr, int align, int cansleep)
{
	pgcnt_t		pgcnt = btopr(size);
	size_t		asize = pgcnt * PAGESIZE;
	page_t		*ppl;
	int		pflag;
	void		*addr;

	extern page_t *page_create_io(vnode_t *, u_offset_t, uint_t,
	    uint_t, struct as *, caddr_t, ddi_dma_attr_t *);
#ifdef lint
	align = align;
#endif

	ASSERT(align <= PAGESIZE);

	/* segkmem_xalloc */

	if (addr = vmem_alloc(heap_arena, asize,
			(cansleep) ? VM_SLEEP : VM_NOSLEEP)) {

		ASSERT(!((uint_t)addr & PAGEOFFSET));

		if (page_resv(pgcnt,
			(cansleep) ? KM_SLEEP : KM_NOSLEEP) == 0) {

			vmem_free(heap_arena, addr, asize);
			return (NULL);
		}
		pflag = PG_EXCL;

		if (cansleep)
			pflag |= PG_WAIT;

		/* 4k req gets from freelists rather than pfn search */
		if (pgcnt > 1)
			pflag |= PG_PHYSCONTIG;

		ppl = page_create_io(&kvp, (u_offset_t)(uintptr_t)addr,
			asize, pflag, &kas, (caddr_t)addr, attr);

		if (!ppl) {
			vmem_free(heap_arena, addr, asize);
			page_unresv(pgcnt);
			return (NULL);
		}

		while (ppl != NULL) {
			page_t	*pp = ppl;
			page_sub(&ppl, pp);
			ASSERT(page_iolock_assert(pp));
			page_io_unlock(pp);
			page_downgrade(pp);
			hat_memload(kas.a_hat, (caddr_t)pp->p_offset,
				pp, (PROT_ALL & ~PROT_USER) |
				HAT_NOSYNC, HAT_LOAD_LOCK);
		}
	}
	return (addr);
}

static void
contig_free(void *addr, size_t size)
{

	pgcnt_t	pgcnt = btopr(size);
	size_t	asize = pgcnt * PAGESIZE;
	caddr_t	a, ea;
	page_t	*pp;

	hat_unload(kas.a_hat, addr, asize, HAT_UNLOAD_UNLOCK);

	for (a = addr, ea = a + asize; a < ea; a += PAGESIZE) {
		pp = page_find(&kvp,
				(u_offset_t)(uintptr_t)a);
		if (!pp)
			panic("contig_free: contig pp not found");

		if (!page_tryupgrade(pp)) {
			page_unlock(pp);
			pp = page_lookup(&kvp,
				(u_offset_t)(uintptr_t)a, SE_EXCL);
			if (pp == NULL)
				panic("contig_free: page freed");
		}
		page_destroy(pp, 0);
	}

	page_unresv(pgcnt);
	vmem_free(heap_arena, addr, asize);
}

extern caddr_t lomem_alloc(uint_t size, ddi_dma_attr_t *attr,
	    int align, int cansleep);
extern void lomem_free(caddr_t kaddr);
extern long	lomempages;

/*
 * Allocate from the system, aligned on a specific boundary.
 * The alignment, if non-zero, must be a power of 2.
 */
static void *
kalloca(size_t size, size_t align, int cansleep, int physcontig,
	ddi_dma_attr_t *attr)
{
	size_t *addr, *raddr, rsize;
	size_t hdrsize = 4 * sizeof (size_t);	/* must be power of 2 */
	int a = kmem_io_index(attr->dma_attr_addr_hi);
	vmem_t *vmp = kmem_io_arena[a];
	kmem_cache_t *cp = NULL;

	align = MAX(align, hdrsize);
	ASSERT((align & (align - 1)) == 0);
	rsize = P2ROUNDUP(P2ROUNDUP(size, align) + align + hdrsize, KA_ALIGN);

	if (physcontig && rsize > PAGESIZE) {
		if (addr = contig_alloc(size, attr, align, cansleep)) {
			if (!putctgas(addr, size))
				contig_free(addr, size);
			else
				return (addr);
		}
		if (btopr(size) <= lomempages && align <= 16)
			return (lomem_alloc(size, attr, align, cansleep));

		return (NULL);
	}

	if (rsize > PAGESIZE) {
		raddr = vmem_alloc(vmp, rsize, VM_NOSLEEP);
	} else {
		cp = kmem_io_cache[a][highbit((rsize >> KA_ALIGN_SHIFT) - 1)];
		raddr = kmem_cache_alloc(cp, KM_NOSLEEP);
	}

	if (raddr == NULL) {
		/*
		 * Failed to allocate it from the vmem arena.  If the alignment
		 * is at most 16 bytes, try to allocate from the lomem pool.
		 * Note: we don't need padding when allocating from lomem.
		 */
		if (align <= 16 &&
		    (raddr = (void *)lomem_alloc(size, attr, align, 0)) != NULL)
			return (raddr);

		if (!cansleep)
			return (NULL);

		if (rsize > PAGESIZE)
			raddr = vmem_alloc(vmp, rsize, VM_SLEEP);
		else
			raddr = kmem_cache_alloc(cp, KM_SLEEP);
	}

	ASSERT(!P2CROSS((uintptr_t)raddr, (uintptr_t)raddr + rsize - 1,
	    PAGESIZE) || rsize > PAGESIZE);

	addr = (size_t *)P2ROUNDUP((uintptr_t)raddr + hdrsize, align);
	addr[-4] = (size_t)cp;
	addr[-3] = (size_t)vmp;
	addr[-2] = (size_t)raddr;
	addr[-1] = rsize;

	return (addr);
}

static void
kfreea(void *addr)
{
	size_t		size;
	extern int	islomembuf(void *);

	if (islomembuf(addr))
		lomem_free(addr);
	else if (!((uint_t)addr & PAGEOFFSET) && (size = getctgsz(addr)))
		contig_free(addr, size);
	else {
		size_t	*saddr = addr;
		if (saddr[-4] == 0)
			vmem_free((vmem_t *)saddr[-3], (void *)saddr[-2],
				saddr[-1]);
		else
			kmem_cache_free((kmem_cache_t *)saddr[-4],
				(void *)saddr[-2]);
	}
}

/*
 * This should actually be called i_ddi_dma_mem_alloc. There should
 * also be an i_ddi_pio_mem_alloc. i_ddi_dma_mem_alloc should call
 * through the device tree with the DDI_CTLOPS_DMA_ALIGN ctl ops to
 * get alignment requirements for DMA memory. i_ddi_pio_mem_alloc
 * should use DDI_CTLOPS_PIO_ALIGN. Since we only have i_ddi_mem_alloc
 * so far which is used for both, DMA and PIO, we have to use the DMA
 * ctl ops to make everybody happy.
 */
int
i_ddi_mem_alloc(dev_info_t *dip, ddi_dma_attr_t *attr,
	size_t length, int cansleep, int streaming,
	ddi_device_acc_attr_t *accattrp, caddr_t *kaddrp,
	size_t *real_length, ddi_acc_hdl_t *ap)
{
	caddr_t a;
	int iomin;
	ddi_acc_impl_t *iap;
	int physcontig = 0;

#if defined(lint)
	dip = dip;
	accattrp = accattrp;
	streaming = streaming;
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
	 * figure out most restrictive alignment requirement
	 */
	iomin = attr->dma_attr_minxfer;
	iomin = maxbit(iomin, attr->dma_attr_align);
	if (iomin == 0)
		return (DDI_FAILURE);

	ASSERT((iomin & (iomin - 1)) == 0);

	length = P2ROUNDUP(length, iomin);

	/*
	 * Determine if we need to satisfy the request for physically
	 * contiguous memory.
	 */
	if ((attr->dma_attr_sgllen == 1) ||
	    (btopr(length) > attr->dma_attr_sgllen))
		physcontig = 1;

	if (iomin > PAGESIZE)
		return (DDI_FAILURE);

	/*
	 * Allocate the requested amount from the system.
	 */
	a = kalloca(length, iomin, cansleep, physcontig, attr);

	if ((*kaddrp = a) == 0)
		return (DDI_FAILURE);

	if (real_length) {
		*real_length = length;
	}
	if (ap) {
		/*
		 * initialize access handle
		 */
		iap = (ddi_acc_impl_t *)ap->ah_platform_private;
		iap->ahi_acc_attr |= DDI_ACCATTR_CPU_VADDR;
		impl_acc_hdl_init(ap);
	}
	return (DDI_SUCCESS);
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

	if (limits == NULL) {
		return (DDI_FAILURE);
	}

	/*
	 * set up DMA attribute structure to pass to i_ddi_mem_alloc()
	 */
	attrp = &dma_attr;
	attrp->dma_attr_version = DMA_ATTR_V0;
	attrp->dma_attr_addr_lo = (uint64_t)limits->dlim_addr_lo;
	attrp->dma_attr_addr_hi = (uint64_t)limits->dlim_addr_hi;
	attrp->dma_attr_count_max = (uint64_t)limits->dlim_ctreg_max;
	attrp->dma_attr_align = 1;
	attrp->dma_attr_burstsizes = (uint_t)limits->dlim_burstsizes;
	attrp->dma_attr_minxfer = (uint32_t)limits->dlim_minxfer;
	attrp->dma_attr_maxxfer = (uint64_t)limits->dlim_reqsize;
	attrp->dma_attr_seg = (uint64_t)limits->dlim_adreg_max;
	attrp->dma_attr_sgllen = limits->dlim_sgllen;
	attrp->dma_attr_granular = (uint32_t)limits->dlim_granular;
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


/*
 * DDI Data Access
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
 * Misc Functions
 */

/*
 * Implementation instance override functions
 *
 * No override on i86pc
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

/*
 * Referenced in common/cpr_driver.c: Power off machine.
 * Don't know how to power off i86pc.
 */
void
arch_power_down()
{
}


/*
 * EISA (Should be in a separate file?)
 */

/*
 * For eisa NVM support
 */
/*
#define	DEBUG_EISA_NVM	1
*/

#define	FALSE	0
#define	TRUE	1

#ifdef KEEP_INT15
/* Start using the property from boot.  Set to 1 to use protected mode int15 */
int eisa_int15 = 0;
#endif

/* place to put the eisa_nvram */
caddr_t	eisa_nvmp;
int	eisa_nvmlength = 0;

/*
 * read slot data from eisa cmos memory
 */
eisa_read_slot(slot, buffer)
int	slot;
char	*buffer;
{
	regs		reg;
	int	status;

	bzero((char *)&reg, sizeof (regs));
	reg.eax.word.ax = (unsigned short)EISA_READ_SLOT_CONFIG;
	reg.ecx.byte.cl = (unsigned char)slot;
	status = eisa_getnvm(&reg);

	/* Arranges data to match "NVM_SLOTINFO" structure. See "nvm.h". */

	*((short *)buffer) = reg.edi.word.di;
	buffer += sizeof (short);
	*((short *)buffer) = reg.esi.word.si;
	buffer += sizeof (short);
	*((short *)buffer) = reg.ebx.word.bx;
	buffer += sizeof (short);
	*buffer++ = reg.edx.byte.dh;
	*buffer++ = reg.edx.byte.dl;
	*((short *)buffer) = reg.ecx.word.cx;
	buffer += sizeof (short);
	return (status);
}

/*
 * read function data from eisa cmos memory
 */
static int
eisa_read_func(int slot, int func, char *buffer)
{
	regs	reg;
	int	status;

	bzero((char *)&reg, sizeof (regs));
	reg.eax.word.ax = (unsigned short)EISA_READ_FUNC_CONFIG;
	reg.ecx.byte.cl = (unsigned char)slot;
	reg.ecx.byte.ch = (unsigned char)func;
	reg.esi.esi = (unsigned int)buffer;
	status = eisa_getnvm(&reg);

	/* Data is arranged to match "NVM_FUNCINFO" structure. See "nvm.h". */

	return (status);
}

/*
 *	"eisa_nvm" -	general-purpose function for extracting configuration
 *			data from EISA non-volatile memory.
 *
 *	Inputs:
 *		A pointer to a big buffer allocated by the caller.
 *			The size of the buffer is a finction of the number
 *			of slotp and functions expected.  Asking for the
 *			whole system configuration can blow 20k.
 *
 *		A argument mask defining the "keys" to search for.
 *
 *		A variable number of arguments following the argument mask.
 *
 *		! CAVEAT !	Arguments passed must be in the order shown in
 *				"key_mask" below.
 *				Arguments may be omitted but the ordering must
 *				be maintained.
 *		Examples:
 *
 * 		slot function (board_id mask) revision checksum type sub-type
 *
 *		slot
 *
 *		slot function
 *
 *		(board_id mask)
 *
 *		(board_id mask) type
 *
 *		(board_id mask) revision
 *
 *		type sub-type
 *
 *		slot sub-type
 *
 *	Output:
 *		The number of bytes actually copied into the caller's buffer.
 *
 *		All slot and function records that pertain to all of the "keys"
 *			in the following format:
 *
 *		short slot_number
 *
 *		1st slot record
 *
 *		    1st function record
 *			.
 *			.
 *			.
 *		    "nth" function record
 *
 *		short slot_number
 *
 *		"nth" slot record
 *
 *		etc . . .
 *
 *	Examples:
 *
 *		bytes = eisa_nvm(buffer, 0);
 *
 *		will copy all slot and function records into "buffer".
 *
 *		bytes = eisa_nvm(buffer, SLOT, 0);
 *
 *		will copy the record for slot 0 and all its function
 *		records into "buffer".
 *
 *		bytes = eisa_nvm(buffer, TYPE, "DISCO");
 *
 *		will copy any/all slot and function records that
 *		pertain to the board type "DISCO" into "buffer".
 *
 *		bytes = eisa_nvm(buffer, EISA_BOARD_ID | TYPE, 0x0140110e,
 *							0xffffff, "COM");
 *
 *		will copy any/all slot and function records that
 *		pertain to the board id xx40110e and the type "COM" into
 *		"buffer".
 *
 *		bytes = eisa_nvm(buffer, BOARD_ID | CHECKSUM | TYPE,
 *				0x0140110e, 0xffffffff, 0xABCD, "ASY");
 *
 *		will copy any/all slot and function records that pertain to
 *		the board id 0140110e, the checksum 0xABCD and the type "ASY"
 *		into "buffer".
 */

/*
	This defines the mask used to determine what arguments are passed in.
*/

/*
 * Fill the data buffer from eisa cmos memory then search for the
 * specified key.
 */
/*
 * the arguments just follow on from the key_mask.
 * the order of the arguments is as above.
 * arguments are only present if the corresponding bit is set in the key_mask
 */

/*VARARGS2*/
int
eisa_nvm(char *data, KEY_MASK key_mask, ...)
{
	NVM_SLOTINFO	slot_info;
	NVM_FUNCINFO	func_info;
	char		*data_start = data;
	char		*new_slot;
	char		*argp = (char *)&key_mask + sizeof (key_mask);
	char		*type_arg = 0;
	char		*sub_type_arg = 0;
	int		len;
	unsigned int	val = 0;
	unsigned int	mask = 0;
	short		slot;
	short		slot_limit;
	short		function;
	short		function_arg;
	short		func_limit;
	unsigned short	revision = 0;
	unsigned short	checksum = 0;
	char		*type;	/* For operations on "type" field. */
	char		found_type;
	char		found_sub_type;


	if (envm_check() == DDI_FAILURE)
		return (0);

	/*
	 * extract the arguments
	 */

	/*
	 * If the slot bit is set get the slot number otherwise
	 * set up to get all slots
	 */
	if (key_mask.slot) {
		slot = *(short *)argp;
		slot_limit = slot + 1;
		argp += sizeof (int);
	} else {
		slot = 0;
		slot_limit = EISA_MAXSLOT;
	}
#ifdef DEBUG_EISA_NVM
	prom_printf("slot = %d  slot_limit = %d\n", slot, slot_limit);
#endif

	/*
	 * If the function bit is set get the function number otherwise
	 * set up to get all functions
	 */
	if (key_mask.function) {
		function_arg = *(short *)argp;
		argp += sizeof (int);
	}

	if (key_mask.board_id) {
		val  = *(unsigned *)argp,
		argp += sizeof (unsigned int);
		mask = *(unsigned *)argp;
		argp += sizeof (unsigned int);
#ifdef DEBUG_EISA_NVM
		prom_printf("val = %d  mask = %d\n", val, mask);
#endif
	}

	if (key_mask.revision) {
		revision = *(unsigned *)argp;
		argp += sizeof (unsigned int);
#ifdef DEBUG_EISA_NVM
		prom_printf("revision = %d\n", revision);
#endif
	}

	if (key_mask.checksum) {
		checksum = *(unsigned *)argp;
		argp += sizeof (unsigned int);
#ifdef DEBUG_EISA_NVM
		prom_printf("checksum = %d\n", checksum);
#endif
	}

	if (key_mask.type) {
		type_arg = *(char **)argp;
		argp += sizeof (char *);
#ifdef DEBUG_EISA_NVM
		prom_printf("type_arg = %s\n", type_arg);
#endif
	}

	if (key_mask.sub_type) {
		sub_type_arg = *(char **)argp;
		argp += sizeof (char *);
#ifdef DEBUG_EISA_NVM
		prom_printf("sub_type_arg = %s\n", sub_type_arg);
#endif
	}

	/* Searches through the required slots. */

	for (; slot < slot_limit; slot++) {
		new_slot = data;

		if (eisa_read_slot(slot, (char *)&slot_info) == 0) {
			/* Handles request for a specific function. */
			/* Checks the slot-related keys. */

			if (key_mask.board_id) {
				if ((*(unsigned int *)slot_info.boardid &
				    mask) != (val & mask))
					continue;
			}

			if (key_mask.revision) {
				if (slot_info.revision != revision)
					continue;
			}

			if (key_mask.checksum) {
				if (slot_info.checksum != checksum)
					continue;
			}

			/* Searches through the required functions. */
			/*
			 * If the function bit is set get the function number
			 * otherwise set up to get all functions
			 */
			if (key_mask.function) {
				function = function_arg;
				func_limit = function + 1;
			} else {
				function = 0;
				func_limit = slot_info.functions;
			}

#ifdef DEBUG_EISA_NVM
			prom_printf("function = %d  func_limit = %d\n",
			    function, func_limit);
#endif
			for (; function < func_limit; function++) {
				if (eisa_read_func(slot, function,
				    (char *)&func_info) == 0) {
					/* Checks the function-related keys. */

					type = (char *)func_info.type;
					found_type = FALSE;
					found_sub_type = FALSE;
					if (key_mask.type) {
						/* Searches for the type */
						/* string specified. */
						len = strlen(type_arg);
						while (*type &&
							*type != ';' &&
							type <
							(char *)func_info.type +
						    sizeof (func_info.type) &&
						    found_type == FALSE) {
							if (strncmp(type,
							    type_arg, len) == 0)
								found_type
								    = TRUE;
							else
								type++;
						}
						if (found_type == FALSE)
							/*
							 * Failed to match
							 * the requested type.
							 * try the next
							 * function
							 */
							continue;
					}

					/*
					 * skip over the type info
					 * skip until ';' NULL or end of
					 * string array
					 */
					while (*type && *type != ';' &&
					    type < (char *)func_info.type +
					    sizeof (func_info.type))
						type++;

					/*
					 * See if we're pointing to a ';'.
					 * If so, skip over it ... and then
					 * see if we need to check for a
					 * subtype.
					 */
					if (*type++ == ';' &&
					    key_mask.sub_type) {
						/*  Searches for the sub-type */
						/* string specified. */
						len = strlen(sub_type_arg);
						while (*type && *type != ';' &&
						    type <
							(char *)func_info.type +
						    sizeof (func_info.type) &&
						    found_sub_type == FALSE) {
							if (strncmp(type,
							    sub_type_arg,
							    len) == 0)
								found_sub_type
								    = TRUE;
							else
								type++;
						}
						if (found_sub_type == FALSE)
							/*
							 * Failed to match the
							 * requested sub_type.
							 * try the next
							 * function
							 */
							continue;
					}

					/*
					 * At this point, any/all keys have
					 * been matched so copies the slot
					 * structure (if not already copied)
					 * and function structure into the
					 * caller's data area.
					*/

					if (data == new_slot) {
						*((short *)data) = slot;
						data += sizeof (short);
						new_slot = data;
						bcopy((char *)&slot_info, data,
						    sizeof (NVM_SLOTINFO));
						((NVM_SLOTINFO *)new_slot)
						    ->functions = 0;
						data += sizeof (NVM_SLOTINFO);
					}

					bcopy((char *)&func_info, data,
					    sizeof (NVM_FUNCINFO));
					((NVM_SLOTINFO *)new_slot)->functions++;
					data += sizeof (NVM_FUNCINFO);

				}   /* end eisa_read_func */
			}   /* end for all functions */
		}   /* end eisa_read_slot */
	}   /* end for all slots */
	return ((int)(data - data_start));
}

static int
eisa_getnvm(regs *rp)
{
	int	slot;
	int	rc;

#ifdef KEEP_INT15
/*
 * The eisa_rom_call code is not ported but exists in 5.3 code (rom_call.s)
 * There does't seem to be much point porting it it the property stuff
 * works.
 */
/*	check for support for rom bios call init 15			*/
	if (eisa_int15)
		return (eisa_rom_call(rp));

#endif
	slot = rp->ecx.byte.cl;
	if (slot >= EISA_MAXSLOT) {
		return (rp->eax.byte.ah = NVM_INVALID_SLOT);
	}

	switch (rp->eax.word.ax) {
	case EISA_READ_SLOT_CONFIG:
		rc = eisa_getslot(rp);
		break;
	case EISA_READ_FUNC_CONFIG:
		rc = eisa_getfunc(rp);
		break;
	default:
		rc = 1;
		break;
	}
	return (rc);
}

static int
eisa_getslot(regs *rp)
{
	struct es_slot *es_slotp = (struct es_slot *)eisa_nvmp;

	es_slotp += rp->ecx.byte.cl;
	rp->eax.word.ax = es_slotp->es_slotinfo.eax.word.ax;
	rp->ebx.word.bx = es_slotp->es_slotinfo.ebx.word.bx;
	rp->ecx.word.cx = es_slotp->es_slotinfo.ecx.word.cx;
	rp->edx.word.dx = es_slotp->es_slotinfo.edx.word.dx;
	rp->esi.word.si = es_slotp->es_slotinfo.esi.word.si;
	rp->edi.word.di = es_slotp->es_slotinfo.edi.word.di;
	return ((int)es_slotp->es_slotinfo.eax.byte.ah);

}

static int
eisa_getfunc(regs *rp)
{
	struct es_func *es_funcp;
	struct es_slot *es_slotp = (struct es_slot *)eisa_nvmp;
	uint_t    func;

	es_slotp += rp->ecx.byte.cl;
	if (!es_slotp->es_funcoffset) {
		return (rp->eax.byte.ah = NVM_EMPTY_SLOT);
	}

	func = (uint_t)rp->ecx.byte.ch;
	if (func >= es_slotp->es_slotinfo.edx.byte.dh) {
		return (rp->eax.byte.ah = NVM_INVALID_FUNCTION);
	}

	es_funcp = (struct es_func *)(eisa_nvmp + es_slotp->es_funcoffset);
	es_funcp += func;
	rp->eax.word.ax = es_funcp->eax.word.ax;
	bcopy((caddr_t)es_funcp->ef_buf, (caddr_t)rp->esi.esi, EFBUFSZ);
	return ((int)es_funcp->eax.byte.ah);
}


/*
 * check if eisa machine
 */

int
envm_check(void)
{
	if (eisa_nvmlength != 0)	/* we have EISA data */
		return (DDI_SUCCESS);
	return (DDI_FAILURE);
}

#ifdef KEEP_INT15
eisa_enable_int15()
{
	eisa_int15 = 1;
}

eisa_disable_int15()
{
	eisa_int15 = 0;
}
#endif


/*
 * x86 Prom (Should be in a separate file)
 */

#ifdef DEBUG
int bootprop_debug = 0;
#endif

char *bootprop_ignore[] = {
	"memory-update",
	"virt-avail",
	"phys-avail",
	"phys-installed",
	(char *)0
};

static void
get_boot_properties(void)
{
	dev_info_t *devi;
	char *name;
	void *value;
	int length;
	char **ignore;
	extern struct bootops *bootops;
	extern char hw_provider[];
	char property_name[50], *tmp_name_ptr;

	/*
	 * Import "root" properties from the boot.
	 *
	 * We do this by invoking BOP_NEXTPROP until the list
	 * is completely copied in.
	 */

	devi = ddi_root_node();
	for (name = BOP_NEXTPROP(bootops, "");		/* get first */
	    name;					/* NULL => DONE */
	    name = BOP_NEXTPROP(bootops, name)) {	/* get next */

	/*
	 * Copy name to property_name, since name
	 * is in the low address range below kernelbase.
	 */

		{
			int i = 0;
			tmp_name_ptr = name;
			while (*tmp_name_ptr) {
				property_name[i] = *tmp_name_ptr++;
				i++;
			}
			property_name[i] = 0;
		}

		for (ignore = bootprop_ignore; *ignore; ++ignore) {
			if (strcmp(*ignore, property_name) == 0)
				break;
		}
		if (*ignore)
			continue;
		length = BOP_GETPROPLEN(bootops, property_name);
		if (length == 0)
			continue;
		/*
		 * special case for eisa nvram.  copy it to a special place
		 * Don't make it a property.
		 */
		if (strcmp(property_name, "eisa-nvram") == 0) {
			if ((value = kmem_zalloc(length, KM_NOSLEEP)) ==
								(void *)NULL)
				cmn_err(CE_PANIC,
						"no memory for EISA NVRAM");
			BOP_GETPROP(bootops, property_name, value);
			eisa_nvmp = (caddr_t)value;
			eisa_nvmlength = length;
			/* done with this.. go around again */
			continue;
		}
		if ((value = kmem_alloc(length, KM_NOSLEEP)) == (void *)NULL)
			cmn_err(CE_PANIC, "no memory for root properties");
		BOP_GETPROP(bootops, property_name, value);
#ifdef DEBUG
	if (bootprop_debug)
		if (length != 4)
			prom_printf("root property '%s' = '%s'\n",
					property_name, value);
		else
			prom_printf("root property '%s' = 0x%x\n",
					property_name, *(int *)value);
#endif
		if (strcmp(name, "si-machine") == 0) {
			(void) strncpy(utsname.machine, value, SYS_NMLN);
			utsname.machine[SYS_NMLN - 1] = (char)NULL;
		} else if (strcmp(name, "si-hw-provider") == 0) {
			(void) strncpy(hw_provider, value, SYS_NMLN);
			hw_provider[SYS_NMLN - 1] = (char)NULL;
		} else
			(void) e_ddi_prop_update_byte_array(DDI_DEV_T_NONE,
			    devi, property_name, (uchar_t *)value,
			    (uint_t)length);
		kmem_free(value, length);
	}
#ifdef DEBUG
	if (bootprop_debug)
		int20();
#endif
}

/*
 * Someday this add/delete pair might manipulate a list.
 * Today they manipulate a one-item list.
 */
void
impl_bus_add_promops(struct impl_bus_promops *ops_p)
{
	impl_busp = ops_p;
}

/*ARGSUSED*/
void
impl_bus_delete_promops(struct impl_bus_promops *ops_p)
{
	impl_busp = NULL;
}

/*
 * impl_bus_initialprobe
 *	Modload the prom simulator, then let it probe to verify existence
 *	and type of PCI support.
 */
void
impl_bus_initialprobe(void)
{
	(void) modload("misc", "pci_autoconfig");
	if (impl_busp != (struct impl_bus_promops *)NULL) {
		(void) impl_busp->ib_probe();
	}
}

/*
 * This is temporary, but absolutely necessary.  If we are being
 * booted with a device tree created by the DevConf project's bootconf
 * program, then we have device information nodes that reflect
 * reality.  At this point in time in the Solaris release schedule, the
 * kernel drivers aren't prepared for reality.  They still depend on their
 * own ad-hoc interpretations of the properties created when their .conf
 * files were interpreted. These drivers use an "ignore-hardware-nodes"
 * property to prevent them from using the nodes passed up from the bootconf
 * device tree.
 *
 * Trying to assemble root file system drivers as we are booting from
 * devconf will fail if the kernel driver is basing its name_addr's on the
 * psuedo-node device info while the bootpath passed up from bootconf is using
 * reality-based name_addrs.  We help the boot along in this case by
 * looking at the pre-bootconf bootpath and determining if we would have
 * successfully matched if that had been the bootpath we had chosen.
 *
 * Note that we only even perform this extra check if we've booted
 * using bootconf's 1275 compliant bootpath, this is the boot device, and
 * we're trying to match the name_addr specified in the 1275 bootpath.
 */

#define	MAXCOMPONENTLEN	32

int
x86_old_bootpath_name_addr_match(dev_info_t *cdip, char *caddr, char *naddr)
{
	/*
	 *  There are multiple criteria to be met before we can even
	 *  consider allowing a name_addr match here.
	 *
	 *  1) We must have been booted such that the bootconf program
	 *	created device tree nodes and properties.  This can be
	 *	determined by examining the 'bootpath' property.  This
	 *	property will be a non-null string iff bootconf was
	 *	involved in the boot.
	 *
	 *  2) The module that we want to match must be the boot device.
	 *
	 *  3) The instance of the module we are thinking of letting be
	 *	our match must be ignoring hardware nodes.
	 *
	 *  4) The name_addr we want to match must be the name_addr
	 *	specified in the 1275 bootpath.
	 */
	static char bootdev_module[MAXCOMPONENTLEN];
	static char bootdev_oldmod[MAXCOMPONENTLEN];
	static char bootdev_newaddr[MAXCOMPONENTLEN];
	static char bootdev_oldaddr[MAXCOMPONENTLEN];
	static int  quickexit;

	char *daddr;
	int dlen;

	char	*lkupname;
	int	rv = DDI_FAILURE;

	if ((ddi_getlongprop(DDI_DEV_T_ANY, cdip, DDI_PROP_DONTPASS,
		"devconf-addr", (caddr_t)&daddr, &dlen) == DDI_PROP_SUCCESS) &&
	    (ddi_getprop(DDI_DEV_T_ANY, cdip, DDI_PROP_DONTPASS,
		"ignore-hardware-nodes", -1) != -1)) {
		if (strcmp(daddr, caddr) == 0) {
			return (DDI_SUCCESS);
		}
	}

	if (quickexit)
		return (rv);

	if (bootdev_module[0] == '\0') {
		char *addrp, *eoaddrp;
		char *busp, *modp, *atp;
		char *bp1275, *bp;
		int  bp1275len, bplen;

		bp1275 = bp = addrp = eoaddrp = busp = modp = atp = NULL;

		if (ddi_getlongprop(DDI_DEV_T_ANY,
		    ddi_root_node(), 0, "bootpath",
		    (caddr_t)&bp1275, &bp1275len) != DDI_PROP_SUCCESS ||
		    bp1275len <= 1) {
			/*
			 * We didn't boot from bootconf so we never need to
			 * do any special matches.
			 */
			quickexit = 1;
			if (bp1275)
				kmem_free(bp1275, bp1275len);
			return (rv);
		}

		if (ddi_getlongprop(DDI_DEV_T_ANY,
		    ddi_root_node(), 0, "boot-path",
		    (caddr_t)&bp, &bplen) != DDI_PROP_SUCCESS || bplen <= 1) {
			/*
			 * No fallback position for matching. This is
			 * certainly unexpected, but we'll handle it
			 * just in case.
			 */
			quickexit = 1;
			kmem_free(bp1275, bp1275len);
			if (bp)
				kmem_free(bp, bplen);
			return (rv);
		}

		/*
		 *  Determine boot device module and 1275 name_addr
		 *
		 *  bootpath assumed to be of the form /bus/module@name_addr
		 */
		if (busp = strchr(bp1275, '/')) {
			if (modp = strchr(busp + 1, '/')) {
				if (atp = strchr(modp + 1, '@')) {
					*atp = '\0';
					addrp = atp + 1;
					if (eoaddrp = strchr(addrp, '/'))
						*eoaddrp = '\0';
				}
			}
		}

		if (modp && addrp) {
			(void) strncpy(bootdev_module, modp + 1,
			    MAXCOMPONENTLEN);
			bootdev_module[MAXCOMPONENTLEN - 1] = '\0';

			(void) strncpy(bootdev_newaddr, addrp, MAXCOMPONENTLEN);
			bootdev_newaddr[MAXCOMPONENTLEN - 1] = '\0';
		} else {
			quickexit = 1;
			kmem_free(bp1275, bp1275len);
			kmem_free(bp, bplen);
			return (rv);
		}

		/*
		 *  Determine fallback name_addr
		 *
		 *  10/3/96 - Also save fallback module name because it
		 *  might actually be different than the current module
		 *  name.  E.G., ISA pnp drivers have new names.
		 *
		 *  bootpath assumed to be of the form /bus/module@name_addr
		 */
		addrp = NULL;
		if (busp = strchr(bp, '/')) {
			if (modp = strchr(busp + 1, '/')) {
				if (atp = strchr(modp + 1, '@')) {
					*atp = '\0';
					addrp = atp + 1;
					if (eoaddrp = strchr(addrp, '/'))
						*eoaddrp = '\0';
				}
			}
		}

		if (modp && addrp) {
			(void) strncpy(bootdev_oldmod, modp + 1,
			    MAXCOMPONENTLEN);
			bootdev_module[MAXCOMPONENTLEN - 1] = '\0';

			(void) strncpy(bootdev_oldaddr, addrp, MAXCOMPONENTLEN);
			bootdev_oldaddr[MAXCOMPONENTLEN - 1] = '\0';
		}

		/* Free up the bootpath storage now that we're done with it. */
		kmem_free(bp1275, bp1275len);
		kmem_free(bp, bplen);

		if (bootdev_oldaddr[0] == '\0') {
			quickexit = 1;
			return (rv);
		}
	}

	if (((lkupname = ddi_get_name(cdip)) != NULL) &&
	    (strcmp(bootdev_module, lkupname) == 0 ||
	    strcmp(bootdev_oldmod, lkupname) == 0) &&
	    ((ddi_getprop(DDI_DEV_T_ANY, cdip, DDI_PROP_DONTPASS,
		"ignore-hardware-nodes", -1) != -1) ||
		ignore_hardware_nodes) &&
	    strcmp(bootdev_newaddr, caddr) == 0 &&
	    strcmp(bootdev_oldaddr, naddr) == 0) {
		rv = DDI_SUCCESS;
	}

	return (rv);
}
