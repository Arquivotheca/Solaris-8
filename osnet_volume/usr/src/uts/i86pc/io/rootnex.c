/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rootnex.c	1.112	99/10/25 SMI"

/*
 * Intel PC root nexus driver
 *	based on sun4c root nexus driver 1.30
 */

#include <sys/sysmacros.h>
#include <sys/conf.h>
#include <sys/autoconf.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/psw.h>
#include <sys/ddidmareq.h>
#include <sys/promif.h>
#include <sys/devops.h>
#include <sys/cpu.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_dev.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <sys/avintr.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/ddi_impldefs.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>

#ifndef	FIXED_4150246
#define	CMP64LT(a, b)	cmp64lt(a, (uint64_t)b)
#define	CMP64GT(a, b)	cmp64gt(a, (uint64_t)b)
int cmp64lt(uint64_t, uint64_t);
int cmp64gt(uint64_t, uint64_t);
#else
#define	CMP64LT(a, b)	((a) < (b))
#define	CMP64GT(a, b)	((a) > (b))
#endif
#define	ptob64(x)		(((uint64_t)(x)) << PAGESHIFT)
#define	INTLEVEL_SOFT	0		/* XXX temp kludge XXX */

extern void i86_pp_map(page_t *pp, caddr_t kaddr);
extern void i86_va_map(caddr_t vaddr, struct as *asp, caddr_t kaddr);

extern int isa_resource_setup(void);

/*
 * DMA related static data
 */
static uintptr_t dvma_call_list_id = 0;

/*
 * Hack to handle poke faults on Calvin-class machines
 */
extern int pokefault;
static kmutex_t pokefault_mutex;


/*
 * Internal functions
 */
static int rootnex_ctl_children(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t ctlop, dev_info_t *child);

/*
 * config information
 */

static int
rootnex_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
    off_t offset, off_t len, caddr_t *vaddrp);

static ddi_intrspec_t
rootnex_get_intrspec(dev_info_t *dip, dev_info_t *rdip,	uint_t inumber);

static int
rootnex_add_intrspec(dev_info_t *dip, dev_info_t *rdip,
    ddi_intrspec_t intrspec, ddi_iblock_cookie_t *iblock_cookiep,
    ddi_idevice_cookie_t *idevice_cookiep,
    uint_t (*int_handler)(caddr_t int_handler_arg),
    caddr_t int_handler_arg, int kind);

static void
rootnex_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
    ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookie);

static int
rootnex_map_fault(dev_info_t *dip, dev_info_t *rdip,
    struct hat *hat, struct seg *seg, caddr_t addr,
    struct devpage *dp, uint_t pfn, uint_t prot, uint_t lock);

static int
rootnex_dma_allochdl(dev_info_t *, dev_info_t *, ddi_dma_attr_t *,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *);

static int
rootnex_dma_freehdl(dev_info_t *, dev_info_t *, ddi_dma_handle_t);

static int
rootnex_dma_bindhdl(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
    struct ddi_dma_req *, ddi_dma_cookie_t *, uint_t *);

static int
rootnex_dma_unbindhdl(dev_info_t *, dev_info_t *, ddi_dma_handle_t);

static int
rootnex_dma_flush(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
    off_t, size_t, uint_t);

static int
rootnex_dma_win(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
    uint_t, off_t *, size_t *, ddi_dma_cookie_t *, uint_t *);

static int
rootnex_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep);

static int
rootnex_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, size_t *lenp, caddr_t *objp, uint_t cache_flags);

static int
rootnex_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);

static struct bus_ops rootnex_bus_ops = {
	BUSO_REV,
	rootnex_map,
	rootnex_get_intrspec,
	rootnex_add_intrspec,
	rootnex_remove_intrspec,
	rootnex_map_fault,
	rootnex_dma_map,
	rootnex_dma_allochdl,
	rootnex_dma_freehdl,
	rootnex_dma_bindhdl,
	rootnex_dma_unbindhdl,
	rootnex_dma_flush,
	rootnex_dma_win,
	rootnex_dma_mctl,
	rootnex_ctlops,
	ddi_bus_prop_op,
	i_ddi_rootnex_get_eventcookie,
	i_ddi_rootnex_add_eventcall,
	i_ddi_rootnex_remove_eventcall,
	i_ddi_rootnex_post_event
};

struct priv_handle {
	caddr_t	ph_vaddr;
	union {
		page_t *pp;
		struct as *asp;
	}ph_u;
	uint_t  ph_mapinfo;
	uint64_t ph_padr;
};
static uint64_t rootnex_get_phyaddr();
static int rootnex_identify(dev_info_t *devi);
static int rootnex_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int rootnex_io_rdsync(ddi_dma_impl_t *hp);
static int rootnex_io_wtsync(ddi_dma_impl_t *hp, int);
static int rootnex_io_brkup_attr(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep,
    struct priv_handle *php);
static int rootnex_io_brkup_lim(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep,
    ddi_dma_lim_t *dma_lim, struct priv_handle *php);

static struct dev_ops rootnex_ops = {
	DEVO_REV,
	0,		/* refcnt */
	ddi_no_info,	/* info */
	rootnex_identify,
	nulldev,	/* probe */
	rootnex_attach,
	nulldev,	/* detach */
	nulldev,	/* reset */
	0,		/* cb_ops */
	&rootnex_bus_ops
};

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a nexus driver */
	"i86pc root nexus 1.112",
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
 * 	identify the root nexus for an Intel 80x86 machine.
 *
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
	mutex_init(&pokefault_mutex, NULL, MUTEX_SPIN, (void *)ipltospl(15));

	add_root_props(devi);

	cmn_err(CE_CONT, "?root nexus = %s\n", ddi_get_name(devi));
	if (isa_resource_setup() != NDI_SUCCESS) {
		cmn_err(CE_WARN, "?rootnex: isa resource setup failed \n");
	}

	i_ddi_rootnex_init_events(devi);

	return (DDI_SUCCESS);
}


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

static void
add_root_props(dev_info_t *devi)
{
	int i;
	struct prop_def *rpp;

	/*
	 * Note this for loop works because all of the root_prop
	 * properties are integers - if this changes, the for
	 * loop will have to change.
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
	 *	"generic-addressing"  (Sun4 -- pseudo OBP/DDI)
	 */

	(void) e_ddi_prop_update_int(DDI_DEV_T_NONE, devi,
	    DDI_RELATIVE_ADDRESSING, 1);

}

/*
 * #define	DDI_MAP_DEBUG (c.f. ddi_impl.c)
 */
#ifdef	DDI_MAP_DEBUG
extern int ddi_map_debug_flag;
#define	ddi_map_debug	if (ddi_map_debug_flag) prom_printf
#endif	DDI_MAP_DEBUG


/*
 * we don't support mapping of I/O cards above 4Gb
 */
static int
rootnex_map_regspec(ddi_map_req_t *mp, caddr_t *vaddrp)
{
	ulong_t base;
	void *cvaddr;
	uint_t npages, pgoffset;
	struct regspec *rp;
	ddi_acc_hdl_t *hp;
	ddi_acc_impl_t *ap;
	uint_t	hat_acc_flags;

	rp = mp->map_obj.rp;
	hp = mp->map_handlep;

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug(
	    "rootnex_map_regspec: <0x%x 0x%x 0x%x> handle 0x%x\n",
	    rp->regspec_bustype, rp->regspec_addr,
	    rp->regspec_size, mp->map_handlep);
#endif	DDI_MAP_DEBUG

	/*
	 * I/O or memory mapping
	 *
	 *	<bustype=0, addr=x, len=x>: memory
	 *	<bustype=1, addr=x, len=x>: i/o
	 *	<bustype>1, addr=0, len=x>: x86-compatibility i/o
	 */

	if (rp->regspec_bustype > 1 && rp->regspec_addr != 0) {
		cmn_err(CE_WARN, "rootnex: invalid register spec"
		    " <0x%x, 0x%x, 0x%x>\n", rp->regspec_bustype,
		    rp->regspec_addr, rp->regspec_size);
		return (DDI_FAILURE);
	}

	if (rp->regspec_bustype != 0) {
		/*
		 * I/O space - needs a handle.
		 */
		if (hp == NULL) {
			return (DDI_FAILURE);
		}
		ap = (ddi_acc_impl_t *)hp->ah_platform_private;
		ap->ahi_acc_attr |= DDI_ACCATTR_IO_SPACE;
		impl_acc_hdl_init(hp);

		if (mp->map_flags & DDI_MF_DEVICE_MAPPING) {
#ifdef  DDI_MAP_DEBUG
			ddi_map_debug("rootnex_map_regspec: mmap() \
to I/O space is not supported.\n");
#endif  DDI_MAP_DEBUG
			return (DDI_ME_INVAL);
		} else {
			/*
			 * 1275-compliant vs. compatibility i/o mapping
			 */
			*vaddrp =
			    (rp->regspec_bustype > 1 && rp->regspec_addr == 0) ?
				((caddr_t)rp->regspec_bustype) :
				((caddr_t)rp->regspec_addr);
		}

#ifdef	DDI_MAP_DEBUG
		ddi_map_debug(
	    "rootnex_map_regspec: \"Mapping\" %d bytes I/O space at 0x%x\n",
		    rp->regspec_size, *vaddrp);
#endif	DDI_MAP_DEBUG
		return (DDI_SUCCESS);
	}

	/*
	 * Memory space
	 */

	if (hp != NULL) {
		/*
		 * hat layer ignores
		 * hp->ah_acc.devacc_attr_endian_flags.
		 */
		switch (hp->ah_acc.devacc_attr_dataorder) {
		case DDI_STRICTORDER_ACC:
			hat_acc_flags = HAT_STRICTORDER;
			break;
		case DDI_UNORDERED_OK_ACC:
			hat_acc_flags = HAT_UNORDERED_OK;
			break;
		case DDI_MERGING_OK_ACC:
			hat_acc_flags = HAT_MERGING_OK;
			break;
		case DDI_LOADCACHING_OK_ACC:
			hat_acc_flags = HAT_LOADCACHING_OK;
			break;
		case DDI_STORECACHING_OK_ACC:
			hat_acc_flags = HAT_STORECACHING_OK;
			break;
		}
		ap = (ddi_acc_impl_t *)hp->ah_platform_private;
		ap->ahi_acc_attr |= DDI_ACCATTR_CPU_VADDR;
		impl_acc_hdl_init(hp);
		hp->ah_hat_flags = hat_acc_flags;
	} else {
		hat_acc_flags = HAT_STRICTORDER;
	}

	base = (ulong_t)rp->regspec_addr & (~MMU_PAGEOFFSET); /* base addr */
	pgoffset = (ulong_t)rp->regspec_addr & MMU_PAGEOFFSET; /* offset */

	if (rp->regspec_size == 0) {
#ifdef  DDI_MAP_DEBUG
		ddi_map_debug("rootnex_map_regspec: zero regspec_size\n");
#endif  DDI_MAP_DEBUG
		return (DDI_ME_INVAL);
	}

	if (mp->map_flags & DDI_MF_DEVICE_MAPPING) {
		*vaddrp = (caddr_t)mmu_btop(base);
	} else {
		npages = mmu_btopr(rp->regspec_size + pgoffset);

#ifdef	DDI_MAP_DEBUG
		ddi_map_debug("rootnex_map_regspec: Mapping %d pages \
physical %x ",
		    npages, base);
#endif	DDI_MAP_DEBUG

		cvaddr = vmem_alloc(heap_arena, ptob(npages), VM_NOSLEEP);
		if (cvaddr == NULL)
			return (DDI_ME_NORESOURCES);

		/*
		 * Now map in the pages we've allocated...
		 */
		hat_devload(kas.a_hat, cvaddr, mmu_ptob(npages), mmu_btop(base),
		    mp->map_prot | hat_acc_flags, HAT_LOAD_LOCK);
		*vaddrp = (caddr_t)cvaddr + pgoffset;
	}

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug("at virtual 0x%x\n", *vaddrp);
#endif	DDI_MAP_DEBUG
	return (DDI_SUCCESS);
}

static int
rootnex_unmap_regspec(ddi_map_req_t *mp, caddr_t *vaddrp)
{
	caddr_t addr = (caddr_t)*vaddrp;
	uint_t npages, pgoffset;
	struct regspec *rp;

	if (mp->map_flags & DDI_MF_DEVICE_MAPPING)
		return (0);

	rp = mp->map_obj.rp;

	if (rp->regspec_size == 0) {
#ifdef  DDI_MAP_DEBUG
		ddi_map_debug("rootnex_unmap_regspec: zero regspec_size\n");
#endif  DDI_MAP_DEBUG
		return (DDI_ME_INVAL);
	}

	/*
	 * I/O or memory mapping:
	 *
	 *	<bustype=0, addr=x, len=x>: memory
	 *	<bustype=1, addr=x, len=x>: i/o
	 *	<bustype>1, addr=0, len=x>: x86-compatibility i/o
	 */
	if (rp->regspec_bustype != 0) {
		/*
		 * This is I/O space, which requires no particular
		 * processing on unmap since it isn't mapped in the
		 * first place.
		 */
		return (DDI_SUCCESS);
	}

	/*
	 * Memory space
	 */
	pgoffset = (uintptr_t)addr & MMU_PAGEOFFSET;
	npages = mmu_btopr(rp->regspec_size + pgoffset);
	hat_unload(kas.a_hat, addr - pgoffset, ptob(npages), HAT_UNLOAD_UNLOCK);
	vmem_free(heap_arena, addr - pgoffset, ptob(npages));

	/*
	 * Destroy the pointer - the mapping has logically gone
	 */
	*vaddrp = (caddr_t)0;

	return (DDI_SUCCESS);
}

static int
rootnex_map_handle(ddi_map_req_t *mp)
{
	ddi_acc_hdl_t *hp;
	ulong_t base;
	uint_t pgoffset;
	struct regspec *rp;

	rp = mp->map_obj.rp;

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug(
	    "rootnex_map_handle: <0x%x 0x%x 0x%x> handle 0x%x\n",
	    rp->regspec_bustype, rp->regspec_addr,
	    rp->regspec_size, mp->map_handlep);
#endif	DDI_MAP_DEBUG

	/*
	 * I/O or memory mapping:
	 *
	 *	<bustype=0, addr=x, len=x>: memory
	 *	<bustype=1, addr=x, len=x>: i/o
	 *	<bustype>1, addr=0, len=x>: x86-compatibility i/o
	 */
	if (rp->regspec_bustype != 0) {
		/*
		 * This refers to I/O space, and we don't support "mapping"
		 * I/O space to a user.
		 */
		return (DDI_FAILURE);
	}

	/*
	 * Set up the hat_flags for the mapping.
	 */
	hp = mp->map_handlep;

	switch (hp->ah_acc.devacc_attr_endian_flags) {
	case DDI_NEVERSWAP_ACC:
		hp->ah_hat_flags = HAT_NEVERSWAP | HAT_STRICTORDER;
		break;
	case DDI_STRUCTURE_LE_ACC:
		hp->ah_hat_flags = HAT_STRUCTURE_LE;
		break;
	case DDI_STRUCTURE_BE_ACC:
		return (DDI_FAILURE);
	default:
		return (DDI_REGS_ACC_CONFLICT);
	}

	switch (hp->ah_acc.devacc_attr_dataorder) {
	case DDI_STRICTORDER_ACC:
		break;
	case DDI_UNORDERED_OK_ACC:
		hp->ah_hat_flags |= HAT_UNORDERED_OK;
		break;
	case DDI_MERGING_OK_ACC:
		hp->ah_hat_flags |= HAT_MERGING_OK;
		break;
	case DDI_LOADCACHING_OK_ACC:
		hp->ah_hat_flags |= HAT_LOADCACHING_OK;
		break;
	case DDI_STORECACHING_OK_ACC:
		hp->ah_hat_flags |= HAT_STORECACHING_OK;
		break;
	default:
		return (DDI_FAILURE);
	}

	base = (ulong_t)rp->regspec_addr & (~MMU_PAGEOFFSET); /* base addr */
	pgoffset = (ulong_t)rp->regspec_addr & MMU_PAGEOFFSET; /* offset */

	if (rp->regspec_size == 0)
		return (DDI_ME_INVAL);

	hp->ah_pfn = mmu_btop(base);
	hp->ah_pnum = mmu_btopr(rp->regspec_size + pgoffset);

	return (DDI_SUCCESS);
}

static int
rootnex_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *vaddrp)
{
	struct regspec *rp, tmp_reg;
	ddi_map_req_t mr = *mp;		/* Get private copy of request */
	int error;

	mp = &mr;

	switch (mp->map_op)  {
	case DDI_MO_MAP_LOCKED:
	case DDI_MO_UNMAP:
	case DDI_MO_MAP_HANDLE:
		break;
	default:
#ifdef	DDI_MAP_DEBUG
		cmn_err(CE_WARN, "rootnex_map: unimplemented map op %d.",
		    mp->map_op);
#endif	DDI_MAP_DEBUG
		return (DDI_ME_UNIMPLEMENTED);
	}

	if (mp->map_flags & DDI_MF_USER_MAPPING)  {
#ifdef	DDI_MAP_DEBUG
		cmn_err(CE_WARN, "rootnex_map: unimplemented map type: user.");
#endif	DDI_MAP_DEBUG
		return (DDI_ME_UNIMPLEMENTED);
	}

	/*
	 * First, if given an rnumber, convert it to a regspec...
	 * (Presumably, this is on behalf of a child of the root node?)
	 */

	if (mp->map_type == DDI_MT_RNUMBER)  {

		int rnumber = mp->map_obj.rnumber;
#ifdef	DDI_MAP_DEBUG
		static char *out_of_range =
		    "rootnex_map: Out of range rnumber <%d>, device <%s>";
#endif	DDI_MAP_DEBUG

		rp = i_ddi_rnumber_to_regspec(rdip, rnumber);
		if (rp == (struct regspec *)0)  {
#ifdef	DDI_MAP_DEBUG
			cmn_err(CE_WARN, out_of_range, rnumber,
			    ddi_get_name(rdip));
#endif	DDI_MAP_DEBUG
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
	 * XXX: (regardless of what's in the parent's range?)
	 */

	tmp_reg = *(mp->map_obj.rp);		/* Preserve underlying data */
	rp = mp->map_obj.rp = &tmp_reg;		/* Use tmp_reg in request */

#ifdef	DDI_MAP_DEBUG
	cmn_err(CE_CONT,
		"rootnex: <%s,%s> <0x%x, 0x%x, 0x%d>"
		" offset %d len %d handle 0x%x\n",
		ddi_get_name(dip), ddi_get_name(rdip),
		rp->regspec_bustype, rp->regspec_addr, rp->regspec_size,
		offset, len, mp->map_handlep);
#endif	DDI_MAP_DEBUG

	/*
	 * I/O or memory mapping:
	 *
	 *	<bustype=0, addr=x, len=x>: memory
	 *	<bustype=1, addr=x, len=x>: i/o
	 *	<bustype>1, addr=0, len=x>: x86-compatibility i/o
	 */

	if (rp->regspec_bustype > 1 && rp->regspec_addr != 0) {
		cmn_err(CE_WARN, "<%s,%s> invalid register spec"
		    " <0x%x, 0x%x, 0x%x>\n", ddi_get_name(dip),
		    ddi_get_name(rdip), rp->regspec_bustype,
		    rp->regspec_addr, rp->regspec_size);
		return (DDI_ME_INVAL);
	}

	if (rp->regspec_bustype > 1 && rp->regspec_addr == 0) {
		/*
		 * compatibility i/o mapping
		 */
		rp->regspec_bustype += (uint_t)offset;
	} else {
		/*
		 * Normal memory or i/o mapping
		 */
		rp->regspec_addr += (uint_t)offset;
	}

	if (len != 0)
		rp->regspec_size = (uint_t)len;

#ifdef	DDI_MAP_DEBUG
	cmn_err(CE_CONT,
		"             <%s,%s> <0x%x, 0x%x, 0x%d>"
		" offset %d len %d handle 0x%x\n",
		ddi_get_name(dip), ddi_get_name(rdip),
		rp->regspec_bustype, rp->regspec_addr, rp->regspec_size,
		offset, len, mp->map_handlep);
#endif	DDI_MAP_DEBUG

	/*
	 * Apply any parent ranges at this level, if applicable.
	 * (This is where nexus specific regspec translation takes place.
	 * Use of this function is implicit agreement that translation is
	 * provided via ddi_apply_range.)
	 */

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug("applying range of parent <%s> to child <%s>...\n",
	    ddi_get_name(dip), ddi_get_name(rdip));
#endif	DDI_MAP_DEBUG

	if ((error = i_ddi_apply_range(dip, rdip, mp->map_obj.rp)) != 0)
		return (error);

	switch (mp->map_op)  {
	case DDI_MO_MAP_LOCKED:

		/*
		 * Set up the locked down kernel mapping to the regspec...
		 */

		return (rootnex_map_regspec(mp, vaddrp));

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

/*
 * rootnex_get_intrspec: rootnex convert an interrupt number to an interrupt
 *			specification. The interrupt number determines
 *			which interrupt spec will be returned if more than
 *			one exists. Look into the parent private data
 *			area of the dev_info structure to find the interrupt
 *			specification.  First check to make sure there is
 *			one that matchs "inumber" and then return a pointer
 *			to it.  Return NULL if one could not be found.
 */
static ddi_intrspec_t
rootnex_get_intrspec(dev_info_t *dip, dev_info_t *rdip, uint_t inumber)
{
	struct ddi_parent_private_data *ppdptr;

#ifdef	lint
	dip = dip;
#endif

	/*
	 * convert the parent private data pointer in the childs dev_info
	 * structure to a pointer to a sunddi_compat_hack structure
	 * to get at the interrupt specifications.
	 */
	ppdptr = (struct ddi_parent_private_data *)
	    (DEVI(rdip))->devi_parent_data;

	/*
	 * validate the interrupt number.
	 */
	if (inumber >= ppdptr->par_nintr) {
		return (NULL);
	}

	/*
	 * return the interrupt structure pointer.
	 */
	return ((ddi_intrspec_t)&ppdptr->par_intr[inumber]);
}

/*
 * rootnex_add_intrspec:
 *
 *	Add an interrupt specification.
 */
static int
rootnex_add_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	uint_t (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg, int kind)
{
	struct intrspec *ispec;
	uint_t pri;
	int soft = 0;
	extern int (*psm_translate_irq)(dev_info_t *, int);

#ifdef	lint
	dip = dip;
#endif

	ispec = (struct intrspec *)intrspec;
	pri = INT_IPL(ispec->intrspec_pri);

	if (kind == IDDI_INTR_TYPE_FAST) {
#ifdef XXX_later
		if (!settrap(rdip, ispec->intrspec_pri, int_handler)) {
			return (DDI_FAILURE);
		}
		ispec->intrspec_func = (uint_t (*)()) 1;
#else
		return (DDI_FAILURE);
#endif
	} else {
		/*
		 * Convert 'soft' pri to "fit" with 4m model
		 */
		soft = (kind == IDDI_INTR_TYPE_SOFT);
		if (soft)
			ispec->intrspec_pri = pri + INTLEVEL_SOFT;
		else {
			ispec->intrspec_pri = pri;
			/*
			 * call psmi to translate the irq with the dip
			 */
			ispec->intrspec_vec = (uint_t)
				(*psm_translate_irq)(rdip, ispec->intrspec_vec);
		}

		if (soft) {
			/* register soft interrupt handler */
			if (!add_avsoftintr((void *)ispec, ispec->intrspec_pri,
			    int_handler, DEVI(rdip)->devi_name,
			    int_handler_arg)) {
				return (DDI_FAILURE);
			}
		} else {
			/* register hardware interrupt handler */
			if (!add_avintr((void *)ispec, ispec->intrspec_pri,
			    int_handler, DEVI(rdip)->devi_name,
			    ispec->intrspec_vec, int_handler_arg)) {
				return (DDI_FAILURE);
			}
		}
		ispec->intrspec_func = int_handler;
	}

	if (iblock_cookiep) {
		*iblock_cookiep = (ddi_iblock_cookie_t)ipltospl(pri);
	}

	if (idevice_cookiep) {
		idevice_cookiep->idev_vector = 0;
		if (kind == IDDI_INTR_TYPE_SOFT) {
			idevice_cookiep->idev_softint = (ulong_t)soft;
		} else {
			/*
			 * The idevice cookie should contain the priority as
			 * understood by the device itself on the bus it
			 * lives on.  Let the nexi beneath sort out the
			 * translation (if any) that's needed.
			 */
			idevice_cookiep->idev_priority = (ushort_t)pri;
		}
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
rootnex_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookie)
{
	struct intrspec *ispec = (struct intrspec *)intrspec;

	if (ispec->intrspec_func == (uint_t (*)()) 0) {
		return;
#ifdef XXX_later
	} else if (ispec->intrspec_func == (uint_t (*)()) 1) {
		(void) settrap(rdip, ispec->intrspec_pri, NULL);
#endif
	} else {
		rem_avintr((void *)ispec, ispec->intrspec_pri,
		    ispec->intrspec_func, ispec->intrspec_vec);
	}
	ispec->intrspec_func = (uint_t (*)()) 0;
}

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

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug("rootnex_map_fault: address <%x> pfn <%x>", addr, pfn);
	ddi_map_debug(" Seg <%s>\n",
	    seg->s_ops == &segdev_ops ? "segdev" :
	    seg == &kvseg ? "segkmem" : "NONE!");
#endif	DDI_MAP_DEBUG

	/*
	 * This is all terribly broken, but it is a start
	 *
	 * XXX	Note that this test means that segdev_ops
	 *	must be exported from seg_dev.c.
	 * XXX	What about devices with their own segment drivers?
	 */
	if (seg->s_ops == &segdev_ops) {
		struct segdev_data *sdp =
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


/*
 * DMA routines- for all 80x86 machines.
 */

/*
 * Shorthand defines
 */

#define	MAP	0
#define	BIND	1
#define	MAX_INT_BUF	(16*MMU_PAGESIZE)
#define	AHI_LIM		dma_lim->dlim_addr_hi
#define	AHI_ATTR	dma_attr->dma_attr_addr_hi
#define	OBJSIZE		dmareq->dmar_object.dmao_size
#define	OBJTYPE		dmareq->dmar_object.dmao_type
#define	FOURG		0x100000000ULL
#define	SIXTEEN_MB	0x1000000

/* #define	DMADEBUG */
#if defined(DEBUG) || defined(lint)
#define	DMADEBUG
static int dmadebug = 0;
#define	DMAPRINT(a)	if (dmadebug) prom_printf a
#else
#define	DMAPRINT(a)	{ }
#endif	/* DEBUG */



/*
 * allocate DMA handle
 */
static int
rootnex_dma_allochdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_attr_t *attr,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *handlep)
{
	ddi_dma_impl_t *hp;
	uint64_t maxsegmentsize_ll;
	uint_t maxsegmentsize;

#ifdef lint
	dip = dip;
#endif


	/*
	 * Validate the dma request.
	 */
#ifdef DMADEBUG
	if (attr->dma_attr_seg < MMU_PAGEOFFSET ||
	    attr->dma_attr_count_max < MMU_PAGEOFFSET ||
	    attr->dma_attr_granular > MMU_PAGESIZE ||
	    attr->dma_attr_maxxfer < MMU_PAGESIZE) {
		DMAPRINT((" bad_limits\n"));
		return (DDI_DMA_BADLIMITS);
	}
#endif
	/*
	 * validate the attribute structure. For now we do not support
	 * negative sgllen.
	 */
	if ((attr->dma_attr_addr_hi <= attr->dma_attr_addr_lo) ||
	    (attr->dma_attr_sgllen <= 0)) {
		return (DDI_DMA_BADATTR);
	}
	if ((attr->dma_attr_seg & MMU_PAGEOFFSET) != MMU_PAGEOFFSET ||
	    MMU_PAGESIZE & (attr->dma_attr_granular - 1) ||
	    attr->dma_attr_sgllen < 0) {
		return (DDI_DMA_BADATTR);
	}


	maxsegmentsize_ll = MIN(attr->dma_attr_seg,
	    MIN((attr->dma_attr_count_max + 1) *
	    attr->dma_attr_minxfer,
	    attr->dma_attr_maxxfer) - 1) + 1;
	/*
	 * We will calculate a 64 bit segment size, if the segment size
	 * is greater that 4G, we will limit it to (4G - 1).
	 * The size of dma object (ddi_dma_obj_t.dmao_size)
	 * is 32 bits.
	 */
	if (maxsegmentsize_ll == 0 || (maxsegmentsize_ll > FOURG))
		maxsegmentsize = FOURG - 1;
	else
		maxsegmentsize = maxsegmentsize_ll;

	/*
	 * We should be able to DMA into every byte offset in a page.
	 */
	if (maxsegmentsize < MMU_PAGESIZE) {
		DMAPRINT((" bad_limits, maxsegmentsize\n"));
		return (DDI_DMA_BADLIMITS);
	}


	hp = (ddi_dma_impl_t *)kmem_zalloc(sizeof (*hp),
		(waitfp == DDI_DMA_SLEEP) ? KM_SLEEP : KM_NOSLEEP);
	if (hp == NULL) {
		if (waitfp != DDI_DMA_DONTWAIT) {
			ddi_set_callback(waitfp, arg, &dvma_call_list_id);
		}
		return (DDI_DMA_NORESOURCES);
	}
	/*
	 * Preallocate space for cookie structures. We will use this when
	 * the request does not span more than (DMAI_SOMEMORE_COOKIES - 1)
	 * pages.
	 */
	hp->dmai_additionalcookiep = (ddi_dma_cookie_t *)
	    kmem_zalloc(sizeof (ddi_dma_cookie_t) * DMAI_SOMEMORE_COOKIES,
		(waitfp == DDI_DMA_SLEEP) ? KM_SLEEP : KM_NOSLEEP);

	/*
	 * Save requestor's information
	 */
	hp->dmai_wins = NULL;
	hp->dmai_kaddr =
	hp->dmai_ibufp = NULL;
	hp->dmai_inuse = 0;
	hp->dmai_minxfer = attr->dma_attr_minxfer;
	hp->dmai_burstsizes = attr->dma_attr_burstsizes;
	hp->dmai_minfo = NULL;
	hp->dmai_rdip = rdip;
	hp->dmai_attr = *attr;
	hp->dmai_mctl = rootnex_dma_mctl;
	hp->dmai_segmentsize = maxsegmentsize;
	*handlep = (ddi_dma_handle_t)hp;

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
rootnex_dma_freehdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;

	/*
	 * free the additional cookie space.
	 */
	if (hp->dmai_additionalcookiep)
	    kmem_free(hp->dmai_additionalcookiep,
		sizeof (ddi_dma_cookie_t) * DMAI_SOMEMORE_COOKIES);

	kmem_free(hp, sizeof (*hp));
	if (dvma_call_list_id)
		ddi_run_callback(&dvma_call_list_id);
	return (DDI_SUCCESS);
}

static int
rootnex_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
    ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	ddi_dma_attr_t *dma_attr = &hp->dmai_attr;
	ddi_dma_cookie_t *cp;
	impl_dma_segment_t *segp;
	uint_t segcount = 1;
	int rval;
	struct priv_handle php;
	uint_t	size, offset;
	uint64_t padr;

	/*
	 * no mutex for speed
	 */
	if (hp->dmai_inuse) {
		return (DDI_DMA_INUSE);
	}
	hp->dmai_inuse = 1;


	size = OBJSIZE;
	/*
	 * get the physical address of the first page of an object
	 * defined through 'dmareq' structure.
	 */
	padr = rootnex_get_phyaddr(dmareq, 0, &php);
	offset = padr & MMU_PAGEOFFSET;
	if (offset & (dma_attr->dma_attr_minxfer - 1)) {
		DMAPRINT((" bad_limits/mapping\n"));
		return (DDI_DMA_NOMAPPING);
	} else if ((dma_attr->dma_attr_sgllen > 1) &&
	    (size <= MMU_PAGESIZE) && CMP64LT(padr, AHI_ATTR)) {
		/*
		 * The object is not more than a PAGESIZE and we could DMA into
		 * the physical page.
		 * The cache is completely coherent, set the NOSYNC flag.
		 */
		hp->dmai_rflags = (dmareq->dmar_flags & DMP_DDIFLAGS) |
			DMP_NOSYNC;
		/*
		 * Fill in the physical address in the cookie pointer.
		 */
		cookiep->dmac_type = php.ph_mapinfo;
		cookiep->dmac_laddress = padr;
		if ((offset + size) <= MMU_PAGESIZE) {
		    cookiep->dmac_size = size;
		    hp->dmai_cookie = NULL;
		    *ccountp = 1;
		} else if (hp->dmai_additionalcookiep) {
		/*
		 * The object spans a page boundary. We will use the space
		 * that we preallocated to store the additional cookie.
		 */
		    cookiep->dmac_size = MMU_PAGESIZE - offset;
		    hp->dmai_cookie = hp->dmai_additionalcookiep;
		    padr = rootnex_get_phyaddr(dmareq,
			(uint_t)cookiep->dmac_size, &php);
		    if (CMP64GT(padr, AHI_ATTR)) {
			/*
			 * We can not DMA into this physical page. We will
			 * need intermediate buffers. Reset the state in
			 * the php structure.
			 */
			padr = rootnex_get_phyaddr(dmareq, 0, &php);
			goto io_brkup_attr;
		    }
		    hp->dmai_additionalcookiep->dmac_type = php.ph_mapinfo;
		    hp->dmai_additionalcookiep->dmac_laddress = padr;
		    hp->dmai_additionalcookiep->dmac_size =
			size - cookiep->dmac_size;
		    *ccountp = 2;
		} else {
			goto io_brkup_attr;
		}
		hp->dmai_kaddr = NULL;
		hp->dmai_segp = NULL;
		hp->dmai_ibufp = NULL;
		return (DDI_DMA_MAPPED);
	}
io_brkup_attr:
	/*
	 * The function rootnex_get_phyaddr() does not save the physical
	 * address in the php structure. Save it here for
	 * rootnext_io_brkup_attr().
	 */
	php.ph_padr = padr;
	rval =  rootnex_io_brkup_attr(dip, rdip, dmareq, handle, &php);
	if (rval && (rval != DDI_DMA_PARTIAL_MAP)) {
		hp->dmai_inuse = 0;
		return (rval);
	}
	hp->dmai_wins = segp = hp->dmai_hds;
	if (hp->dmai_ibufp) {
		(void) rootnex_io_wtsync(hp, BIND);
	}

	while ((segp->dmais_flags & DMAIS_WINEND) == 0) {
		segp = segp->dmais_link;
		segcount++;
	}
	*ccountp = segcount;
	cp = hp->dmai_cookie;
	ASSERT(cp);
	cookiep->dmac_type = cp->dmac_type;
	cookiep->dmac_laddress = cp->dmac_laddress;
	cookiep->dmac_size = cp->dmac_size;
	hp->dmai_cookie++;

	return (rval);
}

/*ARGSUSED*/
static int
rootnex_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	int rval = DDI_SUCCESS;

	if (hp->dmai_ibufp) {
		rval = rootnex_io_rdsync(hp);
		ddi_mem_free(hp->dmai_ibufp);
	}
	if (hp->dmai_kaddr)
		vmem_free(heap_arena, hp->dmai_kaddr, PAGESIZE);
	if (hp->dmai_segp)
	    kmem_free((caddr_t)hp->dmai_segp, hp->dmai_kmsize);
	if (dvma_call_list_id)
		ddi_run_callback(&dvma_call_list_id);
	hp->dmai_inuse = 0;
	return (rval);
}

/*ARGSUSED*/
static int
rootnex_dma_flush(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, off_t off, size_t len,
    uint_t cache_flags)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	int rval = DDI_SUCCESS;

	if (hp->dmai_ibufp) {
		if (cache_flags == DDI_DMA_SYNC_FORDEV) {
			rval = rootnex_io_wtsync(hp, MAP);
		} else {
			rval =  rootnex_io_rdsync(hp);
		}
	}
	return (rval);
}

/*ARGSUSED*/
static int
rootnex_dma_win(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, uint_t win, off_t *offp,
    size_t *lenp, ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	impl_dma_segment_t *segp, *winp = hp->dmai_hds;
	uint_t len, segcount = 1;
	ddi_dma_cookie_t *cp;
	int i;

	/*
	 * win is in the range [0 .. dmai_nwin-1]
	 */
	if (win >= hp->dmai_nwin) {
		return (DDI_FAILURE);
	}
	if (hp->dmai_wins && hp->dmai_ibufp) {
		(void) rootnex_io_rdsync(hp);
	}
	ASSERT(winp->dmais_flags & DMAIS_WINSTRT);
	for (i = 0; i < win; i++) {
		winp = winp->_win._dmais_nex;
		ASSERT(winp);
		ASSERT(winp->dmais_flags & DMAIS_WINSTRT);
	}

	hp->dmai_wins = (impl_dma_segment_t *)winp;
	if (hp->dmai_ibufp)
		(void) rootnex_io_wtsync(hp, BIND);
	segp = winp;
	len = segp->dmais_size;
	*offp = segp->dmais_ofst;
	while ((segp->dmais_flags & DMAIS_WINEND) == 0) {
		segp = segp->dmais_link;
		len += segp->dmais_size;
		segcount++;
	}

	*lenp = len;
	*ccountp = segcount;
	cp = hp->dmai_cookie = winp->dmais_cookie;
	ASSERT(cp);
	cookiep->dmac_type = cp->dmac_type;
	cookiep->dmac_laddress = cp->dmac_laddress;
	cookiep->dmac_size = cp->dmac_size;
	hp->dmai_cookie++;
	DMAPRINT(("getwin win %x mapping %llx size %x\n",
	    (ulong_t)winp, cp->dmac_laddress, cp->dmac_size));

	return (DDI_SUCCESS);
}

static int
rootnex_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep)
{
	ddi_dma_lim_t *dma_lim = dmareq->dmar_limits;
	impl_dma_segment_t *segmentp;
	ddi_dma_impl_t *hp;
	struct priv_handle php;
	uint64_t padr;
	uint_t offset, size;
	int sizehandle;
	int mapinfo;

#ifdef lint
	dip = dip;
#endif

	DMAPRINT(("dma_map: %s (%s) reqp %x ", (handlep)? "alloc" : "advisory",
	    ddi_get_name(rdip), (ulong_t)dmareq));

#ifdef	DMADEBUG
	/*
	 * Validate range checks on DMA limits
	 */
	if ((dma_lim->dlim_adreg_max & MMU_PAGEOFFSET) != MMU_PAGEOFFSET ||
	    dma_lim->dlim_granular > MMU_PAGESIZE ||
	    dma_lim->dlim_sgllen <= 0) {
		DMAPRINT((" bad_limits\n"));
		return (DDI_DMA_BADLIMITS);
	}
#endif
	size = OBJSIZE;
	/*
	 * get the physical address of the first page of an object
	 * defined through 'dmareq' structure.
	 */
	padr = rootnex_get_phyaddr(dmareq, 0, &php);
	mapinfo = php.ph_mapinfo;
	offset = padr & MMU_PAGEOFFSET;
	if (offset & (dma_lim->dlim_minxfer - 1)) {
		DMAPRINT((" bad_limits/mapping\n"));
		return (DDI_DMA_NOMAPPING);
	} else if (((offset + size) < MMU_PAGESIZE) && CMP64LT(padr, AHI_LIM)) {
		/*
		 * The object is less than a PAGESIZE and we could DMA into
		 * the physical page.
		 */
		if (!handlep)
		    return (DDI_DMA_MAPOK);
		sizehandle = sizeof (ddi_dma_impl_t) +
		    sizeof (impl_dma_segment_t);

		hp = (ddi_dma_impl_t *)kmem_alloc(sizehandle,
		    (dmareq->dmar_fp == DDI_DMA_SLEEP) ?
		    KM_SLEEP : KM_NOSLEEP);
		if (!hp) {
			/* let other routine do callback */
			goto breakup_req;
		}
		hp->dmai_kmsize = sizehandle;

		/*
		 * locate segments after dma_impl handle structure
		 */
		segmentp = (impl_dma_segment_t *)(hp + 1);

		/*
		 * Save requestor's information
		 */
		hp->dmai_minxfer = dma_lim->dlim_minxfer;
		hp->dmai_burstsizes = dma_lim->dlim_burstsizes;
		hp->dmai_rdip = rdip;
		hp->dmai_mctl = rootnex_dma_mctl;
		hp->dmai_wins = NULL;
		hp->dmai_kaddr = hp->dmai_ibufp = NULL;
		hp->dmai_hds = segmentp;
		hp->dmai_rflags = dmareq->dmar_flags & DMP_DDIFLAGS;
		hp->dmai_minfo = (void *)mapinfo;
		hp->dmai_object = dmareq->dmar_object;
		if (mapinfo == DMAMI_PAGES) {
			segmentp->_vdmu._dmais_pp = php.ph_u.pp;
			segmentp->dmais_ofst = (uint_t)offset;
		} else {
			segmentp->_vdmu._dmais_va = php.ph_vaddr;
			segmentp->dmais_ofst = 0;
		}
		segmentp->_win._dmais_nex = NULL;
		segmentp->dmais_link = NULL;
		segmentp->_pdmu._dmais_lpd = padr;
		segmentp->dmais_size = size;
		segmentp->dmais_flags = DMAIS_WINSTRT | DMAIS_WINEND;
		segmentp->dmais_hndl = hp;
		*handlep = (ddi_dma_handle_t)hp;
		DMAPRINT(("	QUICKIE handle %x\n", hp));
		return (DDI_DMA_MAPPED);
	} else if (!handlep) {
		return (DDI_DMA_NOMAPPING);
	}
breakup_req:
	/*
	 * The function rootnex_get_phyaddr() does not save the physical
	 * address in the php structure. Save it here for
	 * rootnext_io_brkup_attr().
	 */
	php.ph_padr = padr;
	return (rootnex_io_brkup_lim(dip, rdip,  dmareq, handlep,
		dma_lim, &php));
}

/* CSTYLED */
#define	CAN_COMBINE(psegp, paddr, segsize, sgsize,  mxsegsize, attr, flg) \
((psegp)								&& \
((psegp)->_pdmu._dmais_lpd + (psegp)->dmais_size) == (paddr)	&& \
(((psegp)->dmais_flags & (DMAIS_NEEDINTBUF | DMAIS_COMPLEMENT)) == 0) && \
(((flg) & DMAIS_NEEDINTBUF) == 0)					&& \
(((psegp)->dmais_size + (segsize)) <= (mxsegsize))			&& \
((paddr) & (attr)->dma_attr_seg))

/* CSTYLED */
#define	MARK_WIN_END(segp, prvwinp, cwinp) \
(segp)->dmais_flags |= DMAIS_WINEND;	\
(prvwinp) = (cwinp);			\
(cwinp)->dmais_flags |= DMAIS_WINUIB;	\
(cwinp) = NULL;

/*
 * This function works with the ddi_dma_attr structure.
 * Bugs fixed
 * 1. The old code would ignore the size of the first segment when
 *	computing the total size of the reuqest (sglistsize) for sgllen == 1
 */

/*ARGSUSED*/
int
rootnex_io_brkup_attr(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep,
    struct priv_handle *php)
{
	impl_dma_segment_t *segmentp;
	impl_dma_segment_t *curwinp;
	impl_dma_segment_t *previousp;
	impl_dma_segment_t *prewinp;
	ddi_dma_cookie_t *cookiep;
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handlep;
	caddr_t basevadr;
	caddr_t segmentvadr;
	uint64_t segmentpadr;
	uint_t maxsegmentsize, sizesegment, residual_size;
	uint_t offset, needintbuf, sglistsize, trim;
	int nsegments;
	int mapinfo;
	int reqneedintbuf;
	int rval;
	int segment_flags, win_flags;
	int sgcount;
	int wcount;
	ddi_dma_attr_t *dma_attr = &hp->dmai_attr;
	int sizehandle;

#ifdef lint
	dip = dip;
#endif

	/*
	 * Initialize our local variables from the php structure.
	 * rootnex_get_phyaddr() has populated php structure on its
	 * previous invocation in rootnex_dma_bindhdl().
	 */
	residual_size = OBJSIZE;
	mapinfo = php->ph_mapinfo;
	segmentpadr = php->ph_padr;
	segmentvadr =  php->ph_vaddr;
	basevadr = (mapinfo == DMAMI_PAGES) ? 0 : segmentvadr;
	offset = segmentpadr & MMU_PAGEOFFSET;
	/*
	 * maxsegmentsize was computed and saved in rootnex_dma_allochdl().
	 */
	maxsegmentsize = hp->dmai_segmentsize;

	/*
	 * The number of segments is the number of 4k pages that the
	 * object spans.
	 * Each 4k segment may need another segment to satisfy
	 * device granularity reqirements.
	 * We will never need more than two segments per page.
	 * This may be an overestimate in some cases but it avoids
	 * 64 bit divide operations.
	 */
	nsegments = (offset + residual_size + MMU_PAGEOFFSET) >>
	    (MMU_PAGESHIFT - 1);



	sizehandle = nsegments * (sizeof (impl_dma_segment_t) +
		    sizeof (ddi_dma_cookie_t));

	hp->dmai_segp = kmem_zalloc(sizehandle,
	    (dmareq->dmar_fp == DDI_DMA_SLEEP) ? KM_SLEEP : KM_NOSLEEP);
	if (!hp->dmai_segp) {
		rval = DDI_DMA_NORESOURCES;
		goto bad;
	}
	hp->dmai_kmsize = sizehandle;
	segmentp = (impl_dma_segment_t *)hp->dmai_segp;
	cookiep = (ddi_dma_cookie_t *)(segmentp + nsegments);
	hp->dmai_cookie = cookiep;
	hp->dmai_wins = NULL;
	hp->dmai_kaddr = hp->dmai_ibufp = NULL;
	hp->dmai_hds = prewinp = segmentp;
	hp->dmai_rflags = dmareq->dmar_flags & DMP_DDIFLAGS;
	hp->dmai_minfo = (void *)mapinfo;
	hp->dmai_object = dmareq->dmar_object;

	/*
	 * Breakup the memory object
	 * and build an i/o segment at each boundary condition
	 */
	curwinp = 0;
	needintbuf = 0;
	previousp = 0;
	reqneedintbuf = 0;
	sglistsize = 0;
	wcount = 0;
	sgcount = 1;
	do {
		sizesegment = MIN((MMU_PAGESIZE - offset), residual_size);
		segment_flags = CMP64GT(segmentpadr, AHI_ATTR) ?
		    DMAIS_NEEDINTBUF : 0;
		sglistsize += sizesegment;
		if (sglistsize >= dma_attr->dma_attr_maxxfer) {
			/*
			 * limit the number of bytes to dma_attr_maxxfer
			 */
			sizesegment -=
			    (sglistsize - dma_attr->dma_attr_maxxfer);
			sglistsize = dma_attr->dma_attr_maxxfer;
			sgcount = dma_attr->dma_attr_sgllen + 1;
		}
		if ((dma_attr->dma_attr_sgllen == 1) &&
		    (segmentpadr & (dma_attr->dma_attr_granular - 1)) &&
		    (residual_size != sizesegment)) {
			/*
			 * _no_ scatter/gather capability,
			 * so ensure that size of each segment is a
			 * multiple of dma_attr_granular (== sector size)
			 */
			sizesegment = MIN((uint_t)MMU_PAGESIZE, residual_size);
			segment_flags |= DMAIS_NEEDINTBUF;
			sglistsize = sizesegment;
		}
		if (CAN_COMBINE(previousp, segmentpadr, sizesegment,
		    sglistsize, maxsegmentsize, dma_attr, segment_flags)) {
		    previousp->dmais_flags |= segment_flags;
		    previousp->dmais_size += sizesegment;
		    previousp->dmais_cookie->dmac_size += sizesegment;
		} else {
		    if (dma_attr->dma_attr_sgllen == 1)
			/*
			 * If we can not combine this segment with the
			 * previous segment or if there are no previous
			 * segments, sglistsize should be set to
			 * segmentsize.
			 */
			sglistsize = sizesegment;

		    if (previousp) {
			previousp->dmais_link = segmentp;
		    }
		    segmentp->dmais_cookie = cookiep;
		    segmentp->dmais_hndl = hp;
		    if (curwinp == 0) {
			prewinp->_win._dmais_nex = curwinp = segmentp;
			segment_flags |= DMAIS_WINSTRT;
			win_flags = segment_flags;
			wcount++;
		    } else {
			segmentp->_win._dmais_cur = curwinp;
			win_flags |= segment_flags;
		    }
		    segmentp->dmais_ofst = segmentvadr - basevadr;
		    if (mapinfo == DMAMI_PAGES)
			segmentp->_vdmu._dmais_pp = php->ph_u.pp;
		    else
			segmentp->_vdmu._dmais_va = (caddr_t)segmentvadr;
		    segmentp->_pdmu._dmais_lpd = segmentpadr;
		    segmentp->dmais_flags = (ushort_t)segment_flags;
		    segmentp->dmais_size = sizesegment;
		    cookiep->dmac_laddress = segmentpadr;
		    cookiep->dmac_type = (ulong_t)segmentp;
		    cookiep->dmac_size = sizesegment;
		    cookiep++;
		    --nsegments;
		    if (dma_attr->dma_attr_sgllen > 1)
			sgcount++;
		    if (segment_flags & DMAIS_NEEDINTBUF) {
			if ((dma_attr->dma_attr_sgllen > 1) &&
			    (needintbuf += ptob(btopr(sizesegment)))
				== MAX_INT_BUF) {
				/*
				 * Intermediate buffers need not be contiguous.
				 * we allocate a page of intermediate buffer
				 * for every segment.
				 */
			    reqneedintbuf = needintbuf;
			    needintbuf = 0;
			    sgcount = dma_attr->dma_attr_sgllen + 1;
			    MARK_WIN_END(segmentp, prewinp, curwinp);
			} else if (dma_attr->dma_attr_sgllen == 1) {
			    needintbuf = MMU_PAGESIZE;
			    MARK_WIN_END(segmentp, prewinp, curwinp);
			}
		    }
		    previousp = segmentp++;
		}

		if (sgcount > dma_attr->dma_attr_sgllen) {
		    previousp->dmais_flags |= DMAIS_COMPLEMENT;
		    sgcount = 1;
		    if ((sizesegment != residual_size)	&&
		    (trim = (sglistsize & (dma_attr->dma_attr_granular - 1)))) {
			/*
			 * end of a scatter/gather list!
			 * ensure that total length of list is a
			 * multiple of granular (sector size)
			 */
			previousp->dmais_size -= trim;
			previousp->dmais_cookie->dmac_size -= trim;
			sizesegment -= trim;
		    }
		    sglistsize = 0;
		}
		if (sizesegment && (residual_size -= sizesegment)) {
			/*
			 * Get the physical address of the next page in the
			 * dma object.
			 */
			segmentpadr =
			    rootnex_get_phyaddr(dmareq, sizesegment, php);
			offset = segmentpadr & MMU_PAGEOFFSET;
			segmentvadr += sizesegment;
		}
	} while (residual_size && nsegments);
	ASSERT(residual_size == 0);

	previousp->dmais_link = NULL;
	previousp->dmais_flags |= DMAIS_WINEND;
	if (curwinp) {
		if (win_flags & DMAIS_NEEDINTBUF)
			curwinp->dmais_flags |= DMAIS_WINUIB;
		curwinp->_win._dmais_nex = NULL;
	} else
		prewinp->_win._dmais_nex = NULL;

	if ((needintbuf = MAX(needintbuf, reqneedintbuf)) != 0) {
		uint64_t	saved_align;

		saved_align = dma_attr->dma_attr_align;
		/*
		 * Allocate intermediate buffer. To start with we request
		 * for a page aligned area. This request is satisfied from
		 * the system page free list pool.
		 */
		dma_attr->dma_attr_align = MMU_PAGESIZE;
		if (i_ddi_mem_alloc(dip, dma_attr, needintbuf,
		    (dmareq->dmar_fp == DDI_DMA_SLEEP) ? 0x1 : 0, 1, 0,
		    &hp->dmai_ibufp, (ulong_t *)&hp->dmai_ibfsz,
		    NULL) != DDI_SUCCESS) {
			/*
			 * The caller did not want to wait.
			 * We failed to allocate from the system pool
			 * of pages, try allocating from the lomem pool by
			 * specifying an alignment of 16 bytes.
			 * When we specify an alignment of 16 bytes or less
			 * i_ddi_mem_alloc() will try to allocate memory from
			 * lomem pool before returning a failure status.
			 * We require 4K aligned address, since allocations
			 * from lomem pool are only 16 byte aligned, we
			 * request for an additional 4K page.
			 */
			dma_attr->dma_attr_align = 16;
			if (i_ddi_mem_alloc(dip, dma_attr,
			    needintbuf + MMU_PAGESIZE,
			    (dmareq->dmar_fp == DDI_DMA_SLEEP) ? 0x1 : 0, 1, 0,
			    &hp->dmai_ibufp, (ulong_t *)&hp->dmai_ibfsz,
			    NULL) != DDI_SUCCESS) {
				dma_attr->dma_attr_align = saved_align;
				rval = DDI_DMA_NORESOURCES;
				goto bad;
			}
		}
		if (mapinfo != DMAMI_KVADR) {
			hp->dmai_kaddr = vmem_alloc(heap_arena, PAGESIZE,
			    VM_SLEEP);
		}
		dma_attr->dma_attr_align = saved_align;
	}

	/*
	 * return success
	 */
	ASSERT(wcount > 0);
	if (wcount == 1) {
		hp->dmai_rflags &= ~DDI_DMA_PARTIAL;
		rval = DDI_DMA_MAPPED;
	} else if (hp->dmai_rflags & DDI_DMA_PARTIAL) {
		rval = DDI_DMA_PARTIAL_MAP;
	} else {
		if (hp->dmai_segp)
			kmem_free((caddr_t)hp->dmai_segp,
			    hp->dmai_kmsize);
		return (DDI_DMA_TOOBIG);
	}
	hp->dmai_nwin = wcount;
	return (rval);
bad:
	hp->dmai_cookie = NULL;
	if (hp->dmai_segp)
		kmem_free((caddr_t)hp->dmai_segp, hp->dmai_kmsize);
	if (rval == DDI_DMA_NORESOURCES 	&&
	    dmareq->dmar_fp != DDI_DMA_DONTWAIT &&
	    dmareq->dmar_fp != DDI_DMA_SLEEP)
		ddi_set_callback(dmareq->dmar_fp, dmareq->dmar_arg,
		    &dvma_call_list_id);
	return (rval);
}




/*
 * This function works with the limit structure and does 32 bit arithmetic.
 */

int
rootnex_io_brkup_lim(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep,
    ddi_dma_lim_t *dma_lim, struct priv_handle *php)
{
	impl_dma_segment_t *segmentp;
	impl_dma_segment_t *curwinp;
	impl_dma_segment_t *previousp;
	impl_dma_segment_t *prewinp;
	ddi_dma_impl_t *hp = 0;
	caddr_t basevadr;
	caddr_t segmentvadr;
	uint64_t segmentpadr;
	uint_t maxsegmentsize, sizesegment;
	uint_t needintbuf;
	uint_t offset;
	uint_t residual_size;
	uint_t sglistsize;
	int nsegments;
	int mapinfo;
	int reqneedintbuf;
	int rval;
	int segment_flags, win_flags;
	int sgcount;
	int wcount;
#ifdef DMADEBUG
	int numsegments;
#endif
	int sizehandle;

#ifdef lint
	dip = dip;
#endif

	/*
	 * Validate the dma request.
	 */
#ifdef DMADEBUG
	if (dma_lim->dlim_adreg_max < MMU_PAGEOFFSET ||
	    dma_lim->dlim_ctreg_max < MMU_PAGEOFFSET ||
	    dma_lim->dlim_granular > MMU_PAGESIZE ||
	    dma_lim->dlim_reqsize < MMU_PAGESIZE) {
		DMAPRINT((" bad_limits\n"));
		return (DDI_DMA_BADLIMITS);
	}
#endif

	/*
	 * Initialize our local variables from the php structure.
	 * rootnex_get_phyaddr() has populated php structure on its
	 * previous invocation in rootnex_dma_map().
	 */
	residual_size = OBJSIZE;
	mapinfo = php->ph_mapinfo;
	segmentpadr = php->ph_padr;
	segmentvadr =  php->ph_vaddr;
	basevadr = (mapinfo == DMAMI_PAGES) ? 0 : segmentvadr;
	offset = segmentpadr & MMU_PAGEOFFSET;
	if (dma_lim->dlim_sgllen <= 0 ||
	    (offset & (dma_lim->dlim_minxfer - 1))) {
		DMAPRINT((" bad_limits/mapping\n"));
		rval = DDI_DMA_NOMAPPING;
		goto bad;
	}

	maxsegmentsize = MIN(dma_lim->dlim_adreg_max,
	    MIN((dma_lim->dlim_ctreg_max + 1) * dma_lim->dlim_minxfer,
	    dma_lim->dlim_reqsize) - 1) + 1;
	if (maxsegmentsize == 0)
		maxsegmentsize = FOURG - 1;
	if (maxsegmentsize < MMU_PAGESIZE) {
		DMAPRINT((" bad_limits, maxsegmentsize\n"));
		rval = DDI_DMA_BADLIMITS;
		goto bad;
	}


	/*
	 * The number of segments is the number of 4k pages that the
	 * object spans.
	 * Each 4k segment may need another segment to satisfy
	 * device granularity reqirements.
	 * We will never need more than two segments per page.
	 * This may be an overestimate in some cases but it avoids
	 * 64 bit divide operations.
	 */
	nsegments = (offset + residual_size + MMU_PAGEOFFSET) >>
	    (MMU_PAGESHIFT - 1);

#ifdef DMADEBUG
	numsegments = nsegments;
#endif
	ASSERT(nsegments > 0);


	sizehandle = sizeof (ddi_dma_impl_t) +
		(nsegments * sizeof (impl_dma_segment_t));

	hp = (ddi_dma_impl_t *)kmem_alloc(sizehandle,
		(dmareq->dmar_fp == DDI_DMA_SLEEP) ?
		KM_SLEEP : KM_NOSLEEP);
	if (!hp) {
		rval = DDI_DMA_NORESOURCES;
		goto bad;
	}
	hp->dmai_kmsize = sizehandle;

	/*
	 * locate segments after dma_impl handle structure
	 */
	segmentp = (impl_dma_segment_t *)(hp + 1);

	/*
	 * Save requestor's information
	 */
	hp->dmai_minxfer = dma_lim->dlim_minxfer;
	hp->dmai_burstsizes = dma_lim->dlim_burstsizes;
	hp->dmai_rdip = rdip;
	hp->dmai_mctl = rootnex_dma_mctl;
	hp->dmai_wins = NULL;
	hp->dmai_kaddr = hp->dmai_ibufp = NULL;
	hp->dmai_hds = prewinp = segmentp;
	hp->dmai_rflags = dmareq->dmar_flags & DMP_DDIFLAGS;
	hp->dmai_minfo = (void *)mapinfo;
	hp->dmai_object = dmareq->dmar_object;

	/*
	 * Breakup the memory object
	 * and build an i/o segment at each boundary condition
	 */
	curwinp = 0;
	needintbuf = 0;
	previousp = 0;
	reqneedintbuf = 0;
	sglistsize = 0;
	wcount = 0;
	sgcount = 1;
	do {
		sizesegment =
		    MIN(((uint_t)MMU_PAGESIZE - offset), residual_size);
		segment_flags = CMP64GT(segmentpadr, AHI_LIM) ?
		    DMAIS_NEEDINTBUF : 0;

		if (dma_lim->dlim_sgllen == 1) {
			/*
			 * _no_ scatter/gather capability,
			 * so ensure that size of each segment is a
			 * multiple of dlim_granular (== sector size)
			 */
			if ((segmentpadr & (dma_lim->dlim_granular - 1)) &&
			    residual_size != sizesegment) {
				/*
				 * this segment needs an intermediate buffer
				 */
				sizesegment =
				    MIN((uint_t)MMU_PAGESIZE, residual_size);
				segment_flags |= DMAIS_NEEDINTBUF;
			}
		}

		if (previousp &&
		    (previousp->_pdmu._dmais_lpd + previousp->dmais_size) ==
		    segmentpadr &&
		    (previousp->dmais_flags &
		    (DMAIS_NEEDINTBUF | DMAIS_COMPLEMENT)) == 0 &&
		    (segment_flags & DMAIS_NEEDINTBUF) == 0 &&
		    (previousp->dmais_size + sizesegment) <= maxsegmentsize &&
		    (segmentpadr & dma_lim->dlim_adreg_max) &&
		    (sglistsize + sizesegment) <= dma_lim->dlim_reqsize) {
			/*
			 * combine new segment with previous segment
			 */
			previousp->dmais_flags |= segment_flags;
			previousp->dmais_size += sizesegment;
			if ((sglistsize += sizesegment) ==
			    dma_lim->dlim_reqsize)
				/*
				 * force end of scatter/gather list
				 */
				sgcount = dma_lim->dlim_sgllen + 1;
		} else {
			/*
			 * add new segment to linked list
			 */
			if (previousp) {
				previousp->dmais_link = segmentp;
			}
			segmentp->dmais_hndl = hp;
			if (curwinp == 0) {
				prewinp->_win._dmais_nex =
				    curwinp = segmentp;
				segment_flags |= DMAIS_WINSTRT;
				win_flags = segment_flags;
				wcount++;
			} else {
				segmentp->_win._dmais_cur = curwinp;
				win_flags |= segment_flags;
			}
			segmentp->dmais_ofst = segmentvadr - basevadr;
			if (mapinfo == DMAMI_PAGES) {
				segmentp->_vdmu._dmais_pp = php->ph_u.pp;
			} else {
				segmentp->_vdmu._dmais_va = segmentvadr;
			}
			segmentp->_pdmu._dmais_lpd = segmentpadr;
			segmentp->dmais_flags = (ushort_t)segment_flags;

			if (dma_lim->dlim_sgllen > 1) {
				if (segment_flags & DMAIS_NEEDINTBUF) {
					needintbuf += ptob(btopr(sizesegment));
					if (needintbuf >= MAX_INT_BUF) {
						/*
						 * limit size of intermediate
						 * buffer
						 */
						reqneedintbuf = MAX_INT_BUF;
						needintbuf = 0;
						/*
						 * end of current window
						 */
						segmentp->dmais_flags |=
						    DMAIS_WINEND;
						prewinp = curwinp;
						curwinp->dmais_flags |=
						    DMAIS_WINUIB;
						curwinp = NULL;
						/*
						 * force end of scatter/gather
						 * list
						 */
						sgcount = dma_lim->dlim_sgllen;
					}
				}
				sglistsize += sizesegment;
				if (sglistsize >= dma_lim->dlim_reqsize) {
					/*
					 * limit size of xfer
					 */
					sizesegment -= (sglistsize -
					    dma_lim->dlim_reqsize);
					sglistsize = dma_lim->dlim_reqsize;
					sgcount = dma_lim->dlim_sgllen;
				}
				sgcount++;
			} else {
				/*
				 * _no_ scatter/gather capability,
				 */
				if (segment_flags & DMAIS_NEEDINTBUF) {
					/*
					 * end of window
					 */
					needintbuf = MMU_PAGESIZE;
					segmentp->dmais_flags |= DMAIS_WINEND;
					prewinp = curwinp;
					curwinp->dmais_flags |= DMAIS_WINUIB;
					curwinp = NULL;
				}
			}
			segmentp->dmais_size = sizesegment;
			previousp = segmentp++;
			--nsegments;
		}

		if (sgcount > dma_lim->dlim_sgllen) {
			/*
			 * end of a scatter/gather list!
			 * ensure that total length of list is a
			 * multiple of granular (sector size)
			 */
			if (sizesegment != residual_size) {
				uint_t trim;

				trim = sglistsize &
				    (dma_lim->dlim_granular - 1);
				if (trim >= sizesegment) {
					cmn_err(CE_WARN,
					    "unable to reduce segment size\n");
					rval = DDI_DMA_NOMAPPING;
					goto bad;
				}
				previousp->dmais_size -= trim;
				sizesegment -= trim;
				/* start new scatter/gather list */
				sgcount = 1;
				sglistsize = 0;
			}
			previousp->dmais_flags |= DMAIS_COMPLEMENT;
		}
		if (sizesegment && (residual_size -= sizesegment)) {
			segmentpadr =
			    rootnex_get_phyaddr(dmareq, sizesegment, php);
			offset = segmentpadr & MMU_PAGEOFFSET;
			segmentvadr += sizesegment;
		}
	} while (residual_size && nsegments);
	ASSERT(residual_size == 0);

	previousp->dmais_link = NULL;
	previousp->dmais_flags |= DMAIS_WINEND;
	if (curwinp) {
		if (win_flags & DMAIS_NEEDINTBUF)
			curwinp->dmais_flags |= DMAIS_WINUIB;
		curwinp->_win._dmais_nex = NULL;
	} else
		prewinp->_win._dmais_nex = NULL;

	if ((needintbuf = MAX(needintbuf, reqneedintbuf)) != 0) {
		ddi_dma_attr_t dma_attr;


		dma_attr.dma_attr_version = DMA_ATTR_V0;
		dma_attr.dma_attr_addr_lo = dma_lim->dlim_addr_lo;
		dma_attr.dma_attr_addr_hi = dma_lim->dlim_addr_hi;
		dma_attr.dma_attr_minxfer = dma_lim->dlim_minxfer;
		dma_attr.dma_attr_seg = dma_lim->dlim_adreg_max;
		dma_attr.dma_attr_count_max = dma_lim->dlim_ctreg_max;
		dma_attr.dma_attr_granular = dma_lim->dlim_granular;
		dma_attr.dma_attr_sgllen = dma_lim->dlim_sgllen;
		dma_attr.dma_attr_maxxfer = dma_lim->dlim_reqsize;
		dma_attr.dma_attr_burstsizes = dma_lim->dlim_burstsizes;
		dma_attr.dma_attr_align = MMU_PAGESIZE;
		dma_attr.dma_attr_flags = 0;

		/*
		 * Allocate intermediate buffer.
		 * If the device can't DMA above 16Mb, ask for memory from
		 * the lomem pool. This strategy is a work around for
		 * drivers that are broken. Eating memory from the kmem pool
		 * has a bad effect on these drivers.
		 * If the drivers can DMA above 16M, we request
		 * for a page aligned area. This request is satisfied from
		 * the system page free list pool.
		 */
		if ((dma_lim->dlim_addr_hi <= SIXTEEN_MB) ||
		    (i_ddi_mem_alloc(dip, &dma_attr, needintbuf,
		    (dmareq->dmar_fp == DDI_DMA_SLEEP) ? 0x1 : 0, 1, 0,
		    &hp->dmai_ibufp, (ulong_t *)&hp->dmai_ibfsz,
		    NULL) != DDI_SUCCESS)) {
			/*
			 * The caller did not want to wait.
			 * We failed to allocate from the system pool
			 * of pages, try allocating from the lomem pool by
			 * specifying an alignment of 16 bytes.
			 * When we specify an alignment of 16 bytes or less
			 * i_ddi_mem_alloc() will try to allocate memory from
			 * lomem pool before returning a failure status.
			 * We require 4K aligned address, since allocations
			 * from lomem pool are only 16 byte aligned, we
			 * request for an additional 4K page.
			 */
			dma_attr.dma_attr_align = 16;
			dma_attr.dma_attr_sgllen = 1;
			if (i_ddi_mem_alloc(dip, &dma_attr,
			    needintbuf + MMU_PAGESIZE,
			    (dmareq->dmar_fp == DDI_DMA_SLEEP) ? 0x1 : 0, 1, 0,
			    &hp->dmai_ibufp, (ulong_t *)&hp->dmai_ibfsz,
			    NULL) != DDI_SUCCESS) {
				rval = DDI_DMA_NORESOURCES;
				goto bad;
			}
		}
		if (mapinfo != DMAMI_KVADR) {
			hp->dmai_kaddr = vmem_alloc(heap_arena, PAGESIZE,
			    VM_SLEEP);
		}
	}

	/*
	 * return success
	 */
#ifdef DMADEBUG
	DMAPRINT(("dma_brkup: handle %x nsegments %x \n",
	    hp, numsegments - nsegments));
#endif
	hp->dmai_cookie = NULL;
	*handlep = (ddi_dma_handle_t)hp;
	return (DDI_DMA_MAPPED);
bad:
	if (hp)
		kmem_free((caddr_t)hp, hp->dmai_kmsize);
	if (rval == DDI_DMA_NORESOURCES 	&&
	    dmareq->dmar_fp != DDI_DMA_DONTWAIT &&
	    dmareq->dmar_fp != DDI_DMA_SLEEP)
		ddi_set_callback(dmareq->dmar_fp, dmareq->dmar_arg,
		    &dvma_call_list_id);
	return (rval);
}

int
rootnex_io_wtsync(ddi_dma_impl_t *hp, int type)
{
	impl_dma_segment_t *sp = hp->dmai_wins;
	caddr_t	kviradr, addr;
	caddr_t vsrc;
	ulong_t segoffset, vsoffset;
	int cpycnt;

	addr = hp->dmai_ibufp;
	if ((uint_t)addr & MMU_PAGEOFFSET) {
	    addr = (caddr_t)(((uint_t)addr + MMU_PAGEOFFSET) & ~MMU_PAGEOFFSET);
	}
	if ((sp->dmais_flags & DMAIS_WINUIB) == 0)
		return (DDI_SUCCESS);

	switch ((int)hp->dmai_minfo) {

	case DMAMI_KVADR:
		do if (sp->dmais_flags & DMAIS_NEEDINTBUF) {

			if (hp->dmai_rflags & DDI_DMA_WRITE)
				/*
				 *  copy from segment to buffer
				 */
				bcopy(sp->_vdmu._dmais_va, addr,
				    sp->dmais_size);
			/*
			 * save phys addr of intemediate buffer
			 */
			sp->_pdmu._dmais_lpd = ptob64(hat_getkpfnum(addr));
			if (type == BIND) {
				sp->dmais_cookie->dmac_laddress =
					sp->_pdmu._dmais_lpd;
			}
			addr += MMU_PAGESIZE;
		} while (!(sp->dmais_flags & DMAIS_WINEND) &&
		    (sp = sp->dmais_link));
		break;

	case DMAMI_PAGES:
		do if (sp->dmais_flags & DMAIS_NEEDINTBUF) {

			if (hp->dmai_rflags & DDI_DMA_WRITE) {
				/*
				 * need to mapin page so we can have a
				 * virtual address to do copying
				 */
				i86_pp_map(sp->_vdmu._dmais_pp, hp->dmai_kaddr);
				/*
				 *  copy from segment to buffer
				 */
				bcopy(hp->dmai_kaddr +
				    (sp->dmais_ofst & MMU_PAGEOFFSET),
				    addr, sp->dmais_size);
				/*
				 *  need to mapout page
				 */
				hat_unload(kas.a_hat, hp->dmai_kaddr,
				    MMU_PAGESIZE, HAT_UNLOAD);
			}
			/*
			 * save phys addr of intemediate buffer
			 */
			sp->_pdmu._dmais_lpd = ptob64(hat_getkpfnum(addr));
			if (type == BIND) {
				sp->dmais_cookie->dmac_laddress =
					sp->_pdmu._dmais_lpd;
			}
			addr += MMU_PAGESIZE;
		} while (!(sp->dmais_flags & DMAIS_WINEND) &&
		    (sp = sp->dmais_link));
		break;

	case DMAMI_UVADR:
		do if (sp->dmais_flags & DMAIS_NEEDINTBUF) {

			if (hp->dmai_rflags & DDI_DMA_WRITE) {
				struct page **pplist;
				segoffset = 0;
				do {
					/*
					 * need to mapin page so we can have a
					 * virtual address to do copying
					 */
					vsrc = sp->_vdmu._dmais_va + segoffset;
					vsoffset =
					    (ulong_t)vsrc & MMU_PAGEOFFSET;
					pplist = hp->dmai_object.dmao_obj.
							virt_obj.v_priv;
					/*
					 * check if we have to use the
					 * shadow list or the CPU mapping.
					 */
					if (pplist != NULL) {
						ulong_t base, off;

						base = (ulong_t)hp->dmai_object.
						    dmao_obj.virt_obj.v_addr;
						off = (base & MMU_PAGEOFFSET) +
							(ulong_t)vsrc - base;
						i86_pp_map(pplist[btop(off)],
							hp->dmai_kaddr);
					} else {
						i86_va_map(vsrc,
						    hp->dmai_object.dmao_obj.
							virt_obj.v_as,
						    hp->dmai_kaddr);
					}
					kviradr = hp->dmai_kaddr + vsoffset;
					cpycnt = sp->dmais_size - segoffset;
					if (vsoffset + cpycnt > MMU_PAGESIZE)
						cpycnt = MMU_PAGESIZE -
						    vsoffset;
					/*
					 *  copy from segment to buffer
					 */
					bcopy(kviradr, addr + segoffset,
					    cpycnt);
					/*
					 *  need to mapout page
					 */
					hat_unload(kas.a_hat, hp->dmai_kaddr,
					    MMU_PAGESIZE, HAT_UNLOAD);
					segoffset += cpycnt;
				} while (segoffset < sp->dmais_size);
			}
			/*
			 * save phys addr of intermediate buffer
			 */
			sp->_pdmu._dmais_lpd = ptob64(hat_getkpfnum(addr));
			if (type == BIND) {
				sp->dmais_cookie->dmac_laddress =
					sp->_pdmu._dmais_lpd;
			}
			addr += MMU_PAGESIZE;
		} while (!(sp->dmais_flags & DMAIS_WINEND) &&
		    (sp = sp->dmais_link));
		break;

	default:
		cmn_err(CE_WARN, "Invalid dma handle/map info\n");
	}
	return (DDI_SUCCESS);
}

int
rootnex_io_rdsync(ddi_dma_impl_t *hp)
{
	impl_dma_segment_t *sp = hp->dmai_wins;
	caddr_t	kviradr;
	caddr_t vdest, addr;
	ulong_t segoffset, vdoffset;
	int cpycnt;

	addr = hp->dmai_ibufp;
	if ((uint_t)addr & MMU_PAGEOFFSET) {
	    addr = (caddr_t)(((uint_t)addr + MMU_PAGEOFFSET) & ~MMU_PAGEOFFSET);
	}
	if (!(sp->dmais_flags & DMAIS_WINUIB) ||
			!(hp->dmai_rflags & DDI_DMA_READ))
		return (DDI_SUCCESS);

	switch ((int)hp->dmai_minfo) {

	case DMAMI_KVADR:
		do if (sp->dmais_flags & DMAIS_NEEDINTBUF) {
			/*
			 *  copy from buffer to segment
			 */
			bcopy(addr, sp->_vdmu._dmais_va, sp->dmais_size);
			addr += MMU_PAGESIZE;
		} while (!(sp->dmais_flags & DMAIS_WINEND) &&
		    (sp = sp->dmais_link));
		break;

	case DMAMI_PAGES:
		do if (sp->dmais_flags & DMAIS_NEEDINTBUF) {
			/*
			 * need to mapin page
			 */
			i86_pp_map(sp->_vdmu._dmais_pp, hp->dmai_kaddr);
			/*
			 *  copy from buffer to segment
			 */
			bcopy(addr,
			    (hp->dmai_kaddr +
				(sp->dmais_ofst & MMU_PAGEOFFSET)),
			    sp->dmais_size);

			/*
			 *  need to mapout page
			 */
			hat_unload(kas.a_hat, hp->dmai_kaddr,
			    MMU_PAGESIZE, HAT_UNLOAD);
			addr += MMU_PAGESIZE;
		} while (!(sp->dmais_flags & DMAIS_WINEND) &&
		    (sp = sp->dmais_link));
		break;

	case DMAMI_UVADR:
		do if (sp->dmais_flags & DMAIS_NEEDINTBUF) {
			struct page **pplist;
			segoffset = 0;
			do {
				/*
				 * need to map_in user virtual address
				 */
				vdest = sp->_vdmu._dmais_va + segoffset;
				vdoffset = (ulong_t)vdest & MMU_PAGEOFFSET;
				pplist = hp->dmai_object.dmao_obj.
						virt_obj.v_priv;
				/*
				 * check if we have to use the
				 * shadow list or the CPU mapping.
				 */
				if (pplist != NULL) {
					ulong_t base, off;

					base = (ulong_t)hp->dmai_object.
						dmao_obj.virt_obj.v_addr;
					off = (base & MMU_PAGEOFFSET) +
						(ulong_t)vdest - base;
					i86_pp_map(pplist[btop(off)],
						hp->dmai_kaddr);
				} else {
					i86_va_map(vdest,
					    hp->dmai_object.dmao_obj.
						virt_obj.v_as,
					    hp->dmai_kaddr);
				}
				kviradr = hp->dmai_kaddr + vdoffset;
				cpycnt = sp->dmais_size - segoffset;
				if (vdoffset + cpycnt > MMU_PAGESIZE)
					cpycnt = MMU_PAGESIZE - vdoffset;
				/*
				 *  copy from buffer to segment
				 */
				bcopy(addr + segoffset, kviradr, cpycnt);
				/*
				 *  need to map_out page
				 */
				hat_unload(kas.a_hat, hp->dmai_kaddr,
				    MMU_PAGESIZE, HAT_UNLOAD);
				segoffset += cpycnt;
			} while (segoffset < sp->dmais_size);
			addr += MMU_PAGESIZE;
		} while (!(sp->dmais_flags & DMAIS_WINEND) &&
		    (sp = sp->dmais_link));
		break;

	default:
		cmn_err(CE_WARN, "Invalid dma handle/map info\n");
	}
	return (DDI_SUCCESS);
}

static int
rootnex_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, size_t *lenp,
    caddr_t *objpp, uint_t cache_flags)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	impl_dma_segment_t *sp = (impl_dma_segment_t *)lenp;
	impl_dma_segment_t *wp = (impl_dma_segment_t *)offp;
	ddi_dma_cookie_t *cp;
	int rval = DDI_SUCCESS;

#ifdef lint
	dip = dip;
	rdip = rdip;
#endif

	DMAPRINT(("io_mctl: handle %x ", hp));

	switch (request) {

	case DDI_DMA_SEGTOC:
		/* return device specific dma cookie for segment */
		sp = (impl_dma_segment_t *)cache_flags;
		if (!sp) {
			DMAPRINT(("stoc segment %x end\n", (ulong_t)sp));
			return (DDI_FAILURE);
		}
		cp = (ddi_dma_cookie_t *)objpp;

		/*
		 * use phys addr of actual buffer or intermediate buffer
		 */
		cp->dmac_laddress = sp->_pdmu._dmais_lpd;

		DMAPRINT(("stoc segment %x mapping %x size %x\n",
		    (ulong_t)sp, (ulong_t)sp->_vdmu._dmais_va, sp->dmais_size));

		cp->dmac_type = (ulong_t)sp;
		*lenp = cp->dmac_size = sp->dmais_size;
		*offp = sp->dmais_ofst;
		return (DDI_SUCCESS);

	case DDI_DMA_NEXTSEG:	/* get next DMA segment	*/
		ASSERT(wp->dmais_flags & DMAIS_WINSTRT);
		if (wp != hp->dmai_wins) {
			DMAPRINT(("nxseg: not current window %x\n",
			    (ulong_t)wp));
			return (DDI_DMA_STALE);
		}
		if (!sp) {
			/*
			 * reset to first segment in current window
			 */
			*objpp = (caddr_t)wp;
		} else {
			if (sp->dmais_flags & DMAIS_WINEND) {
				DMAPRINT(("nxseg: seg %x eow\n", (ulong_t)sp));
				return (DDI_DMA_DONE);
			}
			/* check if segment is really in window */
			ASSERT((sp->dmais_flags & DMAIS_WINSTRT) && sp == wp ||
			    !(sp->dmais_flags & DMAIS_WINSTRT) &&
			    sp->_win._dmais_cur == wp);
			*objpp = (caddr_t)sp->dmais_link;
		}
		DMAPRINT(("nxseg: new seg %x\n", (ulong_t)*objpp));
		return (DDI_SUCCESS);

	case DDI_DMA_NEXTWIN:	/* get next DMA window	*/
		if (hp->dmai_wins && hp->dmai_ibufp)
			/*
			 * do implied sync on current window
			 */
			(void) rootnex_io_rdsync(hp);
		if (!wp) {
			/*
			 * reset to (first segment of) first window
			 */
			*objpp = (caddr_t)hp->dmai_hds;
			DMAPRINT(("nxwin: first win %x\n", (ulong_t)*objpp));
		} else {
			ASSERT(wp->dmais_flags & DMAIS_WINSTRT);
			if (wp != hp->dmai_wins) {
				DMAPRINT(("nxwin: win %x not current\n",
				    (ulong_t)wp));
				return (DDI_DMA_STALE);
			}
			if (wp->_win._dmais_nex == 0) {
				DMAPRINT(("nxwin: win %x end\n", (ulong_t)wp));
				return (DDI_DMA_DONE);
			}
			*objpp = (caddr_t)wp->_win._dmais_nex;
			DMAPRINT(("nxwin: new win %x\n", (ulong_t)*objpp));
		}
		hp->dmai_wins = (impl_dma_segment_t *)*objpp;
		if (hp->dmai_ibufp)
			return (rootnex_io_wtsync(hp, MAP));
		return (DDI_SUCCESS);

	case DDI_DMA_FREE:
		DMAPRINT(("free handle\n"));
		if (hp->dmai_ibufp) {
			rval = rootnex_io_rdsync(hp);
			ddi_mem_free(hp->dmai_ibufp);
		}
		if (hp->dmai_kaddr)
			vmem_free(heap_arena, hp->dmai_kaddr, PAGESIZE);
		kmem_free((caddr_t)hp, hp->dmai_kmsize);
		if (dvma_call_list_id)
			ddi_run_callback(&dvma_call_list_id);
		break;

	case DDI_DMA_IOPB_ALLOC:	/* get contiguous DMA-able memory */
		DMAPRINT(("iopb alloc\n"));
		rval = i_ddi_mem_alloc_lim(rdip, (ddi_dma_lim_t *)offp,
		    (uint_t)lenp, 0, 0, 0, objpp, (uint_t *)0, NULL);
		break;

	case DDI_DMA_SMEM_ALLOC:	/* get contiguous DMA-able memory */
		DMAPRINT(("mem alloc\n"));
		rval = i_ddi_mem_alloc_lim(rdip, (ddi_dma_lim_t *)offp,
				(uint_t)lenp, cache_flags, 1, 0, objpp,
				(uint_t *)handle, NULL);
		break;

	case DDI_DMA_KVADDR:
		DMAPRINT(("kvaddr of phys mapping\n"));
		return (DDI_FAILURE);

	case DDI_DMA_GETERR:
		DMAPRINT(("geterr\n"));
		rval = DDI_FAILURE;
		break;

	case DDI_DMA_COFF:
		DMAPRINT(("coff off %x mapping %llx size %x\n",
		    (ulong_t)*objpp, hp->dmai_wins->_pdmu._dmais_lpd,
		    hp->dmai_wins->dmais_size));
		rval = DDI_FAILURE;
		break;

	default:
		DMAPRINT(("unknown 0%x\n", request));
		return (DDI_FAILURE);
	}
	return (rval);
}

/*
 * Root nexus ctl functions
 */
#define	REPORTDEV_BUFSIZE	1024

static int
rootnex_ctl_reportdev(dev_info_t *dev)
{
	int i, n, len, f_len = 0;
	char *buf;

	buf = kmem_alloc(REPORTDEV_BUFSIZE, KM_SLEEP);
	f_len += snprintf(buf, REPORTDEV_BUFSIZE,
	    "%s%d at root", ddi_driver_name(dev), ddi_get_instance(dev));
	len = strlen(buf);

	for (i = 0; i < sparc_pd_getnreg(dev); i++) {

		struct regspec *rp = sparc_pd_getreg(dev, i);

		if (i == 0)
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    ": ");
		else
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    " and ");
		len = strlen(buf);

		switch (rp->regspec_bustype) {

		case BTEISA:
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    "%s 0x%x", DEVI_EISA_NEXNAME, rp->regspec_addr);
			break;

		case BTISA:
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    "%s 0x%x", DEVI_ISA_NEXNAME, rp->regspec_addr);
			break;

		default:
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    "space %x offset %x",
			    rp->regspec_bustype, rp->regspec_addr);
			break;
		}
		len = strlen(buf);
	}
	for (i = 0, n = sparc_pd_getnintr(dev); i < n; i++) {
		int pri;

		if (i != 0) {
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    ",");
			len = strlen(buf);
		}
		pri = INT_IPL(sparc_pd_getintr(dev, i)->intrspec_pri);
		f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
		    " sparc ipl %d", pri);
		len = strlen(buf);
	}
#ifdef DEBUG
	if (f_len + 1 >= REPORTDEV_BUFSIZE) {
		cmn_err(CE_NOTE, "next message is truncated: "
		    "printed length 1024, real length %d", f_len);
	}
#endif DEBUG
	cmn_err(CE_CONT, "?%s\n", buf);
	kmem_free(buf, REPORTDEV_BUFSIZE);
	return (DDI_SUCCESS);
}

/*
 * For the x86 rootnexus, we're prepared to claim that the interrupt string
 * is in the form of a list of <ipl,vec> specifications.
 */

#define	VEC_MIN	1
#define	VEC_MAX	255
static int
rootnex_xlate_intrs(dev_info_t *dip, dev_info_t *rdip, int *in,
	struct ddi_parent_private_data *pdptr)
{
	size_t size;
	int n;
	struct intrspec *new;
	caddr_t got_prop;
	int *inpri;
	int got_len;
	extern int ignore_hardware_nodes;	/* force flag from ddi_impl.c */

	static char bad_intr_fmt[] =
	    "rootnex: bad interrupt spec from %s%d - ipl %d, irq %d\n";

#ifdef	lint
	dip = dip;
#endif
	/*
	 * determine if the driver is expecting the new style "interrupts"
	 * property which just contains the IRQ, or the old style which
	 * contains pairs of <IPL,IRQ>.  if it is the new style, we always
	 * assign IPL 5 unless an "interrupt-priorities" property exists.
	 * in that case, the "interrupt-priorities" property contains the
	 * IPL values that match, one for one, the IRQ values in the
	 * "interrupts" property.
	 */
	inpri = NULL;
	if ((ddi_getprop(DDI_DEV_T_ANY, rdip, DDI_PROP_DONTPASS,
	    "ignore-hardware-nodes", -1) != -1) ||
	    ignore_hardware_nodes) {
		/* the old style "interrupts" property... */

		/*
		 * The list consists of <ipl,vec> elements
		 */
		if ((n = (*in++ >> 1)) < 1)
			return (DDI_FAILURE);

		pdptr->par_nintr = n;
		size = n * sizeof (struct intrspec);
		new = pdptr->par_intr = kmem_zalloc(size, KM_SLEEP);

		while (n--) {
			int level = *in++;
			int vec = *in++;

			if (level < 1 || level > MAXIPL ||
			    vec < VEC_MIN || vec > VEC_MAX) {
				cmn_err(CE_CONT, bad_intr_fmt,
				    DEVI(rdip)->devi_name,
				    DEVI(rdip)->devi_instance, level, vec);
				goto broken;
			}
			new->intrspec_pri = level;
			if (vec != 2)
				new->intrspec_vec = vec;
			else
				/*
				 * irq 2 on the PC bus is tied to irq 9
				 * on ISA, EISA and MicroChannel
				 */
				new->intrspec_vec = 9;
			new++;
		}

		return (DDI_SUCCESS);
	} else {
		/* the new style "interrupts" property... */

		/*
		 * The list consists of <vec> elements
		 */
		if ((n = (*in++)) < 1)
			return (DDI_FAILURE);

		pdptr->par_nintr = n;
		size = n * sizeof (struct intrspec);
		new = pdptr->par_intr = kmem_zalloc(size, KM_SLEEP);

		/* XXX check for "interrupt-priorities" property... */
		if (ddi_getlongprop(DDI_DEV_T_ANY, rdip, DDI_PROP_DONTPASS,
		    "interrupt-priorities", (caddr_t)&got_prop, &got_len)
		    == DDI_PROP_SUCCESS) {
			if (n != (got_len / sizeof (int))) {
				cmn_err(CE_CONT,
				    "rootnex: bad interrupt-priorities length"
				    " from %s%d: expected %d, got %d\n",
				    DEVI(rdip)->devi_name,
				    DEVI(rdip)->devi_instance, n,
				    (got_len / sizeof (int)));
				goto broken;
			}
			inpri = (int *)got_prop;
		}

		while (n--) {
			int level;
			int vec = *in++;

			if (inpri == NULL)
				level = 5;
			else
				level = *inpri++;

			if (level < 1 || level > MAXIPL ||
			    vec < VEC_MIN || vec > VEC_MAX) {
				cmn_err(CE_CONT, bad_intr_fmt,
				    DEVI(rdip)->devi_name,
				    DEVI(rdip)->devi_instance, level, vec);
				goto broken;
			}
			new->intrspec_pri = level;
			if (vec != 2)
				new->intrspec_vec = vec;
			else
				/*
				 * irq 2 on the PC bus is tied to irq 9
				 * on ISA, EISA and MicroChannel
				 */
				new->intrspec_vec = 9;
			new++;
		}

		if (inpri != NULL)
			kmem_free(got_prop, got_len);
		return (DDI_SUCCESS);
	}

broken:
	kmem_free(pdptr->par_intr, size);
	pdptr->par_intr = (void *)0;
	pdptr->par_nintr = 0;
	if (inpri != NULL)
		kmem_free(got_prop, got_len);
	return (DDI_FAILURE);
}

/*ARGSUSED*/
static int
rootnex_ctl_children(dev_info_t *dip, dev_info_t *rdip, ddi_ctl_enum_t ctlop,
    dev_info_t *child)
{
	extern int impl_ddi_sunbus_initchild(dev_info_t *);
	extern void impl_ddi_sunbus_removechild(dev_info_t *);

	switch (ctlop)  {

	case DDI_CTLOPS_INITCHILD:
		return (impl_ddi_sunbus_initchild(child));

	case DDI_CTLOPS_UNINITCHILD:
		impl_ddi_sunbus_removechild(child);
		return (DDI_SUCCESS);
	}

	return (DDI_FAILURE);
}


static int
rootnex_ctlops(dev_info_t *dip, dev_info_t *rdip,
    ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	int n, *ptr;
	struct ddi_parent_private_data *pdp;

	switch (ctlop) {
	case DDI_CTLOPS_DMAPMAPC:
		/*
		 * Return 'partial' to indicate that dma mapping
		 * has to be done in the main MMU.
		 */
		return (DDI_DMA_PARTIAL);

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

	/*
	 * XXX	This pokefault_mutex clutter needs to be done differently.
	 *	Note that i_ddi_poke() calls this routine in the order
	 *	INIT then optionally FLUSH then always FINI.
	 */
	case DDI_CTLOPS_POKE_INIT:
		mutex_enter(&pokefault_mutex);
		pokefault = -1;
		return (DDI_SUCCESS);

	case DDI_CTLOPS_POKE_FLUSH:
		return (DDI_FAILURE);

	case DDI_CTLOPS_POKE_FINI:
		pokefault = 0;
		mutex_exit(&pokefault_mutex);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INITCHILD:
	case DDI_CTLOPS_UNINITCHILD:
		return (rootnex_ctl_children(dip, rdip, ctlop, arg));

	case DDI_CTLOPS_REPORTDEV:
		return (rootnex_ctl_reportdev(rdip));

	case DDI_CTLOPS_IOMIN:
		/*
		 * Nothing to do here but reflect back..
		 */
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
	case DDI_CTLOPS_NINTRS:
		break;

	case DDI_CTLOPS_SIDDEV:
		if (ndi_dev_is_prom_node(rdip))
			return (DDI_SUCCESS);
		if (ndi_dev_is_persistent_node(rdip))
			return (DDI_SUCCESS);
		return (DDI_FAILURE);

	case DDI_CTLOPS_INTR_HILEVEL:
		/*
		 * Indicate whether the interrupt specified is to be handled
		 * above lock level.  In other words, above the level that
		 * cv_signal and default type mutexes can be used.
		 */
		*(int *)result =
		    (INT_IPL(((struct intrspec *)arg)->intrspec_pri)
		    > LOCK_LEVEL);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_XLATE_INTRS:
		return (rootnex_xlate_intrs(dip, rdip, arg, result));

	case DDI_CTLOPS_POWER:
	{
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
		ptr = (int *)result;
		*ptr = pdp->par_nintr;
	} else {
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

/*
 * Get the physical address of an object described by "dmareq".
 * A "segsize" of zero is used to initialize the priv_handle *php.
 * Subsequent calls with a non zero "segsize" would get the corresponding
 * physical address of the dma object.
 * The function returns a 64 bit physical address.
 */
uint64_t
rootnex_get_phyaddr(struct ddi_dma_req *dmareq, uint_t segsize,
struct priv_handle *php)
{
	uint_t	offset;
	page_t	*pp, **pplist;
	caddr_t  vaddr, bvaddr;
	struct as *asp;
	int	index;
	uint64_t segmentpadr;


	switch (dmareq->dmar_object.dmao_type) {
	case DMA_OTYP_PAGES:
		if (segsize) {
		    pp = php->ph_u.pp;
		    vaddr = php->ph_vaddr;
		    offset = (uint_t)vaddr & MMU_PAGEOFFSET;
		    vaddr += segsize;
		    if ((offset += segsize) >= MMU_PAGESIZE) {
			/*
			 * crossed a page boundary, get to the next page.
			 */
			offset &= MMU_PAGEOFFSET;
			pp = pp->p_next;
		    }
		} else {
		/*
		 * Initialize the priv_handle structure.
		 */
		    pp = dmareq->dmar_object.dmao_obj.pp_obj.pp_pp;
		    offset = dmareq->dmar_object.dmao_obj.pp_obj.pp_offset;
		    vaddr = (caddr_t)offset;
		    php->ph_mapinfo = DMAMI_PAGES;
		}
		php->ph_u.pp = pp;
		php->ph_vaddr = vaddr;
		segmentpadr = (uint64_t)offset + ptob64(page_pptonum(pp));
		break;
	case DMA_OTYP_VADDR:
		if (segsize) {
		    asp = php->ph_u.asp;
		    vaddr = php->ph_vaddr;
		    vaddr += segsize;
		} else {
		/*
		 * Initialize the priv_handle structure.
		 */
		    vaddr = dmareq->dmar_object.dmao_obj.virt_obj.v_addr;
		    asp = dmareq->dmar_object.dmao_obj.virt_obj.v_as;
		    if (asp == NULL) {
			php->ph_mapinfo = DMAMI_KVADR;
			asp = &kas;
		    } else php->ph_mapinfo = DMAMI_UVADR;
		    php->ph_u.asp = asp;
		}
		pplist = dmareq->dmar_object.dmao_obj.virt_obj.v_priv;
		offset = (uint_t)vaddr & MMU_PAGEOFFSET;
		if (pplist == NULL) {
			if (asp != &kas) {
				segmentpadr = (uint64_t)offset +
				ptob64(hat_getpfnum(asp->a_hat, vaddr));
			} else {
				segmentpadr = (uint64_t)offset +
				    ptob64(hat_getkpfnum(vaddr));
			}
		} else {
		    bvaddr = dmareq->dmar_object.dmao_obj.virt_obj.v_addr;
		    index = btop(((ulong_t)bvaddr & MMU_PAGEOFFSET) +
			vaddr - bvaddr);
		    segmentpadr = (uint64_t)offset +
			ptob64(page_pptonum(pplist[index]));
		}
		php->ph_vaddr = vaddr;
		break;
	}
	return (segmentpadr);
}
#ifndef	FIXED_4150246
int
cmp64lt(uint64_t a, uint64_t b)
{
	return (a < b);
}
int
cmp64gt(uint64_t a, uint64_t b)
{
	return (a > b);
}
#endif
