/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_IOMMU_H
#define	_SYS_IOMMU_H

#pragma ident	"@(#)iommu.h	1.30	99/04/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	IOMMU_PAGE_SIZE		(4096)		/* 4k page */
#define	IOMMU_PAGE_OFFSET	(4096 - 1)

/* constants for DVMA */
#define	IOMMU_DVMA_RANGE	(0x4000000)	/* 64M for sun4d */
#define	IOMMU_DVMA_BASE		(0 - IOMMU_DVMA_RANGE)
/* SBUS map size: the whole 64M minus 4 pages for dump now */
#define	IOMMU_DVMA_SIZE		(IOMMU_DVMA_RANGE - 4 * IOMMU_PAGE_SIZE)

#ifndef _ASM

#include <sys/avintr.h>
#include <sys/vmem.h>

/* define IOPTEs */
#define	IOPTE_PFN_MSK	0xffffff00
#define	IOPTE_PFN_SHIFT	0x8

#define	IOPTE_CACHE	0x80
#define	IOPTE_STREAM	0x40
#define	IOPTE_INTRA	0x08
#define	IOPTE_WRITE	0x04

#define	IOPTE_FLAG_MSK	0xcc

#define	IOPTE_VALID	0x02
#define	IOPTE_PARITY	0x01

typedef struct {
	uint_t	iopte;
} iommu_pte_t;

/* maybe we should have a sbus.h? */
#define	MX_SBUS		(14)
#define	MX_PRI		(8)
#define	MX_SBUS_SLOTS	(4)		/* 4 slots per Sbus */

/*
 * some bit masks describe slave access burst capability
 * of a slot in DDI way.
 */
#define	NO_SBUS_BURST	(7)		/* 7 realy means no burst */
#define	SBUS_BURST_MASK	(0x7f)		/* all possible bits */
#define	SBUS_BURST_64B	(0x7f)		/* 64 Bytes burst */
#define	HWBC_BURST_MASK	(0x70)		/* hwbcopy supports only BA16 and up */

struct sbus_private {
	int sbi_id;			/* sun4d UnitNum */
	caddr_t va_sbi;			/* vaddr for SBI regs */
	iommu_pte_t *va_xpt;		/* vaddr for IOPTEs */
	vmem_t *map;			/* DVMA map for this IOMMU */
	uint_t dma_reserve;		/* # of pages for dvma_reserve */

	uintptr_t dvma_call_list_id;	/* DVMA callback list */

	kmutex_t dma_pool_lock;
	caddr_t dmaimplbase;		/* protected by dma_pool_lock */

	int burst_size[MX_SBUS_SLOTS];	/* burst size in slot config reg */
	struct autovec *vec;		/* ISR list */
};

extern char DVMA[];

/* prototype for IOMMU related routines */
extern void iommu_init(iommu_pte_t *va_xpt);
extern int iommu_pteload(iommu_pte_t *piopte, uint_t mempfn, uint_t flag);
extern int iommu_unload(iommu_pte_t *va_xpt, caddr_t dvma_addr, int npf);
extern int iommu_pteunload(iommu_pte_t *piopte);
extern iommu_pte_t *iommu_ptefind(iommu_pte_t *va_xpt, caddr_t dvma_addr);

#endif !_ASM

/* some macros for iommu ... */
#define	iommu_btop(x)	(mmu_btop((uint_t)(x)))	/* all have 4k pages */
#define	iommu_btopr(x)	(mmu_btopr((uint_t)(x))) /* all have 4k pages */
#define	iommu_ptob(x)	(mmu_ptob(x))	/* all have 4k pages */

#define	SBUSMAP_MAXRESERVE	iommu_btop(IOMMU_DVMA_SIZE >> 1)
#define	SBUSMAP_SIZE		iommu_btop(IOMMU_DVMA_SIZE)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IOMMU_H */
