/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_NEXUS_H
#define	_SYS_PCI_NEXUS_H

#pragma ident	"@(#)pci_nexus.h	1.9	99/04/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct pci_ispec pci_ispec_t;

struct pci_ispec {
	struct intrspec ispec;		/* interrupt pri/pil, vec/ino, func */
	dev_info_t *pci_ispec_dip;	/* interrupt parent dip */
	uint32_t pci_ispec_intr;	/* dev "interrupts" prop or imap lookup
					 * result storage for UPA intr */
	void *pci_ispec_arg;		/* interrupt handler argument */
	ddi_acc_handle_t pci_ispec_hdl;	/* map hdl to dev PCI config space */
	pci_ispec_t *pci_ispec_next;	/* per ino link list */
};

enum pci_fault_ops { FAULT_LOG, FAULT_RESET, FAULT_POKEFLT, FAULT_POKEFINI };

struct pci_fault_handle {
	dev_info_t *fh_dip;		/* device registered fault handler */
	int (*fh_f)();			/* fault handler function */
	void *fh_arg;			/* argument for fault handler */
	struct pci_fault_handle *fh_next;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_NEXUS_H */
