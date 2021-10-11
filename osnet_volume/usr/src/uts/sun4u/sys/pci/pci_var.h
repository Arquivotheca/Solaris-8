/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PCI_VAR_H
#define	_SYS_PCI_VAR_H

#pragma ident	"@(#)pci_var.h	1.55	99/07/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	PCI_VALID_ID(id) ((id) < MAX_UPA)

/*
 * The following typedef is used to represent a
 * 1275 "bus-range" property of a PCI Bus node.
 */
typedef struct bus_range {
	uint32_t lo;
	uint32_t hi;
} pci_bus_range_t;

/*
 * The following typedef is used to represent an entry in the "ranges"
 * property of a device node.
 */
typedef struct ranges {
	uint32_t child_high;
	uint32_t child_mid;
	uint32_t child_low;
	uint32_t parent_high;
	uint32_t parent_low;
	uint32_t size_high;
	uint32_t size_low;
} pci_ranges_t;

typedef struct config_header_state config_header_state_t;
struct config_header_state {
	dev_info_t *chs_dip;
	uint16_t chs_command;
	uint8_t chs_cache_line_size;
	uint8_t chs_latency_timer;
	uint8_t chs_header_type;
	uint8_t chs_sec_latency_timer;
	uint8_t chs_bridge_control;
};

typedef enum { PSYCHO, SCHIZO } pci_bridge_t;
typedef enum { A, B } pci_side_t;
typedef enum { PCI_NEW, PCI_ATTACHED, PCI_DETACHED, PCI_SUSPENDED } pci_state_t;

/*
 * used for the picN kstats
 */
#define	PSYCHO_NUM_PICS		2
#define	PSYCHO_NUM_EVENTS	26
#define	PSYCHO_PIC0_MASK	0x00000000FFFFFFFFULL	/* pic0 bits of %pic */

/*
 * pci common soft state structure:
 *
 * Each psycho or schizo is represented by a pair of pci nodes in the
 * device tree.  A single pci common soft state is allocated for each
 * pair.  The UPA (Safari) bus id of the psycho (schizo) is used for
 * the instance number.  The attach routine uses the existance of a
 * pci common soft state structure to determine if one node from the
 * pair has been attached.
 */
typedef struct pci_common pci_common_t;
struct pci_common {
	/* Links to functional blocks potentially shared between pci nodes */
	iommu_t *pci_common_iommu_p;
	cb_t *pci_common_cb_p;
	ib_t *pci_common_ib_p;
	ecc_t *pci_common_ecc_p;

	/* pointers & counters to facilitate attach/detach & suspend/resume */
	pci_t *pci_p[2];		/* pci soft states of both sides */
	ushort_t pci_common_refcnt;	/* # of sides suspended + attached */
	ushort_t pci_common_attachcnt;	/* # of sides attached */

	/*
	 * Performance registers and kstat.
	 */
	volatile uint64_t *psycho_pic;	/* perf counter register */
	volatile uint64_t *psycho_pcr;	/* perf counter control */
	kstat_t *psycho_counters_ksp;	/* perf counter kstat */
};

/*
 * used to build array of event-names and pcr-mask values
 */
typedef struct psycho_event_mask {
	char *event_name;
	uint64_t pcr_mask;
} psycho_event_mask_t;

/*
 * pci soft state structure:
 *
 * Each pci node has a pci soft state structure.
 */
struct pci {

	/*
	 * State flags and mutex:
	 */
	pci_state_t pci_state;
	uint_t pci_soft_state;
	uint_t pci_open_count;
#define	PCI_SOFT_STATE_OPEN		0x01
#define	PCI_SOFT_STATE_OPEN_EXCL	0x02
#define	PCI_SOFT_STATE_CLOSED		0x04
	kmutex_t pci_mutex;

	/*
	 * Links to other state structures:
	 */
	pci_common_t *pci_common_p;	/* pointer common soft state */
	dev_info_t *pci_dip;		/* devinfo structure */
	ib_t *pci_ib_p;			/* interrupt block */
	cb_t *pci_cb_p;			/* control block */
	pbm_t *pci_pbm_p;		/* PBM block */
	iommu_t	*pci_iommu_p;		/* IOMMU block */
	sc_t *pci_sc_p;			/* streaming cache block */
	ecc_t *pci_ecc_p;		/* ECC error block */

	/*
	 * other state info:
	 */
	uint_t pci_id;			/* UPA (or Safari) device id */
	pci_side_t pci_side;

	/*
	 * pci device node properties:
	 */
	pci_bus_range_t pci_bus_range;	/* "bus-range" */
	pci_ranges_t *pci_ranges;	/* "ranges" data & length */
	int pci_ranges_length;
	uint32_t *pci_interrupts;	/* "interrupts" data & length */
	int pci_interrupts_length;
	int pci_numproxy;		/* upa interrupt proxies */
	int pci_thermal_interrupt;	/* node has thermal interrupt */

	kmutex_t pci_fh_lst_mutex;		/* Fault handling mutex */
	struct pci_fault_handle *pci_fh_lst;	/* Fault handle list */

	/*
	 * register mapping:
	 */
	caddr_t pci_address[3];
	ddi_acc_handle_t pci_ac[3];

	/*
	 * dma handle data:
	 */
	uintptr_t pci_handle_pool_call_list_id;	/* call back list id */

	/*
	 * cpr support:
	 */
	uint_t pci_config_state_entries;
	config_header_state_t *pci_config_state_p;

	/* Interrupt support */
	int intr_map_size;
	struct intr_map *intr_map;
	struct intr_map_mask *intr_map_mask;
};

/*
 * PSYCHO and PBM soft state macros:
 */
#define	get_pci_soft_state(i)	\
	((pci_t *)ddi_get_soft_state(per_pci_state, (i)))

#define	alloc_pci_soft_state(i)	\
	ddi_soft_state_zalloc(per_pci_state, (i))

#define	free_pci_soft_state(i)	\
	ddi_soft_state_free(per_pci_state, (i))

#define	get_pci_common_soft_state(i)	\
	((pci_common_t *)ddi_get_soft_state(per_pci_common_state, (i)))

#define	alloc_pci_common_soft_state(i)	\
	ddi_soft_state_zalloc(per_pci_common_state, (i))

#define	free_pci_common_soft_state(i)	\
	ddi_soft_state_free(per_pci_common_state, (i))


extern void *per_pci_state;		/* per-pbm soft state pointer */
extern void *per_pci_common_state;	/* per-psycho soft state pointer */
extern kmutex_t pci_global_mutex;	/* attach/detach common struct lock */
extern kmutex_t dvma_active_list_mutex;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_VAR_H */
