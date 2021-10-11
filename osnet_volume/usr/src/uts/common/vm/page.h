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
 *	(c) 1986, 1987, 1988, 1989, 1990, 1993, 1996  Sun Microsystems, Inc
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_VM_PAGE_H
#define	_VM_PAGE_H

#pragma ident	"@(#)page.h	1.124	99/06/01 SMI"

#include <vm/seg.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL) || defined(_KMEMUSER)

/*
 * Shared/Exclusive lock.
 */

/*
 * Types of page locking supported by page_lock & friends.
 */
typedef enum {
	SE_SHARED,
	SE_EXCL			/* exclusive lock (value == -1) */
} se_t;

/*
 * For requesting that page_lock reclaim the page from the free list.
 */
typedef enum {
	P_RECLAIM,		/* reclaim page from free list */
	P_NO_RECLAIM		/* DON`T reclaim the page	*/
} reclaim_t;

#endif	/* _KERNEL | _KMEMUSER */

typedef int	selock_t;

/*
 * Define VM_STATS to turn on all sorts of statistic gathering about
 * the VM layer.  By default, it is only turned on when DEBUG is
 * also defined.
 */
#ifdef DEBUG
#define	VM_STATS
#endif	/* DEBUG */

#ifdef VM_STATS
#define	VM_STAT_ADD(stat)	(stat)++
#else
#define	VM_STAT_ADD(stat)
#endif	/* VM_STATS */

#ifdef _KERNEL

/*
 * Macros to acquire and release the page logical lock.
 */
#define	page_struct_lock(pp)	mutex_enter(&page_llock)
#define	page_struct_unlock(pp)	mutex_exit(&page_llock)

#endif	/* _KERNEL */

#include <sys/t_lock.h>

struct as;

/*
 * Each physical page has a page structure, which is used to maintain
 * these pages as a cache.  A page can be found via a hashed lookup
 * based on the [vp, offset].  If a page has an [vp, offset] identity,
 * then it is entered on a doubly linked circular list off the
 * vnode using the vpnext/vpprev pointers.   If the p_free bit
 * is on, then the page is also on a doubly linked circular free
 * list using next/prev pointers.  If the "p_selock" and "p_iolock"
 * are held, then the page is currently being read in (exclusive p_selock)
 * or written back (shared p_selock).  In this case, the next/prev pointers
 * are used to link the pages together for a consecutive i/o request.  If
 * the page is being brought in from its backing store, then other processes
 * will wait for the i/o to complete before attaching to the page since it
 * will have an "exclusive" lock.
 *
 * Each page structure has the locks described below along with
 * the fields they protect:
 *
 *	p_selock	This is a per-page shared/exclusive lock that is
 *			used to implement the logical shared/exclusive
 *			lock for each page.  The "shared" lock is normally
 *			used in most cases while the "exclusive" lock is
 *			required to destroy or retain exclusive access to
 *			a page (e.g., while reading in pages).  The appropriate
 *			lock is always held whenever there is any reference
 *			to a page structure (e.g., during i/o).
 *
 *				p_hash
 *				p_vnode
 *				p_offset
 *
 *				p_free
 *				p_age
 *
 *	p_iolock	This is a binary semaphore lock that provides
 *			exclusive access to the i/o list links in each
 *			page structure.  It is always held while the page
 *			is on an i/o list (i.e., involved in i/o).  That is,
 *			even though a page may be only `shared' locked
 *			while it is doing a write, the following fields may
 *			change anyway.  Normally, the page must be
 *			`exclusively' locked to change anything in it.
 *
 *				p_next
 *				p_prev
 *
 * The following fields are protected by the global page_llock:
 *
 *				p_lckcnt
 *				p_cowcnt
 *
 * The following lists are protected by the global page_freelock:
 *
 *				page_cachelist
 *				page_freelist
 *
 * The following, for our purposes, are protected by
 * the global freemem_lock:
 *
 *				freemem
 *				freemem_wait
 *				freemem_cv
 *
 * The following fields are protected by hat layer lock(s).  When a page
 * structure is not mapped and is not associated with a vnode (after a call
 * to page_hashout() for example) the p_nrm field may be modified with out
 * holding the hat layer lock:
 *
 *				p_nrm
 *				p_mapping
 *				p_share
 *
 * The following field is file system dependent.  How it is used and
 * the locking strategies applied are up to the individual file system
 * implementation.
 *
 *				p_fsdata
 *
 * The page structure is used to represent and control the system's
 * physical pages.  There is one instance of the structure for each
 * page that is not permenately allocated.  For example, the pages that
 * hold the page structures are permanently held by the kernel
 * and hence do not need page structures to track them.  The array
 * of page structures is allocated early on in the kernel's life and
 * is based on the amount of available physical memory.
 *
 * Each page structure may simultaneously appear on several linked lists.
 * The lists are:  hash list, free or in i/o list, and a vnode's page list.
 * Each type of list is protected by a different group of mutexes as described
 * below:
 *
 * The hash list is used to quickly find a page when the page's vnode and
 * offset within the vnode are known.  Each page that is hashed is
 * connected via the `p_hash' field.  The anchor for each hash is in the
 * array `page_hash'.  An array of mutexes, `ph_mutex', protects the
 * lists anchored by page_hash[].  To either search or modify a given hash
 * list, the appropriate mutex in the ph_mutex array must be held.
 *
 * The free list contains pages that are `free to be given away'.  For
 * efficiency reasons, pages on this list are placed in two catagories:
 * pages that are still associated with a vnode, and pages that are not
 * associated with a vnode.  Free pages always have their `p_free' bit set,
 * free pages that are still associated with a vnode also have their
 * `p_age' bit set.  Pages on the free list are connected via their
 * `p_next' and `p_prev' fields.  When a page is involved in some sort
 * of i/o, it is not free and these fields may be used to link associated
 * pages together.  At the moment, the free list is protected by a
 * single mutex `page_freelock'.  The list of free pages still associated
 * with a vnode is anchored by `page_cachelist' while other free pages
 * are anchored in architecture dependent ways (to handle page coloring etc.).
 *
 * Pages associated with a given vnode appear on a list anchored in the
 * vnode by the `v_pages' field.  They are linked together with
 * `p_vpnext' and `p_vpprev'.  The field `p_offset' contains a page's
 * offset within the vnode.  The pages on this list are not kept in
 * offset order.  These lists, in a manner similar to the hash lists,
 * are protected by an array of mutexes called `vph_hash'.  Before
 * searching or modifying this chain the appropriate mutex in the
 * vph_hash[] array must be held.
 *
 * Again, each of the lists that a page can appear on is protected by a
 * mutex.  Before reading or writing any of the fields comprising the
 * list, the appropriate lock must be held.  These list locks should only
 * be held for very short intervals.
 *
 * In addition to the list locks, each page structure contains a
 * shared/exclusive lock that protects various fields within it.
 * To modify one of these fields, the `p_selock' must be exclusively held.
 * To read a field with a degree of certainty, the lock must be at least
 * held shared.
 *
 * Removing a page structure from one of the lists requires holding
 * the appropriate list lock and the page's p_selock.  A page may be
 * prevented from changing identity, being freed, or otherwise modified
 * by acquiring p_selock shared.
 *
 * To avoid deadlocks, a strict locking protocol must be followed.  Basically
 * there are two cases:  In the first case, the page structure in question
 * is known ahead of time (e.g., when the page is to be added or removed
 * from a list).  In the second case, the page structure is not known and
 * must be found by searching one of the lists.
 *
 * When adding or removing a known page to one of the lists, first the
 * page must be exclusively locked (since at least one of its fields
 * will be modified), second the lock protecting the list must be acquired,
 * third the page inserted or deleted, and finally the list lock dropped.
 *
 * The more interesting case occures when the particular page structure
 * is not known ahead of time.  For example, when a call is made to
 * page_lookup(), it is not known if a page with the desired (vnode and
 * offset pair) identity exists.  So the appropriate mutex in ph_mutex is
 * acquired, the hash list searched, and if the desired page is found
 * an attempt is made to lock it.  The attempt to acquire p_selock must
 * not block while the hash list lock is held.  A deadlock could occure
 * if some other process was trying to remove the page from the list.
 * The removing process (following the above protocol) would have exclusively
 * locked the page, and be spinning waiting to acquire the lock protecting
 * the hash list.  Since the searching process holds the hash list lock
 * and is waiting to acquire the page lock, a deadlock occurs.
 *
 * The proper scheme to follow is: first, lock the appropriate list,
 * search the list, and if the desired page is found either use
 * page_trylock() (which will not block) or pass the address of the
 * list lock to page_lock().  If page_lock() can not acquire the page's
 * lock, it will drop the list lock before going to sleep.  page_lock()
 * returns a value to indicate if the list lock was dropped allowing the
 * calling program to react appropriately (i.e., retry the operation).
 *
 * If the list lock was dropped before the attempt at locking the page
 * was made, checks would have to be made to ensure that the page had
 * not changed identity before its lock was obtained.  This is because
 * the interval between dropping the list lock and acquiring the page
 * lock is indeterminate.
 *
 * In addition, when both a hash list lock (ph_mutex[]) and a vnode list
 * lock (vph_mutex[]) are needed, the hash list lock must be acquired first.
 * The routine page_hashin() is a good example of this sequence.
 * This sequence is ASSERTed by checking that the vph_mutex[] is not held
 * just before each acquisition of one of the mutexs in ph_mutex[].
 *
 * So, as a quick summary:
 *
 * 	pse_mutex[]'s protect the p_selock and p_cv fields.
 *
 * 	p_selock protects the p_free, p_age, p_vnode, p_offset and p_hash,
 *
 * 	ph_mutex[]'s protect the page_hash[] array and its chains.
 *
 * 	vph_mutex[]'s protect the v_pages field and the vp page chains.
 *
 *	First lock the page, then the hash chain, then the vnode chain.  When
 *	this is not possible `trylocks' must be used.  Sleeping while holding
 *	any of these mutexes (p_selock is not a mutex) is not allowed.
 *
 *
 *	field		reading		writing		    ordering
 *	======================================================================
 *	p_vnode		p_selock(E,S)	p_selock(E)
 *	p_offset
 *	p_free
 *	p_age
 *	=====================================================================
 *	p_hash		p_selock(E,S)	p_selock(E) &&	    p_selock, ph_mutex
 *					ph_mutex[]
 *	=====================================================================
 *	p_vpnext	p_selock(E,S)	p_selock(E) &&	    p_selock, vph_mutex
 *	p_vpprev			vph_mutex[]
 *	=====================================================================
 *	When the p_free bit is set:
 *
 *	p_next		p_selock(E,S)	p_selock(E) &&	    p_selock,
 *	p_prev				page_freelock	    page_freelock
 *
 *	When the p_free bit is not set:
 *
 *	p_next		p_selock(E,S)	p_selock(E) &&	    p_selock, p_iolock
 *	p_prev				p_iolock
 *	=====================================================================
 *	p_selock	pse_mutex[]	pse_mutex[]	    can`t acquire any
 *	p_cv						    other mutexes or
 *							    sleep while holding
 *							    this lock.
 *	=====================================================================
 *	p_lckcnt	p_selock(E,S)	p_selock(E) &&
 *	p_cowcnt			page_llock
 *	=====================================================================
 *	p_nrm		hat layer lock	hat layer lock
 *	p_mapping
 *	p_pagenum
 *	=====================================================================
 *
 *	where:
 *		E----> exclusive version of p_selock.
 *		S----> shared version of p_selock.
 *
 *
 *	Global data structures and variable:
 *
 *	field		reading		writing		    ordering
 *	=====================================================================
 *	page_hash[]	ph_mutex[]	ph_mutex[]	    can hold this lock
 *							    before acquiring
 *							    a vph_mutex or
 *							    pse_mutex.
 *	=====================================================================
 *	vp->v_pages	vph_mutex[]	vph_mutex[]	    can only acquire
 *							    a pse_mutex while
 *							    holding this lock.
 *	=====================================================================
 *	page_cachelist	page_freelock	page_freelock	    can't acquire any
 *	page_freelist	page_freelock	page_freelock
 *	=====================================================================
 *	freemem		freemem_lock	freemem_lock	    can't acquire any
 *	freemem_wait					    other mutexes while
 *	freemem_cv					    holding this mutex.
 *	=====================================================================
 *
 * Page relocation, PG_NORELOC and P_NORELOC.
 *
 * Pages may be relocated using the page_relocate() interface. Relocation
 * involves moving the contents and identity of a page to another, free page.
 * To relocate a page, the SE_EXCL lock must be obtained. The way to prevent
 * a page from being relocated is to hold the SE_SHARED lock (the SE_EXCL
 * lock must not be held indefinitely). If the page is going to be held
 * SE_SHARED indefinitely, then the PG_NORELOC hint should be passed
 * to page_create_va so that pages that are prevented from being relocated
 * can be managed differently by the platform specific layer.
 *
 * Pages locked in memory using page_pp_lock (p_lckcnt/p_cowcnt != 0)
 * are guaranteed to be held in memory, but can still be relocated
 * providing the SE_EXCL lock can be obtained.
 *
 * The P_NORELOC bit in the page_t.p_state field is provided for use by
 * the platform specific code in managing pages when the PG_NORELOC
 * hint is used.
 *
 * Memory delete and page locking.
 *
 * The set of all usable pages is managed using the global page list as
 * implemented by the memseg structure defined in the appropriate
 * mach_page.h header file. When memory is added or deleted this list
 * changes. Additions to this list guarantee that the list is never
 * corrupt.  In order to avoid the necessity of an additional lock to
 * protect against failed accesses to the memseg being deleted and, more
 * importantly, the page_ts, the memseg structure is never freed and the
 * page_t virtual address space is remapped to a page (or pages) of
 * zeros.  If a page_t is manipulated while it is p_selock'd, or if it is
 * locked indirectly via a hash or freelist lock, it is not possible for
 * memory delete to collect the page and so that part of the page list is
 * prevented from being deleted. If the page is referenced outside of one
 * of these locks, it is possible for the page_t being referenced to be
 * deleted.  Examples of this are page_t pointers returned by
 * page_numtopp_nolock, page_first and page_next.  Providing the page_t
 * is re-checked after taking the p_selock (for p_vnode != NULL), the
 * remapping to the zero pages will be detected.
 */
typedef struct page {
	struct  vnode *p_vnode;		/* logical vnode this page is from */
	struct page  *p_hash;		/* hash by [vnode, offset] */
	struct page  *p_vpnext;		/* next page in vnode list */
	struct page  *p_vpprev;		/* prev page in vnode list */
	struct page *p_next;		/* next page in free/intrans lists */
	struct page *p_prev;		/* prev page in free/intrans lists */
	u_offset_t  p_offset;		/* offset into vnode for this page */
	selock_t p_selock;		/* shared/exclusive lock on the page */
	ushort_t p_lckcnt;		/* number of locks on page data */
	ushort_t p_cowcnt;		/* number of copy on write lock */
	kcondvar_t p_cv;		/* page struct's condition var */
	kcondvar_t p_io_cv;		/* for iolock */
	uchar_t p_iolock_state;		/* replaces p_iolock */
	uchar_t p_filler;		/* unused at this time */
	uchar_t p_fsdata;		/* file system dependent byte */
	uchar_t p_state;		/* p_free, p_noreloc */
} page_t;

typedef	page_t	devpage_t;
#define	devpage	page

/*
 * Page hash table is a power-of-two in size, externally chained
 * through the hash field.  PAGE_HASHAVELEN is the average length
 * desired for this chain, from which the size of the page_hash
 * table is derived at boot time and stored in the kernel variable
 * page_hashsz.  In the hash function it is given by PAGE_HASHSZ.
 * PAGE_HASHVPSHIFT is defined so that 1 << PAGE_HASHVPSHIFT is
 * the approximate size of a vnode struct.
 *
 * PAGE_HASH_FUNC returns an index into the page_hash[] array.  This
 * index is also used to derive the mutex that protects the chain.
 */

#define	PAGE_HASHSZ	page_hashsz
#define	PAGE_HASHAVELEN		4
#define	PAGE_HASHVPSHIFT	6
#define	PAGE_HASH_FUNC(vp, off) \
	((((uintptr_t)(off) >> PAGESHIFT) + \
		((uintptr_t)(vp) >> PAGE_HASHVPSHIFT)) & \
		(PAGE_HASHSZ - 1))
#ifdef _KERNEL

extern kmutex_t ph_mutex[];
extern uint_t    ph_mutex_shift;
#define	PAGE_HASH_MUTEX(index)  &ph_mutex[(index) >> ph_mutex_shift]

/*
 * Flags used while creating pages.
 */
#define	PG_EXCL		0x0001
#define	PG_WAIT		0x0002
#define	PG_PHYSCONTIG	0x0004		/* NOT SUPPORTED */
#define	PG_MATCH_COLOR	0x0008		/* SUPPORTED by free list routines */
#define	PG_NORELOC	0x0010		/* Non-relocatable alloc hint. */
					/* Page must be PP_ISNORELOC */

#define	PAGE_LOCKED(pp)		((pp)->p_selock != 0)
#define	PAGE_SHARED(pp)		((pp)->p_selock > 0)
#define	PAGE_EXCL(pp)		((pp)->p_selock < 0)
#define	PAGE_LOCKED_SE(pp, se)	\
	((se) == SE_EXCL ? PAGE_EXCL(pp) : PAGE_SHARED(pp))

extern	long page_hashsz;
extern	page_t **page_hash;

extern	kmutex_t page_llock;		/* page logical lock mutex */
extern	kmutex_t freemem_lock;		/* freemem lock */

extern	pgcnt_t	total_pages;		/* total pages in the system */

/*
 * Variables controlling locking of physical memory.
 */
extern	pgcnt_t	pages_pp_maximum;	/* tuning: lock + claim <= max */
extern	void init_pages_pp_maximum(void);

#define	PG_FREE_LIST	1
#define	PG_CACHE_LIST	2

#define	PG_LIST_TAIL	0
#define	PG_LIST_HEAD	1

/*
 * Page frame operations.
 */
page_t	*page_lookup(struct vnode *, u_offset_t, se_t);
page_t	*page_lookup_nowait(struct vnode *, u_offset_t, se_t);
page_t	*page_find(struct vnode *, u_offset_t);
page_t	*page_exists(struct vnode *, u_offset_t);
void	page_needfree(spgcnt_t);
page_t	*page_create(struct vnode *, u_offset_t, size_t, uint_t);
page_t	*page_create_va(struct vnode *, u_offset_t, size_t, uint_t,
	struct seg *, caddr_t);
int	page_create_wait(size_t npages, uint_t flags);
void    page_create_putback(ssize_t npages);
void	page_free(page_t *, int);
void	free_vp_pages(struct vnode *, u_offset_t, size_t);
int	page_reclaim(page_t *, kmutex_t *);
void	page_destroy(page_t *, int);
void	page_destroy_free(page_t *);
void	page_rename(page_t *, struct vnode *, u_offset_t);
int	page_hashin(page_t *, struct vnode *, u_offset_t, kmutex_t *);
void	page_hashout(page_t *, kmutex_t *);
int	page_num_hashin(pfn_t, struct vnode *, u_offset_t);
void	page_add(page_t **, page_t *);
void	page_sub(page_t **, page_t *);
page_t	*page_get_freelist(struct vnode *, u_offset_t, struct seg *,
		caddr_t, size_t, uint_t, void *);

page_t	*page_get_cachelist(struct vnode *, u_offset_t, struct seg *,
		caddr_t, uint_t, void *);
void	page_list_add(int, page_t *, int);
void	page_list_sub(int, page_t *);
void	page_list_break(page_t **, page_t **, size_t);
void	page_list_concat(page_t **, page_t **);
void	page_vpadd(page_t **, page_t *);
void	page_vpsub(page_t **, page_t *);
int	page_lock(page_t *, se_t, kmutex_t *, reclaim_t);
int	page_trylock(page_t *, se_t);
int	page_try_reclaim_lock(page_t *, se_t);
int	page_tryupgrade(page_t *);
void	page_downgrade(page_t *);
void	page_unlock(page_t *);
void	page_lock_delete(page_t *);
int	page_pp_lock(page_t *, int, int);
void	page_pp_unlock(page_t *, int, int);
int	page_pp_useclaim(page_t *, page_t *, uint_t);
int	page_resv(pgcnt_t, uint_t);
void	page_unresv(pgcnt_t);
int	page_addclaim(page_t *);
int	page_subclaim(page_t *);
pfn_t	page_pptonum(page_t *);
page_t	*page_numtopp(pfn_t, se_t);
page_t	*page_numtopp_noreclaim(pfn_t, se_t);
page_t	*page_numtopp_nolock(pfn_t);
page_t	*page_numtopp_nowait(pfn_t, se_t);
page_t  *page_first();
page_t  *page_next(page_t *);
page_t  *page_nextn_raw(page_t *, ulong_t);	/* ((mach_page_t *)pp) += n */
#define	page_next_raw(PP)	page_nextn_raw((PP), 1)
page_t  *page_list_next(page_t *);
page_t  *page_nextn(page_t *, ulong_t);
void	ppcopy(page_t *, page_t *);
void	pagezero(page_t *, uint_t, uint_t);
void	page_io_lock(page_t *);
void	page_io_unlock(page_t *);
int	page_io_trylock(page_t *);
int	page_iolock_assert(page_t *);
void	page_iolock_init(page_t *);
pgcnt_t	page_busy(int);
void	page_lock_init(void);
int	page_isshared(page_t *);
int	page_isfree(page_t *);
int	page_isref(page_t *);
int	page_ismod(page_t *);
int	page_release(page_t *, int);


void page_set_props(page_t *, uint_t);
void page_clr_all_props(page_t *);

kmutex_t *page_vnode_mutex(struct vnode *);
kmutex_t *page_se_mutex(struct page *);

/*
 * Page relocation interfaces. page_relocate() is generic.
 * page_get_replacement_page() is provided by the PSM.
 */
int page_relocate(page_t **, page_t **);
page_t *page_get_replacement_page(page_t *);

/*
 * Tell the PIM we are adding physical memory
 */
void add_physmem(page_t *, size_t);

#endif	/* _KERNEL */

/*
 * Constants used for the p_iolock_state
 */
#define	PAGE_IO_INUSE	0x1
#define	PAGE_IO_WANTED	0x2

/*
 * Constants used for page_release status
 */
#define	PGREL_NOTREL    0x1
#define	PGREL_CLEAN	0x2
#define	PGREL_MOD	0x3

/*
 * The p_state field holds what used to be the p_age and p_free
 * bits.  These fields are protected by p_selock (see above).
 */
#define	P_FREE		0x80		/* Page on free list */
#define	P_NORELOC	0x40		/* Page is non-relocatable */

#define	PP_ISFREE(pp)		((pp)->p_state & P_FREE)
#define	PP_ISAGED(pp)		(((pp)->p_state & P_FREE) && \
					((pp)->p_vnode == NULL))
#define	PP_ISNORELOC(pp)	((pp)->p_state & P_NORELOC)

#define	PP_SETFREE(pp)		((pp)->p_state |= P_FREE)
#define	PP_SETAGED(pp)		ASSERT(PP_ISAGED(pp))
#define	PP_SETNORELOC(pp)	((pp)->p_state |= P_NORELOC)

#define	PP_CLRFREE(pp)		((pp)->p_state &= ~P_FREE)
#define	PP_CLRAGED(pp)		ASSERT(!PP_ISAGED(pp))
#define	PP_CLRNORELOC(pp)	((pp)->p_state &= ~P_NORELOC)

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_PAGE_H */
