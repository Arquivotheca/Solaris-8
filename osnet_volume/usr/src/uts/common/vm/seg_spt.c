/*
 * Copyright (c) 1993, 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)seg_spt.c 1.49	99/08/02 SMI"

#include <sys/param.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <vm/hat.h>
#include <vm/seg.h>
#include <vm/as.h>
#include <vm/anon.h>
#include <vm/page.h>
#include <sys/buf.h>
#include <sys/swap.h>
#include <sys/atomic.h>
#include <vm/seg_spt.h>
#include <sys/debug.h>
#include <sys/vtrace.h>

#include <sys/tnf_probe.h>

#define	SEGSPTADDR	(caddr_t)0x0

/*
 * # pages used for spt
 */
static size_t	spt_used;

/*
 * segspt_minfree is the memory left for system after ISM
 * locked its pages; it is set up to 5% of availrmem in
 * sptcreate when ISM is created.  ISM should not use more
 * than ~90% of availrmem; if it does, then the performance
 * of the system may decrease. Machines with large memories may
 * be able to use up more memory for ISM so we set the default
 * segspt_minfree to 5% (which gives ISM max 95% of availrmem.
 * If somebody wants even more memory for ISM (risking hanging
 * the system) they can patch the segspt_minfree to smaller number.
 */
pgcnt_t segspt_minfree = 0;

static int segspt_create(struct seg *seg, caddr_t argsp);
static int segspt_unmap(struct seg *seg, caddr_t raddr, size_t ssize);
static void segspt_free(struct seg *seg);
static int segspt_lockop(struct seg *seg, caddr_t addr, size_t len, int attr,
    int op, ulong *lockmap, size_t pos);
static int segspt_badops();
static void segspt_free_pages(struct seg *seg, caddr_t addr, size_t len);
static int segspt_kluster(struct seg *seg, caddr_t addr, ssize_t delta);

struct seg_ops segspt_ops = {

	segspt_badops,
	segspt_unmap,
	segspt_free,
	segspt_badops,
	segspt_badops,
	segspt_badops,
	segspt_badops,
	segspt_kluster,
	(size_t (*)()) segspt_badops,	/* swapout */
	segspt_badops,
	(size_t (*)()) segspt_badops,	/* incore */
	segspt_lockop,
	segspt_badops,
	(u_offset_t (*)()) segspt_badops,
	segspt_badops,
	segspt_badops,
	segspt_badops,		/* advise */
	(void (*)()) segspt_badops,
	segspt_badops,
	segspt_badops,
};

static int segspt_shmdup(struct seg *seg, struct seg *newseg);
static int segspt_shmunmap(struct seg *seg, caddr_t raddr, size_t ssize);
static void segspt_shmfree(struct seg *seg);
static faultcode_t segspt_shmfault(struct hat *hat, struct seg *seg,
		caddr_t addr, size_t len, enum fault_type type, enum seg_rw rw);
static faultcode_t segspt_shmfaulta(struct seg *seg, caddr_t addr);
static int segspt_shmsetprot(register struct seg *seg, register caddr_t addr,
			register size_t len, register u_int prot);
static int segspt_shmcheckprot(struct seg *seg, caddr_t addr, size_t size,
			u_int prot);
static int	segspt_shmkluster(struct seg *seg, caddr_t addr, ssize_t delta);
static size_t	segspt_shmswapout(struct seg *seg);
static size_t segspt_shmincore(struct seg *seg, caddr_t addr, size_t len,
			register char *vec);
static int segspt_shmsync(struct seg *seg, register caddr_t addr, size_t len,
			int attr, u_int flags);
static int segspt_shmlockop(struct seg *seg, caddr_t addr, size_t len, int attr,
			int op, ulong *lockmap, size_t pos);
static int segspt_shmgetprot(struct seg *seg, caddr_t addr, size_t len,
			u_int *protv);
static u_offset_t segspt_shmgetoffset(struct seg *seg, caddr_t addr);
static int segspt_shmgettype(struct seg *seg, caddr_t addr);
static int segspt_shmgetvp(struct seg *seg, caddr_t addr, struct vnode **vpp);
static int segspt_shmadvice(struct seg *seg, caddr_t addr, size_t len,
			u_int behav);
static void segspt_shmdump(struct seg *seg);
static int segspt_shmpagelock(struct seg *, caddr_t, size_t,
			struct page ***, enum lock_type, enum seg_rw);
static int segspt_shmgetmemid(struct seg *, caddr_t, memid_t *);

struct seg_ops segspt_shmops = {
	segspt_shmdup,
	segspt_shmunmap,
	segspt_shmfree,
	segspt_shmfault,
	segspt_shmfaulta,
	segspt_shmsetprot,
	segspt_shmcheckprot,
	segspt_shmkluster,
	segspt_shmswapout,
	segspt_shmsync,
	segspt_shmincore,
	segspt_shmlockop,
	segspt_shmgetprot,
	segspt_shmgetoffset,
	segspt_shmgettype,
	segspt_shmgetvp,
	segspt_shmadvice,	/* advise */
	segspt_shmdump,
	segspt_shmpagelock,
	segspt_shmgetmemid,
};

static void segspt_purge(struct seg *seg);
static void segspt_reclaim(struct seg *, caddr_t, size_t, struct page **,
    enum seg_rw);

/* ARGSUSED */
int
sptcreate(size_t size, struct seg **sptseg, struct anon_map *amp, uint_t prot)
{
	int err;
	struct as	*newas;
	struct	segspt_crargs sptcargs;

#ifdef DEBUG
	TNF_PROBE_1(sptcreate, "spt", /* CSTYLED */,
                	tnf_ulong, size, size );
#endif
	if (segspt_minfree == 0)	/* leave min 5% of availrmem for */
		segspt_minfree = availrmem/20;	/* for the system */

	if (!hat_supported(HAT_SHARED_PT, (void *)0))
		return (EINVAL);
	/*
	 * get a new as for this shared memory segment
	 */
	newas = as_alloc();
	sptcargs.amp = amp;
	sptcargs.prot = prot;

	/*
	 * create a shared page table (spt) segment
	 */

	if (err = as_map(newas, SEGSPTADDR, size, segspt_create, &sptcargs)) {
		as_free(newas);
		return (err);
	}
	*sptseg = sptcargs.seg_spt;
	return (0);
}

void
sptdestroy(struct as *as, struct anon_map *amp)
{

#ifdef DEBUG
	TNF_PROBE_0(sptdestroy, "spt", /* CSTYLED */);
#endif
	(void) as_unmap(as, SEGSPTADDR, amp->size);
	as_free(as);

	TRACE_2(TR_FAC_VM, TR_ANON_SHM, "anon shm: as %p, amp %p", as, amp);
}

/*
 * called from seg_free().
 * free (i.e., unlock, unmap, return to free list)
 *  all the pages in the given seg.
 */
void
segspt_free(struct seg	*seg)
{
	struct spt_data *spt = (struct spt_data *)seg->s_data;

	TRACE_1(TR_FAC_VM, TR_ANON_SHM, "anon shm: seg %p", seg);

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	if (spt != NULL) {
		if (spt->realsize)
			segspt_free_pages(seg, seg->s_base, spt->realsize);

		kmem_free(spt->vp, sizeof (*spt->vp));
		mutex_destroy(&spt->lock);
		kmem_free(spt, sizeof (*spt));
	}
}

/* ARGSUSED */
static int
segspt_shmsync(struct seg *seg, caddr_t addr, size_t len, int attr, u_int flags)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (0);
}

/*
 * segspt pages are always "in core" since the memory is locked down.
 */
/* ARGSUSED */
static size_t
segspt_shmincore(struct seg *seg, caddr_t addr, size_t len, char *vec)
{

	caddr_t eo_seg;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
#ifdef lint
	seg = seg;
#endif

	eo_seg = addr + len;
	while (addr < eo_seg) {
		/* page exist, and it's locked. */
		*vec++ = (char)0x9;
		addr += PAGESIZE;
	}
	return (len);
}

/*
 * called from as_ctl(, MC_LOCK,)
 *
 */
/* ARGSUSED */
static int
segspt_lockop(struct seg *seg, caddr_t addr, size_t len, int attr,
    int op, ulong *lockmap, size_t pos)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
	/*
	 * for spt, as->a_paglck is never set
	 * so this routine should not be called.
	 * XXX Should this be a BADOP?
	 */
	return (0);
}

static int
segspt_unmap(struct seg *seg, caddr_t raddr, size_t ssize)
{
	size_t share_size;

	TRACE_3(TR_FAC_VM, TR_ANON_SHM, "anon shm: seg %p, addr %p, size %x",
	    seg, raddr, ssize);

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * seg.s_size may have been rounded up to the largest page size
	 * in shmat().
	 * XXX This should be cleanedup. sptdestroy should take a length
	 * argument which should be the same as sptcreate. Then
	 * this rounding would not be needed (or is done in shm.c)
	 * Only the check for full segment will be needed.
	 */
	share_size = page_get_pagesize(page_num_pagesizes() - 1);
	ssize = roundup(ssize, share_size);

	if (raddr == seg->s_base && ssize == seg->s_size) {
		seg_free(seg);
		return (0);
	} else
		return (EINVAL);
}

int
segspt_badops()
{
	cmn_err(CE_PANIC, "segspt_badops is called");
	return (0);
}

int
segspt_create(struct seg *seg, caddr_t argsp)
{
	int		err = ENOMEM;
#ifdef DEBUG
	size_t		len  = seg->s_size;
#endif /* DEBUG */
	caddr_t		addr = seg->s_base;
	struct spt_data *spt;
	struct 	segspt_crargs *sptcargs = (struct segspt_crargs *)argsp;
	struct anon_map *amp = sptcargs->amp;
	struct	cred	*cred;
	u_long		i, j, anon_index;
	pgcnt_t		npages;
	struct vnode	*vp;
	page_t		**ppa;
	u_int		hat_flags;

	/*
	 * We are holding the a_lock on the underlying dummy as,
	 * so we can make calls to the HAT layer.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

#ifdef DEBUG
	TNF_PROBE_2(segspt_create, "spt", /* CSTYLED */,
                                tnf_opaque, addr, addr,
				tnf_ulong, len, len);
#endif

	npages = btopr(amp->size);

	if (err = anon_swap_adjust(npages)) {
		return (err);
	}
	err = ENOMEM;

	if ((spt = kmem_zalloc(sizeof (*spt), KM_NOSLEEP)) == NULL)
		goto out1;
	mutex_init(&spt->lock, NULL, MUTEX_DEFAULT, NULL);

	if ((ppa = kmem_zalloc(((sizeof (page_t *)) * npages),
	    KM_NOSLEEP)) == NULL)
		goto out2;

	if ((vp = kmem_zalloc(sizeof (*vp), KM_NOSLEEP)) == NULL)
		goto out3;

	seg->s_ops = &segspt_ops;
	spt->vp = vp;
	spt->amp = amp;
	spt->prot = sptcargs->prot;
	seg->s_data = (caddr_t)spt;

	/*
	 * get array of pages for each anon slot in amp
	 */
	cred = CRED();
	anon_index = 0;

	if ((err = anon_map_getpages(amp, anon_index, ptob(npages), ppa,
	    seg, addr, S_CREATE, cred)) != 0)
		goto out4;

	/*
	 * addr is initial address corresponding to the first page on ppa list
	 */
	for (i = 0; i < npages; i++) {
		/* attempt to lock all pages */
		if (!page_pp_lock(ppa[i], 0, 1)) {
			/*
			 * if unable to lock any page, unlock all
			 * of them and return error
			 */
			for (j = 0; j < i; j++)
				page_pp_unlock(ppa[j], 0, 1);
			for (i = 0; i < npages; i++) {
				page_unlock(ppa[i]);
			}
			err = ENOMEM;
			goto out4;
		}
	}

	hat_flags = HAT_LOAD_SHARE;
	/*
	 * Some platforms assume that ISM mappings are HAT_LOAD_LOCK
	 * for the entire life of the segment.
	 */
	if (!hat_supported(HAT_DYNAMIC_ISM_UNMAP, (void *)0))
		hat_flags |= HAT_LOAD_LOCK;
	hat_memload_array(seg->s_as->a_hat, addr, ptob(npages),
	    ppa, spt->prot, hat_flags);

	/*
	 * On platforms that do not support HAT_DYNAMIC_ISM_UNMAP,
	 * we will leave the pages locked SE_SHARED for the life
	 * of the ISM segment. This will prevent any calls to
	 * hat_pageunload() on this ISM segment for those platforms.
	 */
	if (!(hat_flags & HAT_LOAD_LOCK)) {
		/*
		 * On platforms that support HAT_DYNAMIC_ISM_UNMAP,
		 * we no longer need to hold the SE_SHARED lock on the pages,
		 * since L_PAGELOCK and F_SOFTLOCK calls will grab the
		 * SE_SHARED lock on the pages as necessary.
		 */
		for (i = 0; i < npages; i++)
			page_unlock(ppa[i]);
	}
	spt->ppa = NULL;
	spt->pcachecnt = 0;
	kmem_free(ppa, ((sizeof (page_t *)) * npages));
	spt->realsize = ptob(npages);
	atomic_add_long(&spt_used, npages);
	sptcargs->seg_spt = seg;
	return (0);

out4:
	seg->s_data = NULL;
	kmem_free(vp, sizeof (*vp));
out3:
	kmem_free(ppa, ((sizeof (page_t *)) * npages));
out2:
	mutex_destroy(&spt->lock);
	kmem_free(spt, sizeof (*spt));
out1:
	anon_swap_restore(npages);
	return (err);
}

/* ARGSUSED */
void
segspt_free_pages(struct seg *seg, caddr_t addr, size_t len)
{
	struct page 	*pp;
	struct spt_data *spt = (struct spt_data *)seg->s_data;
	pgcnt_t		npages;
	ulong_t		anon_idx;
	struct anon_map *amp;
	struct anon 	*ap;
	struct vnode 	*vp;
	u_offset_t 	off;
	u_int		hat_flags;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	TRACE_3(TR_FAC_VM, TR_ANON_SHM, "anon shm: seg %p, addr %p, spt %p",
	    seg, addr, spt);

	len = roundup(len, PAGESIZE);
	npages = btop(len);
	if (hat_supported(HAT_DYNAMIC_ISM_UNMAP, (void *)0))
		hat_flags = HAT_UNLOAD;
	else
		hat_flags = HAT_UNLOAD_UNLOCK;

	hat_unload(seg->s_as->a_hat, addr, len, hat_flags);

	amp = spt->amp;
	ASSERT(amp);
	for (anon_idx = 0; anon_idx < npages; anon_idx++) {
		if ((ap = anon_get_ptr(amp->ahp, anon_idx)) == NULL) {
			cmn_err(CE_PANIC, "segspt_free_pages: null app");
		}
		swap_xlate(ap, &vp, &off);
		/*
		 * If this platform supports HAT_DYNAMIC_ISM_UNMAP,
		 * the pages won't be having SE_SHARED lock at this
		 * point.
		 *
		 * On platforms that do not support HAT_DYNAMIC_ISM_UNMAP,
		 * the pages are still held SE_SHARED locked from the
		 * original segspt_create()
		 *
		 * Our goal is to get SE_EXCL lock on each page, remove
		 * permanent lock on it and invalidate the page.
		 */
		if (hat_flags == HAT_UNLOAD)
			pp = page_lookup(vp, off, SE_EXCL);
		else {
			if ((pp = page_find(vp, off)) == NULL) {
				cmn_err(CE_PANIC,
					"segspt_free_pages: page not locked");
			}

			if (!page_tryupgrade(pp)) {
				page_unlock(pp);
				pp = page_lookup(vp, off, SE_EXCL);
			}
		}
		if (pp == NULL) {
			cmn_err(CE_PANIC,
				"segspt_free_pages: page not in the system");
		}
		page_pp_unlock(pp, 0, 1);

		/*
		 * It's logical to invalidate the pages here as in most cases
		 * these were created by segspt. Besides, if these pages are
		 * constituents of a largepage and if we just unlock the pages
		 * without invalidating them, pageout_scanner might free them
		 * and place them on the staging list (sun4u) before shm_rm_amp
		 * gets called. This will lead to a panic in page_list_sub()
		 * when page_lookup (called from shmem_unlock() or anon_decref
		 * thru anon_free) tries to reclaim it as P_FREE bit is set but
		 * the page is not in page cachelist.
		 */

		/*LINTED: constant in conditional context */
		VN_DISPOSE(pp, B_INVAL, 0, kcred);
	}

	/*
	 * mark that pages have been released
	 */
	spt->realsize = 0;
	atomic_add_long(&spt_used, -npages);
	anon_swap_restore(npages);
}

/*
 * return locked pages over a given range.
 *
 * We will cache the entire ISM segment and save the pplist for the
 * entire segment in the ppa field of the underlying ISM segment structure.
 * Later, during a call to segspt_reclaim() we will use this ppa array
 * to page_unlock() all of the pages and then we will free this ppa list.
 */
/* ARGSUSED */
static int
segspt_shmpagelock(struct seg *seg, caddr_t addr, size_t len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	struct seg	*sptseg;
	struct spt_data *spt_sd;
	pgcnt_t np, page_index, npages, tot_pages;
	caddr_t a, base_addr;
	struct page **pplist, **pl, *pp;
	struct anon_map *amp;
	u_long anon_index;
	int ret = ENOTSUP;
	u_int pl_built = 0;


	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	page_index = seg_page(seg, addr);
	npages = btopr(len);
	tot_pages = btopr(ssd->amp->size);

	/*
	 * check if the request is larger than number of pages covered
	 * by amp
	 */
	if (page_index + npages > tot_pages)
		return (EFAULT);

	/*
	 * We want to lock/unlock the entire ISM segment. Therefore,
	 * we will be using the underlying sptseg and it's base address
	 * and length for the caching arguments.
	 */
	sptseg = ssd->sptseg;
	ASSERT(sptseg);
	spt_sd = sptseg->s_data;
	ASSERT(spt_sd);

	base_addr = sptseg->s_base;

	if (type == L_PAGEUNLOCK) {
		ASSERT(spt_sd->ppa != NULL);
		seg_pinactive(seg, seg->s_base, ptob(tot_pages), spt_sd->ppa,
			spt_sd->prot, segspt_reclaim);

		/*
		* If someone is blocked while unmapping, we purge
		* segment page cache and thus reclaim pplist synchronously
		* without waiting for seg_pasync_thread. This speeds up
		* unmapping in cases where munmap(2) is called, while
		* raw async i/o is still in progress or where a thread
		* exits on data fault in a multithreaded application.
		*/
		if (AS_ISUNMAPWAIT(seg->s_as) && (ssd->softlockcnt > 0)) {
			segspt_purge(seg);
		}
		return (0);
	} else if (type == L_PAGERECLAIM) {
		ASSERT(spt_sd->ppa != NULL);
		segspt_reclaim(seg, seg->s_base, ptob(tot_pages),
			spt_sd->ppa, spt_sd->prot);
		return (0);
	}

	/*
	 * First try to find pages in segment page cache, without
	 * holding the segment lock.
	 */
	pplist = seg_plookup(seg, seg->s_base, ptob(tot_pages), spt_sd->prot);
	if (pplist != NULL) {
		segplckstat.cache_hit.value.ul++;
		ASSERT(spt_sd->ppa == pplist);
		/*
		 * Since we cache the entire segment, we want to
		 * set ppp to point to the first slot that corresponds
		 * to the requested addr, i.e. page_index.
		 */
		*ppp = &(spt_sd->ppa[page_index]);

		return (0);
	}

	/* The L_PAGELOCK case... */
	mutex_enter(&spt_sd->lock);

	/*
	* try to find pages in segment page cache
	*/
	pplist = seg_plookup(seg, seg->s_base, ptob(tot_pages), spt_sd->prot);
	if (pplist != NULL) {
		segplckstat.cache_hit.value.ul++;
		ASSERT(spt_sd->ppa == pplist);
		/*
		 * Since we cache the entire segment, we want to
		 * set ppp to point to the first slot that corresponds
		 * to the requested addr, i.e. page_index.
		 */
		*ppp = &(spt_sd->ppa[page_index]);

		mutex_exit(&spt_sd->lock);
		return (0);
	} else {
		segplckstat.cache_miss.value.ul++;
	}

	/*
	* No need to worry about protections because ISM pages
	* are always rw.
	*/

	pl = pplist = NULL;
	/*
	 * Do we need to build the ppa array?
	 */
	if (spt_sd->ppa == NULL) {
		pl_built = 1;
		/*
		 * availrmem is decremented once during anon_swap_adjust()
		 * and is incremented during the anon_unresv(), which is
		 * called from shm_rm_amp() when the segment is destroyed.
		 */
		segplckstat.active_pages.value.ul += tot_pages;
		amp = spt_sd->amp;
		ASSERT(amp != NULL);


		/* pcachecnt is protected by spt_sd->lock */
		ASSERT(spt_sd->pcachecnt == 0);
		pplist = kmem_zalloc(sizeof (page_t *) * tot_pages, KM_SLEEP);
		pl = pplist;

		anon_index = seg_page(sptseg, base_addr);
		mutex_enter(&amp->lock);
		for (a = base_addr; a < (base_addr + ptob(tot_pages));
			a += PAGESIZE, anon_index++) {

			struct anon *ap;
			struct vnode *vp;
			u_offset_t off;

			ap = anon_get_ptr(amp->ahp, anon_index);
			ASSERT(ap != NULL);
			swap_xlate(ap, &vp, &off);
			pp = page_lookup(vp, off, SE_SHARED);
			ASSERT(pp != NULL);
			*pplist++ = pp;
		}
		mutex_exit(&amp->lock);
	} else {
		/*
		 * We already have a valid ppa[].
		 */
		pl = spt_sd->ppa;
	}


	/*
	 * In either case, we increment softlockcnt on the 'real' segment.
	 *
	 * softlockcnt is just the number of outstanding lock operations
	 * on the segment rather than the number of pages.
	 */
	atomic_add_long((ulong_t *)(&(ssd->softlockcnt)), 1);

	ASSERT(pl != NULL);
	if (pl_built) {
		ASSERT(spt_sd->ppa == NULL);
		ASSERT(spt_sd->pcachecnt == 0);
		spt_sd->ppa = pl;
	}
	/* pcachecnt is protected by spt_sd->lock */
	spt_sd->pcachecnt++;

	ret = seg_pinsert(seg, seg->s_base, ptob(tot_pages),
		pl, spt_sd->prot, SEGP_FORCE_WIRED, segspt_reclaim);
	if (ret == SEGP_FAIL) {
		/*
		 * seg_pinsert failed. We need to decrement the
		 * pcachcnt and return failure. We return
		 * ENOTSUP, so that the as_pagelock() code will
		 * then try the slower F_SOFTLOCK path.
		 */
		spt_sd->pcachecnt--;
		if (pl_built) {
			/*
			 * No one else has referenced the ppa[].
			 * We created it and we need to destroy it.
			 */
			spt_sd->ppa = NULL;
		} else {
			/*
			 * Someone else is using this ppa[].
			 */
			pl_built = 0;
		}
		ret = ENOTSUP;
		goto insert_fail;
	}

	/*
	 * We can now drop the spt_sd->lock since the ppa[]
	 * exists and he have incremented pacachecnt.
	 */
	mutex_exit(&spt_sd->lock);

	/*
	 * Since we cache the entire segment, we want to
	 * set ppp to point to the first slot that corresponds
	 * to the requested addr, i.e. page_index.
	 */
	*ppp = &(spt_sd->ppa[page_index]);

	return (ret);


insert_fail:
	/*
	 * We will only reach this code if we tried and failed.
	 *
	 * And we can drop the lock on the dummy seg, once we've failed
	 * to set up a new ppa[].
	 */
	mutex_exit(&spt_sd->lock);

	if (pl_built) {
		/*
		 * We created pl and we need to destroy it.
		 */
		pplist = pl;
		np = tot_pages;
		while (np) {
			page_unlock(*pplist);
			np--;
			pplist++;
		}
		kmem_free(pl, sizeof (page_t *) * tot_pages);
		segplckstat.active_pages.value.ul -= tot_pages;
	}
	atomic_add_long((ulong_t *)(&(ssd->softlockcnt)), -1);
	if (ssd->softlockcnt <= 0) {
		if (AS_ISUNMAPWAIT(seg->s_as)) {
			mutex_enter(&seg->s_as->a_contents);
			if (AS_ISUNMAPWAIT(seg->s_as)) {
				AS_CLRUNMAPWAIT(seg->s_as);
				cv_broadcast(&seg->s_as->a_cv);
			}
			mutex_exit(&seg->s_as->a_contents);
		}
	}
	*ppp = NULL;
	return (ret);
}

/*
 * purge any cached pages in the I/O page cache
 */
static void
segspt_purge(struct seg *seg)
{
	seg_ppurge(seg);
}

static void
segspt_reclaim(struct seg *seg, caddr_t addr, size_t len, struct page **pplist,
	enum seg_rw rw)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	struct seg	*sptseg;
	struct spt_data *spt_sd;
	pgcnt_t np, npages;
	struct page **pl;

#ifdef lint
	addr = addr;
#endif

	sptseg = ssd->sptseg;
	spt_sd = sptseg->s_data;
	npages = np = (len >> PAGESHIFT);
	ASSERT(npages);
	pl = pplist;
	ASSERT(pl != NULL);
	ASSERT(pl == spt_sd->ppa);
	ASSERT(npages == btopr(spt_sd->amp->size));
	ASSERT(spt_sd->pcachecnt != 0);

	/*
	 * Acquire the lock on the dummy seg and destroy the
	 * ppa array IF this is the last pcachecnt.
	 */
	mutex_enter(&spt_sd->lock);
	if (--spt_sd->pcachecnt == 0) {

		while (np > (u_int)0) {
			if (rw == S_WRITE) {
				hat_setrefmod(*pplist);
			} else {
				hat_setref(*pplist);
			}
			page_unlock(*pplist);
			np--;
			pplist++;
		}
		/*
		 * Since we want to cach/uncache the entire ISM segment,
		 * we will track the pplist in a segspt specific field
		 * ppa, that is initialized at the time we add an entry to
		 * the cache.
		 */
		ASSERT(spt_sd->pcachecnt == 0);
		kmem_free(pl, sizeof (page_t *) * npages);
		spt_sd->ppa = NULL;
		segplckstat.active_pages.value.ul -= npages;
	}
	mutex_exit(&spt_sd->lock);

	/*
	 * Now decrement softlockcnt.
	 */
	atomic_add_long((ulong_t *)(&(ssd->softlockcnt)), -1);

	if (ssd->softlockcnt <= 0) {
		if (AS_ISUNMAPWAIT(seg->s_as)) {
			mutex_enter(&seg->s_as->a_contents);
			if (AS_ISUNMAPWAIT(seg->s_as)) {
				AS_CLRUNMAPWAIT(seg->s_as);
				cv_broadcast(&seg->s_as->a_cv);
			}
			mutex_exit(&seg->s_as->a_contents);
		}
	}
}

/*
 * Do a F_SOFTUNLOCK call over the range requested.
 * The range must have already been F_SOFTLOCK'ed.
 */
static void
segspt_softunlock(struct seg *seg, caddr_t sptseg_addr,
	size_t len, enum seg_rw rw)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	struct seg	*sptseg;
	struct spt_data *spt_sd;
	page_t *pp;
	caddr_t adr;
	struct vnode *vp;
	u_offset_t offset;
	u_long anon_index;
	struct anon_map *amp;		/* XXX - for locknest */
	struct anon *ap = NULL;
	pgcnt_t npages;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Some platforms assume that ISM mappings are HAT_LOAD_LOCK
	 * and therefore their pages are SE_SHARED locked
	 * for the entire life of the segment.
	 */
	if (!hat_supported(HAT_DYNAMIC_ISM_UNMAP, (void *)0))
		goto softlock_decrement;

	sptseg = ssd->sptseg;
	spt_sd = sptseg->s_data;

	/*
	 * Any thread is free to do a page_find and
	 * page_unlock() on the pages within this seg.
	 *
	 * We are already holding the as->a_lock on the user's
	 * real segment, but we need to hold the a_lock on the
	 * underlying dummy as. This is mostly to satisfy the
	 * underlying HAT layer.
	 */
	AS_LOCK_ENTER(sptseg->s_as, &sptseg->s_as->a_lock, RW_READER);
	hat_unlock(sptseg->s_as->a_hat, sptseg_addr, len);
	AS_LOCK_EXIT(sptseg->s_as, &sptseg->s_as->a_lock);

	amp = spt_sd->amp;
	ASSERT(amp != NULL);
	anon_index = seg_page(sptseg, sptseg_addr);

	for (adr = sptseg_addr; adr < sptseg_addr + len; adr += PAGESIZE) {
		mutex_enter(&amp->lock);
		ap = anon_get_ptr(amp->ahp, anon_index++);
		ASSERT(ap != NULL);
		swap_xlate(ap, &vp, &offset);
		mutex_exit(&amp->lock);

		/*
		 * Use page_find() instead of page_lookup() to
		 * find the page since we know that it has a
		 * "shared" lock.
		 */
		pp = page_find(vp, offset);
		if (pp == NULL) {
			cmn_err(CE_PANIC,
			"segspt_softunlock: addr %p, ap %p, vp %p, off %llx",
				(void *)adr, (void *)ap, (void *)vp, offset);
		}

		if (rw == S_WRITE) {
			hat_setrefmod(pp);
		} else if (rw != S_OTHER) {
			hat_setref(pp);
		}
		page_unlock(pp);
	}

softlock_decrement:
	npages = btopr(len);
	atomic_add_long((ulong_t *)(&(ssd->softlockcnt)), -npages);
	if (ssd->softlockcnt == 0) {
		/*
		 * All SOFTLOCKS are gone. Wakeup any waiting
		 * unmappers so they can try again to unmap.
		 * Check for waiters first without the mutex
		 * held so we don't always grab the mutex on
		 * softunlocks.
		 */
		if (AS_ISUNMAPWAIT(seg->s_as)) {
			mutex_enter(&seg->s_as->a_contents);
			if (AS_ISUNMAPWAIT(seg->s_as)) {
				AS_CLRUNMAPWAIT(seg->s_as);
				cv_broadcast(&seg->s_as->a_cv);
			}
			mutex_exit(&seg->s_as->a_contents);
		}
	}
}

int
segspt_shmattach(struct seg *seg, caddr_t *argsp)
{
	struct sptshm_data *sptarg = (struct sptshm_data *)argsp;
	struct sptshm_data *ssd;
	struct anon_map *shm_amp = sptarg->amp;
	int error;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	ssd = kmem_zalloc((sizeof (*ssd)), KM_NOSLEEP);
	if (ssd == NULL)
		return (ENOMEM);

	ssd->sptas = sptarg->sptas;
	ssd->amp = shm_amp;
	ssd->sptseg = sptarg->sptseg;
	seg->s_data = (void *)ssd;
	seg->s_ops = &segspt_shmops;
	mutex_enter(&shm_amp->lock);
	shm_amp->refcnt++;
	mutex_exit(&shm_amp->lock);

	error = hat_share(seg->s_as->a_hat, seg->s_base,
	    sptarg->sptas->a_hat, SEGSPTADDR, seg->s_size);

	return (error);
}

int
segspt_shmunmap(struct seg *seg, caddr_t raddr, size_t ssize)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	int reclaim = 1;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

retry:
	if (ssd->softlockcnt > 0) {
		if (reclaim == 1) {
			segspt_purge(seg);
			reclaim = 0;
			goto retry;
		}
		return (EAGAIN);
	}

	if (ssize != seg->s_size)
		return (EINVAL);

	hat_unshare(seg->s_as->a_hat, raddr, ssize);
	seg_free(seg);
	return (0);
}

void
segspt_shmfree(struct seg *seg)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	struct anon_map *shm_amp = ssd->amp;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Need to increment refcnt when attaching
	 * and decrement when detaching because of dup().
	 */
	mutex_enter(&shm_amp->lock);
	shm_amp->refcnt--;
	mutex_exit(&shm_amp->lock);

	kmem_free(ssd, sizeof (*ssd));
}

/* ARGSUSED */
int
segspt_shmsetprot(struct seg *seg, caddr_t addr, size_t len, u_int prot)
{

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Shared page table is more than shared mapping.
	 *  Individual process sharing page tables can't change prot
	 *  because there is only one set of page tables.
	 *  This will be allowed after private page table is
	 *  supported.
	 */
/* need to return correct status error? */
	return (0);
}

faultcode_t
segspt_shmfault(struct hat *hat, struct seg *seg, caddr_t addr,
    size_t len, enum fault_type type, enum seg_rw rw)
{
	struct seg		*sptseg;
	struct sptshm_data 	*ssd;
	struct spt_data 	*spt_sd;
	struct as		*curspt;
	pgcnt_t npages;
	size_t share_size, size;
	caddr_t sptseg_addr;
	page_t *pp, **ppa;
	int	i;
	struct vnode *vp;
	u_offset_t offset;
	u_long anon_index = 0;
	struct anon_map *amp;		/* XXX - for locknest */
	struct anon *ap = NULL;


#ifdef lint
	hat = hat;
#endif

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	ssd = (struct sptshm_data *)seg->s_data;
	curspt = ssd->sptas;
	sptseg = ssd->sptseg;
	spt_sd = sptseg->s_data;

	/*
	 * Because of the way spt is implemented
	 * the realsize of the segment does not have to be
	 * equal to the segment size itself. The segment size is
	 * often in multiples of a page size larger than PAGESIZE.
	 * The realsize is rounded up to the nearest PAGESIZE
	 * based on what the user requested. This is a bit of
	 * ungliness that is historical but not easily fixed
	 * without re-designing the higher levels of ISM.
	 */
	ASSERT(addr >= seg->s_base);
	if (((addr + len) - seg->s_base) > spt_sd->realsize)
		return (FC_NOMAP);

	/*
	 * For all of the following cases except F_PROT, we need to
	 * make any necessary adjustments to addr and len
	 * and get all of the necessary page_t's into an array called ppa[].
	 *
	 * The code in shmat() forces base addr and len of ISM segment
	 * to be aligned to largest page size supported. Therefore,
	 * we are able to handle F_SOFTLOCK and F_INVAL calls in "large
	 * pagesize" chunks. We want to make sure that we HAT_LOAD_LOCK
	 * in large pagesize chunks, or else we will screw up the HAT
	 * layer by calling hat_memload_array() with differing page sizes
	 * over a given virtual range.
	 */

	share_size = page_get_pagesize(page_num_pagesizes() - 1);
	sptseg_addr = (caddr_t)((uintptr_t)addr & (~(share_size - 1)));
	size = (addr + len) - sptseg_addr;
	size = roundup(size, share_size);
	npages = btopr(size);

	/*
	 * Now we need to convert from addr in segshm to addr in segspt.
	 */
	anon_index = seg_page(seg, sptseg_addr);
	sptseg_addr = sptseg->s_base + (anon_index * PAGESIZE);
	/*
	 * And now we may have to adjust npages downward if we have
	 * exceeded the realsize of the segment.
	 */
	if ((sptseg_addr + ptob(npages)) > (sptseg->s_base + spt_sd->realsize))
		size = (sptseg->s_base + spt_sd->realsize) - sptseg_addr;

	npages = btopr(size);
	ASSERT(sptseg_addr < (sptseg->s_base + sptseg->s_size));

	switch (type) {

	case F_SOFTLOCK:

		/*
		 * availrmem is decremented once during anon_swap_adjust()
		 * and is incremented during the anon_unresv(), which is
		 * called from shm_rm_amp() when the segment is destroyed.
		 */
		atomic_add_long((ulong_t *)(&(ssd->softlockcnt)), npages);
		/*
		 * Some platforms assume that ISM pages are SE_SHARED
		 * locked for the entire life of the segment.
		 */
		if (!hat_supported(HAT_DYNAMIC_ISM_UNMAP, (void *)0))
			return (0);

		/*
		 * Fall through to the F_INVAL case to load up the hat layer
		 * entries with the HAT_LOAD_LOCK flag.
		 */

		/* FALLTHRU */
	case F_INVAL:
		if ((rw == S_EXEC) && !(spt_sd->prot & PROT_EXEC))
			return (FC_NOMAP);

		/*
		 * Some platforms that do NOT support DYNAMIC_ISM_UNMAP
		 * may still rely on this call to hat_share(). That
		 * would imply that those hat's can fault on a
		 * HAT_LOAD_LOCK translation, which would seem
		 * contradictory.
		 */
		if (!hat_supported(HAT_DYNAMIC_ISM_UNMAP, (void *)0)) {
			if (hat_share(seg->s_as->a_hat, seg->s_base,
				curspt->a_hat, sptseg->s_base,
				sptseg->s_size) != 0) {
					cmn_err(CE_PANIC,
					"hat_share error in ISM fault");
			}
			return (0);
		}

		ppa = kmem_zalloc(sizeof (page_t *) * npages, KM_SLEEP);
		/*
		 * I see no need to lock the real seg,
		 * here, because all of our work will be on the underlying
		 * dummy seg.
		 *
		 * sptseg_addr and npages now account for large pages.
		 */
		amp = spt_sd->amp;
		ASSERT(amp != NULL);
		anon_index = seg_page(sptseg, sptseg_addr);
		mutex_enter(&amp->lock);
		for (i = 0; i < npages; i++) {
			ap = anon_get_ptr(amp->ahp, anon_index++);
			ASSERT(ap != NULL);
			swap_xlate(ap, &vp, &offset);
			pp = page_lookup(vp, offset, SE_SHARED);
			ASSERT(pp != NULL);
			ppa[i] = pp;
		}
		mutex_exit(&amp->lock);

		ASSERT(i == npages);

		/*
		 * We are already holding the as->a_lock on the user's
		 * real segment, but we need to hold the a_lock on the
		 * underlying dummy as. This is mostly to satisfy the
		 * underlying HAT layer.
		 */
		AS_LOCK_ENTER(sptseg->s_as, &sptseg->s_as->a_lock, RW_READER);
		if (type == F_SOFTLOCK) {
			/*
			 * Load up the translation keeping it
			 * locked and don't unlock the page.
			 */
			hat_memload_array(sptseg->s_as->a_hat, sptseg_addr,
				ptob(npages), ppa, spt_sd->prot,
				HAT_LOAD_LOCK | HAT_LOAD_SHARE |
				HAT_RELOAD_SHARE);
		} else {
			hat_memload_array(sptseg->s_as->a_hat, sptseg_addr,
				ptob(npages), ppa, spt_sd->prot,
				HAT_LOAD_SHARE | HAT_RELOAD_SHARE);
			/*
			 * And now drop the SE_SHARED lock(s).
			 */
			for (i = 0; i < npages; i++)
				page_unlock(ppa[i]);
		}
		AS_LOCK_EXIT(sptseg->s_as, &sptseg->s_as->a_lock);

		kmem_free(ppa, sizeof (page_t *) * npages);
		return (0);

	case F_SOFTUNLOCK:

		/*
		 * This is a bit ugly, we pass in the real seg pointer,
		 * but the sptseg_addr is the virtual address within the
		 * dummy seg.
		 */
		segspt_softunlock(seg, sptseg_addr, ptob(npages), rw);
		return (0);

	case F_PROT:
		/*
		 * This takes care of the unusual case where a user
		 * allocates a stack in shared memory and a register
		 * window overflow is written to that stack page before
		 * it is otherwise modified.
		 *
		 * We can get away with this because ISM segments are
		 * always rw. Other than this unusual case, there
		 * should be no instances of protection violations.
		 */
		return (0);

	default:
#ifdef DEBUG
		cmn_err(CE_WARN, "segspt_shmfault default type?");
#endif
		return (FC_NOMAP);
	}
}

/*ARGSUSED*/
static faultcode_t
segspt_shmfaulta(struct seg *seg, caddr_t addr)
{
	return (0);
}

/*ARGSUSED*/
static int
segspt_shmkluster(struct seg *seg, caddr_t addr, ssize_t delta)
{
	return (0);
}

/*ARGSUSED*/
static size_t
segspt_shmswapout(struct seg *seg)
{
	return (0);
}

/*
 * duplicate the shared page tables
 */
int
segspt_shmdup(struct seg *seg, struct seg *newseg)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	struct anon_map *amp = ssd->amp;
	struct sptshm_data *nsd;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	nsd = (struct sptshm_data *)kmem_zalloc((sizeof (*nsd)), KM_SLEEP);
	newseg->s_data = (void *) nsd;
	nsd->sptas = ssd->sptas;
	nsd->amp = amp;
	nsd->sptseg = ssd->sptseg;
	newseg->s_ops = &segspt_shmops;

	mutex_enter(&amp->lock);
	amp->refcnt++;
	mutex_exit(&amp->lock);

	return (hat_share(newseg->s_as->a_hat, newseg->s_base,
	    ssd->sptas->a_hat, SEGSPTADDR, seg->s_size));
}

/* ARGSUSED */
int
segspt_shmcheckprot(struct seg *seg, caddr_t addr, size_t size, u_int prot)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	struct spt_data *spt = (struct spt_data *)ssd->sptseg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * ISM segment is always rw.
	 */
	return (((spt->prot & prot) != prot) ? EACCES : 0);
}

/* ARGSUSED */
static int
segspt_shmlockop(struct seg *seg, caddr_t addr, size_t len,
    int attr, int op, ulong *lockmap, size_t pos)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/* ISM pages are always locked. */
	return (0);
}

/* ARGSUSED */
int
segspt_shmgetprot(struct seg *seg, caddr_t addr, size_t len, u_int *protv)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	struct spt_data *spt = (struct spt_data *)ssd->sptseg->s_data;
	spgcnt_t pgno = seg_page(seg, addr+len) - seg_page(seg, addr) + 1;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * ISM segment is always rw.
	 */
	while (--pgno >= 0)
		*protv++ = spt->prot;
	return (0);
}

/* ARGSUSED */
u_offset_t
segspt_shmgetoffset(struct seg *seg, caddr_t addr)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/* Offset does not matter in ISM memory */

	return ((u_offset_t)0);
}

/* ARGSUSED */
int
segspt_shmgettype(struct seg *seg, caddr_t addr)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/* The shared memory mapping is always MAP_SHARED */

	return (MAP_SHARED);
}

/* ARGSUSED */
int
segspt_shmgetvp(struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	struct spt_data *spt = (struct spt_data *)ssd->sptseg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	*vpp = spt->vp;
	return (0);

}

/* ARGSUSED */
static int
segspt_shmadvice(struct seg *seg, caddr_t addr, size_t len, u_int behav)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (0);
}

/* ARGSUSED */
void
segspt_shmdump(struct seg *seg)
{
	/* no-op for ISM segment */
}

/* ARGSUSED */
int
segspt_kluster(struct seg *seg, caddr_t addr, ssize_t delta)
{
	return (0);
}

/*
 * get a memory ID for an addr in a given segment
 */
static int
segspt_shmgetmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	struct sptshm_data *sptshm = (struct sptshm_data *)seg->s_data;
	struct anon 	*ap;
	size_t		anon_index;
	struct anon_map	*amp = sptshm->amp;
	struct spt_data	*sptdat = sptshm->sptseg->s_data;

	anon_index = seg_page(seg, addr);

	if (addr > (seg->s_base + sptdat->realsize)) {
		return (EFAULT);
	}

	ap = anon_get_ptr(amp->ahp, anon_index);
	memidp->val[0] = (u_longlong_t)ap;
	memidp->val[1] = (u_longlong_t)((uintptr_t)addr & PAGEOFFSET);
	return (0);
}
