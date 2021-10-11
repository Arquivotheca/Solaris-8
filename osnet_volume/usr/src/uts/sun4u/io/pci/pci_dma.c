/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_dma.c	1.11	99/12/01 SMI"

/*
 * PCI nexus DVMA objects and support routines:
 *	dmai_nexus_private implementation
 *	dma_map/dma_bind_handle implementation
 *	utility routines to check dma size and limits
 *	runtime DVMA debug
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/async.h>
#include <sys/sysmacros.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/pci/pci_obj.h>

/*LINTLIBRARY*/

/* dvma space cache performance counters */
static uint_t pci_dvma_fast_track_success_cnt = 0;
static uint_t pci_dvma_no_fast_track_cnt = 0;
static uint_t pci_dvma_cache_exhaust_fallback = 0;

/* internal verification routines */
static int
check_dma_limits(pci_t *pci_p, struct ddi_dma_req *dmareq, size_t *sizep);

static void
check_dma_size(ddi_dma_impl_t *mp, size_t *sizep);

static int
check_dma_mode(pci_t *pci_p, caddr_t vaddr, struct ddi_dma_req *dmareq,
	size_t npages, iommu_dma_t *dma_type, pci_dmai_nexus_private_t *np);

#define	DMA_IS_NOLIMIT(lo, hi, cnt_max) (((lo) == 0) && \
	(((hi) & UINT32_MAX) == UINT32_MAX) && \
	(((cnt_max) & UINT32_MAX) == UINT32_MAX))

/*
 * Allocate a structure of ddi_dma_impt_t + pci_dmai_nexus_private_t,
 * and collapse the number of kmem_zalloc() calls. Should be inlined.
 *
 * return: DDI_SUCCESS DDI_DMA_NORESOURCES
 */
int
pci_alloc_mp(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t *handlep,
	int (*waitfp)(caddr_t), caddr_t arg, pci_t *pci_p)
{
	register ddi_dma_impl_t *mp;
	int sz = sizeof (ddi_dma_impl_t) + sizeof (pci_dmai_nexus_private_t);

	ASSERT(handlep);

	/* Caution: we don't use zalloc to enhance performance! */
	mp = kmem_alloc(sz, waitfp == DDI_DMA_SLEEP ? KM_SLEEP : KM_NOSLEEP);
	if (mp == 0) {
		DEBUG0(DBG_DMA_MAP, dip, "can't alloc dma_impl\n");
		if (waitfp != DDI_DMA_DONTWAIT) {
			DEBUG0(DBG_DMA_MAP, dip, "set callback\n");
			ddi_set_callback(waitfp, arg,
				&pci_p->pci_handle_pool_call_list_id);
		}
		return (DDI_DMA_NORESOURCES);
	}

	mp->dmai_nexus_private = (caddr_t)(mp + 1);
	((pci_dmai_nexus_private_t *)mp->dmai_nexus_private)->lists_sz = 0;
	mp->dmai_rdip = rdip;
	mp->dmai_inuse = 0;
	/* 4294615 pci: panic while calling ddi_check_dma_handle */
	mp->dmai_fault = 0;
	mp->dmai_fault_check = NULL;
	mp->dmai_fault_notify = NULL;

	/*
	 * kmem_alloc debug: the following fields are not zero-ed
	mp->dmai_rflags = 0;
	mp->dmai_ndvmapages = 0;
	mp->dmai_size = 0;
	mp->dmai_offset = 0;
	mp->dmai_mapping = 0;
	mp->dmai_nwin = 0;
	mp->dmai_winsize = 0;
	bzero(&mp->dmai_object, sizeof (ddi_dma_obj_t));
	bzero(&mp->dmai_attr, sizeof (ddi_dma_attr_t));
	 */

	*handlep = (ddi_dma_handle_t)mp;
	DEBUG2(DBG_DMA_MAP, dip, "pci_alloc_mp: rdip=%s sz=%x\n",
		ddi_driver_name(rdip), sz);
	return (DDI_SUCCESS);
}

void
pci_free_mp(ddi_dma_impl_t *mp)
{
	pci_dmai_nexus_private_t *np;
	ASSERT(mp);

	np = (pci_dmai_nexus_private_t *)mp->dmai_nexus_private;
	pci_free_np_pages(np, 0);
	kmem_free(mp,
		sizeof (ddi_dma_impl_t) + sizeof (pci_dmai_nexus_private_t));
}

/* This is a potential place we can record npages statistics */
int
pci_alloc_np_pages(pci_dmai_nexus_private_t *np, size_t npages,
	int alloc_cookies)
{
	size_t sz, ck_sz, pg_sz;

	bzero(np, sizeof (pci_dmai_nexus_private_t));
	if (!npages)
		return (DDI_FAILURE);

	if ((npages == 1) && !alloc_cookies)
		return (0);

	ck_sz = alloc_cookies ? sizeof (ddi_dma_cookie_t) * (npages + 1) : 0;
	pg_sz = sizeof (iopfn_t) * npages;
	np->lists_sz = sz = pg_sz + ck_sz;	/* np->lists_sz could be 0 */

	if (sz) { /* cookies must be contiguous per ddi */
		np->page_list = kmem_alloc(sz, KM_SLEEP);
		np->npages = npages;
	}

	if (alloc_cookies) {
		np->ncookies = npages + 1;
		np->cookie_list = (ddi_dma_cookie_t *)(np->page_list + npages);
	}
	return (0);
}

void
pci_free_np_pages(pci_dmai_nexus_private_t *np, int purge)
{
	if (np->lists_sz) {
		kmem_free(np->page_list, np->lists_sz);
	}
	if (purge) {
		bzero(np, sizeof (struct pci_dmai_nexus_private));
	}
}

#if defined(DEBUG)
size_t
pci_get_np_ncookies(pci_dmai_nexus_private_t *np) {
	return (np->ncookies);
}

void
pci_set_np_ncookies(pci_dmai_nexus_private_t *np, size_t nc) {
	np->ncookies = nc;
}

iopfn_t
pci_get_mp_pfn(pci_dmai_nexus_private_t *np, size_t page_no)
{
	ASSERT(np);
	if (np->lists_sz)
		return (np->page_list[page_no]);
	ASSERT(page_no == 0);
	return (np->pfn0);
}

void
pci_set_mp_pfn(pci_dmai_nexus_private_t *np, size_t page_no, iopfn_t pfn)
{
	ASSERT(np);
	if (np->lists_sz)
		np->page_list[page_no] = pfn;
	else {
		ASSERT(page_no == 0);
		np->pfn0 = pfn;
	}
}

static uint_t pci_dvmaft_npages = 0;
static uint_t pci_dvmaft_align = 0;
static uint_t pci_dvmaft_nwin = 0;
static uint_t pci_dvmaft_limit = 0;
#endif

/*
 * fast trace cache entry to iommu context, inserts 3 0 bits between
 * upper 6-bits and lower 3-bits of the 9-bit cache entry
 */
#define	IOMMU_FCE_TO_CTX(i)	(((((i) << 3) | ((i) & 0x7)) & 0xfc7) + 56)

/*
 * common routine shared by pci_dma_bindhdl() and pci_dma_map()
 * return value:
 *	DDI_DMA_PARTIAL_MAP	 1
 *	DDI_DMA_MAPOK		 0
 *	DDI_DMA_MAPPED		 0
 *	DDI_DMA_NORESOURCES	-1
 *	DDI_DMA_NOMAPPING	-2
 *	DDI_DMA_TOOBIG		-3
 */
int
pci_dma_map_impl(dev_info_t *dip, dev_info_t *rdip,
	struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep, int bindcall)
{
	pci_t *pci_p = get_pci_soft_state(ddi_get_instance(dip));
	iommu_t *iommu_p = pci_p->pci_iommu_p;

	ddi_dma_impl_t *mp = NULL;
	pci_dmai_nexus_private_t *np = NULL;

	iommu_dma_t dma_type = IOMMU_XLATE; /* default transfer type */
	size_t actual_size, requested_size;
	uintptr_t offset, vaddr = 0;
	size_t npages, nwin;
	int dmareq_type = dmareq->dmar_object.dmao_type;
	int rval, ret, red_zone;
	dvma_addr_t dvma_pg, dvma_pg_index;
	uint64_t align;
	iopfn_t pfn;

	ulong_t hi, lo;
	uint_t clustsz;
	uint32_t count_max;

	/* figure out npages and allocate pfn_list */
	switch (dmareq_type) {
	case DMA_OTYP_VADDR:
		/*
		 * Get the mappings virtual address, offset and address space
		 * structure from the map request.
		 */
		vaddr = (uintptr_t)dmareq->dmar_object.dmao_obj.virt_obj.v_addr;
		offset = vaddr & IOMMU_PAGE_OFFSET;
		break;

	case DMA_OTYP_PAGES:
		offset = dmareq->dmar_object.dmao_obj.pp_obj.pp_offset;
		break;

	case DMA_OTYP_PADDR:
	default:
		ASSERT(0);
		DEBUG1(DBG_DMA_MAP, dip, "unsupported dma type (%x)\n",
			dmareq_type);
		return (DDI_DMA_NOMAPPING);
	}

	actual_size = requested_size = dmareq->dmar_object.dmao_size;
	npages = IOMMU_BTOPR(requested_size + offset);
	DEBUG4(DBG_DMA_MAP, dip, "vaddr=%08x.%08x offset=%x DMA pages=%x\n",
	    (uint_t)(((uint64_t)vaddr) >> 32), (uint_t)(vaddr), offset, npages);

	if (handlep) {
		if (bindcall) {
			mp = (ddi_dma_impl_t *)(*handlep);
			if (mp->dmai_flags & DMAI_FLAGS_BYPASS)
				dma_type = IOMMU_BYPASS;

			lo = (ulong_t)mp->dmai_attr.dma_attr_addr_lo;
			hi = (ulong_t)mp->dmai_attr.dma_attr_addr_hi;
			count_max = (uint32_t)mp->dmai_attr.dma_attr_count_max;
		} else {
			if (pci_alloc_mp(dip, rdip, handlep, dmareq->dmar_fp,
				dmareq->dmar_arg, pci_p))
				return (DDI_DMA_NORESOURCES);

			mp = (ddi_dma_impl_t *)(*handlep);
			mp->dmai_minxfer = dmareq->dmar_limits->dlim_minxfer;
			mp->dmai_burstsizes =
				dmareq->dmar_limits->dlim_burstsizes;

			lo = dmareq->dmar_limits->dlim_addr_lo;
			hi = dmareq->dmar_limits->dlim_addr_hi;
			count_max = dmareq->dmar_limits->dlim_cntr_max;
			if (DMA_IS_NOLIMIT(lo, hi, count_max))
				mp->dmai_flags |= DMP_NOLIMIT;
		}
		mp->dmai_rflags	= dmareq->dmar_flags & DMP_DDIFLAGS;
		if (!pci_stream_buf_enable || !pci_stream_buf_exists)
			mp->dmai_rflags |= DDI_DMA_CONSISTENT;
		mp->dmai_object	= dmareq->dmar_object; /* whole thing */

		np = (pci_dmai_nexus_private_t *)mp->dmai_nexus_private;
		/* also allocate cookies if bypass is implied */
		if (pci_alloc_np_pages(np, npages, dma_type == IOMMU_BYPASS)) {
			ret = DDI_DMA_NOMAPPING;
			goto cleanup;
		}
	}

	/* trim dma size down to window size */
	if (bindcall)
		check_dma_size(mp, &actual_size);
	else
		if (ret = check_dma_limits(pci_p, dmareq, &actual_size))
			goto cleanup;

	/*
	 * if nwin = 1, then the following ASSERT should be true
	ASSERT(actual_size == requested_size);
	 */

	if (actual_size < requested_size) {
		if ((dmareq->dmar_flags & DDI_DMA_PARTIAL) == 0) {
			DEBUG2(DBG_DMA_MAP, pci_p->pci_dip,
				"req size %x too large(%x)\n",
				requested_size, actual_size);
			ret = DDI_DMA_TOOBIG;
			goto cleanup;
		}
		nwin = (uint_t)((requested_size / actual_size) +
			((requested_size % actual_size) ? 1 : 0));
		DEBUG3(DBG_DMA_MAP, dip, "partial - size %x => %x(%x wins)\n",
			dmareq->dmar_object.dmao_size, actual_size, nwin);
		ret = DDI_DMA_PARTIAL_MAP;
	} else {
		nwin = 1;
		ret = DDI_DMA_MAPPED;
	}

	/* fill the pfn list */
	switch (dmareq_type) {
	case DMA_OTYP_VADDR: {
		page_t **pplist = dmareq->dmar_object.dmao_obj.virt_obj.v_priv;
		if (pplist == NULL) {
			rval = check_dma_mode(pci_p, (caddr_t)vaddr, dmareq,
				npages, &dma_type, np);

			/* ASSERT(dma_type == IOMMU_XLATE); XXX peer-to-peer? */
			if (rval) {
				/* ASSERT(0); */
				ret = rval;
				goto cleanup;
			}
		} else { /* we don't check mixed mode here ??? */
			int i;
			pplist += offset / IOMMU_PAGE_SIZE;
			DEBUG2(DBG_DMA_MAP, dip,
				"shadow pplist=%x, %x pages, pfns= ",
				pplist, npages);
			for (i = 0; i < npages; i++) {
				pfn = page_pptonum(pplist[i]);
				if (np)
					PCI_SET_MP_PFN(np, i, pfn);
				DEBUG1(DBG_DMA_MAP|DBG_CONT, dip, "%x ", pfn);
			}
			DEBUG0(DBG_DMA_MAP|DBG_CONT, dip, "\n");
		}
		}
		break;

	case DMA_OTYP_PAGES: {
		int i;
		page_t *pp = dmareq->dmar_object.dmao_obj.pp_obj.pp_pp;
		DEBUG1(DBG_DMA_MAP, dip, "pp=%x pfns=", pp);

		for (i = 0; i < npages; i++, pp = pp->p_next) {
			pfn = page_pptonum(pp);
			if (np)
				PCI_SET_MP_PFN(np, i, pfn);
			DEBUG1(DBG_DMA_MAP|DBG_CONT, dip, "%x ", pfn);
		}
		DEBUG0(DBG_DMA_MAP|DBG_CONT, dip, "\n");
		}
		break;

	default:
		ASSERT(0);
	}

	/*
	 * If this was just an advisory mapping call then we're done.
	 */
	if (handlep == NULL)
		return (DDI_DMA_MAPOK);

	if (ret == DDI_DMA_MAPPED)
		mp->dmai_rflags &= ~DDI_DMA_PARTIAL;

	/*
	 * If we are doing DMA to the same PCI bus segment, we don't
	 * use the IOMMU, we just use the PCI bus address.
	 */
	switch (dma_type) {
	case PCI_PEER_TO_PEER:
		mp->dmai_mapping =
			PCI_GET_MP_PFN(np, 0) << MMU_PAGESHIFT + offset;
		mp->dmai_flags |= DMAI_FLAGS_PEER_TO_PEER;
		/* FALLTHROUGH */
	case IOMMU_BYPASS:
		mp->dmai_ndvmapages = 0;
		mp->dmai_flags |= DMAI_FLAGS_INUSE;

		/*
		 * PEER_TO_PEER or IOMMU_BYPASS DMA must be consistent and
		 * can't have a redzone.
		 */
		mp->dmai_rflags |= DDI_DMA_CONSISTENT | DMP_NOSYNC;
		mp->dmai_rflags &= ~DDI_DMA_REDZONE;

		/*
		 * Initialize the handle's cookie information based on the
		 * newly established mapping.
		 */
		dump_dma_handle(DBG_DMA_MAP, dip, mp);
		return (DDI_DMA_MAPPED);
	default:
		break;
	}

	ASSERT(dma_type == IOMMU_XLATE);

	mp->dmai_nwin = nwin;
	mp->dmai_size = mp->dmai_winsize = actual_size;

	npages = IOMMU_BTOPR(actual_size + offset); /* overwrite npages */
	mp->dmai_ndvmapages = npages;
	align = bindcall ? mp->dmai_attr.dma_attr_align : 0;
	DEBUG4(DBG_DMA_MAP, dip,
		"dmai_size=%x dmai_winsize=%x nwin=%x align=%x\n",
		mp->dmai_size, mp->dmai_winsize, mp->dmai_nwin, align);

	red_zone = HAS_REDZONE(mp);
	if (mp->dmai_rflags & DDI_DMA_CONSISTENT)
		mp->dmai_rflags |= DMP_NOSYNC;

	pfn = PCI_GET_MP_PFN(np, 0);
	np->tte = MAKE_TTE_TEMPLATE(pfn, mp);

	/* less than 8 pages fast track */
	clustsz = pci_dvma_page_cache_clustsz;
	if (((npages + red_zone) <= clustsz) && (align <= clustsz) &&
		(nwin == 1) && (mp->dmai_flags & DMP_NOLIMIT)) {
		uint64_t *last_addr = iommu_p->iommu_tsb_vaddr +
			pci_dvma_page_cache_entries * clustsz;
		uint64_t *addr;
		int i = iommu_p->iommu_dvma_addr_scan_start;
		extern uint8_t ldstub(uint8_t *);

#ifdef lint
		last_addr = last_addr;
#endif
		DEBUG2(DBG_DMA_MAP, dip, "fast_track: last_addr=%x "
			"tsb_base=%x\n", last_addr, iommu_p->iommu_tsb_vaddr);

		for (; i < pci_dvma_page_cache_entries; i++)
			if (!ldstub(iommu_p->iommu_dvma_cache_locks + i))
				break;

		if (i >= pci_dvma_page_cache_entries)
			for (i = 0; i < pci_dvma_page_cache_entries; i++)
				if (!ldstub(iommu_p->iommu_dvma_cache_locks +
					i))
					break;

		if (i >= pci_dvma_page_cache_entries) {
			pci_dvma_cache_exhaust_fallback++;
			goto fallback;
		}

		if (pci_use_contexts) {
			dvma_context_t ctx = IOMMU_FCE_TO_CTX(i);
			np->tte |= pci_iommu_ctx2tte(ctx);
			mp->dmai_flags |= DMAI_FLAGS_CONTEXT;
			DEBUG1(DBG_DMA_MAP, dip, "fast_track: ctx=0x%x ", ctx);
		}

		iommu_p->iommu_dvma_addr_scan_start = i + 1;
		dvma_pg = iommu_p->dvma_base_pg + i * clustsz;
		DEBUG1(DBG_DMA_MAP, dip, "fast_track: dvma_pg=%x\n", dvma_pg);

		addr = iommu_p->iommu_tsb_vaddr + i * clustsz;
		for (i = 0; i < npages; i++, addr++) {
			ASSERT(TTE_IS_INVALID(*addr));
			*addr = np->tte | IOMMU_PTOB(PCI_GET_MP_PFN(np, i));
			DEBUG3(DBG_DMA_MAP, dip,
				"fast_track: tte=%08x.%08x addr=%08x\n",
				(uint_t)(*addr >> 32), (uint_t)(*addr), addr);
		}

		pci_dvma_fast_track_success_cnt++;
		if (red_zone) {
			ASSERT(TTE_IS_INVALID(*addr));
			*addr = COMMON_IOMMU_INVALID_TTE;
			IOMMU_PAGE_FLUSH(iommu_p, dvma_pg + npages);
		}
		mp->dmai_mapping = IOMMU_PTOB(dvma_pg) + offset;
		mp->dmai_flags |= DMAI_FLAGS_FASTTRACK;
		if (pci_dvma_debug_on)
			pci_dvma_alloc_debug((char *)mp->dmai_mapping,
				mp->dmai_size, mp);
		goto done;
	}

#ifdef DEBUG
	if ((npages + red_zone) > clustsz)
		pci_dvmaft_npages++;
	else if (align > clustsz)
		pci_dvmaft_align++;
	else if (nwin != 1)
		pci_dvmaft_nwin++;
	else if (!(mp->dmai_flags & DMP_NOLIMIT))
		pci_dvmaft_limit++;
#endif

fallback:
	pci_dvma_no_fast_track_cnt++;
	if (align) {
		align--;
		align = IOMMU_BTOP(align);
	}
	if (bindcall && mp->dmai_attr.dma_attr_seg)
		align = MAX(align, npages - 1);

	/*
	 * Get the dvma space and map in the first window.
	 */
	dvma_pg = iommu_get_dvma_pages(iommu_p, npages + red_zone, lo, hi,
		align, count_max + 1,
		(dmareq->dmar_fp == DDI_DMA_SLEEP) ? 1 : 0);
	DEBUG2(DBG_DMA_MAP, dip, "get_dvma_pages: dvma_pg=%x index=%x\n",
		dvma_pg, dvma_pg - iommu_p->dvma_base_pg);
	if (dvma_pg == 0)
		goto noresource;

	dvma_pg_index = dvma_pg - iommu_p->dvma_base_pg;
	if (dvma_pg_index < (pci_dvma_page_cache_entries * clustsz))
		cmn_err(CE_PANIC, "bad dvma_pg %x\n", (int)dvma_pg);

	if ((npages >= pci_context_minpages) && pci_use_contexts) {
		dvma_context_t ctx;
		ctx = pci_iommu_get_dvma_context(iommu_p, dvma_pg_index);
		if (ctx) {
			np->tte |= pci_iommu_ctx2tte(ctx);
			mp->dmai_flags |= DMAI_FLAGS_CONTEXT;
		}
	}

	/* Program the IOMMU TSB */
	mp->dmai_mapping = IOMMU_PTOB(dvma_pg) + offset;
	iommu_map_pages(iommu_p, mp, dvma_pg, npages, 0);
done:
	mp->dmai_flags |= DMAI_FLAGS_INUSE;
	DEBUG1(DBG_DMA_MAP, dip, "dma_map_impl ret=%s\n",
		ret == DDI_DMA_MAPPED ? "mapped" : "partial mapped");
	return (ret); /* DDI_DMA_MAPPED or DDI_DMA_PARTIAL_MAP */

noresource:
	if (dmareq->dmar_fp != DDI_DMA_DONTWAIT) {
		DEBUG0(DBG_DMA_MAP, dip, "dvma_pg 0 - set callback\n");
		ddi_set_callback(dmareq->dmar_fp, dmareq->dmar_arg,
			&iommu_p->iommu_dvma_call_list_id);
	}
	DEBUG0(DBG_DMA_MAP, dip, "dvma_pg 0 - DDI_DMA_NORESOURCES\n");
	ret = DDI_DMA_NORESOURCES;

cleanup:
	if (handlep && !bindcall) {
		ASSERT(mp);
		pci_free_mp(mp);
	}
	return (ret);
}


/*
 * check_dma_limits
 *
 * This routine is called from the dma map routine to sanity check the
 * limit structure and determine if the limit structure implies a
 * partial mapping.  If the size of the mapping exceeds the limits
 * and partial mapping is permitted, the partial size is returned
 * through the sizep parameter.
 *
 * used by: pci_dma_map()
 *
 * return value:
 *
 *	0			- on success
 *	DDI_DMA_NOMAPPING 	- if limits are bogus
 *	DDI_DMA_TOOBIG		- if limits are too small for transfer
 *				  an partial mapping not permitted
 */
static int
check_dma_limits(pci_t *pci_p, struct ddi_dma_req *dmareq, size_t *sizep)
{
	iommu_t *iommu_p = pci_p->pci_iommu_p;
	uint_t low, high;

	/* make sure the req hi-lo and iommu hi-lo overlap */
	low = MAX(dmareq->dmar_limits->dlim_addr_lo, iommu_p->iommu_dvma_base);
	high = MIN(dmareq->dmar_limits->dlim_addr_hi, iommu_p->iommu_dvma_end);
	if (high <= low) {
		DEBUG0(DBG_DMA_MAP, pci_p->pci_dip,
			"limits exclude dvma range (DDI_DMA_NOMAPPING)\n");
		return (DDI_DMA_NOMAPPING);
	}

	/* trim down the size to a partial window size */
	*sizep = MIN(*sizep - 1, high - low);
	*sizep = MIN(*sizep, dmareq->dmar_limits->dlim_cntr_max) + 1;

	return (0);
}


/*
 * check_dma_attr
 *
 * This routine is called from the alloc handle entry point to sanity check the
 * dma attribute structure.
 *
 * use by; pci_dma_allochdl()
 *
 * return value:
 *
 *	DDI_SUCCESS		- on success
 *	DDI_DMA_BADATTR		- attribute has invalid version number
 *				  or address limits exclude dvma space
 */
int
check_dma_attr(pci_t *pci_p, ddi_dma_impl_t *mp, ddi_dma_attr_t *attrp)
{
	iommu_t *iommu_p = pci_p->pci_iommu_p;
	uint64_t hi, lo;
	uint64_t attr_hi = attrp->dma_attr_addr_hi;
	uint64_t attr_lo = attrp->dma_attr_addr_lo;
	uint64_t attr_count_max = attrp->dma_attr_count_max;

	DEBUG3(DBG_DMA_ALLOCH, pci_p->pci_dip, "attrp=%x attr_hi=%08x.%08x ",
		attrp, (uint_t)(attr_hi >> 32), (uint_t)attr_hi);
	DEBUG4(DBG_DMA_ALLOCH, pci_p->pci_dip,
		"attr_lo=%08x.%08x cnt_max=%08x.%08x\n",
		(uint_t)(attr_lo >> 32), (uint_t)attr_lo,
		(uint_t)(attr_count_max >> 32), (uint_t)attr_count_max);

	if (attrp->dma_attr_version != DMA_ATTR_V0)
		return (DDI_DMA_BADATTR);

	if (attrp->dma_attr_flags & DDI_DMA_FORCE_PHYSICAL) { /* BYPASS */

		DEBUG0(DBG_DMA_ALLOCH, pci_p->pci_dip, "bypass mode\n");
		mp->dmai_flags |= DMAI_FLAGS_BYPASS;
		lo = (uint64_t)iommu_p->iommu_dma_bypass_base;
		hi = (uint64_t)iommu_p->iommu_dma_bypass_end;

	} else { /* IOMMU_XLATE */

		lo = (uint64_t)iommu_p->iommu_dvma_base;
		hi = (uint64_t)iommu_p->iommu_dvma_end & 0xffffffffull;

		/* record the realistic version of counter_max as our copy */
		if (mp->dmai_attr.dma_attr_seg) {
			mp->dmai_attr.dma_attr_count_max =
				MIN(attr_count_max, mp->dmai_attr.dma_attr_seg);
		}
		if (DMA_IS_NOLIMIT(attr_lo, attr_hi, attr_count_max)) {
			mp->dmai_flags |= DMP_NOLIMIT;
			return (DDI_SUCCESS);
		}
	}

	lo = MAX(attrp->dma_attr_addr_lo, lo);
	hi = MIN(attrp->dma_attr_addr_hi, hi);

	DEBUG4(DBG_DMA_ALLOCH, pci_p->pci_dip, "hi=%08x.%08x, lo=%08x.%08x\n",
		(uint_t)(hi >> 32), (uint_t)hi, (uint_t)(lo >> 32), (uint_t)lo);

	if (hi <= lo) {
		DEBUG0(DBG_DMA_ALLOCH, pci_p->pci_dip,
			"limits exclude dvma range (DDI_DMA_BADATTR)\n");
		return (DDI_DMA_BADATTR);
	}

	mp->dmai_attr.dma_attr_addr_lo = lo;
	mp->dmai_attr.dma_attr_addr_hi = hi;

	return (DDI_SUCCESS);
}


/*
 * check_dma_size
 *
 * This routine is called from the bind handle entry point to determine if the
 * dma request needs to be resized based upon the attributes for the handle.
 *
 * used by: pci_dma_bindhdl()
 *
 */
static void
check_dma_size(ddi_dma_impl_t *mp, size_t *sizep)
{
	size_t sz = *sizep - 1;

	/* trim down the size to a partial window size */
	sz = MIN(sz,
	    mp->dmai_attr.dma_attr_addr_hi - mp->dmai_attr.dma_attr_addr_lo);

	if (!(mp->dmai_flags & DMAI_FLAGS_BYPASS))
		sz = MIN(sz, mp->dmai_attr.dma_attr_count_max);

	*sizep = sz + 1;
}


/*
 * check_dma_mode
 *
 * This routine is called from the dma map routine when the mapping
 * object is presented as a virtual address/size pair.  This routine
 * examines the physical pages in the virtual address range to determine
 * if we have intra bus DMA or ordinary DVMA and make sure we disallow
 * mixed mode DMAs. dma_type is set to PEER_TO_PEER if this is an
 * intra-pbm (peer-to-peer) transfer.
 *
 * This routine also fills and array of the page frame numbers for the
 * virtual address range.
 *
 * used by: pci_dma_map_impl()
 *
 * return value:
 *	DDI_SUCCESS
 *	DDI_DMA_NOMAPPING
 *
 */
static int
check_dma_mode(pci_t *pci_p, caddr_t vaddr, struct ddi_dma_req *dmareq,
	size_t npages, iommu_dma_t *dma_type, pci_dmai_nexus_private_t *np)
{
	iopfn_t pfn, base_pfn, last_pfn, pfn_adj = 0;
	uint_t i;
	enum { INSIDE, OUTSIDE } first_page_mode;

	struct hat *as_hat;
	/* get as_hat */ {
		struct as *as;
		as = dmareq->dmar_object.dmao_obj.virt_obj.v_as;
		if (as == (struct as *)0)
			as = &kas;
		as_hat = as->a_hat;
	}

	/* set base_pfn and last_pfn */ {
		pbm_t *pbm_p = pci_p->pci_pbm_p;
		base_pfn = pbm_p->pbm_base_pfn;
		last_pfn = pbm_p->pbm_last_pfn;
		DEBUG2(DBG_CHK_MOD, pci_p->pci_dip, "base_pfn=%x last_pfn=%x\n",
			base_pfn, last_pfn);
	}


	/* set up mapping for the first page */
	vaddr = (caddr_t)((uintptr_t)vaddr & ~IOMMU_PAGE_OFFSET);
	pfn = hat_getpfnum(as_hat, vaddr);
	if (pfn == PFN_INVALID)
		goto err_badpfn;

	DEBUG2(DBG_CHK_MOD, pci_p->pci_dip,
		"PCI_SET_MP_PFN: pg0 vaddr=%x pfn=%x\n", vaddr, pfn);

	/* XXX does not handle discontiguous IO and MEM space */
#define	PFN_TYPE(pfn) ((pfn < base_pfn) || (pfn > last_pfn) ? OUTSIDE : INSIDE)
	/* get the dma_mode for the 1st page */
	first_page_mode = PFN_TYPE(pfn);
	if (first_page_mode == INSIDE) {
		*dma_type = PCI_PEER_TO_PEER;
		pfn_adj = base_pfn; /* adjustment for peer-to-peer DMA */
		DEBUG1(DBG_CHK_MOD, pci_p->pci_dip, "pfn_adj=%x\n", pfn_adj);
		pfn -= pfn_adj;
	}

	if (np) {
		PCI_SET_MP_PFN(np, 0, pfn);
		DEBUG3(DBG_CHK_MOD, pci_p->pci_dip,
			"PCI_SET_MP_PFN: np=%x page0 pfn=%08x.%08x\n",
			np, (uint_t)(pfn >> 32), (uint_t)pfn);
	}

	vaddr += IOMMU_PAGE_SIZE;
	for (i = 1; i < npages; i++, vaddr += IOMMU_PAGE_SIZE) {
		pfn = hat_getpfnum(as_hat, vaddr);
		if (pfn == PFN_INVALID)
			goto err_badpfn;

		DEBUG3(DBG_CHK_MOD, pci_p->pci_dip, "pg[%x] vaddr=%x pfn=%x\n",
			i, vaddr, pfn);

		if (PFN_TYPE(pfn) != first_page_mode)
			goto err_mixmode;

		pfn -= pfn_adj;
		if (np) {
			PCI_SET_MP_PFN(np, i, pfn);
			DEBUG4(DBG_CHK_MOD, pci_p->pci_dip,
			    "PCI_SET_MP_PFN: np=%x page_no=%x pfn=%08x.%08x\n",
			    np, i, (uint_t)(pfn >> 32), (uint_t)pfn);
		}
	}
	return (DDI_SUCCESS);

err_badpfn:
	cmn_err(CE_WARN, "%s%d: can't get page frames for vaddr %x ",
		ddi_driver_name(pci_p->pci_dip),
		ddi_get_instance(pci_p->pci_dip), (int)vaddr);
	return (DDI_DMA_NOMAPPING);

err_mixmode:
	DEBUG0(DBG_CHK_MOD, pci_p->pci_dip, "mix mode error\n");
	return (DDI_DMA_NOMAPPING);
}

kmutex_t dvma_active_list_mutex;

#define	DVMA_ALLOC_REC_MAX 512
#define	DVMA_FREE_REC_MAX 512

static uint_t dvma_alloc_rec_index = 0;
static uint_t dvma_free_rec_index = 0;
static uint_t dvma_active_count = 0;

static struct dvma_rec *dvma_alloc_rec = NULL;
static struct dvma_rec *dvma_free_rec = NULL;
static struct dvma_rec *dvma_active_list = NULL;

/*
 * on=1 :	we are in dvma debug mode
 * off=1:	we are about to turn off dvma debug
 */
static uint_t pci_dvma_debug_off = 0;

static void
pci_dvma_debug_init()
{
	ASSERT(MUTEX_HELD(&dvma_active_list_mutex));
	cmn_err(CE_NOTE, "PCI DVMA stat ON");

	dvma_alloc_rec = kmem_alloc(
		sizeof (struct dvma_rec) * DVMA_ALLOC_REC_MAX, KM_SLEEP);
	dvma_free_rec = kmem_alloc(
		sizeof (struct dvma_rec) * DVMA_FREE_REC_MAX, KM_SLEEP);

	dvma_active_list = NULL;
	dvma_alloc_rec_index = 0;
	dvma_free_rec_index = 0;
	dvma_active_count = 0;
}

static void
pci_dvma_debug_fini()
{
	register struct dvma_rec *prev, *ptr;

	ASSERT(MUTEX_HELD(&dvma_active_list_mutex));
	cmn_err(CE_NOTE, "PCI DVMA stat OFF");

	kmem_free(dvma_alloc_rec,
		sizeof (struct dvma_rec) * DVMA_ALLOC_REC_MAX);
	kmem_free(dvma_free_rec,
		sizeof (struct dvma_rec) * DVMA_FREE_REC_MAX);
	dvma_alloc_rec = dvma_free_rec = NULL;

	prev = dvma_active_list;
	if (!prev)
		return;
	for (ptr = prev->next; ptr; prev = ptr, ptr = ptr->next)
		kmem_free(prev, sizeof (struct dvma_rec));
	kmem_free(prev, sizeof (struct dvma_rec));

	dvma_active_list = NULL;
	dvma_alloc_rec_index = 0;
	dvma_free_rec_index = 0;
	dvma_active_count = 0;

	pci_dvma_debug_off = 0;
	pci_dvma_debug_on = 0;
}

void
pci_dvma_alloc_debug(char *address, uint_t len, ddi_dma_impl_t *mp)
{
	struct dvma_rec *ptr;
	mutex_enter(&dvma_active_list_mutex);

	if (!dvma_alloc_rec)
		pci_dvma_debug_init();
	if (pci_dvma_debug_off) {
		pci_dvma_debug_fini();
		goto done;
	}

	dvma_alloc_rec[dvma_alloc_rec_index].dvma_addr = address;
	dvma_alloc_rec[dvma_alloc_rec_index].len = len;
	dvma_alloc_rec[dvma_alloc_rec_index].mp = mp;
	if (++dvma_alloc_rec_index == DVMA_ALLOC_REC_MAX)
		dvma_alloc_rec_index = 0;

	ptr = kmem_alloc(sizeof (struct dvma_rec), KM_SLEEP);
	ptr->dvma_addr = address;
	ptr->len = len;
	ptr->mp = mp;

	ptr->next = dvma_active_list;
	dvma_active_list = ptr;
	dvma_active_count++;
done:
	mutex_exit(&dvma_active_list_mutex);
}

void
pci_dvma_free_debug(char *address, uint_t len, ddi_dma_impl_t *mp)
{
	struct dvma_rec *ptr, *ptr_save;
	mutex_enter(&dvma_active_list_mutex);

	if (!dvma_alloc_rec)
		pci_dvma_debug_init();
	if (pci_dvma_debug_off) {
		pci_dvma_debug_fini();
		goto done;
	}

	dvma_free_rec[dvma_free_rec_index].dvma_addr = address;
	dvma_free_rec[dvma_free_rec_index].len = len;
	dvma_free_rec[dvma_free_rec_index].mp = mp;
	if (++dvma_free_rec_index == DVMA_FREE_REC_MAX)
		dvma_free_rec_index = 0;

	ptr_save = dvma_active_list;
	for (ptr = dvma_active_list; ptr; ptr = ptr->next) {
		if ((ptr->dvma_addr == address) && (ptr->len = len))
			break;
		ptr_save = ptr;
	}
	if (!ptr) {
		cmn_err(CE_WARN, "bad dvma free addr=%lx len=%x",
			(long)address, len);
		goto done;
	}
	if (ptr == dvma_active_list)
		dvma_active_list = ptr->next;
	else
		ptr_save->next = ptr->next;
	kmem_free(ptr, sizeof (struct dvma_rec));
	dvma_active_count--;
done:
	mutex_exit(&dvma_active_list_mutex);
}

#ifdef DEBUG
void
dump_dma_handle(uint64_t flag, dev_info_t *dip, ddi_dma_impl_t *hp)
{
	DEBUG4(flag, dip,
		"dma handle: inuse=%x mapping=%x size=%x ndvmapages=%x\n",
		hp->dmai_inuse, hp->dmai_mapping, hp->dmai_size,
		hp->dmai_ndvmapages);
	DEBUG3(flag|DBG_CONT, dip, "\t\toffset=%x winsize=%x win=%x\n",
		hp->dmai_offset, hp->dmai_winsize, hp->dmai_nwin);
}
#endif
