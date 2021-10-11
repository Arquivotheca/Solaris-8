/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)iommunex.c	1.94	99/08/28 SMI"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/ddidmareq.h>
#include <sys/devops.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_implfuncs.h>
#include <sys/modctl.h>
#include <sys/cpu.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <vm/seg.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <sys/autoconf.h>
#include <sys/avintr.h>
#include <sys/machsystm.h>
#include <sys/archsystm.h>
#include <sys/bt.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <vm/mach_page.h>

#include <vm/hat_srmmu.h>
#include <sys/iommu.h>
#include <sys/callb.h>

extern int do_pg_coloring;
extern void vac_color_sync();

#define	VCOLOR(pp) (((machpage_t *)pp)->p_vcolor << MMU_PAGESHIFT)
#define	ALIGNED(pp, addr) (VCOLOR(pp) == (addr & vac_mask))

static int
iommunex_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep);

static int
iommunex_dma_allochdl(dev_info_t *, dev_info_t *, ddi_dma_attr_t *,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *);

static int
iommunex_dma_freehdl(dev_info_t *, dev_info_t *, ddi_dma_handle_t);

static int
iommunex_dma_bindhdl(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
    struct ddi_dma_req *, ddi_dma_cookie_t *, uint_t *);

static int
iommunex_dma_unbindhdl(dev_info_t *, dev_info_t *, ddi_dma_handle_t);

static int
iommunex_dma_flush(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
    off_t, size_t, uint_t);

static int
iommunex_dma_win(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
    uint_t, off_t *, size_t *, ddi_dma_cookie_t *, uint_t *);

static int
iommunex_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, size_t *lenp, caddr_t *objp, uint_t cache_flags);

static int
iommunex_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t,
    void *, void *);

static struct bus_ops iommunex_bus_ops = {
	BUSO_REV,
	i_ddi_bus_map,
	i_ddi_get_intrspec,
	i_ddi_add_intrspec,
	i_ddi_remove_intrspec,
	i_ddi_map_fault,
	iommunex_dma_map,
	iommunex_dma_allochdl,
	iommunex_dma_freehdl,
	iommunex_dma_bindhdl,
	iommunex_dma_unbindhdl,
	iommunex_dma_flush,
	iommunex_dma_win,
	iommunex_dma_mctl,
	iommunex_ctlops,
	ddi_bus_prop_op,
	0,			/* (*bus_get_eventcookie)();	*/
	0,			/* (*bus_add_eventcall)();	*/
	0,			/* (*bus_remove_eventcall)();	*/
	0			/* (*bus_post_event)();		*/
};

static int iommunex_identify(dev_info_t *);
static int iommunex_attach(dev_info_t *, ddi_attach_cmd_t);

static struct dev_ops iommu_ops = {
	DEVO_REV,
	0,		/* refcnt */
	ddi_no_info,	/* info */
	iommunex_identify,
	0,		/* probe */
	iommunex_attach,
	nodev,		/* detach */
	nodev,		/* reset */
	0,		/* cb_ops */
	&iommunex_bus_ops
};

static struct modldrv modldrv = {
	&mod_driverops,	/* Type of module.  This one is a driver */
	"iommu nexus driver 1.94", /* Name of module. */
	&iommu_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

/*
 * This is the driver initialization routine.
 */

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

extern int tsunami_control_store_bug;
extern int swift_tlb_flush_bug;

/*
 * protected by ddi_callback_mutex (in ddi_set_callback(),
 * and in real_callback_run())
 */
static uintptr_t dvma_call_list_id = 0;

static int dma_reserve = SBUSMAP_MAXRESERVE;

static kmutex_t dma_pool_lock;

static ulong_t dump_dvma;
extern size_t dump_reserve;		/* machdep.c */


/*ARGSUSED*/
static boolean_t
iommunex_dump_done(void *arg, int code)
{
	if (code == CB_CODE_CPR_RESUME) {
		if (dump_dvma)
			vmem_free(dvmamap, (void *)dump_dvma, dump_reserve);
		dump_dvma = 0;
		dump_reserve = 0;
	}
	return (B_TRUE);
}

static int
iommunex_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "iommu") == 0) {
		return (DDI_IDENTIFIED);
	}
	return (DDI_NOT_IDENTIFIED);
}

static int
iommunex_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	mutex_init(&dma_pool_lock, NULL, MUTEX_DRIVER, NULL);

	/*
	 * If this *ever* changes, we'd better fail horribly fast..
	 */
	/*CONSTANTCONDITION*/
	ASSERT(MMU_PAGESIZE == IOMMU_PAGE_SIZE);

	/*
	 * Arrange for cpr not to bother us about our hardware state,
	 * since the prom has already set it up by the time we would
	 * need to restore it
	 */
	if (ddi_prop_create(DDI_DEV_T_NONE, devi, 0, "pm-hardware-state",
	    (caddr_t)"no-suspend-resume",
	    strlen("no-suspend-resume")+1) != DDI_SUCCESS)
		cmn_err(CE_PANIC, "iommunex cannot create pm-hardware-state "
		    "property\n");

	ddi_report_dev(devi);

	(void) callb_add(iommunex_dump_done, 0, CB_CL_CPR_DMA, "iommunex");

	return (DDI_SUCCESS);
}

/*
 * DMA routines for sun4m platform
 */

#define	PTECSIZE	(64)

extern void srmmu_vacsync(uint_t);
extern int fd_page_vac_align(page_t *);

static void iommunex_vacsync(ddi_dma_impl_t *, int, uint_t, off_t, uint_t);
static void iommunex_map_pte(int, struct pte *, page_t **,
		iommu_pte_t *, ulong_t);
static void iommunex_map_pp(int, page_t *, ulong_t, iommu_pte_t *);
static void iommunex_map_window(ddi_dma_impl_t *, ulong_t, ulong_t);


static int
check_dma_attr(struct ddi_dma_req *dmareq, ddi_dma_attr_t *dma_attr,
    uint_t *size)
{
	uint_t addrlow;
	uint_t addrhigh;
	uint_t segalign;
	uint_t smask;

	smask = *size - 1;
	segalign = (uint_t)dma_attr->dma_attr_seg;
	if (smask > segalign) {
		if ((dmareq->dmar_flags & DDI_DMA_PARTIAL) == 0) {
			return (DDI_DMA_TOOBIG);
		}
		*size = segalign + 1;
	}
	addrlow = (uint_t)dma_attr->dma_attr_addr_lo;
	addrhigh = (uint_t)dma_attr->dma_attr_addr_hi;
	if (addrlow + smask > addrhigh || addrlow + smask < addrlow) {
		if (!((addrlow + dmareq->dmar_object.dmao_size == 0) &&
		    (addrhigh == (uint_t)-1))) {
			if ((dmareq->dmar_flags & DDI_DMA_PARTIAL) == 0) {
				return (DDI_DMA_TOOBIG);
			}
			*size = min(addrhigh - addrlow + 1, *size);
		}
	}
	return (DDI_DMA_MAPOK);
}

/*
 * Read pte's from the hardware mapping registers into the pte array given.
 * If asked, we also enforce some rules on non-obmem ptes. Note that
 * the mapping must be locked down.
 */
int
impl_read_hwmap(struct as *as, caddr_t addr, int npf,
    struct pte *pte, int cknonobmem)
{
	struct pte *hpte, tpte;
	int bustype = BT_DRAM;
	int chkbt = 1;
	int level = 0;
	uint_t off, span;
	struct ptbl *ptbl;
	kmutex_t *mtx;

	hpte = NULL;

	while (npf != 0) {
		if (level != 3 || ((uint_t)MMU_L2_OFF(addr) < MMU_L3_SIZE)) {
			hpte = srmmu_ptefind_nolock(as, addr, &level);
		} else {
			hpte++;
		}

		mmu_readpte(hpte, &tpte);
		if (!pte_valid(&tpte)) {
			/*
			 * Even locked ptes can turn up invalid
			 * if we call ptefind without the hat lock
			 * if another cpu is sync'ing the rm bits
			 * at the same time.  We avoid this race
			 * by retrying ptefind with the lock.
			 */
			hpte = srmmu_ptefind(as, addr, &level,
				&ptbl, &mtx, LK_PTBL_SHARED);

			mmu_readpte(hpte, &tpte);
			unlock_ptbl(ptbl, mtx);

			if (!pte_valid(&tpte)) {
				cmn_err(CE_CONT, "impl_read_hwmap no pte\n");
				bustype = -1;
				break;
			}
		}

		switch (level) {
		case 3:
			off = 0;
			span = 1;
			break;
		case 2:
			off = MMU_L2_OFF(addr);
			span = MIN(npf, mmu_btopr(MMU_L2_SIZE - off));
			break;
		case 1:
			off = MMU_L1_OFF(addr);
			span = MIN(npf, mmu_btopr(MMU_L1_SIZE - off));
			break;
		}
		off = mmu_btop(off);

		if (cknonobmem) {
			if (chkbt) {
				bustype = impl_bustype(tpte.PhysicalPageNumber);
				if (bustype == BT_UNKNOWN) {
					cmn_err(CE_CONT,
						"impl_read_hwmap BT_UNKNOWN\n");
					bustype = -1;
					break;
				}
				chkbt = 0;
			} else {
				if (impl_bustype(tpte.PhysicalPageNumber) !=
				    bustype) {
					/*
					 * we don't allow mixing bus types.
					 */
					cmn_err(CE_CONT,
					    "impl_read_hwmap: mixed bustype\n");
					bustype = -1;
					break;
				}
			}
		}

		/*
		 * We make the translation writable, even if the current
		 * mapping is read only.  This is necessary because the
		 * new pte is blindly used in other places where it needs
		 * to be writable.
		 */
		tpte.PhysicalPageNumber += off;
		tpte.AccessPermissions = MMU_STD_SRWX; /* XXX -  generous? */

		/*
		 * Copy the hw ptes to the sw array.
		 */
		npf -= span;
		addr += mmu_ptob(span);
		while (span--) {
			*pte = tpte;
			tpte.PhysicalPageNumber++;
			pte++;
		}
	}

	/*
	 * we return bustype or -1 (failure).
	 */
	return (bustype);
}

/*ARGSUSED*/
static int
iommunex_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
    ddi_dma_cookie_t *cp, uint_t *ccountp)
{
	extern struct as kas;
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	ddi_dma_attr_t *dma_attr;
	struct pte stackptes[PTECSIZE + 1];
	struct pte *allocpte;
	struct pte *ptep;
	page_t *pp;
	uint_t rflags;
	struct as *as;
	uint_t size, vcolor;
	uint_t burstsizes;
	uint_t dvma_align;
	ulong_t addr, offset;
	int npages, rval;
	int mappages, naptes = 0;
	iommu_pte_t *piopte;
	ulong_t ioaddr;
	int memtype = BT_DRAM;
	struct page **pplist = NULL;

#ifdef lint
	dip = dip;
#endif

	if (mp->dmai_inuse) {
		return (DDI_DMA_INUSE);
	}
	dma_attr = &mp->dmai_attr;
	size = dmareq->dmar_object.dmao_size;
	if (!(mp->dmai_rflags & DMP_NOLIMIT)) {
		rval = check_dma_attr(dmareq, dma_attr, &size);
		if (rval != DDI_DMA_MAPOK) {
			return (rval);
		}
	}
	mp->dmai_inuse = 1;
	mp->dmai_offset = 0;
	burstsizes = dma_attr->dma_attr_burstsizes;
	rflags = (dmareq->dmar_flags & DMP_DDIFLAGS);

	/*
	 * Validate the DMA request.
	 */
	switch (dmareq->dmar_object.dmao_type) {
	case DMA_OTYP_VADDR:
		addr = (ulong_t)dmareq->dmar_object.dmao_obj.virt_obj.v_addr;
		offset = addr & MMU_PAGEOFFSET;
		as = dmareq->dmar_object.dmao_obj.virt_obj.v_as;
		if (as == NULL)
			as = &kas;
		addr &= ~MMU_PAGEOFFSET;
		pplist = dmareq->dmar_object.dmao_obj.virt_obj.v_priv;
		npages = mmu_btopr(dmareq->dmar_object.dmao_size + offset);
		if (pplist == NULL) {
			if (npages > (PTECSIZE + 1)) {
				allocpte = kmem_alloc(
					npages * sizeof (struct pte), KM_SLEEP);
				ptep = allocpte;
				naptes = npages;
			} else {
				ptep = stackptes;
			}
			memtype = impl_read_hwmap(as, (caddr_t)addr,
					npages, ptep, 1);
		}

		switch (memtype) {
		case BT_DRAM:
		case BT_NVRAM:

			/*
			 * just up to 32 byte bursts to memory on sun4m
			 */
			if (dmareq->dmar_flags & DDI_DMA_SBUS_64BIT) {
				burstsizes &= 0x3F003F;
			} else {
				burstsizes &= 0x3F;
			}
			if (burstsizes == 0) {
				rval = DDI_DMA_NOMAPPING;
				goto bad;
			}
			break;
		case BT_SBUS:
		case BT_VIDEO:
			/* go ahead and map it as usual. */
			break;
		default:
			rval = DDI_DMA_NOMAPPING;
			goto bad;
		}
		pp = NULL;
		break;

	case DMA_OTYP_PAGES:
		pp = dmareq->dmar_object.dmao_obj.pp_obj.pp_pp;
		ASSERT(pp);
		ASSERT(page_iolock_assert(pp));
		offset = dmareq->dmar_object.dmao_obj.pp_obj.pp_offset;
		npages = mmu_btopr(dmareq->dmar_object.dmao_size + offset);
		break;

	case DMA_OTYP_PADDR:
	default:
		rval = DDI_DMA_NOMAPPING;
		goto bad;
	}

	mp->dmai_burstsizes = burstsizes;
	if (rflags & DDI_DMA_PARTIAL) {
		if (size != dmareq->dmar_object.dmao_size) {
			/*
			 * If the request is for partial mapping arrangement,
			 * the device has to be able to address at least the
			 * size of the window we are establishing.
			 */
			if (size < mmu_ptob(PTECSIZE + mmu_btopr(offset))) {
				rval = DDI_DMA_NOMAPPING;
				goto bad;
			}
			npages = mmu_btopr(size + offset);
		}

		/*
		 * If the size requested is less than a moderate amt, skip
		 * the partial mapping stuff - it's not worth the effort.
		 */
		if (npages > PTECSIZE + 1) {
			npages = PTECSIZE + mmu_btopr(offset);
			size = mmu_ptob(PTECSIZE);
			if (dmareq->dmar_object.dmao_type == DMA_OTYP_VADDR) {
				if (pplist == NULL) {
					mp->dmai_minfo = (void *)allocpte;
				} else {
					mp->dmai_minfo = (void *)pplist;
					rflags |= DMP_SHADOW;
				}
			}
		} else {
			rflags ^= DDI_DMA_PARTIAL;
		}
	} else {
		if (npages >= mmu_btop(IOMMU_DVMA_RANGE) - 0x40) {
			rval = DDI_DMA_TOOBIG;
			goto bad;
		}
	}
	mp->dmai_size = size;
	mp->dmai_ndvmapages = npages;
	mappages = npages;
	dvma_align = MAX((uint_t)dma_attr->dma_attr_align, PAGESIZE);
	vcolor = (uint_t)-1;

	/*
	 * we only want to do the aliasing dance if the
	 * platform provides I/O cache coherence.
	 */
	ioaddr = 0;
	if (vac && (cache & CACHE_IOCOHERENT)) {
		if (pp != NULL) {
			if (do_pg_coloring)
				vcolor = VCOLOR(pp);
			else {
				ohat_mlist_enter(pp);
				vcolor = fd_page_vac_align(pp);
				ohat_mlist_exit(pp);
				if (vcolor != (uint_t)-1)
					vcolor = mmu_ptob(vcolor);
			}
		} else {
			vcolor = addr & vac_mask;
		}
		/*
		 * If vcolor is a multiple of dvma_align, we can satisfy
		 * both the DVMA alignment *and* vac coloring constraints.
		 * If not, we'll just fall through to DVMA alignment only.
		 */
		if ((vcolor & (dvma_align - 1)) == 0 && dump_dvma == 0) {
			ioaddr = (ulong_t)vmem_xalloc(dvmamap, ptob(mappages),
			    MAX(dvma_align, shm_alignment), vcolor,
			    (uint_t)dma_attr->dma_attr_seg + 1,
			    (void *)dma_attr->dma_attr_addr_lo,
			    (void *)(dma_attr->dma_attr_addr_hi + 1),
			    VM_NOSLEEP);
		}
	}

	if (ioaddr == 0 && dump_dvma == 0) {
		ioaddr = (ulong_t)vmem_xalloc(dvmamap, ptob(mappages),
		    dvma_align, 0, (uint_t)dma_attr->dma_attr_seg + 1,
		    (void *)dma_attr->dma_attr_addr_lo,
		    (void *)(dma_attr->dma_attr_addr_hi + 1),
		    dmareq->dmar_fp == DDI_DMA_SLEEP ? VM_SLEEP : VM_NOSLEEP);
	}

	/*
	 * when dump_reserve is set, the value is the max write-size for
	 * a following series of VOP_DUMP(); a leading VOP_DUMP() request
	 * is called to alloc dvma space, the alloc is recorded at
	 * dump_dvma and is not freed until iommunex_dump_done().
	 *
	 * This dvma pre-allocation is reused during dump and provides
	 * a stable view of pages by avoiding ongoing allocations which
	 * alter various pages associated with a vmem_t; for cpr, these
	 * alterations lead to data corruption and panics.
	 */
	if (dump_reserve) {
		if (dump_dvma) {
			ASSERT(ioaddr == 0);
			ioaddr = dump_dvma;
		} else {
			ASSERT(ptob(mappages) == dump_reserve);
			dump_dvma = ioaddr;
		}
	}

	if (ioaddr == 0) {
		if (dmareq->dmar_fp == DDI_DMA_SLEEP)
			rval = DDI_DMA_NOMAPPING;
		else
			rval = DDI_DMA_NORESOURCES;
		goto bad;
	}
	ASSERT((caddr_t)ioaddr >= (caddr_t)IOMMU_DVMA_BASE);

	mp->dmai_mapping = (ulong_t)(ioaddr + offset);
	ASSERT((mp->dmai_mapping & ~(uint_t)dma_attr->dma_attr_seg) ==
	    ((mp->dmai_mapping + (mp->dmai_size - 1)) &
		~(uint_t)dma_attr->dma_attr_seg));

	piopte = &ioptes[iommu_btop((caddr_t)ioaddr - IOMMU_DVMA_BASE)];
	ASSERT(piopte != NULL);

	if (pp) {
		iommunex_map_pp(npages, pp, ioaddr, piopte);
	} else {
		iommunex_map_pte(npages, ptep, pplist, piopte, ioaddr);
	}

out:
	if (cp) {
		cp->dmac_notused = 0;
		cp->dmac_address = mp->dmai_mapping;
		cp->dmac_size = mp->dmai_size;
		cp->dmac_type = 0;
		*ccountp = 1;
	}
	mp->dmai_rflags = rflags | (mp->dmai_rflags & DMP_NOLIMIT);
	mp->dmai_object = dmareq->dmar_object;
	if (rflags & DDI_DMA_PARTIAL) {
		size = iommu_ptob(
			mp->dmai_ndvmapages - iommu_btopr(offset));
		mp->dmai_nwin =
		    (dmareq->dmar_object.dmao_size + (size - 1)) / size;
		return (DDI_DMA_PARTIAL_MAP);
	} else {
		mp->dmai_nwin = 0;
		if (naptes) {
			kmem_free(allocpte, naptes * sizeof (struct pte));
			mp->dmai_minfo = NULL;
		}
		return (DDI_DMA_MAPPED);
	}
bad:
	if (naptes) {
		kmem_free(allocpte, naptes * sizeof (struct pte));
	}
	if (rval == DDI_DMA_NORESOURCES &&
	    dmareq->dmar_fp != DDI_DMA_DONTWAIT) {
		ddi_set_callback(dmareq->dmar_fp,
		    dmareq->dmar_arg, &dvma_call_list_id);
	}
	return (rval);
}

static int
iommunex_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep)
{
	ddi_dma_lim_t *dma_lim = dmareq->dmar_limits;
	ddi_dma_impl_t *mp;
	ddi_dma_attr_t *dma_attr;
	ulong_t addrlow, addrhigh;
	int rval;

#ifdef lint
	dip = dip;
#endif

	/*
	 * If not an advisory call, get a DMA handle
	 */
	if (!handlep) {
		return (DDI_DMA_MAPOK);
	}
	if (dma_lim->dlim_burstsizes == 0) {
		return (DDI_DMA_NOMAPPING);
	}

	/*
	 * Check sanity for high and low address limits
	 */
	addrlow = dma_lim->dlim_addr_lo;
	addrhigh = dma_lim->dlim_addr_hi;
	if ((addrhigh <= addrlow) || (addrhigh < (ulong_t)IOMMU_DVMA_BASE)) {
		return (DDI_DMA_NOMAPPING);
	}
	mp = kmem_alloc(sizeof (*mp),
		    (dmareq->dmar_fp == DDI_DMA_SLEEP) ? KM_SLEEP : KM_NOSLEEP);
	if (mp == NULL) {
		if (dmareq->dmar_fp != DDI_DMA_DONTWAIT) {
			ddi_set_callback(dmareq->dmar_fp,
				dmareq->dmar_arg, &dvma_call_list_id);
		}
		return (DDI_DMA_NORESOURCES);
	}
	mp->dmai_rdip = rdip;
	mp->dmai_rflags = dmareq->dmar_flags & DMP_DDIFLAGS;
	mp->dmai_minxfer = dma_lim->dlim_minxfer;
	mp->dmai_burstsizes = dma_lim->dlim_burstsizes;
	mp->dmai_offset = 0;
	mp->dmai_ndvmapages = 0;
	mp->dmai_minfo = 0;
	mp->dmai_inuse = 0;
	dma_attr = &mp->dmai_attr;
	dma_attr->dma_attr_align = 1;
	dma_attr->dma_attr_addr_lo = addrlow;
	dma_attr->dma_attr_addr_hi = addrhigh;
	dma_attr->dma_attr_seg = dma_lim->dlim_cntr_max;
	dma_attr->dma_attr_burstsizes = dma_lim->dlim_burstsizes;
	rval = iommunex_dma_bindhdl(dip, rdip, (ddi_dma_handle_t)mp,
		dmareq, NULL, NULL);
	if (rval && (rval != DDI_DMA_PARTIAL_MAP)) {
		kmem_free(mp, sizeof (*mp));
	} else {
		*handlep = (ddi_dma_handle_t)mp;
	}
	return (rval);
}

int
iommunex_dma_allochdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_attr_t *dma_attr, int (*waitfp)(caddr_t), caddr_t arg,
    ddi_dma_handle_t *handlep)
{
	ulong_t addrlow, addrhigh;
	ddi_dma_impl_t *mp;

#ifdef lint
	dip = dip;
#endif

	if (dma_attr->dma_attr_burstsizes == 0) {
		return (DDI_DMA_BADATTR);
	}
	addrlow = (ulong_t)dma_attr->dma_attr_addr_lo;
	addrhigh = (ulong_t)dma_attr->dma_attr_addr_hi;
	if ((addrhigh <= addrlow) || (addrhigh < (ulong_t)IOMMU_DVMA_BASE)) {
		return (DDI_DMA_BADATTR);
	}
	if (dma_attr->dma_attr_flags & DDI_DMA_FORCE_PHYSICAL) {
		return (DDI_DMA_BADATTR);
	}

	mp = kmem_zalloc(sizeof (*mp),
		(waitfp == DDI_DMA_SLEEP) ? KM_SLEEP : KM_NOSLEEP);
	if (mp == NULL) {
		if (waitfp != DDI_DMA_DONTWAIT) {
			ddi_set_callback(waitfp, arg, &dvma_call_list_id);
		}
		return (DDI_DMA_NORESOURCES);
	}
	mp->dmai_rdip = rdip;
	mp->dmai_minxfer = (uint_t)dma_attr->dma_attr_minxfer;
	mp->dmai_burstsizes = (uint_t)dma_attr->dma_attr_burstsizes;
	mp->dmai_attr = *dma_attr;
	if ((uint_t)dma_attr->dma_attr_seg == (uint_t)-1 &&
	    addrhigh == (uint_t)-1 && addrlow == 0 &&
	    ((uint_t)dma_attr->dma_attr_align <= IOMMU_PAGE_SIZE)) {
		mp->dmai_rflags |= DMP_NOLIMIT;
	}
	*handlep = (ddi_dma_handle_t)mp;
	return (DDI_SUCCESS);
}

static int
iommunex_dma_freehdl(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;

#ifdef lint
	dip = dip;
	rdip = rdip;
#endif

	kmem_free(mp, sizeof (*mp));

	if (dvma_call_list_id != 0) {
		ddi_run_callback(&dvma_call_list_id);
	}
	return (DDI_SUCCESS);
}

static void
iommunex_map_pte(int npages, struct pte *ptep, page_t **pplist,
	iommu_pte_t *piopte, ulong_t ioaddr)
{
	uint_t pfn, iom_flag;
	page_t *pp;

	/*
	 * Note: Since srmmu and iommu pte fit per sun4m arch spec
	 *	 we could just do a
	 *		 *((uint_t *)piopte) = *ptep
	 *	 and skip that jazz but who knows what might break
	 */
	while (npages > 0) {
		/* always starts with $ DVMA */
		iom_flag = IOPTE_WRITE | IOPTE_CACHE | IOPTE_VALID;

		if (pplist) {
			pfn = ((machpage_t *)(*pplist))->p_pagenum;
			if (PP_ISNC(*pplist)) {
				iom_flag &= ~IOPTE_CACHE;
			}
			pplist++;
		} else {
			pfn = MAKE_PFNUM(ptep);
			if (!ptep->Cacheable) {
				iom_flag &= ~IOPTE_CACHE;
			}
			ptep++;
		}

		if (do_pg_coloring && vac) {
			pp = page_numtopp_nolock(pfn);
			if (pp && !ALIGNED(pp, ioaddr))
				vac_color_sync(VCOLOR(pp), pfn);
		}

		*((uint_t *)piopte) = (pfn << IOPTE_PFN_SHIFT) | iom_flag;

		if (!tsunami_control_store_bug) {
			iommu_addr_flush((int)ioaddr & IOMMU_FLUSH_MSK);
		}

		piopte++;
		npages--;
		ioaddr += IOMMU_PAGE_SIZE;
	}

	/*
	 * Tsunami and Swift need to flush entire TLB
	 */
	if (swift_tlb_flush_bug || tsunami_control_store_bug) {
		mmu_flushall();
	}

}

static void
iommunex_map_pp(int npages, page_t *pp, ulong_t ioaddr, iommu_pte_t *piopte)
{
	uint_t pfn, iom_flag;
	int align;

	while (npages > 0) {
		/* always start with $ DVMA */
		iom_flag = IOPTE_WRITE | IOPTE_CACHE | IOPTE_VALID;

		ASSERT(page_iolock_assert(pp));
		pfn = ((machpage_t *)pp)->p_pagenum;
		if (vac && (cache & CACHE_IOCOHERENT)) {
			/*
			 * For I/O cache coherent VAC machines.
			 */
			ohat_mlist_enter(pp);
			if (do_pg_coloring) {
				if (!ALIGNED(pp, ioaddr) && !PP_ISNC(pp)) {
					if (hat_page_is_mapped(pp))
						iom_flag &= ~IOPTE_CACHE;
					vac_color_sync(VCOLOR(pp), pfn);
				}
			} else if (hat_page_is_mapped(pp) && !PP_ISNC(pp)) {
				align = fd_page_vac_align(pp);
				align = mmu_ptob(align);
				if (align != (ioaddr & vac_mask)) {
					/*
					 * NOTE: this is the case
					 * where the page is marked as
					 * $able in system MMU but we
					 * cannot alias it on the
					 * IOMMU, so we have to flush
					 * out the cache and do a
					 * non-$ DVMA on this page.
					 */
					iom_flag &= ~IOPTE_CACHE;
					srmmu_vacsync(pfn);
				}
			}
			ohat_mlist_exit(pp);
		} else if (PP_ISNC(pp)) {
			iom_flag &= ~IOPTE_CACHE;
		}

		*((uint_t *)piopte) = (pfn << IOPTE_PFN_SHIFT) | iom_flag;

		/*
		 * XXX This statement needs to be before the iommu tlb
		 * flush. For yet unknown reasons there is some timing
		 * problem in flushing the iommu tlb on SS5 which will
		 * affect the srmmu tlb lookup if pp->p_next misses the
		 * D$. You might get the wrong data loaded into the D$.
		 * see 1185222 for details.
		 */
		pp = pp->p_next;

		if (!tsunami_control_store_bug) {
			iommu_addr_flush((int)ioaddr & IOMMU_FLUSH_MSK);
		}

		piopte++;
		ioaddr += IOMMU_PAGE_SIZE;
		npages--;
	}

	/*
	 * Tsunami and Swift need to flush entire TLB
	 */
	if (swift_tlb_flush_bug || tsunami_control_store_bug) {
		mmu_flushall();
	}
}

/*
 * For non-coherent caches (small4m), we always flush reads.
 */
#define	IOMMU_NC_FLUSH_READ(c, npages, mp, cflags, off, len)		\
{									\
	if (((c & (CACHE_VAC|CACHE_IOCOHERENT)) == CACHE_VAC) && npages)\
	    iommunex_vacsync(mp, npages, cflags, off, len);		\
	flush_writebuffers();						\
	if ((c & (CACHE_PAC|CACHE_IOCOHERENT)) == CACHE_PAC) {		\
		pac_flushall();						\
	}								\
}

/*
 * XXX	This ASSERT needs to be replaced by some code when machines
 *	that trip over it appear.
 */
#define	IOMMU_NC_FLUSH_WRITE(c)						\
{									\
	ASSERT((c & CACHE_IOCOHERENT) || !(c & CACHE_WRITEBACK));	\
}


static int
iommunex_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	ulong_t addr;
	int npages;

#ifdef lint
	dip = dip;
	rdip = rdip;
#endif

	addr = mp->dmai_mapping & ~IOMMU_PAGE_OFFSET;
	ASSERT(iommu_ptefind(addr) != NULL);
	npages = mp->dmai_ndvmapages;

	/*
	 * flush IOC and do a free DDI_DMA_SYNC_FORCPU.
	 */
	if (mp->dmai_rflags & DDI_DMA_READ) {
		IOMMU_NC_FLUSH_READ(cache, npages, mp,
					DDI_DMA_SYNC_FORCPU, 0, 0);
	} else {
		IOMMU_NC_FLUSH_WRITE(cache);
	}

#ifdef DEBUG
	if (npages)
		(void) iommu_unload(addr, npages);
#endif DEBUG

	if (mp->dmai_minfo) {
		if (!(mp->dmai_rflags & DMP_SHADOW)) {
			ulong_t addr;
			uint_t naptes;

			addr = (ulong_t)mp->dmai_object.
			    dmao_obj.virt_obj.v_addr;
			naptes = mmu_btopr(mp->dmai_object.dmao_size +
			    (addr & MMU_PAGEOFFSET));
			kmem_free(mp->dmai_minfo, naptes * sizeof (struct pte));
		}
		mp->dmai_minfo = NULL;
	}

	if (npages && (addr != dump_dvma))
		vmem_free(dvmamap, (void *)addr, ptob(npages));

	mp->dmai_ndvmapages = 0;
	mp->dmai_inuse = 0;

	if (dvma_call_list_id != 0) {
		ddi_run_callback(&dvma_call_list_id);
	}
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
iommunex_dma_flush(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, off_t off, size_t len,
    uint_t cache_flags)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;

	if ((cache_flags == DDI_DMA_SYNC_FORKERNEL) ||
	    (cache_flags == DDI_DMA_SYNC_FORCPU)) {
		IOMMU_NC_FLUSH_READ(cache, mp->dmai_ndvmapages,
			mp, cache_flags, off, len);
	} else {
		IOMMU_NC_FLUSH_WRITE(cache);
	}
	return (DDI_SUCCESS);
}

static int
iommunex_dma_win(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, uint_t win, off_t *offp,
    size_t *lenp, ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	ulong_t offset;
	ulong_t winsize, newoff;

#ifdef lint
	dip = dip;
	rdip = rdip;
#endif

	offset = mp->dmai_mapping & IOMMU_PAGE_OFFSET;
	winsize = iommu_ptob(mp->dmai_ndvmapages - iommu_btopr(offset));

	/*
	 * win is in the range [0 .. dmai_nwin-1]
	 */
	if (win >= mp->dmai_nwin) {
		return (DDI_FAILURE);
	}
	newoff = win * winsize;
	if (newoff > mp->dmai_object.dmao_size - mp->dmai_minxfer) {
		return (DDI_FAILURE);
	}
	ASSERT(cookiep);
	cookiep->dmac_notused = 0;
	cookiep->dmac_address = mp->dmai_mapping;
	cookiep->dmac_type = 0;
	*ccountp = 1;
	*offp = (off_t)newoff;
	*lenp = (size_t)winsize;
	if (newoff == mp->dmai_offset) {
		cookiep->dmac_size = mp->dmai_size;
		return (DDI_SUCCESS);
	}

	iommunex_map_window(mp, newoff, winsize);
	/*
	 * last window might be shorter.
	 */
	cookiep->dmac_size = mp->dmai_size;

	return (DDI_SUCCESS);
}

static void
iommunex_map_window(ddi_dma_impl_t *mp, ulong_t newoff, ulong_t winsize)
{
	ulong_t addr;
	int npages;
	struct pte *ptep;
	page_t *pp;
	ulong_t flags;
	iommu_pte_t *piopte;
	struct page **pplist = NULL;

	addr = mp->dmai_mapping & ~IOMMU_PAGE_OFFSET;
	npages = mp->dmai_ndvmapages;

	/*
	 * flush IOC and do a free DDI_DMA_SYNC_FORCPU.
	 */
	if (mp->dmai_rflags & DDI_DMA_READ) {
		IOMMU_NC_FLUSH_READ(cache, npages, mp,
					DDI_DMA_SYNC_FORCPU, 0, 0);
	} else {
		IOMMU_NC_FLUSH_WRITE(cache);
	}

#ifdef DEBUG
	if (npages)
		(void) iommu_unload(addr, npages);
#endif DEBUG

	mp->dmai_offset = newoff;
	mp->dmai_size = mp->dmai_object.dmao_size - newoff;
	mp->dmai_size = MIN(mp->dmai_size, winsize);
	npages = mmu_btopr(mp->dmai_size + (mp->dmai_mapping & MMU_PAGEOFFSET));

	piopte = iommu_ptefind(mp->dmai_mapping);
	ASSERT(piopte != NULL);

	if (mp->dmai_object.dmao_type == DMA_OTYP_VADDR) {
		if (mp->dmai_rflags & DMP_SHADOW) {
			pplist = (struct page **)mp->dmai_minfo;
			ASSERT(pplist != NULL);
			pplist = pplist + (newoff >> MMU_PAGESHIFT);
		} else {
			ptep = (struct pte *)mp->dmai_minfo;
			ASSERT(ptep != NULL);
			ptep = ptep + (newoff >> MMU_PAGESHIFT);
		}
		iommunex_map_pte(npages, ptep, pplist, piopte, addr);
	} else {
		pp = mp->dmai_object.dmao_obj.pp_obj.pp_pp;
		flags = 0;
		while (flags < newoff) {
			ASSERT(page_iolock_assert(pp));
			pp = pp->p_next;
			flags += MMU_PAGESIZE;
		}
		iommunex_map_pp(npages, pp, mp->dmai_mapping, piopte);
	}
}


static int
iommunex_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, size_t *lenp,
    caddr_t *objp, uint_t cache_flags)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	ddi_dma_cookie_t *cp;
	ulong_t addr, offset;

#ifdef lint
	dip = dip;
	cache_flags = cache_flags;
#endif

	switch (request) {
	case DDI_DMA_FREE:
	{
		(void) iommunex_dma_unbindhdl(dip, rdip, handle);
		kmem_free(mp, sizeof (*mp));

		if (dvma_call_list_id != 0) {
			ddi_run_callback(&dvma_call_list_id);
		}
		break;
	}

	case DDI_DMA_HTOC:
		/*
		 * Note that we are *not* cognizant of partial mappings
		 * at this level. We only support offsets for cookies
		 * that would then stick within the current mapping for
		 * a device.
		 */
		addr = (ulong_t)*offp;
		if (addr >= (ulong_t)mp->dmai_size) {
			return (DDI_FAILURE);
		}
		cp = (ddi_dma_cookie_t *)objp;
		cp->dmac_notused = 0;
		cp->dmac_address = (mp->dmai_mapping + addr);
		cp->dmac_size =
		    mp->dmai_mapping + mp->dmai_size - cp->dmac_address;
		cp->dmac_type = 0;
		break;

	case DDI_DMA_KVADDR:
		return (DDI_FAILURE);

	case DDI_DMA_NEXTWIN:
	{
		ddi_dma_win_t *owin, *nwin;
		ulong_t winsize, newoff;

		mp = (ddi_dma_impl_t *)handle;
		owin = (ddi_dma_win_t *)offp;
		nwin = (ddi_dma_win_t *)objp;
		if (mp->dmai_rflags & DDI_DMA_PARTIAL) {
			if (*owin == NULL) {
				mp->dmai_offset = 0;
				*nwin = (ddi_dma_win_t)mp;
				return (DDI_SUCCESS);
			}

			offset = mp->dmai_mapping & IOMMU_PAGE_OFFSET;
			winsize = iommu_ptob(mp->dmai_ndvmapages -
				iommu_btopr(offset));
			newoff = mp->dmai_offset + winsize;
			if (newoff > mp->dmai_object.dmao_size -
				mp->dmai_minxfer) {
				return (DDI_DMA_DONE);
			}
			iommunex_map_window(mp, newoff, winsize);

		} else {
			if (*owin != NULL) {
				return (DDI_DMA_DONE);
			}
			mp->dmai_offset = 0;
			*nwin = (ddi_dma_win_t)mp;
		}
		break;
	}

	case DDI_DMA_NEXTSEG:
	{
		ddi_dma_seg_t *oseg, *nseg;

		oseg = (ddi_dma_seg_t *)lenp;
		if (*oseg != NULL) {
			return (DDI_DMA_DONE);
		} else {
			nseg = (ddi_dma_seg_t *)objp;
			*nseg = *((ddi_dma_seg_t *)offp);
		}
		break;
	}

	case DDI_DMA_SEGTOC:
	{
		ddi_dma_seg_impl_t *seg;

		seg = (ddi_dma_seg_impl_t *)handle;
		cp = (ddi_dma_cookie_t *)objp;
		cp->dmac_notused = 0;
		cp->dmac_address = seg->dmai_mapping;
		cp->dmac_size = *lenp = seg->dmai_size;
		cp->dmac_type = 0;
		*offp = seg->dmai_offset;
		break;
	}

	case DDI_DMA_MOVWIN:
	{
		ulong_t winsize, newoff;

		offset = mp->dmai_mapping & IOMMU_PAGE_OFFSET;
		winsize = iommu_ptob(mp->dmai_ndvmapages - iommu_btopr(offset));

		if ((mp->dmai_rflags & DDI_DMA_PARTIAL) == 0) {
			return (DDI_FAILURE);
		}

		if (*lenp != (size_t)-1 && *lenp != winsize) {
			return (DDI_FAILURE);
		}
		newoff = (ulong_t)*offp;
		if (newoff & (winsize - 1)) {
			return (DDI_FAILURE);
		}
		if (newoff > mp->dmai_object.dmao_size - mp->dmai_minxfer) {
			return (DDI_FAILURE);
		}
		*offp = (off_t)newoff;
		*lenp = (size_t)winsize;

		iommunex_map_window(mp, newoff, winsize);

		if ((cp = (ddi_dma_cookie_t *)objp) != 0) {
			cp->dmac_notused = 0;
			cp->dmac_address = mp->dmai_mapping;
			cp->dmac_size = mp->dmai_size;
			cp->dmac_type = 0;
		}
		break;
	}

	case DDI_DMA_REPWIN:
		if ((mp->dmai_rflags & DDI_DMA_PARTIAL) == 0) {
			return (DDI_FAILURE);
		}
		*offp = (off_t)mp->dmai_offset;
		addr = mp->dmai_ndvmapages -
		    iommu_btopr(mp->dmai_mapping & IOMMU_PAGE_OFFSET);
		*lenp = (size_t)mmu_ptob(addr);
		break;

	case DDI_DMA_GETERR:
		break;

	case DDI_DMA_COFF:
		cp = (ddi_dma_cookie_t *)offp;
		addr = cp->dmac_address;
		if (addr < mp->dmai_mapping ||
		    addr >= mp->dmai_mapping + mp->dmai_size)
			return (DDI_FAILURE);
		*objp = (caddr_t)(addr - mp->dmai_mapping);
		break;

	case DDI_DMA_RESERVE:
	{
		struct ddi_dma_req *dmareqp;
		ddi_dma_lim_t *dma_lim;
		ddi_dma_handle_t *handlep;
		ulong_t addrlow, addrhigh;
		uint_t np, dvma_pfn;
		ulong_t ioaddr;

		dmareqp = (struct ddi_dma_req *)offp;
		dma_lim = dmareqp->dmar_limits;
		if (dma_lim->dlim_burstsizes == 0) {
			return (DDI_DMA_BADLIMITS);
		}
		addrlow = dma_lim->dlim_addr_lo;
		addrhigh = dma_lim->dlim_addr_hi;
		if ((addrhigh <= addrlow) ||
		    (addrhigh < (ulong_t)IOMMU_DVMA_BASE)) {
			return (DDI_DMA_BADLIMITS);
		}
		np = dmareqp->dmar_object.dmao_size;
		mutex_enter(&dma_pool_lock);
		if (np > dma_reserve) {
			mutex_exit(&dma_pool_lock);
			return (DDI_DMA_NORESOURCES);
		}
		dma_reserve -= np;
		mutex_exit(&dma_pool_lock);
		mp = kmem_zalloc(sizeof (*mp), KM_SLEEP);
		mp->dmai_rdip = rdip;
		mp->dmai_minxfer = dma_lim->dlim_minxfer;
		mp->dmai_burstsizes = dma_lim->dlim_burstsizes;
		ioaddr = (ulong_t)vmem_xalloc(dvmamap, ptob(np), PAGESIZE,
		    0, dma_lim->dlim_cntr_max + 1,
		    (void *)addrlow, (void *)(addrhigh + 1), VM_SLEEP);
		dvma_pfn = iommu_btop(ioaddr - IOMMU_DVMA_BASE);
		mp->dmai_mapping = (ulong_t)dvma_pfn;
		mp->dmai_rflags = DMP_BYPASSNEXUS;
		mp->dmai_ndvmapages = np;
		handlep = (ddi_dma_handle_t *)objp;
		*handlep = (ddi_dma_handle_t)mp;
		break;
	}
	case DDI_DMA_RELEASE:
	{
		ulong_t ioaddr, dvma_pfn;

		dvma_pfn = mp->dmai_mapping;
		ioaddr = iommu_ptob(dvma_pfn) + IOMMU_DVMA_BASE;
		vmem_free(dvmamap, (void *)ioaddr, ptob(mp->dmai_ndvmapages));
		mutex_enter(&dma_pool_lock);
		dma_reserve += mp->dmai_ndvmapages;
		mutex_exit(&dma_pool_lock);

		kmem_free(mp, sizeof (*mp));

		if (dvma_call_list_id != 0) {
			ddi_run_callback(&dvma_call_list_id);
		}
		break;
	}

	default:
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

#define	REPORTDEV_BUFSIZE	1024

/*ARGSUSED*/
static int
iommunex_report_dev(dev_info_t *dip, dev_info_t *rdip)
{
	char *buf;
	int i, n, len, f_len;
	dev_info_t *pdev;
	extern int impl_bustype(uint_t);

	pdev = (dev_info_t *)DEVI(rdip)->devi_parent;
	if ((DEVI_PD(rdip) == NULL) || (pdev == NULL))
		return (DDI_FAILURE);

	buf = kmem_alloc(REPORTDEV_BUFSIZE, KM_SLEEP);
	f_len = snprintf(buf, REPORTDEV_BUFSIZE, "%s%d at %s%d",
	    ddi_driver_name(rdip), ddi_get_instance(rdip),
	    ddi_driver_name(pdev), ddi_get_instance(pdev));
	len = strlen(buf);

	for (i = 0, n = sparc_pd_getnreg(rdip); i < n; i++) {

		struct regspec *rp = sparc_pd_getreg(rdip, i);

		if (i == 0) {
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    ": ");
		} else {
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    " and ");
		}
		len = strlen(buf);

		switch (impl_bustype(PTE_BUSTYPE_PFN(rp->regspec_bustype,
		    mmu_btop(rp->regspec_addr)))) {

		case BT_OBIO:
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    "obio 0x%x", rp->regspec_addr);
			break;

		default:
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    "space %x offset %x",
			    rp->regspec_bustype, rp->regspec_addr);
			break;
		}
		len = strlen(buf);
	}

	/*
	 * We'd report interrupts here if any of our immediate
	 * children had any.
	 */
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

static int
iommunex_ctlops(dev_info_t *dip, dev_info_t *rdip, ddi_ctl_enum_t op,
    void *a, void *r)
{
	switch (op) {

	default:
		return (ddi_ctlops(dip, rdip, op, a, r));

	case DDI_CTLOPS_REPORTDEV:
		return (iommunex_report_dev(dip, rdip));

	case DDI_CTLOPS_DVMAPAGESIZE:
		*(ulong_t *)r = IOMMU_PAGE_SIZE;
		return (DDI_SUCCESS);
	}
}

static void
iommunex_vacsync(ddi_dma_impl_t *mp, int npages,
	uint_t cache_flags, off_t offset, uint_t length)
{
	page_t *pp;
	uint_t pfn;
	uint_t addr, endmap;

	switch (mp->dmai_object.dmao_type) {
	case DMA_OTYP_VADDR:

		addr = mp->dmai_mapping + offset;
		endmap = mp->dmai_mapping + mp->dmai_size;

		ASSERT((addr >= mp->dmai_mapping) && (addr <= endmap));

		if ((length == 0) || (length == (uint_t)-1) ||
		    ((addr + length) >= endmap))
			length = endmap - addr;

		/*
		 * If the object vaddr is below KERNELBASE then we need to
		 * flush in the correct context. Also, if the type of flush
		 * is not FORKERNEL then there may be more than one mapping
		 * for this object and we must flush them all.
		 */
		if ((mp->dmai_object.dmao_obj.virt_obj.v_addr <
			(caddr_t)KERNELBASE) ||
			(cache_flags != DDI_DMA_SYNC_FORKERNEL)) {

			npages = mmu_btopr(length + (addr & MMU_PAGEOFFSET));
			addr &= ~MMU_PAGEOFFSET;

			while (npages-- > 0) {
				ASSERT(iommu_ptefind(addr) != NULL);
				pfn = IOMMU_MK_PFN(iommu_ptefind(addr));
				pp = page_numtopp_nolock(pfn);
				if (pp) {
					ohat_mlist_enter(pp);
					if (hat_page_is_mapped(pp) &&
					    !PP_ISNC(pp)) {
						srmmu_vacsync(pfn);
					}
					ohat_mlist_exit(pp);
				}
				addr += MMU_PAGESIZE;
			}
		} else {
			vac_flush(mp->dmai_object.dmao_obj.virt_obj.v_addr
					+ offset, length);
		}
		break;

	case DMA_OTYP_PAGES:
		pp = mp->dmai_object.dmao_obj.pp_obj.pp_pp;
		while (npages-- > 0) {
			pfn = ((machpage_t *)pp)->p_pagenum;
			ASSERT(pp != (page_t *)NULL);
			ASSERT(page_iolock_assert(pp));
			ohat_mlist_enter(pp);
			if (hat_page_is_mapped(pp) && !PP_ISNC(pp)) {
				srmmu_vacsync(pfn);
			}
			ohat_mlist_exit(pp);
			pp = pp->p_next;
		}
		break;

	case DMA_OTYP_PADDR:
		/* not support by IOMMU nexus */
	default:
		break;
	}
}
