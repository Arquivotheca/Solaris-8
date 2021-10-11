/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _IA32_SYS_PTE_H
#define	_IA32_SYS_PTE_H

#pragma ident	"@(#)pte.h	1.22	99/09/16 SMI"

/*
 * Copyright (c) 1991, 1992, Sun Microsystems, Inc. All Rights Reserved.
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
#include <ia32/sys/mmu.h>
#endif /* _ASM */

#ifdef	__cplusplus
extern "C" {
#endif


#define	PTE_GB_MASK		0x100		/* global enable */
#define	PTE_RM_MASK		0x60
#define	PTE_REF_MASK		0x20
#define	PTE_MOD_MASK		0x40
#define	PTE_RW_MASK		0x02
#define	PTE_RM_SHIFT		5
#define	PTE_PERMS(b)		(((b) & 0x3) << 1)
#define	PTE_PERMMASK		((0x3 << 1))
#define	PTE_PERMSHIFT		(1)
#define	PTE_WRITETHRU(w)	(((w) & 0x1) << 3)
#define	PTE_NONCACHEABLE(d)	(((d) & 0x1) << 4)
#define	PTE_REF(r)		(((r) & 0x1) << 5)
#define	PTE_MOD(m)		(((m) & 0x1) << 6)
#define	PTE_OSRESERVED(o)	(((o) & 0x7) << 9)
#define	pte_valid(pte)		(((pte_t *)(pte))->Present)
#define	pte_ro(pte)		(!(((pte_t *)(pte))->AccessPermissions & 1))

#define	MAKE_PROT(v)		PTE_PERMS(v)
#define	PG_PROT			MAKE_PROT(0x3)
#define	PG_KW			MAKE_PROT(MMU_STD_SRWX)
#define	PG_KR			MAKE_PROT(MMU_STD_SRX)
#define	PG_UW			MAKE_PROT(MMU_STD_SRWXURWX)
#define	PG_URKR			MAKE_PROT(MMU_STD_SRXURX)
#define	PG_UR			MAKE_PROT(MMU_STD_SRXURX)
#define	PG_UPAGE		PG_KW	/* Intel u pages not user readable */


#define	PG_V			1

typedef struct cr3ctx {
	uint_t		ct_cr3;
	struct hat	*ct_hat;
} cr3ctx_t;

#ifndef _ASM

#ifdef	PTE36			/* PTE36 ---------------------------- */

typedef uint64_t	pteval_t;
typedef	pteval_t *	pteptr_t;

typedef struct pte32 {
	uint32_t Present:1;
	uint32_t AccessPermissions:2;
	uint32_t WriteThru:1;
	uint32_t NonCacheable:1;
	uint32_t Referenced:1;
	uint32_t Modified:1;
	uint32_t MustBeZero:1;
	uint32_t GlobalEnable:1;
	uint32_t OSReserved:3;
	uint32_t PhysicalPageNumber:20;
} pte32_t;


typedef struct pte {
	uint32_t Present:1;
	uint32_t AccessPermissions:2;
	uint32_t WriteThru:1;
	uint32_t NonCacheable:1;
	uint32_t Referenced:1;
	uint32_t Modified:1;
	uint32_t MustBeZero:1;
	uint32_t GlobalEnable:1;
	uint32_t OSReserved:3;
	uint32_t PhysicalPageNumberL:20;
	uint32_t PhysicalPageNumberH;
					/*
					 * An easy way to ensure that
					 * reserved bits are zero.
					 */
} pte_t;
struct  pte64 {
	uint32_t	pte64_0_31;
	uint32_t	pte64_32_64;
};

#define	UNLOAD_PTE(a, b) \
	{ ((struct pte64 *)(a))->pte64_0_31 = (b); \
		((struct pte64 *)(a))->pte64_32_64 = (((uint64_t)(b)) >> 32); }

#define	LOAD_PTE(a, b) \
	{ ((struct pte64 *)(a))->pte64_32_64 = (((uint64_t)(b)) >> 32); \
		((struct pte64 *)(a))->pte64_0_31 = (b); }

/*
 * not necessary to set the most significant 32 bits - avoid race conditions
 * that would occur that may result in inconsistent values in pte or pde tables.
 */
#define	INVALIDATE_PTE(a) \
	(*((uint32_t *)(a))) = (uint32_t)MMU_STD_INVALIDPTE


#define	PTE_PFN(p)		(((uint64_t)(p)) >> 12)
#define	PTE32_PFN(p)		(((uint32_t)(p)) >> 12)
#define	PTEVAL2PFN(p)		((p) >> 12)

#define	pte_valid(pte)		(((pte_t *)(pte))->Present)
#define	pte32_valid(pte)	(((pte_t *)(pte))->Present)

#define	MMU_PTTBL_SIZE		32
#define	MMU_L1_INDEX(a)		(((uint_t)(a)) >> 21)
#define	MMU_L1_KINDEX(a)	(((uint_t)((a) - 0xc0000000)) >> 21)
#define	MMU_L2_INDEX(a)		((((uint_t)(a)) >> 12) & 0x1ff)
#define	MMU32_L1_VA(a)		(((uint_t)(a)) << 22)
#define	MMU32_L1_INDEX(a)	(((uint_t)(a)) >> 22)
#define	MMU32_L2_INDEX(a)	((((uint_t)(a)) >> 12) & 0x3ff)
#define	PAGETABLE_INDEX(a)	MMU_L2_INDEX(a)
#define	PAGEDIR_INDEX(a)	MMU_L1_INDEX(a)
#define	PAGETABLE32_INDEX(a)	MMU32_L2_INDEX(a)
#define	PAGEDIR32_INDEX(a)	MMU32_L1_INDEX(a)
/* ### remove uintXX references given pteval_t */
#define	two_mb_page(pte)	(*((uint64_t *)(pte)) & 0x80)
#define	four_mb_page(pte)	(*((uint32_t *)(pte)) & 0x80)
#define	IN_SAME_2MB_PAGE(a, b)	(MMU_L1_INDEX(a) == MMU_L1_INDEX(b))
#define	TWOMB_PDE(a, g,  b, c) \
	((((uint64_t)((uint_t)(a))) << MMU_STD_PAGESHIFT) |\
	((g) << 8) | 0x80 |(((b) & 0x03) << 1) | (c))
#define	TWOMB_PDE_ATTR(a, g,  b, pati, ce, wt, c) \
	((((uint64_t)((uint_t)(a))) << MMU_STD_PAGESHIFT) |\
	((g) << 8) | 0x80 |(((b) & 0x03) << 1) | ((pati) << 12)|\
	((!(ce)) << 4) | ((wt) << 3) | (c))

#define	PTE_GRDA	0x1e0
#define	MOVPTE32_2_PTE64(a, b) ((*((uint64_t *)(b))) =\
			    ((uint64_t)(*((uint32_t *)(a)) & ~PTE_GRDA)))
#define	MOV4MBPTE32_2_2MBPTE64(a, b) ((*((uint64_t *)(b))) =\
			    (((uint64_t)(*((uint32_t *)(a)) & ~PTE_GRDA))|\
			    0x80))
#define	MMU_STD_INVALIDPTE	((uint64_t)0)
#define	NPDPERAS	4
#define	NPTEPERPT	512	/* entries in page table */
#define	NPTESHIFT	9
#define	PTSIZE		(NPTEPERPT * MMU_PAGESIZE)	/* bytes mapped */
#define	PTOFFSET	(PTSIZE - 1)
#define	NPTEPERPT32	1024	/* entries in page table */
#define	NPTESHIFT32	10
#define	PTSIZE32	(NPTEPERPT32 * MMU_PAGESIZE)	/* bytes mapped */
#define	MMU_NPTE_ONE	2048 /* 2048 PTE's in first level table */
#define	MMU_NPTE_TWO	512  /* 512 PTE's in second level table */
#define	MMU32_NPTE_ONE	1024 /* 1024 PTE's in first level table */
#define	MMU32_NPTE_TWO	1024 /* 1024 PTE's in second level table */
#define	TWOMB_PAGESIZE		0x200000
#define	TWOMB_PAGEOFFSET	(TWOMB_PAGESIZE - 1)
#define	TWOMB_PAGESHIFT		21
#define	LARGE_PAGESHIFT		21

/*
 * with 8 byte page dir entries, 1 page needed to hold the kernel
 * page directory per GByte of kernel address space.
 */
#define	ONEGB			0x40000000
#define	PAGEDIRPTR_MAPSZ	ONEGB
	/* PTE36: each page can map 1 GB */
#define	KPGDIR_PGCNT		(howmany((0 - kernelbase), PAGEDIRPTR_MAPSZ))

extern uint64_t *kernel_only_pttbl;
#define	mmu_pgdirptbl_load(cpu, index, value)\
	((cpu)->cpu_pgdirpttbl[(index)] = (uint64_t)(value))

#define	CAS_PTE(a, b, c) 	cas64(a, b, c)
#define	BOOT_SIZE	(2 * FOURMB_PAGESIZE)
#define	PAGEDIR_SIZE	(MMU_PAGESIZE * NPDPERAS)
#define	LGPG_PDE	TWOMB_PDE_ATTR
#define	LARGE_PAGE	two_mb_page
#define	ALIGN_TONEXT_LGPAGE(a) ((uint_t)((a) + \
		TWOMB_PAGESIZE) & ~TWOMB_PAGEOFFSET);
#define	LARGEPAGESIZE TWOMB_PAGESIZE

#else		/* PTE36 */
				/* PTE32 ---------------------------- */


typedef uint32_t	pteval_t;
typedef	pteval_t *	pteptr_t;

typedef struct pte {
	uint_t Present:1;
	uint_t AccessPermissions:2;
	uint_t WriteThru:1;
	uint_t NonCacheable:1;
	uint_t Referenced:1;
	uint_t Modified:1;
	uint_t MustBeZero:1;
	uint_t GlobalEnable:1;
	uint_t OSReserved:3;
	uint_t PhysicalPageNumber:20;
} pte_t;

#define	pte32_t		pte_t

#define	LOAD_PTE(a, b)		(*(pteptr_t)(a) = b)
#define	UNLOAD_PTE(a, b)	(*(pteptr_t)(a) = b)
#define	INVALIDATE_PTE(a)	*(pteptr_t)(a) = MMU_STD_INVALIDPTE

#define	FOURMB_PDE_ATTR(a, g,  b, pati, ce, wt, c) \
	((((uint32_t)((uint_t)(a))) << MMU_STD_PAGESHIFT) |\
	((g) << 8) | 0x80 |(((b) & 0x03) << 1) | ((pati) << 12)|\
	((!(ce)) << 4) | ((wt) << 3) | (c))

#define	PTE_VALID	0x01
#define	PTE_LARGEPAGE	0x80
#define	PTE_SRWX	0x02
/*
 * The following defines MMU constants in 32 bit pte mode
 */
#define	NPTEPERPT	1024	/* entries in page table */
#define	NPTESHIFT	10
#define	PTSIZE		(NPTEPERPT * MMU_PAGESIZE)	/* bytes mapped */
#define	MMU_NPTE_ONE	1024 /* 1024 PTE's in first level table */
#define	MMU_NPTE_TWO	1024 /* 1024 PTE's in second level table */
#define	MMU_L1_VA(a)	(((uint_t)(a)) << 22)
#define	MMU_L1_INDEX(a) (((uint_t)(a)) >> 22)
#define	MMU_L2_INDEX(a) ((((uint_t)(a)) >> 12) & 0x3ff)
#define	PAGETABLE_INDEX(a)	MMU_L2_INDEX(a)
#define	PAGEDIR_INDEX(a)	MMU_L1_INDEX(a)
#define	PTE_PFN(p)		(((uint32_t)(p)) >> 12)
#define	PTEVAL2PFN(p)		((p) >> 12)
#define	MMU_L2_VA(a)		((a) << 12)
#define	four_mb_page(pte)	(*((uint_t *)(pte)) & 0x80)
#define	MMU_STD_INVALIDPTE	(0)
#define	PTOFFSET		(PTSIZE - 1)
#define	NPDPERAS		1	/* # of page dir pages for 4GB virt */
#define	MMU32_L1_INDEX 		MMU_L1_INDEX
#define	MMU32_L2_INDEX 		MMU_L2_INDEX
#define	pte32_valid		pte_valid

#define	LARGE_PAGESHIFT		22

#define	CAS_PTE(a, b, c) 	cas32(a, b, c)
#define	BOOT_SIZE		(0)
#define	PAGEDIR_SIZE		(MMU_PAGESIZE)
#define	LGPG_PDE		FOURMB_PDE_ATTR
#define	LARGE_PAGE		four_mb_page
#define	LARGEPAGESIZE		FOURMB_PAGESIZE
#define	PAGEDIRPTR_MAPSZ	0	/* 4GB as int */
	/* 4GB - 1 page, since we know it will all fit in one */

#define	ALIGN_TONEXT_LGPAGE(a) ((uint_t)((a) + \
		FOURMB_PAGESIZE) & ~FOURMB_PAGEOFFSET);

#endif	/* PTE36 */

/*
 * generate a pte for the specified page frame, with the
 * specified permissions, possibly cacheable.
 * (unsigned long long ((unsigned int)(ppn))) ensures that
 * the sign bit of ppn is not propagated during the shift
 * operation even if ppn is defined to be of type int.
 */

#define	MRPTEOF(ppn, os, m, r, c, w, a, p)	\
	((((pteval_t)((uint32_t)(ppn)))<<12)|\
	((os)<<9)|((m)<<6)|((r)<<5)|	\
	((!(c))<<4)|((w)<<3)|((a)<<1)|p)

#define	MRDPTEOF(ppn, os, m, r, pat_i, c, w, a, p)	\
	((((pteval_t)((uint32_t)(ppn)))<<12)|\
	((os)<<9)|((pat_i)<<7)|((m)<<6)|((r)<<5)|	\
	((!(c))<<4)|((w)<<3)|((a)<<1)|p)

#define	PTEOF(p, a, c)	MRPTEOF(p, 0, 0, 0, c, 0, a, 1)

#define	PTBL_ENT(p) (((pteval_t)((uint32_t)(p))<<12)|0x01)
#define	PTEOF_C(p, a)\
	(((pteval_t)((uint32_t)(p))<<12)|((a)<<1)|1)
#define	PTEOF_CS(p, a, s)\
	(((pteval_t)((uint32_t)(p))<<12)|((a)<<1)|((s) << 9)|1)
#define	MAKE_PFNUM(a)	(*((pteval_t *)(a)) >> 12)
#define	CHG_PFN(pteval, p) \
	((pteval) = ((pteval) & 0xFFF) | ((pteval_t)(p)) << 12)
#define	pte_konly(pte)		(((*(pteval_t *)(pte)) & 0x4) == 0)
#define	pte_ronly(pte)		(((*(pteval_t *)(pte)) & 0x2) == 0)
#define	pte_cacheable(pte)	(((*(pteval_t *)(pte)) & 0x10) == 0)
#define	pte_writethru(pte)	(((*(pteval_t *)(pte)) & 0x8) != 0)
#define	pte_pati(pte)		(((*(pteval_t *)(pte)) & 0x80) != 0)
#define	pte_accessed(pte)	((*(pteval_t *)(pte)) & 0x20)
#define	pte_dirty(pte)		((*(pteval_t *)(pte)) & 0x40)
#define	pte_accdir(pte)		((*(pteval_t *)(pte)) & 0x60)

/* manipulate S/W bits for indicating pte scanned in purge */
#define	setpte_scanned(a)	(*(a) |= (1 << 11))
#define	clrpte_scanned(a)	(*(a) &= ~(1 << 11))
#define	getpte_scanned(a)	(((*(a)) & (1 << 11)) != 0)
/* manipulate S/W bits for indicating LOAD_NOCONSIST */
#define	setpte_noconsist(a)	(*(a) |= (1 << 10))
#define	clrpte_noconsist(a)	(*(a) &= ~(1 << 10))
#define	getpte_noconsist(a)	(((*(a)) & (1 << 10)) != 0)
/* manipulate S/W bits for indicating LOAD_NOSYNC */
#define	setpte_nosync(a)	(*(a) |= (1 << 9))
#define	clrpte_nosync(a)	(*(a) &= ~(1 << 9))
#define	getpte_nosync(a)	(((*(a)) & (1 << 9)) != 0)

#define	pte_readonly(x)			(!(*((x)) & 0x02))
#define	pte_readonly_and_notdirty(x)	(!(*(x) & 0x42))
#define	ptevalue_dirty(a)		((a) & 0x40)


#define	mmu_pdeptr_cpu(cpu, addr) \
	((pteval_t *)(&((cpu)->cpu_pagedir[MMU_L1_INDEX(addr)])))
#define	mmu_pdeload_cpu(cpu, addr, value) \
	(*(pteval_t *)(mmu_pdeptr_cpu(cpu, addr)) = (pteval_t)(value))
#define	mmu_pdeptr(addr)		mmu_pdeptr_cpu(CPU, addr)
#define	mmu_pdeload(addr, value)	mmu_pdeload_cpu(CPU, addr, value)

#endif /* !_ASM */


#ifdef	__cplusplus
}
#endif

#endif /* !_IA32_SYS_PTE_H */
