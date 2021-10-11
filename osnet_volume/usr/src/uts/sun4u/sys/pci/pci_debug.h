/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PCI_DEBUG_H
#define	_SYS_PCI_DEBUG_H

#pragma ident	"@(#)pci_debug.h	1.2	98/07/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(DEBUG)
#define	DBG_ATTACH		0x1ull
#define	DBG_DETACH		0x2ull

#define	DBG_MAP			0x4ull

#define	DBG_G_ISPEC		0x10ull
#define	DBG_A_ISPEC		0x20ull
#define	DBG_R_ISPEC		0x40ull
#define	DBG_INIT_CLD		0x80ull

#define	DBG_CTLOPS		0x100ull
#define	DBG_INTR		0x200ull
#define	DBG_ERR_INTR		0x400ull
#define	DBG_BUS_FAULT		0x800ull

#define	DBG_DMA_ALLOCH		0x10000ull
#define	DBG_DMA_FREEH		0x20000ull
#define	DBG_DMA_BINDH		0x40000ull
#define	DBG_DMA_UNBINDH		0x80000ull

#define	DBG_DMA_MAP		0x100000ull
#define	DBG_CHK_MOD		0x200000ull
#define	DBG_BYPASS		0x400000ull
#define	DBG_IOMMU		0x800000ull

#define	DBG_DMA_WIN		0x1000000ull
#define	DBG_MAP_WIN		0x2000000ull
#define	DBG_UNMAP_WIN		0x4000000ull
#define	DBG_DMA_CTL		0x8000000ull

#define	DBG_DMA_FLUSH		0x10000000ull
#define	DBG_DMASYNC_FLUSH	0x20000000ull
#define	DBG_FAST_DVMA		0x40000000ull

#define	DBG_SC			(0x10ull << 32)
#define	DBG_IB			(0x20ull << 32)
#define	DBG_CB			(0x40ull << 32)
#define	DBG_PBM			(0x80ull << 32)

#define	DBG_CONT		(0x100ull << 32)

#define	DBG_OPEN		(0x1000ull << 32)
#define	DBG_CLOSE		(0x2000ull << 32)
#define	DBG_IOCTL		(0x4000ull << 32)

#define	DEBUG0(flag, dip, fmt)	\
	pci_debug(flag, dip, fmt, 0, 0, 0, 0, 0);
#define	DEBUG1(flag, dip, fmt, a1)	\
	pci_debug(flag, dip, fmt, (uintptr_t)(a1), 0, 0, 0, 0);
#define	DEBUG2(flag, dip, fmt, a1, a2)	\
	pci_debug(flag, dip, fmt, (uintptr_t)(a1), (uintptr_t)(a2), 0, 0, 0);
#define	DEBUG3(flag, dip, fmt, a1, a2, a3)	\
	pci_debug(flag, dip, fmt, (uintptr_t)(a1),	\
		(uintptr_t)(a2), (uintptr_t)(a3), 0, 0);
#define	DEBUG4(flag, dip, fmt, a1, a2, a3, a4)	\
	pci_debug(flag, dip, fmt, (uintptr_t)(a1),	\
		(uintptr_t)(a2), (uintptr_t)(a3), \
		(uintptr_t)(a4), 0);
#define	DEBUG5(flag, dip, fmt, a1, a2, a3, a4, a5)	\
	pci_debug(flag, dip, fmt, (uintptr_t)(a1),	\
		(uintptr_t)(a2), (uintptr_t)(a3), \
		(uintptr_t)(a4), (uintptr_t)(a5));

extern void pci_debug(uint64_t, dev_info_t *, char *,
			uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
#else
#define	DEBUG0(flag, dip, fmt)
#define	DEBUG1(flag, dip, fmt, a1)
#define	DEBUG2(flag, dip, fmt, a1, a2)
#define	DEBUG3(flag, dip, fmt, a1, a2, a3)
#define	DEBUG4(flag, dip, fmt, a1, a2, a3, a4)
#define	DEBUG5(flag, dip, fmt, a1, a2, a3, a4, a5)
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_DEBUG_H */
