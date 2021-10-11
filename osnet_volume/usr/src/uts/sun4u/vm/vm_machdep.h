/*
 * Copyright (c) 1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * UNIX machine dependent virtual memory support.
 */

#ifndef	_VM_MACHDEP_H
#define	_VM_MACHDEP_H

#pragma ident	"@(#)vm_machdep.h	1.1	99/01/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Per page size free lists. Allocated dynamically.
 */
#define	MAX_MEM_TYPES	2	/* 0 = reloc, 1 = noreloc */
#define	MTYPE_RELOC	0
#define	MTYPE_NORELOC	1

#define	PP_2_MTYPE(pp)	(PP_ISNORELOC(MACHPP2PP(pp)) ?	\
			MTYPE_NORELOC : MTYPE_RELOC)

extern page_t **page_freelists[MAX_MEM_NODES][MMU_PAGE_SIZES][MAX_MEM_TYPES];

#define	PAGE_FREELISTS(mnode, pgsz, color, mtype) \
	((*(page_freelists[mnode][pgsz][mtype] + (color))))

/*
 * For now there is only a single size cache list.
 * Allocated dynamically.
 */
extern	page_t **page_cachelists[MAX_MEM_NODES][MAX_MEM_TYPES];

#define	PAGE_CACHELISTS(mnode, color, mtype) \
	((*(page_cachelists[mnode][mtype] + (color))))

/*
 * There are at most 512 colors/bins.  Spread them out under a
 * couple of locks.  There are mutexes for both the page freelist
 * and the page cachelist.  We want enough locks to make contention
 * reasonable, but not too many -- otherwise page_list_lock() gets
 * so expensive that it becomes the bottleneck!
 */
#define	NPC_MUTEX	16

extern kmutex_t	fpc_mutex[MAX_MEM_NODES][NPC_MUTEX];
extern kmutex_t	cpc_mutex[MAX_MEM_NODES][NPC_MUTEX];

#define	PP_2_BIN(pp)							\
	(((pp->p_pagenum) & page_colors_mask) >>			\
	(hw_page_array[pp->p_cons].shift - hw_page_array[0].shift))

#define	PP_2_MEM_NODE(pp)	(PFN_2_MEM_NODE((pp)->p_pagenum))

#define	PC_BIN_MUTEX(mnode, bin, list) ((list == PG_FREE_LIST) ?	\
	&fpc_mutex[mnode][(bin) & (NPC_MUTEX - 1)] :			\
	&cpc_mutex[mnode][(bin) & (NPC_MUTEX - 1)])

/*
 * Platform specific page routines
 */
extern void mach_page_add(page_t **, page_t *);
extern void mach_page_sub(page_t **, page_t *);
extern uint_t page_get_pagecolors(uint_t szc);

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_MACHDEP_H */
