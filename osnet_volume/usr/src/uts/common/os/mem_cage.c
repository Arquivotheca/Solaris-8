
/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mem_cage.c	1.57	99/05/14 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/callb.h>
#include <sys/vnode.h>
#include <sys/debug.h>
#include <sys/systm.h>		/* for bzero */
#include <sys/memlist.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/vmsystm.h>	/* for NOMEMWAIT() */
#include <sys/atomic.h>		/* used to update kcage_freemem */
#include <sys/kmem.h>		/* for kmem_reap */
#include <sys/errno.h>
#include <sys/mem_cage.h>
#include <vm/page.h>
#include <vm/hat.h>
#include <sys/mem_config.h>

extern pri_t maxclsyspri;

#ifdef DEBUG
#define	KCAGE_STATS
#endif

#ifdef KCAGE_STATS

#define	KCAGE_STATS_VERSION 8	/* can help report generators */
#define	KCAGE_STATS_NSCANS 256	/* depth of scan statistics buffer */

struct kcage_stats_scan {
	/* managed by KCAGE_STAT_* macros */
	clock_t	scan_lbolt;
	u_int	scan_id;

	/* set in kcage_cageout() */
	u_int	kt_passes;
	clock_t	kt_ticks;
	pgcnt_t	kt_kcage_freemem_start;
	pgcnt_t	kt_kcage_freemem_end;
	pgcnt_t kt_freemem_start;
	pgcnt_t kt_freemem_end;
	u_int	kt_examined;
	u_int	kt_cantlock;
	u_int	kt_gotone;
	u_int	kt_gotonefree;
	u_int	kt_skiplevel;
	u_int	kt_skipshared;
	u_int	kt_skiprefd;
	u_int	kt_destroy;

	/* set in kcage_invalidate_page() */
	u_int	kip_reloclocked;
	u_int	kip_relocmod;
	u_int	kip_destroy;
	u_int	kip_nomem;

	/* set in kcage_expand() */
	u_int	ke_wanted;
	u_int	ke_examined;
	u_int	ke_lefthole;
	u_int	ke_gotone;
	u_int	ke_gotonefree;
};

struct kcage_stats {
	/* managed by KCAGE_STAT_* macros */
	u_int	version;
	u_int	size;

	/* set in kcage_cageout */
	u_int	kt_wakeups;
	u_int	kt_scans;

	/* set in kcage_expand */
	u_int	ke_calls;
	u_int	ke_nopfn;
	u_int	ke_nopaget;
	u_int	ke_isnoreloc;
	u_int	ke_deleting;
	u_int	ke_lowfreemem;
	u_int	ke_terminate;

	/* set in kcage_freemem_add() */
	u_int	kfa_trottlewake;

	/* set in kcage_freemem_sub() */
	u_int	kfs_cagewake;

	/* set in kcage_create_throttle */
	u_int	kct_calls;
	u_int	kct_critical;
	u_int	kct_exempt;
	u_int	kct_cagewake;
	u_int	kct_wait;

	/* set in kcage_cageout_wakeup */
	u_int	kcw_expandearly;

	/* managed by KCAGE_STAT_* macros */
	u_int	scan_array_size;
	u_int	scan_index;
	struct kcage_stats_scan scans[KCAGE_STATS_NSCANS];
};

static struct kcage_stats kcage_stats;
static struct kcage_stats_scan kcage_stats_scan_zero;

/*
 * No real need for atomics here. For the most part the incs and sets are
 * done by the kernel cage thread. There are a few that are done by any
 * number of other threads. Those cases are noted by comments.
 */
#define	KCAGE_STAT_INCR(m)	kcage_stats.m++

#define	KCAGE_STAT_INCR_SCAN(m)	\
	KCAGE_STAT_INCR(scans[kcage_stats.scan_index].m)

#define	KCAGE_STAT_SET(m, v)	kcage_stats.m = (v)

#define	KCAGE_STAT_SETZ(m, v)	\
	if (kcage_stats.m == 0) kcage_stats.m = (v)

#define	KCAGE_STAT_SET_SCAN(m, v)	\
	KCAGE_STAT_SET(scans[kcage_stats.scan_index].m, v)

#define	KCAGE_STAT_SETZ_SCAN(m, v)	\
	KCAGE_STAT_SETZ(scans[kcage_stats.scan_index].m, v)

#define	KCAGE_STAT_INC_SCAN_INDEX \
	KCAGE_STAT_SET_SCAN(scan_lbolt, lbolt); \
	KCAGE_STAT_SET_SCAN(scan_id, kcage_stats.scan_index); \
	kcage_stats.scan_index = \
	(kcage_stats.scan_index + 1) % KCAGE_STATS_NSCANS; \
	kcage_stats.scans[kcage_stats.scan_index] = kcage_stats_scan_zero

#define	KCAGE_STAT_INIT_SCAN_INDEX \
	kcage_stats.version = KCAGE_STATS_VERSION; \
	kcage_stats.size = sizeof (kcage_stats); \
	kcage_stats.scan_array_size = KCAGE_STATS_NSCANS; \
	kcage_stats.scan_index = 0

#else /* KCAGE_STATS */

#define	KCAGE_STAT_INCR(v)
#define	KCAGE_STAT_INCR_SCAN(v)
#define	KCAGE_STAT_SET(m, v)
#define	KCAGE_STAT_SETZ(m, v)
#define	KCAGE_STAT_SET_SCAN(m, v)
#define	KCAGE_STAT_SETZ_SCAN(m, v)
#define	KCAGE_STAT_INC_SCAN_INDEX
#define	KCAGE_STAT_INIT_SCAN_INDEX

#endif /* KCAGE_STATS */

static kmutex_t kcage_throttle_mutex;	/* protects kcage_throttle_cv */
static kcondvar_t kcage_throttle_cv;

static kmutex_t kcage_cageout_mutex;	/* protects cv and ready flag */
static kcondvar_t kcage_cageout_cv;	/* cageout thread naps here */
static int kcage_cageout_ready;		/* nonzero when cageout thread ready */
static kthread_id_t kcage_cageout_thread; /* to aid debugging */

static kmutex_t kcage_range_mutex;	/* proctects kcage_glist elements */

/*
 * Cage expansion happens within a range.
 */
struct kcage_glist {
	struct kcage_glist	*next;
	pfn_t			base;
	pfn_t			lim;
	pfn_t			curr;
	int			decr;
};

static struct kcage_glist *kcage_glist;
static struct kcage_glist *kcage_current_glist;

/*
 * The firstfree element is provided so that kmem_alloc can be avoided
 * until that cage has somewhere to go. This is not currently a problem
 * as early kmem_alloc's use BOP_ALLOC instead of page_create_va.
 */
static struct kcage_glist kcage_glist_firstfree;
static struct kcage_glist *kcage_glist_freelist = &kcage_glist_firstfree;

/*
 * Miscellaneous forward references
 */
static struct kcage_glist *kcage_glist_alloc(void);
static int kcage_glist_delete(pfn_t, pfn_t, struct kcage_glist **);
static void kcage_cageout(void);
static int kcage_invalidate_page(page_t *);

/*
 * Kernel Memory Cage counters and thresholds.
 */
int kcage_on = 0;
pgcnt_t kcage_freemem;
pgcnt_t kcage_needfree;
pgcnt_t kcage_lotsfree;
pgcnt_t kcage_desfree;
pgcnt_t kcage_minfree;
pgcnt_t kcage_throttlefree;

/*
 * Startup and Dynamic Reconfiguration interfaces.
 * kcage_range_lock()
 * kcage_range_unlock()
 * kcage_range_islocked()
 * kcage_range_add()
 * kcage_range_del()
 * kcage_init()
 * kcage_set_thresholds()
 */

int
kcage_range_trylock(void)
{
	return (mutex_tryenter(&kcage_range_mutex));
}

void
kcage_range_lock(void)
{
	mutex_enter(&kcage_range_mutex);
}

void
kcage_range_unlock(void)
{
	mutex_exit(&kcage_range_mutex);
}

int
kcage_range_islocked(void)
{
	return (MUTEX_HELD(&kcage_range_mutex));
}

int
kcage_range_init(struct memlist *ml, int decr)
{
	int ret = 0;

	ASSERT(kcage_range_islocked());

	if (decr) {
		while (ml->next != NULL)
			ml = ml->next;
	}

	while (ml != NULL) {
		ret = kcage_range_add(btop(ml->address), btop(ml->size), decr);
		if (ret)
			break;

		ml = (decr ? ml->prev : ml->next);
	}

	return (ret);
}

/*
 * Third arg controls direction of growth: 0: increasing pfns,
 * 1: decreasing.
 * Calls to add and delete must be protected by calls to
 * kcage_range_lock() and kcage_range_unlock().
 */
int
kcage_range_add(pfn_t base, pgcnt_t npgs, int decr)
{
	struct kcage_glist *new, **lpp;
	pfn_t lim;

	ASSERT(kcage_range_islocked());

	ASSERT(npgs != 0);
	if (npgs == 0)
		return (EINVAL);

	lim = base + npgs;

	ASSERT(lim > base);
	if (lim <= base)
		return (EINVAL);

	new = kcage_glist_alloc();
	if (new == NULL) {
		return (ENOMEM);
	}
	new->base = base;
	new->lim = lim;
	new->decr = decr;
	if (new->decr != 0)
		new->curr = new->lim;
	else
		new->curr = new->base;
	/*
	 * Any overlapping existing ranges are removed by deleting
	 * from the new list as we search for the tail.
	 */
	lpp = &kcage_glist;
	while (*lpp != NULL) {
		int ret;
		ret = kcage_glist_delete((*lpp)->base, (*lpp)->lim, &new);
		if (ret != 0)
			return (ret);
		lpp = &(*lpp)->next;
	}
	*lpp = new;

	if (kcage_current_glist == NULL)
		kcage_current_glist = kcage_glist;

	return (0);
}

/*
 * Calls to add and delete must be protected by calls to
 * kcage_range_lock() and kcage_range_unlock().
 */
int
kcage_range_delete(pfn_t base, pgcnt_t npgs)
{
	struct kcage_glist *lp;
	pfn_t lim;

	ASSERT(kcage_range_islocked());

	ASSERT(npgs != 0);
	if (npgs == 0)
		return (EINVAL);

	lim = base + npgs;

	ASSERT(lim > base);
	if (lim <= base)
		return (EINVAL);

	/*
	 * Check if the delete is OK first as a number of elements
	 * might be involved and it will be difficult to go
	 * back and undo (can't just add the range back in).
	 */
	for (lp = kcage_glist; lp != NULL; lp = lp->next) {
		/*
		 * If there have been no pages allocated from this
		 * element, we don't need to check it.
		 */
		if ((lp->decr == 0 && lp->curr == lp->base) ||
		    (lp->decr != 0 && lp->curr == lp->lim))
			continue;
		/*
		 * If the element does not overlap, its OK.
		 */
		if (base >= lp->lim || lim <= lp->base)
			continue;
		/*
		 * Overlapping element: Does the range to be deleted
		 * overlap the area already used? If so fail.
		 */
		if (lp->decr == 0 && base < lp->curr && lim >= lp->base) {
			return (EBUSY);
		}
		if (lp->decr != 0 && base < lp->lim && lim >= lp->curr) {
			return (EBUSY);
		}
	}
	return (kcage_glist_delete(base, lim, &kcage_glist));
}

/*
 * No locking is required here as the whole operation is covered
 * by the kcage_range_lock().
 */
static struct kcage_glist *
kcage_glist_alloc(void)
{
	struct kcage_glist *new;

	if ((new = kcage_glist_freelist) != NULL) {
		kcage_glist_freelist = new->next;
		bzero(new, sizeof (*new));
	} else {
		new = kmem_zalloc(sizeof (struct kcage_glist), KM_NOSLEEP);
	}
	return (new);
}

static void
kcage_glist_free(struct kcage_glist *lp)
{
	lp->next = kcage_glist_freelist;
	kcage_glist_freelist = lp;
}

static int
kcage_glist_delete(pfn_t base, pfn_t lim, struct kcage_glist **lpp)
{
	struct kcage_glist *lp;

	while ((lp = *lpp) != NULL) {
		if (lim > lp->base && base < lp->lim) {
			/* The delete range overlaps this element. */
			if (base <= lp->base && lim >= lp->lim) {
				/* Delete whole element. */
				*lpp = lp->next;
				kcage_glist_free(lp);
				continue;
			}

			/* Partial delete. */
			if (base > lp->base && lim < lp->lim) {
				struct kcage_glist *new;

				/*
				 * Remove a section from the middle,
				 * need to allocate a new element.
				 */
				new = kcage_glist_alloc();
				if (new == NULL) {
					return (ENOMEM);
				}

				/*
				 * Tranfser unused range to new.
				 * Edit lp in place to preserve
				 * kcage_current_glist.
				 */
				new->decr = lp->decr;
				if (new->decr != 0) {
					new->base = lp->base;
					new->lim = base;
					new->curr = base;

					lp->base = lim;
				} else {
					new->base = lim;
					new->lim = lp->lim;
					new->curr = new->base;

					lp->lim = base;
				}

				/* Insert new. */
				new->next = lp->next;
				lp->next = new;
				lpp = &lp->next;
			} else {
				/* Delete part of current block. */
				if (base > lp->base) {
					ASSERT(lim >= lp->lim);
					ASSERT(base < lp->lim);
					if (lp->decr != 0 &&
					    lp->curr == lp->lim)
						lp->curr = base;
					lp->lim = base;
				} else {
					ASSERT(base <= lp->base);
					ASSERT(lim > lp->base);
					if (lp->decr == 0 &&
					    lp->curr == lp->base)
						lp->curr = lim;
					lp->base = lim;
				}
			}
		}
		lpp = &(*lpp)->next;
	}

	return (0);
}

/*
 * The caller of kcage_get_pfn must hold the kcage_range_lock to make
 * sure that there are no concurrent calls. The same lock
 * must be obtained for range add and delete by calling
 * kcage_range_lock() and kcage_range_unlock().
 */
static pfn_t
kcage_get_pfn(void)
{
	struct kcage_glist *lp;
	pfn_t pfn;

	ASSERT(kcage_range_islocked());

	lp = kcage_current_glist;
	while (lp != NULL) {
		if (lp->decr != 0) {
			if (lp->curr != lp->base) {
				pfn = --lp->curr;
				kcage_current_glist = lp;
				return (pfn);
			}
		} else {
			if (lp->curr != lp->lim) {
				pfn = lp->curr++;
				kcage_current_glist = lp;
				return (pfn);
			}
		}

		lp = lp->next;
	}

	return (PFN_INVALID);
}

/*
 * Walk the physical address space of the cage.
 * This routine does not guarantee to return PFNs in the order
 * in which they were allocated to the cage. Instead, it walks
 * each range as they appear on the growth list returning the PFNs
 * range in ascending order.
 *
 * To begin scanning at lower edge of cage, reset should be nonzero.
 * To step through cage, reset should be zero.
 *
 * PFN_INVALID will be returned when the upper end of the cage is
 * reached -- indicating a full scan of the cage has been completed since
 * previous reset. PFN_INVALID will continue to be returned until
 * kcage_walk_cage is reset.
 *
 * It is possible to receive a PFN_INVALID result on reset if a growth
 * list is not installed or if none of the PFNs in the installed list have
 * been allocated to the cage. In otherwords, there is no cage.
 *
 * Caller need not hold kcage_range_lock while calling this function
 * as the front part of the list is static - pages never come out of
 * the cage.
 *
 * The caller is expected to only be kcage_cageout().
 */
static pfn_t
kcage_walk_cage(int reset)
{
	static struct kcage_glist *lp = NULL;
	static pfn_t pfn;

	if (reset)
		lp = NULL;
	if (lp == NULL) {
		lp = kcage_glist;
		pfn = PFN_INVALID;
	}
again:
	if (pfn == PFN_INVALID) {
		if (lp == NULL)
			return (PFN_INVALID);

		if (lp->decr != 0) {
			/*
			 * In this range the cage grows from the highest
			 * address towards the lowest.
			 * Arrange to return pfns from curr to lim-1,
			 * inclusive, in ascending order.
			 */

			pfn = lp->curr;
		} else {
			/*
			 * In this range the cage grows from the lowest
			 * address towards the highest.
			 * Arrange to return pfns from base to curr,
			 * inclusive, in ascending order.
			 */

			pfn = lp->base;
		}
	}

	if (lp->decr != 0) {		/* decrementing pfn */
		if (pfn == lp->lim) {
			/* Don't go beyond the static part of the glist. */
			if (lp == kcage_current_glist)
				lp = NULL;
			else
				lp = lp->next;
			pfn = PFN_INVALID;
			goto again;
		}

		ASSERT(pfn >= lp->curr && pfn < lp->lim);
	} else {			/* incrementing pfn */
		if (pfn == lp->curr) {
			/* Don't go beyond the static part of the glist. */
			if (lp == kcage_current_glist)
				lp = NULL;
			else
				lp = lp->next;
			pfn = PFN_INVALID;
			goto again;
		}

		ASSERT(pfn >= lp->base && pfn < lp->curr);
	}

	return (pfn++);
}

/*
 * Callback functions for to recalc cage thresholds after
 * Kphysm memory add/delete operations.
 */
/*ARGSUSED*/
static void
kcage_kphysm_postadd_cb(void *arg, pgcnt_t delta_pages)
{
	kcage_recalc_thresholds();
}

/*ARGSUSED*/
static int
kcage_kphysm_predel_cb(void *arg, pgcnt_t delta_pages)
{
	/* TODO: when should cage refuse memory delete requests? */
	return (0);
}

/*ARGSUSED*/
static  void
kcage_kphysm_postdel_cb(void *arg, pgcnt_t delta_pages, int cancelled)
{
	kcage_recalc_thresholds();
}

static kphysm_setup_vector_t kcage_kphysm_vectors = {
	KPHYSM_SETUP_VECTOR_VERSION,
	kcage_kphysm_postadd_cb,
	kcage_kphysm_predel_cb,
	kcage_kphysm_postdel_cb
};

/*
 * Kcage_init() builds the cage and initializes the cage thresholds.
 * The size of the cage is determined by the argument preferred_size.
 * or the actual amount of memory, whichever is smaller.
 */
void
kcage_init(pgcnt_t preferred_size)
{
	pgcnt_t wanted;
	pfn_t pfn;
	page_t *pp;

	ASSERT(!kcage_on);
	ASSERT(kcage_range_islocked());

	/* Debug note: initialize this now so early expansions can stat */
	KCAGE_STAT_INIT_SCAN_INDEX;

	/*
	 * Initialize cage thresholds and install kphysm callback.
	 * If we can't arrange to have the thresholds track with
	 * available physical memory, then the cage thresholds may
	 * end up over time at levels that adversly effect system
	 * performance; so, bail out.
	 */
	kcage_recalc_thresholds();
	if (kphysm_setup_func_register(&kcage_kphysm_vectors, NULL)) {
		ASSERT(0);		/* Catch this in DEBUG kernels. */
		return;
	}

	/*
	 * Limit startup cage size within the range of kcage_minfree
	 * and availrmem, inclusively.
	 */
	wanted = MIN(MAX(preferred_size, kcage_minfree), availrmem);

	/*
	 * Construct the cage. PFNs are allocated from the glist. It
	 * is assumed that the list has been properly ordered for the
	 * platform by the platform code. Typically, this is as simple
	 * as calling kcage_range_init(phys_avail, decr), where decr is
	 * 1 if the kernel has been loaded into upper end of physical
	 * memory, or 0 if the kernel has been loaded at the low end.
	 *
	 * Note: it is assumed that we are in the startup flow, so there
	 * is no reason to grab the page lock.
	 */
	kcage_freemem = 0;
	pfn = PFN_INVALID;			/* prime for alignment test */
	while (wanted != 0) {
		if ((pfn = kcage_get_pfn()) == PFN_INVALID)
			break;

		if ((pp = page_numtopp_nolock(pfn)) != NULL) {
			if (PP_ISFREE(pp)) {
				int	which = PP_ISAGED(pp) ?
						PG_FREE_LIST : PG_CACHE_LIST;
				/*
				 * Free pages at this stage should
				 * never be locked.  However, we'll
				 * be paranoid about it.  If by chance
				 * we can't acquire the lock on a page
				 * then leave it as a whole.  We must
				 * be able to relocate such pages to
				 * their proper freelist, and we can't
				 * do that unless we can lock them.
				 */
				if (page_trylock(pp, SE_EXCL)) {
					if (PP_ISNORELOC(pp) == 0) {
						page_list_sub(which, pp);
						PP_SETNORELOC(pp);
						page_list_add(which, pp,
								PG_LIST_TAIL);
					}
					page_unlock(pp);
				}
			} else {
				PP_SETNORELOC(pp);
			}
		}

		wanted -= 1;
	}

	/*
	 * Need to go through and find kernel allocated pages
	 * and capture them into the Cage.  These will primarily
	 * be pages gotten through boot_alloc().
	 */
	if ((pp = page_first()) != NULL) {
		extern struct vnode	kvp;

		do {
			if (pp->p_vnode != &kvp)
				continue;

			if (PP_ISFREE(pp)) {
				int	which = PP_ISAGED(pp) ?
						PG_FREE_LIST : PG_CACHE_LIST;

				if (page_trylock(pp, SE_EXCL)) {
					if (PP_ISNORELOC(pp) == 0) {
						page_list_sub(which, pp);
						PP_SETNORELOC(pp);
						page_list_add(which, pp,
								PG_LIST_TAIL);
					}
					page_unlock(pp);
				}
			} else {
				PP_SETNORELOC(pp);
			}
		} while ((pp = page_next(pp)) != page_first());
	}

	kcage_on = 1;
}

void
kcage_recalc_thresholds()
{
	static int first = 1;
	static pgcnt_t init_lotsfree;
	static pgcnt_t init_desfree;
	static pgcnt_t init_minfree;
	static pgcnt_t init_throttlefree;

	/* TODO: any reason to take more care than this with live editing? */
	mutex_enter(&kcage_cageout_mutex);
	mutex_enter(&freemem_lock);

	if (first) {
		first = 0;
		init_lotsfree = kcage_lotsfree;
		init_desfree = kcage_desfree;
		init_minfree = kcage_minfree;
		init_throttlefree = kcage_throttlefree;
	} else {
		kcage_lotsfree = init_lotsfree;
		kcage_desfree = init_desfree;
		kcage_minfree = init_minfree;
		kcage_throttlefree = init_throttlefree;
	}

	if (kcage_lotsfree == 0)
		kcage_lotsfree = MAX(32, total_pages / 256);

	if (kcage_minfree == 0)
		kcage_minfree = MAX(32, kcage_lotsfree / 2);

	if (kcage_desfree == 0)
		kcage_desfree = MAX(32, kcage_minfree);

	if (kcage_throttlefree == 0)
		kcage_throttlefree = MAX(32, kcage_minfree / 2);

	mutex_exit(&freemem_lock);
	mutex_exit(&kcage_cageout_mutex);

	if (kcage_cageout_ready) {
		if (kcage_freemem < kcage_desfree)
			kcage_cageout_wakeup();

		if (kcage_needfree) {
			mutex_enter(&kcage_throttle_mutex);
			cv_broadcast(&kcage_throttle_cv);
			mutex_exit(&kcage_throttle_mutex);
		}
	}
}

/*
 * Pageout interface:
 * kcage_cageout_init()
 */
void
kcage_cageout_init()
{
	if (kcage_on) {
		mutex_enter(&kcage_cageout_mutex);

		kcage_cageout_thread = thread_create(NULL, PAGESIZE,
			kcage_cageout, 0, 0, proc_pageout, TS_RUN,
			maxclsyspri - 1);

		if (kcage_cageout_thread == NULL)
			panic("kcage_cageout_init:"
				" unable to create cageout thread");

		mutex_exit(&kcage_cageout_mutex);
	}
}


/*
 * VM Interfaces:
 * kcage_create_throttle()
 * kcage_freemem_add()
 * kcage_freemem_sub()
 */

void
kcage_create_throttle(pgcnt_t npages, int flags)
{
	KCAGE_STAT_INCR(kct_calls);		/* unprotected incr. */

	kcage_cageout_wakeup();			/* just to be sure */
	KCAGE_STAT_INCR(kct_cagewake);		/* unprotected incr. */

	/*
	 * NEVER throttle threads which are critical for proper
	 * vm management.
	 */
	if (NOMEMWAIT()) {
		KCAGE_STAT_INCR(kct_critical);	/* unprotected incr. */
		return;
	}

	/*
	 * NEVER throttle real-time threads or any thread that won't wait
	 */
	if ((flags & PG_WAIT) == 0 || DISP_PRIO(curthread) > maxclsyspri) {
		KCAGE_STAT_INCR(kct_exempt);	/* unprotected incr. */
		return;
	}

	/*
	 * Cause all other threads (which are assumed to not be
	 * critical) to wait here until their request can be
	 * satisfied. Be alittle paranoid and wake the kernel
	 * cage on each loop through this logic.
	 */
	while (kcage_freemem < kcage_throttlefree + npages) {
		ASSERT(kcage_on);

		if (kcage_cageout_ready) {
			mutex_enter(&kcage_throttle_mutex);

			kcage_needfree += npages;
			KCAGE_STAT_INCR(kct_wait);

			kcage_cageout_wakeup();
			KCAGE_STAT_INCR(kct_cagewake);

			cv_wait(&kcage_throttle_cv, &kcage_throttle_mutex);
			kcage_needfree -= npages;

			mutex_exit(&kcage_throttle_mutex);
		} else {
			/*
			 * NOTE: atomics are used just in case we enter
			 * mp operation before the cageout thread is ready.
			 */
			atomic_add_long(&kcage_needfree, npages);

			kcage_cageout_wakeup();
			KCAGE_STAT_INCR(kct_cagewake);	/* unprotected incr. */

			atomic_add_long(&kcage_needfree, -npages);
		}
	}
}

void
kcage_freemem_add(ssize_t npages)
{
	atomic_add_long(&kcage_freemem, npages);

	if (kcage_needfree != 0 &&
		kcage_freemem >= (kcage_throttlefree + kcage_needfree)) {

		mutex_enter(&kcage_throttle_mutex);
		cv_broadcast(&kcage_throttle_cv);
		KCAGE_STAT_INCR(kfa_trottlewake);
		mutex_exit(&kcage_throttle_mutex);
	}
}

void
kcage_freemem_sub(ssize_t npages)
{
	atomic_add_long(&kcage_freemem, -npages);

	if (kcage_freemem < kcage_desfree) {
		kcage_cageout_wakeup();
		KCAGE_STAT_INCR(kfs_cagewake); /* unprotected incr. */
	}
}

/*
 * Attempt to convert page to a caged page (set the P_NORELOC flag).
 * If successful and pages is free, move page to the tail of whichever
 * list it is on.
 * Returns:
 *   EBUSY  page already locked, unable to convert. Page free state unknown.
 *   ENOMEM page assimilated, but memory too low to relocate. Page not free.
 *   EAGAIN page assimilated. Page not free.
 *   0      page assimilated. Page free.
 * NOTE: With error codes ENOMEM, EBUSY, and 0 (zero), there is no way
 * to distinguish between a page that was already a NORELOC page from
 * those newly converted to NORELOC pages by this invokation of
 * kcage_assimilate_page.
 */
static int
kcage_assimilate_page(page_t *pp)
{
	if (page_trylock(pp, SE_EXCL)) {
		if (PP_ISNORELOC(pp)) {
check_free_and_return:
			if (PP_ISFREE(pp)) {
				page_unlock(pp);
				return (0);
			} else {
				page_unlock(pp);
				return (EBUSY);
			}
			/*NOTREACHED*/
		}
	} else {
		if (page_trylock(pp, SE_SHARED)) {
			if (PP_ISNORELOC(pp))
				goto check_free_and_return;
		} else
			return (EAGAIN);

		/*
		 * We have the shared lock so no one is
		 * changing p_state. If the page is not
		 * free, we can set the NORELOC flag as
		 * it does not affect the current user
		 * of the page.
		 */
		if (!PP_ISFREE(pp)) {
			PP_SETNORELOC(pp);
			page_unlock(pp);
			return (EBUSY);
		}

		/*
		 * Need to upgrade the lock on it and set the NORELOC
		 * bit. If it is free then remove it from the free
		 * list so that the platform free list code can keep
		 * NORELOC pages where they should be.
		 */
		/*
		 * Before doing anything, get the exclusive lock.
		 * This may fail (eg ISM pages are left shared locked).
		 * If the page is free this will leave a hole in the
		 * cage. There is no solution yet to this.
		 */
		if (!page_tryupgrade(pp)) {
			page_unlock(pp);
			return (EAGAIN);
		}
	}

	ASSERT(PAGE_EXCL(pp));

	if (PP_ISFREE(pp)) {
		int which = PP_ISAGED(pp) ? PG_FREE_LIST : PG_CACHE_LIST;

		page_list_sub(which, pp);
		PP_SETNORELOC(pp);
		page_list_add(which, pp, PG_LIST_TAIL);

		page_unlock(pp);
		return (0);
	} else {
		PP_SETNORELOC(pp);
		return (kcage_invalidate_page(pp));
	}
	/*NOTREACHED*/
}

static int
kcage_expand()
{
	int did_something = 0;

	spgcnt_t wanted;
	pfn_t pfn;
	page_t *pp;
	pgcnt_t n;
	/* TODO: we don't really need nf any more. Keep for kstat hint? */
	pgcnt_t nf;

	/*
	 * Expand the cage if available cage memory is really low. Calculate
	 * the amount required to return kcage_freemem to the level of
	 * kcage_lotsfree, or to satisfy throttled requests, whichever is
	 * more.  It is rare for their sum to create an artificial threshold
	 * above kcage_lotsfree, but it is possible.
	 *
	 * Exit early if expansion amount is equal to or less than zero.
	 * (<0 is possible if kcage_freemem rises suddenly.)
	 *
	 * Exit early when the global page pool (apparently) does not
	 * have enough free pages to page_relocate() even a single page.
	 */
	wanted = MAX(kcage_lotsfree, kcage_throttlefree + kcage_needfree)
		- kcage_freemem;
	if (wanted <= 0)
		return (0);
	else if (freemem < pageout_reserve + 1) {
		KCAGE_STAT_INCR(ke_lowfreemem);
		return (0);
	}

	/*
	 * Try to get the range list lock. If the lock is already
	 * held, then don't get stuck here waiting for it.
	 */
	if (!kcage_range_trylock())
		return (0);

	KCAGE_STAT_INCR(ke_calls);
	KCAGE_STAT_SET_SCAN(ke_wanted, (u_int)wanted);

	/*
	 * Assimilate more pages from the global page pool into the cage.
	 */
	n = 0;				/* number of pages PP_SETNORELOC'd */
	nf = 0;				/* number of those actually free */
	while (kcage_on && n < wanted) {
		pfn = kcage_get_pfn();
		if (pfn == PFN_INVALID) {	/* eek! no where to grow */
			KCAGE_STAT_INCR(ke_nopfn);
			goto terminate;
		}

		KCAGE_STAT_INCR_SCAN(ke_examined);

		if ((pp = page_numtopp_nolock(pfn)) == NULL) {
			KCAGE_STAT_INCR(ke_nopaget);
			continue;
		}

		/*
		 * Sanity check. Skip this pfn if it is
		 * being deleted.
		 */
		if (pfn_is_being_deleted(pfn)) {
			KCAGE_STAT_INCR(ke_deleting);
			continue;
		}

		/*
		 * NORELOC is only set at boot-time or by this routine
		 * under the kcage_range_mutex lock which is currently
		 * held. This means we can do a fast check here before
		 * locking the page in kcage_assimilate_page.
		 */
		if (PP_ISNORELOC(pp)) {
			KCAGE_STAT_INCR(ke_isnoreloc);
			continue;
		}

		switch (kcage_assimilate_page(pp)) {
			case 0:		/* assimilated, page is free */
				KCAGE_STAT_INCR_SCAN(ke_gotonefree);
				did_something = 1;
				nf++;
				n++;
				break;

			case EBUSY:	/* assimilated, page not free */
				KCAGE_STAT_INCR_SCAN(ke_gotone);
				did_something = 1;
				n++;
				break;

			case ENOMEM:	/* assimilated, page not free */
				KCAGE_STAT_INCR(ke_terminate);
				n++;
				goto terminate;

			case EAGAIN:	/* cant assimilate */
				KCAGE_STAT_INCR_SCAN(ke_lefthole);
				break;

			default:	/* catch this with debug kernels */
				ASSERT(0);
				break;
		}
	}

	/*
	 * Realign cage edge with the nearest physical address
	 * boundry for big pages. This is done to give us a
	 * better chance of actually getting usable big pages
	 * in the cage.
	 */

terminate:
	kcage_range_unlock();

	return (did_something);
}

/*
 * Relocate page opp (Original Page Pointer) from cage pool to page rpp
 * (Replacement Page Pointer) in the global pool. Page opp will be freed
 * if relocation is successful, otherwise it is only unlocked.
 * On entry, page opp must be exclusively locked and not free.
 */
static int
kcage_relocate_page(page_t *pp)
{
	page_t *opp = pp;
	page_t *rpp = NULL;
	int npgs;

	ASSERT(!PP_ISFREE(opp));
	ASSERT(PAGE_EXCL(opp));

	npgs = page_relocate(&opp, &rpp);
	if (npgs > 0) {
		while (npgs-- > 0) {
			page_t *tpp;

			ASSERT(rpp != NULL);
			tpp = rpp;
			page_sub(&rpp, tpp);
			page_unlock(tpp);

			ASSERT(opp != NULL);
			tpp = opp;
			page_sub(&opp, tpp);
			page_free(tpp, 1);
		}

		ASSERT(rpp == NULL);
		ASSERT(opp == NULL);

		return (0);		/* success */
	}

	page_unlock(opp);
	return (ENOMEM);
}

/*
 * Based on page_invalidate_pages()
 *
 * Kcage_invalidate_page() uses page_relocate() twice. Both instances
 * of use must be updated to match the new page_relocate() when it
 * becomes available.
 *
 * Return result of kcage_relocate_page or zero if page was directly freed.
 */
static int
kcage_invalidate_page(page_t *pp)
{
	int result;
	int mod;

#ifdef sparc
	extern struct vnode prom_ppages;
	ASSERT(pp->p_vnode != &prom_ppages);
#endif sparc

	ASSERT(!PP_ISFREE(pp));
	ASSERT(PAGE_EXCL(pp));

	/* wrong ASSERT(pp->p_vnode != &kvp); */
	/* If it is SEGKP, is there a special way of doing it? */

	/*
	 * Is this page involved in some I/O? shared?
	 * The page_struct_lock need not be acquired to
	 * examine these fields since the page has an
	 * "exclusive" lock.
	 */
	if (pp->p_lckcnt != 0 || pp->p_cowcnt != 0) {
		result = kcage_relocate_page(pp);
#ifdef KCAGE_STATS
		if (result == 0)
			KCAGE_STAT_INCR_SCAN(kip_reloclocked);
		else if (result == ENOMEM)
			KCAGE_STAT_INCR_SCAN(kip_nomem);
#endif
		return (result);
	}

	ASSERT(pp->p_vnode->v_type != VCHR);

	/*
	 * Check the modified bit. Leave the bits alone in hardware.
	 */
	mod = (hat_pagesync(pp, HAT_SYNC_DONTZERO | HAT_SYNC_STOPON_MOD)
		& P_MOD);
	if (mod) {
		result = kcage_relocate_page(pp);
#ifdef KCAGE_STATS
		if (result == 0)
			KCAGE_STAT_INCR_SCAN(kip_relocmod);
		else if (result == ENOMEM)
			KCAGE_STAT_INCR_SCAN(kip_nomem);
#endif
		return (result);
	}

	page_destroy(pp, 0);
	KCAGE_STAT_INCR_SCAN(kip_destroy);
	return (0);
}

static void
kcage_cageout()
{
	pfn_t pfn;
	page_t *pp;
	callb_cpr_t cprinfo;
	int did_something;
	int scan_again;
	pfn_t start_pfn;
	int pass;
	int last_pass;
	int pages_skipped;
	int shared_skipped;
	u_int shared_level = 8;
#ifdef KCAGE_STATS
	clock_t scan_start;
#endif

	CALLB_CPR_INIT(&cprinfo, &kcage_cageout_mutex,
		callb_generic_cpr, "cageout");

	mutex_enter(&kcage_cageout_mutex);

	pfn = PFN_INVALID;		/* force scan reset */
	start_pfn = PFN_INVALID;	/* force init with 1st cage pfn */
	kcage_cageout_ready = 1;	/* switch kcage_cageout_wakeup mode */

loop:
	/*
	 * Wait here. Sooner or later, kcage_freemem_sub() will notice
	 * that kcage_freemem is less than kcage_desfree. When it does
	 * notice, kcage_freemem_sub() will wake us up via call to
	 * kcage_cageout_wakeup().
	 */
	CALLB_CPR_SAFE_BEGIN(&cprinfo);
	cv_wait(&kcage_cageout_cv, &kcage_cageout_mutex);
	CALLB_CPR_SAFE_END(&cprinfo, &kcage_cageout_mutex);

	KCAGE_STAT_INCR(kt_wakeups);
	KCAGE_STAT_SET_SCAN(kt_freemem_start, freemem);
	KCAGE_STAT_SET_SCAN(kt_kcage_freemem_start, kcage_freemem);
	pass = 0;
	last_pass = 0;

#ifdef KCAGE_STATS
	scan_start = lbolt;
#endif

again:
	if (!kcage_on)
		goto loop;

	KCAGE_STAT_INCR(kt_scans);
	KCAGE_STAT_INCR_SCAN(kt_passes);

	did_something = 0;
	pages_skipped = 0;
	shared_skipped = 0;
	while ((kcage_freemem < kcage_lotsfree || kcage_needfree) &&
		(pfn = kcage_walk_cage(pfn == PFN_INVALID)) != PFN_INVALID) {

		if (start_pfn == PFN_INVALID)
			start_pfn = pfn;
		else if (start_pfn == pfn) {
			last_pass = pass;
			pass += 1;
		}

		pp = page_numtopp_nolock(pfn);
		if (pp == NULL) {
			continue;
		}

		KCAGE_STAT_INCR_SCAN(kt_examined);

		/*
		 * Do a quick PP_ISNORELOC() and PP_ISFREE test outside
		 * of the lock. If one is missed it will be seen next
		 * time through.
		 *
		 * Skip non-caged-pages. These pages can exist in the cage
		 * because, if during cage expansion, a page is
		 * encountered that is long-term locked the lock prevents the
		 * expansion logic from setting the P_NORELOC flag. Hence,
		 * non-caged-pages surrounded by caged-pages.
		 */
		if (!PP_ISNORELOC(pp)) {
			switch (kcage_assimilate_page(pp)) {
				case 0:
					did_something = 1;
					KCAGE_STAT_INCR_SCAN(kt_gotonefree);
					break;

				case EBUSY:
					did_something = 1;
					KCAGE_STAT_INCR_SCAN(kt_gotone);
					break;

				case EAGAIN:
				case ENOMEM:
					break;

				default:
					/* catch this with debug kernels */
					ASSERT(0);
					break;
			}

			continue;
		} else {
			int prm;

			if (PP_ISFREE(pp)) {
				continue;
			}

			if (!page_trylock(pp, SE_EXCL)) {
				KCAGE_STAT_INCR_SCAN(kt_cantlock);
				continue;
			}

			/* P_NORELOC bit should not have gone away. */
			ASSERT(PP_ISNORELOC(pp));
			if (!PP_ISNORELOC(pp) || PP_ISFREE(pp)) {
				page_unlock(pp);
				continue;
			}

			KCAGE_STAT_SET_SCAN(kt_skiplevel, shared_level);
			if (hat_page_getshare(pp) > shared_level) {
				page_unlock(pp);
				pages_skipped = 1;
				shared_skipped = 1;
				KCAGE_STAT_INCR_SCAN(kt_skipshared);
				continue;
			}

			prm = hat_pagesync(pp,
				HAT_SYNC_DONTZERO | HAT_SYNC_STOPON_MOD);

			/* On first pass ignore ref'd pages */
			if (pass <= 1 && (prm & P_REF)) {
				KCAGE_STAT_INCR_SCAN(kt_skiprefd);
				pages_skipped = 1;
				page_unlock(pp);
				continue;
			}

			/* On next pass, just page_destroy */
			if (pass <= 2) {
				if ((prm & P_MOD) ||
					pp->p_lckcnt || pp->p_cowcnt) {
					pages_skipped = 1;
					page_unlock(pp);
				} else {
					KCAGE_STAT_INCR_SCAN(kt_destroy);
					page_destroy(pp, 0);
					did_something = 1;
				}
				continue;
			}

			if (kcage_invalidate_page(pp) == 0)
				did_something = 1;

			/*
			 * No need to drop the page lock here.
			 * Kcage_invalidate_page has done that for us
			 * either explicitly or through a page_free.
			 */
		}
	}

	/*
	 * Expand the cage only if available cage memory is really low.
	 * This test is done only after a complete scan of the cage.
	 * The reason for not checking and expanding more often is to
	 * avoid rapid expansion of the cage. Naturally, scanning the
	 * cage takes time. So by scanning first, we use that work as a
	 * delay loop in between expand decisions.
	 */

	scan_again = 0;
	if (kcage_freemem < kcage_minfree || kcage_needfree) {
		/*
		 * Kcage_expand() will return a non-zero value if it was
		 * able to expand the cage -- whether or not the new
		 * pages are free and immediately usable. If non-zero,
		 * we do another scan of the cage. The pages might be
		 * freed during that scan or by time we get back here.
		 * If not, we will attempt another expansion.
		 * However, if kcage_expand() returns zero, then it was
		 * unable to expand the cage. This is the case when the
		 * the growth list is exausted, therefore no work was done
		 * and there is no reason to scan the cage again.
		 */
		if (pass <= 3 && pages_skipped)
			scan_again = 1;
		else
			(void) kcage_expand(); /* don't scan again */
	} else if (kcage_freemem < kcage_lotsfree) {
		/*
		 * If available cage memory is less than abundant
		 * and a full scan of the cage has not yet been completed,
		 * or a scan has completed and some work was performed,
		 * or pages were skipped because of sharing,
		 * or we simply have not yet completed two passes,
		 * then do another scan.
		 */
		if (pass <= 2 && pages_skipped)
			scan_again = 1;
		if (pass == last_pass || did_something)
			scan_again = 1;
		else if (shared_skipped && shared_level < (8<<24)) {
			shared_level <<= 1;
			scan_again = 1;
		}
	}

	if (scan_again)
		goto again;
	else {
		if (shared_level > 8)
			shared_level >>= 1;

		KCAGE_STAT_SET_SCAN(kt_freemem_end, freemem);
		KCAGE_STAT_SET_SCAN(kt_kcage_freemem_end, kcage_freemem);
		KCAGE_STAT_SET_SCAN(kt_ticks, lbolt - scan_start);
		KCAGE_STAT_INC_SCAN_INDEX;
		goto loop;
	}

	/*NOTREACHED*/
}

void
kcage_cageout_wakeup()
{
	if (mutex_tryenter(&kcage_cageout_mutex)) {
		if (kcage_cageout_ready) {
			cv_signal(&kcage_cageout_cv);
		} else if (kcage_freemem < kcage_minfree || kcage_needfree) {
			/*
			 * Available cage memory is really low. Time to
			 * start expanding the cage. However, the
			 * kernel cage thread is not yet ready to
			 * do the work. Use *this* thread, which is
			 * most likely to be t0, to do the work.
			 */
			KCAGE_STAT_INCR(kcw_expandearly);
			(void) kcage_expand();
			KCAGE_STAT_INC_SCAN_INDEX;
		}

		mutex_exit(&kcage_cageout_mutex);
	}
	/* else, kernel cage thread is already running */
}
