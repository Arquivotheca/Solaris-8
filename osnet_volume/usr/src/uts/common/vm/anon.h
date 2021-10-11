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

#ifndef	_VM_ANON_H
#define	_VM_ANON_H

#pragma ident	"@(#)anon.h	1.69	99/06/01 SMI"
/*	From: SVr4.0 "kernel:vm/anon.h	1.8"			*/

#include <sys/cred.h>
#include <vm/seg.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * VM - Anonymous pages.
 */

typedef	unsigned long anoff_t;		/* anon offsets */

/*
 *	Each anonymous page, either in memory or in swap, has an anon structure.
 * The structure (slot) provides a level of indirection between anonymous pages
 * and their backing store.
 *
 *	(an_vp, an_off) names the vnode of the anonymous page for this slot.
 *
 * 	(an_pvp, an_poff) names the location of the physical backing store
 * 	for the page this slot represents. If the name is null there is no
 * 	associated physical store. The physical backing store location can
 *	change while the slot is in use.
 *
 *	an_hash is a hash list of anon slots. The list is hashed by
 * 	(an_vp, an_off) of the associated anonymous page and provides a
 *	method of going from the name of an anonymous page to its
 * 	associated anon slot.
 *
 *	an_refcnt holds a reference count which is the number of separate
 * 	copies that will need to be created in case of copy-on-write.
 *	A refcnt > 0 protects the existence of the slot. The refcnt is
 * 	initialized to 1 when the anon slot is created in anon_alloc().
 *	If a client obtains an anon slot and allows multiple threads to
 * 	share it, then it is the client's responsibility to insure that
 *	it does not allow one thread to try to reference the slot at the
 *	same time as another is trying to decrement the last count and
 *	destroy the anon slot. E.g., the seg_vn segment type protects
 *	against this with higher level locks.
 */

struct anon {
	struct vnode *an_vp;	/* vnode of anon page */
	struct vnode *an_pvp;	/* vnode of physical backing store */
	anoff_t an_off;		/* offset of anon page */
	anoff_t an_poff;	/* offset in vnode */
	struct anon *an_hash;	/* hash table of anon slots */
	int an_refcnt;		/* # of people sharing slot */
};

#ifdef _KERNEL
/*
 * The swapinfo_lock protects:
 *		swapinfo list
 *		individual swapinfo structures
 *
 * The anoninfo_lock protects:
 *		anoninfo counters
 *
 * The anonhash_lock protects:
 *		anon hash lists
 *		anon slot fields
 *
 * Fields in the anon slot which are read-only for the life of the slot
 * (an_vp, an_off) do not require the anonhash_lock be held to access them.
 * If you access a field without the anonhash_lock held you must be holding
 * the slot with an_refcnt to make sure it isn't destroyed.
 * To write (an_pvp, an_poff) in a given slot you must also hold the
 * p_iolock of the anonymous page for slot.
 */
extern kmutex_t anoninfo_lock;
extern kmutex_t swapinfo_lock;
extern kmutex_t anonhash_lock[];

/*
 * Global hash table to provide a function from (vp, off) -> ap
 */
extern size_t anon_hash_size;
extern struct anon **anon_hash;
#define	ANON_HASH_SIZE	anon_hash_size
#define	ANON_HASHAVELEN	4
#define	ANON_HASH(VP, OFF)	\
((((uintptr_t)(VP) >> 7)  ^ ((OFF) >> PAGESHIFT)) & (ANON_HASH_SIZE - 1))

#define	AH_LOCK_SIZE	64
#define	AH_LOCK(vp, off) (ANON_HASH((vp), (off)) & (AH_LOCK_SIZE -1))

#endif	/* _KERNEL */

/*
 * Anonymous backing store accounting structure for swapctl.
 *
 * ani_max = maximum amount of swap space
 *	(including potentially available physical memory)
 * ani_free = amount of unallocated anonymous memory
 *	(some of which might be reserved and including
 *	potentially available physical memory)
 * ani_resv = amount of claimed (reserved) anonymous memory
 *
 * The swap data can be aquired more efficiently through the
 * kstats interface.
 * Total slots currently available for reservation =
 *	MAX(ani_max - ani_resv, 0) + (availrmem - swapfs_minfree)
 */
struct anoninfo {
	pgcnt_t	ani_max;
	pgcnt_t	ani_free;
	pgcnt_t	ani_resv;
};

#ifdef _SYSCALL32
struct anoninfo32 {
	size32_t ani_max;
	size32_t ani_free;
	size32_t ani_resv;
};
#endif /* _SYSCALL32 */

/*
 * Define the NCPU pool of the ani_free counters. Update the counter
 * of the cpu on which the thread is running and in every clock intr
 * sync anoninfo.ani_free with the current total off all the NCPU entries.
 */

typedef	struct	ani_free {
	kmutex_t	ani_lock;
	pgcnt_t		ani_count;
} ani_free_t;

#define	ANI_MAX_POOL	8
extern	ani_free_t	ani_free_pool[];

#define	ANI_ADD(inc)	{ \
	ani_free_t	*anifp; \
	int		index; \
	index = (CPU->cpu_id & (ANI_MAX_POOL - 1)); \
	anifp = &ani_free_pool[index]; \
	mutex_enter(&anifp->ani_lock); \
	anifp->ani_count += inc; \
	mutex_exit(&anifp->ani_lock); \
}

/*
 * Anon array pointers are allocated in chunks. Each chunk
 * has PAGESIZE/sizeof(u_long *) of anon pointers.
 * There are two levels of arrays for anon array pointers larger
 * than a chunk. The first level points to anon array chunks.
 * The second level consists of chunks of anon pointers.
 *
 * If anon array is smaller than a chunk then the whole anon array
 * is created (memory is allocated for whole anon array).
 * If anon array is larger than a chunk only first level array is
 * allocated. Then other arrays (chunks) are allocated only when
 * they are initialized with anon pointers.
 */
struct anon_hdr {
	pgcnt_t	size;		/* number of pointers to (anon) pages */
	void	**array_chunk;	/* pointers to anon pointers or chunks of */
				/* anon pointers */
	int	flags;		/* ANON_ALLOC_FORCE force preallocation of */
				/* whole anon array	*/
};

#ifdef	_LP64
#define	ANON_PTRSHIFT	3
#else
#define	ANON_PTRSHIFT	2
#endif

#define	ANON_CHUNK_SIZE		(PAGESIZE >> ANON_PTRSHIFT)
#define	ANON_CHUNK_SHIFT	(PAGESHIFT - ANON_PTRSHIFT)
#define	ANON_CHUNK_OFF		(ANON_CHUNK_SIZE - 1)

/*
 * Anon flags.
 */
#define	ANON_SLEEP		0x0	/* ok to block */
#define	ANON_NOSLEEP		0x1	/* non-blocking call */
#define	ANON_ALLOC_FORCE	0x2	/* force single level anon array */

/*
 * The anon_map structure is used by various clients of the anon layer to
 * manage anonymous memory.   When anonymous memory is shared,
 * then the different clients sharing it will point to the
 * same anon_map structure.  Also, if a segment is unmapped
 * in the middle where an anon_map structure exists, the
 * newly created segment will also share the anon_map structure,
 * although the two segments will use different ranges of the
 * anon array.  When mappings are private (or shared with
 * a reference count of 1), an unmap operation will free up
 * a range of anon slots in the array given by the anon_map
 * structure.  Because of fragmentation due to this unmapping,
 * we have to store the size of the anon array in the anon_map
 * structure so that we can free everything when the referernce
 * count goes to zero.
 */
struct anon_map {
	kmutex_t serial_lock;	/* serialize anon allocation operations */
	kmutex_t lock;		/* protect anon_map and anon ptr array */
	size_t	size;		/* size in bytes mapped by the anon array */
	struct anon_hdr *ahp; 	/* anon array header pointer, containing */
				/* anon pointer array(s) */
	size_t	swresv;		/* swap space reserved for this anon_map */
	uint_t	refcnt;		/* reference count on this structure */
	void 	*a_resv;	/* locality info */
};

#ifdef _KERNEL
/*
 * Anonymous backing store accounting structure for kernel.
 * ani_max = total reservable slots on physical (disk-backed) swap
 * ani_phys_resv = total phys slots reserved for use by clients
 * ani_mem_resv = total mem slots reserved for use by clients
 * ani_free = # unallocated physical slots + # of reserved unallocated
 * memory slots
 */

/*
 * Initial total swap slots available for reservation
 */
#define	TOTAL_AVAILABLE_SWAP \
	(k_anoninfo.ani_max + MAX((spgcnt_t)(availrmem - swapfs_minfree), 0))

/*
 * Swap slots currently available for reservation
 */
#define	CURRENT_TOTAL_AVAILABLE_SWAP \
	((k_anoninfo.ani_max - k_anoninfo.ani_phys_resv) +	\
			MAX((spgcnt_t)(availrmem - swapfs_minfree), 0))

struct k_anoninfo {
	pgcnt_t	ani_max;	/* total reservable slots on phys */
					/* (disk) swap */
	pgcnt_t	ani_free;	/* # of unallocated phys and mem slots */
	pgcnt_t	ani_phys_resv;	/* # of reserved phys (disk) slots */
	pgcnt_t	ani_mem_resv;	/* # of reserved mem slots */
	pgcnt_t	ani_locked_swap; /* # of swap slots locked in reserved */
				/* mem swap */
};

extern	struct k_anoninfo k_anoninfo;

#if	defined(__STDC__)	/* prototypes not for use by adbgen */

extern void	anon_init(void);
extern struct	anon *anon_alloc(struct vnode *vp, anoff_t off);
extern void	anon_dup(struct anon_hdr *oldahp, ulong_t old_idx,
			struct anon_hdr *newahp, ulong_t new_idx, size_t size);
extern void	anon_free(struct anon_hdr *aap, ulong_t index, size_t size);
extern void	anon_disclaim(struct anon_hdr *aap, ulong_t index, size_t size);
extern int	anon_getpage(struct anon **app, uint_t *protp, struct page **pl,
		    size_t plsz, struct seg *seg, caddr_t addr,
		    enum seg_rw rw, struct cred *cred);
extern  struct page	*anon_private(struct anon **app, struct seg *seg,
			    caddr_t addr, uint_t prot, struct page *opp,
			    int oppflags, struct cred *cred);
extern struct page	*anon_zero(struct seg *seg, caddr_t addr,
			    struct anon **app, struct cred *cred);
extern int	anon_map_getpages(struct anon_map *amp, ulong_t start_index,
		    size_t len, struct page **ppa,
		    struct seg *seg, caddr_t addr,
		    enum seg_rw rw, struct cred *cred);
extern int	anon_resvmem(size_t size, uint_t takemem);
extern void	anon_unresv(size_t size);
struct anon_map	*anonmap_alloc(size_t size, size_t swresv);
void		anonmap_free(struct anon_map *amp);
void		anon_decref(struct anon *);
int		non_anon(struct anon_hdr *ahp, ulong_t anon_idx,
			u_offset_t *offp, size_t *lenp);
pgcnt_t		anon_pages(struct anon_hdr *ahp, ulong_t anon_index,
			pgcnt_t nslots);
int		anon_swap_adjust(pgcnt_t);
void		anon_swap_restore(pgcnt_t);

extern struct anon_hdr 	*anon_create(pgcnt_t npages, int flags);
extern void	anon_release(struct anon_hdr *ahp, pgcnt_t npages);
extern struct anon *anon_get_ptr(struct anon_hdr *ahp, ulong_t offset);
extern int	anon_set_ptr(struct anon_hdr *ahp, ulong_t offset,
			struct anon *ap, int flags);
extern int 	anon_copy_ptr(struct anon_hdr *saap, ulong_t soff,
			struct anon_hdr *daap, ulong_t doff,
			pgcnt_t npages, int flags);
#endif	/* __STDC__ */

/*
 * anon_resv checks to see if there is enough swap space to fulfill a
 * request and if so, reserves the appropriate anonymous memory resources.
 * anon_checkspace just checks to see if there is space to fulfill the request,
 * without taking any resources.  Both return 1 if successful and 0 if not.
 */
#define	anon_resv(size)		anon_resvmem((size), 1)
#define	anon_checkspace(size)	anon_resvmem((size), 0)

/*
 * Flags to anon_private
 */
#define	STEAL_PAGE	0x1	/* page can be stolen */
#define	LOCK_PAGE	0x2	/* page must be ``logically'' locked */

/*
 * SEGKP ANON pages that are locked are assumed to be LWP stack pages
 * and thus count towards the user pages locked count.
 * This value is protected by the same lock as availrmem.
 */
extern pgcnt_t anon_segkp_pages_locked;

extern int anon_debug;

#ifdef ANON_DEBUG

#define	A_ANON	0x01
#define	A_RESV	0x02
#define	A_MRESV	0x04

/* vararg-like debugging macro. */
#define	ANON_PRINT(f, printf_args) \
		if (anon_debug & f) \
			printf printf_args

#else	/* ANON_DEBUG */

#define	ANON_PRINT(f, printf_args)

#endif	/* ANON_DEBUG */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_ANON_H */
