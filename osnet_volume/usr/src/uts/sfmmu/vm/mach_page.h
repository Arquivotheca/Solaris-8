/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _MACH_PAGE_H
#define	_MACH_PAGE_H

#pragma ident	"@(#)mach_page.h	1.14	99/12/12 SMI"

/*
 * The file contains the platform specific page structure
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The PSM portion of a page structure
 */
typedef struct machpage {
	page_t	p_paget;		/* PIM portion of page_t */
	struct	sf_hment *p_mapping;	/* hat specific translation info */
	uint_t	p_pagenum;		/* physical page number */
	uchar_t	p_nrm;			/* non-cache, ref, mod readonly bits */
	uchar_t	p_vcolor;		/* virtual color */
	uchar_t	p_index;		/* mapping index */
	uchar_t	p_cons: 2,		/* constituent page size */
		p_conslist: 1,		/* on constituent staging list */
		p_filler: 5;		/* unused at this time */
	uint_t	p_share;		/* number of mappings to this page */
} machpage_t;

#define	PP2MACHPP(gen_pp)	((struct machpage *)(gen_pp))
#define	MACHPP2PP(mach_pp)	((struct page *)(mach_pp))
#define	genp_vpnext		p_paget.p_vpnext
#define	genp_vpprev		p_paget.p_vpprev
#define	genp_next		p_paget.p_next
#define	genp_prev		p_paget.p_prev
#define	genp_offset		p_paget.p_offset
#define	genp_vnode		p_paget.p_vnode

/*
 * Each segment of physical memory is described by a memseg struct. Within
 * a segment, memory is considered contiguous. The segments from a linked
 * list to describe all of physical memory.
 */
struct memseg {
	machpage_t *pages, *epages;	/* [from, to] in page array */
	pfn_t pages_base, pages_end;	/* [from, to] in page numbers */
	struct memseg *next;		/* next segment in list */
	struct memseg *lnext;		/* next segment in deleted list */
};

#define	MSEG_NPAGES(SEG)	((SEG)->pages_end - (SEG)->pages_base)

extern struct memseg *memsegs;		/* list of memory segments */
void build_pfn_hash();

#ifdef __cplusplus
}
#endif

#endif /* _MACH_PAGE_H */
