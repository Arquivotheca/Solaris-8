/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_IA32_SYS_MMU_H
#define	_IA32_SYS_MMU_H

#pragma ident	"@(#)mmu.h	1.26	99/05/12 SMI"

#ifndef _ASM
#include <sys/pte.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Definitions for the Intel 80x86 MMU
 */

/*
 * Page fault error code, pushed onto stack on page fault exception
 */
#define	MMU_PFEC_P		0x1	/* Page present */
#define	MMU_PFEC_WRITE		0x2	/* Write access */
#define	MMU_PFEC_USER		0x4	/* User mode access */

/* Access types based on above error codes */
#define	MMU_PFEC_AT_MASK	(MMU_PFEC_USER|MMU_PFEC_WRITE)
#define	MMU_PFEC_AT_UREAD	MMU_PFEC_USER
#define	MMU_PFEC_AT_UWRITE	(MMU_PFEC_USER|MMU_PFEC_WRITE)
#define	MMU_PFEC_AT_SREAD	0
#define	MMU_PFEC_AT_SWRITE	MMU_PFEC_WRITE

#define	MMU_STD_SRX		0
#define	MMU_STD_SRWX		1
#define	MMU_STD_SRXURX		2
#define	MMU_STD_SRWXURWX	3

#if defined(_KERNEL) && !defined(_ASM)

extern int valid_va_range(caddr_t *, size_t *, size_t, int);

#endif /* defined(_KERNEL) && !defined(_ASM) */

/*
 * Page directory and physical page parameters
 */
#ifndef MMU_PAGESIZE
#define	MMU_PAGESIZE	4096
#endif



#define	MMU_STD_PAGESIZE	MMU_PAGESIZE
#define	MMU_STD_PAGEMASK	0xFFFFF000
#define	MMU_STD_PAGESHIFT	12
#define	MMU_STD_SEGSHIFT	22



/* ### also in pte.h */

#define	TWOMB_PAGESIZE		0x200000
#define	TWOMB_PAGEOFFSET	(TWOMB_PAGESIZE - 1)
#define	TWOMB_PAGESHIFT		21
#define	FOURMB_PAGESIZE		0x400000
#define	FOURMB_PAGEOFFSET	(FOURMB_PAGESIZE - 1)
#define	FOURMB_PAGESHIFT	22
#define	FOURMB_PAGEMASK		(~FOURMB_PAGEOFFSET)

#define	HAT_INVLDPFNUM		0xffffffff

#define	IN_SAME_4MB_PAGE(a, b)	(MMU_L1_INDEX(a)  ==  MMU_L1_INDEX(b))
#define	FOURMB_PDE(a, g,  b, c) \
	((((uint32_t)((uint_t)(a))) << MMU_STD_PAGESHIFT) |\
	((g) << 8) | PTE_LARGEPAGE |(((b) & 0x03) << 1) | (c))

#ifndef _ASM
#define	mmu_tlbflush_all()	reload_cr3()

/* Low-level functions */
extern void mmu_tlbflush_entry(caddr_t);
extern uint_t cr3(void);
extern void reload_cr3(void);
extern void setcr3(uint_t);
extern void mmu_loadcr3(struct cpu *, void *);
extern void setup_kernel_page_directory(struct cpu *);
extern void setup_vaddr_for_ppcopy(struct cpu *);
extern void clear_bootpde(struct cpu *);
#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif /* _IA32_SYS_MMU_H */
