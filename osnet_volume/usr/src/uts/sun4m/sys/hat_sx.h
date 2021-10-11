/*
 * Copyright (c) 1992,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This file defines data structures necessary for SX psuedo HAT layer.
 */

#ifndef	_SYS_HAT_SX_H
#define	_SYS_HAT_SX_H

#pragma ident	"@(#)hat_sx.h	1.13	98/01/06 SMI"

#include <sys/types.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/debug/debug.h>
#include <vm/seg.h>
#include <vm/hat.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * HAT mapping entries for SX are allocated one block at a time, with
 * each block containing 16 hments. Since there is one hment for each mapping
 * to a page and each hment blk has 16 hments, each hment block is used to
 * track 16 pages or 64k of virtual address space.
 * The total number of hment blocks allocated is a function of the
 * number of pagetables allocated by the SRMMU hat layer, which itself
 * is a function of the amount of physical memory in the system.
 * Every address space has a hash table which is used to find the hment for
 * a SXified virtual address backed by a page struct.
 */

#define	SX_HMENT_NBUCKETS	64	/* 64 buckets on the hashlist */
#define	SX_HMENT_BLKSIZE	16	/* Allocate 16 hme's at a time */

#define	SX_HMENT_BLKSHIFT	16	/* log2(PAGESIZE) + */
					/* log2(SX_HMENT_BLKSIZE) */
#define	SX_HMEBLK_HASH(va)	((((uint_t)(va) >> (uint_t)SX_HMENT_BLKSHIFT) \
						% (SX_HMENT_NBUCKETS - 1)))

#define	SX_HMENT_MASK		(0xffff0000)

/*
 *  This is the block of hme's themselves.
 *  Each hash bucket points to a linked list of (struct sx_hmentblk)'s.
 */
struct sx_hmentblk {
	caddr_t			sh_baseaddr;	/* Base virtual address */
						/* for hmegroup */
	struct sx_hmentblk	*sh_next;	/* next block in this bucket */
	struct sx_hmentblk	*sh_prev;	/* next block in this bucket */
	struct hment		*sh_hmebase;	/* First hme in the group */
	int			sh_validcnt;	/* delete block when == 0 */
};

/*
 * Original virtual address/SX virtual address, length information
 * For each of the virtual address range that is cloned for SX access, we
 * maintain two lists in the SX HAT private data. Each list contains identical
 * information i.e original virtual address, the corresponding cloned SX
 * virtual address and the size of the range being cloned for SX. We maintain
 * two lists to provide an easy key for a search criteria i.e one list is used
 * search for original virtual addresses and the other is used to search for
 * SX addresses. Two separate lists are maintained to avoid problems with
 * locking the lists, when elements of the list are deleted when translations
 * are being unloaded.
 */
struct sx_addr_memlist {

	caddr_t orig_vaddr;	/* starting original virtual address */
	caddr_t sx_vaddr;	/* starting SX virtual address */
	uint_t	size;		/* size of same */
	struct	sx_addr_memlist *next;	/* link to next list element */
	struct  sx_addr_memlist *prev;	/* link to prev list element */
};

struct sx_vaddr_memlist {

	caddr_t sx_vaddr;	/* starting SX virtual address */
	uint_t  size;		/* size of same */
	struct  sx_vaddr_memlist *next;	/* link to next list element */
	struct	sx_vaddr_memlist *prev;	/* link to prev list element */
};

/*
 * Per SX HAT private data
 */
struct sx_hat_data {
	struct as		*sx_mmu_as;	/* Containing address space */
	struct sx_hmentblk	*sx_mmu_hashtable; /* Hashtable for this as */
	struct sx_addr_memlist *sx_mmu_orig_memlist;
	    /* {addr,len} pairs denoting Original, SXified address ranges */
	struct sx_addr_memlist *sx_mmu_sx_memlist;
};

/*
 * Callback routine called from sx_mmu_unload() when unloading mappings
 * cloned for SX. The function pointer is initialized by the SX driver during
 * SX driver attach time.
 */
extern void (*sx_mmu_unmap_callback)(struct as *, caddr_t, uint_t);

extern void sx_mmu_add_vaddr(struct as *, caddr_t, caddr_t, uint_t);
extern void sx_mmu_del_vaddr(struct as *, caddr_t, caddr_t, uint_t, uint_t);

#define	ORIG_ADDR	0
#define	SX_ADDR 	1

/*
 * routine called to verify whether a page has SX mappings
 */
extern int sx_mmu_vrfy_sxpp(struct page *);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_HAT_SX_H */
