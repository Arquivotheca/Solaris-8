/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_IOMMU_H
#define	_SYS_IOMMU_H

#pragma ident	"@(#)iommu.h	1.26	99/04/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* page offsets - fixed for architecture  */
#define	IOMMU_CTL_REG		0x000	   /* control regs */
#define	IOMMU_BASE_REG		0x004	   /* base addr regs */
#define	IOMMU_FLUSH_ALL_REG	0x014	   /* flush all regs */
#define	IOMMU_ADDR_FLUSH_REG	0x018	   /* addr flush regs */


/* constants for DVMA */
#define	IOMMU_CTL_RANGE		(2)		/* 64MB xlation range */
						/* 16MB * 2 ** <range> */
#define	IOMMU_DVMA_RANGE	(0x1000000 << IOMMU_CTL_RANGE)
#define	IOMMU_DVMA_BASE		(0 - IOMMU_DVMA_RANGE)

#define	IOMMU_PAGE_SIZE		(4096)		/* 4k page */
#define	IOMMU_PAGE_OFFSET	(4096 - 1)
#define	IOMMU_N_PTES		(IOMMU_DVMA_RANGE/IOMMU_PAGE_SIZE)
#define	IOMMU_PTE_TBL_SIZE	(IOMMU_N_PTES << 2)	/* 4B for each entry */

#define	IOMMU_PTE_BASE_SHIFT	(14)	/* PA<35:14> is used in base reg */

#ifndef _ASM
extern caddr_t v_iommu_addr;
#define	V_IOMMU_CTL_REG		(v_iommu_addr+IOMMU_CTL_REG)
#define	V_IOMMU_BASE_REG	(v_iommu_addr+IOMMU_BASE_REG)
#define	V_IOMMU_FLUSH_ALL_REG	(v_iommu_addr+IOMMU_FLUSH_ALL_REG)
#define	V_IOMMU_ADDR_FLUSH_REG	(v_iommu_addr+IOMMU_ADDR_FLUSH_REG)

/* define IOPTEs */

#define	IOPTE_PFN_MSK	0xffffff00
#define	IOPTE_PFN_SHIFT 0x8
#define	IOMMU_MK_PFN(piopte)	(((piopte)->iopte) >> IOPTE_PFN_SHIFT)

#define	IOPTE_CACHE	0x80
#define	IOPTE_WRITE	0x04
#define	IOPTE_VALID	0x02

typedef struct iommu_pte {
	uint_t	iopte;
} iommu_pte_t;

/* iommu registers */
union iommu_ctl_reg {
	struct {
		uint_t impl:4;	/* implementation # */
		uint_t version:4; /* version # */
		uint_t rsvd:19;	/* reserved */
		uint_t range:3;	/* dvma range */
		uint_t diag:1;	/* diagnostic enable */
		uint_t enable:1; /* iommu enable */
	} ctl_reg;
	uint_t	ctl_uint;
};	/* control register */

union iommu_base_reg {
	struct {
		uint_t base:22;	/* base of iopte tbl */
		uint_t rsvd:10;	/* reserved */
	} base_reg;
	uint_t	base_uint;
};	/* base register */

/* iommu address flush registers */
union iommu_flush_reg {
	struct {
		uint_t rsvd1:1;		/* reserved 1 */
		uint_t flush_addr:19;	/* flush addr */
		uint_t rsvd2:12;	/* reserved 2 */
	} flush_reg;
	uint_t	flush_uint;
};	/* flush register */

extern int iom;
extern char DVMA[];

extern iommu_pte_t *ioptes, *eioptes;

#endif /* !_ASM */

/* an mask that takes out unused bits in dvma address before flush */
/*
 * NOTE: sun4m defines bit 31 to be a don't care but small4m
 *	requires it to be defined.
 */
#define	IOMMU_FLUSH_MSK		0xFFFFF000

/* some macros for iommu ... */
#define	iommu_btop(x)	(mmu_btop((uint_t)(x)))	/* all have 4k pages */
#define	iommu_btopr(x)	(mmu_btopr((uint_t)(x))) /* all have 4k pages */
#define	iommu_ptob(x)	(mmu_ptob(x))	/* all have 4k pages */

#define	SBUSMAP_MAXRESERVE	(iommu_btop(IOMMU_DVMA_RANGE) >> 1)

/* flags for iom_pteload/iom_pagesync */
#define	IOM_WRITE	0x1
#define	IOM_CACHE	0x2

#if defined(_KERNEL) && !defined(_ASM)

extern void iommu_init(void);
extern int iommu_pteload(iommu_pte_t *, int, int);
extern int iommu_unload(uint_t, int);
extern int iommu_pteunload(iommu_pte_t *);
extern iommu_pte_t *iommu_ptefind(int);
extern void iommu_readpte(iommu_pte_t *, iommu_pte_t *);

extern void mmu_readpte(struct pte *, struct pte *);
extern void mmu_flushall(void);

extern void iommu_set_ctl(uint_t);
extern void iommu_set_base(uint_t);
extern void iommu_flush_all(void);
extern void iommu_addr_flush(int);

#endif /* _KERNEL && !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IOMMU_H */
