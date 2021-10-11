/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_NEXUSINTR_H
#define	_SYS_NEXUSINTR_H

#pragma ident	"@(#)nexusintr.h	1.7	99/04/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/dditypes.h>

/*
 * The consolidated interrupt op definition.
 * The following ops are processed at the first nexus who claims ownership of
 * the interrupt domain. (i.e. usually it's parent)
 * DDI_INTR_CTLOPS_ALLOC_ISPEC:
 *	This op is used to allocate a platform specific interrupt
 *	specification.
 *
 *	note: SPARC platforms will use this op to also set the bus interrupt
 * 	value taken from the IEEE 1275 "interrupts" property.  A processor
 *	interrupt level may also be calculated during this call.
 *
 * DDI_INTR_CTLOPS_FREE_ISPEC:
 *	This op is used to free an interrupt specification.
 *
 * DDI_INTR_CTLOPS_NINTRS:
 *	This op is used to determine the number of interrupts that a device
 *	supports.
 *
 * The following ops are processed up through the device tree terminating at
 * the root nexus.
 * DDI_INTR_CTLOPS_ADD:
 *	This op is used to add an interrupt handler into the system for a given
 *	device  Any interrupt translation should be done now before passing the
 *	ADD call up the device tree.  Any nexus specific programming for the
 *	interrupt should be done too.
 *
 * DDI_INTR_CTLOPS_REMOVE:
 *	This op is used to remove an interrupt handler from the system
 *	for a given device.  Any interrupt translation should be done now
 *	before passing the REMOVE call up the device tree.  Any nexus
 *	specific programming for the interrupt should be done too.
 *
 * DDI_INTR_CTLOPS_HILEVEL:
 *	This op is used to determine if a device has a high level interrupt.
 *	Any interrupt translation should be done now before passing the
 *	HILEVEL call up the device tree.
 *
 * Note:
 * 1) There is no XLATE intr ctlop.  This is due to the fact that all the ops
 * which percolate up through the ddi device tree, must have a translation
 * performed dynamically at each nexus where a translation is meaningful.
 * 2) The sun4u PCI bus nexus drivers can be used to see how the
 * intr_ctlop bus op should be implemented.
 */
typedef enum {
	DDI_INTR_CTLOPS_ALLOC_ISPEC = 0,
	DDI_INTR_CTLOPS_FREE_ISPEC,
	DDI_INTR_CTLOPS_ADD,
	DDI_INTR_CTLOPS_REMOVE,
	DDI_INTR_CTLOPS_NINTRS,
	DDI_INTR_CTLOPS_HILEVEL
} ddi_intr_ctlop_t;

typedef struct ddi_intr_info {
	ddi_intrspec_t ii_ispec;  /* Stacked interrupt specification */
	ddi_iblock_cookie_t *ii_iblock_cookiep;
	ddi_idevice_cookie_t *ii_idevice_cookiep;
	uint_t (*ii_int_handler)(caddr_t ii_int_handler_arg);
	caddr_t ii_int_handler_arg;
	int32_t ii_kind;	  /* kind of interrupt spec */
	uint32_t ii_inum;	  /* Interrupt index of interrupt spec. */
} ddi_intr_info_t;


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_NEXUSINTR_H */
