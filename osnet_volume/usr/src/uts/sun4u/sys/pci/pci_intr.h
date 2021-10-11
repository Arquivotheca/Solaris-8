/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_INTR_H
#define	_SYS_PCI_INTR_H

#pragma ident	"@(#)pci_intr.h	1.7	99/04/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern void pci_initchild_intr(dev_info_t *child);
extern dev_info_t *get_my_childs_dip(dev_info_t *dip, dev_info_t *rdip);
extern uint_t iline_to_pil(dev_info_t *rdip);
extern int pci_add_intr_impl(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_info_t *intr_info);
extern int pci_remove_intr_impl(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_info_t *intr_info);
extern uint32_t pci_xlate_intr(dev_info_t *dip, dev_info_t *rdip,
    ib_t *ib_p, uint32_t intr);


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_INTR_H */
