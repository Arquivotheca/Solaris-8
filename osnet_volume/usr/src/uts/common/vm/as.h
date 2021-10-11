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
 *	Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef	_VM_AS_H
#define	_VM_AS_H

#pragma ident	"@(#)as.h	1.69	99/08/31 SMI"

#include <sys/watchpoint.h>
#include <vm/seg.h>
#include <vm/faultcode.h>
#include <vm/hat.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * VM - Address spaces.
 */

/*
 * Each address space consists of a sorted list of segments
 * and machine dependent address translation information.
 *
 * All the hard work is in the segment drivers and the
 * hardware address translation code.
 *
 * The segment list can be represented as a simple linked list or as a
 * skiplist.  The skiplist facilitates faster searches when there are
 * many segments.  A linked list is used until a_nsegs grows beyond
 * AS_MUTATION_THRESH.  At that point, the linked list mutates into a
 * skiplist.  a_lrep indicates the list's current representation.
 *
 * The address space lock (a_lock) is a long term lock which serializes
 * access to certain operations (as_map, as_unmap) and protects the
 * underlying generic segment data (seg.h) along with some fields in the
 * address space structure as shown below:
 *
 *	address space structure 	segment structure
 *
 *	a_segs				s_base
 *	a_size				s_size
 *	a_nsegs				s_as
 *	a_hilevel			s_next
 *	a_cache				s_prev
 *	a_lrep				s_ops
 *	a_tail				s_data
 *
 * The address space contents lock (a_contents) is a short term
 * lock that protects most of the data in the address space structure.
 * This lock is always acquired after the "a_lock" in all situations
 * except while dealing with a_claimgap to avoid deadlocks.
 *
 * The following fields are protected by this lock:
 *
 *	a_paglck
 *	a_claimgap
 *	a_unmapwait
 *	a_cache
 *
 * Note that a_cache can be updated while a_lock is held for read, but
 * when the segment list representation is a skiplist (a_lrep ==
 * AS_LREP_SKIPLIST), updates to (*a_cache.spath) are protected by the
 * mutex lock a_contents.  This ensures the atomicity of the update,
 * and guarantees internal consistency of spath's contents.  During a
 * skiplist insertion or deletion, however, a_cache is also protected
 * by a_lock being held for write.
 *
 * The address space lock (a_lock) is always held prior to any segment
 * operation.  Some segment drivers use the address space lock to protect
 * some or all of their segment private data, provided the version of
 * "a_lock" (read vs. write) is consistent with the use of the data.
 *
 * The following fields are protected by the hat layer lock:
 *
 *	a_vbits
 *	a_hat
 *	a_hrm
 */
struct as {
	kmutex_t a_contents;	/* protect certain fields in the structure */
	uchar_t  a_flags;	/* as attributes */
	uchar_t	a_vbits;	/* used for collecting statistics */
	kcondvar_t a_cv;	/* used by as_rangelock */
	struct	hat *a_hat;	/* hat structure */
	struct	hrmstat *a_hrm; /* ref and mod bits */
	caddr_t	a_userlimit;	/* highest allowable address in this as */
	union {
		struct seg *seglast;	/* last segment hit on the addr space */
		ssl_spath *spath;	/* last search path in seg skiplist */
	} a_cache;
	krwlock_t a_lock;	/* protects fields below + a_cache */
	int	a_nwpage;	/* number of watched pages */
	struct watched_page *a_wpage;	/* list of watched pages (procfs) */
	seg_next a_segs;	/* segments in this address space. */
	size_t	a_size;		/* size of address space */
	struct 	seg *a_tail;	/* last element in the segment list. */
	uint_t	a_nsegs;	/* number of elements in segment list */
	uchar_t	a_lrep;		/* representation of a_segs: see #defines */
	uchar_t	a_hilevel;	/* highest level in the a_segs skiplist */
	uchar_t	a_unused;
	uchar_t	a_updatedir;	/* mappings changed, rebuild as_objectdir */
	vnode_t	**a_objectdir;	/* object directory (procfs) */
	size_t	a_sizedir;	/* size of object directory */
};

#define	AS_PAGLCK		0x80
#define	AS_CLAIMGAP		0x40
#define	AS_UNMAPWAIT		0x20

#define	AS_ISPGLCK(as)		((as)->a_flags & AS_PAGLCK)
#define	AS_ISCLAIMGAP(as)	((as)->a_flags & AS_CLAIMGAP)
#define	AS_ISUNMAPWAIT(as)	((as)->a_flags & AS_UNMAPWAIT)

#define	AS_SETPGLCK(as)		((as)->a_flags |= AS_PAGLCK)
#define	AS_SETCLAIMGAP(as)	((as)->a_flags |= AS_CLAIMGAP)
#define	AS_SETUNMAPWAIT(as)	((as)->a_flags |= AS_UNMAPWAIT)

#define	AS_CLRPGLCK(as)		((as)->a_flags &= ~AS_PAGLCK)
#define	AS_CLRCLAIMGAP(as)	((as)->a_flags &= ~AS_CLAIMGAP)
#define	AS_CLRUNMAPWAIT(as)	((as)->a_flags &= ~AS_UNMAPWAIT)

#define	AS_TYPE_64BIT(as)	\
	    (((as)->a_userlimit > (caddr_t)UINT32_MAX) ? 1 : 0)

/*
 * List representations; values for a_listrep
 */
#define	AS_LREP_LINKEDLIST	0
#define	AS_LREP_SKIPLIST	1

#ifdef _KERNEL

/*
 * Segment list representation mutation threshold.
 */
#define	AS_MUTATION_THRESH	225

/*
 * Macro to turn a (seg_next) into a (seg *)
 */
#define	AS_SEGP(as, n)	((as)->a_lrep == AS_LREP_LINKEDLIST ? \
			    ((n).list) : ((n).skiplist->segs[0]))

/*
 * Flags for as_gap.
 */
#define	AH_DIR		0x1	/* direction flag mask */
#define	AH_LO		0x0	/* find lowest hole */
#define	AH_HI		0x1	/* find highest hole */
#define	AH_CONTAIN	0x2	/* hole must contain `addr' */

extern struct as kas;		/* kernel's address space */

/*
 * Macros for address space locking.
 */
#define	AS_LOCK_ENTER(as, lock, type)		rw_enter((lock), (type))
#define	AS_LOCK_EXIT(as, lock)			rw_exit((lock))
#define	AS_LOCK_DESTROY(as, lock)		rw_destroy((lock))
#define	AS_LOCK_TRYENTER(as, lock, type)	rw_tryenter((lock), (type))

/*
 * Macros to test lock states.
 */
#define	AS_LOCK_HELD(as, lock)		RW_LOCK_HELD((lock))
#define	AS_READ_HELD(as, lock)		RW_READ_HELD((lock))
#define	AS_WRITE_HELD(as, lock)		RW_WRITE_HELD((lock))

void	as_init(void);
struct	seg *as_segat(struct as *as, caddr_t addr);
void	as_rangelock(struct as *as);
void	as_rangeunlock(struct as *as);
struct	as *as_alloc(void);
void	as_free(struct as *as);
int	as_dup(struct as *as, struct as **outas);
struct	seg *as_findseg(struct as *as, caddr_t addr, int tail);
int	as_addseg(struct as *as, struct seg *newseg);
struct	seg *as_removeseg(struct as *as, caddr_t addr);
faultcode_t as_fault(struct hat *hat, struct as *as, caddr_t addr, size_t size,
		enum fault_type type, enum seg_rw rw);
faultcode_t as_faulta(struct as *as, caddr_t addr, size_t size);
int	as_setprot(struct as *as, caddr_t addr, size_t size, uint_t prot);
int	as_checkprot(struct as *as, caddr_t addr, size_t size, uint_t prot);
int	as_unmap(struct as *as, caddr_t addr, size_t size);
int	as_map(struct as *as, caddr_t addr, size_t size, int ((*crfp)()),
		void *argsp);
int	as_gap(struct as *as, size_t minlen, caddr_t *basep, size_t *lenp,
		uint_t flags, caddr_t addr);
int	as_memory(struct as *as, caddr_t *basep, size_t *lenp);
size_t	as_swapout(struct as *as);
int	as_incore(struct as *as, caddr_t addr, size_t size, char *vec,
		size_t *sizep);
int	as_ctl(struct as *as, caddr_t addr, size_t size, int func, int attr,
		uintptr_t arg, ulong_t *lock_map, size_t pos);
int	as_exec(struct as *oas, caddr_t ostka, size_t stksz,
		struct as *nas, caddr_t nstka, uint_t hatflag);
int	as_pagelock(struct as *as, struct page ***ppp, caddr_t addr,
		size_t size, enum seg_rw rw);
void	as_pageunlock(struct as *as, struct page **pp, caddr_t addr,
		size_t size, enum seg_rw rw);
void	as_pagereclaim(struct as *as, struct page **pp, caddr_t addr,
		size_t size, enum seg_rw rw);
faultcode_t	as_pageflip(struct  as *as_to, caddr_t addr_to, caddr_t kaddr,
			    size_t *sizep);
void	as_setwatch(struct as *as);
void	as_clearwatch(struct as *as);
int	as_getmemid(struct as *, caddr_t, memid_t *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_AS_H */
