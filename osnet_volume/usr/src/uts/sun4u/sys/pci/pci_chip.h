/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_CHIP_H
#define	_SYS_PCI_CHIP_H

#pragma ident	"@(#)pci_chip.h	1.4	99/06/30 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int pci_obj_setup(pci_t *pci_p);
extern void pci_obj_destroy(pci_t *pci_p);
extern void pci_obj_resume(pci_t *pci_p);
extern void pci_obj_suspend(pci_t *pci_p);

/*
 * function prototypes for performance kstats.
 */
extern void psycho_add_kstats(pci_common_t *softsp, dev_info_t *dip);
extern void psycho_add_picN_kstats(dev_info_t *dip);
extern int psycho_counters_kstat_update(kstat_t *, int);

extern void pbm_disable_pci_errors(pbm_t *pbm_p);
extern int pbm_has_pass_1_cheerio(pci_t *pci_p);

extern uint64_t *ib_intr_map_reg_addr(ib_t *ib_p, ib_ino_t ino);
extern uint64_t *ib_clear_intr_reg_addr(ib_t *ib_p, ib_ino_t ino);

extern void pci_cb_setup(cb_t *cb_p);

extern uintptr_t pci_ecc_setup(ecc_t *ecc_p);
extern ushort_t pci_ecc_get_synd(uint64_t afsr);

extern uintptr_t pci_iommu_setup(iommu_t *iommu_p);
extern void pci_iommu_teardown(iommu_t *iommu_p);

extern dvma_context_t pci_iommu_get_dvma_context(iommu_t *iommu_p,
	dvma_addr_t dvma_pg_index);
extern void pci_iommu_free_dvma_context(iommu_t *iommu_p, dvma_context_t ctx);
extern uint64_t pci_iommu_ctx2tte(dvma_context_t ctx);
extern dvma_context_t pci_iommu_tte2ctx(uint64_t tte);

extern void pci_pbm_setup(pbm_t *pbm_p);

extern uintptr_t pci_ib_setup(ib_t *ib_p);

extern void pci_sc_setup(sc_t *sc_p);

extern int pci_get_numproxy(dev_info_t *dip);

extern int pci_fault(enum pci_fault_ops op, void *arg);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_CHIP_H */
