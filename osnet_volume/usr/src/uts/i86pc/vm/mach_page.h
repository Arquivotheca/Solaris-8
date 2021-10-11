/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _MACH_PAGE_H
#define	_MACH_PAGE_H

#pragma ident	"@(#)mach_page.h	1.12	99/06/02 SMI"

#include <vm/page.h>
/*
 * The file contains the platform specific page structure
 */

#ifdef __cplusplus
extern "C" {
#endif


#ifdef	NUMA

/*
 * p_flag definition
 *
 * 31 ........ 16-15 .........4-3.........0
 * |flag values  | reserved    | node id |
 *
 * The lower 4 bits of p_flag contain the nodeid.
 * The upper 16 bits hold the bitmask of nodes on which this
 * page has not been replicated yet. p_flag is protected
 * by the p_inuse bit.
 *
 */

/* p_flag values */
#define	P_NODEMASK	0xf
#define	P_REPLICATE	0xffff0000
					/*
					 * bit mask of nodes that need
					 * replicated page
					 */
#define	P_NODESHIFT	16

#ifdef NUMA

#define	p_mapping	p_mapinfo->hmep
#define	p_shadow	p_mapinfo->shadow /* A linked list of local pages */
#define	p_flag		p_mapinfo->flag	/* lower 4 bits hold node-id */

#endif

typedef struct ppmap {
	struct hment	*hmep;
	struct page	*shadow;
	uint32_t	flag;
} ppmap_t;
#endif
/*
 * The PSM portion of a page structure
 */
typedef struct machpage {
	page_t	p_paget;		/* PIM portion of page_t */
	struct	hment *p_mapping;	/* hat specific translation info */
	uint_t	p_pagenum;		/* physical page number */
	uint_t	p_share;		/* number of mappings to this page */
	ushort_t p_deleted;		/* # of hmes pointing to invalid ptes */
	uchar_t	p_flags;		/* See bits defined below */
	uchar_t	p_nrm;			/* non-cache, ref, mod readonly bits */
	void 	*p_resv;		/* reserved for future use */
	/* p_nrm is to be operated with atomic operations */
} machpage_t;

#define	P_MLISTLOCKED	1		/* p_mapping, share & deleted locked */
#define	P_MLISTWAIT	2		/* waiting for above lock */
#define	P_LARGEPAGE	4		/* first page of a large page array */
#define	P_KVSEGPAGE	8		/* Page below 4GB for kvseg */

/*
 * Each segment of physical memory is described by a memseg struct. Within
 * a segment, memory is considered contiguous. The segments form a linked
 * list to describe all of physical memory.
 */
struct memseg {
	machpage_t *pages, *epages;	/* [from, to] in page array */
	uint_t pages_base, pages_end;	/* [from, to] in page numbers */
	struct memseg *next;		/* next segment in list */
};

extern struct memseg *memsegs;		/* list of memory segments */

void build_pfn_hash();

#ifdef __cplusplus
}
#endif

#endif /* _MACH_PAGE_H */
