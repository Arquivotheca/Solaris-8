/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_space.c	1.6	99/07/21 SMI"

/*
 * PCI nexus driver tunables
 */

#include <sys/types.h>
#include <sys/pci.h>

/*LINTLIBRARY*/

/*
 * By initializing pci_interrupt_priorities_property to 1, the priority
 * level of the interrupt handler for a PCI device can be defined via an
 * "interrupt-priorities" property.  This property is an array of integer
 * values that have a one to one mapping the the "interrupts" property.
 * For example, if a device's "interrupts" property was (1, 2) and its
 * "interrupt-priorities" value was (5, 12), the handler for the first
 * interrupt would run at cpu priority 5 and the second at priority 12.
 * This would override the drivers standard mechanism for assigning
 * priorities to interrupt handlers.
 */
uint_t pci_interrupt_priorities_property = 1;

/*
 * By initializing pci_config_space_size_zero to 1, the driver will
 * tolerate mapping requests for configuration space "reg" entries whose
 * size is not zero.
 */
uint_t pci_config_space_size_zero = 1;

/*
 * The variable controls the default setting of the command register
 * for pci devices.  See init_child() for details.
 *
 * This flags also controls the setting of bits in the bridge control
 * register pci to pci bridges.  See init_child() for details.
 */
ushort_t pci_command_default = PCI_COMM_SERR_ENABLE |
				PCI_COMM_WAIT_CYC_ENAB |
				PCI_COMM_PARITY_DETECT |
				PCI_COMM_ME |
				PCI_COMM_MAE |
				PCI_COMM_IO;

/*
 * The following variable enables a workaround for the following obp bug:
 *
 *	1234181 - obp should set latency timer registers in pci
 *		configuration header
 *
 * Until this bug gets fixed in the obp, the following workaround should
 * be enabled.
 */
uint_t pci_set_latency_timer_register = 1;

/*
 * The following variable enables a workaround for an obp bug to be
 * submitted.  A bug requesting a workaround fof this problem has
 * been filed:
 *
 *	1235094 - need workarounds on positron nexus drivers to set cache
 *		line size registers
 *
 * Until this bug gets fixed in the obp, the following workaround should
 * be enabled.
 */
uint_t pci_set_cache_line_size_register = 1;

/*
 * The following driver parameters are defined as variables to allow
 * patching for debugging and tuning.  Flags that can be set on a per
 * PBM basis are bit fields where the PBM device instance number maps
 * to the bit position.
 */
#ifdef DEBUG
uint64_t pci_debug_flags = 0;
#endif
uint_t pci_disable_pass1_workarounds = 0;
uint_t pci_disable_pass2_workarounds = 0;
uint_t pci_disable_pass3_workarounds = 0;
uint_t pci_disable_plus_workarounds = 0;
uint_t pci_disable_default_workarounds = 0;
uint_t ecc_error_intr_enable = 1;
uint_t pci_sbh_error_intr_enable = (uint_t)-1;
uint_t pci_dto_error_intr_enable = (uint_t)-1;
uint_t pci_stream_buf_enable = (uint_t)-1;
uint_t pci_stream_buf_exists = 1;
uint_t pci_rerun_disable = (uint_t)-1;
uint_t pci_enable_periodic_loopback_dma = 0;
uint_t pci_enable_retry_arb = (uint_t)-1;

uint_t pci_bus_parking_enable = (uint_t)-1;
uint_t pci_error_intr_enable = (uint_t)-1;
uint_t pci_retry_disable = 0;
uint_t pci_retry_enable = 0;
uint_t pci_dwsync_disable = 0;
uint_t pci_intsync_disable = 0;
uint_t pci_b_arb_enable = 0xf;
uint_t pci_a_arb_enable = 0xf;
uint_t pci_ecc_afsr_retries = 100;	/* XXX - what's a good value? */

uint_t pci_intr_retry_intv = 5;		/* for interrupt retry reg */
uint8_t pci_latency_timer = 0x40;	/* for pci latency timer reg */
uint_t pci_sync_buf_timeout = 100;	/* 100 ticks = 1 second */
uint_t pci_panic_on_sbh_errors = 0;
uint_t pci_panic_on_fatal_errors = 1;	/* should be 1 at beta */
uint_t pci_per_enable = 1;
uint_t pci_thermal_intr_fatal = 1;	/* thermal interrupts fatal */
uint_t pci_lock_sbuf = 0;
uint_t pci_use_contexts = 1;
uint_t pci_sc_use_contexts = 1;

#ifdef SCHIZO_HW_BUGID_4238567
uint_t pci_context_minpages = 1;
#else
uint_t pci_context_minpages = 2;
#endif /* SCHIZO_HW_BUGID_4238567 */

/*
 * The following flag controls behavior of the ino handler routine
 * when multiple interrupts are attached to a single ino.  Typically
 * this case would occur for the ino's assigned to the PCI bus slots
 * with multi-function devices or bus bridges.
 *
 * Setting the flag to zero causes the ino handler routine to return
 * after finding the first interrupt handler to claim the interrupt.
 *
 * Setting the flag to non-zero causes the ino handler routine to
 * return after making one complete pass through the interrupt
 * handlers.
 */
uint_t pci_check_all_handlers = 1;

ulong_t pci_iommu_dvma_end = 0xfffffffful;
uint_t pci_lock_tlb = 0;
uint_t pci_dvma_debug_on = 0;

/*
 * dvma address space allocation cache variables
 */
uint_t pci_dvma_page_cache_entries = 0x200;	/* # of chunks (1 << bits) */
uint_t pci_dvma_page_cache_clustsz = 0x8;	/* # of pages per chunk */
uint_t pci_dvma_map_free_cnt = 0;
uint_t pci_dvma_cache_free_cnt = 0;
uint_t pci_disable_fdvma = 0;

uint_t pci_iommu_ctx_lock_failure = 0;

/*
 * This flag preserves prom iommu settings by copying prom TSB entries
 * to corresponding kernel TSB entry locations. It should be removed
 * after the interface properties from obp have become default.
 */
uint_t pci_preserve_iommu_tsb = 1;
