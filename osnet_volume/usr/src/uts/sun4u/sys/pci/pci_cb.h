/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_CB_H
#define	_SYS_PCI_CB_H

#pragma ident	"@(#)pci_cb.h	1.4	99/07/09 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * control block soft state structure:
 *
 * Each pci node contains shares a control block structure with its peer
 * node.  The control block node contains csr and id registers for chip
 * and acts as a "catch all" for other functionality that does not cleanly
 * fall into other functional blocks.  This block is also used to handle
 * software workarounds for known hardware bugs in different chip revs.
 */
typedef struct cb cb_t;
struct cb {

	pci_t *cb_pci_p;	/* link back to pci soft state */

	/*
	 * virtual address of control block registers:
	 */
	volatile uint64_t *cb_id_reg;
	volatile uint64_t *cb_control_status_reg;

	/*
	 * thermal interrupt mondo and priority (if used):
	 */
	uint_t cb_thermal_mondo;
	uint_t cb_thermal_pil;
};

extern uint_t pci_bus_parking_enable;
extern uint_t pci_error_intr_enable;
extern uint_t pci_retry_disable;
extern uint_t pci_retry_enable;
extern uint_t pci_dwsync_disable;
extern uint_t pci_intsync_disable;
extern uint_t pci_b_arb_enable;
extern uint_t pci_a_arb_enable;

extern void cb_create(pci_t *pci_p);
extern void cb_destroy(pci_t *pci_p);
extern void cb_register_intr(cb_t *cb_p);
extern void cb_enable_intr(cb_t *cb_p);


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_CB_H */
