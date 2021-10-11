/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_PBM_H
#define	_SYS_PCI_PBM_H

#pragma ident	"@(#)pci_pbm.h	1.5	99/04/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The following structure represents the pci configuration header
 * for a psycho or schizo PBM.
 */
typedef struct config_header config_header_t;
struct config_header {
	volatile uint16_t ch_device_id;
	volatile uint16_t ch_vendor_id;
	volatile uint16_t ch_command_reg;
	volatile uint16_t ch_status_reg;
	volatile uint8_t ch_revision_id_reg;
	volatile uint8_t ch_programming_if_code_reg;
	volatile uint8_t ch_sub_class_reg;
	volatile uint8_t ch_base_class_reg;
	volatile uint8_t ch_cache_line_size_reg;
	volatile uint8_t ch_latency_timer_reg;
	volatile uint8_t ch_header_type_reg;
};

typedef enum { PBM_SPEED_33MHZ, PBM_SPEED_66MHZ } pbm_speed_t;

/*
 * pbm block soft state structure:
 *
 * Each pci node has its own private pbm block structure.
 */
typedef struct pbm pbm_t;
struct pbm {
	pci_t *pbm_pci_p;	/* link back to pci soft state */
	pbm_speed_t pbm_speed;	/* PCI bus speed (33 or 66 Mhz) */

	/*
	 * PBM control and error registers:
	 */
	volatile uint64_t *pbm_ctrl_reg;
	volatile uint64_t *pbm_async_flt_status_reg;
	volatile uint64_t *pbm_async_flt_addr_reg;
	volatile uint64_t *pbm_diag_reg;
	volatile uint64_t *pbm_dma_sync_reg;

	/*
	 * PCI configuration header block for the PBM:
	 */
	config_header_t *pbm_config_header;
	ddi_acc_handle_t pbm_config_ac;

	/*
	 * Memory address range on this PBM used to determine DMA on this pbm
	 */
	iopfn_t pbm_base_pfn;
	iopfn_t pbm_last_pfn;

	/*
	 * pbm error interrupt mondo and priority:
	 */
	uint_t pbm_bus_error_mondo;
	ddi_iblock_cookie_t pbm_iblock_cookie;

	/*
	 * support for ddi_poke:
	 */
	ddi_nofault_data_t *nofault_data;
	kmutex_t pbm_pokefault_mutex;
};

/*
 * forward declarations (object creation and destruction):
 */

extern void pbm_create(pci_t *pci_p);
extern void pbm_destroy(pci_t *pci_p);
extern void pbm_configure(pbm_t *pbm_p);
extern void pbm_register_intr(pbm_t *pbm_p);
extern void pbm_clear_error(pbm_t *pbm_p);
extern void pbm_enable_intr(pbm_t *pbm_p);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_PBM_H */
