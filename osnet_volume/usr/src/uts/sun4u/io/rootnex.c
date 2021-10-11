/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rootnex.c	1.87	99/12/16 SMI"

/*
 * sun4u root nexus driver
 *
 * XXX	Now that we no longer handle DMA in this nexus
 *	many of the includes below should be omitted.
 */

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/devops.h>
#include <sys/sunddi.h>
#include <sys/obpdefs.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_implfuncs.h>
#include <sys/kmem.h>
#include <sys/vmem.h>
#include <sys/cmn_err.h>
#include <vm/seg_kmem.h>
#include <vm/seg_dev.h>
#include <sys/machparam.h>
#include <sys/cpuvar.h>
#include <sys/ivintr.h>
#include <sys/ddi_subrdefs.h>
#include <sys/modctl.h>
#include <sys/errno.h>
#include <sys/spl.h>
#include <sys/sysiosbus.h>
#include <sys/machsystm.h>
#include <sys/vmsystm.h>
#include <sys/nexusintr.h>
#include <sys/privregs.h>
#include <sys/sunndi.h>
#include <sys/intreg.h>

/* Useful debugging Stuff */
#include <sys/nexusdebug.h>
#define	ROOTNEX_MAP_DEBUG		0x1
#define	ROOTNEX_INTR_DEBUG		0x2

/*
 * config information
 */

static int
rootnex_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
    off_t offset, off_t len, caddr_t *vaddrp);

static void
rootnex_get_intr_impl(dev_info_t *dip, dev_info_t *rdip, uint_t inumber,
    ddi_ispec_t **ispecp);

static int
rootnex_add_intr_impl(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_info_t *intr_info);

static void
rootnex_remove_intr_impl(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_info_t *intr_info);

static int
rootnex_intr_ctlops(dev_info_t *dip, dev_info_t *rdip, ddi_intr_ctlop_t ctlop,
    void *arg, void *result);

static int
rootnex_map_fault(dev_info_t *dip, dev_info_t *rdip,
    struct hat *hat, struct seg *seg, caddr_t addr,
    struct devpage *dp, uint_t pfn, uint_t prot, uint_t lock);

static int
rootnex_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);

static struct cb_ops rootnex_cb_ops = {
	nodev,		/* open */
	nodev,		/* close */
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,		/* read */
	nodev,		/* write */
	nodev,		/* ioctl */
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* chpoll */
	ddi_prop_op,	/* cb_prop_op */
	NULL,		/* struct streamtab */
	D_NEW | D_MP | D_HOTPLUG,	/* compatibility flags */
	CB_REV,		/* Rev */
	nodev,		/* cb_aread */
	nodev		/* cb_awrite */
};

static struct bus_ops rootnex_bus_ops = {
	BUSO_REV,
	rootnex_map,
	NULL,
	NULL,
	NULL,
	rootnex_map_fault,
	ddi_no_dma_map,		/* no rootnex_dma_map- now in sysio nexus */
	ddi_no_dma_allochdl,
	ddi_no_dma_freehdl,
	ddi_no_dma_bindhdl,
	ddi_no_dma_unbindhdl,
	ddi_no_dma_flush,
	ddi_no_dma_win,
	ddi_no_dma_mctl,	/* no rootnex_dma_mctl- now in sysio nexus */
	rootnex_ctlops,
	ddi_bus_prop_op,
	i_ddi_rootnex_get_eventcookie,
	i_ddi_rootnex_add_eventcall,
	i_ddi_rootnex_remove_eventcall,
	i_ddi_rootnex_post_event,
	rootnex_intr_ctlops
};

static int rootnex_identify(dev_info_t *devi);
static int rootnex_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int rootnex_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);

static struct dev_ops rootnex_ops = {
	DEVO_REV,
	0,		/* refcnt */
	ddi_no_info,	/* info */
	rootnex_identify,
	0,		/* probe */
	rootnex_attach,
	rootnex_detach,
	nodev,		/* reset */
	&rootnex_cb_ops,
	&rootnex_bus_ops
};


extern uint_t	root_phys_addr_lo_mask;
extern struct mod_ops mod_driverops;
extern struct dev_ops rootnex_ops;
extern struct cpu cpu0;

/*
 * Add statically defined root properties to this list...
 */
static const int pagesize = PAGESIZE;
static const int mmu_pagesize = MMU_PAGESIZE;
static const int mmu_pageoffset = MMU_PAGEOFFSET;

struct prop_def {
	char *prop_name;
	caddr_t prop_value;
};

static struct prop_def root_props[] = {
	{ "PAGESIZE",		(caddr_t)&pagesize },
	{ "MMU_PAGESIZE",	(caddr_t)&mmu_pagesize},
	{ "MMU_PAGEOFFSET",	(caddr_t)&mmu_pageoffset},
};

#define	NROOT_PROPS	(sizeof (root_props) / sizeof (struct prop_def))

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops,	/* Type of module.  This one is a nexus driver */
	"sun4u root nexus 1.87",
	&rootnex_ops,	/* Driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * rootnex_identify:
 *
 * 	identify the root nexus.
 */
static int
rootnex_identify(dev_info_t *devi)
{
	if (ddi_root_node() == devi)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

/*
 * rootnex_attach:
 *
 *	attach the root nexus.
 */
static void add_root_props(dev_info_t *);

/*ARGSUSED*/
static int
rootnex_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int length;
	char *valuep = 0;

	add_root_props(devi);

	if (ddi_prop_op(DDI_DEV_T_ANY, devi, PROP_LEN_AND_VAL_ALLOC,
	    DDI_PROP_DONTPASS, "banner-name", (caddr_t)&valuep, &length)
	    == DDI_PROP_SUCCESS) {
		cmn_err(CE_CONT, "?root nexus = %s\n", valuep);
		kmem_free((void *)valuep, (size_t)length);
	}
	/*
	 * Add a no-suspend-resume property so that NDI
	 * does not attempt to suspend/resume the rootnex
	 * (or any of its aliases) node.
	 */
	(void) ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
	    "pm-hardware-state", "no-suspend-resume",
	    strlen("no-suspend-resume") + 1);

	i_ddi_rootnex_init_events(devi);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
rootnex_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	return (DDI_SUCCESS);
}

static void
add_root_props(dev_info_t *devi)
{
	int i;
	struct prop_def *rpp;

	/*
	 * Note that this for loop works because all of the
	 * properties in root_prop are integers
	 */
	for (i = 0, rpp = root_props; i < NROOT_PROPS; ++i, ++rpp) {
		(void) e_ddi_prop_update_int(DDI_DEV_T_NONE, devi,
		    rpp->prop_name, *((int *)rpp->prop_value));
	}

	/*
	 * Create the root node "boolean" property
	 * corresponding to addressing type supported in the root node:
	 *
	 * Choices are:
	 *	"relative-addressing" (OBP PROMS)
	 *	"generic-addressing"  (SunMon -- pseudo OBP/DDI)
	 */

	(void) e_ddi_prop_update_int(DDI_DEV_T_NONE, devi,
	    DDI_RELATIVE_ADDRESSING, 1);
}

static int
rootnex_map_regspec(ddi_map_req_t *mp, caddr_t *vaddrp, uint_t mapping_attr)
{
	ulong_t base;
	caddr_t kaddr;
	pgcnt_t npages;
	pfn_t 	pfn;
	uint_t 	pgoffset;
	struct regspec *rp = mp->map_obj.rp;

	base = (ulong_t)rp->regspec_addr & (~MMU_PAGEOFFSET); /* base addr */

	/*
	 * Take the bustype and the physical page base within the
	 * bus space and turn it into a 28 bit page frame number.
	 */

	pfn = BUSTYPE_TO_PFN(rp->regspec_bustype, mmu_btop(base));


	/*
	 * Do a quick sanity check to make sure we are in I/O space.
	 */
	if (pf_is_memory(pfn))
		return (DDI_ME_INVAL);

	if (rp->regspec_size == 0) {
		DPRINTF(ROOTNEX_MAP_DEBUG, ("rootnex_map_regspec: zero "
		    "regspec_size\n"));
		return (DDI_ME_INVAL);
	}

	if (mp->map_flags & DDI_MF_DEVICE_MAPPING)
		*vaddrp = (caddr_t)pfn;
	else {
		pgoffset = (ulong_t)rp->regspec_addr & MMU_PAGEOFFSET;
		npages = mmu_btopr(rp->regspec_size + pgoffset);

		DPRINTF(ROOTNEX_MAP_DEBUG, ("rootnex_map_regspec: Mapping "
		    "%lu pages physical %x.%lx ", npages, rp->regspec_bustype,
		    base));

		kaddr = vmem_alloc(heap_arena, ptob(npages), VM_NOSLEEP);
		if (kaddr == NULL)
			return (DDI_ME_NORESOURCES);

		/*
		 * Now map in the pages we've allocated...
		 */
		hat_devload(kas.a_hat, kaddr, ptob(npages), pfn,
		    mp->map_prot | mapping_attr, HAT_LOAD_LOCK);

		*vaddrp = kaddr + pgoffset;
	}

	DPRINTF(ROOTNEX_MAP_DEBUG, ("at virtual 0x%x\n", *vaddrp));
	return (0);
}

static int
rootnex_unmap_regspec(ddi_map_req_t *mp, caddr_t *vaddrp)
{
	caddr_t addr = *vaddrp;
	pgcnt_t npages;
	uint_t  pgoffset;
	caddr_t base;
	struct regspec *rp;

	if (mp->map_flags & DDI_MF_DEVICE_MAPPING)
		return (0);

	rp = mp->map_obj.rp;
	pgoffset = (uintptr_t)addr & MMU_PAGEOFFSET;

	if (rp->regspec_size == 0) {
		DPRINTF(ROOTNEX_MAP_DEBUG, ("rootnex_unmap_regspec: "
		    "zero regspec_size\n"));
		return (DDI_ME_INVAL);
	}

	base = addr - pgoffset;
	npages = mmu_btopr(rp->regspec_size + pgoffset);
	hat_unload(kas.a_hat, base, ptob(npages), HAT_UNLOAD_UNLOCK);
	vmem_free(heap_arena, base, ptob(npages));

	/*
	 * Destroy the pointer - the mapping has logically gone
	 */
	*vaddrp = (caddr_t)0;

	return (0);
}

static int
rootnex_map_handle(ddi_map_req_t *mp)
{
	ddi_acc_hdl_t *hp;
	uint_t hat_flags;
	register struct regspec *rp;

	/*
	 * Set up the hat_flags for the mapping.
	 */
	hp = mp->map_handlep;

	switch (hp->ah_acc.devacc_attr_endian_flags) {
	case DDI_NEVERSWAP_ACC:
		hat_flags = HAT_NEVERSWAP | HAT_STRICTORDER;
		break;
	case DDI_STRUCTURE_BE_ACC:
		hat_flags = HAT_STRUCTURE_BE;
		break;
	case DDI_STRUCTURE_LE_ACC:
		hat_flags = HAT_STRUCTURE_LE;
		break;
	default:
		return (DDI_REGS_ACC_CONFLICT);
	}

	switch (hp->ah_acc.devacc_attr_dataorder) {
	case DDI_STRICTORDER_ACC:
		break;
	case DDI_UNORDERED_OK_ACC:
		hat_flags |= HAT_UNORDERED_OK;
		break;
	case DDI_MERGING_OK_ACC:
		hat_flags |= HAT_MERGING_OK;
		break;
	case DDI_LOADCACHING_OK_ACC:
		hat_flags |= HAT_LOADCACHING_OK;
		break;
	case DDI_STORECACHING_OK_ACC:
		hat_flags |= HAT_STORECACHING_OK;
		break;
	default:
		return (DDI_FAILURE);
	}

	rp = mp->map_obj.rp;
	if (rp->regspec_size == 0)
		return (DDI_ME_INVAL);

	hp->ah_hat_flags = hat_flags;
	hp->ah_pfn = mmu_btop((ulong_t)rp->regspec_addr & (~MMU_PAGEOFFSET));
	hp->ah_pnum = mmu_btopr(rp->regspec_size +
	    (ulong_t)rp->regspec_addr & MMU_PAGEOFFSET);
	return (DDI_SUCCESS);
}

static int
rootnex_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
    off_t offset, off_t len, caddr_t *vaddrp)
{
	struct regspec *rp, tmp_reg;
	ddi_map_req_t mr = *mp;		/* Get private copy of request */
	int error;
	uint_t mapping_attr;
	ddi_acc_hdl_t *hp = NULL;

	mp = &mr;

	switch (mp->map_op)  {
	case DDI_MO_MAP_LOCKED:
	case DDI_MO_UNMAP:
	case DDI_MO_MAP_HANDLE:
		break;
	default:
		DPRINTF(ROOTNEX_MAP_DEBUG, ("rootnex_map: unimplemented map "
		    "op %d.", mp->map_op));
		return (DDI_ME_UNIMPLEMENTED);
	}

	if (mp->map_flags & DDI_MF_USER_MAPPING)  {
		DPRINTF(ROOTNEX_MAP_DEBUG, ("rootnex_map: unimplemented map "
		    "type: user."));
		return (DDI_ME_UNIMPLEMENTED);
	}

	/*
	 * First, if given an rnumber, convert it to a regspec...
	 * (Presumably, this is on behalf of a child of the root node?)
	 */

	if (mp->map_type == DDI_MT_RNUMBER)  {

		int rnumber = mp->map_obj.rnumber;

		rp = i_ddi_rnumber_to_regspec(rdip, rnumber);
		if (rp == (struct regspec *)0)  {
			DPRINTF(ROOTNEX_MAP_DEBUG, ("rootnex_map: Out of "
			    "range rnumber <%d>, device <%s>", rnumber,
			    ddi_get_name(rdip)));
			return (DDI_ME_RNUMBER_RANGE);
		}

		/*
		 * Convert the given ddi_map_req_t from rnumber to regspec...
		 */

		mp->map_type = DDI_MT_REGSPEC;
		mp->map_obj.rp = rp;
	}

	/*
	 * Adjust offset and length correspnding to called values...
	 * XXX: A non-zero length means override the one in the regspec
	 * XXX: regardless of what's in the parent's range?.
	 */

	tmp_reg = *(mp->map_obj.rp);		/* Preserve underlying data */
	rp = mp->map_obj.rp = &tmp_reg;		/* Use tmp_reg in request */

	rp->regspec_addr += (uint_t)offset;
	if (len != 0)
		rp->regspec_size = (uint_t)len;

	/*
	 * Apply any parent ranges at this level, if applicable.
	 * (This is where nexus specific regspec translation takes place.
	 * Use of this function is implicit agreement that translation is
	 * provided via ddi_apply_range.)
	 */

	DPRINTF(ROOTNEX_MAP_DEBUG, ("rootnex_map: applying range of parent "
	    "<%s> to child <%s>...\n", ddi_get_name(dip), ddi_get_name(rdip)));

	if ((error = i_ddi_apply_range(dip, rdip, mp->map_obj.rp)) != 0)
		return (error);

	switch (mp->map_op)  {
	case DDI_MO_MAP_LOCKED:

		/*
		 * Set up the locked down kernel mapping to the regspec...
		 */

		/*
		 * If we were passed an access handle we need to determine
		 * the "endian-ness" of the mapping and fill in the handle.
		 */
		if (mp->map_handlep) {
			hp = mp->map_handlep;
			switch (hp->ah_acc.devacc_attr_endian_flags) {
			case DDI_NEVERSWAP_ACC:
				mapping_attr = HAT_NEVERSWAP | HAT_STRICTORDER;
				break;
			case DDI_STRUCTURE_BE_ACC:
				mapping_attr = HAT_STRUCTURE_BE;
				break;
			case DDI_STRUCTURE_LE_ACC:
				mapping_attr = HAT_STRUCTURE_LE;
				break;
			default:
				return (DDI_REGS_ACC_CONFLICT);
			}

			switch (hp->ah_acc.devacc_attr_dataorder) {
			case DDI_STRICTORDER_ACC:
				break;
			case DDI_UNORDERED_OK_ACC:
				mapping_attr |= HAT_UNORDERED_OK;
				break;
			case DDI_MERGING_OK_ACC:
				mapping_attr |= HAT_MERGING_OK;
				break;
			case DDI_LOADCACHING_OK_ACC:
				mapping_attr |= HAT_LOADCACHING_OK;
				break;
			case DDI_STORECACHING_OK_ACC:
				mapping_attr |= HAT_STORECACHING_OK;
				break;
			default:
				return (DDI_REGS_ACC_CONFLICT);
			}
		} else {
			mapping_attr = HAT_NEVERSWAP | HAT_STRICTORDER;
		}

		/*
		 * Set up the mapping.
		 */
		error = rootnex_map_regspec(mp, vaddrp, mapping_attr);

		/*
		 * Fill in the access handle if needed.
		 */
		if (hp) {
			hp->ah_addr = *vaddrp;
			hp->ah_hat_flags = mapping_attr;
			if (error == 0)
				impl_acc_hdl_init(hp);
		}
		return (error);

	case DDI_MO_UNMAP:

		/*
		 * Release mapping...
		 */

		return (rootnex_unmap_regspec(mp, vaddrp));

	case DDI_MO_MAP_HANDLE:
		return (rootnex_map_handle(mp));

	}

	return (DDI_ME_UNIMPLEMENTED);
}


/*ARGSUSED*/
static void
rootnex_get_intr_impl(dev_info_t *dip, dev_info_t *rdip, uint_t inumber,
    ddi_ispec_t **ispecp)
{
	int32_t portid;


	i_ddi_alloc_ispec(rdip, inumber, (ddi_intrspec_t *)ispecp);

	if (((portid = ddi_prop_get_int(DDI_DEV_T_ANY, rdip,
	    DDI_PROP_DONTPASS, "upa-portid", -1)) != -1) ||
	    ((portid = ddi_prop_get_int(DDI_DEV_T_ANY, rdip,
		DDI_PROP_DONTPASS, "portid", -1)) != -1)) {
		if (ddi_getprop(DDI_DEV_T_ANY, rdip, DDI_PROP_DONTPASS,
		    "upa-interrupt-slave", 0) != 0) {
			/* Give slave devices pri of 5. e.g. fb's */
			(*ispecp)->is_pil = 5;
		}
		/*
		 * Translate the interrupt property by stuffing in the portid
		 * for those devices which have a portid.
		 */
		*((*ispecp)->is_intr) |= (UPAID_TO_IGN(portid) << 6);
	}

	DPRINTF(ROOTNEX_INTR_DEBUG, ("rootnex_get_intr_impl: device %s%d "
	    "ispecp 0x%p\n", ddi_get_name(rdip), ddi_get_instance(rdip),
	    *ispecp));
}

/*
 * rootnex_add_intrspec:
 *
 *	Add an interrupt specification.
 */
/*ARGSUSED*/
static int
rootnex_add_intr_impl(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_info_t *intr_info)
{
	ddi_iblock_cookie_t *iblock_cookiep = intr_info->ii_iblock_cookiep;
	ddi_idevice_cookie_t *idevice_cookiep =
	    intr_info->ii_idevice_cookiep;
	uint_t (*int_handler)(caddr_t int_handler_arg) =
	    intr_info->ii_int_handler;
	caddr_t int_handler_arg = intr_info->ii_int_handler_arg;
	int32_t kind = intr_info->ii_kind;
	uint32_t pil;

	switch (kind) {
	case IDDI_INTR_TYPE_FAST:
		return (DDI_FAILURE);

	case IDDI_INTR_TYPE_SOFT: {
		ddi_softispec_t *ispecp =
		    (ddi_softispec_t *)intr_info->ii_ispec;
		uint_t rval;

		if ((rval = (uint_t)add_softintr(ispecp->sis_pil, int_handler,
		    int_handler_arg)) == 0)
			return (DDI_FAILURE);

		ispecp->sis_softint_id = rval;
		pil = ispecp->sis_pil;

		break;
	}

	case IDDI_INTR_TYPE_NORMAL: {
		ddi_ispec_t *ispecp = (ddi_ispec_t *)intr_info->ii_ispec;
		struct intr_vector intr_node;
		volatile uint64_t *intr_mapping_reg;
		volatile uint64_t mondo_vector;
		int32_t r_upaid = -1;
		int32_t slave = 0;
		int32_t len;

		/*
		 * Hack to support the UPA slave devices before the 1275
		 * support for imap was introduced.
		 */
		if (ddi_getproplen(DDI_DEV_T_ANY, dip, NULL, "interrupt-map",
		    &len) != DDI_PROP_SUCCESS &&
		    ddi_getprop(DDI_DEV_T_ANY, rdip, DDI_PROP_DONTPASS,
			"upa-interrupt-slave", 0) != 0 &&
		    ddi_get_parent(rdip) == dip) {
			slave = 1;

			if ((r_upaid = ddi_prop_get_int(DDI_DEV_T_ANY, rdip,
			    DDI_PROP_DONTPASS, "upa-portid", -1)) != -1) {
				extern uint64_t *
				    get_intr_mapping_reg(int, int);

				if ((intr_mapping_reg = get_intr_mapping_reg(
				    r_upaid, 1)) == NULL)
					return (DDI_FAILURE);
			} else
				return (DDI_FAILURE);
		}

		/* Sanity check the entry we're about to add */
		rem_ivintr(*ispecp->is_intr, &intr_node);

		if (intr_node.iv_handler) {
			cmn_err(CE_WARN, "UPA device mondo 0x%x in use\n",
			    *ispecp->is_intr);
			return (DDI_FAILURE);
		}

		/*
		 * If the PIL was set and is valid use it, otherwise
		 * default it to 1
		 */
		pil = ispecp->is_pil;
		if ((ispecp->is_pil < 1) || (ispecp->is_pil > PIL_MAX))
			pil = ispecp->is_pil = 1;

		add_ivintr(*ispecp->is_intr, pil, int_handler,
		    int_handler_arg);

		/*
		 * Hack to support the UPA slave devices before the 1275
		 * support for imap was introduced.
		 */
		if (slave) {
			/*
			 * Program the interrupt mapping register.
			 * Interrupts from the slave UPA devices are
			 * directed at the boot CPU until it is known
			 * that they can be safely redirected while
			 * running under load.
			 */
			mondo_vector = cpu0.cpu_id << IMR_TID_SHIFT;
			mondo_vector |=
			    (IMR_VALID | (uint64_t)*ispecp->is_intr);

				/* Set the mapping register */
			*intr_mapping_reg = mondo_vector;

				/* Flush write buffers */
			mondo_vector = *intr_mapping_reg;

		}

		break;
	}

	default:
		return (DDI_INTR_NOTFOUND);
	}

	/* Program the iblock cookie */
	if (iblock_cookiep) {
		*iblock_cookiep =
		    (ddi_iblock_cookie_t)ipltospl(pil);
	}

	/* Program the device cookie */
	if (idevice_cookiep) {
		idevice_cookiep->idev_vector = 0;
		idevice_cookiep->idev_priority = pil;
	}

	return (DDI_SUCCESS);
}

/*
 * rootnex_remove_intrspec:
 *
 *	remove an interrupt specification.
 *
 */
/*ARGSUSED*/
static void
rootnex_remove_intr_impl(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_info_t *intr_info)
{
	int32_t kind = intr_info->ii_kind;

	switch (kind) {
	case IDDI_INTR_TYPE_SOFT: {
		ddi_softispec_t *ispecp =
		    (ddi_softispec_t *)intr_info->ii_ispec;
		rem_softintr(ispecp->sis_softint_id);
		return;
	}

	case IDDI_INTR_TYPE_NORMAL: {
		ddi_ispec_t *ispecp = (ddi_ispec_t *)intr_info->ii_ispec;
		struct intr_vector int_vec;
		int len;

		/*
		 * Hack to support the UPA slave devices before the 1275
		 * support for imap was introduced.
		 */
		if (ddi_getproplen(DDI_DEV_T_ANY, dip, NULL, "interrupt-map",
		    &len) != DDI_PROP_SUCCESS &&
		    ddi_getprop(DDI_DEV_T_ANY, rdip, DDI_PROP_DONTPASS,
			"upa-interrupt-slave", 0) != 0) {
			int32_t r_upaid = -1;

			if ((r_upaid = ddi_prop_get_int(DDI_DEV_T_ANY, rdip,
			    DDI_PROP_DONTPASS, "upa-portid", -1)) != -1 &&
			    ddi_get_parent(rdip) == dip) {
				volatile uint64_t *intr_mapping_reg;
				volatile uint64_t flush_data;
				extern uint64_t *
				    get_intr_mapping_reg(int, int);

				if ((intr_mapping_reg = get_intr_mapping_reg(
				    r_upaid, 1)) == NULL)
					return;

				/* Clear the mapping register */
				*intr_mapping_reg = 0x0ull;

				/* Flush write buffers */
				flush_data = *intr_mapping_reg;
#ifdef lint
				flush_data = flush_data;
#endif lint
			}
		}

		rem_ivintr(*ispecp->is_intr, &int_vec);
		ispecp->is_pil = int_vec.iv_pil;
		intr_info->ii_int_handler = int_vec.iv_handler;
		intr_info->ii_int_handler_arg = int_vec.iv_arg;
	}
	}
}

static int
rootnex_intr_ctlops(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_ctlop_t op, void *arg, void *result)
{
	int32_t rval = DDI_SUCCESS;

	switch (op) {
	case DDI_INTR_CTLOPS_ALLOC_ISPEC:
		rootnex_get_intr_impl(dip, rdip, (uint_t)arg,
		    (ddi_ispec_t **)result);
		break;

	case DDI_INTR_CTLOPS_FREE_ISPEC:
		i_ddi_free_ispec((ddi_intrspec_t)arg);
		break;

	case DDI_INTR_CTLOPS_ADD:
		rval = rootnex_add_intr_impl(dip, rdip,
		    (ddi_intr_info_t *)arg);
		break;

	case DDI_INTR_CTLOPS_REMOVE:
		rootnex_remove_intr_impl(dip, rdip, (ddi_intr_info_t *)arg);

		break;

	case DDI_INTR_CTLOPS_NINTRS:
		*(int *)result = i_ddi_get_nintrs(rdip);
		break;

	case DDI_INTR_CTLOPS_HILEVEL: {
		ddi_intr_info_t *intr_info = (ddi_intr_info_t *)arg;
		ddi_ispec_t *ispecp = (ddi_ispec_t *)intr_info->ii_ispec;
		/*
		 * Indicate whether the interrupt specified is to be handled
		 * above lock level.  In other words, above the level that
		 * cv_signal and default type mutexes can be used.
		 */
		if (ispecp->is_pil < 1 && ispecp->is_pil > PIL_MAX) {
			*(int *)result = 0;
			rval = DDI_FAILURE;
		} else {
			*(int *)result = (ispecp->is_pil > LOCK_LEVEL);
		}
		break;
	}
	default:
		rval = DDI_FAILURE;
	}

	return (rval);
}


/*
 * Shorthand defines
 */

#define	DMAOBJ_PP_PP	dmao_obj.pp_obj.pp_pp
#define	DMAOBJ_PP_OFF	dmao_ogj.pp_obj.pp_offset
#define	ALO		dma_lim->dlim_addr_lo
#define	AHI		dma_lim->dlim_addr_hi
#define	OBJSIZE		dmareq->dmar_object.dmao_size
#define	ORIGVADDR	dmareq->dmar_object.dmao_obj.virt_obj.v_addr
#define	RED		((mp->dmai_rflags & DDI_DMA_REDZONE)? 1 : 0)
#define	DIRECTION	(mp->dmai_rflags & DDI_DMA_RDWR)

/*
 * rootnex_map_fault:
 *
 *	fault in mappings for requestors
 */

/*ARGSUSED*/
static int
rootnex_map_fault(dev_info_t *dip, dev_info_t *rdip,
    struct hat *hat, struct seg *seg, caddr_t addr,
    struct devpage *dp, uint_t pfn, uint_t prot, uint_t lock)
{
	extern struct seg_ops segdev_ops;

	DPRINTF(ROOTNEX_MAP_DEBUG, ("rootnex_map_fault: address <%x> pfn <%x>",
	    addr, pfn));
	DPRINTF(ROOTNEX_MAP_DEBUG, (" Seg <%s>\n",
	    seg->s_ops == &segdev_ops ? "segdev" :
	    seg == &kvseg ? "segkmem" : "NONE!"));

	/*
	 * This is all terribly broken, but it is a start
	 *
	 * XXX	Note that this test means that segdev_ops
	 *	must be exported from seg_dev.c.
	 * XXX	What about devices with their own segment drivers?
	 */
	if (seg->s_ops == &segdev_ops) {
		register struct segdev_data *sdp =
		    (struct segdev_data *)seg->s_data;

		if (hat == NULL) {
			/*
			 * This is one plausible interpretation of
			 * a null hat i.e. use the first hat on the
			 * address space hat list which by convention is
			 * the hat of the system MMU.  At alternative
			 * would be to panic .. this might well be better ..
			 */
			ASSERT(AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));
			hat = seg->s_as->a_hat;
			cmn_err(CE_NOTE, "rootnex_map_fault: nil hat");
		}
		hat_devload(hat, addr, MMU_PAGESIZE, pfn, prot | sdp->hat_attr,
		    (lock ? HAT_LOAD_LOCK : HAT_LOAD));
	} else if (seg == &kvseg && dp == (struct devpage *)0) {
		hat_devload(kas.a_hat, addr, MMU_PAGESIZE, pfn, prot,
		    HAT_LOAD_LOCK);
	} else
		return (DDI_FAILURE);
	return (DDI_SUCCESS);
}

static int
rootnex_ctl_initchild(dev_info_t *dip)
{
	int n, size_cells;
	dev_info_t *parent;
	struct regspec *rp;
	struct ddi_parent_private_data *pd;
	char	*node_name;
	int	err, port_id;
	uint_t len = 0;
	int *reg_prop;

	/*
	 * Hack to push portid property for fhc node that is under root(/fhc).
	 */
	node_name = ddi_node_name(dip);
	if ((strcmp(node_name, "fhc") == 0) ||
	    (strcmp(node_name, "mem-unit") == 0) ||
	    (strcmp(node_name, "central") == 0)) {
		(void) ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, OBP_REG, &reg_prop, &len);
		ASSERT(len != 0);
		rp = (struct regspec *)reg_prop;
#ifdef	_STARFIRE
		port_id = ((((rp->regspec_bustype) & 0x6) >> 1) |
		    (((rp->regspec_bustype) & 0xF0) >> 2) |
		    (((rp->regspec_bustype) & 0x8) << 3));

#else
		port_id = (rp->regspec_bustype >> 1) & 0x1f;
#endif
		ddi_prop_free(reg_prop);
		err = ddi_prop_create(DDI_DEV_T_NONE, dip,
		    DDI_PROP_CANSLEEP, "upa-portid", (caddr_t)&port_id,
		    sizeof (int));
		if (err != DDI_SUCCESS)
			cmn_err(CE_WARN,
			    "Error in creating upa-portid property for fhc.\n");

	}
	if ((n = impl_ddi_sunbus_initchild(dip)) != DDI_SUCCESS)
		return (n);

	/*
	 * If there are no "reg"s in the child node, return.
	 */

	pd = (struct ddi_parent_private_data *)ddi_get_parent_data(dip);
	if ((pd == NULL) || (pd->par_nreg == 0))
		return (DDI_SUCCESS);

	parent = ddi_get_parent(dip);

	/*
	 * If the parent #size-cells is 2, convert the upa-style
	 * upa-style reg property from 2-size cells to 1 size cell
	 * format, ignoring the size_hi, which must be zero for devices.
	 * (It won't be zero in the memory list properties in the memory
	 * nodes, but that doesn't matter here.)
	 */

	size_cells = ddi_prop_get_int(DDI_DEV_T_ANY, parent,
	    DDI_PROP_DONTPASS, "#size-cells", 1);

	if (size_cells != 1)  {

		int j;
		struct regspec *irp;
		struct upa_reg {
			uint_t addr_hi, addr_lo, size_hi, size_lo;
		};
		struct upa_reg *upa_rp;

		ASSERT(size_cells == 2);

		/*
		 * We already looked the property up once before if
		 * pd is non-NULL.
		 */
		(void) ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, OBP_REG, &reg_prop, &len);
		ASSERT(len != 0);

		n = sizeof (struct upa_reg) / sizeof (int);
		n = len / n;

		/*
		 * We're allocating a buffer the size of the PROM's property,
		 * but we're only using a smaller portion when we assign it
		 * to a regspec.  We do this so that in unitchild, we will
		 * always free the right amount of memory.
		 */
		irp = rp = (struct regspec *)reg_prop;
		upa_rp = (struct upa_reg *)pd->par_reg;

		for (j = 0; j < n; ++j, ++rp, ++upa_rp) {
			ASSERT(upa_rp->size_hi == 0);
			rp->regspec_bustype = upa_rp->addr_hi;
			rp->regspec_addr = upa_rp->addr_lo;
			rp->regspec_size = upa_rp->size_lo;
		}

		ddi_prop_free((void *)pd->par_reg);
		pd->par_nreg = n;
		pd->par_reg = irp;
	}

	/*
	 * If this is a slave device sitting on the UPA, we assume that
	 * This device can accept DMA accesses from other devices.  We need
	 * to register this fact with the system by using the highest
	 * and lowest physical pfns of the devices register space.  This
	 * will then represent a physical block of addresses that are valid
	 * for DMA accesses.
	 */
	if (ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS, "upa-portid",
	    -1) != -1)
		if (ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
		    "upa-interrupt-slave", 0)) {
			uint_t lopfn = 0xffffffffu;
			uint_t hipfn = 0;
			int i;
			extern void pf_set_dmacapable(uint_t, uint_t);

			/* Scan the devices highest and lowest physical pfns */
			for (i = 0, rp = pd->par_reg; i < pd->par_nreg;
			    i++, rp++) {
				uint64_t addr;
				uint_t tmphipfn, tmplopfn;

				addr = (unsigned long long)
				    ((unsigned long long)
					rp->regspec_bustype << 32);
				addr |= (uint64_t)rp->regspec_addr;
				tmplopfn = (uint_t)(addr >> MMU_PAGESHIFT);
				addr += (uint64_t)(rp->regspec_size - 1);
				tmphipfn = (uint_t)(addr >> MMU_PAGESHIFT);

				hipfn = (tmphipfn > hipfn) ? tmphipfn : hipfn;
				lopfn = (tmplopfn < lopfn) ? tmplopfn : lopfn;
			}

			pf_set_dmacapable(hipfn, lopfn);
		}

	return (DDI_SUCCESS);
}


int
rootnex_ctl_uninitchild(dev_info_t *dip)
{
	extern void impl_ddi_sunbus_removechild(dev_info_t *);

	struct ddi_parent_private_data *pd;
	pd = (struct ddi_parent_private_data *)ddi_get_parent_data(dip);

	if (ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS, "upa-portid",
	    -1) != -1)
		if (ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
		    "upa-interrupt-slave", 0)) {
			struct regspec *rp;
			extern void pf_unset_dmacapable(uint_t);
			unsigned long long addr;
			uint_t pfn;

			rp = pd->par_reg;
			addr = (unsigned long long) ((unsigned long long)
			    rp->regspec_bustype << 32);
			addr |= (unsigned long long) rp->regspec_addr;
			pfn = (uint_t)(addr >> MMU_PAGESHIFT);

			pf_unset_dmacapable(pfn);
		}
	impl_ddi_sunbus_removechild(dip);
	return (DDI_SUCCESS);
}


static int
rootnex_ctl_reportdev(dev_info_t *dev)
{
	register int n;
	struct regspec *rp;
	char buf[80];
	char *p = buf;
	int	portid;

	(void) sprintf(p, "%s%d at root", ddi_driver_name(dev),
	    ddi_get_instance(dev));
	p += strlen(p);

	if ((n = sparc_pd_getnreg(dev)) > 0) {
		rp = sparc_pd_getreg(dev, 0);

		(void) strcpy(p, ": ");
		p += strlen(p);

		/*
		 * This stuff needs to be fixed correctly for the FFB
		 * devices and the UPA add-on devices.
		 */
		portid = ddi_prop_get_int(DDI_DEV_T_ANY, dev,
		    DDI_PROP_DONTPASS, "upa-portid", -1);
		if (portid != -1)
			(void) sprintf(p, "UPA 0x%x 0x%x%s",
			    portid,
			    rp->regspec_addr, (n > 1 ? "" : " ..."));
		else {
			portid = ddi_prop_get_int(DDI_DEV_T_ANY, dev,
			    DDI_PROP_DONTPASS, "portid", -1);
			if (portid == -1)
				printf("could not find portid property in %s\n",
				    DEVI(dev)->devi_node_name);
			else
				(void) sprintf(p, "SAFARI 0x%x 0x%x%s",
				    portid,
				    rp->regspec_addr &
				    root_phys_addr_lo_mask,
				    (n > 1 ? "" : " ..."));
		}
		p += strlen(p);
	}

	/*
	 * This is where we need to print out the interrupt specifications
	 * for the FFB device and any UPA add-on devices.  Not sure how to
	 * do this yet?
	 */
	cmn_err(CE_CONT, "?%s\n", buf);
	return (DDI_SUCCESS);
}


/*ARGSUSED*/
static int
rootnex_ctlops(dev_info_t *dip, dev_info_t *rdip,
    ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	register int n, *ptr;
	register struct ddi_parent_private_data *pdp;

	switch (ctlop) {
	case DDI_CTLOPS_DMAPMAPC:
		return (DDI_FAILURE);

	case DDI_CTLOPS_BTOP:
		/*
		 * Convert byte count input to physical page units.
		 * (byte counts that are not a page-size multiple
		 * are rounded down)
		 */
		*(ulong_t *)result = btop(*(ulong_t *)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_PTOB:
		/*
		 * Convert size in physical pages to bytes
		 */
		*(ulong_t *)result = ptob(*(ulong_t *)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_BTOPR:
		/*
		 * Convert byte count input to physical page units
		 * (byte counts that are not a page-size multiple
		 * are rounded up)
		 */
		*(ulong_t *)result = btopr(*(ulong_t *)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INITCHILD:
		return (rootnex_ctl_initchild((dev_info_t *)arg));

	case DDI_CTLOPS_UNINITCHILD:
		return (rootnex_ctl_uninitchild((dev_info_t *)arg));

	case DDI_CTLOPS_REPORTDEV:
		return (rootnex_ctl_reportdev(rdip));

	case DDI_CTLOPS_IOMIN:
		/*
		 * Nothing to do here but reflect back..
		 */
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
		break;

	case DDI_CTLOPS_SIDDEV:
		if (ndi_dev_is_prom_node(rdip))
			return (DDI_SUCCESS);
		if (ndi_dev_is_persistent_node(rdip))
			return (DDI_SUCCESS);
		return (DDI_FAILURE);

	case DDI_CTLOPS_POWER: {
		return ((*pm_platform_power)((power_req_t *)arg));
	}

	default:
		return (DDI_FAILURE);
	}

	/*
	 * The rest are for "hardware" properties
	 */

	pdp = (struct ddi_parent_private_data *)
	    (DEVI(rdip))->devi_parent_data;

	if (!pdp) {
		return (DDI_FAILURE);
	} else if (ctlop == DDI_CTLOPS_NREGS) {
		ptr = (int *)result;
		*ptr = pdp->par_nreg;
	} else if (ctlop == DDI_CTLOPS_NINTRS) {
		return (DDI_FAILURE);
	} else {	/* ctlop == DDI_CTLOPS_REGSIZE */
		off_t *size = (off_t *)result;

		ptr = (int *)arg;
		n = *ptr;
		if (n >= pdp->par_nreg) {
			return (DDI_FAILURE);
		}
		*size = (off_t)pdp->par_reg[n].regspec_size;
	}
	return (DDI_SUCCESS);
}
