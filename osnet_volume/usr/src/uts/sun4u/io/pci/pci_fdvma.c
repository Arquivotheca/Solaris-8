/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_fdvma.c	1.7	99/11/15 SMI"

/*
 * Internal PCI Fast DVMA implementation
 */
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/async.h>
#include <sys/sysmacros.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/dvma.h>
#include <sys/pci/pci_obj.h>

/*LINTLIBRARY*/
/*
 * function prototypes for fast dvma ops:
 */
static void fast_dvma_kaddr_load(ddi_dma_handle_t h, caddr_t a, uint_t len,
	uint_t index, ddi_dma_cookie_t *cp);
static void fast_dvma_unload(ddi_dma_handle_t h, uint_t index, uint_t view);
static void fast_dvma_sync(ddi_dma_handle_t h, uint_t index, uint_t view);

/*
 * fast dvma ops structure:
 */
static struct dvma_ops fast_dvma_ops = {
	DVMAO_REV,
	fast_dvma_kaddr_load,
	fast_dvma_unload,
	fast_dvma_sync
};

/*
 * The following routines are used to implement the sun4u fast dvma
 * routines on this bus.
 */

/*ARGSUSED*/
static void
fast_dvma_kaddr_load(ddi_dma_handle_t h, caddr_t a, uint_t len, uint_t index,
	ddi_dma_cookie_t *cp)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)h;
	struct fast_dvma *fdvma = (struct fast_dvma *)mp->dmai_nexus_private;
	pci_t *pci_p = (pci_t *)fdvma->softsp;
	iommu_t *iommu_p = pci_p->pci_iommu_p;
	dvma_addr_t dvma_addr, dvma_pg;
	uint32_t offset;
	size_t npages, pg_index;
	iopfn_t pfn;
	int i;
	uint64_t tte;

	offset = (uint32_t)a & IOMMU_PAGE_OFFSET;
	npages = IOMMU_BTOPR(len + offset);
	if (!npages)
		return;

	/* make sure we don't exceed reserved boundary */
	DEBUG3(DBG_FAST_DVMA, pci_p->pci_dip,
		"kaddr_load - a=%x len=%x index=%x\n", a, len, index);
	if (index + npages > mp->dmai_ndvmapages) {
		cmn_err(pci_panic_on_fatal_errors ? CE_PANIC : CE_WARN,
			"%s%d: kaddr_load: index + size exceeds limit\n",
			ddi_driver_name(pci_p->pci_dip),
			ddi_get_instance(pci_p->pci_dip));
		return;
	}
	fdvma->pagecnt[index] = npages;

	dvma_addr = mp->dmai_mapping + IOMMU_PTOB(index);
	dvma_pg = IOMMU_BTOP(dvma_addr);
	pg_index = dvma_pg - iommu_p->dvma_base_pg;

	/* construct the dma cookie to be returned */
	MAKE_DMA_COOKIE(cp, dvma_addr | offset, len);
	DEBUG2(DBG_FAST_DVMA, pci_p->pci_dip,
		"kaddr_load - dmac_address=%x dmac_size=%x\n",
		cp->dmac_address, cp->dmac_size);

	for (i = 0; i < npages; i++, a += IOMMU_PAGE_SIZE) {
		pfn = hat_getpfnum(kas.a_hat, a);
		if (pfn == PFN_INVALID)
			goto bad_pfn;

		IOMMU_PAGE_FLUSH(iommu_p, (dvma_pg + i));
		if (i == 0)	/* setup template, all bits except pfn value */
			tte = MAKE_TTE_TEMPLATE(pfn, mp);

		/* XXX assumes iommu and mmu has same page size */
		iommu_p->iommu_tsb_vaddr[pg_index + i] = tte | IOMMU_PTOB(pfn);
	}
	mp->dmai_flags = DMAI_FLAGS_INUSE;
	return;
bad_pfn:
	cmn_err(CE_WARN, "%s%d: can't get page frame for vaddr %x",
		ddi_driver_name(pci_p->pci_dip),
		ddi_get_instance(pci_p->pci_dip), (int)a);
}

/*ARGSUSED*/
static void
fast_dvma_unload(ddi_dma_handle_t h, uint_t index, uint_t view)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)h;
	struct fast_dvma *fdvma = (struct fast_dvma *)mp->dmai_nexus_private;
	pci_t *pci_p = (pci_t *)fdvma->softsp;

	DEBUG2(DBG_FAST_DVMA, pci_p->pci_dip,
		"unload - index=%x view=%x\n", index, view);
	DEBUG2(DBG_FAST_DVMA, pci_p->pci_dip,
		"unload - sbuf_flush addr %x len=%x\n",
		IOMMU_PTOB(index), IOMMU_PTOB(fdvma->pagecnt[index]));
	STREAMING_BUF_FLUSH(pci_p, mp, 0,
		IOMMU_PTOB(index), IOMMU_PTOB(fdvma->pagecnt[index]));
}

/*ARGSUSED*/
static void
fast_dvma_sync(ddi_dma_handle_t h, uint_t index, uint_t view)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)h;
	struct fast_dvma *fdvma = (struct fast_dvma *)mp->dmai_nexus_private;
	pci_t *pci_p = (pci_t *)fdvma->softsp;

	DEBUG2(DBG_FAST_DVMA, pci_p->pci_dip,
		"sync - index=%x view=%x\n", index, view);
	DEBUG2(DBG_FAST_DVMA, pci_p->pci_dip,
		"sync - sbuf_flush addr %x len=%x\n",
		IOMMU_PTOB(index), IOMMU_PTOB(fdvma->pagecnt[index]));
	STREAMING_BUF_FLUSH(pci_p, mp, 0,
		IOMMU_PTOB(index), IOMMU_PTOB(fdvma->pagecnt[index]));
}

int
pci_fdvma_reserve(dev_info_t *dip, dev_info_t *rdip, pci_t *pci_p,
	struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep)
{
	struct fast_dvma *fdvma;
	dvma_addr_t dvma_pg;
	iommu_t *iommu_p = pci_p->pci_iommu_p;
	size_t npages;
	ddi_dma_impl_t *mp;

	if (pci_disable_fdvma)
		return (DDI_FAILURE);

	DEBUG2(DBG_DMA_CTL, dip, "DDI_DMA_RESERVE: rdip=%s%d\n",
		ddi_driver_name(rdip), ddi_get_instance(rdip));

	/*
	 * Check the limit structure.
	 */
	if ((dmareq->dmar_limits->dlim_addr_lo >=
		dmareq->dmar_limits->dlim_addr_hi) ||
		(dmareq->dmar_limits->dlim_addr_hi < iommu_p->iommu_dvma_base))
		return (DDI_DMA_BADLIMITS);

	/*
	 * Check the size of the request.
	 */
	npages = dmareq->dmar_object.dmao_size;
	if (npages > iommu_p->iommu_dvma_reserve)
		return (DDI_DMA_NORESOURCES);

	/*
	 * Allocate the dma handle.
	 */
	mp = (ddi_dma_impl_t *)kmem_zalloc(sizeof (ddi_dma_impl_t), KM_SLEEP);

	/*
	 * Get entries from dvma space map.
	 */
	iommu_p = pci_p->pci_iommu_p;
	dvma_pg = iommu_get_dvma_pages(iommu_p, npages,
		dmareq->dmar_limits->dlim_addr_lo,
		dmareq->dmar_limits->dlim_addr_hi,
		0, dmareq->dmar_limits->dlim_cntr_max + 1,
		(dmareq->dmar_fp == DDI_DMA_SLEEP) ? 1 : 0);
	if (dvma_pg == 0) {
		kmem_free(mp, sizeof (ddi_dma_impl_t));
		return (DDI_DMA_NOMAPPING);
	}
	iommu_p->iommu_dvma_reserve -= npages;

	/*
	 * Create the fast dvma request structure.
	 */
	fdvma = kmem_alloc(sizeof (struct fast_dvma), KM_SLEEP);
	fdvma->pagecnt = kmem_alloc(npages * sizeof (uint_t), KM_SLEEP);
	fdvma->ops = &fast_dvma_ops;
	fdvma->softsp = (caddr_t)pci_p;
	fdvma->sync_flag = NULL;

	/*
	 * Initialize the handle.
	 */
	mp->dmai_rdip = rdip;
	mp->dmai_rflags = DMP_BYPASSNEXUS | DDI_DMA_READ;
	if (!pci_stream_buf_enable || !pci_stream_buf_exists)
		mp->dmai_rflags |= DDI_DMA_CONSISTENT;
	mp->dmai_minxfer = dmareq->dmar_limits->dlim_minxfer;
	mp->dmai_burstsizes = dmareq->dmar_limits->dlim_burstsizes;
	mp->dmai_mapping = IOMMU_PTOB(dvma_pg);
	mp->dmai_ndvmapages = npages;
	mp->dmai_size =  npages * IOMMU_PAGE_SIZE;
	mp->dmai_nwin = 0;
	mp->dmai_nexus_private = (caddr_t)fdvma;
	DEBUG4(DBG_DMA_CTL, dip,
		"DDI_DMA_RESERVE: mp=%x dvma=%x npages=%x private=%x\n",
		mp, mp->dmai_mapping, npages, fdvma);
	*handlep = (ddi_dma_handle_t)mp;
	return (DDI_SUCCESS);
}

int
pci_fdvma_release(dev_info_t *dip, pci_t *pci_p, ddi_dma_impl_t *mp)
{
	iommu_t *iommu_p = pci_p->pci_iommu_p;
	size_t npages;
	struct fast_dvma *fdvma = (struct fast_dvma *)mp->dmai_nexus_private;

	if (pci_disable_fdvma)
		return (DDI_FAILURE);

	/*
	 * Make sure the handle has really been setup for fast dma.
	 */
	if (mp->dmai_rflags != (DMP_BYPASSNEXUS|DDI_DMA_READ)) {
		DEBUG0(DBG_DMA_CTL, dip, "DDI_DMA_RELEASE: not fast dma\n");
		return (DDI_FAILURE);
	}

	/*
	 * Make sure all the reserved dvma addresses are flushed
	 * from the iommu and freed.
	 */
	iommu_p = pci_p->pci_iommu_p;
	iommu_unmap_window(iommu_p, mp);
	STREAMING_BUF_FLUSH(pci_p, mp, 0, 0, 0);
	npages = mp->dmai_ndvmapages;
	iommu_free_dvma_pages(iommu_p, npages, (dvma_addr_t)mp->dmai_mapping);
	mp->dmai_ndvmapages = 0;

	/*
	 * Now that we've freed some dvma space, see if there
	 * is anyone waiting for some.
	 */
	if (iommu_p->iommu_dvma_call_list_id != 0) {
		DEBUG0(DBG_DMA_CTL, dip, "run dvma callback\n");
		ddi_run_callback(&iommu_p->iommu_dvma_call_list_id);
	}

	/*
	 * Free the memory allocated by the private data.
	 */
	kmem_free(fdvma->pagecnt, npages * sizeof (uint_t));
	kmem_free(fdvma, sizeof (struct fast_dvma));

	/*
	 * Free the handle and decrement reserve counter.
	 */
	kmem_free(mp, sizeof (ddi_dma_impl_t));
	iommu_p->iommu_dvma_reserve += npages;

	/*
	 * Now that we've free a handle, see if there is anyone
	 * waiting for one.
	 */
	if (pci_p->pci_handle_pool_call_list_id != 0) {
		DEBUG0(DBG_DMA_CTL, dip, "run handle callback\n");
		ddi_run_callback(&pci_p->pci_handle_pool_call_list_id);
	}
	return (DDI_SUCCESS);
}
