/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	Copyright (c) 1986-1990,1996-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef	_VM_SEG_MAP_H
#define	_VM_SEG_MAP_H

#pragma ident	"@(#)seg_map.h	1.44	99/07/16 SMI"
/*	From:	SVr4.0	"kernel:vm/seg_map.h	1.11"		*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * When segmap is created it is possible to program its behavior,
 *	using the create args [needed for performance reasons].
 * Segmap creates n lists of pages.
 *	For VAC machines, there will be at least one free list
 *	per color. If more than one free list per color is needed,
 *	set nfreelist as needed.
 *
 *	For PAC machines, it will be treated as VAC with only one
 *	color- every page is of the same color. Again, set nfreelist
 *	to get more than one free list.
 */
struct segmap_crargs {
	uint_t	prot;
	uint_t	shmsize;	/* shm_alignment for VAC, 0 for PAC. */
	uint_t	nfreelist;	/* number of freelist per color, >= 1 */
};

/*
 * Each smap struct represents a MAXBSIZE sized mapping to the
 * <sm_vp, sm_off> given in the structure.  The location of the
 * the structure in the array gives the virtual address of the
 * mapping. Structure rearranged for 64bit sm_off.
 */
struct	smap {
	struct	vnode	*sm_vp;		/* vnode pointer (if mapped) */

	/*
	 * These next 3 entries can be coded as
	 * ushort_t's if we are tight on memory.
	 */
	struct	smap	*sm_hash;	/* hash pointer */
	struct	smap	*sm_next;	/* next pointer */
	struct	smap	*sm_prev;	/* previous pointer */
	u_offset_t	sm_off;		/* file offset for mapping */

	ushort_t	sm_bitmap;	/* bit map for locked translations */
	ushort_t	sm_refcnt;	/* reference count for uses */
};

struct	smfree {
	struct	smap	*sm_free;	/* free list array pointer */
	kmutex_t	sm_mtx;	/* protects smap data of this color */
	kcondvar_t	sm_free_cv;
	ushort_t	sm_want;	/* someone wants a slot of this color */
};

/*
 * (Semi) private data maintained by the segmap driver per SEGMENT mapping
 * All fields in segmap_data are read-only after the segment is created.
 *
 */

struct	segmap_data {
	struct	smap	*smd_sm;	/* array of smap structures */
	struct  smfree	*smd_free;	/* array of separately locked colors */
	struct  smap	**smd_hash;	/* ptr to hash header array */
	short		smd_nfree;	/* number of colors */
	uchar_t		smd_prot;	/* protections for all smap's */
};

/*
 * Statistics for segmap operations.
 *
 * No explicit locking to protect these stats.
 */
struct segmapcnt {
	kstat_named_t	smp_fault;	/* number of segmap_faults */
	kstat_named_t	smp_faulta;	/* number of segmap_faultas */
	kstat_named_t	smp_getmap;	/* number of segmap_getmaps */
	kstat_named_t	smp_get_use;	/* getmaps that reuse existing map */
	kstat_named_t	smp_get_reclaim; /* getmaps that do a reclaim */
	kstat_named_t	smp_get_reuse;	/* getmaps that reuse a slot */
	kstat_named_t	smp_get_unused;	/* getmaps that reuse existing map */
	kstat_named_t	smp_get_nofree;	/* getmaps with no free slots */
	kstat_named_t	smp_rel_async;	/* releases that are async */
	kstat_named_t	smp_rel_write;	/* releases that write */
	kstat_named_t	smp_rel_free;	/* releases that free */
	kstat_named_t	smp_rel_abort;	/* releases that abort */
	kstat_named_t	smp_rel_dontneed; /* releases with dontneed set */
	kstat_named_t	smp_release;	/* releases with no other action */
	kstat_named_t	smp_pagecreate;	/* pagecreates */
	kstat_named_t   smp_free_notfree; /* pages not freed in */
					/* segmap_pagefree */
	kstat_named_t   smp_free_dirty; /* dirty pages freeed */
					/* in segmap_pagefree */
	kstat_named_t   smp_free;	/* clean pages freeed in */
					/* segmap_pagefree */
};

/*
 * These are flags used on release.  Some of these might get handled
 * by segment operations needed for msync (when we figure them out).
 * SM_ASYNC modifies SM_WRITE.  SM_DONTNEED modifies SM_FREE.  SM_FREE
 * and SM_INVAL are mutually exclusive.
 */
#define	SM_WRITE	0x01		/* write back the pages upon release */
#define	SM_ASYNC	0x02		/* do the write asynchronously */
#define	SM_FREE		0x04		/* put pages back on free list */
#define	SM_INVAL	0x08		/* invalidate page (no caching) */
#define	SM_DONTNEED	0x10		/* less likely to be needed soon */

#define	MAXBSHIFT	13		/* log2(MAXBSIZE) */

#define	MAXBOFFSET	(MAXBSIZE - 1)
#define	MAXBMASK	(~MAXBOFFSET)

/*
 * SMAP_HASHAVELEN is the average length desired for this chain, from
 * which the size of the smd_hash table is derived at segment create time.
 * SMAP_HASHVPSHIFT is defined so that 1 << SMAP_HASHVPSHIFT is the
 * approximate size of a vnode struct.
 */
#define	SMAP_HASHAVELEN		4
#define	SMAP_HASHVPSHIFT	6


#ifdef _KERNEL
/*
 * The kernel generic mapping segment.
 */
extern struct seg *segkmap;

/*
 * Public seg_map segment operations.
 */
extern int	segmap_create(struct seg *, void *);
extern int	segmap_pagecreate(struct seg *, caddr_t, size_t, int);
extern void	segmap_pageunlock(struct seg *, caddr_t, size_t, enum seg_rw);
extern faultcode_t segmap_fault(struct hat *, struct seg *, caddr_t, size_t,
		enum fault_type, enum seg_rw);
extern caddr_t	segmap_getmap(struct seg *, struct vnode *, u_offset_t);
extern caddr_t	segmap_getmapflt(struct seg *, struct vnode *, u_offset_t,
		size_t, int, enum seg_rw);
extern int	segmap_release(struct seg *, caddr_t, uint_t);
extern void	segmap_flush(struct seg *, struct vnode *);
extern void	segmap_inval(struct seg *, struct vnode *, u_offset_t);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_SEG_MAP_H */
