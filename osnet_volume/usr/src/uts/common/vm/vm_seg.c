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
 * 	Copyright (c) 1986-1990, 1995-1998 by Sun Microsystems, Inc.
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma ident	"@(#)vm_seg.c	1.64	99/05/04 SMI"
/*	From:	SVr4.0	"kernel:vm/vm_seg.c	1.14"		*/

/*
 * VM - segment management.
 */

#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/vmsystm.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/callb.h>
#include <sys/mem_config.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>

/*
 * kstats for segment pagelock cache
 */
segplckstat_t segplckstat = {
	{ "cache_hit",		KSTAT_DATA_ULONG },
	{ "cache_miss",		KSTAT_DATA_ULONG },
	{ "active_pages",	KSTAT_DATA_ULONG },
	{ "cached_pages",	KSTAT_DATA_ULONG },
	{ "purge_count",	KSTAT_DATA_ULONG },
};

kstat_named_t *segplckstat_ptr = (kstat_named_t *)&segplckstat;
uint segplckstat_ndata = sizeof (segplckstat) / sizeof (kstat_named_t);

/*
 * kstats for segment advise
 */
segadvstat_t segadvstat = {
	{ "MADV_FREE_hit",	KSTAT_DATA_ULONG },
	{ "MADV_FREE_miss",	KSTAT_DATA_ULONG },
};

kstat_named_t *segadvstat_ptr = (kstat_named_t *)&segadvstat;
uint segadvstat_ndata = sizeof (segadvstat) / sizeof (kstat_named_t);

/* #define	PDEBUG */
#if defined(PDEBUG) || defined(lint) || defined(__lint)
int pdebug = 0;
#else
#define	pdebug		0
#endif	/* PDEBUG */

#define	PPRINTF				if (pdebug) printf
#define	PPRINT(x)			PPRINTF(x)
#define	PPRINT1(x, a)			PPRINTF(x, a)
#define	PPRINT2(x, a, b)		PPRINTF(x, a, b)
#define	PPRINT3(x, a, b, c)		PPRINTF(x, a, b, c)
#define	PPRINT4(x, a, b, c, d)		PPRINTF(x, a, b, c, d)
#define	PPRINT5(x, a, b, c, d, e)	PPRINTF(x, a, b, c, d, e)

#define	P_HASHMASK		(p_hashsize - 1)
#define	P_BASESHIFT		6

/*
 * entry in the segment page cache
 */
struct seg_pcache {
	struct seg_pcache *p_hnext;	/* list for hashed blocks */
	struct seg_pcache *p_hprev;
	int		p_active;	/* active count */
	int		p_ref;		/* ref bit */
	size_t		p_len;		/* segment length */
	caddr_t		p_addr;		/* base address */
	struct seg 	*p_seg;		/* segment */
	struct page	**p_pp;		/* pp shadow list */
	enum seg_rw	p_rw;		/* rw */
	uint_t		p_flags;	/* bit flags */
	void		(*p_callback)(struct seg *, caddr_t, size_t,
			    struct page **, enum seg_rw);
};

struct seg_phash {
	struct seg_pcache *p_hnext;	/* list for hashed blocks */
	struct seg_pcache *p_hprev;
	int p_qlen;			/* Q length */
	kmutex_t p_hmutex;		/* protects hash bucket */
};

static int seg_preap_time = 20;	/* reclaim every 20 secs */
static int seg_pmaxqlen = 5;	/* max Q length in hash list */
static int seg_ppcount = 5;	/* max # of purges per reclaim interval */
static int seg_plazy = 1;	/* if 1, pages are cached after pageunlock */
static pgcnt_t seg_pwindow;	/* max # of pages that can be cached */
static pgcnt_t seg_plocked;	/* # of pages which are cached by pagelock */
int seg_preapahead;

static uint_t seg_pdisable = 0;	/* if not 0, caching temporarily disabled */

static int seg_pupdate_active = 1;	/* background reclaim thread */
static clock_t seg_preap_interval;	/* reap interval in ticks */

static kmutex_t seg_pcache;	/* protects the whole pagelock cache */
static kmutex_t seg_pmem;	/* protects window counter */
static ksema_t seg_psaync_sem;	/* sema for reclaim thread */
static struct seg_phash *p_hashtab;
static int p_hashsize;

#define	p_hash(seg) \
	(P_HASHMASK & \
	((uintptr_t)(seg) >> P_BASESHIFT))

#define	p_match(pcp, seg, addr, len, rw) \
	(((pcp)->p_seg == (seg) && \
	(pcp)->p_addr == (addr) && \
	(pcp)->p_rw == (rw) && \
	(pcp)->p_len == (len)) ? 1 : 0)

#define	p_match_pp(pcp, seg, addr, len, pp, rw) \
	(((pcp)->p_seg == (seg) && \
	(pcp)->p_addr == (addr) && \
	(pcp)->p_pp == (pp) && \
	(pcp)->p_rw == (rw) && \
	(pcp)->p_len == (len)) ? 1 : 0)


/*
 * lookup an address range in pagelock cache. Return shadow list
 * and bump up active count.
 */
struct page **
seg_plookup(struct seg *seg, caddr_t addr, size_t len, enum seg_rw rw)
{
	struct seg_pcache *pcp;
	struct seg_phash *hp;

	if (seg_plazy == 0) {
		return (NULL);
	}

	hp = &p_hashtab[p_hash(seg)];
	mutex_enter(&hp->p_hmutex);
	for (pcp = hp->p_hnext; pcp != (struct seg_pcache *)hp;
	    pcp = pcp->p_hnext) {
		if (p_match(pcp, seg, addr, len, rw)) {
			pcp->p_active++;
			mutex_exit(&hp->p_hmutex);

			PPRINT5("seg_plookup hit: seg %p, addr %p, "
			    "len %lx, count %d, pplist %p \n",
			    (void *)seg, (void *)addr, len, pcp->p_active,
			    (void *)pcp->p_pp);

			return (pcp->p_pp);
		}
	}
	mutex_exit(&hp->p_hmutex);

	PPRINT("seg_plookup miss:\n");

	return (NULL);
}

/*
 * mark address range inactive. If the cache is off or the address
 * range is not in the cache we call the segment driver to reclaim
 * the pages. Otherwise just decrement active count and set ref bit.
 */
void
seg_pinactive(struct seg *seg, caddr_t addr, size_t len, struct page **pp,
    enum seg_rw rw, void (*callback)(struct seg *, caddr_t, size_t,
    struct page **, enum seg_rw))
{
	struct seg_pcache *pcp;
	struct seg_phash *hp;

	if (seg_plazy == 0) {
		(*callback)(seg, addr, len, pp, rw);
		return;
	}
	hp = &p_hashtab[p_hash(seg)];
	mutex_enter(&hp->p_hmutex);
	for (pcp = hp->p_hnext; pcp != (struct seg_pcache *)hp;
	    pcp = pcp->p_hnext) {
		if (p_match_pp(pcp, seg, addr, len, pp, rw)) {
			pcp->p_active--;
			ASSERT(pcp->p_active >= 0);
			if (pcp->p_active == 0 && seg_pdisable) {
				int npages;

				ASSERT(callback == pcp->p_callback);
				/* free the entry */
				hp->p_qlen--;
				pcp->p_hprev->p_hnext = pcp->p_hnext;
				pcp->p_hnext->p_hprev = pcp->p_hprev;
				mutex_exit(&hp->p_hmutex);
				npages = pcp->p_len >> PAGESHIFT;
				kmem_free(pcp, sizeof (struct seg_pcache));
				mutex_enter(&seg_pmem);
				seg_plocked -= npages;
				segplckstat.cached_pages.value.ul -= npages;
				mutex_exit(&seg_pmem);
				goto out;
			}
			pcp->p_ref = 1;
			mutex_exit(&hp->p_hmutex);
			return;
		}
	}
	mutex_exit(&hp->p_hmutex);
out:
	(*callback)(seg, addr, len, pp, rw);
}


/*
 * insert address range with shadow list into pagelock cache. If
 * the cache is off or caching is temporarily disabled or the allowed
 * 'window' is exceeded - return SEGP_FAIL. Otherwise return
 * SEGP_SUCCESS.
 */
int
seg_pinsert(struct seg *seg, caddr_t addr, size_t len, struct page **pp,
    enum seg_rw rw, uint_t flags, void (*callback)(struct seg *, caddr_t,
    size_t, struct page **, enum seg_rw))
{
	struct seg_pcache *pcp;
	struct seg_phash *hp;
	ssize_t npages;
	long wired;

	if (seg_plazy == 0) {
		return (SEGP_FAIL);
	}
	if (seg_pdisable != 0) {
		return (SEGP_FAIL);
	}
	hp = &p_hashtab[p_hash(seg)];
	if (hp->p_qlen > seg_pmaxqlen) {
		return (SEGP_FAIL);
	}
	npages = len >> PAGESHIFT;
	mutex_enter(&seg_pmem);
	wired = seg_plocked + npages;
	/*
	 * If the SEGP_FORCE_WIRED flag is set,
	 * we skip the check for seg_pwindow.
	 */
	if ((flags & SEGP_FORCE_WIRED) == 0) {
		if (wired > seg_pwindow) {
			mutex_exit(&seg_pmem);
			return (SEGP_FAIL);
		}
	}
	seg_plocked = wired;
	segplckstat.cached_pages.value.ul += (uint32_t)npages;
	mutex_exit(&seg_pmem);

	pcp = kmem_alloc(sizeof (struct seg_pcache), KM_SLEEP);
	pcp->p_seg = seg;
	pcp->p_addr = addr;
	pcp->p_len = len;
	pcp->p_pp = pp;
	pcp->p_rw = rw;
	pcp->p_callback = callback;
	pcp->p_active = 1;
	pcp->p_flags = flags;

	PPRINT4("seg_pinsert: seg %p, addr %p, len %lx, pplist %p\n",
	    (void *)seg, (void *)addr, len, (void *)pp);

	hp = &p_hashtab[p_hash(seg)];
	mutex_enter(&hp->p_hmutex);
	hp->p_qlen++;
	pcp->p_hnext = hp->p_hnext;
	pcp->p_hprev = (struct seg_pcache *)hp;
	hp->p_hnext->p_hprev = pcp;
	hp->p_hnext = pcp;
	mutex_exit(&hp->p_hmutex);
	return (SEGP_SUCCESS);
}

/*
 * purge all entries from the pagelock cache if not active
 * and not recently used. Drop all locks and call through
 * the address space into the segment driver to reclaim
 * the pages. This makes sure we get the address space
 * and segment driver locking right.
 */
static void
seg_ppurge_all(int force)
{
	struct seg_pcache *delcallb_list = NULL;
	struct seg_pcache *pcp;
	struct seg_phash *hp;
	int purge_count = 0;
	ssize_t npages = 0;

	/*
	 * if the cache if off or empty, return
	 */
	if (seg_plazy == 0 || seg_plocked == 0) {
		return;
	}
	for (hp = p_hashtab; hp < &p_hashtab[p_hashsize]; hp++) {
		mutex_enter(&hp->p_hmutex);
		pcp = hp->p_hnext;
		while ((pcp != (struct seg_pcache *)hp) &&
				(purge_count <= seg_ppcount)) {

			/*
			 * purge entries which are not active and
			 * have not been used recently and
			 * have the SEGP_ASYNC_FLUSH flag.
			 *
			 * In the 'force' case, we ignore the
			 * SEGP_ASYNC_FLUSH flag.
			 */
			if (!(pcp->p_flags & SEGP_ASYNC_FLUSH))
				pcp->p_ref = 1;
			if (force)
				pcp->p_ref = 0;
			if (!pcp->p_ref && !pcp->p_active) {
				struct as *as = pcp->p_seg->s_as;

				/*
				 * try to get the readers lock on the address
				 * space before taking out the cache element.
				 * This ensures as_pagereclaim() can actually
				 * call through the address space and free
				 * the pages. If we don't get the lock, just
				 * skip this entry. The pages will be reclaimed
				 * by the segment driver at unmap time.
				 */
				if (AS_LOCK_TRYENTER(as, &as->a_lock,
				    RW_READER)) {
					hp->p_qlen--;
					pcp->p_hprev->p_hnext = pcp->p_hnext;
					pcp->p_hnext->p_hprev = pcp->p_hprev;
					pcp->p_hprev = delcallb_list;
					delcallb_list = pcp;
					purge_count++;
				}
			} else {
				pcp->p_ref = 0;
			}
			pcp = pcp->p_hnext;
		}
		mutex_exit(&hp->p_hmutex);
		if (!force && purge_count > seg_ppcount)
			break;
	}

	/*
	 * run the delayed callback list. We don't want to hold the
	 * cache lock during a call through the address space.
	 */
	while (delcallb_list != NULL) {
		struct as *as;

		pcp = delcallb_list;
		delcallb_list = pcp->p_hprev;
		as = pcp->p_seg->s_as;

		PPRINT4("seg_ppurge_all: purge seg %p, addr %p, len %lx, "
		    "pplist %p\n", (void *)pcp->p_seg, (void *)pcp->p_addr,
		    pcp->p_len, (void *)pcp->p_pp);

		as_pagereclaim(as, pcp->p_pp, pcp->p_addr,
		    pcp->p_len, pcp->p_rw);
		AS_LOCK_EXIT(as, &as->a_lock);
		npages += pcp->p_len >> PAGESHIFT;
		kmem_free(pcp, sizeof (struct seg_pcache));
	}
	mutex_enter(&seg_pmem);
	seg_plocked -= npages;
	segplckstat.cached_pages.value.ul -= (uint32_t)npages;
	mutex_exit(&seg_pmem);
}

/*
 * purge all entries for a given segment. Since we
 * callback into the segment driver directly for page
 * reclaim the caller needs to hold the right locks.
 */
void
seg_ppurge(struct seg *seg)
{
	struct seg_pcache *delcallb_list = NULL;
	struct seg_pcache *pcp;
	struct seg_phash *hp;
	ssize_t npages = 0;

	if (seg_plazy == 0) {
		return;
	}
	segplckstat.purge_count.value.ul++;
	hp = &p_hashtab[p_hash(seg)];
	mutex_enter(&hp->p_hmutex);
	pcp = hp->p_hnext;
	while (pcp != (struct seg_pcache *)hp) {
		if (pcp->p_seg == seg) {
			if (pcp->p_active) {
				break;
			}
			hp->p_qlen--;
			pcp->p_hprev->p_hnext = pcp->p_hnext;
			pcp->p_hnext->p_hprev = pcp->p_hprev;
			pcp->p_hprev = delcallb_list;
			delcallb_list = pcp;
		}
		pcp = pcp->p_hnext;
	}
	mutex_exit(&hp->p_hmutex);
	while (delcallb_list != NULL) {
		pcp = delcallb_list;
		delcallb_list = pcp->p_hprev;

		PPRINT4("seg_ppurge: purge seg %p, addr %p, len %lx, "
		    "pplist %p\n", (void *)seg, (void *)pcp->p_addr,
		    pcp->p_len, (void *)pcp->p_pp);

		ASSERT(seg == pcp->p_seg);
		(*pcp->p_callback)(seg, pcp->p_addr,
		    pcp->p_len, pcp->p_pp, pcp->p_rw);
		npages += pcp->p_len >> PAGESHIFT;
		kmem_free(pcp, sizeof (struct seg_pcache));
	}
	mutex_enter(&seg_pmem);
	seg_plocked -= npages;
	segplckstat.cached_pages.value.ul -= (uint32_t)npages;
	mutex_exit(&seg_pmem);
}

static void seg_pinit_mem_config(void);

/*
 * setup the pagelock cache
 */
static void
seg_pinit(void)
{
	struct seg_phash *hp;
	int i;
	u_int physmegs;

	sema_init(&seg_psaync_sem, 0, NULL, SEMA_DEFAULT, NULL);

	mutex_enter(&seg_pcache);
	if (p_hashtab == NULL) {
		physmegs = physmem >> (20 - PAGESHIFT);

		/* If p_hashsize was not set in /etc/system ... */
		if (p_hashsize == 0) {
			/*
			 * Choose p_hashsize based on physmem.
			 */
			if (physmegs < 64) {
				p_hashsize = 64;
			} else if (physmegs < 1024) {
				p_hashsize = 1024;
			} else {
				p_hashsize = 8192;
			}
		}

		p_hashtab = kmem_zalloc(
			p_hashsize * sizeof (struct seg_phash), KM_SLEEP);
		for (i = 0; i < p_hashsize; i++) {
			hp = (struct seg_phash *)&p_hashtab[i];
			hp->p_hnext = (struct seg_pcache *)hp;
			hp->p_hprev = (struct seg_pcache *)hp;
			mutex_init(&hp->p_hmutex, NULL, MUTEX_DEFAULT, NULL);
		}
		if (seg_pwindow == 0) {
			if (physmegs < 24) {
				/* don't use cache */
				seg_plazy = 0;
			} else if (physmegs < 64) {
				seg_pwindow = physmem >> 5; /* 3% of memory */
			} else {
				seg_pwindow = physmem >> 3; /* 12% of memory */
			}
		}
	}
	mutex_exit(&seg_pcache);

	seg_pinit_mem_config();
}

/*
 * called by pageout if memory is low
 */
void
seg_preap(void)
{
	/*
	 * if the cache if off or empty, return
	 */
	if (seg_plocked == 0 || seg_plazy == 0) {
		return;
	}
	sema_v(&seg_psaync_sem);
}

static void seg_pupdate(void *);

/*
 * run as a backgroud thread and reclaim pagelock
 * pages which have not been used recently
 */
void
seg_pasync_thread(void)
{
	callb_cpr_t cpr_info;
	kmutex_t pasync_lock;	/* just for CPR stuff */

	mutex_init(&pasync_lock, NULL, MUTEX_DEFAULT, NULL);

	CALLB_CPR_INIT(&cpr_info, &pasync_lock,
		callb_generic_cpr, "seg_pasync");

	if (seg_preap_interval == 0) {
		seg_preap_interval = seg_preap_time * hz;
	} else {
		seg_preap_interval *= hz;
	}
	if (seg_plazy && seg_pupdate_active) {
		(void) timeout(seg_pupdate, NULL, seg_preap_interval);
	}

	for (;;) {
		mutex_enter(&pasync_lock);
		CALLB_CPR_SAFE_BEGIN(&cpr_info);
		mutex_exit(&pasync_lock);
		sema_p(&seg_psaync_sem);
		mutex_enter(&pasync_lock);
		CALLB_CPR_SAFE_END(&cpr_info, &pasync_lock);
		mutex_exit(&pasync_lock);

		seg_ppurge_all(0);
	}
}

static void
seg_pupdate(void *dummy)
{
	sema_v(&seg_psaync_sem);

	if (seg_plazy && seg_pupdate_active) {
		(void) timeout(seg_pupdate, dummy, seg_preap_interval);
	}
}

static struct kmem_cache *seg_cache;

/*
 * Initialize segment management data structures.
 */
void
seg_init(void)
{
	kstat_t *ksp;

	seg_cache = kmem_cache_create("seg_cache", sizeof (struct seg),
		0, NULL, NULL, NULL, NULL, NULL, 0);

	ksp = kstat_create("unix", 0, "segplckstat", "vm", KSTAT_TYPE_NAMED,
		segplckstat_ndata, KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *)segplckstat_ptr;
		kstat_install(ksp);
	}

	ksp = kstat_create("unix", 0, "segadvstat", "vm", KSTAT_TYPE_NAMED,
		segadvstat_ndata, KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *)segadvstat_ptr;
		kstat_install(ksp);
	}

	seg_pinit();
}

/*
 * Allocate a segment to cover [base, base+size]
 * and attach it to the specified address space.
 */
struct seg *
seg_alloc(struct as *as, caddr_t base, size_t size)
{
	struct seg *new;
	caddr_t segbase;
	size_t segsize;

	segbase = (caddr_t)((uintptr_t)base & PAGEMASK);
	segsize = (((uintptr_t)(base + size) + PAGEOFFSET) & PAGEMASK) -
	    (uintptr_t)segbase;

	if (!valid_va_range(&segbase, &segsize, segsize, AH_LO))
		return ((struct seg *)NULL);	/* bad virtual addr range */

	if (as != &kas &&
	    valid_usr_range(segbase, segsize, 0, as,
	    as->a_userlimit) != RANGE_OKAY)
		return ((struct seg *)NULL);	/* bad virtual addr range */

	new = kmem_cache_alloc(seg_cache, KM_SLEEP);
	new->s_ops = NULL;
	new->s_data = NULL;
	if (seg_attach(as, segbase, segsize, new) < 0) {
		kmem_cache_free(seg_cache, new);
		return ((struct seg *)NULL);
	}
	/* caller must fill in ops, data */
	return (new);
}

/*
 * Attach a segment to the address space.  Used by seg_alloc()
 * and for kernel startup to attach to static segments.
 */
int
seg_attach(struct as *as, caddr_t base, size_t size, struct seg *seg)
{
	seg->s_as = as;
	seg->s_base = base;
	seg->s_size = size;

	/*
	 * as_addseg() will add the segment at the appropraite point
	 * in the list. It will return -1 if there is overlap with
	 * an already existing segment.
	 */
	return (as_addseg(as, seg));
}

/*
 * Unmap a segment and free it from its associated address space.
 * This should be called by anybody who's finished with a whole segment's
 * mapping.  Just calls SEGOP_UNMAP() on the whole mapping .  It is the
 * responsibility of the segment driver to unlink the the segment
 * from the address space, and to free public and private data structures
 * associated with the segment.  (This is typically done by a call to
 * seg_free()).
 */
void
seg_unmap(struct seg *seg)
{
#ifdef DEBUG
	int ret;
#endif /* DEBUG */

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/* Shouldn't have called seg_unmap if mapping isn't yet established */
	ASSERT(seg->s_data != NULL);

	/* Unmap the whole mapping */
#ifdef DEBUG
	ret = SEGOP_UNMAP(seg, seg->s_base, seg->s_size);
	ASSERT(ret == 0);
#else
	SEGOP_UNMAP(seg, seg->s_base, seg->s_size);
#endif /* DEBUG */
}

/*
 * Free the segment from its associated as. This should only be called
 * if a mapping to the segment has not yet been established (e.g., if
 * an error occurs in the middle of doing an as_map when the segment
 * has already been partially set up) or if it has already been deleted
 * (e.g., from a segment driver unmap routine if the unmap applies to the
 * entire segment). If the mapping is currently set up then seg_unmap() should
 * be called instead.
 */
void
seg_free(struct seg *seg)
{
	register struct as *as = seg->s_as;
	struct seg *tseg = as_removeseg(as, seg->s_base);

	ASSERT(tseg == seg);

	/*
	 * If the segment private data field is NULL,
	 * then segment driver is not attached yet.
	 */
	if (seg->s_data != NULL)
		SEGOP_FREE(seg);

	kmem_cache_free(seg_cache, seg);
}

/*ARGSUSED*/
static void
seg_p_mem_config_post_add(
	void *arg,
	pgcnt_t delta_pages)
{
	/* Nothing to do. */
}

/*ARGSUSED*/
static int
seg_p_mem_config_pre_del(
	void *arg,
	pgcnt_t delta_pages)
{
	mutex_enter(&seg_pcache);
	seg_pdisable++;
	ASSERT(seg_pdisable != 0);
	mutex_exit(&seg_pcache);

	while (seg_plocked != 0) {
		seg_ppurge_all(1);
		if (seg_plocked != 0) {
			delay(hz/4);
		}
	}
	return (0);
}

/*ARGSUSED*/
static void
seg_p_mem_config_post_del(
	void *arg,
	pgcnt_t delta_pages,
	int cancelled)
{
	mutex_enter(&seg_pcache);
	ASSERT(seg_pdisable != 0);
	seg_pdisable--;
	mutex_exit(&seg_pcache);
}

static kphysm_setup_vector_t seg_p_mem_config_vec = {
	KPHYSM_SETUP_VECTOR_VERSION,
	seg_p_mem_config_post_add,
	seg_p_mem_config_pre_del,
	seg_p_mem_config_post_del,
};

static void
seg_pinit_mem_config(void)
{
	int ret;

	ret = kphysm_setup_func_register(&seg_p_mem_config_vec, (void *)NULL);
	/*
	 * Want to catch this in the debug kernel. At run time, if the
	 * callbacks don't get run all will be OK as the disable just makes
	 * it more likely that the pages can be collected.
	 */
	ASSERT(ret == 0);
}
