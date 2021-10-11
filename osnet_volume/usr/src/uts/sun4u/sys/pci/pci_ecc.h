/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_ECC_H
#define	_SYS_PCI_ECC_H

#pragma ident	"@(#)pci_ecc.h	1.7	99/11/15 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct ecc_intr_info ecc_intr_info_t;
struct ecc_intr_info {
	struct ecc *ecc_p;

	/*
	 * so ecc_intr knows whether its being called b/c of a UE or CE
	 * error.
	 */
#define	PCI_ECC_UE	0
#define	PCI_ECC_CE	1
	int ecc_type;

	/*
	 * ECC status registers.
	 */
	volatile uint64_t *ecc_async_flt_status_reg;
	volatile uint64_t *ecc_async_flt_addr_reg;

	/*
	 * Mondo numbers and priority levels of UE or CE interrupt:
	 */
	uint_t ecc_mondo;
	uint_t ecc_pil;

	/*
	 * Implementation-specific masks & shift values.
	 */
	uint64_t ecc_errpndg_mask;	/* 0 if not applicable. */
	uint64_t ecc_offset_mask;
	uint_t ecc_offset_shift;
	uint_t ecc_size_log2;
};

typedef struct ecc ecc_t;
struct ecc {

	dev_info_t *ecc_dip;

	pci_t *ecc_pci_p;		/* link to PBM interrupt block */

	/*
	 * ECC control and status registers:
	 */
	volatile uint64_t *ecc_ctrl_reg;

	/*
	 * Information specific to error type.
	 */
	struct ecc_intr_info ecc_ue;
	struct ecc_intr_info ecc_ce;
};

extern char *ecc_main_fmt, *dw_fmt, *ecc_sec_fmt, *dvma_rd, *dvma_wr, *pio_wr;
extern void ecc_create(pci_t *pci_p);
extern void ecc_destroy(pci_t *pci_p);
extern void ecc_register_intr(ecc_t *ecc_p);
extern void ecc_enable_intr(ecc_t *ecc_p);
extern void ecc_disable_wait(ecc_t *ecc_p);
extern void ecc_disable_nowait(ecc_t *ecc_p);
extern void ecc_configure(ecc_t *ecc_p);
extern uint_t ecc_log_ue_error(struct async_flt *ecc, char *unum);
extern uint_t ecc_log_ce_error(struct async_flt *ecc, char *unum);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_ECC_H */
