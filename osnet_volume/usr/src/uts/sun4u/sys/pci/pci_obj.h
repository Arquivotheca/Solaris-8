/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_OBJ_H
#define	_SYS_PCI_OBJ_H

#pragma ident	"@(#)pci_obj.h	1.4	98/11/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/pci.h>
#include <sys/pci/pci_nexus.h>
#include <sys/pci/pci_types.h>
#include <sys/pci/pci_iommu.h>
#include <sys/pci/pci_dma.h>
#include <sys/pci/pci_sc.h>	/* needs pci_iommu.h */
#include <sys/pci/pci_fdvma.h>
#include <sys/pci/pci_cb.h>
#include <sys/pci/pci_ib.h>
#include <sys/pci/pci_ecc.h>
#include <sys/pci/pci_pbm.h>
#include <sys/pci/pci_intr.h>	/* needs pci_ib.h */
#include <sys/pci/pci_var.h>
#include <sys/pci/pci_util.h>
#include <sys/pci/pci_space.h>
#include <sys/pci/pci_regs.h>
#include <sys/pci/pci_debug.h>
#include <sys/pci/pci_chip.h>	/* collection of chip specific interface */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_OBJ_H */
