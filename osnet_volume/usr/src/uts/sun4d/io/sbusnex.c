/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sbusnex.c	1.101	99/08/28 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/avintr.h>
#include <sys/spl.h>

#include <vm/seg.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <vm/hat.h>
#include <vm/page.h>
#include <vm/mach_page.h>

#include <sys/machsystm.h>
#include <sys/iocache.h>
#include <sys/physaddr.h>
#include <sys/iommu.h>
#include <sys/bt.h>

/*
 * Configuration information
 */

static int sbus_to_sparc(int), sparc_to_sbus(int);

static int
sbus_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);

static int
sbus_dma_map(dev_info_t *, dev_info_t *, struct ddi_dma_req *,
	ddi_dma_handle_t *);

static int
sbus_dma_allochdl(dev_info_t *, dev_info_t *, ddi_dma_attr_t *,
	int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *);

static int
sbus_dma_freehdl(dev_info_t *, dev_info_t *, ddi_dma_handle_t);

static int
sbus_dma_bindhdl(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
	struct ddi_dma_req *, ddi_dma_cookie_t *, uint_t *);

static int
sbus_dma_unbindhdl(dev_info_t *, dev_info_t *, ddi_dma_handle_t);

static int
sbus_dma_flush(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
	off_t, size_t, uint_t);

static int
sbus_dma_win(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
	uint_t, off_t *, size_t *, ddi_dma_cookie_t *, uint_t *);

static int
sbus_dma_mctl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle,
	enum ddi_dma_ctlops request, off_t *offp, size_t *lenp,
	caddr_t *objp, uint_t flags);

static int
sbus_add_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	uint_t (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg, int kind);

static void
sbus_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookiep);

static struct bus_ops sbus_bus_ops = {
	BUSO_REV,
	i_ddi_bus_map,
	i_ddi_get_intrspec,
	sbus_add_intrspec,
	sbus_remove_intrspec,
	i_ddi_map_fault,
	sbus_dma_map,
	sbus_dma_allochdl,
	sbus_dma_freehdl,
	sbus_dma_bindhdl,
	sbus_dma_unbindhdl,
	sbus_dma_flush,
	sbus_dma_win,
	sbus_dma_mctl,
	sbus_ctlops,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0	/* (*bus_post_event)();		*/
};

static int sbus_identify(dev_info_t *);
static int sbus_attach(dev_info_t *, ddi_attach_cmd_t cmd);

static struct dev_ops sbus_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	sbus_identify,		/* identify */
	0,			/* probe */
	sbus_attach,		/* attach */
	nodev,			/* detach */
	nodev,			/* reset */
	(struct cb_ops *)0,	/* driver operations */
	&sbus_bus_ops		/* bus operations */
};

static int fd_sbus_slot_id(dev_info_t *dip, dev_info_t *rdip);
static int fd_target_slot_id(struct pte *ptep, int cur_sbus_id);
static uint_t recover_iom_flag(iommu_pte_t *piopte);
#ifdef DEV_TO_DEV_DMA
static int fd_target_sbus_id(struct dma_phys_mapc *pd);
#endif DEV_TO_DEV_DMA

static void sbus_map_pte(int, struct pte *, page_t **,
    uint_t, iommu_pte_t *);
static void sbus_map_pp(int, page_t *, uint_t, iommu_pte_t *);
static void sbus_map_window(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_impl_t *, ulong_t, ulong_t);

extern int get_deviceid(int nodeid, int parent);
extern int nexus_note_sbus(uint_t unit, struct autovec *handler);
extern int xdb_sbus_unit(int device_id);

extern struct sbus_private sbus_pri_data[];
extern int nsbus;
extern int sbus_nvect;		/* # intr's supported per level same slot */

#define	get_autovec_base(dip) \
	(((struct sbus_private *)(DEVI((dip))->devi_driver_data))->vec)

#include <sys/modctl.h>
extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops, 	/* Type of module.  This one is a driver */
	"SBus nexus driver 1.101",	/* Name of module. */
	&sbus_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

/*
 * These are the module initialization routines.
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

static int sbus_burst_sizes;
static int sbus_max_burst_size, sbus_min_burst_size;
static uint_t sbus_dma_reserve = SBUSMAP_MAXRESERVE;

static int
sbus_identify(dev_info_t *devi)
{
	char *name = ddi_get_name(devi);
	int rc = DDI_NOT_IDENTIFIED;

	if (strcmp(name, "sbi") == 0) {
		rc = DDI_IDENTIFIED;
	}

	return (rc);
}

/*ARGSUSED*/
static int
sbus_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	static int first_sbus = 1; /* current unit number for attach */

	struct sbus_private *pprivate;
	int parent_node_id;
	int node_id;
	int device_id;
	int sbus_index, i;
	volatile uint_t *sbi_ctl;	/* SBI CTL register pointer */
	char name[40];

	parent_node_id = ddi_get_nodeid(ddi_get_parent(devi));
	node_id = ddi_get_nodeid(devi);
	device_id = get_deviceid(node_id, parent_node_id);
	sbus_index = xdb_sbus_unit(device_id);

	if (first_sbus) {
		/*
		 * We also *know* when memory is the DVMA slave,
		 * it takes all burstsizes. And we *know*
		 * SBI supports all burstsizes as a controller.
		 * So sbus_burst_sizes (which describes burst
		 * capability of system memory as DVMA slave)
		 * is 64 byte burst which is the max possible
		 * on currrent Sbus.
		 */
		sbus_burst_sizes = SBUS_BURST_64B;
		sbus_max_burst_size = 1 << (ddi_fls(sbus_burst_sizes) - 1);
		sbus_min_burst_size = 1 << (ddi_ffs(sbus_burst_sizes) - 1);

		first_sbus = 0;
	}

	pprivate = &sbus_pri_data[sbus_index];

	/* save private data ptr */
	ddi_set_driver_private(devi, (caddr_t)pprivate);

	pprivate->sbi_id = sbus_index;
	mutex_init(&pprivate->dma_pool_lock, NULL, MUTEX_DEFAULT, NULL);

	/*
	 * Map in SBI registers. Note, rnumber 1 is for sbi registers,
	 * rnumber 2 is for IOPTEs. Sbi reg has 1 page, but IOPTEs has
	 * 16 pages (64K).
	 */

	if (ddi_map_regs(devi, 1, (caddr_t *)&pprivate->va_sbi, 0, 0)) {
		cmn_err(CE_CONT, "?sbus%d: unable to map SBI registers\n",
				sbus_index);
		return (-1);
	}

	/*
	 * Modify the SBI control register so that the SDTOL field
	 * contains the default SDTOL, 0xD. This gives a much longer
	 * timeout than that programmed in by the PROM (0x1).
	 */
	sbi_ctl = (uint_t *)VA_SBI_CNTL(pprivate->va_sbi);
	*sbi_ctl = sbi_set_sdtol(*sbi_ctl, DFLT_SDTOL);

	/* map in XPT */
	if (ddi_map_regs(devi, 2, (caddr_t *)&pprivate->va_xpt, 0, 0)) {
		cmn_err(CE_CONT, "?sbus%d: unable to map XPT registers\n",
				sbus_index);
		return (DDI_FAILURE);
	}

	(void) snprintf(name, sizeof (name), "%s%d_dvma",
	    ddi_driver_name(devi), ddi_get_instance(devi));
	pprivate->map = vmem_create(name, (void *)IOMMU_DVMA_BASE,
	    IOMMU_DVMA_SIZE, PAGESIZE, NULL, NULL, NULL, 0, VM_NOSLEEP);
	if (pprivate->map == NULL)
		return (DDI_FAILURE);

	pprivate->dma_reserve = sbus_dma_reserve;

	/* allocate the interrupt autovector array */
	pprivate->vec = (struct autovec *)
		kmem_zalloc(sizeof (struct autovec) *
		    MX_PRI * MX_SBUS_SLOTS * sbus_nvect, KM_NOSLEEP);

	if (pprivate->vec == NULL) {
		cmn_err(CE_PANIC, "sbus_attach: kmem_zalloc of vec failed\n");
	}

	/* let intr.c knows about our interrupt list ... */
	(void) nexus_note_sbus(sbus_index, pprivate->vec);

	/* now clean up the hardware */
	iommu_init(pprivate->va_xpt);
	stream_dvma_init(pprivate->va_sbi);


	/*
	 * init. burst mode in each slot.
	 */
	for (i = 0; i < MX_SBUS_SLOTS; i++) {
		extern uint_t slot_burst_patch[MX_SBUS][MX_SBUS_SLOTS];

		if (slot_burst_patch[sbus_index][i] != 0) {
			set_sbus_burst_size(pprivate->va_sbi, i,
			    slot_burst_patch[sbus_index][i]);
			pprivate->burst_size[i] =
			    slot_burst_patch[sbus_index][i];
		} else {
			set_sbus_burst_size(pprivate->va_sbi, i, NO_SBUS_BURST);
			pprivate->burst_size[i] = NO_SBUS_BURST;
		}
	}

	nsbus++;

	ddi_report_dev(devi);
	return (DDI_SUCCESS);
}


/*
 * DMA routines for sun4d platform
 */

#define	PTECSIZE	(64)	/* 256K DMA window size */

/*
 * Macros to flush and invalidate streaming buffers
 */
#define	SBI_FWB		0x1
#define	FLUSH_SBUS_WRTBUF(va_sbi)					\
{									\
	volatile uint_t *ptmp;						\
									\
	ptmp = (uint_t *)va_sbi;					\
	*ptmp = SBI_FWB;						\
	while (*ptmp & SBI_FWB);					\
}

#define	SBI_IRB		0x2
#define	INVALIDATE_SBUS_RDBUF(va_sbi)					\
{									\
	volatile uint_t *ptmp;						\
									\
	ptmp = (uint_t *)va_sbi;					\
	*ptmp = SBI_IRB;						\
	while (*ptmp & SBI_IRB);					\
}

static int
sbus_dma_allochdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_attr_t *dma_attr,
	int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *handlep)
{
	ulong_t addrlow, addrhigh;
	ddi_dma_impl_t *mp;
	int slot_id;
	struct sbus_private *ppri =
		(struct sbus_private *)ddi_get_driver_private(dip);

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
			ddi_set_callback(waitfp, arg, &ppri->dvma_call_list_id);
		}
		return (DDI_DMA_NORESOURCES);
	}
	mp->dmai_rdip = rdip;
	mp->dmai_minxfer = (uint_t)dma_attr->dma_attr_minxfer;
	mp->dmai_burstsizes = (uint_t)dma_attr->dma_attr_burstsizes;
	mp->dmai_attr = *dma_attr;
	mp->dmai_nwin = 1;
	if ((slot_id = fd_sbus_slot_id(dip, rdip)) < 0) {
		cmn_err(CE_PANIC, "sbus_dma_allochdl: bad slot_id");
	}
	mp->dmai_sbi = get_slot_ctlreg(ppri->va_sbi, slot_id);
	if ((uint_t)dma_attr->dma_attr_seg == (uint_t)-1 &&
	    addrhigh == (uint_t)-1 && addrlow == 0 &&
	    ((uint_t)dma_attr->dma_attr_align <= IOMMU_PAGE_SIZE)) {
		mp->dmai_rflags |= DMP_NOLIMIT;
	}
	*handlep = (ddi_dma_handle_t)mp;
	return (DDI_SUCCESS);
}

static int
sbus_dma_freehdl(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	struct sbus_private *ppri =
		(struct sbus_private *)ddi_get_driver_private(dip);

#ifdef lint
	rdip = rdip;
#endif
	kmem_free(mp, sizeof (*mp));

	if (ppri->dvma_call_list_id != 0) {
		ddi_run_callback(&ppri->dvma_call_list_id);
	}
	return (DDI_SUCCESS);
}

static uint_t
recover_iom_flag(iommu_pte_t *piopte)
{
	iommu_pte_t tmp_pte;

	tmp_pte = *piopte;	/* read in pte. */

	return (tmp_pte.iopte & IOPTE_FLAG_MSK);
}

static int
sbus_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	ulong_t addr;
	uint_t npages;
	struct sbus_private *ppri =
		(struct sbus_private *)ddi_get_driver_private(dip);
#ifdef lint
	rdip = rdip;
#endif
	addr = mp->dmai_mapping & ~MMU_PAGEOFFSET;
	npages = mp->dmai_ndvmapages;

	if ((mp->dmai_rflags & DDI_DMA_READ) &&
	    !(mp->dmai_rflags & DDI_DMA_CONSISTENT)) {
		/* flush stream write buffers */
		FLUSH_SBUS_WRTBUF(mp->dmai_sbi);
	}

#ifdef DEBUG
	if (npages)
		(void) iommu_unload(ppri->va_xpt, (caddr_t)addr, npages);
#endif DEBUG

	if (mp->dmai_minfo) {
		if (!(mp->dmai_rflags & DMP_SHADOW)) {
			ulong_t addr;
			uint_t naptes;

			addr = (ulong_t)mp->dmai_object.
			    dmao_obj.virt_obj.v_addr;
			naptes = mmu_btopr(mp->dmai_object.dmao_size +
			    (addr & MMU_PAGEOFFSET));
			kmem_free(mp->dmai_minfo,
			    naptes * sizeof (struct pte));
		}
		mp->dmai_minfo = NULL;
	}

	if (npages)
		vmem_free(ppri->map, (void *)addr, ptob(npages));

	mp->dmai_ndvmapages = 0;
	mp->dmai_inuse = 0;

	if (ppri->dvma_call_list_id != 0) {
		ddi_run_callback(&ppri->dvma_call_list_id);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
sbus_dma_flush(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, off_t off, size_t len,
    uint_t cache_flags)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;

	if (cache_flags == DDI_DMA_SYNC_FORDEV) {
		INVALIDATE_SBUS_RDBUF(mp->dmai_sbi);
	} else {
		FLUSH_SBUS_WRTBUF(mp->dmai_sbi);
	}
	return (DDI_SUCCESS);
}

static int
sbus_dma_win(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, uint_t win, off_t *offp,
    size_t *lenp, ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	ulong_t offset;
	ulong_t winsize, newoff;

#ifdef lint
	rdip = rdip;
#endif
	offset = mp->dmai_mapping & MMU_PAGEOFFSET;
	winsize = mmu_ptob(mp->dmai_ndvmapages - mmu_btopr(offset));

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

	sbus_map_window(dip, rdip, mp, newoff, winsize);
	/*
	 * last window might be shorter.
	 */
	cookiep->dmac_size = mp->dmai_size;

	return (DDI_SUCCESS);
}

static void
sbus_map_window(dev_info_t *dip, dev_info_t *rdip, ddi_dma_impl_t *mp,
    ulong_t newoff, ulong_t winsize)
{
	struct pte *ptep;
	page_t *pp;
	ulong_t addr, flags;
	iommu_pte_t *piopte;
	uint_t npages, iom_flag;
	struct sbus_private *ppri =
		(struct sbus_private *)ddi_get_driver_private(dip);
	struct page **pplist = NULL;

#ifdef lint
	rdip = rdip;
#endif
	addr = mp->dmai_mapping & ~MMU_PAGEOFFSET;
	npages = mp->dmai_ndvmapages;

	piopte = iommu_ptefind(ppri->va_xpt, (caddr_t)addr);
	ASSERT(piopte);

	iom_flag = recover_iom_flag(piopte);
	iom_flag |= IOPTE_VALID;

	if ((mp->dmai_rflags & DDI_DMA_READ) &&
	    !(mp->dmai_rflags & DDI_DMA_CONSISTENT)) {
		/* flush stream buffers */
		FLUSH_SBUS_WRTBUF(mp->dmai_sbi);
	}

#ifdef DEBUG
	if (npages)
		(void) iommu_unload(ppri->va_xpt, (caddr_t)addr, npages);
#endif DEBUG

	mp->dmai_offset = newoff;
	mp->dmai_size = mp->dmai_object.dmao_size - newoff;
	mp->dmai_size = MIN(mp->dmai_size, winsize);
	npages = mmu_btopr(mp->dmai_size + (mp->dmai_mapping & MMU_PAGEOFFSET));

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
		sbus_map_pte(npages, ptep, pplist, iom_flag, piopte);
	} else {
		pp = mp->dmai_object.dmao_obj.pp_obj.pp_pp;
		flags = 0;
		while (flags < newoff) {
			ASSERT(page_iolock_assert(pp));
			pp = pp->p_next;
			flags += MMU_PAGESIZE;
		}
		sbus_map_pp(npages, pp, iom_flag, piopte);
	}

	/*
	 * also invalidate read stream buffer
	 */
	if ((mp->dmai_rflags & DDI_DMA_WRITE) && (iom_flag & IOPTE_STREAM)) {
		INVALIDATE_SBUS_RDBUF(mp->dmai_sbi);
	}
}

static int
sbus_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep)
{
	ddi_dma_lim_t *dma_lim = dmareq->dmar_limits;
	ddi_dma_impl_t *mp;
	ddi_dma_attr_t *dma_attr;
	ulong_t addrlow, addrhigh;
	int rval, slot_id;
	struct sbus_private *ppri =
		(struct sbus_private *)ddi_get_driver_private(dip);

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

	/*
	 * If not an advisory call, get a DMA handle
	 */
	if (!handlep) {
		return (DDI_DMA_MAPOK);
	}
	mp = kmem_alloc(sizeof (*mp),
		    (dmareq->dmar_fp == DDI_DMA_SLEEP) ? KM_SLEEP : KM_NOSLEEP);
	if (mp == NULL) {
		if (dmareq->dmar_fp != DDI_DMA_DONTWAIT) {
			ddi_set_callback(dmareq->dmar_fp,
			    dmareq->dmar_arg, &ppri->dvma_call_list_id);
		}
		return (DDI_DMA_NORESOURCES);
	}
	mp->dmai_rdip = rdip;
	mp->dmai_rflags = dmareq->dmar_flags & DMP_DDIFLAGS;
	mp->dmai_minxfer = dma_lim->dlim_minxfer;
	mp->dmai_offset = 0;
	mp->dmai_ndvmapages = 0;
	mp->dmai_minfo = 0;
	mp->dmai_inuse = 0;
	if ((slot_id = fd_sbus_slot_id(dip, rdip)) < 0) {
		cmn_err(CE_PANIC, "sbus_dma_map: bad slot_id");
	}
	mp->dmai_sbi = get_slot_ctlreg(ppri->va_sbi, slot_id);
	dma_attr = &mp->dmai_attr;
	dma_attr->dma_attr_align = 1;
	dma_attr->dma_attr_addr_lo = addrlow;
	dma_attr->dma_attr_addr_hi = addrhigh;
	dma_attr->dma_attr_seg = dma_lim->dlim_cntr_max;
	dma_attr->dma_attr_burstsizes = dma_lim->dlim_burstsizes;
	rval = sbus_dma_bindhdl(dip, rdip, (ddi_dma_handle_t)mp,
		dmareq, NULL, NULL);
	if (rval && (rval != DDI_DMA_PARTIAL_MAP)) {
		kmem_free(mp, sizeof (*mp));
	} else {
		*handlep = (ddi_dma_handle_t)mp;
	}
	return (rval);
}

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
 * Return bustype or -1 if invalid.
 */
int
impl_read_hwmap(struct as *as, caddr_t addr, int npf, struct pte *pte,
    int cknonobmem)
{
	struct pte *hpte, tpte;
	int bustype = BT_DRAM;
	int chkbt = 1;
	int level = 0;
	int off, span;
	struct ptbl *ptbl;
	kmutex_t *mtx;

	hpte = NULL;

	while (npf > 0) {

		/*
		 * If it's a L3 pte and it's not at L3 page table boundary,
		 * then we know the pte is still in the same page table.
		 * a hpte++ would be faster than a srmmu_ptefind. Unfortunately,
		 * for L2, L1 ptes, we cannot use the same trick since they
		 * can have a smaller (eg. L3) pte right after it.
		 */
		if (level != 3 || ((uint_t)MMU_L2_OFF(addr) < MMU_L3_SIZE)) {
			hpte = srmmu_ptefind_nolock(as, addr, &level);
		} else {
			hpte++;
		}

		ASSERT(hpte != NULL);
		tpte = *hpte;

		if (!pte_valid(&tpte)) {
			/*
			 * Even locked ptes can turn up invalid
			 * if we call ptefind without the hat lock
			 * if another cpu is sync'ing the rm bits
			 * at the same time.  We avoid this race
			 * by retrying ptefind with the lock.
			 */
			hpte = srmmu_ptefind(as, addr, &level, &ptbl, &mtx,
				LK_PTBL_SHARED);
			tpte = *hpte;
			unlock_ptbl(ptbl, mtx);

			if (!pte_valid(&tpte)) {
				cmn_err(CE_CONT,
					"impl_read_hwmap invalid pte\n");
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
		tpte.PhysicalPageNumber += off;

		if (cknonobmem) {
			if (chkbt) {
				bustype = impl_bustype(MAKE_PFNUM(&tpte));
				/*
				 * DVMA can happen to memory or Sbus.
				 */
				if (bustype != BT_DRAM &&
				    bustype != BT_NVRAM &&
				    bustype != BT_SBUS) {
					cmn_err(CE_CONT,
					    "impl_read_hwmap:bustype err\n");
					bustype = -1;
					break;
				}
				chkbt = 0;
			} else {
				if (impl_bustype(MAKE_PFNUM(&tpte))
				    != bustype) {
					/*
					 * we don't allow mixing bus types.
					 */
					cmn_err(CE_CONT,
					    "impl_read_hwmap:mixed bustype\n");
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
		tpte.AccessPermissions = MMU_STD_SRWX;	/* XXX -  generous? */

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

	return (bustype);
}

static int
sbus_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
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
	uint_t dvma_pfn, size;
	uint_t burstsizes;
	ulong_t addr, ioaddr, offset;
	int npages, rval;
	int memtype = BT_DRAM;
	int naptes = 0;
	iommu_pte_t *piopte;
	uint_t iom_flag = IOPTE_VALID|IOPTE_WRITE;
	struct sbus_private *ppri =
		(struct sbus_private *)ddi_get_driver_private(dip);
	int target_burst_sizes;
	struct page **pplist = NULL;

#ifdef lint
	rdip = rdip;
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
			target_burst_sizes = sbus_burst_sizes;
			break;

		case BT_SBUS:
			if (fd_target_slot_id(ptep, ppri->sbi_id) < 0) {
				rval = DDI_DMA_NOMAPPING;
				goto bad;
			}
			target_burst_sizes = sbus_burst_sizes;
			iom_flag |= IOPTE_INTRA;
			break;

		default:
			rval = DDI_DMA_NOMAPPING;
			goto bad;
		}
		pp = NULL;
		break;

	case DMA_OTYP_PAGES:
		pp = dmareq->dmar_object.dmao_obj.pp_obj.pp_pp;
		offset = dmareq->dmar_object.dmao_obj.pp_obj.pp_offset;
		npages = mmu_btopr(dmareq->dmar_object.dmao_size + offset);

		target_burst_sizes = sbus_burst_sizes;
		break;

	case DMA_OTYP_PADDR:
	default:
		/*
		 * Not a supported type for this implementation
		 */
		rval = DDI_DMA_NOMAPPING;
		goto bad;
	}

	burstsizes = (dma_attr->dma_attr_burstsizes &= target_burst_sizes);
	if (burstsizes == 0) {
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
		if (npages >= SBUSMAP_SIZE - 0x40) {
			rval = DDI_DMA_TOOBIG;
			goto bad;
		}
	}
	mp->dmai_size = size;
	mp->dmai_ndvmapages = npages;
	if (mp->dmai_rflags & DMP_NOLIMIT) {
		ioaddr = (ulong_t)vmem_alloc(ppri->map, ptob(npages),
		    dmareq->dmar_fp == DDI_DMA_SLEEP ? VM_SLEEP : VM_NOSLEEP);
	} else {
		ioaddr = (ulong_t)vmem_xalloc(ppri->map, ptob(npages),
		    MAX(dma_attr->dma_attr_align, PAGESIZE), 0,
		    dma_attr->dma_attr_seg + 1,
		    (void *)dma_attr->dma_attr_addr_lo,
		    (void *)(dma_attr->dma_attr_addr_hi + 1),
		    dmareq->dmar_fp == DDI_DMA_SLEEP ? VM_SLEEP : VM_NOSLEEP);
	}
	if (ioaddr == 0) {
		if (dmareq->dmar_fp == DDI_DMA_SLEEP)
			rval = DDI_DMA_NOMAPPING;
		else
			rval = DDI_DMA_NORESOURCES;
		goto bad;
	}
	ASSERT((caddr_t)ioaddr >= (caddr_t)IOMMU_DVMA_BASE);
	dvma_pfn = iommu_btop((caddr_t)ioaddr - (caddr_t)IOMMU_DVMA_BASE);
	piopte = &ppri->va_xpt[dvma_pfn];
	ASSERT(piopte != NULL);

	if (!(iom_flag & IOPTE_INTRA)) {
		/* DVMA to memory */
		iom_flag |= IOPTE_CACHE;

		if (rflags & DDI_DMA_CONSISTENT) {
			rflags |= DMP_NOSYNC;
		} else {
			/* stream mode */
			iom_flag |= IOPTE_STREAM;
		}
	}

	mp->dmai_mapping = (ulong_t)(ioaddr + offset);
	ASSERT((mp->dmai_mapping & ~(uint_t)dma_attr->dma_attr_seg) ==
	    ((mp->dmai_mapping + (mp->dmai_size - 1)) &
		~(uint_t)dma_attr->dma_attr_seg));

	if (pp) {
		sbus_map_pp(npages, pp, iom_flag, piopte);
	} else {
		sbus_map_pte(npages, ptep, pplist, iom_flag, piopte);
	}

	/*
	 * invalidate stream read buffer
	 */
	if ((rflags & DDI_DMA_WRITE) && (iom_flag & IOPTE_STREAM)) {
		INVALIDATE_SBUS_RDBUF(mp->dmai_sbi);
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
		size = iommu_ptob(mp->dmai_ndvmapages - iommu_btopr(offset));
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
		    dmareq->dmar_arg, &ppri->dvma_call_list_id);
	}
	mp->dmai_inuse = 0;
	return (rval);
}

static void
sbus_map_pp(int npages, page_t *pp, uint_t iom_flag,
    iommu_pte_t *piopte)
{
	uint_t pfn;

	while (npages > 0) {
		pfn = ((machpage_t *)pp)->p_pagenum;

		*(uint_t *)piopte = (pfn << IOPTE_PFN_SHIFT) | iom_flag;

		npages--;
		piopte++;
		pp = pp->p_next;
	}
}

static void
sbus_map_pte(int npages, struct pte *ptep, page_t **pplist,
    uint_t iom_flag, iommu_pte_t *piopte)
{
	uint_t pfn;

	while (npages > 0) {
		if (pplist) {
			pfn = ((machpage_t *)(*pplist))->p_pagenum;
			pplist++;
		} else {
			pfn = MAKE_PFNUM(ptep);
			ptep++;
		}

		*(uint_t *)piopte = (pfn << IOPTE_PFN_SHIFT) | iom_flag;

		npages--;
		piopte++;
	}
}

static int
sbus_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, size_t *lenp,
    caddr_t *objp, uint_t cache_flags)
{
	ulong_t addr, offset;
	ddi_dma_cookie_t *cp;
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;
	struct sbus_private *ppri =
		(struct sbus_private *)ddi_get_driver_private(dip);

#ifdef lint
	cache_flags = cache_flags;
#endif

	switch (request) {
	case DDI_DMA_FREE:
	{
		(void) sbus_dma_unbindhdl(dip, rdip, handle);
		kmem_free(mp, sizeof (*mp));

		if (ppri->dvma_call_list_id != 0) {
			ddi_run_callback(&ppri->dvma_call_list_id);
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

		if (mp->dmai_rflags & DMP_PHYSADDR) {
			return (DDI_FAILURE);
		}

		/*
		 * Unfortunately on an IOMMU machine, the dvma cookie
		 * is not valid on the SRMMU at all. So we simply
		 * returns failure here.
		 *
		 * XXX: maybe we want to do a map in instead? but this
		 *	would create a leak since there is no
		 *	corresponding map out call in DDI. So just
		 *	return a error status and let driver
		 *	do map in/map out.
		 */
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

			offset = mp->dmai_mapping & MMU_PAGEOFFSET;
			winsize = mmu_ptob(mp->dmai_ndvmapages -
				mmu_btopr(offset));
			newoff = mp->dmai_offset + winsize;
			if (newoff > mp->dmai_object.dmao_size -
				mp->dmai_minxfer) {
				return (DDI_DMA_DONE);
			}
			sbus_map_window(dip, rdip, mp, newoff, winsize);

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

		offset = mp->dmai_mapping & MMU_PAGEOFFSET;
		winsize = mmu_ptob(mp->dmai_ndvmapages - mmu_btopr(offset));

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

		sbus_map_window(dip, rdip, mp, newoff, winsize);

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
		    mmu_btopr(mp->dmai_mapping & MMU_PAGEOFFSET);
		*lenp = (size_t)ptob(addr);
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
		int slot_id;

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
		mutex_enter(&ppri->dma_pool_lock);
		if (np > ppri->dma_reserve) {
			mutex_exit(&ppri->dma_pool_lock);
			return (DDI_DMA_NORESOURCES);
		}
		ppri->dma_reserve -= np;
		mutex_exit(&ppri->dma_pool_lock);
		mp = kmem_zalloc(sizeof (*mp), KM_SLEEP);
		mp->dmai_rdip = rdip;
		mp->dmai_minxfer = dma_lim->dlim_minxfer;
		mp->dmai_burstsizes = dma_lim->dlim_burstsizes;
		mp->dmai_minfo = (void *)ppri;
		mp->dmai_rflags = DMP_BYPASSNEXUS;
		if ((slot_id = fd_sbus_slot_id(dip, rdip)) < 0)
			cmn_err(CE_PANIC, "sbus_dma_mctl: bad slot_id");
		mp->dmai_sbi = get_slot_ctlreg(ppri->va_sbi, slot_id);

		ioaddr = (ulong_t)vmem_xalloc(ppri->map, ptob(np),
		    PAGESIZE, 0, dma_lim->dlim_cntr_max + 1,
		    (void *)addrlow, (void *)(addrhigh + 1), VM_SLEEP);
		dvma_pfn = iommu_btop(ioaddr - IOMMU_DVMA_BASE);
		mp->dmai_ndvmapages = np;
		mp->dmai_mapping = (ulong_t)dvma_pfn;
		handlep = (ddi_dma_handle_t *)objp;
		*handlep = (ddi_dma_handle_t)mp;
		break;
	}
	case DDI_DMA_RELEASE:
	{
		ulong_t ioaddr, dvma_pfn;

		dvma_pfn = mp->dmai_mapping;
		ioaddr = iommu_ptob(dvma_pfn) + IOMMU_DVMA_BASE;
		vmem_free(ppri->map, (void *)ioaddr, ptob(mp->dmai_ndvmapages));

		mutex_enter(&ppri->dma_pool_lock);
		ppri->dma_reserve += mp->dmai_ndvmapages;
		mutex_exit(&ppri->dma_pool_lock);
		kmem_free(mp, sizeof (*mp));

		if (ppri->dvma_call_list_id != 0) {
			ddi_run_callback(&ppri->dvma_call_list_id);
		}
		break;
	}

	default:
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

/* #define SBUS_DEBUG */

#ifdef  SBUS_DEBUG
int sbus_debug_flag;
#define	sbus_debug	if (sbus_debug_flag) printf
#endif  SBUS_DEBUG

static int
sbus_ctlops(dev_info_t *dip, dev_info_t *rdip,
    ddi_ctl_enum_t op, void *arg, void *result)
{
	switch (op) {
	case DDI_CTLOPS_INITCHILD: {
		struct sbus_private *ppri = (struct sbus_private *)
			ddi_get_driver_private(dip);
		int slot_id, burst_sizes, retval;

		extern int impl_ddi_sbus_initchild(dev_info_t *);
		extern uint_t slot_burst_patch[MX_SBUS][MX_SBUS_SLOTS];

		if (strcmp(ddi_get_name((dev_info_t *)arg), "sbusmem") == 0) {
			dev_info_t *cdip = (dev_info_t *)arg;
			int i, n;
			int slot, size;
			char ident[10];

			/* should we set DDI_PROP_NOTPROM? */

			slot = ddi_getprop(DDI_DEV_T_NONE, cdip,
			    DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP, "slot", -1);
			if (slot == -1) {
#ifdef  SBUS_DEBUG
				sbus_debug("can't get slot property\n");
#endif  SBUS_DEBUG
				return (DDI_FAILURE);
			}

#ifdef  SBUS_DEBUG
			sbus_debug("number of range properties (slots) %d\n",
			    sparc_pd_getnrng(dip));
#endif  SBUS_DEBUG

			for (i = 0, n = sparc_pd_getnrng(dip); i < n; i++) {
				struct rangespec *rp = sparc_pd_getrng(dip, i);

				if (rp->rng_cbustype == (uint_t)slot) {
					struct regspec r;

					/* create reg property */

					r.regspec_bustype = (uint_t)slot;
					r.regspec_addr = 0;
					r.regspec_size = rp->rng_size;
					(void) ddi_prop_create(DDI_DEV_T_NONE,
					    cdip, DDI_PROP_CANSLEEP, "reg",
					    (caddr_t)&r,
					    sizeof (struct regspec));

#ifdef	SBUS_DEBUG
		sbus_debug("range property: cbustype %x\n", rp->rng_cbustype);
		sbus_debug("		    coffset  %x\n", rp->rng_coffset);
		sbus_debug("		    bustype  %x\n", rp->rng_bustype);
		sbus_debug("		    offset   %x\n", rp->rng_offset);
		sbus_debug("		    size     %x\n\n", rp->rng_size);
		sbus_debug("reg property:   bustype  %x\n", r.regspec_bustype);
		sbus_debug("		    addr     %x\n", r.regspec_addr);
		sbus_debug("                size     %x\n", r.regspec_size);
#endif	SBUS_DEBUG
					/* create size property for slot */

					size = rp->rng_size;
					(void) ddi_prop_create(DDI_DEV_T_NONE,
					    cdip, DDI_PROP_CANSLEEP, "size",
					    (caddr_t)&size, sizeof (int));

					(void) sprintf(ident, "slot%x", slot);
					(void) ddi_prop_create(DDI_DEV_T_NONE,
					    cdip, DDI_PROP_CANSLEEP, "ident",
					    ident, sizeof (ident));

					return (impl_ddi_sbus_initchild(arg));
				}
			}
			return (DDI_FAILURE);
		}

		retval = impl_ddi_sbus_initchild((dev_info_t *)arg);
		if (retval != DDI_SUCCESS)
			/* node not properly initialized- NOT_WELL_FORMED. */
			return (retval);

		if ((slot_id = fd_sbus_slot_id(dip, (dev_info_t *)arg)) < 0) {
			panic("SBUS_INITCHILD: bad sbus_slot");
		}

		/*
		 * Disable this slot for auto slot config.
		 */
		if (slot_burst_patch[ppri-> sbi_id][slot_id] != 0) {
			/*
			 * Someone patched this slot to be not autoconfigable.
			 */
			return (retval);
		}

		burst_sizes = ddi_getprop(DDI_DEV_T_ANY, (dev_info_t *)
			arg, DDI_PROP_DONTPASS, "burst-sizes", -1);

		/* if there is no burst-size property, we return */
		if (burst_sizes <= NO_SBUS_BURST)
			return (retval);

		/*
		 * We update burst size tracking on this slot
		 * either this is the first burst size property
		 * or when burst size changes for that slot.
		 */
		if (ppri->burst_size[slot_id] == NO_SBUS_BURST) {
			ppri->burst_size[slot_id] = burst_sizes;
			set_sbus_burst_size(ppri->va_sbi, slot_id,
				burst_sizes);
		} else {
			uint_t new;

			new = burst_sizes & ppri->burst_size[slot_id];
			if (ppri->burst_size[slot_id] != new) {
				ppri->burst_size[slot_id] = new;
				set_sbus_burst_size(ppri->va_sbi, slot_id,
					new);
			}
		}

		return (retval);
	}

	case DDI_CTLOPS_UNINITCHILD: {
		extern void impl_ddi_sunbus_removechild(dev_info_t *);

		impl_ddi_sunbus_removechild(arg);
		return (DDI_SUCCESS);
	}

	case DDI_CTLOPS_IOMIN: {
		int val = *((int *)result);

		/*
		 * The 'arg' value of nonzero indicates 'streaming' mode.
		 * If in streaming mode, pick the largest of our burstsizes
		 * available and say that that is our minimum value (modulo
		 * what mincycle is).
		 */
		if ((int)arg)
			val = maxbit(val, sbus_max_burst_size);
		else
			val = maxbit(val, sbus_min_burst_size);

		*((int *)result) = val;
		return (ddi_ctlops(dip, rdip, op, arg, result));
	}

	/*
	 * These specific uglinesses are needed right now because
	 * we do not store our child addresses as 'sbus' addresses
	 * as yet. This will also have to change for OBPV2. This
	 * could also be much more robust in that it should check
	 * to make sure that the address really is an SBus address.
	 */

#define	REPORTDEV_BUFSIZE	1024

	case DDI_CTLOPS_REPORTDEV: {

		dev_info_t *pdev;
		int i, n, len, f_len = 0;
		char *buf;

		int sbusid = DEVI(dip)->devi_instance;

		if (DEVI_PD(rdip) == NULL)
			return (DDI_FAILURE);

		pdev = (dev_info_t *)DEVI(rdip)->devi_parent;
		buf = kmem_alloc(REPORTDEV_BUFSIZE, KM_SLEEP);
		f_len += snprintf(buf, REPORTDEV_BUFSIZE, "%s%d at %s%d",
		    ddi_driver_name(rdip), ddi_get_instance(rdip),
		    ddi_driver_name(pdev), ddi_get_instance(pdev));
		len = strlen(buf);

		for (i = 0, n = sparc_pd_getnreg(rdip); i < n; i++) {

			struct regspec *rp = sparc_pd_getreg(rdip, i);

			if (i == 0) {
				f_len += snprintf(buf + len,
				    REPORTDEV_BUFSIZE - len, ": ");
			} else {
				f_len += snprintf(buf + len,
				    REPORTDEV_BUFSIZE - len, " and ");
			}
			len = strlen(buf);
			f_len += snprintf(buf + len, REPORTDEV_BUFSIZE - len,
			    " SBus%d slot %x 0x%x", sbusid,
			    rp->regspec_bustype, rp->regspec_addr);
			len = strlen(buf);
		}

		for (i = 0, n = sparc_pd_getnintr(rdip); i < n; i++) {

			int pri, sbuslevel;

			if (i != 0) {
				f_len += snprintf(buf + len,
				    REPORTDEV_BUFSIZE - len, ",");
				len = strlen(buf);
			}

			pri = INT_IPL(sparc_pd_getintr(rdip, i)->intrspec_pri);

			if ((sbuslevel = sparc_to_sbus(pri)) != -1) {
				f_len += snprintf(buf + len,
				    REPORTDEV_BUFSIZE - len,
				    " SBus level %d", sbuslevel);
				len = strlen(buf);
			}

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

	case DDI_CTLOPS_DVMAPAGESIZE: {
		*(ulong_t *)result = IOMMU_PAGE_SIZE;
		return (DDI_SUCCESS);
	}

	case DDI_CTLOPS_XLATE_INTRS: {
		static int sbus_ctl_xlate_intrs(dev_info_t *, dev_info_t *,
			int *, struct ddi_parent_private_data *);

		return (sbus_ctl_xlate_intrs(dip, rdip, arg, result));
	}

	case DDI_CTLOPS_SLAVEONLY: {
		/* sun4d does not have slave only slots */
		return (DDI_FAILURE);
	}

	case DDI_CTLOPS_AFFINITY: {
		dev_info_t *dipb = (dev_info_t *)arg;
		int r_slot, b_slot;

		if ((b_slot = fd_sbus_slot_id(dip, dipb)) < 0)
			return (DDI_FAILURE);

		if ((r_slot = fd_sbus_slot_id(dip, rdip)) < 0)
			return (DDI_FAILURE);

		return ((b_slot == r_slot)? DDI_SUCCESS : DDI_FAILURE);

	}

	case DDI_CTLOPS_DMAPMAPC: {

#ifdef DEV_TO_DEV_DMA
		/*
		 * we sort of optimize of our DEV to DEV DVMA setup
		 * such that DMAPMAPC would never be called.
		 */
		struct dma_phys_mapc *pd = (struct dma_phys_mapc *)arg;

		/*
		 * here we verify that the target slot is on the same Sbus
		 * that master is on.
		 */
		if (fd_target_sbus_id(pd) == ddi_get_unit(dip))
			/* DDI_DMA_PARTIAL tells dma_setup to setup mappins. */
			return (DDI_DMA_PARTIAL);
		else
			return (DDI_FAILURE);
#endif DEV_TO_DEV_DMA
		cmn_err(CE_CONT, "?DDI_DMAPMAPC called!!\n");
		return (DDI_FAILURE);
	}

	default:
		return (ddi_ctlops(dip, rdip, op, arg, result));
	}
}

static
int
fd_target_slot_id(struct pte *ptep, int cur_sbus_id)
{
	int pfn, slot_id, sbus_id;

	pfn = MAKE_PFNUM(ptep);

	/* Sbus (or board) id is at PA<30:33> */
	sbus_id = (pfn >> 18) & 0xf;
	ASSERT((sbus_id >= 0) && (sbus_id < MX_SBUS));

	/*
	 * Verify that the slave is on the same Sbus
	 * that master is on.
	 */
	if (sbus_id != cur_sbus_id)
		return (-1);

	/* slot id is at PA<28:29> */
	slot_id = (pfn >> 16) & 0x3;

	return (slot_id);
}

#ifdef DEV_TO_DEV_DMA
static int
fd_target_sbus_id(struct dma_phys_mapc *pd)
{
	int pfn, sbus_id;

	pfn = MAKE_PFNUM(pd->ptes);

	/* Sbus id is at PA<30:33> */
	sbus_id = (pfn >> 18) & 0xf;

	ASSERT((sbus_id >= 0) && (sbus_id < MX_SBUS));

	return (sbus_id);
}
#endif DEV_TO_DEV_DMA

static int
fd_sbus_slot_id(dev_info_t *dip, dev_info_t *rdip)
{
	dev_info_t *ancestor = rdip;
	dev_info_t *tmp;

	/*
	 * look for the node that's a direct child of Sbus node.
	 */
	while (ancestor && (tmp = ddi_get_parent(ancestor)) != dip) {
		ancestor = tmp;
	}

	if (tmp != dip)
		/* this node is not on the Sbus root'ed by dip */
		return (-1);

	if ((DEVI_PD(ancestor))->par_nreg > 0) {
		/*
		 * resspec_bustype is the slot #.
		 */
		struct ddi_parent_private_data *ppd = DEVI_PD(ancestor);
		struct regspec *reg = ppd->par_reg;

		if (reg->regspec_bustype >= 0 &&
				reg->regspec_bustype < MX_SBUS_SLOTS)
			return ((int)reg->regspec_bustype);
		else {
			return (-1);
		}
	} else {
		return (-1);
	}
}

static struct autovec *
sbus_find_vector(dev_info_t *dip, dev_info_t *rdip, int pri)
{
	struct autovec *base = get_autovec_base(dip);
	struct autovec *av;
	int sbus_slot;

	if ((sbus_slot = fd_sbus_slot_id(dip, rdip)) < 0)
		panic("sbus_find_vector: bad sbus_slot");

	ASSERT(pri < MX_PRI);

	/* ptr arithmetic! */
	av = base + pri * MX_SBUS_SLOTS * sbus_nvect + sbus_slot * sbus_nvect;

	return (av);
}

static int
sbus_add_hard(dev_info_t *dip, dev_info_t *rdip, ddi_intrspec_t intrspec,
    ddi_iblock_cookie_t *iblock_cookiep, ddi_idevice_cookie_t *idevice_cookiep,
    uint_t (*int_handler)(caddr_t int_handler_arg), caddr_t int_handler_arg)
{
	char *name = ddi_get_name(rdip);

	struct intrspec *ispec = (struct intrspec *)intrspec;
	uint_t pri = sparc_to_sbus(INT_IPL(ispec->intrspec_pri));
	struct autovec *av = sbus_find_vector(dip, rdip, pri);

	if (av == NULL) {
		cmn_err(CE_CONT, "?sbus_add_hard: no new av slots!\n");
		return (DDI_FAILURE);
	}

	if (av->av_vector != NULL) {
		/*
		 * Support for more than 1 intr handler
		 * at the same level, same slot.
		 */
		int inst = ddi_get_instance(rdip);
		int i;

#ifdef	DEBUG
		cmn_err(CE_CONT, "?sbus_add_hard: multiple vectored interrupt "
		    "name=%s%d, pri=%d, av=0x%p\n",
		    name, inst, pri, (void *)av);
#endif	/* DEBUG */

		/*
		 * look for a vacant slot; the new handler gets installed after
		 * the previously installed handlers of this level and slot;
		 * check for duplicates along the way.
		 */
		for (i = 0; i < sbus_nvect; i++, av++) {
			if ((av->av_vector == int_handler) &&
			    (av->av_intarg == int_handler_arg)) {
				cmn_err(CE_WARN, "sbus_add_hard: %s%d: "
				    "duplicate level 0x%x interrupt handler",
				    name, inst, pri);
				break;
			}
			if (av->av_vector == NULL)  /* unused slot found */
				break;
		}

		/*
		 * check if we found an unused autovector slot;
		 * the tuneable sbus_nvect determines the number of
		 * autovector slots we have available.
		 */
		if (i >= sbus_nvect) {
			cmn_err(CE_WARN, "sbus_add_hard: %s%d: autovectored "
			    "interrupt at level 0x%x exceeded tuneable "
			    "sbus_nvect", name, inst, pri);
			return (DDI_FAILURE);
		}
	}

	av->av_devi = dip;
	av->av_intarg = int_handler_arg;
	av->av_vector = int_handler;

	ispec->intrspec_func = int_handler;
	ispec->intrspec_pri = INT_IPL(ispec->intrspec_pri);

	if (iblock_cookiep) {
		*iblock_cookiep = (ddi_iblock_cookie_t)
			ipltospl(INT_IPL(ispec->intrspec_pri));
	}

	if (idevice_cookiep) {
		idevice_cookiep->idev_vector = 0;
		idevice_cookiep->idev_priority = pri;
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static void
sbus_rem_hard(dev_info_t *dip, dev_info_t *rdip, ddi_intrspec_t intrspec,
	ddi_iblock_cookie_t iblock_cookiep)
{
	struct intrspec *ispec = (struct intrspec *)intrspec;
	uint_t pri = sparc_to_sbus(INT_IPL(ispec->intrspec_pri));
	volatile struct autovec *av = sbus_find_vector(dip, rdip, pri);
	volatile struct autovec *beginp = av;
	volatile struct autovec *endp;
	int i;
	extern void wait_till_seen(int ipl);
	static uint_t sbus_nullintr(caddr_t);

	if (ispec->intrspec_func == (uint_t (*)()) 0) {
		return;
	}

	/*
	 * In support of multiple interrupts per level, same slot,
	 * we need to search the given autovector array for the interrupt
	 * to remove.  The tuneable sbus_nvect variable determines
	 * the maximum number of autovector elements supported with
	 * this instantiation of the system.
	 */
	for (i = 0; i < sbus_nvect; i++, av++) {
		if (((av->av_vector == ispec->intrspec_func) &&
		    (av->av_devi == dip)) || (av->av_vector == NULL))
			break;
	}

	if (av->av_vector == NULL || i >= sbus_nvect) {
		cmn_err(CE_CONT, "?sbus_rem_hard: no av found!\n");
		return;
	}

	/*
	 * now that we found an autovector to remove,
	 * find another one at the end of the array to move back up
	 * into its spot.  Find the end/last installed autovector.
	 */
	endp = beginp + (sbus_nvect - 1);	/* work back from the end */
	while ((endp > beginp) && (endp->av_vector == NULL))
		endp--;

	/*
	 * if the autovector to be ousted is at the end,
	 * shorten the list, else move an autovector from the end
	 * to the vacated slot.
	 */
	if (av == endp) {
		av->av_vector = NULL;
		wait_till_seen(INT_IPL(ispec->intrspec_pri));
	} else {
		/*
		 * The order of these writes is important!
		 */
		av->av_vector = sbus_nullintr;
		/*
		 * protect against calling the handler being removed
		 * with the arg meant for the handler at the end of
		 * the list
		 */
		wait_till_seen(INT_IPL(ispec->intrspec_pri));
		av->av_devi = endp->av_devi;
		av->av_intarg = endp->av_intarg;
		av->av_vector = endp->av_vector;
		/*
		 * protect against removing the handler next on
		 * the list after some cpu has already passed (missed)
		 * the new copy of it in the list
		 * (prevents spurious interrupt message)
		 */
		wait_till_seen(INT_IPL(ispec->intrspec_pri));
		endp->av_vector = NULL;
		/*
		 * protect against passing the arg meant for a
		 * new handler being added to the list (by
		 * sbus_add_hard) to the handler we've just removed
		 * from the end of the list
		 */
		wait_till_seen(INT_IPL(ispec->intrspec_pri));
	}

	ispec->intrspec_func = (uint_t (*)()) 0;
}


/*
 * add_intrspec - Add an interrupt specification.
 */
static int
sbus_add_intrspec(
	dev_info_t *dip,
	dev_info_t *rdip,
	ddi_intrspec_t intrspec,
	ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	uint_t (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg,
	int kind)
{
	int r;

	ASSERT(intrspec != 0);
	ASSERT(rdip != 0);
	ASSERT(ddi_get_driver(rdip) != 0);

	switch (kind) {
	case IDDI_INTR_TYPE_NORMAL:
		mutex_enter(&av_lock);
		r = sbus_add_hard(dip, rdip, intrspec,
			iblock_cookiep, idevice_cookiep,
			int_handler, int_handler_arg);
		mutex_exit(&av_lock);
		break;

	default:
		/*
		 * I can't do it, pass the buck to my parent
		 */
		r = i_ddi_add_intrspec(dip, rdip, intrspec,
			iblock_cookiep, idevice_cookiep,
			int_handler, int_handler_arg, kind);

		if (r == DDI_SUCCESS && idevice_cookiep) {
			idevice_cookiep->idev_priority =
			    sparc_to_sbus(idevice_cookiep->idev_priority);
		}
		break;
	}

	return (r);
}

/*
 * remove_intrspec - Remove an interrupt specification.
 */
static void
sbus_remove_intrspec(
	dev_info_t *dip,
	dev_info_t *rdip,
	ddi_intrspec_t intrspec,
	ddi_iblock_cookie_t iblock_cookiep)
{
	ASSERT(intrspec != 0);
	ASSERT(rdip != 0);

	mutex_enter(&av_lock);
	sbus_rem_hard(dip, rdip, intrspec, iblock_cookiep);
	mutex_exit(&av_lock);
}

/*
 * We're prepared to claim that the interrupt string is in
 * the form of a list of <SBusintr> specifications. Translate it.
 */
static int
sbus_ctl_xlate_intrs(dev_info_t *dip, dev_info_t *rdip, int *in,
	struct ddi_parent_private_data *pdptr)
{
	int n;
	size_t size;
	struct intrspec *new;

	static char bad_sbusintr_fmt[] =
	    "sbus%d: bad interrupt spec for %s%d - SBus level %d\n";

	/*
	 * The list consists of <SBuspri> elements
	 */
	if ((n = *in++) < 1)
		return (DDI_FAILURE);

	pdptr->par_nintr = n;
	size = n * sizeof (struct intrspec);
	new = pdptr->par_intr = kmem_zalloc(size, KM_SLEEP);

	while (n--) {
		int level = *in++;

		if (level < 1 || level > 7) {
			cmn_err(CE_CONT, bad_sbusintr_fmt,
			    DEVI(dip)->devi_instance, DEVI(rdip)->devi_name,
			    DEVI(rdip)->devi_instance, level);
			goto broken;
			/*NOTREACHED*/
		}

		new->intrspec_pri = sbus_to_sparc(level) | INTLEVEL_SBUS;
		new++;
	}

	return (DDI_SUCCESS);
	/*NOTREACHED*/

broken:
	kmem_free(pdptr->par_intr, size);
	pdptr->par_intr = (void *)0;
	pdptr->par_nintr = 0;
	return (DDI_FAILURE);
}

/*
 * Here's a slight ugliness.  Because the sun4m architecture decided to
 * change the mapping of SBus levels to sparc ipl's, we can't both hide
 * all the details of interrupt mappings here -and- maintain implementation
 * architecture dependence, sigh.  So, we reference a table that's defined
 * in autoconf.c of the relevant kernel architecture. Either this
 * remapping should never have happened, or the sun4m SBus shouldn't have
 * had the name 'sbus'.  But -hey- we all have to live with our past
 * misdemeanours .. and this doesn't seem too horrendous.
 */

static int
sbus_to_sparc(int sbuslevel)
{
	static const char sbus_to_sparc_tbl[] = {
		-1, 2, 3, 5, 7, 9, 11, 13
	};

	if (sbuslevel < 1 || sbuslevel > 7)
		return (-1);
	else
		return ((int)sbus_to_sparc_tbl[sbuslevel]);
}

static int
sparc_to_sbus(int pri)
{
	static const char sparc_to_sbus_tbl[] = {
		-1, -1, 1, 2, -1, 3, -1, 4,
		-1, 5, -1, 6, -1, 7, -1, -1
	};

	if (pri < 1 || pri > 15)
		return (-1);
	else
		return ((int)sparc_to_sbus_tbl[pri]);
}

/*ARGSUSED*/
static uint_t
sbus_nullintr(caddr_t intrg)
{
	return (DDI_INTR_UNCLAIMED);
}
