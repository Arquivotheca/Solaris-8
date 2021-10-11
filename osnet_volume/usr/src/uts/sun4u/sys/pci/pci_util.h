/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_UTIL_H
#define	_SYS_PCI_UTIL_H

#pragma ident	"@(#)pci_util.h	1.7	99/06/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int init_child(pci_t *pci_p, dev_info_t *child);
extern int report_dev(dev_info_t *dip);
extern int get_pci_properties(pci_t *pci_p, dev_info_t *dip);
extern int map_pci_registers(pci_t *pci_p, dev_info_t *dip);
extern void free_pci_properties(pci_t *pci_p);
extern void unmap_pci_registers(pci_t *pci_p);
extern void save_config_regs(pci_t *pci_p);
extern void restore_config_regs(pci_t *pci_p);
extern void fault_init(pci_t *pci_p);
extern void fault_fini(pci_t *pci_p);
extern int log_pci_cfg_err(ushort_t e, int bridge_secondary);
extern int pci_get_portid(dev_info_t *dip);
/* bus map routines */
extern int xlate_reg_prop(pci_t *pci_p, dev_info_t *dip,
	pci_regspec_t *pci_rp, off_t off, off_t len, struct regspec *rp);
extern int get_reg_set(pci_t *pci_p, dev_info_t *child,
	int rnumber, off_t off, off_t len, struct regspec *rp);

/* bus add intrspec */
extern uint_t get_reg_set_size(dev_info_t *child, int rnumber);
extern uint_t get_nreg_set(dev_info_t *child);
extern uint_t get_nintr(dev_info_t *child);
extern uintptr_t get_config_reg_base(pci_t *pci_p);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_UTIL_H */
