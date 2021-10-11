/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_FDVMA_H
#define	_SYS_PCI_FDVMA_H

#pragma ident	"@(#)pci_fdvma.h	1.2	98/07/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int pci_fdvma_reserve(dev_info_t *dip, dev_info_t *rdip, pci_t *pci_p,
	struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep);
extern int pci_fdvma_release(dev_info_t *dip, pci_t *pci_p, ddi_dma_impl_t *mp);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_FDVMA_H */
