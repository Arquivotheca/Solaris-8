/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_SPACE_H
#define	_SYS_PCI_SPACE_H

#pragma ident	"@(#)pci_space.h	1.7	99/07/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern uint_t pci_interrupt_priorities_property;
extern uint_t pci_config_space_size_zero;
extern ushort_t pci_command_default;
extern uint_t pci_set_latency_timer_register;
extern uint_t pci_set_cache_line_size_register;

#ifdef DEBUG
extern uint64_t pci_debug_flags;
#endif
extern uint_t pci_disable_pass1_workarounds;
extern uint_t pci_disable_pass2_workarounds;
extern uint_t pci_disable_pass3_workarounds;
extern uint_t pci_disable_plus_workarounds;
extern uint_t pci_disable_default_workarounds;
extern uint_t ecc_error_intr_enable;
extern uint_t pci_sbh_error_intr_enable;
extern uint_t pci_dto_error_intr_enable;
extern uint_t pci_stream_buf_enable;
extern uint_t pci_stream_buf_exists;
extern uint_t pci_rerun_disable;
extern uint_t pci_enable_periodic_loopback_dma;
extern uint_t pci_enable_retry_arb;

extern uint_t pci_bus_parking_enable;
extern uint_t pci_error_intr_enable;
extern uint_t pci_retry_disable;
extern uint_t pci_retry_enable;
extern uint_t pci_dwsync_disable;
extern uint_t pci_intsync_disable;
extern uint_t pci_b_arb_enable;
extern uint_t pci_a_arb_enable;
extern uint_t pci_ecc_afsr_retries;

extern uint_t pci_intr_retry_intv;
extern uint8_t pci_latency_timer;
extern uint_t pci_sync_buf_timeout;
extern uint_t pci_panic_on_sbh_errors;
extern uint_t pci_panic_on_fatal_errors;
extern uint_t pci_per_enable;
extern uint_t pci_thermal_intr_fatal;
extern uint_t pci_lock_sbuf;
extern uint_t pci_use_contexts;
extern uint_t pci_sc_use_contexts;
extern uint_t pci_context_minpages;

extern uint_t pci_check_all_handlers;
extern ulong_t pci_iommu_dvma_end;
extern uint_t pci_lock_tlb;

extern uint_t pci_dvma_debug_on;
extern uint_t pci_dvma_page_cache_entries;
extern uint_t pci_dvma_page_cache_clustsz;
extern uint_t pci_dvma_map_free_cnt;
extern uint_t pci_dvma_cache_free_cnt;
extern uint_t pci_disable_fdvma;

extern uint_t pci_iommu_ctx_lock_failure;
extern uint_t pci_preserve_iommu_tsb;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_SPACE_H */
