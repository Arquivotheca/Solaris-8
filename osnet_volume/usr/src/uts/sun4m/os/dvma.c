/*
 * Copyright (c) 1990-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dvma.c	1.17	98/10/25 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/cpu.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/pte.h>
#include <sys/machsystm.h>
#include <sys/mmu.h>
#include <sys/iommu.h>
#include <sys/debug.h>
#include <vm/hat_srmmu.h>

extern int tsunami_control_store_bug;
extern int swift_tlb_flush_bug;

extern void pac_flushall(void);

#define	HD	((ddi_dma_impl_t *)h)->dmai_rdip

unsigned long
dvma_pagesize(dev_info_t *dip)
{
	auto unsigned long dvmapgsz;

	(void) ddi_ctlops(dip, dip, DDI_CTLOPS_DVMAPAGESIZE,
	    NULL, (void *) &dvmapgsz);
	return (dvmapgsz);
}

int
dvma_reserve(dev_info_t *dip,  ddi_dma_lim_t *limp, u_int pages,
    ddi_dma_handle_t *handlep)
{
	auto ddi_dma_lim_t dma_lim;
	auto ddi_dma_impl_t implhdl;
	struct ddi_dma_req dmareq;
	ddi_dma_handle_t reqhdl;
	ddi_dma_impl_t *mp;
	int ret;

	if (limp == (ddi_dma_lim_t *)0) {
		return (DDI_DMA_BADLIMITS);
	} else {
		dma_lim = *limp;
	}
	dmareq.dmar_limits = &dma_lim;
	dmareq.dmar_object.dmao_size = pages;
	/*
	 * pass in a dummy handle. This avoids the problem when
	 * somebody is dereferencing the handle before checking
	 * the operation. This can be avoided once we separate
	 * handle allocation and actual operation.
	 */
	bzero((caddr_t)&implhdl, sizeof (ddi_dma_impl_t));
	reqhdl = (ddi_dma_handle_t)&implhdl;

	ret = ddi_dma_mctl(dip, dip, reqhdl, DDI_DMA_RESERVE, (off_t *)&dmareq,
	    0, (caddr_t *)handlep, 0);

	if (ret == DDI_SUCCESS) {
		mp = (ddi_dma_impl_t *)(*handlep);
		if (!(mp->dmai_rflags & DMP_BYPASSNEXUS)) {
			u_int np = mp->dmai_ndvmapages;

			mp->dmai_mapping = (u_long)kmem_alloc(
				sizeof (ddi_dma_lim_t), KM_SLEEP);
			bcopy((char *)&dma_lim, (char *)mp->dmai_mapping,
			    sizeof (ddi_dma_lim_t));
			mp->dmai_minfo = kmem_alloc(
				np * sizeof (ddi_dma_handle_t), KM_SLEEP);
		}
	}
	return (ret);
}

void
dvma_release(ddi_dma_handle_t h)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)h;

	if (!(mp->dmai_rflags & DMP_BYPASSNEXUS)) {
		u_int np = mp->dmai_ndvmapages;

		kmem_free((void *)mp->dmai_mapping, sizeof (ddi_dma_lim_t));
		kmem_free(mp->dmai_minfo, np * sizeof (ddi_dma_handle_t));
	}
	(void) ddi_dma_mctl(HD, HD, h, DDI_DMA_RELEASE, 0, 0, 0, 0);

}

void
dvma_kaddr_load(ddi_dma_handle_t h, caddr_t a, u_int len, u_int index,
	ddi_dma_cookie_t *cp)
{
	register ddi_dma_impl_t *mp = (ddi_dma_impl_t *)h;
	iommu_pte_t *piopte;
	u_long ioaddr, off, addr;
	u_int srmmupte, dvma_pfn, pfn;
	int npages;
	struct page *pp;
	extern void srmmu_vacsync(u_int);

	if (mp->dmai_rflags & DMP_BYPASSNEXUS) {
		addr = (u_long)a;
		dvma_pfn =  ((u_int)mp->dmai_mapping) + index;
		piopte = &ioptes[dvma_pfn];
		ASSERT(piopte != NULL);

		off = addr & MMU_PAGEOFFSET;
		npages = mmu_btopr(len + off);
		ioaddr = (IOMMU_DVMA_BASE + iommu_ptob(dvma_pfn));
		cp->dmac_address = ioaddr | off;
		cp->dmac_size = len;

		if (npages == 1) {
			srmmupte = mmu_probe((caddr_t)addr, NULL);
			if (srmmupte == 0) {
				/*
				 * Grab the hat mutex and re-check the address.
				 * If it's valid, we got here because of a pte
				 * that was temporarily invalidated by the
				 * ptesync code.
				 */
				mmu_getpte((caddr_t)addr,
				    (struct pte *)&srmmupte);
				if (!pte_valid((struct pte *)&srmmupte)) {
					cmn_err(CE_PANIC, "dvma: no mapping");
				}
			}
			if (vac) {
				pfn = MAKE_PFNUM((struct pte *)&srmmupte);
				pp = page_numtopp_nolock(pfn);
				if (pp) {
					ohat_mlist_enter(pp);
					srmmu_vacsync(pfn);
					ohat_mlist_exit(pp);
				}
			}
			*((u_int *)piopte) = srmmupte;
			/*
			 * this assumes that ioaddr is IOMMU page aligned
			 */
			ASSERT(ioaddr == (ioaddr & IOMMU_FLUSH_MSK));

			if (!tsunami_control_store_bug) {
				iommu_addr_flush((int)ioaddr);
			}

		} else {
			while (npages > 0) {
				srmmupte = mmu_probe((caddr_t)addr, NULL);
				if (srmmupte == 0) {
					mmu_getpte((caddr_t)addr,
					    (struct pte *)&srmmupte);
					if (!pte_valid((struct pte *)&srmmupte))
						cmn_err(CE_PANIC,
						    "dvma: no mapping");
				}
				if (vac) {
					pfn =
					    MAKE_PFNUM((struct pte *)&srmmupte);
					pp = page_numtopp_nolock(pfn);
					if (pp) {
						ohat_mlist_enter(pp);
						srmmu_vacsync(pfn);
						ohat_mlist_exit(pp);
					}
				}
				*((u_int *)piopte) = srmmupte;
				ASSERT(ioaddr == (ioaddr & IOMMU_FLUSH_MSK));

				if (!tsunami_control_store_bug) {
					iommu_addr_flush((int)ioaddr);
				}

				piopte++;
				npages--;
				addr += MMU_PAGESIZE;
				ioaddr += IOMMU_PAGE_SIZE;
			}
		}

		/*
		 * Tsunami and Swift processors need to flush
		 * entire TLB
		 */
		if (swift_tlb_flush_bug || tsunami_control_store_bug) {
			mmu_flushall();
		}

	} else  {
		ddi_dma_handle_t handle;
		ddi_dma_lim_t *limp;

		limp = (ddi_dma_lim_t *)mp->dmai_mapping;
		(void) ddi_dma_addr_setup(HD, NULL, a, len, DDI_DMA_RDWR,
			DDI_DMA_SLEEP, NULL, limp, &handle);
		((ddi_dma_handle_t *)mp->dmai_minfo)[index] = handle;
		(void) ddi_dma_htoc(handle, 0, cp);
	}
}

/*
 * For non-coherent PAC caches (small4m), we always flush reads.
 */
#define	IOMMU_NC_FLUSH_READ(c)						\
{									\
	if ((c & (CACHE_PAC|CACHE_IOCOHERENT)) == CACHE_PAC) {		\
		pac_flushall();						\
	}								\
}

void
dvma_unload(ddi_dma_handle_t h, u_int objindex, u_int type)
{
	register ddi_dma_impl_t *mp = (ddi_dma_impl_t *)h;

	if (mp->dmai_rflags & DMP_BYPASSNEXUS) {
		if (type == DDI_DMA_SYNC_FORKERNEL) {
			flush_writebuffers();
			IOMMU_NC_FLUSH_READ(cache);
		}
	} else {
		ddi_dma_handle_t handle;

		handle = ((ddi_dma_handle_t *)mp->dmai_minfo)[objindex];
		(void) ddi_dma_free(handle);
	}
}

void
dvma_sync(ddi_dma_handle_t h, u_int objindex, u_int type)
{
	register ddi_dma_impl_t *mp = (ddi_dma_impl_t *)h;

	if (type == DDI_DMA_SYNC_FORDEV)
		return;

	if (mp->dmai_rflags & DMP_BYPASSNEXUS) {
		flush_writebuffers();
		IOMMU_NC_FLUSH_READ(cache);
	} else {
		ddi_dma_handle_t handle;

		handle = ((ddi_dma_handle_t *)mp->dmai_minfo)[objindex];
		(void) ddi_dma_sync(handle, 0, 0, type);
	}
}
