/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PCI_IOMMU_H
#define	_SYS_PCI_IOMMU_H

#pragma ident	"@(#)pci_iommu.h	1.28	99/07/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/vmem.h>

typedef uint64_t dvma_addr_t;
typedef uint64_t dma_bypass_addr_t;
typedef uint64_t dma_peer_addr_t;
typedef uint16_t dvma_context_t;
typedef uint64_t window_t;

/*
 * The following typedef's represents the types for DMA transactions
 * and corresponding DMA addresses supported by psycho/schizo.
 */
typedef enum { IOMMU_XLATE, IOMMU_BYPASS, PCI_PEER_TO_PEER } iommu_dma_t;

/*
 * The following macros define the iommu page size and related operations.
 */
#define	IOMMU_PAGE_SHIFT	13
#define	IOMMU_PAGE_SIZE		(1 << IOMMU_PAGE_SHIFT)
#define	IOMMU_PAGE_MASK		~(IOMMU_PAGE_SIZE - 1)
#define	IOMMU_PAGE_OFFSET	(IOMMU_PAGE_SIZE - 1)
#define	IOMMU_PTOB(x)		((x) << IOMMU_PAGE_SHIFT)
#define	IOMMU_BTOP(x)		(((ulong_t)(x)) >> IOMMU_PAGE_SHIFT)
#define	IOMMU_BTOPR(x)	((((x) + IOMMU_PAGE_OFFSET) >> IOMMU_PAGE_SHIFT))

/*
 * control register decoding
 */
/* tsb size: 0=1k 1=2k 2=4k 3=8k 4=16k 5=32k 6=64k 7=128k */
#define	IOMMU_CTL_TO_TSBSIZE(ctl)	((ctl) >> 16)
#define	IOMMU_TSBSIZE_TO_TSBENTRIES(s)	(1 << s << 10)

/*
 * boiler plate for tte (everything except the pfn)
 */
#define	MAKE_TTE_TEMPLATE(pfn, mp) (COMMON_IOMMU_TTE_V | \
	(pf_is_memory(pfn) ? COMMON_IOMMU_TTE_C : 0) | \
	((mp->dmai_rflags & DDI_DMA_READ) ? COMMON_IOMMU_TTE_W : 0) | \
	((mp->dmai_rflags & DDI_DMA_CONSISTENT) ? 0 : COMMON_IOMMU_TTE_S))
#define	TTE_IS_INVALID(tte)	(((tte) & COMMON_IOMMU_TTE_V) == 0x0ull)

/* defines for iommu spare handling */
#define	IOMMU_TSB_ISASPARE	0x01
#define	IOMMU_TSB_ISUNAVAILABLE	0x02

/*
 * The following macros define the address ranges supported for DVMA
 * and iommu bypass transfers.
 */
#define	COMMON_IOMMU_BYPASS_BASE	0xFFFC000000000000ull
#define	COMMON_IOMMU_BYPASS_END		0xFFFC00FFFFFFFFFFull

/*
 * For iommu bypass addresses, bit 43 specifies cacheability.
 */
#define	COMMON_IOMMU_BYPASS_NONCACHE	0x0000080000000000ull

/*
 * Generic psycho iommu definitions and types:
 */
#define	COMMON_IOMMU_TLB_SIZE	16

/*
 * The following macros are for loading and unloading iotte
 * entries.
 */
#define	COMMON_IOMMU_TTE_SIZE		8
#define	COMMON_IOMMU_TTE_V		0x8000000000000000ull
#define	COMMON_IOMMU_TTE_S		0x1000000000000000ull
#define	COMMON_IOMMU_TTE_C		0x0000000000000010ull
#define	COMMON_IOMMU_TTE_W		0x0000000000000002ull
#define	COMMON_IOMMU_INVALID_TTE	0x0000000000000000ull

/*
 * iommu block soft state structure:
 *
 * Each pci node may share an iommu block structure with its peer
 * node of have its own private iommu block structure.
 */
typedef struct iommu iommu_t;
struct iommu {

	pci_t *iommu_pci_p;	/* link back to pci soft state */

	volatile uint64_t *iommu_ctrl_reg;
	volatile uint64_t *iommu_tsb_base_addr_reg;
	volatile uint64_t *iommu_flush_page_reg;

	/* schizo only registers */
	volatile uint64_t *iommu_flush_ctx_reg;

	/*
	 * diagnostic access registers:
	 */
	volatile uint64_t *iommu_tlb_tag_diag_acc;
	volatile uint64_t *iommu_tlb_data_diag_acc;

	/*
	 * virtual and physical addresses and size of the iommu tsb:
	 */
	uint64_t *iommu_tsb_vaddr;
	uint64_t iommu_tsb_paddr;
	uint_t iommu_tsb_entries;
	uint_t iommu_tsb_size;
	uint_t iommu_contexts;
	uint_t iommu_spare_slot;

	/*
	 * address ranges of dvma space:
	 */
	dvma_addr_t iommu_dvma_base;
	dvma_addr_t iommu_dvma_end;
	dvma_addr_t dvma_base_pg;	/* = IOMMU_BTOP(iommu_dvma_base) */
	dvma_addr_t dvma_end_pg;	/* = IOMMU_BTOP(iommu_dvma_end) */

	/*
	 * address ranges of dma bypass space:
	 */
	dma_bypass_addr_t iommu_dma_bypass_base;
	dma_bypass_addr_t iommu_dma_bypass_end;

	/*
	 * virtual memory map and callback id for dvma space:
	 */
	vmem_t *iommu_dvma_map;
	uintptr_t iommu_dvma_call_list_id;
	char iommu_dvma_map_name[64];

	/*
	 * fields for fast dvma interfaces:
	 */
	ulong_t iommu_dvma_reserve;

	/*
	 * dvma fast track page cache byte map
	 */
	uint8_t *iommu_dvma_cache_locks;
	uint_t iommu_dvma_addr_scan_start;

	/*
	 * dvma context bitmap
	 */
	uint64_t *iommu_ctx_bitmap;
};

#define	IOMMU_PAGE_FLUSH(iommu_p, dvma_pg) \
	*(iommu_p)->iommu_flush_page_reg = IOMMU_PTOB(dvma_pg)
#define	IOMMU_UNLOAD_TTE(iommu_p, dvma_pg) { \
	size_t index = (dvma_pg) - (iommu_p)->dvma_base_pg; \
	(iommu_p)->iommu_tsb_vaddr[index] = COMMON_IOMMU_INVALID_TTE; \
}

#define	IOMMU_CONTEXT_BITS 12
#define	IOMMU_CTX_MASK		((1 << IOMMU_CONTEXT_BITS) - 1)
#define	IOMMU_TTE_CTX_SHIFT	47
#define	IOMMU_CTX2TTE(ctx) (((uint64_t)(ctx)) << IOMMU_TTE_CTX_SHIFT)
#define	IOMMU_TTE2CTX(tte) (((tte) >> IOMMU_TTE_CTX_SHIFT) & IOMMU_CTX_MASK)

#define	MP2NP(mp)	((pci_dmai_nexus_private_t *)(mp)->dmai_nexus_private)
#define	MP2CTX(mp)	IOMMU_TTE2CTX(MP2NP(mp)->tte)

/* dvma routines */
extern dvma_addr_t iommu_get_dvma_pages(iommu_t *iommu_p, size_t npages,
			dvma_addr_t addrlo, dvma_addr_t addrhi,
			dvma_addr_t align, uint32_t cntr_max, int cansleep);
extern void iommu_map_pages(iommu_t *iommu_p, ddi_dma_impl_t *mp,
			dvma_addr_t dvma_pg, size_t npages, size_t pfn_index);
extern void iommu_map_window(iommu_t *iommu_p,
			ddi_dma_impl_t *mp, window_t window);
extern void iommu_unmap_window(iommu_t *iommu_p, ddi_dma_impl_t *mp);
extern void iommu_free_dvma_pages(iommu_t *iommu_p, size_t npages,
			dvma_addr_t dvma_addr);
extern int iommu_create_bypass_cookies(iommu_t *iommu_p, dev_info_t *dip,
			ddi_dma_impl_t *mp, ddi_dma_cookie_t *cookiep);

/* iommu initialization routines */
extern void iommu_configure(iommu_t *iommu_p);
extern void iommu_create(pci_t *pci_p);
extern void iommu_destroy(pci_t *pci_p);
extern uintptr_t get_reg_base(pci_t *pci_p);
extern int iommu_tsb_alloc(iommu_t *iommu_p);

/* iommu take over routines */
extern void iommu_preserve_tsb(iommu_t *iommu_p);
extern void iommu_obp_to_kernel(iommu_t *iommu_p);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_IOMMU_H */
