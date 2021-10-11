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
 * 	Copyright (c) 1986-1991, 1993, 1996-1997 by Sun Microsystems, Inc.
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vm_page.c	1.219	99/11/20 SMI"

/*
 * VM - physical page management.
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/vm.h>
#include <sys/vtrace.h>
#include <sys/swap.h>
#include <sys/cmn_err.h>
#include <sys/tuneable.h>
#include <sys/sysmacros.h>
#include <sys/cpuvar.h>
#include <sys/callb.h>
#include <sys/debug.h>
#include <sys/tnf_probe.h>
#include <sys/condvar_impl.h>
#include <sys/mem_config.h>
#include <sys/mem_cage.h>
#include <sys/kmem.h>
#include <sys/atomic.h>

#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/pvn.h>
#include <vm/seg_kmem.h>

#include <fs/fs_subr.h>

static int nopageage = 0;

static pgcnt_t max_page_get;	/* max page_get request size in pages */
pgcnt_t total_pages = 0;	/* total number of pages (used by /proc) */

/*
 * freemem_lock protects all freemem variables:
 * availrmem.
 */
kmutex_t freemem_lock;
pgcnt_t availrmem;
pgcnt_t availrmem_initial;

/*
 * new_freemem_lock protects freemem, freemem_wait & freemem_cv.
 */
static kmutex_t	new_freemem_lock;
static uint_t	freemem_wait;	/* someone waiting for freemem */
static kcondvar_t freemem_cv;

/*
 * Set to non-zero to avoid reclaiming pages which are
 * busy being paged back until the i/o is completed.
 */
int nopagereclaim = 0;

/*
 * The logical page free list is maintained as two lists, the 'free'
 * and the 'cache' lists.
 * The free list contains those pages that should be reused first.
 *
 * The implementation of the lists is machine dependent.
 * page_get_freelist(), page_get_cachelist(),
 * page_list_sub(), and page_list_add()
 * form the interface to the machine dependent implementation.
 *
 * Pages with p_free set are on the cache list.
 * Pages with p_free and p_age set are on the free list,
 *
 * A page may be locked while on either list.
 */

/*
 * free list accounting stuff.
 *
 *
 * Spread out the value for the number of pages on the
 * page free and page cache lists.  If there is just one
 * value, then it must be under just one lock.
 * The lock contention and cache traffic are a real bother.
 *
 * When we acquire and then drop a single pcf lock
 * we can start in the middle of the array of pcf structures.
 * If we acquire more than one pcf lock at a time, we need to
 * start at the front to avoid deadlocking.
 *
 * pcf_count holds the number of pages in each pool.
 *
 * pcf_block is set when page_create_get_something() has asked the
 * PSM page freelist and page cachelist routines without specifying
 * a color and nothing came back.  This is used to block anything
 * else from moving pages from one list to the other while the
 * lists are searched again.  If a page is freeed while pcf_block is
 * set, then pcf_reserve is incremented.  pcgs_unblock() takes care
 * of clearning pcf_block, doing the wakeups, etc.
 */

#if NCPU <= 4
#define	PAD	1
#define	PCF_FANOUT	4
static	uint_t	pcf_mask = 0x3;
#else
#define	PAD	9
#define	PCF_FANOUT	16
static	uint_t	pcf_mask = 0xF;
#endif

struct pcf {
	uint_t		pcf_touch;	/* just to help the cache */
	uint_t		pcf_count;	/* page count */
	kmutex_t	pcf_lock;	/* protects the structure */
	uint_t		pcf_wait;	/* number of waiters */
	uint_t		pcf_block; 	/* pcgs flag to page_free() */
	uint_t		pcf_reserve; 	/* pages freed after pcf_block set */
	uint_t		pcf_fill[PAD];	/* to line up on the caches */
};

static struct	pcf	pcf[PCF_FANOUT];
#define	PCF_INDEX()	((CPU->cpu_id) & (pcf_mask))

kmutex_t	pcgs_lock;		/* serializes page_create_get_ */
kmutex_t	pcgs_wait_lock;		/* used for delay in pcgs */
kcondvar_t	pcgs_cv;		/* cv for delay in pcgs */

#define	PAGE_LOCK_MAXIMUM \
	((1 << (sizeof (((page_t *)0)->p_lckcnt) * NBBY)) - 1)

#ifdef VM_STATS

/*
 * No locks, but so what, they are only statistics.
 */

static struct page_tcnt {
	int	pc_free_cache;		/* free's into cache list */
	int	pc_free_dontneed;	/* free's with dontneed */
	int	pc_free_pageout;	/* free's from pageout */
	int	pc_free_free;		/* free's into free list */
	int	pc_get_cache;		/* get's from cache list */
	int	pc_get_free;		/* get's from free list */
	int	pc_reclaim;		/* reclaim's */
	int	pc_abortfree;		/* abort's of free pages */
	int	pc_find_hit;		/* find's that find page */
	int	pc_find_miss;		/* find's that don't find page */
	int	pc_destroy_free;	/* # of free pages destroyed */
#define	PC_HASH_CNT	(4*PAGE_HASHAVELEN)
	int	pc_find_hashlen[PC_HASH_CNT+1];
} pagecnt;

uint_t	hashin_count;
uint_t	hashin_not_held;
uint_t	hashin_already;

uint_t	hashout_count;
uint_t	hashout_not_held;

uint_t	page_create_count;
uint_t	page_create_not_enough;
uint_t	page_create_not_enough_again;
uint_t	page_create_zero;
uint_t	page_create_hashout;
uint_t	page_create_page_lock_failed;
uint_t	page_create_trylock_failed;
uint_t	page_create_found_one;
uint_t	page_create_hashin_failed;
uint_t	page_create_dropped_phm;

uint_t	page_create_new;
uint_t	page_create_exists;
uint_t	page_create_putbacks;
uint_t	page_create_overshoot;

uint_t	page_reclaim_zero;
uint_t	page_reclaim_zero_locked;

uint_t	page_rename_exists;
uint_t	page_rename_count;

uint_t	page_lookup_cnt[20];
uint_t	page_lookup_nowait_cnt[10];
uint_t	page_find_cnt;
uint_t	page_exists_cnt;
uint_t	page_lookup_dev_cnt;
uint_t	get_cachelist_cnt;
uint_t	page_create_cnt[10];
/*
 * Collects statistics.
 */
#define	PAGE_HASH_SEARCH(index, pp, vp, off) { \
	uint_t	mylen = 0; \
			\
	for ((pp) = page_hash[(index)]; (pp); (pp) = (pp)->p_hash, mylen++) { \
		if ((pp)->p_vnode == (vp) && (pp)->p_offset == (off)) \
			break; \
	} \
	if ((pp) != NULL) \
		pagecnt.pc_find_hit++; \
	else \
		pagecnt.pc_find_miss++; \
	if (mylen > PC_HASH_CNT) \
		mylen = PC_HASH_CNT; \
	pagecnt.pc_find_hashlen[mylen]++; \
}

#else	/* VM_STATS */

/*
 * Don't collect statistics
 */
#define	PAGE_HASH_SEARCH(index, pp, vp, off) { \
	for ((pp) = page_hash[(index)]; (pp); (pp) = (pp)->p_hash) { \
		if ((pp)->p_vnode == (vp) && (pp)->p_offset == (off)) \
			break; \
	} \
}

#endif	/* VM_STATS */

static void page_init_mem_config(void);

/*
 * vm subsystem related initialization
 */
void
vm_init(void)
{
	boolean_t callb_vm_cpr(void *, int);

	(void) callb_add(callb_vm_cpr, 0, CB_CL_CPR_VM, "vm");
	page_init_mem_config();
}

/*
 * This function is called at startup and when memory is added or deleted.
 */
void
init_pages_pp_maximum()
{
	pgcnt_t p_min;
	static pgcnt_t pages_pp_maximum_startup;
	static int init_done;
	static pgcnt_t avrmem_delta;

	if (init_done == 0) {
		/* Save initial, possibly user set, value. */
		pages_pp_maximum_startup = pages_pp_maximum;

		/*
		 * Setting of pages_pp_maximum is based first time
		 * on the value of availrmem just after the start-up
		 * allocations. To preserve this relationship at run
		 * time, use a delta from availrmem_initial.
		 */
		ASSERT(availrmem_initial >= availrmem);
		avrmem_delta = availrmem_initial - availrmem;

		/* Make sure we don't come through here again. */
		init_done = 1;
	}
	pages_pp_maximum = pages_pp_maximum_startup;
	/*
	 * Determine pages_pp_maximum, the number of currently available
	 * pages (availrmem) that can't be `locked'. If not set by
	 * the user, we set it to 10% of the currently available memory.
	 * But we also insist that it be greater than tune.t_minarmem;
	 * otherwise a process could lock down a lot of memory, get swapped
	 * out, and never have enough to get swapped back in.
	 */
	p_min = MAX(tune.t_minarmem+100, (availrmem_initial-avrmem_delta)/10);
	if (pages_pp_maximum <= p_min) {
		pages_pp_maximum = p_min;
	}
}

void
set_max_page_get(pgcnt_t target_total_pages)
{
	max_page_get = target_total_pages / 2;
}

static pgcnt_t pending_delete;

/*ARGSUSED*/
static void
page_mem_config_post_add(
	void *arg,
	pgcnt_t delta_pages)
{
	set_max_page_get(total_pages - pending_delete);
	init_pages_pp_maximum();
}

/*ARGSUSED*/
static int
page_mem_config_pre_del(
	void *arg,
	pgcnt_t delta_pages)
{
	pgcnt_t nv;

	nv = atomic_add_long_nv(&pending_delete, (spgcnt_t)delta_pages);
	set_max_page_get(total_pages - nv);
	return (0);
}

/*ARGSUSED*/
static void
page_mem_config_post_del(
	void *arg,
	pgcnt_t delta_pages,
	int cancelled)
{
	pgcnt_t nv;

	nv = atomic_add_long_nv(&pending_delete, -(spgcnt_t)delta_pages);
	set_max_page_get(total_pages - nv);
	if (!cancelled)
		init_pages_pp_maximum();
}

static kphysm_setup_vector_t page_mem_config_vec = {
	KPHYSM_SETUP_VECTOR_VERSION,
	page_mem_config_post_add,
	page_mem_config_pre_del,
	page_mem_config_post_del,
};

static void
page_init_mem_config(void)
{
	int ret;

	ret = kphysm_setup_func_register(&page_mem_config_vec, (void *)NULL);
	ASSERT(ret == 0);
}

/*
 * add a physical chunk of memory to the pool
 * Since we cannot call the dynamic memory allocator yet,
 * we have startup() allocate memory for the page
 * structs, the hash tables.
 * num is the number of page structures and base is the
 * physical page number to be associated with the first page.
 */
void
add_physmem(pp, num)
	page_t *pp;
	pgcnt_t num;
{
	TRACE_2(TR_FAC_VM, TR_PAGE_INIT,
		"add_physmem:pp %p num %lu", pp, num);

	/*
	 * Arbitrarily limit the max page_get request
	 * to 1/2 of the page structs we have.
	 */
	total_pages += num;
	set_max_page_get(total_pages);

	/*
	 * The physical space for the pages array
	 * representing ram pages has already been
	 * allocated.  Here we initialize each lock
	 * in the page structure, and put each on
	 * the free list
	 */
	for (; num; pp = page_next_raw(pp), num--) {
		/*
		 * Initialize and acquire the write version
		 * of the per-page readers/writer lock.
		 */
		pp->p_selock = 0;

		/*
		 * Initialize IO lock
		 */
		page_iolock_init(pp);

		while (!page_lock(pp, SE_EXCL, (kmutex_t *)NULL, P_RECLAIM))
			continue;

		page_free(pp, 1);
	}
}


/*
 * Find a page representing the specified [vp, offset].
 * If we find the page but it is intransit coming in,
 * it will have an "exclusive" lock and we wait for
 * the i/o to complete.  A page found on the free list
 * is always reclaimed and then locked.  On success, the page
 * is locked, its data is valid and it isn't on the free
 * list, while a NULL is returned if the page doesn't exist.
 */
page_t *
page_lookup(vnode_t *vp, u_offset_t off, se_t se)
{
	page_t		*pp;
	se_t		ose = se;
	kmutex_t	*phm;
	ulong_t		index;
	uint_t		hash_locked;

	ASSERT(MUTEX_NOT_HELD(page_vnode_mutex(vp)));
	VM_STAT_ADD(page_lookup_cnt[0]);

	/*
	 * Acquire the appropriate page hash lock since
	 * we have to search the hash list.  Pages that
	 * hash to this list can't change identity while
	 * this lock is held.
	 */
	hash_locked = 0;
	index = PAGE_HASH_FUNC(vp, off);
	phm = NULL;
top:
	PAGE_HASH_SEARCH(index, pp, vp, off);
	if (pp != NULL) {
		VM_STAT_ADD(page_lookup_cnt[1]);
		/*
		 * If "nopagereclaim" is set and the "shared" lock
		 * is requested then we must not reclaim a page
		 * involved in i/o, so acquire the "exclusive"
		 * lock to ensure that the caller is blocked
		 * until the i/o completes.
		 */
		if (nopagereclaim && se == SE_SHARED)
			se = SE_EXCL;

		if (!hash_locked) {
			VM_STAT_ADD(page_lookup_cnt[2]);
			if (!page_try_reclaim_lock(pp, se)) {
				/*
				 * On a miss, acquire the phm.  Then
				 * next time, page_lock() will be called,
				 * causing a wait if the page is busy.
				 * just looping with page_trylock() would
				 * get pretty boring.
				 */
				VM_STAT_ADD(page_lookup_cnt[3]);
				phm = PAGE_HASH_MUTEX(index);
				mutex_enter(phm);
				hash_locked = 1;
				goto top;
			}
		} else {
			VM_STAT_ADD(page_lookup_cnt[4]);
			if (!page_lock(pp, se, phm, P_RECLAIM)) {
				VM_STAT_ADD(page_lookup_cnt[5]);
				goto top;
			}
		}

		/*
		 * Since `pp' is locked it can not change identity now.
		 * Reconfirm we locked the correct page.
		 *
		 * Both the p_vnode and p_offset *must* be cast volatile
		 * to force a reload of their values: The PAGE_HASH_SEARCH
		 * macro will have stuffed p_vnode and p_offset into
		 * registers before calling page_trylock(); another thread,
		 * actually holding the hash lock, could have changed the
		 * page's identity in memory, but our registers would not
		 * be changed, fooling the reconfirmation.  If the hash
		 * lock was held during the search, the casting would
		 * not be needed.
		 */
		VM_STAT_ADD(page_lookup_cnt[6]);
		if (((volatile struct vnode *)(pp->p_vnode) != vp) ||
		    ((u_offset_t)(pp->p_offset) != off)) {
			VM_STAT_ADD(page_lookup_cnt[7]);
			if (hash_locked) {
				cmn_err(CE_PANIC,
				    "page_lookup: lost page %p",
				    (void *)pp);
			}
			page_unlock(pp);
			phm = PAGE_HASH_MUTEX(index);
			mutex_enter(phm);
			hash_locked = 1;
			goto top;
		}

		/*
		 * If page_trylock() was called, then pp may still be on
		 * the cachelist (can't be on the free list, it would not
		 * have been found in the search).  If it is on the
		 * cachelist it must be pulled now. To pull the page from
		 * the cachelist, it must be exclusively locked.
		 *
		 * The other big difference between page_trylock() and
		 * page_lock(), is that page_lock() will pull the
		 * page from whatever free list (the cache list in this
		 * case) the page is on.  If page_trylock() was used
		 * above, then we have to do the reclaim ourselves.
		 */
		if ((!hash_locked) && (PP_ISFREE(pp))) {
			ASSERT(PP_ISAGED(pp) == 0);
			VM_STAT_ADD(page_lookup_cnt[8]);

			/*
			 * page_relcaim will insure that we
			 * have this page exclusively
			 */

			if (!page_reclaim(pp, NULL)) {
				/*
				 * Page_reclaim dropped whatever lock
				 * we held.
				 */
				VM_STAT_ADD(page_lookup_cnt[10]);
				phm = PAGE_HASH_MUTEX(index);
				mutex_enter(phm);
				hash_locked = 1;
				goto top;
			} else if (se == SE_SHARED) {
				VM_STAT_ADD(page_lookup_cnt[11]);
				page_downgrade(pp);
				ose = SE_EXCL;	/* prevent double downgrade */
			}
		}
		if (nopagereclaim && ose == SE_SHARED) {
			page_downgrade(pp);
		}
	} else if (!hash_locked) {
		VM_STAT_ADD(page_lookup_cnt[12]);
		phm = PAGE_HASH_MUTEX(index);
		mutex_enter(phm);
		hash_locked = 1;
		goto top;
#ifdef VM_STATS
	} else {
		VM_STAT_ADD(page_lookup_cnt[13]);
#endif
	}

	if (hash_locked) {
		VM_STAT_ADD(page_lookup_cnt[14]);
		mutex_exit(phm);
	}

	ASSERT(pp ? PAGE_LOCKED_SE(pp, se) : 1);

	ASSERT(pp ? ((PP_ISFREE(pp) == 0) && (PP_ISAGED(pp) == 0)) : 1);

	return (pp);
}

/*
 * Search the hash list for the page representing the
 * specified [vp, offset] and return it locked.  Skip
 * free pages and pages that cannot be locked as requested.
 * Used while attempting to kluster pages.
 */
page_t *
page_lookup_nowait(vnode_t *vp, u_offset_t off, se_t se)
{
	page_t		*pp;
	kmutex_t	*phm;
	ulong_t		index;
	uint_t		locked;

	ASSERT(MUTEX_NOT_HELD(page_vnode_mutex(vp)));
	VM_STAT_ADD(page_lookup_nowait_cnt[0]);

	index = PAGE_HASH_FUNC(vp, off);
	PAGE_HASH_SEARCH(index, pp, vp, off);
	locked = 0;
	if (pp == NULL) {
top:
		VM_STAT_ADD(page_lookup_nowait_cnt[1]);
		locked = 1;
		phm = PAGE_HASH_MUTEX(index);
		mutex_enter(phm);
		PAGE_HASH_SEARCH(index, pp, vp, off);
	}

	if (pp == NULL || PP_ISFREE(pp)) {
		VM_STAT_ADD(page_lookup_nowait_cnt[2]);
		pp = NULL;
	} else {
		if (!page_trylock(pp, se)) {
			VM_STAT_ADD(page_lookup_nowait_cnt[3]);
			pp = NULL;
		} else {
			VM_STAT_ADD(page_lookup_nowait_cnt[4]);
			/*
			 * See the comment in page_lookup()
			 */
			if (((volatile struct vnode *)(pp->p_vnode) != vp) ||
			    ((u_offset_t)(pp->p_offset) != off)) {
				VM_STAT_ADD(page_lookup_nowait_cnt[5]);
				if (locked) {
					cmn_err(CE_PANIC,
					    "page_lookup_nowait %p",
					    (void *)pp);
				}
				page_unlock(pp);
				goto top;
			}
			if (PP_ISFREE(pp)) {
				VM_STAT_ADD(page_lookup_nowait_cnt[6]);
				page_unlock(pp);
				pp = NULL;
			}
		}
	}
	if (locked) {
		VM_STAT_ADD(page_lookup_nowait_cnt[7]);
		mutex_exit(phm);
	}

	ASSERT(pp ? PAGE_LOCKED_SE(pp, se) : 1);

	return (pp);
}

/*
 * Search the hash list for a page with the specified [vp, off]
 * that is known to exist and is already locked.  This routine
 * is typically used by segment SOFTUNLOCK routines.
 */
page_t *
page_find(vnode_t *vp, u_offset_t off)
{
	page_t		*pp;
	kmutex_t	*phm;
	ulong_t		index;

	ASSERT(MUTEX_NOT_HELD(page_vnode_mutex(vp)));
	VM_STAT_ADD(page_find_cnt);

	index = PAGE_HASH_FUNC(vp, off);
	phm = PAGE_HASH_MUTEX(index);

	mutex_enter(phm);
	PAGE_HASH_SEARCH(index, pp, vp, off);
	mutex_exit(phm);

	ASSERT(pp != NULL);
	ASSERT(PAGE_LOCKED(pp) || panicstr);
	return (pp);
}

/*
 * Determine whether a page with the specified [vp, off]
 * currently exists in the system.  Obviously this should
 * only be considered as a hint since nothing prevents the
 * page from disappearing or appearing immediately after
 * the return from this routine. Subsequently, we don't
 * even bother to lock the list.
 */
page_t *
page_exists(vnode_t *vp, u_offset_t off)
{
	page_t	*pp;
	ulong_t		index;

	ASSERT(MUTEX_NOT_HELD(page_vnode_mutex(vp)));
	VM_STAT_ADD(page_exists_cnt);

	index = PAGE_HASH_FUNC(vp, off);
	PAGE_HASH_SEARCH(index, pp, vp, off);

	return (pp);
}

/*
 * 'freemem' is used all over the kernel as an indication of how many
 * pages are free (either on the cache list or on the free page list)
 * in the system.  In very few places is a really accurate 'freemem'
 * needed.  To avoid contention of the lock protecting a the
 * single freemem, it was spread out into NCPU buckets.  Set_freemem
 * sets freemem to the total of all NCPU buckets.  It is called from
 * clock() on each TICK.
 */
void
set_freemem()
{
	struct pcf	*p;
	ulong_t		t;
	uint_t		i;

	t = 0;
	p = pcf;
	for (i = 0;  i < PCF_FANOUT; i++) {
		t += p->pcf_count;
		p++;
	}
	freemem = t;

	/*
	 * Don't worry about grabbing mutex.  It's not that
	 * critical if we miss a tick or two.  This is
	 * where we wakeup possible delayers in
	 * page_create_get_something().
	 */
	cv_broadcast(&pcgs_cv);
}

ulong_t
get_freemem()
{
	struct pcf	*p;
	ulong_t		t;
	uint_t		i;

	t = 0;
	p = pcf;
	for (i = 0; i < PCF_FANOUT; i++) {
		t += p->pcf_count;
		p++;
	}
	/*
	 * We just calculated it, might as well set it.
	 */
	freemem = t;
	return (t);
}

/*
 * Acquire all of the page cache & free (pcf) locks.
 */
void
pcf_acquire_all()
{
	struct pcf	*p;
	uint_t		i;

	p = pcf;
	for (i = 0; i < PCF_FANOUT; i++) {
		p->pcf_touch = 1;
		mutex_enter(&p->pcf_lock);
		p++;
	}
}

/*
 * Release all the pcf_locks.
 */
void
pcf_release_all()
{
	struct pcf	*p;
	uint_t		i;

	p = pcf;
	for (i = 0; i < PCF_FANOUT; i++) {
		mutex_exit(&p->pcf_lock);
		p++;
	}
}

/*
 * Inform the VM system that we need some pages freed up.
 * Calls must be symmetric, e.g.:
 *
 *	page_needfree(100);
 *	wait a bit;
 *	page_needfree(-100);
 */
void
page_needfree(spgcnt_t npages)
{
	mutex_enter(&new_freemem_lock);
	needfree += npages;
	mutex_exit(&new_freemem_lock);
}

/*
 * Throttle for page_create(): try to prevent freemem from dropping
 * below throttlefree.  We can't provide a 100% guarantee because
 * KM_NOSLEEP allocations, page_reclaim(), and various other things
 * nibble away at the freelist.  However, we can block all PG_WAIT
 * allocations until memory becomes available.  The motivation is
 * that several things can fall apart when there's no free memory:
 *
 * (1) If pageout() needs memory to push a page, the system deadlocks.
 *
 * (2) By (broken) specification, timeout(9F) can neither fail nor
 *     block, so it has no choice but to panic the system if it
 *     cannot allocate a callout structure.
 *
 * (3) Like timeout(), ddi_set_callback() cannot fail and cannot block;
 *     it panics if it cannot allocate a callback structure.
 *
 * (4) Untold numbers of third-party drivers have not yet been hardened
 *     against KM_NOSLEEP and/or allocb() failures; they simply assume
 *     success and panic the system with a data fault on failure.
 *     (The long-term solution to this particular problem is to ship
 *     hostile fault-injecting DEBUG kernels with the DDK.)
 *
 * It is theoretically impossible to guarantee success of non-blocking
 * allocations, but in practice, this throttle is very hard to break.
 */
static int
page_create_throttle(pgcnt_t npages, int flags)
{
	ulong_t	fm;
	uint_t	i;

	/*
	 * NEVER deny pages to pageout or sched
	 */
	if (curproc == proc_pageout || curthread == &t0)
		return (1);

	/*
	 * If the allocation can't block, we look favorably upon it
	 * unless we're below pageout_reserve.  In that case we fail
	 * the allocation because we want to make sure there are a few
	 * pages available for pageout.
	 */
	if ((flags & PG_WAIT) == 0)
		return (freemem >= npages + pageout_reserve);

	cv_signal(&proc_pageout->p_cv);

	while (freemem < npages + throttlefree) {
		pcf_acquire_all();
		mutex_enter(&new_freemem_lock);
		fm = 0;
		for (i = 0; i < PCF_FANOUT; i++) {
			fm += pcf[i].pcf_count;
			pcf[i].pcf_wait++;
			mutex_exit(&pcf[i].pcf_lock);
		}
		freemem = fm;
		needfree += npages;
		freemem_wait++;
		cv_wait(&freemem_cv, &new_freemem_lock);
		freemem_wait--;
		needfree -= npages;
		mutex_exit(&new_freemem_lock);
	}
	return (1);
}

/*
 * page_create_wait() is called to either coalecse pages from the
 * different pcf buckets or to wait because there simply are not
 * enough pages to satisfy the caller's request.
 *
 * Sadly, this is called from platform/vm/vm_machdep.c
 */
int
page_create_wait(size_t npages, uint_t flags)
{
	pgcnt_t		total;
	uint_t		i;
	struct pcf	*p;

	/*
	 * Wait until there are enough free pages to satisfy our
	 * entire request.
	 * We set needfree += npages before prodding pageout, to make sure
	 * it does real work when npages > lotsfree > freemem.
	 */
	VM_STAT_ADD(page_create_not_enough);

	ASSERT(!kcage_on ? !(flags & PG_NORELOC) : 1);
checkagain:
	if ((flags & PG_NORELOC) &&
	    kcage_freemem < kcage_throttlefree + npages)
		kcage_create_throttle(npages, flags);

	if (freemem < npages + throttlefree)
		if (!page_create_throttle(npages, flags))
			return (0);

	/*
	 * Since page_create_va() looked at every
	 * bucket, assume we are going to have to wait.
	 * Get all of the pcf locks.
	 */
	total = 0;
	p = pcf;
	for (i = 0; i < PCF_FANOUT; i++) {
		p->pcf_touch = 1;
		mutex_enter(&p->pcf_lock);
		total += p->pcf_count;
		if (total >= npages) {
			/*
			 * Wow!  There are enough pages laying around
			 * to satisfy the request.  Do the accounting,
			 * drop the locks we acquired, and go back.
			 *
			 * freemem is not protected by any lock. So,
			 * we cannot have any assertion containing
			 * freemem.
			 */
			freemem -= npages;

			while (p >= pcf) {
				if (p->pcf_count <= npages) {
					npages -= p->pcf_count;
					p->pcf_count = 0;
				} else {
					p->pcf_count -= (uint_t)npages;
					npages = 0;
				}
				mutex_exit(&p->pcf_lock);
				p--;
			}
			ASSERT(npages == 0);
			return (1);
		}
		p++;
	}

	/*
	 * All of the pcf locks are held, there are not enough pages
	 * to satisfy the request (npages < total).
	 * Be sure to acquire the new_freemem_lock before dropping
	 * the pcf locks.  This prevents dropping wakeups in page_free().
	 * The order is always pcf_lock then new_freemem_lock.
	 *
	 * Since we hold all the pcf locks, it is a good time to set freemem.
	 *
	 * If the caller does not want to wait, return now.
	 * Else turn the pageout daemon loose to find something
	 * and wait till it does.
	 *
	 */
	freemem = total;

	if ((flags & PG_WAIT) == 0) {
		pcf_release_all();

		TRACE_2(TR_FAC_VM, TR_PAGE_CREATE_NOMEM,
		"page_create_nomem:npages %ld freemem %ld", npages, freemem);
		return (0);
	}

	cv_signal(&proc_pageout->p_cv);

	TRACE_2(TR_FAC_VM, TR_PAGE_CREATE_SLEEP_START,
	    "page_create_sleep_start: freemem %ld needfree %ld",
	    freemem, needfree);

	/*
	 * We are going to wait.
	 * We currently hold all of the pcf_locks,
	 * get the new_freemem_lock (it protects freemem_wait),
	 * before dropping the pcf_locks.
	 */
	mutex_enter(&new_freemem_lock);

	p = pcf;
	for (i = 0; i < PCF_FANOUT; i++) {
		p->pcf_wait++;
		mutex_exit(&p->pcf_lock);
		p++;
	}

	needfree += npages;
	freemem_wait++;

	cv_wait(&freemem_cv, &new_freemem_lock);

	freemem_wait--;
	needfree -= npages;

	mutex_exit(&new_freemem_lock);

	TRACE_2(TR_FAC_VM, TR_PAGE_CREATE_SLEEP_END,
	    "page_create_sleep_end: freemem %ld needfree %ld",
	    freemem, needfree);

	VM_STAT_ADD(page_create_not_enough_again);
	goto checkagain;
}

/*
 * A routine to do the opposite of page_create_wait().
 */
void
page_create_putback(spgcnt_t npages)
{
	struct pcf	*p;
	pgcnt_t		lump;
	uint_t		*which;

	/*
	 * When a contiguous lump is broken up, we have to
	 * deal with lots of pages (min 64) so lets spread
	 * the wealth around.
	 */
	lump = roundup(npages, PCF_FANOUT) / PCF_FANOUT;
	freemem += npages;

	for (p = pcf; (npages > 0) && (p < &pcf[PCF_FANOUT]); p++) {
		which = &p->pcf_count;

		mutex_enter(&p->pcf_lock);

		if (p->pcf_block) {
			which = &p->pcf_reserve;
		}

		if (lump < npages) {
			*which += (uint_t)lump;
			npages -= lump;
		} else {
			*which += (uint_t)npages;
			npages = 0;
		}

		if (p->pcf_wait) {
			mutex_enter(&new_freemem_lock);
			/*
			 * Check to see if some other thread
			 * is actually waiting.  Another bucket
			 * may have woken it up by now.  If there
			 * are no waiters, then set our pcf_wait
			 * count to zero to avoid coming in here
			 * next time.
			 */
			if (freemem_wait) {
				if (npages > 1) {
					cv_broadcast(&freemem_cv);
				} else {
					cv_signal(&freemem_cv);
				}
				p->pcf_wait--;
			} else {
				p->pcf_wait = 0;
			}
			mutex_exit(&new_freemem_lock);
		}
		mutex_exit(&p->pcf_lock);
	}
	ASSERT(npages == 0);
}

/*
 * A helper routine for page_create_get_something.
 * The indenting got to deep down there.
 * Unblock the pcf counters.  Any pages freed after
 * pcf_block got set are moved to pcf_count and
 * wakeups (cv_broadcast() or cv_signal()) are done as needed.
 */
static void
pcgs_unblock(void)
{
	int		i;
	struct pcf	*p;

	/* Update freemem while we're here. */
	freemem = 0;
	p = pcf;
	for (i = 0; i < PCF_FANOUT; i++) {
		mutex_enter(&p->pcf_lock);
		ASSERT(p->pcf_count == 0);
		p->pcf_count = p->pcf_reserve;
		p->pcf_block = 0;
		freemem += p->pcf_count;
		if (p->pcf_wait) {
			mutex_enter(&new_freemem_lock);
			if (freemem_wait) {
				if (p->pcf_reserve > 1) {
					cv_broadcast(&freemem_cv);
					p->pcf_wait = 0;
				} else {
					cv_signal(&freemem_cv);
					p->pcf_wait--;
				}
			} else {
				p->pcf_wait = 0;
			}
			mutex_exit(&new_freemem_lock);
		}
		p->pcf_reserve = 0;
		mutex_exit(&p->pcf_lock);
		p++;
	}
}

/*
 * Called from page_create_va() when both the cache and free lists
 * have been checked once.
 *
 * Either returns a page or panics since the accounting was done
 * way before we got here.
 *
 * We don't come here often, so leave the accounting on permanently.
 */

#define	MAX_PCGS	100

#ifdef	DEBUG
#define	PCGS_TRIES	100
#else	DEBUG
#define	PCGS_TRIES	10
#endif	DEBUG

#ifdef	VM_STATS
uint_t	pcgs_counts[PCGS_TRIES];
uint_t	pcgs_too_many;
uint_t	pcgs_entered;
uint_t	pcgs_entered_noreloc;
uint_t	pcgs_locked;
#endif	VM_STATS

static page_t *
page_create_get_something(vnode_t *vp, u_offset_t off, struct seg *seg,
    caddr_t vaddr, uint_t flags)
{
	uint_t		count;
	page_t		*pp;
	uint_t		locked, i;
	struct	pcf	*p;

	VM_STAT_ADD(pcgs_entered);
#ifdef VM_STATS
	if ((flags & PG_NORELOC) != 0) {
		VM_STAT_ADD(pcgs_entered_noreloc);
	}
#endif
	/*
	 * Time to get serious.
	 * We failed to get a `correctly colored' page from both the
	 * free and cache lists.
	 * We escalate in stage.
	 *
	 * First try both lists without worring about color.
	 *
	 * Then, grab all page accounting locks (ie. pcf[]) and
	 * steal any pages that they have and set the pcf_block flag to
	 * stop deletions from the lists.  This will help because
	 * a page can get added to the free list while we are looking
	 * at the cache list, then another page could be added to the cache
	 * list allowing the page on the free list to be removed as we
	 * move from looking at the cache list to the free list. This
	 * could happen over and over. We would never find the page
	 * we have accounted for.
	 *
	 * Noreloc pages are a subset of the global (relocatable) page pool.
	 * They are not tracked separately in the pcf bins, so it is
	 * impossible to know when doing pcf accounting if the available
	 * page(s) are noreloc pages or not. When looking for a noreloc page
	 * it is quite easy to end up here even if the global (relocatable)
	 * page pool has plenty of free pages but the noreloc pool is empty.
	 *
	 * When the noreloc pool is empty (or low), additional noreloc pages
	 * are created by converting pages from the global page pool. This
	 * process will stall during pcf accounting if the pcf bins are
	 * already locked. Such is the case when a noreloc allocation is
	 * looping here in page_create_get_something waiting for more noreloc
	 * pages to appear.
	 *
	 * Short of adding a new field to the pcf bins to accurately track
	 * the number of free noreloc pages, we instead do not grab the
	 * pcgs_lock, do not set the pcf blocks and do not timeout when
	 * allocating a noreloc page. This allows noreloc allocations to
	 * loop without blocking global page pool allocations.
	 *
	 * NOTE: the behaviour of page_create_get_something has not changed
	 * for the case of global page pool allocations.
	 */

	flags &= ~PG_MATCH_COLOR;
	locked = 0;

	for (count = 0; kcage_on || count < MAX_PCGS; count++) {
		pp = page_get_freelist(vp, off, seg, vaddr, PAGESIZE,
			flags, NULL);
		if (pp == NULL) {
			pp = page_get_cachelist(vp, off, seg, vaddr,
				flags, NULL);
		}
		if (pp == NULL) {
			/*
			 * Serialize.  Don't fight with other pcgs().
			 */
			if (!locked && (!kcage_on || !(flags & PG_NORELOC))) {
				mutex_enter(&pcgs_lock);
				VM_STAT_ADD(pcgs_locked);
				locked = 1;
				p = pcf;
				for (i = 0; i < PCF_FANOUT; i++) {
					mutex_enter(&p->pcf_lock);
					ASSERT(p->pcf_block == 0);
					p->pcf_block = 1;
					p->pcf_reserve = p->pcf_count;
					p->pcf_count = 0;
					mutex_exit(&p->pcf_lock);
					p++;
				}
				freemem = 0;
			}

			if (count) {
				/*
				 * Since page_free() puts pages on
				 * a list then accounts for it, we
				 * just have to wait for page_free()
				 * to unlock any page it was working
				 * with. The page_lock()-page_reclaim()
				 * path falls in the same boat.
				 *
				 * We don't need to check on the
				 * PG_WAIT flag, we have already
				 * accounted for the page we are
				 * looking for in page_create_va().
				 *
				 * We just wait a moment to let any
				 * locked pages on the lists free up,
				 * then continue around and try again.
				 *
				 * Will be awakened by set_freemem().
				 */
				mutex_enter(&pcgs_wait_lock);
				cv_wait(&pcgs_cv, &pcgs_wait_lock);
				mutex_exit(&pcgs_wait_lock);
			}
		} else {
#ifdef VM_STATS
			if (count >= PCGS_TRIES) {
				VM_STAT_ADD(pcgs_too_many);
			} else {
				VM_STAT_ADD(pcgs_counts[count]);
			}
#endif
			if (locked) {
				pcgs_unblock();
				mutex_exit(&pcgs_lock);
			}
			return (pp);
		}
	}
	/*
	 * we go down holding the pcf locks.
	 */
	cmn_err(CE_PANIC, "no %spage found %d",
		((flags & PG_NORELOC) ? "non-reloc " : ""), count);
	/*NOTREACHED*/
}

/*
 * Create enough pages for "bytes" worth of data starting at
 * "off" in "vp".
 *
 *	Where flag must be one of:
 *
 *		PG_EXCL:	Exclusive create (fail if any page already
 *				exists in the page cache) which does not
 *				wait for memory to become available.
 *
 *		PG_WAIT:	Non-exclusive create which can wait for
 *				memory to become available.
 *
 *		PG_PHYSCONTIG:	Allocate physically contiguous pages.
 *				(Not Supported)
 *
 * A doubly linked list of pages is returned to the caller.  Each page
 * on the list has the "exclusive" (p_selock) lock and "iolock" (p_iolock)
 * lock.
 *
 * Unable to change the parameters to page_create() in a minor release,
 * we renamed page_create() to page_create_va(), changed all known calls
 * from page_create() to page_create_va(), and created this wrapper.
 *
 * Upon a major release, we should break compatibility by deleting this
 * wrapper, and replacing all the strings "page_create_va", with "page_create".
 *
 * NOTE: There is a copy of this interface as page_create_io() in
 *	 i86/vm/vm_machdep.c. Any bugs fixed here should be applied
 *	 there.
 */
page_t *
page_create(vnode_t *vp, u_offset_t off, size_t bytes, uint_t flags)
{
	caddr_t random_vaddr;
	struct seg kseg;

#ifdef DEBUG
	cmn_err(CE_WARN, "Using deprecated interface page_create: caller %p",
	    (void *)caller());
#endif

	random_vaddr = (caddr_t)(((uintptr_t)vp >> 7) ^ (off >> PAGESHIFT));
	kseg.s_as = &kas;

	return (page_create_va(vp, off, bytes, flags, &kseg, random_vaddr));
}

page_t *
page_create_va(vnode_t *vp, u_offset_t off, size_t bytes, uint_t flags,
    struct seg *seg, caddr_t vaddr)
{
	page_t		*plist = NULL;
	pgcnt_t		npages;
	pgcnt_t		found_on_free = 0;
	pgcnt_t		pages_req;
	page_t		*npp = NULL;
	uint_t		enough;
	uint_t		i;
	uint_t		pcf_index;
	struct pcf	*p;
	struct pcf	*q;

	TRACE_5(TR_FAC_VM, TR_PAGE_CREATE_START,
	"page_create_start:vp %p off %llx bytes %lu flags %x freemem %ld",
		vp, off, bytes, flags, freemem);

	ASSERT(bytes != 0 && vp != NULL);

	if ((flags & PG_EXCL) == 0 && (flags & PG_WAIT) == 0) {
		cmn_err(CE_PANIC, "page_create: invalid flags");
	}
	ASSERT((flags & ~(PG_EXCL | PG_WAIT | PG_NORELOC)) == 0);
	    /* but no others */

	pages_req = npages = btopr(bytes);
	/*
	 * Try to see whether request is too large to *ever* be
	 * satisfied, in order to prevent deadlock.  We arbitrarily
	 * decide to limit maximum size requests to max_page_get.
	 */
	if (npages >= max_page_get) {
		if ((flags & PG_WAIT) == 0) {
			TRACE_4(TR_FAC_VM, TR_PAGE_CREATE_TOOBIG,
			    "page_create_toobig:vp %p off %llx npages "
			    "%lu max_page_get %lu",
			    vp, off, npages, max_page_get);
			return (NULL);
		} else {
			cmn_err(CE_WARN,
			    "Request for too much kernel memory "
			    "(%lu bytes), will hang forever", bytes);
			for (;;)
				delay(1000000000);
		}
	}

	if (!kcage_on) {
		/*
		 * Cage is OFF, so everything is a RELOC request.
		 */
		flags &= ~PG_NORELOC;
	}

	/*
	 * If cage is on, dampen draw from cage when available
	 * cage space is low.
	 */
	if ((flags & PG_NORELOC) &&
		kcage_freemem < kcage_throttlefree + npages) {

		/*
		 * The cage is on, the caller wants NORELOC
		 * pages and available cage memory is very low.
		 * Call kcage_create_throttle() to attempt to
		 * control demand on the cage.
		 */
		kcage_create_throttle(npages, flags);
	}

	if (freemem <= throttlefree + npages)
		if (!page_create_throttle(npages, flags))
			return (NULL);

	VM_STAT_ADD(page_create_cnt[0]);

	enough = 0;
	pcf_index = PCF_INDEX();

	p = &pcf[pcf_index];
	p->pcf_touch = 1;
	q = &pcf[PCF_FANOUT];
	for (i = 0; i < PCF_FANOUT; i++) {
		if (p->pcf_count > npages) {
			/*
			 * a good one to try.
			 */
			mutex_enter(&p->pcf_lock);
			if (p->pcf_count > npages) {
				p->pcf_count -= (uint_t)npages;
				/*
				 * freemem is not protected by any lock.
				 * Thus, we cannot have any assertion
				 * containing freemem here.
				 */
				freemem -= npages;
				enough = 1;
				mutex_exit(&p->pcf_lock);
				break;
			}
			mutex_exit(&p->pcf_lock);
		}
		p++;
		if (p >= q) {
			p = pcf;
		}
		p->pcf_touch = 1;
	}

	if (!enough) {
		/*
		 * Have to look harder.  If npages is greater than
		 * one, then we might have to coalecse the counters.
		 *
		 * Go wait.  We come back having accounted
		 * for the memory.
		 */
		VM_STAT_ADD(page_create_cnt[1]);
		if (!page_create_wait(npages, flags)) {
			VM_STAT_ADD(page_create_cnt[2]);
			return (NULL);
		}
	}

	TRACE_3(TR_FAC_VM, TR_PAGE_CREATE_SUCCESS,
		"page_create_success:vp %p off %llx freemem %ld",
		vp, off, freemem);

	/*
	 * If satisfying this request has left us with too little
	 * memory, start the wheels turning to get some back.  The
	 * first clause of the test prevents waking up the pageout
	 * daemon in situations where it would decide that there's
	 * nothing to do.
	 */
	if (nscan < desscan && freemem < minfree) {
		TRACE_1(TR_FAC_VM, TR_PAGEOUT_CV_SIGNAL,
			"pageout_cv_signal:freemem %ld", freemem);
		cv_signal(&proc_pageout->p_cv);
	}

	/*
	 * Loop around collecting the requested number of pages.
	 * Most of the time, we have to `create' a new page. With
	 * this in mind, pull the page off the free list before
	 * getting the hash lock.  This will minimize the hash
	 * lock hold time, nesting, and the like.  If it turns
	 * out we don't need the page, we put it back at the end.
	 */
	while (npages--) {
		page_t		*pp;
		kmutex_t	*phm = NULL;
		ulong_t		index;

		index = PAGE_HASH_FUNC(vp, off);
top:
		ASSERT(phm == NULL);
		ASSERT(index == PAGE_HASH_FUNC(vp, off));
		ASSERT(MUTEX_NOT_HELD(page_vnode_mutex(vp)));

		if (npp == NULL) {
			/*
			 * Try to get a page from the freelist (ie,
			 * a page with no [vp, off] tag).  If that
			 * fails, use the cachelist.
			 *
			 * During the first attempt at both the free
			 * and cache lists we try for the correct color.
			 */
			/*
			 * XXXX-how do we deal with virtual indexed
			 * caches and and colors?
			 */
			VM_STAT_ADD(page_create_cnt[4]);
			npp = page_get_freelist(vp, off,
			    seg, vaddr, PAGESIZE, flags | PG_MATCH_COLOR, NULL);
			if (npp == NULL) {
				npp = page_get_cachelist(vp, off, seg,
				    vaddr, flags | PG_MATCH_COLOR, NULL);
				if (npp == NULL) {
					npp = page_create_get_something(vp,
					    off, seg, vaddr,
					    flags & ~PG_MATCH_COLOR);
				}

				if (PP_ISAGED(npp) == 0) {
					/*
					 * Since this page came from the
					 * cachelist, we must destroy the
					 * old vnode association.
					 */
					page_hashout(npp, (kmutex_t *)NULL);
				}
			}
		}

		/*
		 * We own this page!
		 */
		ASSERT(PAGE_EXCL(npp));
		ASSERT(npp->p_vnode == NULL);
		ASSERT(!hat_page_is_mapped(npp));
		PP_CLRFREE(npp);
		PP_CLRAGED(npp);

		/*
		 * Here we have a page in our hot little mits and are
		 * just waiting to stuff it on the appropriate lists.
		 * Get the mutex and check to see if it really does
		 * not exist.
		 */
		phm = PAGE_HASH_MUTEX(index);
		mutex_enter(phm);
		PAGE_HASH_SEARCH(index, pp, vp, off);
		if (pp == NULL) {
			VM_STAT_ADD(page_create_new);
			pp = npp;
			npp = NULL;
			if (!page_hashin(pp, vp, off, phm)) {
				/*
				 * Since we hold the page hash mutex and
				 * just searched for this page, page_hashin
				 * had better not fail.  If it does, that
				 * means somethread did not follow the
				 * page hash mutex rules.  Panic now and
				 * get it over with.  As usual, go down
				 * holding all the locks.
				 */
				ASSERT(MUTEX_NOT_HELD(phm));
				cmn_err(CE_PANIC,
				    "page_create: hashin failed %p %p %llx %p",
				    (void *)pp, (void *)vp, off, (void *)phm);

			}
			ASSERT(MUTEX_NOT_HELD(phm));	/* hashin dropped it */
			phm = NULL;

			/*
			 * Hat layer locking need not be done to set
			 * the following bits since the page is not hashed
			 * and was on the free list (i.e., had no mappings).
			 *
			 * Set the reference bit to protect
			 * against immediate pageout
			 *
			 * XXXmh modify freelist code to set reference
			 * bit so we don't have to do it here.
			 */
			page_set_props(pp, P_REF);
			found_on_free++;
		} else {
			VM_STAT_ADD(page_create_exists);
			if (flags & PG_EXCL) {
				/*
				 * Found an existing page, and the caller
				 * wanted all new pages.  Undo all of the work
				 * we have done.
				 */
				mutex_exit(phm);
				phm = NULL;
				while (plist != NULL) {
					pp = plist;
					page_sub(&plist, pp);
					page_io_unlock(pp);
					/*LINTED: constant in conditional ctx*/
					VN_DISPOSE(pp, B_INVAL, 0, kcred);
				}
				VM_STAT_ADD(page_create_found_one);
				goto fail;
			}
			ASSERT(flags & PG_WAIT);
			if (!page_lock(pp, SE_EXCL, phm, P_NO_RECLAIM)) {
				/*
				 * Start all over again if we blocked trying
				 * to lock the page.
				 */
				mutex_exit(phm);
				VM_STAT_ADD(page_create_page_lock_failed);
				phm = NULL;
				goto top;
			}
			mutex_exit(phm);
			phm = NULL;

			if (PP_ISFREE(pp)) {
				ASSERT(PP_ISAGED(pp) == 0);
				VM_STAT_ADD(pagecnt.pc_get_cache);
				page_list_sub(PG_CACHE_LIST, pp);
				PP_CLRFREE(pp);
				found_on_free++;
			}
		}

		/*
		 * Got a page!  It is locked.  Acquire the i/o
		 * lock since we are going to use the p_next and
		 * p_prev fields to link the requested pages together.
		 */
		page_io_lock(pp);
		page_add(&plist, pp);
		plist = plist->p_next;
		off += PAGESIZE;
		vaddr += PAGESIZE;
	}

	ASSERT((flags & PG_EXCL) ? (found_on_free == pages_req) : 1);
fail:
	if (npp != NULL) {
		/*
		 * Did not need this page after all.
		 * Put it back on the free list.
		 */
		VM_STAT_ADD(page_create_putbacks);
		PP_SETFREE(npp);
		PP_SETAGED(npp);
		npp->p_offset = (u_offset_t)-1;
		page_list_add(PG_FREE_LIST, npp, PG_LIST_TAIL);
		page_unlock(npp);

	}

	ASSERT(pages_req >= found_on_free);

	{
		uint_t overshoot = (uint_t)(pages_req - found_on_free);

		if (overshoot) {
			VM_STAT_ADD(page_create_overshoot);
			p = &pcf[pcf_index];
			p->pcf_touch = 1;
			mutex_enter(&p->pcf_lock);
			if (p->pcf_block) {
				p->pcf_reserve += overshoot;
			} else {
				p->pcf_count += overshoot;
				if (p->pcf_wait) {
					mutex_enter(&new_freemem_lock);
					if (freemem_wait) {
						cv_signal(&freemem_cv);
						p->pcf_wait--;
					} else {
						p->pcf_wait = 0;
					}
					mutex_exit(&new_freemem_lock);
				}
			}
			mutex_exit(&p->pcf_lock);
			/* freemem is approximate, so this test OK */
			if (!p->pcf_block)
				freemem += overshoot;
		}
	}

	return (plist);
}

/*
 * Put page on the "free" list.
 * The free list is really two lists maintained by
 * the PSM of whatever machine we happen to be on.
 */
void
page_free(page_t *pp, int dontneed)
{
	struct pcf	*p;
	uint_t		pcf_index;

	ASSERT((PAGE_EXCL(pp) &&
	    !page_iolock_assert(pp)) || panicstr);

	if (PP_ISFREE(pp))
		cmn_err(CE_PANIC, "page_free: page %p is free", (void *)pp);

	/*
	 * The page_struct_lock need not be acquired to examine these
	 * fields since the page has an "exclusive" lock.
	 */
	if (hat_page_is_mapped(pp) || pp->p_lckcnt != 0 || pp->p_cowcnt != 0)
		cmn_err(CE_PANIC, "page_free");

	ASSERT(!hat_page_getshare(pp));

	PP_SETFREE(pp);
	page_clr_all_props(pp);
	ASSERT(!hat_page_getshare(pp));

	/*
	 * Now we add the page to the head of the free list.
	 * But if this page is associated with a paged vnode
	 * then we adjust the head forward so that the page is
	 * effectively at the end of the list.
	 */
	if (pp->p_vnode == NULL) {
		/*
		 * Page has no identity, put it on the free list.
		 */
		PP_SETAGED(pp);
		pp->p_offset = (u_offset_t)-1;
		page_list_add(PG_FREE_LIST, pp, PG_LIST_TAIL);
		VM_STAT_ADD(pagecnt.pc_free_free);
		TRACE_5(TR_FAC_VM, TR_PAGE_FREE_FREE,
		    "page_free_free:pp %p vp %p off %llx "
		    "dontneed %x freemem %lx",
		    pp, pp->p_vnode, pp->p_offset, dontneed, freemem);
	} else {
		PP_CLRAGED(pp);

		if (!dontneed || nopageage) {
			/* move it to the tail of the list */
			page_list_add(PG_CACHE_LIST, pp, PG_LIST_TAIL);

			VM_STAT_ADD(pagecnt.pc_free_cache);
			TRACE_5(TR_FAC_VM, TR_PAGE_FREE_CACHE_TAIL,
			    "page_free_cache_tail:pp %p vp %p off %llx "
			    "dontneed %x freemem %lx",
			    pp, pp->p_vnode, pp->p_offset, dontneed, freemem);
		} else {
			page_list_add(PG_CACHE_LIST, pp, PG_LIST_HEAD);

			VM_STAT_ADD(pagecnt.pc_free_dontneed);
			TRACE_5(TR_FAC_VM, TR_PAGE_FREE_CACHE_HEAD,
			    "page_free_cache_head:pp %p vp %p off %llx "
			    "dontneed %x freemem %lx",
			    pp, pp->p_vnode, pp->p_offset, dontneed, freemem);
		}
	}
	page_unlock(pp);

	/*
	 * Now do the `freemem' accounting.
	 */
	pcf_index = PCF_INDEX();
	p = &pcf[pcf_index];
	p->pcf_touch = 1;

	mutex_enter(&p->pcf_lock);
	if (p->pcf_block) {
		p->pcf_reserve += 1;
	} else {
		p->pcf_count += 1;
		if (p->pcf_wait) {
			mutex_enter(&new_freemem_lock);
			/*
			 * Check to see if some other thread
			 * is actually waiting.  Another bucket
			 * may have woken it up by now.  If there
			 * are no waiters, then set our pcf_wait
			 * count to zero to avoid coming in here
			 * next time.  Also, since only one page
			 * was put on the free list, just wake
			 * up one waiter.
			 */
			if (freemem_wait) {
				cv_signal(&freemem_cv);
				p->pcf_wait--;
			} else {
				p->pcf_wait = 0;
			}
			mutex_exit(&new_freemem_lock);
		}
	}
	mutex_exit(&p->pcf_lock);

	/* freemem is approximate, so this test OK */
	if (!p->pcf_block)
		freemem += 1;
}

static int free_pages = 1;

/*
 * XXX - Turn this into a MACRO.
 *
 * It also looks like only segvn_unmap and segvn_free
 * call it.
 */
void
free_vp_pages(vnode_t *vp, u_offset_t off, size_t len)
{
	page_t *pp;
	u_offset_t eoff, pgoff;
	extern int swap_in_range(vnode_t *, u_offset_t, size_t);

	eoff = off + len;

	if (free_pages == 0)
		return;
	if (swap_in_range(vp, off, len))
		return;

	for (pgoff = off; pgoff < eoff; pgoff += PAGESIZE) {
		pp = page_lookup_nowait(vp, pgoff, SE_EXCL);
		if (pp != NULL)
			(void) page_release(pp, 1);
	}
}

/*
 * Reclaim the given page from the free list.
 * Returns 1 on success or 0 on failure.
 *
 * The page is unlocked if it can't be reclaimed (when freemem == 0).
 * If `lock' is non-null, it will be dropped and re-acquired if
 * the routine must wait while freemem is 0.
 *
 * As it turns out, boot_getpages() does this.  It picks a page,
 * based on where OBP mapped in some address, gets its pfn, searches
 * the memsegs, locks the page, then pulls it off the free list!
 */
int
page_reclaim(page_t *pp, kmutex_t *lock)
{
	struct pcf	*p;
	uint_t		pcf_index;
	struct cpu	*cpup;
	int		enough;
	uint_t		i;

	ASSERT(lock != NULL ? MUTEX_HELD(lock) : 1);
	ASSERT(PAGE_EXCL(pp) && PP_ISFREE(pp));

	/*
	 * If `freemem' is 0, we cannot reclaim this page from the
	 * freelist, so release every lock we might hold: the page,
	 * and the `lock' before blocking.
	 *
	 * The only way `freemem' can become 0 while there are pages
	 * marked free (have their p->p_free bit set) is when the
	 * system is low on memory and doing a page_create().  In
	 * order to guarantee that once page_create() starts acquiring
	 * pages it will be able to get all that it needs since `freemem'
	 * was decreased by the requested amount.  So, we need to release
	 * this page, and let page_create() have it.
	 *
	 * Since `freemem' being zero is not supposed to happen, just
	 * use the usual hash stuff as a starting point.  If that bucket
	 * is empty, then assume the worst, and start at the beginning
	 * of the pcf array.  If we always start at the beginning
	 * when acquiring more than one pcf lock, there won't be any
	 * deadlock problems.
	 */

	/* TODO: Do we need to test kcage_freemem if PG_NORELOC(pp)? */

	if (freemem <= throttlefree && !page_create_throttle(1l, 0)) {
		pcf_acquire_all();
		goto page_reclaim_nomem;
	}

	enough = 0;
	pcf_index = PCF_INDEX();
	p = &pcf[pcf_index];
	p->pcf_touch = 1;
	mutex_enter(&p->pcf_lock);
	if (p->pcf_count >= 1) {
		enough = 1;
		p->pcf_count--;
	}
	mutex_exit(&p->pcf_lock);

	if (!enough) {
		VM_STAT_ADD(page_reclaim_zero);
		/*
		 * Check again. Its possible that some other thread
		 * could have been right behind us, and added one
		 * to a list somewhere.  Acquire each of the pcf locks
		 * until we find a page.
		 */
		p = pcf;
		for (i = 0; i < PCF_FANOUT; i++) {
			p->pcf_touch = 1;
			mutex_enter(&p->pcf_lock);
			if (p->pcf_count >= 1) {
				p->pcf_count -= 1;
				enough = 1;
				break;
			}
			p++;
		}

		if (!enough) {
page_reclaim_nomem:
			/*
			 * We really can't have page `pp'.
			 * Time for the no-memory dance with
			 * page_free().  This is just like
			 * page_create_wait().  Plus the added
			 * attraction of releasing whatever mutex
			 * we held when we were called with in `lock'.
			 * Page_unlock() will wakeup any thread
			 * waiting around for this page.
			 */
			if (lock) {
				VM_STAT_ADD(page_reclaim_zero_locked);
				mutex_exit(lock);
			}
			page_unlock(pp);

			/*
			 * get this before we drop all the pcf locks.
			 */
			mutex_enter(&new_freemem_lock);

			p = pcf;
			for (i = 0; i < PCF_FANOUT; i++) {
				p->pcf_wait++;
				mutex_exit(&p->pcf_lock);
				p++;
			}

			freemem_wait++;
			cv_wait(&freemem_cv, &new_freemem_lock);
			freemem_wait--;

			mutex_exit(&new_freemem_lock);

			if (lock) {
				mutex_enter(lock);
			}
			return (0);
		}

		/*
		 * There was a page to be found.
		 * The pcf accounting has been done,
		 * though none of the pcf_wait flags have been set,
		 * drop the locks and continue on.
		 */
		while (p >= pcf) {
			mutex_exit(&p->pcf_lock);
			p--;
		}
	}

	/*
	 * freemem is not protected by any lock. Thus, we cannot
	 * have any assertion containing freemem here.
	 */
	freemem -= 1;

	VM_STAT_ADD(pagecnt.pc_reclaim);
	if (PP_ISAGED(pp)) {
		page_list_sub(PG_FREE_LIST, pp);
		TRACE_3(TR_FAC_VM, TR_PAGE_UNFREE_FREE,
		    "page_reclaim_free:pp %p age %x freemem %ld",
		    pp, PP_ISAGED(pp), freemem);
	} else {
		page_list_sub(PG_CACHE_LIST, pp);
		TRACE_2(TR_FAC_VM, TR_PAGE_UNFREE_CACHE,
		    "page_reclaim_cache:pp %p age %x",
		    pp, PP_ISAGED(pp));
	}

	/*
	 * clear the p_free & p_age bits since this page is no longer
	 * on the free list.  Notice that there was a brief time where
	 * a page is marked as free, but is not on the list.
	 *
	 * Set the reference bit to protect against immediate pageout.
	 */
	PP_CLRFREE(pp);
	PP_CLRAGED(pp);
	page_set_props(pp, P_REF);

	CPU_STAT_ENTER_K();
	cpup = CPU;	/* get cpup now that CPU cannot change */
	CPU_STAT_ADDQ(cpup, cpu_vminfo.pgrec, 1);
	CPU_STAT_ADDQ(cpup, cpu_vminfo.pgfrec, 1);
	CPU_STAT_EXIT_K();

	return (1);
}



/*
 * Destroy identity of the page and put it back on
 * the page free list.  Assumes that the caller has
 * acquired the "exclusive" lock on the page.
 */
void
page_destroy(page_t *pp, int dontfree)
{
	ASSERT((PAGE_EXCL(pp) &&
	    !page_iolock_assert(pp)) || panicstr);

	TRACE_3(TR_FAC_VM, TR_PAGE_DESTROY,
		"page_destroy:pp %p vp %p offset %llx",
		pp, pp->p_vnode, pp->p_offset);

	/*
	 * Unload translations, if any, then hash out the
	 * page to erase its identity.
	 */
	(void) hat_pageunload(pp, HAT_FORCE_PGUNLOAD);
	page_hashout(pp, (kmutex_t *)NULL);

	if (!dontfree) {
		/*
		 * Acquire the "freemem_lock" for availrmem.
		 * The page_struct_lock need not be acquired for lckcnt
		 * and cowcnt since the page has an "exclusive" lock.
		 */
		if ((pp->p_lckcnt != 0) || (pp->p_cowcnt != 0)) {
			mutex_enter(&freemem_lock);
			if (pp->p_lckcnt != 0) {
				availrmem++;
				pp->p_lckcnt = 0;
			}
			if (pp->p_cowcnt != 0) {
				availrmem += pp->p_cowcnt;
				pp->p_cowcnt = 0;
			}
			mutex_exit(&freemem_lock);
		}
		/*
		 * Put the page on the "free" list.
		 */
		page_free(pp, 0);
	}
}

/*
 * Similar to page_destroy(), but destroys pages which are
 * locked and known to be on the page free list.  Since
 * the page is known to be free and locked, no one can access
 * it.
 *
 * Also, the number of free pages does not change.
 */
void
page_destroy_free(page_t *pp)
{
	ASSERT(PAGE_EXCL(pp));
	ASSERT(PP_ISFREE(pp));
	ASSERT(pp->p_vnode);
	ASSERT(hat_page_getattr(pp, P_MOD | P_REF | P_RO) == 0);
	ASSERT(!hat_page_is_mapped(pp));
	ASSERT(PP_ISAGED(pp) == 0);

	VM_STAT_ADD(pagecnt.pc_destroy_free);
	page_list_sub(PG_CACHE_LIST, pp);

	page_hashout(pp, (kmutex_t *)NULL);
	ASSERT(pp->p_vnode == NULL);
	ASSERT(pp->p_offset == (u_offset_t)-1);
	ASSERT(pp->p_hash == NULL);

	PP_SETAGED(pp);
	page_list_add(PG_FREE_LIST, pp, PG_LIST_TAIL);
	page_unlock(pp);

	mutex_enter(&new_freemem_lock);
	if (freemem_wait) {
		cv_signal(&freemem_cv);
	}
	mutex_exit(&new_freemem_lock);
}

/*
 * Rename the page "opp" to have an identity specified
 * by [vp, off].  If a page already exists with this name
 * it is locked and destroyed.  Note that the page's
 * translations are not unloaded during the rename.
 *
 * This routine is used by the anon layer to "steal" the
 * original page and is not unlike destroying a page and
 * creating a new page using the same page frame.
 *
 * XXX -- Could deadlock if caller 1 tries to rename A to B while
 * caller 2 tries to rename B to A.
 */
void
page_rename(page_t *opp, vnode_t *vp, u_offset_t off)
{
	page_t		*pp;
	int		olckcnt = 0;
	int		ocowcnt = 0;
	kmutex_t	*phm;
	ulong_t		index;

	ASSERT(PAGE_EXCL(opp) && !page_iolock_assert(opp));
	ASSERT(MUTEX_NOT_HELD(page_vnode_mutex(vp)));
	ASSERT(PP_ISFREE(opp) == 0);

	VM_STAT_ADD(page_rename_count);

	TRACE_4(TR_FAC_VM, TR_PAGE_RENAME,
		"page rename:oldvp %p oldoff %llx vp %p off %llx",
		opp->p_vnode, opp->p_offset, vp, off);

	page_hashout(opp, (kmutex_t *)NULL);
	PP_CLRAGED(opp);

	/*
	 * Acquire the appropriate page hash lock, since
	 * we're going to rename the page.
	 */
	index = PAGE_HASH_FUNC(vp, off);
	phm = PAGE_HASH_MUTEX(index);
	mutex_enter(phm);
top:
	/*
	 * Look for an existing page with this name and destroy it if found.
	 * By holding the page hash lock all the way to the page_hashin()
	 * call, we are assured that no page can be created with this
	 * identity.  In the case when the phm lock is dropped to undo any
	 * hat layer mappings, the existing page is held with an "exclusive"
	 * lock, again preventing another page from being created with
	 * this identity.
	 */
	PAGE_HASH_SEARCH(index, pp, vp, off);
	if (pp != NULL) {
		VM_STAT_ADD(page_rename_exists);

		/*
		 * As it turns out, this is one of only two places where
		 * page_lock() needs to hold the passed in lock in the
		 * successful case.  In all of the others, the lock could
		 * be dropped as soon as the attempt is made to lock
		 * the page.  It is tempting to add yet another arguement,
		 * PL_KEEP or PL_DROP, to let page_lock know what to do.
		 */
		if (!page_lock(pp, SE_EXCL, phm, P_RECLAIM)) {
			/*
			 * Went to sleep because the page could not
			 * be locked.  We were woken up when the page
			 * was unlocked, or when the page was destroyed.
			 * In either case, `phm' was dropped while we
			 * slept.  Hence we should not just roar through
			 * this loop.
			 */
			goto top;
		}

		if (hat_page_is_mapped(pp)) {
			/*
			 * Unload translations.  Since we hold the
			 * exclusive lock on this page, the page
			 * can not be changed while we drop phm.
			 * This is also not a lock protocol violation,
			 * but rather the proper way to do things.
			 */
			mutex_exit(phm);
			(void) hat_pageunload(pp, HAT_FORCE_PGUNLOAD);
			mutex_enter(phm);
		}
		page_hashout(pp, phm);
	}
	/*
	 * Hash in the page with the new identity.
	 * page_hashin drops phm.
	 */
	if (!page_hashin(opp, vp, off, phm)) {
		/*
		 * We were holding phm while we searched for [vp, off]
		 * and only dropped phm if we found and locked a page.
		 * If we can't create this page now, then some thing
		 * is really broken.
		 */
		cmn_err(CE_PANIC, "page_rename: Can't hash in page: %p",
		    (void *)pp);
	}

	ASSERT(MUTEX_NOT_HELD(phm));

	/*
	 * Now that we have dropped phm, lets get around to finishing up
	 * with pp.
	 */
	if (pp != NULL) {
		ASSERT(!hat_page_is_mapped(pp));
		/*
		 * Save the locks for transfer to the new page and then
		 * clear them so page_free doesn't think they're important.
		 * The page_struct_lock need not be acquired for lckcnt and
		 * cowcnt since the page has an "exclusive" lock.
		 */
		olckcnt = pp->p_lckcnt;
		ocowcnt = pp->p_cowcnt;
		pp->p_lckcnt = pp->p_cowcnt = 0;

		/*
		 * Put the page on the "free" list after we drop
		 * the lock.  The less work under the lock the better.
		 */
		/*LINTED: constant in conditional context*/
		VN_DISPOSE(pp, B_FREE, 0, kcred);
	}

	/*
	 * Transfer the lock count from the old page (if any).
	 * The page_struct_lock need not be acquired for lckcnt and
	 * cowcnt since the page has an "exclusive" lock.
	 */
	opp->p_lckcnt += olckcnt;
	opp->p_cowcnt += ocowcnt;
}

/*
 * This function flips pages between "pp_to" and "pp_from", so that "pp_to"
 * will assume "pp_from"'s identity and vice versa, assuming all the
 * mappings to the pages have been successfully flipped by hat_pageflip.
 * It will deal with the rest of stuff here (mainly the page_hash table
 * and vp lists).
 * "hat_pageflip" is responsible for swapping p_mapping, p_index, p_share,
 * p_inuse, p_wanted fields. All the rest have to be handled here.
 *
 * Somewhere in the code it also makes an assumption that pages on
 * "pp_from" belong to kvp.
 */
void
page_flip(page_t *pp_to, page_t *pp_from)
{
	page_t	*tmpp;
	struct	vnode *vp_to, *vp_from;
	kmutex_t *phm_to, *phm_from;
	ulong_t	index_to, index_from;
	uchar_t	tmp;

	/*
	 * XXX - How about P_PNC, (Are we going to page flip w/ sx
	 * memory?) P_TNC (It shouldn't have any VAC conflict because
	 * we only have one mapping.)
	 */
	hat_setrefmod(pp_from);

	/*
	 * We try to rehash two pages in one shot. This should be much
	 * faster than first hashing out then hashing in pages.
	 */
	vp_to = pp_to->p_vnode;
	vp_from = pp_from->p_vnode;
	index_to = PAGE_HASH_FUNC(vp_to, pp_to->p_offset);
	index_from = PAGE_HASH_FUNC(vp_from, pp_from->p_offset);

	phm_to = PAGE_HASH_MUTEX(index_to);
	phm_from = PAGE_HASH_MUTEX(index_from);

	/* We define a lock order here to be bigger-numbered lock first. */
	if ((uintptr_t)phm_from > (uintptr_t)phm_to) {
		mutex_enter(phm_from);
		mutex_enter(phm_to);
	} else if (phm_from == phm_to) {
		mutex_enter(phm_to);
	} else {
		mutex_enter(phm_to);
		mutex_enter(phm_from);
	}

	if (index_to != index_from) {
		page_t	**hpp;

		hpp = &page_hash[index_to];
		for (;;) {
			if (*hpp == pp_to)
				break;
			hpp = &(*hpp)->p_hash;
		}
		*hpp = pp_from;

		hpp = &page_hash[index_from];
		for (;;) {
			if (*hpp == pp_from)
				break;
			hpp = &(*hpp)->p_hash;
		}
		*hpp = pp_to;

		tmpp = pp_from->p_hash;
		pp_from->p_hash = pp_to->p_hash;
		pp_to->p_hash = tmpp;
	}

	/*
	 * The following has to be done while the hash mutex is still
	 * being held. Otherwise a new page with the same id may get
	 * created inadvertently.
	 *
	 * The lock order defined by VM is p_selock -> hash_mutex.
	 */
	pp_from->p_vnode = vp_to;
	index_from = pp_from->p_offset;
	pp_from->p_offset = pp_to->p_offset;
	pp_to->p_vnode = vp_from;
	pp_to->p_offset = index_from;

	mutex_exit(phm_to);
	if (phm_from != phm_to)
		mutex_exit(phm_from);

	ASSERT(vp_from == &kvp);
	/*
	 * At this moment, pages are on the new hash chain, but the
	 * old v_pages list. Is this a problem?
	 * (Since we are holding the excl lock. This should be ok.)
	 * TODO: Not OK - pvn_vplist_dirty() has a problem (BugId 4094021)
	 */

	/* Define the lock order to be kvp first */
	phm_from = page_vnode_mutex(vp_from);
		mutex_enter(phm_from);
	phm_to = page_vnode_mutex(vp_to);
	if (phm_to != phm_from)
		mutex_enter(phm_to);

#ifdef slow
	page_vpsub(&vp_to->v_pages, pp_to);
	page_vpsub(&vp_from->v_pages, pp_from);
	if (vp_to->v_pages)
		page_vpadd(&vp_to->v_pages->p_vpprev->p_vpnext, pp_from);
	else
		page_vpadd(&vp_to->v_pages, pp);
	if (vp_from->v_pages)
		page_vpadd(&vp_from->v_pages->p_vpprev->p_vpnext, pp_to);
	else
		page_vpadd(&vp_from->v_pages, pp);
#else
	/*
	 * page_hashin says we have to add pages to the end of vplist
	 * in order for pvn_vplist_dirty to work properly.
	 * Why? (I don't see it in pvn_vplist_dirty except for fairness.
	 * But why is fairness important?)
	 */
	pp_from->p_vpprev->p_vpnext = pp_to;
	tmpp = pp_from->p_vpnext;
	tmpp->p_vpprev = pp_to;
	if (pp_to->p_vpnext == pp_to) {
		pp_to->p_vpnext = tmpp;
		pp_to->p_vpprev = pp_from->p_vpprev;
		vp_to->v_pages = pp_from->p_vpnext =
		    pp_from->p_vpprev = pp_from;
	} else {
		pp_to->p_vpnext->p_vpprev = pp_from;
		pp_to->p_vpprev->p_vpnext = pp_from;
		pp_from->p_vpnext = pp_to->p_vpnext;
		pp_to->p_vpnext = tmpp;
		tmpp = pp_from->p_vpprev;
		pp_from->p_vpprev = pp_to->p_vpprev;
		pp_to->p_vpprev = tmpp;
		if (vp_to->v_pages == pp_to)
			vp_to->v_pages = pp_from;
	}
	if (vp_from->v_pages == pp_from)
		vp_from->v_pages = pp_to;
#endif

	mutex_exit(phm_to);
	if (phm_from != phm_to)
		mutex_exit(phm_from);
	/*
	 * The page_struct_lock need not be acquired for lckcnt and
	 * cowcnt since the page has an "exclusive" lock.
	 */
	ASSERT(pp_from->p_lckcnt == 0);
	ASSERT(pp_from->p_cowcnt == 0);
	pp_from->p_lckcnt = pp_to->p_lckcnt;
	pp_from->p_cowcnt = pp_to->p_cowcnt;
	pp_to->p_lckcnt = pp_to->p_cowcnt = 0;

	/* XXX - Do we need to protect fsdata? */
	tmp = pp_from->p_fsdata;
	pp_from->p_fsdata = pp_to->p_fsdata;
	pp_to->p_fsdata = tmp;
	page_unlock(pp_from);

	/*
	 * pp_to will stay shared-locked because it becomes a kvp page
	 * now.
	 */
	page_downgrade(pp_to);
}

/*
 * Add page `pp' to both the hash and vp chains for [vp, offset].
 *
 * It is required that pages be added to the end of the vplist
 * in order for pvn_vplist_dirty to work properly.
 *
 * Returns 1 on success and 0 on failure.
 * If phm is non-NULL, it is dropped.
 */
int
page_hashin(page_t *pp, vnode_t *vp, u_offset_t offset, kmutex_t *phm)
{
	page_t		**hpp;
	page_t		*tp;
	kmutex_t	*vphm;
	ulong_t		index;

	ASSERT(PAGE_EXCL(pp));
	ASSERT(vp != NULL);

	/*
	 * Technically, if phm was non-NULL and held, it would be fine
	 * if the vnode mutex was also held.  But since we are going
	 * to pick up the vnode mutex below, it had better not be held now.
	 */
	ASSERT(MUTEX_NOT_HELD(page_vnode_mutex(vp)));

	TRACE_3(TR_FAC_VM, TR_PAGE_HASHIN,
		"page_hashin:pp %p vp %p offset %llx",
		pp, vp, offset);

	VM_STAT_ADD(hashin_count);
	/*
	 * Be sure to set these up before the page is inserted on the hash
	 * list.  As soon as the page is placed on the list some other
	 * thread might get confused and wonder how this page could
	 * possibly hash to this list.
	 */
	pp->p_vnode = vp;
	pp->p_offset = offset;

	index = PAGE_HASH_FUNC(vp, offset);
	hpp = &page_hash[index];

	if (phm == NULL) {
		VM_STAT_ADD(hashin_not_held);
		phm = PAGE_HASH_MUTEX(index);
		mutex_enter(phm);
	}
	ASSERT(phm == PAGE_HASH_MUTEX(index));
	ASSERT(MUTEX_HELD(phm));

	for (tp = *hpp; tp != NULL; tp = tp->p_hash) {
		if (tp->p_vnode == vp && tp->p_offset == offset) {
			/*
			 * This page is already here!
			 */
			mutex_exit(phm);
			VM_STAT_ADD(hashin_already);
			pp->p_vnode = NULL;
			pp->p_offset = offset;
			return (0);
		}
	}

	pp->p_hash = *hpp;
	*hpp = pp;
	mutex_exit(phm);

	/*
	 * Add the page to the end of the linked list of pages.
	 * Assuming that we are traversing the vnode in forward,
	 * sequential order, this will leave the page list sorted.
	 */
	vphm = page_vnode_mutex(vp);
	mutex_enter(vphm);
	if (vp->v_pages)
		page_vpadd(&vp->v_pages->p_vpprev->p_vpnext, pp);
	else
		page_vpadd(&vp->v_pages, pp);
	mutex_exit(vphm);
	return (1);
}

/*
 * Adds a page structure representing the physical page
 * to the hash/vp chains for [vp, offset].
 *
 * Returns 1 on success and 0 on failure.
 */
int
page_num_hashin(pfn_t pfn, vnode_t *vp, u_offset_t offset)
{
	page_t *pp;
	int ret;

	/*
	 * Obtain the page structure representing the physical page.
	 */
	pp = page_numtopp(pfn, SE_EXCL);
	if (pp == NULL || PP_ISFREE(pp))
		return (0);

	ret = page_hashin(pp, vp, offset, (kmutex_t *)NULL);
	return (ret);
}

/*
 * Remove page ``pp'' from the hash and vp chains and remove vp association.
 *
 * When `phm' is non-NULL it contains the address of the mutex protecting the
 * hash list pp is on.  It is not dropped.
 */
void
page_hashout(page_t *pp, kmutex_t *phm)
{
	page_t		**hpp, *hp;
	struct vnode	*vp;
	ulong_t		index;
	kmutex_t	*nphm;
	kmutex_t	*vphm;
	kmutex_t	*sep;

	ASSERT(phm != NULL ? MUTEX_HELD(phm) : 1);
	ASSERT(pp->p_vnode != NULL);
	ASSERT((PAGE_EXCL(pp) &&
	    !page_iolock_assert(pp)) || panicstr);
	ASSERT(MUTEX_NOT_HELD(page_vnode_mutex(pp->p_vnode)));

	vp = pp->p_vnode;

	TRACE_3(TR_FAC_VM, TR_PAGE_HASHOUT,
		"page_hashout:pp %p vp %p offset %llx",
		pp, vp, pp->p_offset);

	/* Kernel probe */
	TNF_PROBE_2(page_unmap, "vm pagefault", /* CSTYLED */,
	    tnf_opaque, vnode, vp,
	    tnf_offset, offset, pp->p_offset);

	/*
	 * First, take pp off of its hash chain.
	 */
	VM_STAT_ADD(hashout_count);
	index = PAGE_HASH_FUNC(vp, pp->p_offset);
	hpp = &page_hash[index];
	if (phm == NULL) {
		VM_STAT_ADD(hashout_not_held);
		nphm = PAGE_HASH_MUTEX(index);
		mutex_enter(nphm);
	}
	ASSERT(phm ? phm == PAGE_HASH_MUTEX(index) : 1);

	for (;;) {
		hp = *hpp;
		if (hp == pp)
			break;
		if (hp == NULL)
			cmn_err(CE_PANIC, "page_hashout");
		hpp = &hp->p_hash;
	}
	*hpp = pp->p_hash;

	if (phm == NULL)
		mutex_exit(nphm);

	/*
	 * Then, remove it from its associated vnode.
	 */
	vphm = page_vnode_mutex(vp);
	mutex_enter(vphm);
	if (vp->v_pages)
		page_vpsub(&vp->v_pages, pp);
	mutex_exit(vphm);

	pp->p_hash = NULL;
	page_clr_all_props(pp);
	pp->p_vnode = NULL;
	pp->p_offset = (u_offset_t)-1;

	/*
	 * Wake up processes waiting for this page.  The page's
	 * identity has been changed, and is probably not the
	 * desired page any longer.
	 */
	sep = page_se_mutex(pp);
	mutex_enter(sep);
	if (CV_HAS_WAITERS(&pp->p_cv))
		cv_broadcast(&pp->p_cv);
	mutex_exit(sep);
}

/*
 * Add the page to the front of a linked list of pages
 * using the p_next & p_prev pointers for the list.
 * The caller is responsible for protecting the list pointers.
 */
void
page_add(page_t **ppp, page_t *pp)
{
	ASSERT(PAGE_EXCL(pp) || (PAGE_SHARED(pp) && page_iolock_assert(pp)));

	if (*ppp == NULL) {
		pp->p_next = pp->p_prev = pp;
	} else {
		pp->p_next = *ppp;
		pp->p_prev = (*ppp)->p_prev;
		(*ppp)->p_prev = pp;
		pp->p_prev->p_next = pp;
	}
	*ppp = pp;
}

/*
 * Remove this page from a linked list of pages
 * using the p_next & p_prev pointers for the list.
 *
 * The caller is responsible for protecting the list pointers.
 */
void
page_sub(page_t **ppp, page_t *pp)
{
	ASSERT((PP_ISFREE(pp)) ? 1 :
	    (PAGE_EXCL(pp)) || (PAGE_SHARED(pp) && page_iolock_assert(pp)));

	if (*ppp == NULL || pp == NULL) {
		cmn_err(CE_PANIC, "page_sub: bad arg(s): pp %p, *ppp %p",
		    (void *)pp, (void *)(*ppp));
	}

	if (*ppp == pp)
		*ppp = pp->p_next;		/* go to next page */

	if (*ppp == pp)
		*ppp = NULL;			/* page list is gone */
	else {
		pp->p_prev->p_next = pp->p_next;
		pp->p_next->p_prev = pp->p_prev;
	}
	pp->p_prev = pp->p_next = pp;		/* make pp a list of one */
}

/*
 * Break page list cppp into two lists with npages in the first list.
 * The tail is returned in nppp.
 */
void
page_list_break(page_t **oppp, page_t **nppp, pgcnt_t npages)
{
	page_t *s1pp = *oppp;
	page_t *s2pp;
	page_t *e1pp, *e2pp;
	long n = 0;

	if (s1pp == NULL) {
		*nppp = NULL;
		return;
	}
	if (npages == 0) {
		*nppp = s1pp;
		*oppp = NULL;
		return;
	}
	for (n = 0, s2pp = *oppp; n < npages; n++) {
		s2pp = s2pp->p_next;
	}
	/* Fix head and tail of new lists */
	e1pp = s2pp->p_prev;
	e2pp = s1pp->p_prev;
	s1pp->p_prev = e1pp;
	e1pp->p_next = s1pp;
	s2pp->p_prev = e2pp;
	e2pp->p_next = s2pp;

	/* second list empty */
	if (s2pp == s1pp) {
		*oppp = s1pp;
		*nppp = NULL;
	} else {
		*oppp = s1pp;
		*nppp = s2pp;
	}
}

/*
 * Concatenate page list nppp onto the end of list ppp.
 */
void
page_list_concat(page_t **ppp, page_t **nppp)
{
	page_t *s1pp, *s2pp, *e1pp, *e2pp;

	if (*nppp == NULL) {
		return;
	}
	if (*ppp == NULL) {
		*ppp = *nppp;
		return;
	}
	s1pp = *ppp;
	e1pp =  s1pp->p_prev;
	s2pp = *nppp;
	e2pp = s2pp->p_prev;
	s1pp->p_prev = e2pp;
	e2pp->p_next = s1pp;
	e1pp->p_next = s2pp;
	s2pp->p_prev = e1pp;
}

/*
 * return the next page in the page list
 */
page_t *
page_list_next(page_t *pp)
{
	return (pp->p_next);
}


/*
 * Add the page to the front of the linked list of pages
 * using p_vpnext/p_vpprev pointers for the list.
 *
 * The caller is responsible for protecting the lists.
 */
void
page_vpadd(page_t **ppp, page_t *pp)
{
	if (*ppp == NULL) {
		pp->p_vpnext = pp->p_vpprev = pp;
	} else {
		pp->p_vpnext = *ppp;
		pp->p_vpprev = (*ppp)->p_vpprev;
		(*ppp)->p_vpprev = pp;
		pp->p_vpprev->p_vpnext = pp;
	}
	*ppp = pp;
}

/*
 * Remove this page from the linked list of pages
 * using p_vpnext/p_vpprev pointers for the list.
 *
 * The caller is responsible for protecting the lists.
 */
void
page_vpsub(page_t **ppp, page_t *pp)
{
	if (*ppp == NULL || pp == NULL) {
		cmn_err(CE_PANIC, "page_vpsub: bad arg(s): pp %p, *ppp %p",
		    (void *)pp, (void *)(*ppp));
	}

	if (*ppp == pp)
		*ppp = pp->p_vpnext;		/* go to next page */

	if (*ppp == pp)
		*ppp = NULL;			/* page list is gone */
	else {
		pp->p_vpprev->p_vpnext = pp->p_vpnext;
		pp->p_vpnext->p_vpprev = pp->p_vpprev;
	}
	pp->p_vpprev = pp->p_vpnext = pp;	/* make pp a list of one */
}

/*
 * Lock a physical page into memory "long term".  Used to support "lock
 * in memory" functions.  Accepts the page to be locked, and a cow variable
 * to indicate whether a the lock will travel to the new page during
 * a potential copy-on-write.
 *
 * XXX - Use incr_cowcnt/incr_lckcnt to simplify common code
 *	 RFE 4141534
 */
int
page_pp_lock(
	page_t *pp,			/* page to be locked */
	int cow,			/* cow lock */
	int kernel)			/* must succeed -- ignore checking */
{
	int r = 0;			/* result -- assume failure */

	ASSERT(PAGE_LOCKED(pp));

	page_struct_lock(pp);
	/*
	 * Acquire the "freemem_lock" for availrmem.
	 */
	if (cow) {
		mutex_enter(&freemem_lock);
		if ((availrmem > pages_pp_maximum) &&
				(pp->p_cowcnt < (ushort_t)PAGE_LOCK_MAXIMUM)) {
			availrmem--;
			mutex_exit(&freemem_lock);
			r = 1;
			if (++pp->p_cowcnt == (ushort_t)PAGE_LOCK_MAXIMUM) {
				cmn_err(CE_WARN,
				"COW lock limit reached on pfn 0x%lx\n",
					page_pptonum(pp));
			}
		} else
			mutex_exit(&freemem_lock);
	} else {
		if (pp->p_lckcnt) {
			if (pp->p_lckcnt < (ushort_t)PAGE_LOCK_MAXIMUM) {
				r = 1;
				if (++pp->p_lckcnt ==
						(ushort_t)PAGE_LOCK_MAXIMUM) {
					cmn_err(CE_WARN, "Page lock limit "
					"reached on pfn 0x%lx\n",
						page_pptonum(pp));
				}
			}
		} else {
			if (kernel) {
				/* availrmem accounting done by caller */
				++pp->p_lckcnt;
				r = 1;
			} else {
				mutex_enter(&freemem_lock);
				if (availrmem > pages_pp_maximum) {
					availrmem--;
					++pp->p_lckcnt;
					r = 1;
				}
				mutex_exit(&freemem_lock);
			}
		}
	}
	page_struct_unlock(pp);
	return (r);
}

/*
 * Decommit a lock on a physical page frame.  Account for cow locks if
 * appropriate.
 */
void
page_pp_unlock(
	page_t *pp,			/* page to be unlocked */
	int cow,			/* expect cow lock */
	int kernel)			/* this was a kernel lock */
{
	ASSERT(PAGE_LOCKED(pp));

	page_struct_lock(pp);
	/*
	 * Acquire the "freemem_lock" for availrmem.
	 * If cowcnt or lcknt is already 0 do nothing; i.e., we
	 * could be called to unlock even if nothing is locked. This could
	 * happen if locked file pages were truncated (removing the lock)
	 * and the file was grown again and new pages faulted in; the new
	 * pages are unlocked but the segment still thinks they're locked.
	 */
	if (cow) {
		if (pp->p_cowcnt) {
			mutex_enter(&freemem_lock);
			pp->p_cowcnt--;
			availrmem++;
			mutex_exit(&freemem_lock);
		}
	} else {
		if (pp->p_lckcnt && --pp->p_lckcnt == 0) {
			if (!kernel) {
				mutex_enter(&freemem_lock);
				availrmem++;
				mutex_exit(&freemem_lock);
			}
		}
	}
	page_struct_unlock(pp);
}

/*
 * See Statement at the beginning of segvn_lockop() regarding
 * the way we handle cowcnts and lckcnts.
 *
 * Transfer cowcnt on 'opp' to cowcnt on 'npp' if the vpage
 * that breaks COW has PROT_WRITE.
 *
 * Note that, we may also break COW in case we are softlocking
 * on read access during physio;
 * in this softlock case, the vpage may not have PROT_WRITE.
 * So, we need to transfer lckcnt on 'opp' to lckcnt on 'npp'
 * if the vpage doesn't have PROT_WRITE.
 *
 * This routine is never called if we are stealing a page
 * in anon_private.
 */
int
page_pp_useclaim(
	page_t *opp,		/* original page frame losing lock */
	page_t *npp,		/* new page frame gaining lock */
	uint_t	write_perm) 	/* set if vpage has PROT_WRITE */
{
	ASSERT(PAGE_LOCKED(opp));
	ASSERT(PAGE_LOCKED(npp));

	page_struct_lock(opp);

	ASSERT(npp->p_cowcnt == 0);
	ASSERT(npp->p_lckcnt == 0);

	/* Don't use claim if nothing is locked (see page_pp_unlock above) */
	if ((write_perm && !opp->p_cowcnt) ||
			(!write_perm && !opp->p_lckcnt)) {
		page_struct_unlock(opp);
		return (1);
	}

	if (write_perm) {
		npp->p_cowcnt++;
		ASSERT(opp->p_cowcnt != 0);
		opp->p_cowcnt--;
	} else {

		ASSERT(opp->p_lckcnt != 0);

		/*
		 * We need to decrement availrmem only if
		 * p_lckcnt on original page is more than 1
		 * because we require an additional copy to be
		 * locked. Note that read-only copy of original
		 * page is still going to be locked after locking
		 * new page.
		 */
		if (opp->p_lckcnt > 1) {
			mutex_enter(&freemem_lock);
			if (availrmem > pages_pp_maximum) {
				availrmem--;
				mutex_exit(&freemem_lock);
			} else {
				mutex_exit(&freemem_lock);
				page_struct_unlock(opp);
				return (0);
			}
		}
		/*
		 * Don't decrement availrmem if p_lckcnt on
		 * original page is 1. Here, we are unlocking
		 * read-only copy belonging to original page and
		 * are locking a copy belonging to new page.
		 */
		npp->p_lckcnt++;
		opp->p_lckcnt--;
	}
	page_struct_unlock(opp);
	return (1);
}

/*
 * This routine reserves availrmem for npages;
 * 	flags: KM_NOSLEEP or KM_SLEEP
 * 	returns 1 on success or 0 on failure
 */
int
page_resv(pgcnt_t npages, uint_t flags)
{
	mutex_enter(&freemem_lock);
	while (availrmem < tune.t_minarmem + npages) {
		if (flags & KM_NOSLEEP) {
			mutex_exit(&freemem_lock);
			return (0);
		}
		mutex_exit(&freemem_lock);
		page_needfree(npages);
		kmem_reap();
		delay(hz >> 2);
		page_needfree(-(spgcnt_t)npages);
		mutex_enter(&freemem_lock);
	}
	availrmem -= npages;
	mutex_exit(&freemem_lock);
	return (1);
}

/*
 * This routine unreserves availrmem for npages;
 */
void
page_unresv(pgcnt_t npages)
{
	mutex_enter(&freemem_lock);
	availrmem += npages;
	mutex_exit(&freemem_lock);
}

/*
 * Simple claim adjust functions -- used to support changes in
 * claims due to changes in access permissions.  Used by segvn_setprot().
 *
 * XXX - Use incr_cowcnt to simplify the common code
 *	 RFE 4141534
 */
int
page_addclaim(page_t *pp)
{
	int r = 0;			/* result */

	ASSERT(PAGE_LOCKED(pp));

	page_struct_lock(pp);
	ASSERT(pp->p_lckcnt != 0);

	if (pp->p_lckcnt == 1) {
		if (pp->p_cowcnt < (ushort_t)PAGE_LOCK_MAXIMUM) {
			--pp->p_lckcnt;
			r = 1;
			if (++pp->p_cowcnt == (ushort_t)PAGE_LOCK_MAXIMUM) {
				cmn_err(CE_WARN,
					"COW lock limit reached on pfn 0x%lx\n",
						page_pptonum(pp));
			}
		}
	} else {
		mutex_enter(&freemem_lock);
		if ((availrmem > pages_pp_maximum) &&
				(pp->p_cowcnt < (ushort_t)PAGE_LOCK_MAXIMUM)) {
			--availrmem;
			mutex_exit(&freemem_lock);
			--pp->p_lckcnt;
			r = 1;
			if (++pp->p_cowcnt == (ushort_t)PAGE_LOCK_MAXIMUM) {
				cmn_err(CE_WARN,
					"COW lock limit reached on pfn 0x%lx\n",
						page_pptonum(pp));
			}
		} else
			mutex_exit(&freemem_lock);
	}
	page_struct_unlock(pp);
	return (r);
}

/*
 * XXX - Use incr_lckcnt to simplify common code; RFE 4141534
 */

int
page_subclaim(page_t *pp)
{
	int r = 0;

	ASSERT(PAGE_LOCKED(pp));

	page_struct_lock(pp);
	ASSERT(pp->p_cowcnt != 0);

	if (pp->p_lckcnt) {
		if (pp->p_lckcnt < (ushort_t)PAGE_LOCK_MAXIMUM) {
			r = 1;
			/*
			 * for availrmem
			 */
			mutex_enter(&freemem_lock);
			availrmem++;
			mutex_exit(&freemem_lock);

			pp->p_cowcnt--;

			if (++pp->p_lckcnt == (ushort_t)PAGE_LOCK_MAXIMUM) {
				cmn_err(CE_WARN,
				"Page lock limit reached on pfn 0x%lx\n",
					page_pptonum(pp));
			}
		}
	} else {
		r = 1;
		pp->p_cowcnt--;
		pp->p_lckcnt++;
	}
	page_struct_unlock(pp);
	return (r);
}

page_t *
page_numtopp(pfn_t pfnum, se_t se)
{
	page_t *pp;

retry:
	pp = page_numtopp_nolock(pfnum);
	if (pp == NULL) {
		return ((page_t *)NULL);
	}

	/*
	 * Acquire the appropriate lock on the page.
	 */
	while (!page_lock(pp, se, (kmutex_t *)NULL, P_RECLAIM)) {
		if (page_pptonum(pp) != pfnum)
			goto retry;
		continue;
	}

	if (page_pptonum(pp) != pfnum) {
		page_unlock(pp);
		goto retry;
	}

	return (pp);
}

page_t *
page_numtopp_noreclaim(pfn_t pfnum, se_t se)
{
	page_t *pp;

retry:
	pp = page_numtopp_nolock(pfnum);
	if (pp == NULL) {
		return ((page_t *)NULL);
	}

	/*
	 * Acquire the appropriate lock on the page.
	 */
	while (!page_lock(pp, se, (kmutex_t *)NULL, P_NO_RECLAIM)) {
		if (page_pptonum(pp) != pfnum)
			goto retry;
		continue;
	}

	if (page_pptonum(pp) != pfnum) {
		page_unlock(pp);
		goto retry;
	}

	return (pp);
}

/*
 * This routine is like page_numtopp, but will only return page structs
 * for pages which are ok for loading into hardware using the page struct.
 */
page_t *
page_numtopp_nowait(pfn_t pfnum, se_t se)
{
	page_t *pp;

retry:
	pp = page_numtopp_nolock(pfnum);
	if (pp == NULL) {
		return ((page_t *)NULL);
	}

	/*
	 * Try to acquire the appropriate lock on the page.
	 */
	if (PP_ISFREE(pp))
		pp = NULL;
	else {
		if (!page_trylock(pp, se))
			pp = NULL;
		else {
			if (page_pptonum(pp) != pfnum) {
				page_unlock(pp);
				goto retry;
			}
			if (PP_ISFREE(pp)) {
				page_unlock(pp);
				pp = NULL;
			}
		}
	}
	return (pp);
}

/*
 * Returns a count of dirty pages that are in the process
 * of being written out.  If 'cleanit' is set, try to push the page.
 */
pgcnt_t
page_busy(int cleanit)
{
	page_t *page0 = page_first();
	page_t *pp = page0;
	pgcnt_t nppbusy = 0;
	u_offset_t off;

	do {
		vnode_t *vp = pp->p_vnode;

		/*
		 * A page is a candidate for syncing if it is:
		 *
		 * (a)	On neither the freelist nor the cachelist
		 * (b)	Hashed onto a vnode
		 * (c)	Not a kernel page
		 * (d)	Dirty
		 * (e)	Not part of a swapfile
		 * (f)	Backed by a filesystem which doesn't have a
		 *	stubbed-out sync operation
		 */
		if (!PP_ISFREE(pp) && vp != NULL && vp != &kvp &&
		    hat_ismod(pp) && !IS_SWAPVP(vp) &&
		    vp->v_vfsp->vfs_op->vfs_sync != fs_sync) {
			nppbusy++;
			if (!cleanit)
				continue;
			if (!page_trylock(pp, SE_EXCL))
				continue;

			if (PP_ISFREE(pp) || vp == NULL || IS_SWAPVP(vp) ||
			    pp->p_lckcnt != 0 || pp->p_cowcnt != 0 ||
			    !(hat_pagesync(pp,
			    HAT_SYNC_DONTZERO | HAT_SYNC_STOPON_MOD) & P_MOD)) {
				page_unlock(pp);
				continue;
			}
			off = pp->p_offset;
			VN_HOLD(vp);
			page_unlock(pp);
			(void) VOP_PUTPAGE(vp, off, PAGESIZE,
			    B_ASYNC | B_FREE, kcred);
			VN_RELE(vp);
		}
	} while ((pp = page_next(pp)) != page0);

	return (nppbusy);
}

void page_invalidate_pages(void);

/*
 * callback handler to vm sub-system
 *
 * callers make sure no recursive entries to this func.
 */
/*ARGSUSED*/
boolean_t
callb_vm_cpr(void *arg, int code)
{
	if (code == CB_CODE_CPR_CHKPT)
		page_invalidate_pages();
	return (B_TRUE);
}

/*
 * Invalidate all pages of the system.
 * It shouldn't be called until all user page activities are all stopped.
 */
void
page_invalidate_pages()
{
	page_t *pp;
	page_t *page0;
	pgcnt_t nbusypages;
	int retry = 0;
	const int MAXRETRIES = 4;
#ifdef sparc
	extern struct vnode prom_ppages;
#endif sparc

top:
	/*
	 * Flush dirty pages and destory the clean ones.
	 */
	nbusypages = 0;

	pp = page0 = page_first();
	do {
		struct vnode	*vp;
		u_offset_t	offset;
		int		mod;

		/*
		 * skip the page if it has no vnode or the page associated
		 * with the kernel vnode or prom allocated kernel mem.
		 */
#ifdef sparc
		if ((vp = pp->p_vnode) == NULL || vp == &kvp ||
		    vp == &prom_ppages)
#else /* x86 doesn't have prom or prom_ppage */
		if ((vp = pp->p_vnode) == NULL || vp == &kvp)
#endif sparc
			continue;

		/*
		 * skip the page which is already free invalidated.
		 */
		if (PP_ISFREE(pp) && PP_ISAGED(pp))
			continue;

		/*
		 * skip pages that are already locked or can't be "exclusively"
		 * locked or are already free.  After we lock the page, check
		 * the free and age bits again to be sure it's not destroied
		 * yet.
		 * To achieve max. parallelization, we use page_trylock instead
		 * of page_lock so that we don't get block on individual pages
		 * while we have thousands of other pages to process.
		 */
		if (!page_trylock(pp, SE_EXCL)) {
			nbusypages++;
			continue;
		} else if (PP_ISFREE(pp)) {
			if (!PP_ISAGED(pp)) {
				page_destroy_free(pp);
			} else {
				page_unlock(pp);
			}
			continue;
		}
		/*
		 * Is this page involved in some I/O? shared?
		 *
		 * The page_struct_lock need not be acquired to
		 * examine these fields since the page has an
		 * "exclusive" lock.
		 */
		if (pp->p_lckcnt != 0 || pp->p_cowcnt != 0) {
			page_unlock(pp);
			continue;
		}

		if (vp->v_type == VCHR) {
			cmn_err(CE_PANIC, "vp->v_type == VCHR");
		}

		/*
		 * Check the modified bit. Leave the bits alone in hardware
		 * (they will be modified if we do the putpage).
		 */
		mod = (hat_pagesync(pp, HAT_SYNC_DONTZERO | HAT_SYNC_STOPON_MOD)
			& P_MOD);
		if (mod) {
			offset = pp->p_offset;
			/*
			 * Hold the vnode before releasing the page lock
			 * to prevent it from being freed and re-used by
			 * some other thread.
			 */
			VN_HOLD(vp);
			page_unlock(pp);
			/*
			 * No error return is checked here. Callers such as
			 * cpr deals with the dirty pages at the dump time
			 * if this putpage fails.
			 */
			VOP_PUTPAGE(vp, offset, PAGESIZE, B_INVAL, kcred);
			VN_RELE(vp);
		} else {
			page_destroy(pp, 0);
		}
	} while ((pp = page_next(pp)) != page0);
	if (nbusypages && retry++ < MAXRETRIES) {
		delay(1);
		goto top;
	}
}

/*
 * This function moves the identity of page "pp_from" to
 * page "pp_to".  Both pages must be locked on entry. "pp_to" is free
 * and aged (no identity).
 */
void
page_relocate_hash(page_t *pp_to, page_t *pp_from)
{
	struct	vnode *vp;
	kmutex_t *phm, *vphm;
	ulong_t	index_hash;

	/*
	 * Rehash two pages in one shot. This is much
	 * faster than first hashing out then hashing in pages.
	 */
	ASSERT(PAGE_EXCL(pp_to));
	ASSERT(PAGE_EXCL(pp_from));
	vp = pp_from->p_vnode;
	ASSERT(pp_from->p_vnode != NULL);
	ASSERT(pp_to->p_vnode == NULL);
	index_hash = PAGE_HASH_FUNC(vp, pp_from->p_offset);

	phm = PAGE_HASH_MUTEX(index_hash);

	vphm = page_vnode_mutex(vp);

	/* Observe mutex ordering. */
	mutex_enter(vphm);
	mutex_enter(phm);

	{
		page_t	**hpp;

		hpp = &page_hash[index_hash];
		for (;;) {
			ASSERT(*hpp != NULL);
			if (*hpp == pp_from)
				break;
			hpp = &(*hpp)->p_hash;
		}
		*hpp = pp_to;

		pp_to->p_hash = pp_from->p_hash;
		pp_from->p_hash = NULL;
	}

	/*
	 * The following has to be done while the hash mutex is still
	 * being held. Otherwise a new page with the same id may get
	 * created inadvertently.
	 *
	 * The lock order defined by VM is p_selock -> hash_mutex.
	 */
	pp_to->p_vnode = vp;
	pp_to->p_offset = pp_from->p_offset;
	pp_from->p_vnode = NULL;
	pp_from->p_offset = (u_offset_t)-1;

	mutex_exit(phm);

	/*
	 * At this moment, pages are on the new hash chain, but the
	 * old v_pages list.
	 */

	/*
	 * Do the equivalent of:
	 * page_vpsub(&vp->v_pages, pp_from);
	 * if (vp->v_pages)
	 *	page_vpadd(&vp->v_pages->p_vpprev->p_vpnext, pp_to);
	 * else
	 *	page_vpadd(&vp->v_pages, pp_to);
	 */
	/*
	 * Replace pp_from by pp_to on v_pages list in place.
	 */
	if (pp_from->p_vpnext == pp_from) {
		ASSERT(vp->v_pages == pp_from);
		pp_to->p_vpnext = pp_to->p_vpprev = pp_to;
		vp->v_pages = pp_to;
	} else {
		pp_from->p_vpprev->p_vpnext = pp_to;
		pp_from->p_vpnext->p_vpprev = pp_to;
		pp_to->p_vpprev = pp_from->p_vpprev;
		pp_to->p_vpnext = pp_from->p_vpnext;
		if (vp->v_pages == pp_from)
			vp->v_pages = pp_to;

	}
	pp_from->p_vpnext = pp_from->p_vpprev = NULL;

	mutex_exit(vphm);
	/*
	 * The page_struct_lock need not be acquired for lckcnt and
	 * cowcnt since the page has an "exclusive" lock.
	 */
	ASSERT(pp_to->p_lckcnt == 0);
	ASSERT(pp_to->p_cowcnt == 0);
	pp_to->p_lckcnt = pp_from->p_lckcnt;
	pp_to->p_cowcnt = pp_from->p_cowcnt;
	pp_from->p_lckcnt = pp_from->p_cowcnt = 0;

	/* The following comment preserved from page_flip(). */
	/* XXX - Do we need to protect fsdata? */
	pp_to->p_fsdata = pp_from->p_fsdata;
}

extern int platform_page_relocate(page_t **, page_t **);

/*
 * Return the number of PAGESIZE pages relocated.
 */
int
page_relocate(
	page_t **target,
	page_t **replacement)
{
	page_t *repl;
	page_t *pl = NULL;
	uint_t ppattr;
	int	ret;

	ret = platform_page_relocate(target, replacement);
	/*
	 * if platform_page_relocate() is supported, it will
	 * return the number of pages relocated, otherwise it
	 * will return -1.
	 */
	if (ret >= 0)
		return (ret);

	ASSERT(PAGE_EXCL(*target));
	ASSERT(!PP_ISFREE(*target));

	if ((repl = *replacement) == NULL) {
		pgcnt_t dofree;

		dofree = 1;	/* Size of target page in MMU pages */
		if (!page_create_wait(dofree, 0))
			return (0);
		repl = page_get_replacement_page(*target);
		if (repl == NULL) {
			page_create_putback(dofree);
			return (0);
		}
	} else {
		/* EMPTY */
		ASSERT(PAGE_LOCKED(repl));
	}

	(void) hat_pageunload(*target, HAT_FORCE_PGUNLOAD);
	ASSERT(hat_page_getshare(*target) == 0);

	/*
	 * We can use the generic props routines here as we have the SE_EXCL
	 * lock on the page and no mappings. This avoids the HAT layer lock.
	 * There is no generic props routine for getting the props.
	 */
	/* Save props. */
	ppattr = hat_page_getattr(*target, (P_MOD | P_REF | P_RO));
	/* Copy contents. */
	ppcopy(*target, repl);
	/* Move the page's identity. */
	page_relocate_hash(repl, *target);
	/* Restore props. */
	page_clr_all_props(repl);
	page_set_props(repl, ppattr);

	ASSERT(hat_page_getshare(*target) == 0);
	ASSERT(hat_page_getshare(repl) == 0);
	page_add(&pl, *target);
	*target = pl;
	if (*replacement == NULL)
		*replacement = repl;
	return (1);
}

/*
 * Release the page lock on a page, place on cachelist
 * tail if no longer mapped. Caller can let us know if
 * the page is known to be clean.
 */
int
page_release(page_t *pp, int checkmod)
{
	int status;

	ASSERT(PAGE_LOCKED(pp) && !PP_ISFREE(pp) &&
		(pp->p_vnode != NULL));

	if (!hat_page_is_mapped(pp) && !IS_SWAPVP(pp->p_vnode) &&
	    ((PAGE_SHARED(pp) && page_tryupgrade(pp)) || PAGE_EXCL(pp)) &&
	    pp->p_lckcnt == 0 && pp->p_cowcnt == 0 &&
	    !hat_page_is_mapped(pp)) {

		/* If page is modified, unlock it */
		if (checkmod && (hat_pagesync(pp, HAT_SYNC_DONTZERO |
		    HAT_SYNC_STOPON_MOD) & P_MOD)) {
			page_unlock(pp);
			status = PGREL_MOD;
		} else {
			/*LINTED: constant in conditional context*/
			VN_DISPOSE(pp, B_FREE, 0, kcred);
			status = PGREL_CLEAN;
		}
	} else {
		page_unlock(pp);
		status = PGREL_NOTREL;
	}
	return (status);
}
