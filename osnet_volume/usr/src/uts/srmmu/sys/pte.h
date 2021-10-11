/*
 * Copyright (c) 1991,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PTE_H
#define	_SYS_PTE_H

#pragma ident	"@(#)pte.h	1.40	98/07/21 SMI"

/*
 * Copyright (c) 1988, Sun Microsystems, Inc. All Rights Reserved.
 * Sun considers its source code as an unpublished, proprietary
 * trade secret, and it is available only under strict license
 * provisions. This copyright notice is placed here only to protect
 * Sun in the event the source is deemed a published work. Disassembly,
 * decompilation, or other means of reducing the object code to human
 * readable form is prohibited by the license agreement under which
 * this code is provided to the user or company in possession of this
 * copy
 *
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the
 * Government is subject to restrictions as set forth in subparagraph
 * (c) (1) (ii) of the Rights in Technical Data and Computeer Software
 * clause at DFARS 52.227-7013 and in similar clauses in the FAR and
 * NASA FAR Supplement
 */

#ifndef _ASM
#include <sys/types.h>
#endif /* _ASM */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * FIXME: This is for non Maxiray SRMMU, but there seems to
 *	  be no references to these macro in the code anyway.
 */
#ifdef FIXME
#define	FLP_VPN(a)	(((a) << 12) & 0xFFFFF000)
#define	FLP_TYPE(b)	(((b) << 8) & 0x00000F00)
#define	FLP_XVPN(a)	(((a) >> 12) & 0xFFFFF)
#define	FLP_XTYPE(b)	(((b) >> 8) & 0xF)
#endif /* FIXME */

/* The values of the type field above */
#define	MMU_FLP_PTE		0
#define	MMU_FLP_LEVEL2		1	/* segment */
#define	MMU_FLP_LEVEL1		2	/* region */
#define	MMU_FLP_ROOT		3	/* context */
#define	MMU_FLP_ALL		4	/* all */
/* 5-0xf are reserved */

#ifndef _ASM

/*
 * generate a pte for the specified page frame, with the
 * specified permissions, possibly cacheable.
 * note: upper bits of page frame may modify the space value,
 * this is a feature not a bug.
 */

#define	MRPTEOF(s, p, a, c, m, r)		\
	(((s)<<28)|((p)<<8)|((c)<<7)|((m)<<6)|	\
	((r)<<5)|((a)<<2)|MMU_ET_PTE)

#define	PTEOF(s, p, a, c)	MRPTEOF(s, p, a, c, 0, 0)

typedef struct pte {
	uint_t PhysicalPageNumber:24;
	uint_t Cacheable:1;
	uint_t Modified:1;
	uint_t Referenced:1;
	uint_t AccessPermissions:3;
	uint_t EntryType:2;
} pte_t;

union ptes {
	struct pte pte;
	uint_t pte_int;
};
#endif /* !_ASM */

#define	PTE_REF_MASK		0x20
#define	PTE_MOD_MASK		0x40
#define	PTE_RM_MASK		0x60
#define	PTE_C_MASK		0x80		/* cacheable */
#define	PTE_ETYPE(a)		((a) & 0x3)
#define	PTE_ETYPEMASK		(0x3)
#define	PTE_PERMS(b)		(((b) & 0x7) << 2)
#define	PTE_PERMMASK		((0x7 << 2))
#define	PTE_PERMSHIFT		(2)
#define	PTE_REF(c)		(((c) & 0x1) << 5)
#define	PTE_MOD(c)		(((c) & 0x1) << 6)
#define	PTE_CACHEABLE(d)	(((d) & 0x1) << 7)
#define	PTE_PFN(p)		((p >> 8) & 0xFFFFFF)
#define	PTE_DSI_PPN(d, p)	((((d) & 0x3) << 22) | (p) & 0x3FFFFF)

#define	PTE_NO_RM_MASK		(~PTE_RM_MASK)

#define	PTE_BUSTYPE(p)		(((p) >> 20) & 0xF)
#define	PTE_BUSTYPE_PFN(b, p)	((((b) & 0xF) << 20) | (p) & 0xFFFFF)

#define	pte_valid(pte) (((pte)->EntryType) == MMU_ET_PTE)

#define	pte_konly(pte) (((pte)->AccessPermissions == MMU_STD_SRX) || \
			((pte)->AccessPermissions == MMU_STD_SRWX))

#define	PTE_ACC_WRITE		(0x1)
#define	pte_ronly(pte) (!((pte)->AccessPermissions & PTE_ACC_WRITE))

#ifdef sun4m
#define	pte_memory(pte) (((pte)->PhysicalPageNumber & 0xf00000) == 0)
#endif sun4m

#ifdef sun4d
#define	pte_memory(pte) ((pte)->Cacheable)
#endif sun4d

#define	MAKE_PFNUM(a)	\
	(((struct pte *)(a))->PhysicalPageNumber)

/* Definitions for EntryType */

#define	MMU_ET_INVALID		0
#define	MMU_ET_PTP		1
#define	MMU_ET_PTE		2

#define	PG_V			MMU_ET_PTE

/* Definitions for AccessPermissions */

#define	MMU_STD_SRUR		0
#define	MMU_STD_SRWURW		1
#define	MMU_STD_SRXURX		2
#define	MMU_STD_SRWXURWX	3
#define	MMU_STD_SXUX		4
#define	MMU_STD_SRWUR		5
#define	MMU_STD_SRX		6
#define	MMU_STD_SRWX		7

#define	MAKE_PROT(v)		PTE_PERMS(v)
#define	PG_PROT			MAKE_PROT(0x7)
#define	PG_KW			MAKE_PROT(MMU_STD_SRWX)
#define	PG_KR			MAKE_PROT(MMU_STD_SRX)
#define	PG_UW			MAKE_PROT(MMU_STD_SRWXURWX)
#define	PG_URKR			MAKE_PROT(MMU_STD_SRUR)
#define	PG_UR			MAKE_PROT(MMU_STD_SRUR)
#define	PG_UPAGE		MAKE_PROT(MMU_SRWUR)

#define	MMU_STD_PAGEMASK	0xFFFFFF000
#define	MMU_STD_PAGESHIFT	12 /* only maps 35:12 */
#define	MMU_STD_PTPSHIFT	6 /* Pointer is 35:6 */
#define	MMU_STD_PAGESIZE	(1 << MMU_STD_PAGESHIFT)

#define	MMU_STD_SEGSHIFT	18
#define	MMU_STD_RGNSHIFT	24
#define	MMU_STD_SEGMENTSIZE	(1 << MMU_STD_SEGSHIFT)
#define	MMU_STD_REGIONSIZE	(1 << MMU_STD_RGNSHIFT)

#define	MMU_STD_ROOTMASK	0xFFFFF /* after pageshift */
#define	MMU_STD_ROOTSHIFT	20
#define	MMU_STD_FIRSTMASK	0xFFF
#define	MMU_STD_FIRSTSHIFT	12
#define	MMU_STD_SECONDMASK	0x3F
#define	MMU_STD_SECONDSHIFT	6
#define	MMU_STD_THIRDMASK	0x0
#define	MMU_STD_THIRDSHIFT	0

#define	MMU_ROOT_XTR(a)		(((a) & 0xFFFFF000) >> MMU_STD_PAGESHIFT)
#define	MMU_FIRST_XTR(a)	(((a)&0x00FFF000) >> MMU_STD_PAGESHIFT)
#define	MMU_SECOND_XTR(a)	(((a)&0x0003F000) >> MMU_STD_PAGESHIFT)

#define	MMU_L1_SIZE		(1 << (MMU_STD_PAGESHIFT + MMU_STD_FIRSTSHIFT))
#define	MMU_L2_SIZE		(1 << (MMU_STD_PAGESHIFT + MMU_STD_SECONDSHIFT))
#define	MMU_L3_SIZE		(1 << (MMU_STD_PAGESHIFT + MMU_STD_THIRDSHIFT))

#define	MMU_L1_INDEX(a)		((((uint_t)(a)) & 0xFF000000) >> \
				    (MMU_STD_PAGESHIFT + MMU_STD_FIRSTSHIFT))
#define	MMU_L2_INDEX(a) 	((((uint_t)(a)) & 0x00FC0000) >> \
				    (MMU_STD_PAGESHIFT + MMU_STD_SECONDSHIFT))
#define	MMU_L3_INDEX(a)		((((uint_t)(a)) & 0x0003F000) >>  \
				    (MMU_STD_PAGESHIFT + MMU_STD_THIRDSHIFT))

#define	MMU_L1_BASE(a)		((caddr_t)(((uint_t)(a)) & 0xFF000000))
#define	MMU_L2_BASE(a)		((caddr_t)(((uint_t)(a)) & 0xFFFC0000))
#define	MMU_L3_BASE(a)		((caddr_t)(((uint_t)(a)) & 0xFFFFF000))

#define	MMU_L1_OFF(a)		(((uint_t)(a)) & 0x00FFFFFF)
#define	MMU_L2_OFF(a)		(((uint_t)(a)) & 0x0003FFFF)
#define	MMU_L3_OFF(a)		(((uint_t)(a)) & 0x00000FFF)

#define	MMU_L3_MASK		0xFFFFF000
#define	MMU_L2_MASK		0xFFFC0000
#define	MMU_L1_MASK		0xFF000000

#define	MMU_L2_BITS		0x0003F000
#define	MMU_L1_BITS		0x00FFF000

#define	MMU_L1_VA(a)	((a << \
	(MMU_STD_PAGESHIFT + MMU_STD_FIRSTSHIFT)) & 0xff000000)
#define	MMU_L2_VA(a)	((a << \
	(MMU_STD_PAGESHIFT + MMU_STD_SECONDSHIFT)) & 0xfc0000)
#define	MMU_L3_VA(a)	((a  << \
	(MMU_STD_PAGESHIFT + MMU_STD_THIRDSHIFT)) & 0x3f000)



/*
 * Macro for translating a virtual address to a physical page frame
 * number.
 */
#define	VA_TO_PFN(va)	(PTE_PFN(mmu_probe(va, NULL)))

/*
 * Macro for translating a virtual address to a physical address.
 * Assumes that bits <35:32> of the physical address are 0.
 */
#define	VA_TO_PA(va)	(VA_TO_PFN(va) << MMU_STD_PAGESHIFT)



#ifndef _ASM
extern struct pte mmu_pteinvalid;
#endif /* _ASM */
#define	MMU_STD_INVALIDPTP	(0)	/* 0 entry type */
#define	MMU_STD_INVALIDPTE	(0)	/* 0 entry type */

#ifndef _ASM
/*
 * generate a ptp referencing a table starting at the specified
 * physical address.
 */
#define	PTPOF(ta)	((((unsigned)(ta))<<2)|MMU_ET_PTP)

struct ptp {
	uint_t PageTablePointer:30;
	uint_t EntryType:2; /* Checked for validity */
};

union ptpe {
	struct pte pte;
	struct ptp ptp;
	uint_t ptpe_int;
};

#endif /* !_ASM */

#define	MMU_NPTE_ONE	256		/* 256 PTEs in first level table */
#define	MMU_NPTE_TWO	64
#define	MMU_NPTE_THREE	64

#if !defined(_ASM) && defined(_KERNEL)

extern int	bustype();

#endif /* !defined(_ASM) && defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif /* !_SYS_PTE_H */
