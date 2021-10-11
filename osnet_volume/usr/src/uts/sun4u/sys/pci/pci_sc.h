/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_SC_H
#define	_SYS_PCI_SC_H

#pragma ident	"@(#)pci_sc.h	1.6	99/11/15 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * streaming cache (sc) block soft state structure:
 *
 * Each pci node contains has its own private sc block structure.
 */
typedef struct sc sc_t;
struct sc {

	pci_t *sc_pci_p;	/* link back to pci soft state */

	/*
	 * control registers (psycho and schizo):
	 */
	volatile uint64_t *sc_ctrl_reg;
	volatile uint64_t *sc_invl_reg;
	volatile uint64_t *sc_sync_reg;

	/*
	 * control registers (schizo only):
	 */
	volatile uint64_t *sc_ctx_invl_reg;
	volatile uint64_t *sc_ctx_match_reg;

	/*
	 * diagnostic access registers:
	 */
	volatile uint64_t *sc_data_diag_acc;
	volatile uint64_t *sc_tag_diag_acc;
	volatile uint64_t *sc_ltag_diag_acc;

	/*
	 * virtual and physical addresses of sync flag:
	 */
	caddr_t sc_sync_flag_base;
	volatile uint64_t *sc_sync_flag_vaddr;
	uint64_t sc_sync_flag_addr;

	kmutex_t sc_sync_mutex;		/* mutex for flush/sync register */
};

#define	PCI_SBUF_ENTRIES	16	/* number of i/o cache lines */
#define	PCI_SBUF_LINE_SIZE	64	/* size of i/o cache line */
#define	PCI_SYNC_FLAG_SIZE	64	/* size of i/o cache sync buffer */

#define	PCI_CACHE_LINE_SIZE	(PCI_SBUF_LINE_SIZE / 4)

extern void sc_create(pci_t *pci_p);
extern void sc_destroy(pci_t *pci_p);
extern void sc_configure(sc_t *sc_p);
extern void sc_flush(sc_t *sc_p, ddi_dma_impl_t *mp,
	dvma_context_t context, off_t offset, size_t length);

#define	STREAMING_BUF_FLUSH(pci_p, mp, context, offset, len) \
ASSERT(!pci_stream_buf_exists || pci_p->pci_sc_p); \
if (pci_stream_buf_exists && \
	!(mp->dmai_rflags & DDI_DMA_CONSISTENT)) { \
	sc_t *sc_p = pci_p->pci_sc_p; \
	sc_flush(sc_p, mp, context, offset, len); \
}

/*
 * The most significant bit (63) of each context match register.
 */
#define	SC_CMR_DIRTY_BIT	1
#define	SC_ENTRIES		16
#define	SC_ENT_SHIFT		(64 - SC_ENTRIES)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_SC_H */
