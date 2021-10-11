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
 *	Copyright (c) 1986-1990,1995-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef	_VM_SEG_H
#define	_VM_SEG_H

#pragma ident	"@(#)seg.h	1.59	99/05/04 SMI"
/*	From:	SVr4.0	"kernel:vm/seg.h	1.10"		*/

#include <sys/vnode.h>
#include <vm/seg_enum.h>
#include <vm/faultcode.h>
#include <vm/hat.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * VM - Segments.
 */

/*
 * Segment skiplist parameters.
 *
 * SSL_BFACTOR must be a power of 2, and
 * SSL_BFACTOR * SSL_NLEVELS must be <= 32
 * or ssl_random() can run out of bits.
 */

#define	SSL_NLEVELS		4	/* Number of levels in segs_skiplist */
#define	SSL_BFACTOR		4	/* Branch factor in skip list */
#define	SSL_LOG2BF		2	/* log2 of SSL_BFACTOR */
#define	SSL_UNUSED		((struct seg *)-1)

typedef struct {
	struct seg *segs[SSL_NLEVELS];	/* head/next in a segs skiplist */
} seg_skiplist;

typedef struct {
	seg_skiplist *ssls[SSL_NLEVELS];	/* an ssl search path */
} ssl_spath;

typedef union {
	struct seg	*list;		/* "next" of simple linked list */
	seg_skiplist	*skiplist;	/* "next" of segment skiplist */
} seg_next;

/*
 * kstat statistics for segment pagelock cache
 */
typedef struct {
	kstat_named_t cache_hit;
	kstat_named_t cache_miss;
	kstat_named_t active_pages;
	kstat_named_t cached_pages;
	kstat_named_t purge_count;
} segplckstat_t;

/*
 * kstat statistics for segment advise
 */
typedef struct {
	kstat_named_t MADV_FREE_hit;
	kstat_named_t MADV_FREE_miss;
} segadvstat_t;

/*
 * memory object ids
 */
typedef struct memid { u_longlong_t val[2]; } memid_t;

/*
 * An address space contains a set of segments, managed by drivers.
 * Drivers support mapped devices, sharing, copy-on-write, etc.
 *
 * The seg structure contains a lock to prevent races, the base virtual
 * address and size of the segment, a back pointer to the containing
 * address space, pointers to maintain a circularly doubly linked list
 * of segments in the same address space, and procedure and data hooks
 * for the driver.  The seg list on the address space is sorted by
 * ascending base addresses and overlapping segments are not allowed.
 *
 * After a segment is created, faults may occur on pages of the segment.
 * When a fault occurs, the fault handling code must get the desired
 * object and set up the hardware translation to the object.  For some
 * objects, the fault handling code also implements copy-on-write.
 *
 * When the hat wants to unload a translation, it can call the unload
 * routine which is responsible for processing reference and modify bits.
 *
 * Each segment is protected by it's containing address space lock.  To
 * access any field in the segment structure, the "as" must be locked.
 * If a segment field is to be modified, the address space lock must be
 * write locked.
 */

struct seg {
	caddr_t	s_base;			/* base virtual address */
	size_t	s_size;			/* size in bytes */
	struct	as *s_as;		/* containing address space */
	seg_next s_next;		/* next seg in this address space */
	struct	seg *s_prev;		/* prev seg in this address space */
	struct	seg_ops *s_ops;		/* ops vector: see below */
	void *s_data;			/* private data for instance */
};

struct	seg_ops {
	int	(*dup)(struct seg *, struct seg *);
	int	(*unmap)(struct seg *, caddr_t, size_t);
	void	(*free)(struct seg *);
	faultcode_t (*fault)(struct hat *, struct seg *, caddr_t, size_t,
	    enum fault_type, enum seg_rw);
	faultcode_t (*faulta)(struct seg *, caddr_t);
	int	(*setprot)(struct seg *, caddr_t, size_t, uint_t);
	int	(*checkprot)(struct seg *, caddr_t, size_t, uint_t);
	int	(*kluster)(struct seg *, caddr_t, ssize_t);
	size_t	(*swapout)(struct seg *);
	int	(*sync)(struct seg *, caddr_t, size_t, int, uint_t);
	size_t	(*incore)(struct seg *, caddr_t, size_t, char *);
	int	(*lockop)(struct seg *, caddr_t, size_t, int, int, ulong_t *,
			size_t);
	int	(*getprot)(struct seg *, caddr_t, size_t, uint_t *);
	u_offset_t	(*getoffset)(struct seg *, caddr_t);
	int	(*gettype)(struct seg *, caddr_t);
	int	(*getvp)(struct seg *, caddr_t, struct vnode **);
	int	(*advise)(struct seg *, caddr_t, size_t, uint_t);
	void	(*dump)(struct seg *);
	int	(*pagelock)(struct seg *, caddr_t, size_t, struct page ***,
			enum lock_type, enum seg_rw);
	int	(*getmemid)(struct seg *, caddr_t, memid_t *);
};

#ifdef _KERNEL
/*
 * Generic segment operations
 */
extern	void	seg_init(void);
extern	struct	seg *seg_alloc(struct as *as, caddr_t base, size_t size);
extern	int	seg_attach(struct as *as, caddr_t base, size_t size,
			struct seg *seg);
extern	void	seg_unmap(struct seg *seg);
extern	void	seg_free(struct seg *seg);

/*
 * functions for pagelock cache support
 */
extern	void	seg_ppurge(struct seg *seg);
extern	void	seg_pinactive(struct seg *seg, caddr_t addr, size_t len,
			struct page **pp, enum seg_rw rw, void (*callback)());
extern	int	seg_pinsert(struct seg *seg, caddr_t addr, size_t len,
			struct page **pp, enum seg_rw rw, uint_t flags,
			void (*callback)());
extern	struct	page **seg_plookup(struct seg *seg, caddr_t addr,
			size_t len, enum seg_rw rw);
extern	void	seg_pasync_thread(void);
extern	void	seg_preap(void);

extern	int	seg_preapahead;
extern	segplckstat_t segplckstat;
extern	segadvstat_t  segadvstat;
/*
 * Flags for pagelock cache support
 */
#define	SEGP_ASYNC_FLUSH	0x1	/* flushed by async thread */
#define	SEGP_FORCE_WIRED	0x2	/* skip check against seg_pwindow */

/*
 * Return values for seg_pinsert function.
 */
#define	SEGP_SUCCESS		0	/* seg_pinsert() succeeded */
#define	SEGP_FAIL		1	/* seg_pinsert() failed */

#define	SEGOP_DUP(s, n)		    (*(s)->s_ops->dup)((s), (n))
#define	SEGOP_UNMAP(s, a, l)	    (*(s)->s_ops->unmap)((s), (a), (l))
#define	SEGOP_FREE(s)		    (*(s)->s_ops->free)((s))
#define	SEGOP_FAULT(h, s, a, l, t, rw) \
		(*(s)->s_ops->fault)((h), (s), (a), (l), (t), (rw))
#define	SEGOP_FAULTA(s, a)	    (*(s)->s_ops->faulta)((s), (a))
#define	SEGOP_SETPROT(s, a, l, p)   (*(s)->s_ops->setprot)((s), (a), (l), (p))
#define	SEGOP_CHECKPROT(s, a, l, p) (*(s)->s_ops->checkprot)((s), (a), (l), (p))
#define	SEGOP_KLUSTER(s, a, d)	    (*(s)->s_ops->kluster)((s), (a), (d))
#define	SEGOP_SWAPOUT(s)	    (*(s)->s_ops->swapout)((s))
#define	SEGOP_SYNC(s, a, l, atr, f) \
		(*(s)->s_ops->sync)((s), (a), (l), (atr), (f))
#define	SEGOP_INCORE(s, a, l, v)    (*(s)->s_ops->incore)((s), (a), (l), (v))
#define	SEGOP_LOCKOP(s, a, l, atr, op, b, p) \
		(*(s)->s_ops->lockop)((s), (a), (l), (atr), (op), (b), (p))
#define	SEGOP_GETPROT(s, a, l, p)   (*(s)->s_ops->getprot)((s), (a), (l), (p))
#define	SEGOP_GETOFFSET(s, a)	    (*(s)->s_ops->getoffset)((s), (a))
#define	SEGOP_GETTYPE(s, a)	    (*(s)->s_ops->gettype)((s), (a))
#define	SEGOP_GETVP(s, a, vpp)	    (*(s)->s_ops->getvp)((s), (a), (vpp))
#define	SEGOP_ADVISE(s, a, l, b)    (*(s)->s_ops->advise)((s), (a), (l), (b))
#define	SEGOP_DUMP(s)		    (*(s)->s_ops->dump)((s))
#define	SEGOP_PAGELOCK(s, a, l, p, t, rw) \
		(*(s)->s_ops->pagelock)((s), (a), (l), (p), (t), (rw))
#define	SEGOP_GETMEMID(s, a, mp)    (*(s)->s_ops->getmemid)((s), (a), (mp))

#define	seg_page(seg, addr) \
	(((uintptr_t)((addr) - (seg)->s_base)) >> PAGESHIFT)

#define	seg_pages(seg) \
	(((uintptr_t)((seg)->s_size + PAGEOFFSET)) >> PAGESHIFT)

#define	IE_NOMEM	-1	/* internal to seg layer */

#ifdef VMDEBUG

uint_t	seg_page(struct seg *, caddr_t);
uint_t	seg_pages(struct seg *);

#endif	/* VMDEBUG */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_SEG_H */
