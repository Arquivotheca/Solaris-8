/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ifndef	_SYS_MEMCTL_H
#define	_SYS_MEMCTL_H

#pragma ident	"@(#)memctl.h	1.4	93/05/29 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

/*
 * The maximum page number of an EMC memory controller
 */

#define	EMC_MAX_PFN	0x20000

/*
 * Macros for converting physical addresses to slot numbers for an
 * EMC or SMC memory controller
 */

#define	PMEM_NUMSLOTS	8		/* no. slots of  C2 memory */
#define	PMEM_SLOTSHIFT	26	/* size in bits of slot part of address */
#define	PMEM_PFNSHIFT	(PMEM_SLOTSHIFT - MMU_PAGESHIFT)
#define	PMEM_SLOT(addr)	((addr >> PMEM_SLOTSHIFT) & (PMEM_NUMSLOTS - 1))
#define	PMEM_PFN_SLOT(pfn) ((pfn >> PMEM_PFNSHIFT) & (PMEM_NUMSLOTS - 1))

#ifndef	_ASM

/*
 * Struct used to record async fault handlers that drivers have registered
 * and type of memory in each slot (EMC or SMC only)
 */
struct memslot {
	int ms_bustype;			/* BT_DRAM or BT_NVRAM for now */
	void *ms_fault_specific;	/* varies by fault type */
					/* AFLT_ECC: address of bat lo reg */
	struct dev_info *ms_dip;	/* driver */
	void *ms_arg;			/* arg to pass to ms_func */
	int (*ms_func)(void *, void *);	/* fault handler */
};

/* mc_type defines */

enum mc_type {
	MC_MMC, MC_EMC, MC_SMC
};

extern enum mc_type mc_type;
extern enum mc_type memctl_type(void);

#endif	/* !_ASM */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MEMCTL_H */
