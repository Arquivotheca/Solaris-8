/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_TYPES_H
#define	_SYS_PCI_TYPES_H

#pragma ident	"@(#)pci_types.h	1.5	99/07/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	HI32(x) ((uint32_t)((x) >> 32))
#define	LO32(x) ((uint32_t)(x))

typedef struct pci pci_t;

/*
 * external global function prototypes
 */
extern int pf_is_memory(pfn_t);
extern void do_shutdown();
extern void power_down();
extern void set_intr_mapping_reg(int, uint64_t *, int);
extern int impl_ddi_merge_child(dev_info_t *child);
extern uint_t intr_add_cpu(void (*func)(void *, int, uint_t), void *, int, int);
extern void intr_rem_cpu(int);

/*
 * external global iommu resources
 */
extern int iommu_tsb_alloc_size[];
extern caddr_t iommu_tsb_vaddr[];
extern uchar_t iommu_tsb_spare[];

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_TYPES_H */
