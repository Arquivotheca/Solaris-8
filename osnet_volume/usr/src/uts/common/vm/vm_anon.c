/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986, 1987, 1988, 1989, 1990, 1993, 1996  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vm_anon.c	1.144	99/07/16 SMI"

/*
 * VM - anonymous pages.
 *
 * This layer sits immediately above the vm_swap layer.  It manages
 * physical pages that have no permanent identity in the file system
 * name space, using the services of the vm_swap layer to allocate
 * backing storage for these pages.  Since these pages have no external
 * identity, they are discarded when the last reference is removed.
 *
 * An important function of this layer is to manage low-level sharing
 * of pages that are logically distinct but that happen to be
 * physically identical (e.g., the corresponding pages of the processes
 * resulting from a fork before one process or the other changes their
 * contents).  This pseudo-sharing is present only as an optimization
 * and is not to be confused with true sharing in which multiple
 * address spaces deliberately contain references to the same object;
 * such sharing is managed at a higher level.
 *
 * The key data structure here is the anon struct, which contains a
 * reference count for its associated physical page and a hint about
 * the identity of that page.  Anon structs typically live in arrays,
 * with an instance's position in its array determining where the
 * corresponding backing storage is allocated; however, the swap_xlate()
 * routine abstracts away this representation information so that the
 * rest of the anon layer need not know it.  (See the swap layer for
 * more details on anon struct layout.)
 *
 * In the future versions of the system, the association between an
 * anon struct and its position on backing store will change so that
 * we don't require backing store all anonymous pages in the system.
 * This is important for consideration for large memory systems.
 * We can also use this technique to delay binding physical locations
 * to anonymous pages until pageout/swapout time where we can make
 * smarter allocation decisions to improve anonymous klustering.
 *
 * Many of the routines defined here take a (struct anon **) argument,
 * which allows the code at this level to manage anon pages directly,
 * so that callers can regard anon structs as opaque objects and not be
 * concerned with assigning or inspecting their contents.
 *
 * Clients of this layer refer to anon pages indirectly.  That is, they
 * maintain arrays of pointers to anon structs rather than maintaining
 * anon structs themselves.  The (struct anon **) arguments mentioned
 * above are pointers to entries in these arrays.  It is these arrays
 * that capture the mapping between offsets within a given segment and
 * the corresponding anonymous backing storage address.
 */

#ifdef DEBUG
#define	ANON_DEBUG
#endif

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/cred.h>
#include <sys/thread.h>
#include <sys/vnode.h>
#include <sys/cpuvar.h>
#include <sys/swap.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/bitmap.h>
#include <sys/vmsystm.h>
#include <sys/debug.h>
#include <sys/tnf_probe.h>

#include <vm/as.h>
#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/rm.h>

int anon_debug;

kmutex_t	anoninfo_lock;
struct		k_anoninfo k_anoninfo;
ani_free_t	ani_free_pool[ANI_MAX_POOL];

/*
 * Global hash table for (vp, off) -> anon slot
 */
extern	int swap_maxcontig;
size_t	anon_hash_size;
struct anon **anon_hash;

static struct kmem_cache *anon_cache;
static struct kmem_cache *anonmap_cache;

/*ARGSUSED*/
static int
anonmap_cache_constructor(void *buf, void *cdrarg, int kmflags)
{
	struct anon_map *amp = buf;

	mutex_init(&amp->lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&amp->serial_lock, NULL, MUTEX_DEFAULT, NULL);
	return (0);
}

/*ARGSUSED1*/
static void
anonmap_cache_destructor(void *buf, void *cdrarg)
{
	struct anon_map *amp = buf;

	mutex_destroy(&amp->lock);
	mutex_destroy(&amp->serial_lock);
}

kmutex_t	anonhash_lock[AH_LOCK_SIZE];

void
anon_init(void)
{
	int i;

	anon_hash_size = 1L << highbit(physmem / ANON_HASHAVELEN);

	for (i = 0; i < AH_LOCK_SIZE; i++)
		mutex_init(&anonhash_lock[i], NULL, MUTEX_DEFAULT, NULL);

	anon_hash = (struct anon **)
		kmem_zalloc(sizeof (struct anon *) * anon_hash_size, KM_SLEEP);
	anon_cache = kmem_cache_create("anon_cache", sizeof (struct anon),
		0, NULL, NULL, NULL, NULL, NULL, 0);
	anonmap_cache = kmem_cache_create("anonmap_cache",
		sizeof (struct anon_map), 0,
		anonmap_cache_constructor, anonmap_cache_destructor, NULL,
		NULL, NULL, 0);
	swap_maxcontig = 1024 * 1024 / PAGESIZE;	/* 1MB of pages */

}

/*
 * Global anon slot hash table manipulation.
 */

static void
anon_addhash(struct anon *ap)
{
	int index;

	ASSERT(MUTEX_HELD(&anonhash_lock[AH_LOCK(ap->an_vp, ap->an_off)]));
	index = ANON_HASH(ap->an_vp, ap->an_off);
	ap->an_hash = anon_hash[index];
	anon_hash[index] = ap;
}

static void
anon_rmhash(struct anon *ap)
{
	struct anon **app;

	ASSERT(MUTEX_HELD(&anonhash_lock[AH_LOCK(ap->an_vp, ap->an_off)]));

	for (app = &anon_hash[ANON_HASH(ap->an_vp, ap->an_off)];
	    *app; app = &((*app)->an_hash)) {
		if (*app == ap) {
			*app = ap->an_hash;
			break;
		}
	}
}

/*
 * The anon array interfaces. Functions allocating,
 * freeing array of pointers, and returning/setting
 * entries in the array of pointers for a given offset.
 *
 * Create the list of pointers
 */
struct anon_hdr *
anon_create(pgcnt_t npages, int flags)
{
	struct anon_hdr *ahp;
	ulong_t nchunks;
	int	kmemflags = KM_SLEEP;

	if (flags & ANON_NOSLEEP)
		kmemflags = KM_NOSLEEP;


	ahp = kmem_zalloc(sizeof (struct anon_hdr), kmemflags);
	if (ahp == NULL)
		return (NULL);

	/*
	 * Single level case.
	 */
	ahp->size = npages;
	if (npages <= ANON_CHUNK_SIZE || (flags & ANON_ALLOC_FORCE)) {

		if (flags & ANON_ALLOC_FORCE)
			ahp->flags |= ANON_ALLOC_FORCE;

		ahp->array_chunk = kmem_zalloc(
		    ahp->size * sizeof (struct anon *), kmemflags);

		if (ahp->array_chunk == NULL) {
			kmem_free(ahp, sizeof (struct anon_hdr));
			return (NULL);
		}
	} else {
		/*
		 * 2 Level case.
		 */
		nchunks = (ahp->size + ANON_CHUNK_OFF) >> ANON_CHUNK_SHIFT;

		ahp->array_chunk = kmem_zalloc(nchunks * sizeof (ulong_t *),
		    kmemflags);

		if (ahp->array_chunk == NULL) {
			kmem_free(ahp, sizeof (struct anon_hdr));
			return (NULL);
		}
	}
	return (ahp);
}

/*
 * Free the array of pointers
 */
void
anon_release(struct anon_hdr *ahp, pgcnt_t npages)
{
	ulong_t i;
	void **ppp;
	ulong_t nchunks;

	ASSERT(npages == ahp->size);

	/*
	 * Single level case.
	 */
	if (npages <= ANON_CHUNK_SIZE || (ahp->flags & ANON_ALLOC_FORCE)) {
		kmem_free(ahp->array_chunk, ahp->size * sizeof (struct anon *));
	} else {
		/*
		 * 2 level case.
		 */
		nchunks = (ahp->size + ANON_CHUNK_OFF) >> ANON_CHUNK_SHIFT;
		for (i = 0; i < nchunks; i++) {
			ppp = &ahp->array_chunk[i];
			if (*ppp != NULL)
				kmem_free(*ppp, PAGESIZE);
		}
		kmem_free(ahp->array_chunk, nchunks * sizeof (ulong_t *));
	}
	kmem_free(ahp, sizeof (struct anon_hdr));
}

/*
 * Return the pointer from the list for a
 * specified anon index.
 */
struct anon *
anon_get_ptr(struct anon_hdr *ahp, ulong_t an_idx)
{
	struct anon **app;

	ASSERT(an_idx < ahp->size);

	/*
	 * Single level case.
	 */
	if ((ahp->size <= ANON_CHUNK_SIZE) || (ahp->flags & ANON_ALLOC_FORCE)) {
		return (ahp->array_chunk[an_idx]);
	} else {

		/*
		 * 2 level case.
		 */
		app = ahp->array_chunk[an_idx >> ANON_CHUNK_SHIFT];
		if (app) {
			return (app[an_idx & ANON_CHUNK_OFF]);
		} else {
			return (NULL);
		}
	}
}

/*
 * Return the anon pointer for the first valid entry in the anon list,
 * starting from the given index.
 */
struct anon *
anon_get_next_ptr(struct anon_hdr *ahp, ulong_t *index)
{
	struct anon *ap;
	struct anon **app;
	ulong_t chunkoff;
	ulong_t i;
	ulong_t j;
	pgcnt_t size;

	i = *index;
	size = ahp->size;

	ASSERT(i < size);

	if ((size <= ANON_CHUNK_SIZE) || (ahp->flags & ANON_ALLOC_FORCE)) {
		/*
		 * 1 level case
		 */
		while (i < size) {
			ap = ahp->array_chunk[i];
			if (ap) {
				*index = i;
				return (ap);
			}
			i++;
		}
	} else {
		/*
		 * 2 level case
		 */
		chunkoff = i & ANON_CHUNK_OFF;
		while (i < size) {
			app = ahp->array_chunk[i >> ANON_CHUNK_SHIFT];
			if (app)
				for (j = chunkoff; j < ANON_CHUNK_SIZE; j++) {
					ap = app[j];
					if (ap) {
						*index = i + (j - chunkoff);
						return (ap);
					}
				}
			chunkoff = 0;
			i = (i + ANON_CHUNK_SIZE) & ~ANON_CHUNK_OFF;
		}
	}
	*index = size;
	return (NULL);
}

/*
 * Set list entry with a given pointer for a specified offset
 */
int
anon_set_ptr(struct anon_hdr *ahp, ulong_t an_idx, struct anon *ap, int flags)
{
	void		**ppp;
	struct anon	**app;
	int	kmemflags = KM_SLEEP;

	if (flags & ANON_NOSLEEP)
		kmemflags = KM_NOSLEEP;

	ASSERT(an_idx < ahp->size);

	/*
	 * Single level case.
	 */
	if (ahp->size <= ANON_CHUNK_SIZE || (ahp->flags & ANON_ALLOC_FORCE)) {
		ahp->array_chunk[an_idx] = ap;
	} else {

		/*
		 * 2 level case.
		 */
		ppp = &ahp->array_chunk[an_idx >> ANON_CHUNK_SHIFT];

		ASSERT(ppp != NULL);
		if (*ppp == NULL) {
			*ppp = kmem_zalloc(PAGESIZE, kmemflags);
			if (*ppp == NULL)
				return (ENOMEM);
		}
		app = *ppp;
		app[an_idx & ANON_CHUNK_OFF] = ap;
	}
	return (0);
}

/*
 * Copy anon array into a given new anon array
 */
int
anon_copy_ptr(struct anon_hdr *sahp, ulong_t s_idx,
	struct anon_hdr *dahp, ulong_t d_idx,
	pgcnt_t npages, int flags)
{
	void **sapp, **dapp;
	void *ap;
	int	kmemflags = KM_SLEEP;

	if (flags & ANON_NOSLEEP)
		kmemflags = KM_NOSLEEP;

	ASSERT((s_idx < sahp->size) && (d_idx < dahp->size));
	ASSERT((npages <= sahp->size) && (npages <= dahp->size));

	/*
	 * Both arrays are 1 level.
	 */
	if (((sahp->size <= ANON_CHUNK_SIZE) &&
	    (dahp->size <= ANON_CHUNK_SIZE)) ||
	    ((sahp->flags & ANON_ALLOC_FORCE) &&
	    (dahp->flags & ANON_ALLOC_FORCE))) {

		bcopy(&sahp->array_chunk[s_idx], &dahp->array_chunk[d_idx],
		    npages * sizeof (struct anon *));
		return (0);
	}

	/*
	 * Both arrays are 2 levels.
	 */
	if (sahp->size > ANON_CHUNK_SIZE &&
	    dahp->size > ANON_CHUNK_SIZE &&
	    ((sahp->flags & ANON_ALLOC_FORCE) == 0) &&
	    ((dahp->flags & ANON_ALLOC_FORCE) == 0)) {

		ulong_t sapidx, dapidx;
		ulong_t *sap, *dap;
		ulong_t chknp;

		while (npages != 0) {

			sapidx = s_idx & ANON_CHUNK_OFF;
			dapidx = d_idx & ANON_CHUNK_OFF;
			chknp = ANON_CHUNK_SIZE - MAX(sapidx, dapidx);
			if (chknp > npages)
				chknp = npages;

			sapp = &sahp->array_chunk[s_idx >> ANON_CHUNK_SHIFT];
			if ((sap = *sapp) != NULL) {
				dapp = &dahp->array_chunk[d_idx
							>> ANON_CHUNK_SHIFT];
				if ((dap = *dapp) == NULL) {
					*dapp = kmem_zalloc(PAGESIZE,
								kmemflags);
					if ((dap = *dapp) == NULL)
						return (ENOMEM);
				}
				bcopy((sap + sapidx), (dap + dapidx),
						chknp << ANON_PTRSHIFT);
			}
			s_idx += chknp;
			d_idx += chknp;
			npages -= chknp;
		}
		return (0);
	}

	/*
	 * At least one of the arrays is 2 level.
	 */
	while (npages--) {
		if ((ap = anon_get_ptr(sahp, s_idx)) != NULL) {
			if (anon_set_ptr(dahp, d_idx, ap, flags) == ENOMEM)
					return (ENOMEM);
		}
		s_idx++;
		d_idx++;
	}
	return (0);
}

/*
 * Called from clock handler to sync ani_free value.
 */

void
set_anoninfo(void)
{
	int	ix;
	pgcnt_t	total = 0;

	for (ix = 0; ix < ANI_MAX_POOL; ix++) {
		total += ani_free_pool[ix].ani_count;
	}
	k_anoninfo.ani_free = total;
}

/*
 * Reserve anon space.
 *
 * It's no longer simply a matter of incrementing ani_resv to
 * reserve swap space, we need to check memory-based as well
 * as disk-backed (physical) swap.  The following algorithm
 * is used:
 * 	Check the space on physical swap
 * 		i.e. amount needed < ani_max - ani_phys_resv
 * 	If we are swapping on swapfs check
 *		amount needed < (availrmem - swapfs_minfree)
 * Since the algorithm to check for the quantity of swap space is
 * almost the same as that for reserving it, we'll just use anon_resvmem
 * with a flag to decrement availrmem.
 *
 * Return non-zero on success.
 */
int
anon_resvmem(size_t size, u_int takemem)
{
	pgcnt_t npages = btopr(size);
	pgcnt_t mswap_pages = 0;
	pgcnt_t pswap_pages = 0;

	mutex_enter(&anoninfo_lock);

	/*
	 * pswap_pages is the number of pages we can take from
	 * physical (i.e. disk-backed) swap.
	 */
	ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);
	pswap_pages = k_anoninfo.ani_max - k_anoninfo.ani_phys_resv;

	ANON_PRINT(A_RESV,
	    ("anon_resvmem: npages %lu takemem %u pswap %lu caller %p\n",
	    npages, takemem, pswap_pages, (void *)caller()));

	if (npages <= pswap_pages) {
		/*
		 * we have enough space on a physical swap
		 */
		if (takemem)
			k_anoninfo.ani_phys_resv += npages;
		mutex_exit(&anoninfo_lock);
		return (1);
	} else if (pswap_pages != 0) {
		/*
		 * we have some space on a physical swap
		 */
		if (takemem) {
			/*
			 * use up remainder of phys swap
			 */
			k_anoninfo.ani_phys_resv += pswap_pages;
			ASSERT(k_anoninfo.ani_phys_resv == k_anoninfo.ani_max);
		}
	}
	/*
	 * since (npages > pswap_pages) we need mem swap
	 * mswap_pages is the number of pages needed from availrmem
	 */
	ASSERT(npages > pswap_pages);
	mswap_pages = npages - pswap_pages;

	ANON_PRINT(A_RESV, ("anon_resvmem: need %ld pages from memory\n",
	    mswap_pages));

	/*
	 * priv processes can reserve memory as swap as long as availrmem
	 * remains greater than swapfs_minfree; in the case of non-priv
	 * processes, memory can be reserved as swap only if availrmem
	 * doesn't fall below (swapfs_minfree + swapfs_reserve). Thus,
	 * swapfs_reserve amount of memswap is not available to non-priv
	 * processes. This protects daemons such as automounter dying
	 * as a result of application processes eating away almost entire
	 * membased swap. This safeguard becomes useless if apps are run
	 * with root access.
	 *
	 * swapfs_reserve is minimum of 4Mb or 1/16 of physmem.
	 *
	 */
	mutex_enter(&freemem_lock);
	if (availrmem > (swapfs_minfree + swapfs_reserve + mswap_pages) ||
		(CRED()->cr_ruid == 0 &&
		availrmem > (swapfs_minfree + mswap_pages))) {

		if (takemem) {
			/*
			 * Take the memory from the rest of the system.
			 */
			availrmem -= mswap_pages;
			mutex_exit(&freemem_lock);
			k_anoninfo.ani_mem_resv += mswap_pages;
			ANI_ADD(mswap_pages);
			ANON_PRINT((A_RESV | A_MRESV),
				("anon_resvmem: took %ld pages of availrmem\n",
				mswap_pages));
		} else {
			mutex_exit(&freemem_lock);
		}

		ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);
		mutex_exit(&anoninfo_lock);
		return (1);

	} else {
		/*
		 * Fail if not enough memory
		 */

		if (takemem) {
			k_anoninfo.ani_phys_resv -= pswap_pages;
		}

		mutex_exit(&freemem_lock);
		mutex_exit(&anoninfo_lock);
		ANON_PRINT(A_RESV,
			("anon_resvmem: not enough space from swapfs\n"));
		return (0);
	}
}


/*
 * Give back an anon reservation.
 */
void
anon_unresv(size_t size)
{
	pgcnt_t npages = btopr(size);
	spgcnt_t mem_free_pages = 0;
	pgcnt_t phys_free_slots;
#ifdef	ANON_DEBUG
	pgcnt_t mem_resv;
#endif

	mutex_enter(&anoninfo_lock);

	ASSERT(k_anoninfo.ani_mem_resv >= k_anoninfo.ani_locked_swap);
	/*
	 * If some of this reservation belonged to swapfs
	 * give it back to availrmem.
	 * ani_mem_resv is the amount of availrmem swapfs has reserved.
	 * but some of that memory could be locked by segspt so we can only
	 * return non locked ani_mem_resv back to availrmem
	 */
	if (k_anoninfo.ani_mem_resv > k_anoninfo.ani_locked_swap) {
		ANON_PRINT((A_RESV | A_MRESV),
		    ("anon_unresv: growing availrmem by %ld pages\n",
		    MIN(k_anoninfo.ani_mem_resv, npages)));

		mem_free_pages = MIN((spgcnt_t)(k_anoninfo.ani_mem_resv -
					k_anoninfo.ani_locked_swap), npages);
		mutex_enter(&freemem_lock);
		availrmem += mem_free_pages;
		mutex_exit(&freemem_lock);
		k_anoninfo.ani_mem_resv -= mem_free_pages;

		ANI_ADD(-mem_free_pages);
	}
	/*
	 * The remainder of the pages is returned to phys swap
	 */
	ASSERT(npages >= mem_free_pages);
	phys_free_slots = npages - mem_free_pages;

	if (phys_free_slots) {
	    k_anoninfo.ani_phys_resv -= phys_free_slots;
	}

#ifdef	ANON_DEBUG
	mem_resv = k_anoninfo.ani_mem_resv;
#endif

	ASSERT(k_anoninfo.ani_mem_resv >= k_anoninfo.ani_locked_swap);
	ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);

	mutex_exit(&anoninfo_lock);

	ANON_PRINT(A_RESV, ("anon_unresv: %lu, tot %lu, caller %p\n",
	    npages, mem_resv, (void *)caller()));
}

/*
 * Allocate an anon slot and return it with the lock held.
 */
struct anon *
anon_alloc(struct vnode *vp, anoff_t off)
{
	struct anon	*ap;
	kmutex_t	*ahm;

	ap = kmem_cache_alloc(anon_cache, KM_SLEEP);
	if (vp == NULL) {
		swap_alloc(ap);
	} else {
		ap->an_vp = vp;
		ap->an_off = off;
	}
	ap->an_refcnt = 1;
	ap->an_pvp = NULL;
	ap->an_poff = 0;
	ahm = &anonhash_lock[AH_LOCK(ap->an_vp, ap->an_off)];
	mutex_enter(ahm);
	anon_addhash(ap);
	mutex_exit(ahm);
	ANI_ADD(-1);
	ANON_PRINT(A_ANON, ("anon_alloc: returning ap %p, vp %p\n",
	    (void *)ap, (ap ? (void *)ap->an_vp : NULL)));
	return (ap);
}

/*
 * Decrement the reference count of an anon page.
 * If reference count goes to zero, free it and
 * its associated page (if any).
 */
void
anon_decref(struct anon *ap)
{
	page_t *pp;
	struct vnode *vp;
	anoff_t off;
	kmutex_t *ahm;

	ahm = &anonhash_lock[AH_LOCK(ap->an_vp, ap->an_off)];
	mutex_enter(ahm);
	ASSERT(ap->an_refcnt != 0);
	if (ap->an_refcnt == 0)
		cmn_err(CE_PANIC, "anon_decref: slot count 0\n");
	if (--ap->an_refcnt == 0) {
		swap_xlate(ap, &vp, &off);
		mutex_exit(ahm);

		/*
		 * If there is a page for this anon slot we will need to
		 * call VN_DISPOSE to get rid of the vp association and
		 * put the page back on the free list as really free.
		 * Acquire the "exclusive" lock to ensure that any
		 * pending i/o always completes before the swap slot
		 * is freed.
		 */
		pp = page_lookup(vp, (u_offset_t)off, SE_EXCL);

		/*
		 * If there was a page, we've synchronized on it (getting
		 * the exclusive lock is as good as gettting the iolock)
		 * so now we can free the physical backing store. Also, this
		 * is where we would free the name of the anonymous page
		 * (swap_free(ap)), a no-op in the current implementation.
		 */
		mutex_enter(ahm);
		ASSERT(ap->an_refcnt == 0);
		anon_rmhash(ap);
		if (ap->an_pvp)
			swap_phys_free(ap->an_pvp, ap->an_poff, PAGESIZE);
		mutex_exit(ahm);

		if (pp != NULL) {
			/*LINTED: constant in conditional context */
			VN_DISPOSE(pp, B_INVAL, 0, kcred);
		}
		ANON_PRINT(A_ANON, ("anon_decref: free ap %p, vp %p\n",
		    (void *)ap, (void *)ap->an_vp));
		kmem_cache_free(anon_cache, ap);

		ANI_ADD(1);
	} else {
		mutex_exit(ahm);
	}
}

/*
 * Duplicate references to size bytes worth of anon pages.
 * Used when duplicating a segment that contains private anon pages.
 * This code assumes that procedure calling this one has already used
 * hat_chgprot() to disable write access to the range of addresses that
 * that *old actually refers to.
 */
void
anon_dup(struct anon_hdr *old, ulong_t old_idx, struct anon_hdr *new,
			ulong_t new_idx, size_t size)
{
	spgcnt_t npages;
	kmutex_t *ahm;
	struct anon *ap;
	ulong_t off;
	ulong_t index;

	npages = btopr(size);
	while (npages > 0) {
		index = old_idx;
		if ((ap = anon_get_next_ptr(old, &index)) == NULL)
			break;

		off = index - old_idx;
		npages -= off;
		if (npages <= 0)
			break;

		(void) anon_set_ptr(new, new_idx + off, ap, ANON_SLEEP);
		ahm = &anonhash_lock[AH_LOCK(ap->an_vp, ap->an_off)];

		mutex_enter(ahm);
		ap->an_refcnt++;
		mutex_exit(ahm);

		off++;
		new_idx += off;
		old_idx += off;
		npages--;
	}
}

/*
 * Free a group of "size" anon pages, size in bytes,
 * and clear out the pointers to the anon entries.
 */
void
anon_free(struct anon_hdr *ahp, ulong_t index, size_t size)
{
	spgcnt_t npages;
	struct anon *ap;
	ulong_t old;

	npages = btopr(size);

	/*
	 * Large Files: The following is the assertion to validate the
	 * above cast.
	 *
	 * XXX64: needs work.
	 */

	ASSERT(btopr(size) <= UINT_MAX);

	while (npages > 0) {
		old = index;
		if ((ap = anon_get_next_ptr(ahp, &index)) == NULL)
			break;

		npages -= index - old;
		if (npages <= 0)
			break;

		(void) anon_set_ptr(ahp, index, NULL, ANON_SLEEP);
		anon_decref(ap);
		/*
		 * Bump index and decrement page count
		 */
		index++;
		npages--;
	}
}

/*
 * Make anonymous pages discardable
 */
void
anon_disclaim(struct anon_hdr *ahp, ulong_t index, size_t size)
{
	spgcnt_t npages = btopr(size);
	struct anon *ap;
	struct vnode *vp;
	anoff_t off;
	page_t *pp;
	kmutex_t *ahm;
	ulong_t old;

	while (npages > 0) {

		/*
		 * get anon pointer and index for the first valid entry
		 * in the anon list, starting from "index"
		 */
		old = index;
		if ((ap = anon_get_next_ptr(ahp, &index)) == NULL)
			break;

		/*
		 * decrement npages by number of NULL anon slots we skipped
		 */
		npages -= index - old;
		if (npages <= 0)
			break;

		/*
		 * increment index and decrement page count to account
		 * for the current anon slot.
		 */
		index++;
		npages--;

		/*
		 * get anonymous page and try to lock it SE_EXCL;
		 * since MADV_FREE is advisory, we move on to next
		 * anon slot in case we couldn't grab the lock
		 */
		swap_xlate(ap, &vp, &off);
		if ((pp = page_lookup_nowait(vp, (u_offset_t)off, SE_EXCL))
								== NULL) {
			segadvstat.MADV_FREE_miss.value.ul++;
			continue;
		}

		/*
		 * we cannot free a page which is permanently locked.
		 *
		 * The page_struct_lock need not be acquired to examine
		 * these fields since the page has an "exclusive" lock.
		 */
		if (pp->p_lckcnt != 0 || pp->p_cowcnt != 0) {
			page_unlock(pp);
			segadvstat.MADV_FREE_miss.value.ul++;
			continue;
		}

		ahm = &anonhash_lock[AH_LOCK(vp, off)];
		mutex_enter(ahm);
		ASSERT(ap->an_refcnt != 0);

		/*
		 * skip this one if copy-on-write is not yet broken.
		 */
		if (ap->an_refcnt > 1) {
			mutex_exit(ahm);
			page_unlock(pp);
			segadvstat.MADV_FREE_miss.value.ul++;
			continue;
		}

		/*
		 * free swap slot; it will force fault handler to create
		 * a zfod page in case this page is stolen by pageout.
		 *
		 * It guarantees that references to this page will not
		 * cause page to be read from swap slot until it is
		 * modified again.
		 */
		if (ap->an_pvp) {
			swap_phys_free(ap->an_pvp, ap->an_poff, PAGESIZE);
			ap->an_pvp = NULL;
			ap->an_poff = 0;
		}
		mutex_exit(ahm);

		segadvstat.MADV_FREE_hit.value.ul++;

		/*
		 * we want to make this page appear non_dirty to pageout
		 * daemon so that pageout won't push it to swap space
		 * until it gets modified again.
		 *
		 * copy ref/mod bits from hardware to page_t *and*
		 * clear them in hardware;
		 */
		(void) hat_pagesync(pp, HAT_SYNC_ZERORM);

		/*
		 * clear ref/mod bits in page_t;
		 */
		hat_clrrefmod(pp);

		/*
		 * while we are at it, unload all the translations and
		 * free the page if freemem is low;
		 * this will save some work for pageout_scanner
		 */
		if (freemem < lotsfree) {
			(void) hat_pageunload(pp, HAT_FORCE_PGUNLOAD);

			/*LINTED: constant in conditional context */
			VN_DISPOSE(pp, B_FREE, 0, kcred);
			continue;
		}

		/*
		 * unlock page
		 */
		page_unlock(pp);
	}
}

/*
 * Return the kept page(s) and protections back to the segment driver.
 */
int
anon_getpage(
	struct anon **app,
	u_int *protp,
	page_t *pl[],
	size_t plsz,
	struct seg *seg,
	caddr_t addr,
	enum seg_rw rw,
	struct cred *cred)
{
	page_t *pp;
	struct anon *ap = *app;
	struct vnode *vp;
	anoff_t off;
	int err;
	kmutex_t *ahm;

	swap_xlate(ap, &vp, &off);

	/*
	 * Lookup the page. If page is being paged in,
	 * wait for it to finish as we must return a list of
	 * pages since this routine acts like the VOP_GETPAGE
	 * routine does.
	 */
	if (pl != NULL && (pp = page_lookup(vp, (u_offset_t)off, SE_SHARED))) {
		ahm = &anonhash_lock[AH_LOCK(ap->an_vp, ap->an_off)];
		mutex_enter(ahm);
		if (ap->an_refcnt == 1)
			*protp = PROT_ALL;
		else
			*protp = PROT_ALL & ~PROT_WRITE;
		mutex_exit(ahm);
		pl[0] = pp;
		pl[1] = NULL;
		return (0);
	}

	/*
	 * Simply treat it as a vnode fault on the anon vp.
	 */

	TRACE_3(TR_FAC_VM, TR_ANON_GETPAGE,
		"anon_getpage:seg %x addr %x vp %x",
		seg, addr, vp);

	err = VOP_GETPAGE(vp, (u_offset_t)off, PAGESIZE, protp, pl, plsz,
	    seg, addr, rw, cred);

	if (err == 0 && pl != NULL) {
		ahm = &anonhash_lock[AH_LOCK(ap->an_vp, ap->an_off)];
		mutex_enter(ahm);
		if (ap->an_refcnt != 1)
			*protp &= ~PROT_WRITE;	/* make read-only */
		mutex_exit(ahm);
	}
	return (err);
}

/*
 * Turn a reference to an object or shared anon page
 * into a private page with a copy of the data from the
 * original page which is always locked by the caller.
 * This routine unloads the translation and unlocks the
 * original page, if it isn't being stolen, before returning
 * to the caller.
 *
 * NOTE:  The original anon slot is not freed by this routine
 *	  It must be freed by the caller while holding the
 *	  "anon_map" lock to prevent races which can occur if
 *	  a process has multiple lwps in its address space.
 */
page_t *
anon_private(
	struct anon **app,
	struct seg *seg,
	caddr_t addr,
	u_int	prot,
	page_t *opp,
	int oppflags,
	struct cred *cred)
{
	struct anon *old = *app;
	struct anon *new;
	page_t *pp;
	struct vnode *vp;
	anoff_t off;
	page_t *anon_pl[1 + 1];
	int err;

	if (oppflags & STEAL_PAGE)
		ASSERT(PAGE_EXCL(opp));
	else
		ASSERT(PAGE_LOCKED(opp));

	CPU_STAT_ADD_K(cpu_vminfo.cow_fault, 1);

	/* Kernel probe */
	TNF_PROBE_1(anon_private, "vm pagefault", /* CSTYLED */,
		tnf_opaque,	address,	addr);

	*app = new = anon_alloc(NULL, 0);
	swap_xlate(new, &vp, &off);

	if (oppflags & STEAL_PAGE) {
		page_rename(opp, vp, (u_offset_t)off);
		pp = opp;
		TRACE_5(TR_FAC_VM, TR_ANON_PRIVATE,
			"anon_private:seg %p addr %x pp %p vp %p off %lx",
			seg, addr, pp, vp, off);
		hat_setmod(pp);
		return (pp);
	}

	/*
	 * Call the VOP_GETPAGE routine to create the page, thereby
	 * enabling the vnode driver to allocate any filesystem
	 * space (e.g., disk block allocation for UFS).  This also
	 * prevents more than one page from being added to the
	 * vnode at the same time.
	 */
	err = VOP_GETPAGE(vp, (u_offset_t)off, PAGESIZE, NULL,
	    anon_pl, PAGESIZE, seg, addr, S_CREATE, cred);
	if (err)
		goto out;

	pp = anon_pl[0];

	/*
	 * If the original page was locked, we need to move the lock
	 * to the new page by transfering 'cowcnt/lckcnt' of the original
	 * page to 'cowcnt/lckcnt' of the new page.
	 *
	 * See Statement at the beginning of segvn_lockop() and
	 * comments in page_pp_useclaim() regarding the way
	 * cowcnts/lckcnts are handled.
	 */
	if (oppflags & LOCK_PAGE) {
		if (!page_pp_useclaim(opp, pp, prot & PROT_WRITE)) {
			goto out;
		}
	}

	/*
	 * Now copy the contents from the original page,
	 * which is locked and loaded in the MMU by
	 * the caller to prevent yet another page fault.
	 */
	ppcopy(opp, pp);		/* XXX - should set mod bit in here */

	hat_setrefmod(pp);		/* mark as modified */

	/*
	 * Unload the old translation.
	 */
	hat_unload(seg->s_as->a_hat, addr, PAGESIZE, HAT_UNLOAD);

	/*
	 * Free unmapped, unmodified original page.
	 * or release the lock on the original page,
	 * otherwise the process will sleep forever in
	 * anon_decref() waiting for the "exclusive" lock
	 * on the page.
	 */
	(void) page_release(opp, 1);

	/*
	 * we are done with page creation so downgrade the new
	 * page's selock to shared, this helps when multiple
	 * as_fault(...SOFTLOCK...) are done to the same
	 * page(aio)
	 */
	page_downgrade(pp);

	/*
	 * NOTE:  The original anon slot must be freed by the
	 * caller while holding the "anon_map" lock, if we
	 * copied away from an anonymous page.
	 */
	return (pp);

out:
	*app = old;
	anon_decref(new);
	page_unlock(opp);
	return ((page_t *)NULL);
}

/*
 * Allocate a private zero-filled anon page.
 */
page_t *
anon_zero(struct seg *seg, caddr_t addr, struct anon **app, struct cred *cred)
{
	struct anon *ap;
	page_t *pp;
	struct vnode *vp;
	anoff_t off;
	page_t *anon_pl[1 + 1];
	int err;

	/* Kernel probe */
	TNF_PROBE_1(anon_zero, "vm pagefault", /* CSTYLED */,
		tnf_opaque,	address,	addr);

	*app = ap = anon_alloc(NULL, 0);
	swap_xlate(ap, &vp, &off);

	/*
	 * Call the VOP_GETPAGE routine to create the page, thereby
	 * enabling the vnode driver to allocate any filesystem
	 * dependent structures (e.g., disk block allocation for UFS).
	 * This also prevents more than on page from being added to
	 * the vnode at the same time since it is locked.
	 */
	err = VOP_GETPAGE(vp, off, PAGESIZE, NULL,
	    anon_pl, PAGESIZE, seg, addr, S_CREATE, cred);
	if (err) {
		*app = NULL;
		anon_decref(ap);
		return (NULL);
	}
	pp = anon_pl[0];

	pagezero(pp, 0, PAGESIZE);	/* XXX - should set mod bit */
	page_downgrade(pp);
	CPU_STAT_ADD_K(cpu_vminfo.zfod, 1);
	hat_setrefmod(pp);	/* mark as modified so pageout writes back */
	return (pp);
}

/*
 * Allocate array of private zero-filled anon pages for empty slots
 * and kept pages for non empty slots within given range.
 *
 * NOTE: This rountine will try and use large pages
 *	if available and supported by underlying platform.
 */
int
anon_map_getpages(
	struct anon_map *amp,
	ulong_t start_index,
	size_t len,
	page_t *ppa[],
	struct seg *seg,
	caddr_t addr,
	enum seg_rw rw,
	struct cred *cred)
{

	struct anon	*ap;
	struct vnode	*ap_vp;
	page_t		*pp, *pplist, *anon_pl[1 + 1];
	int		err = 0;
	ulong_t		p_index, index;
	pgcnt_t		npgs, pg_cnt;
	u_int		l_szc, szc, prot;
	anoff_t		ap_off;
	size_t		pgsz;

	/*
	 * XXX For now only handle S_CREATE.
	 */
	ASSERT(rw == S_CREATE);

	index	= start_index;
	p_index	= 0;
	npgs	= btopr(len);

	/*
	 * If this platform supports mulitple page sizes
	 * then try and allocate directly from the free
	 * list for pages larger than PAGESIZE.
	 *
	 * NOTE:When we have page_create_ru we can stop
	 *	directly allocating from the freelist.
	 */
	l_szc  = page_num_pagesizes() - 1;
	mutex_enter(&amp->serial_lock);
	while (npgs) {

		/*
		 * if anon slot already exists
		 *   (means page has been created)
		 * so 1) look up the page
		 *    2) if the page is still in memory, get it.
		 *    3) if not, create a page and
		 *	  page in from physical swap device.
		 * These are done in anon_getpage().
		 */
		ap = anon_get_ptr(amp->ahp, index);
		if (ap) {
			err = anon_getpage(&ap, &prot, anon_pl, PAGESIZE,
					seg, addr, S_READ, cred);
			if (err) {
				mutex_exit(&amp->serial_lock);
				cmn_err(CE_PANIC,
					"anon_map_getpages: anon_getpage");
			}
			pp = anon_pl[0];
			ppa[p_index++] = pp;

			addr += PAGESIZE;
			index++;
			npgs--;
			continue;
		}

		/*
		 * Now try and allocate the largest page possible
		 * for the current address and range.
		 * Keep dropping down in page size until:
		 *
		 *	1) Properly aligned
		 *	2) Does not overlap existing anon pages
		 *	3) Fits in remaining range.
		 *	4) able to allocate one.
		 *
		 * NOTE: XXX When page_create_ru is completed this code
		 *	 will change.
		 */
		szc    = l_szc;
		pplist = NULL;
		pg_cnt = 0;
		while (szc) {
			pgsz	= page_get_pagesize(szc);
			pg_cnt	= pgsz / PAGESIZE;
			if (IS_P2ALIGNED(addr, pgsz) && pg_cnt <= npgs &&
				anon_pages(amp->ahp, start_index + index,
								pg_cnt) == 0) {
				/*
				 * XXX
				 * Since we are faking page_create()
				 * we also need to do the freemem and
				 * pcf accounting.
				 */
				(void) page_create_wait(pg_cnt, PG_WAIT);

				pplist = page_get_freelist(
						(struct vnode *)NULL,
							(u_offset_t)0,
						seg, addr, pgsz, 0, NULL);

				if (pplist == NULL) {
					page_create_putback(pg_cnt);
				}

				/*
				 * If a request for a page of size
				 * larger than PAGESIZE failed
				 * then don't try that size anymore.
				 */
				if (pplist == NULL) {
					l_szc = szc - 1;
				} else {
					break;
				}
			}
			szc--;
		}

		/*
		 * If just using PAGESIZE pages then don't
		 * directly allocate from the free list.
		 */
		if (pplist == NULL) {
			ASSERT(szc == 0);
			pp = anon_zero(seg, addr, &ap, cred);
			if (pp == NULL) {
				mutex_exit(&amp->serial_lock);
				cmn_err(CE_PANIC,
					"anon_map_getpages: anon_zero");
			}
			ppa[p_index++] = pp;

			mutex_enter(&amp->lock);
			ASSERT(anon_get_ptr(amp->ahp, index) == NULL);
			(void) anon_set_ptr(amp->ahp, index, ap, ANON_SLEEP);
			mutex_exit(&amp->lock);

			addr += PAGESIZE;
			index++;
			npgs--;
			continue;
		}

		/*
		 * pplist is a list of pg_cnt PAGESIZE pages.
		 * These pages are locked SE_EXCL since they
		 * came directly off the free list.
		 */
		while (pg_cnt--) {

			ap = anon_alloc(NULL, 0);
			swap_xlate(ap, &ap_vp, &ap_off);

			ASSERT(pplist != NULL);
			pp = pplist;
			page_sub(&pplist, pp);
			PP_CLRFREE(pp);
			PP_CLRAGED(pp);

			if (!page_hashin(pp, ap_vp, ap_off, NULL)) {
				mutex_exit(&amp->serial_lock);
				cmn_err(CE_PANIC,
					"anon_map_getpages page_hashin");
			}
			pagezero(pp, 0, PAGESIZE);
			CPU_STAT_ADD_K(cpu_vminfo.zfod, 1);
			page_downgrade(pp);

			err = VOP_GETPAGE(ap_vp, ap_off, PAGESIZE,
					(u_int *)NULL, anon_pl, PAGESIZE, seg,
					addr, S_WRITE, cred);
			if (err) {
				mutex_exit(&amp->serial_lock);
				cmn_err(CE_PANIC, "anon_map_getpages: S_WRITE");
			}

			/*
			 * Unlock page once since it came off the
			 * freelist locked. The call to VOP_GETPAGE
			 * will leave it locked SHARED. Also mark
			 * as modified so pageout writes back.
			 */
			page_unlock(pp);
			hat_setrefmod(pp);

			mutex_enter(&amp->lock);

			ASSERT(anon_get_ptr(amp->ahp, index) == NULL);
			(void) anon_set_ptr(amp->ahp, index, ap, ANON_SLEEP);
			mutex_exit(&amp->lock);
			ppa[p_index++] = pp;

			addr += PAGESIZE;
			index++;
			npgs--;
		}
	}
	mutex_exit(&amp->serial_lock);
	return (0);
}

/*
 * Allocate and initialize an anon_map structure for seg
 * associating the given swap reservation with the new anon_map.
 */
struct anon_map *
anonmap_alloc(size_t size, size_t swresv)
{
	struct anon_map *amp;		/* XXX - For locknest */

	amp = kmem_cache_alloc(anonmap_cache, KM_SLEEP);

	amp->refcnt = 1;
	amp->size = size;

	amp->ahp = anon_create(btopr(size), ANON_SLEEP);
	amp->swresv = swresv;
	return (amp);
}

void
anonmap_free(struct anon_map *amp)
{
	ASSERT(amp->ahp);
	ASSERT(amp->refcnt == 0);

	anon_release(amp->ahp, btopr(amp->size));
	kmem_cache_free(anonmap_cache, amp);
}

/*
 * Returns true if the app array has some empty slots.
 * The offp and lenp paramters are in/out paramters.  On entry
 * these values represent the starting offset and length of the
 * mapping.  When true is returned, these values may be modified
 * to be the largest range which includes empty slots.
 */
int
non_anon(struct anon_hdr *ahp, ulong_t anon_idx, u_offset_t *offp,
				size_t *lenp)
{
	ulong_t i, el;
	ssize_t low, high;
	struct anon *ap;

	low = -1;
	for (i = 0, el = *lenp; i < el; i += PAGESIZE, anon_idx++) {
		ap = anon_get_ptr(ahp, anon_idx);
		if (ap == NULL) {
			if (low == -1)
				low = i;
			high = i;
		}
	}
	if (low != -1) {
		/*
		 * Found at least one non-anon page.
		 * Set up the off and len return values.
		 */
		if (low != 0)
			*offp += low;
		*lenp = high - low + PAGESIZE;
		return (1);
	}
	return (0);
}

/*
 * Return a count of the number of existing anon pages in the anon array
 * app in the range (off, off+len). The array and slots must be guaranteed
 * stable by the caller.
 */
pgcnt_t
anon_pages(struct anon_hdr *ahp, ulong_t anon_index, pgcnt_t nslots)
{
	pgcnt_t cnt = 0;

	while (nslots-- > 0) {
		if ((anon_get_ptr(ahp, anon_index)) != NULL)
			cnt++;
		anon_index++;
	}
	return (cnt);
}

/*
 * Move reserved phys swap into memory swap (unreserve phys swap
 * and reserve mem swap by the same amount).
 * Used by segspt when it needs to lock resrved swap npages in memory
 */
int
anon_swap_adjust(pgcnt_t npages)
{
	pgcnt_t unlocked_mem_swap;

	mutex_enter(&anoninfo_lock);

	ASSERT(k_anoninfo.ani_mem_resv >= k_anoninfo.ani_locked_swap);
	ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);

	unlocked_mem_swap = k_anoninfo.ani_mem_resv
					- k_anoninfo.ani_locked_swap;
	if (npages > unlocked_mem_swap) {
		spgcnt_t adjusted_swap = npages - unlocked_mem_swap;

		/*
		 * if there is not enough unlocked mem swap we take missing
		 * amount from phys swap and give it to mem swap
		 */
		mutex_enter(&freemem_lock);
		if (availrmem < adjusted_swap + segspt_minfree) {
			mutex_exit(&freemem_lock);
			mutex_exit(&anoninfo_lock);
			return (ENOMEM);
		}
		availrmem -= adjusted_swap;
		mutex_exit(&freemem_lock);

		k_anoninfo.ani_mem_resv += adjusted_swap;
		ASSERT(k_anoninfo.ani_phys_resv >= adjusted_swap);
		k_anoninfo.ani_phys_resv -= adjusted_swap;

		ANI_ADD(adjusted_swap);
	}
	k_anoninfo.ani_locked_swap += npages;

	ASSERT(k_anoninfo.ani_mem_resv >= k_anoninfo.ani_locked_swap);
	ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);

	mutex_exit(&anoninfo_lock);

	return (0);
}

/*
 * 'unlocked' reserved mem swap so when it is unreserved it
 * can be moved back phys (disk) swap
 */
void
anon_swap_restore(pgcnt_t npages)
{
	mutex_enter(&anoninfo_lock);

	ASSERT(k_anoninfo.ani_locked_swap <= k_anoninfo.ani_mem_resv);

	ASSERT(k_anoninfo.ani_locked_swap >= npages);
	k_anoninfo.ani_locked_swap -= npages;

	ASSERT(k_anoninfo.ani_locked_swap <= k_anoninfo.ani_mem_resv);

	mutex_exit(&anoninfo_lock);
}
