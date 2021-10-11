/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_DMA_H
#define	_SYS_PCI_DMA_H

#pragma ident	"@(#)pci_dma.h	1.6	99/06/30 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	MAKE_DMA_COOKIE(cp, address, size)	\
	{					\
		(cp)->dmac_notused = 0;		\
		(cp)->dmac_type = 0;		\
		(cp)->dmac_laddress = (address);	\
		(cp)->dmac_size = (size);	\
	}

#define	HAS_REDZONE(mp)	(((mp)->dmai_rflags & DDI_DMA_REDZONE) ? 1 : 0)

/*
 * flags for overloading dmai_inuse field of the dma request
 * structure:
 */
#define	dmai_flags		dmai_inuse
#define	DMAI_FLAGS_INUSE	0x1
#define	DMAI_FLAGS_BYPASS	0x2
#define	DMAI_FLAGS_PEER_TO_PEER	0x4
#define	DMAI_FLAGS_CONTEXT	0x8
#define	DMAI_FLAGS_FASTTRACK	0x10
/* XXX 0x1000 is reserved for DMP_NOLIMIT */

/*
 * contains the nexus dma implementation detail. It is allocated
 * with each dma handle and freed with dma handle. All operations
 * are done through corresponding functions in pci_dma.c.
 * Direct access to structure members is strongly discouraged.
 *
 * Note that the fast dvma routines cannot be mixed with
 * the traditional dvma routines - the fast dvma routines have
 * their own use for the dmai_nexus_private pointer.
 */
typedef	uint64_t iopfn_t;
typedef struct pci_dmai_nexus_private {
	size_t lists_sz;
	iopfn_t pfn0; /* should be a union with cookie fields */

	uint64_t tte; /* template tte: all bits except pfn */
	size_t npages;
	iopfn_t *page_list;

	size_t ncookies;
	ddi_dma_cookie_t *cookie_list;	/* bypass cookies */
} pci_dmai_nexus_private_t;

/* dvma debug records */
struct dvma_rec {
	char *dvma_addr;
	uint_t len;
	ddi_dma_impl_t *mp;
	struct dvma_rec *next;
};

extern int pci_alloc_mp(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t *handlep,  int (*waitfp)(caddr_t), caddr_t arg,
	pci_t *pci_p);
extern void pci_free_mp(ddi_dma_impl_t *mp);
extern int pci_alloc_np_pages(pci_dmai_nexus_private_t *np, size_t npages,
	int alloc_cookies);
extern void pci_free_np_pages(pci_dmai_nexus_private_t *np, int purge);

#if defined(DEBUG)

extern size_t	pci_get_np_ncookies(pci_dmai_nexus_private_t *np);
extern void	pci_set_np_ncookies(pci_dmai_nexus_private_t *np, size_t nc);
extern iopfn_t	pci_get_mp_pfn(pci_dmai_nexus_private_t *np, size_t page_no);
extern void	pci_set_mp_pfn(pci_dmai_nexus_private_t *np, size_t page_no,
	iopfn_t pfn);

#define	PCI_GET_NP_NCOOKIES(np)			pci_get_np_ncookies(np)
#define	PCI_SET_NP_NCOOKIES(np, nc)		pci_set_np_ncookies(np, nc)
#define	PCI_GET_MP_PFN(np, page_no)		pci_get_mp_pfn(np, page_no)
#define	PCI_SET_MP_PFN(np, page_no, pfn)	pci_set_mp_pfn(np, page_no, pfn)

#else

#define	PCI_GET_NP_NCOOKIES(np)		((np)->ncookies)
#define	PCI_SET_NP_NCOOKIES(np, nc)	((np)->ncookies = (nc))
#define	PCI_GET_MP_PFN(np, page_no) \
		((np)->lists_sz ? (np)->page_list[page_no] : (np)->pfn0)
#define	PCI_SET_MP_PFN(np, page_no, pfn) { \
		if ((np)->lists_sz) \
			(np)->page_list[page_no] = (pfn); \
		else \
			(np)->pfn0 = (pfn); \
		}

#endif

extern int pci_dma_map_impl(dev_info_t *dip, dev_info_t *rdip,
	struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep, int bindcall);
extern int check_dma_attr(pci_t *pci_p, ddi_dma_impl_t *mp,
	ddi_dma_attr_t *attrp);
extern void pci_dvma_alloc_debug(char *address, uint_t len, ddi_dma_impl_t *mp);
extern void pci_dvma_free_debug(char *address, uint_t len, ddi_dma_impl_t *mp);
extern int pci_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request, off_t *offp,
    size_t *lenp, caddr_t *objp, uint_t cache_flags);

#if defined(DEBUG)
extern void dump_dma_handle(uint64_t flag, dev_info_t *dip, ddi_dma_impl_t *hp);
#else
#define	dump_dma_handle(flag, dip, hp)
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_DMA_H */
