/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

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
 * 	(c) 1986, 1987, 1988, 1989, 1990, 1995, 1996  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)seg_vn.c	1.198	99/12/04 SMI"

/*
 * VM - shared or copy-on-write from a vnode/anonymous memory.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/debug.h>
#include <sys/cred.h>
#include <sys/vmsystm.h>
#include <sys/tuneable.h>
#include <sys/bitmap.h>
#include <sys/swap.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/vtrace.h>
#include <sys/cmn_err.h>
#include <sys/vm.h>
#include <sys/dumphdr.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <vm/pvn.h>
#include <vm/anon.h>
#include <vm/page.h>
#include <vm/vpage.h>

/*
 * Private seg op routines.
 */
static int	segvn_dup(struct seg *seg, struct seg *newseg);
static int	segvn_unmap(struct seg *seg, caddr_t addr, size_t len);
static void	segvn_free(struct seg *seg);
static faultcode_t segvn_fault(struct hat *hat, struct seg *seg,
		    caddr_t addr, size_t len, enum fault_type type,
		    enum seg_rw rw);
static faultcode_t segvn_faulta(struct seg *seg, caddr_t addr);
static int	segvn_setprot(struct seg *seg, caddr_t addr,
		    size_t len, uint_t prot);
static int	segvn_checkprot(struct seg *seg, caddr_t addr,
		    size_t len, uint_t prot);
static int	segvn_kluster(struct seg *seg, caddr_t addr, ssize_t delta);
static size_t	segvn_swapout(struct seg *seg);
static int	segvn_sync(struct seg *seg, caddr_t addr, size_t len,
		    int attr, uint_t flags);
static size_t	segvn_incore(struct seg *seg, caddr_t addr, size_t len,
		    char *vec);
static int	segvn_lockop(struct seg *seg, caddr_t addr, size_t len,
		    int attr, int op, ulong_t *lockmap, size_t pos);
static int	segvn_getprot(struct seg *seg, caddr_t addr, size_t len,
		    uint_t *protv);
static u_offset_t	segvn_getoffset(struct seg *seg, caddr_t addr);
static int	segvn_gettype(struct seg *seg, caddr_t addr);
static int	segvn_getvp(struct seg *seg, caddr_t addr,
		    struct vnode **vpp);
static int	segvn_advise(struct seg *seg, caddr_t addr, size_t len,
		    uint_t behav);
static void	segvn_dump(struct seg *seg);
static int	segvn_pagelock(struct seg *seg, caddr_t addr, size_t len,
		    struct page ***ppp, enum lock_type type, enum seg_rw rw);
static int	segvn_getmemid(struct seg *seg, caddr_t addr,
		    memid_t *memidp);

struct	seg_ops segvn_ops = {
	segvn_dup,
	segvn_unmap,
	segvn_free,
	segvn_fault,
	segvn_faulta,
	segvn_setprot,
	segvn_checkprot,
	segvn_kluster,
	segvn_swapout,
	segvn_sync,
	segvn_incore,
	segvn_lockop,
	segvn_getprot,
	segvn_getoffset,
	segvn_gettype,
	segvn_getvp,
	segvn_advise,
	segvn_dump,
	segvn_pagelock,
	segvn_getmemid,
};

#define	ZFOD(prot, max)	{ NULL, NULL, 0, MAP_PRIVATE, prot, max, 0, NULL }

/*
 * Common zfod structures, provided as a shorthand for others to use.
 */
static segvn_crargs_t zfod_segvn_crargs = ZFOD(PROT_ZFOD, PROT_ALL);
static segvn_crargs_t kzfod_segvn_crargs = ZFOD(PROT_ZFOD & ~PROT_USER,
	PROT_ALL & ~PROT_USER);
static segvn_crargs_t stack_noexec_crargs = ZFOD(PROT_ZFOD & ~PROT_EXEC,
	PROT_ALL);

caddr_t	zfod_argsp = (caddr_t)&zfod_segvn_crargs;	/* user zfod argsp */
caddr_t	kzfod_argsp = (caddr_t)&kzfod_segvn_crargs;	/* kernel zfod argsp */
caddr_t	stack_exec_argsp = (caddr_t)&zfod_segvn_crargs;	/* executable stack */
caddr_t	stack_noexec_argsp = (caddr_t)&stack_noexec_crargs; /* noexec stack */

#define	vpgtob(n)	((n) * sizeof (struct vpage))	/* For brevity */

static	uint_t anon_slop = 64*1024;	/* allow segs to expand in place */
size_t	segvn_comb_thrshld = UINT_MAX;	/* patchable -- see 1196681 */

static int	segvn_concat(struct seg *, struct seg *);
static int	segvn_extend_prev(struct seg *, struct seg *,
		    struct segvn_crargs *, size_t);
static int	segvn_extend_next(struct seg *, struct seg *,
		    struct segvn_crargs *, size_t);
static void	segvn_softunlock(struct seg *, caddr_t, size_t, enum seg_rw);
static void	segvn_pagelist_rele(page_t **);
static int	segvn_faultpage(struct hat *, struct seg *, caddr_t,
    u_offset_t, struct vpage *, page_t **, uint_t,
    enum fault_type, enum seg_rw);
static void	segvn_vpage(struct seg *);

static void segvn_purge(struct seg *seg);
static void segvn_reclaim(struct seg *, caddr_t, size_t, struct page **,
    enum seg_rw);

static struct kmem_cache *segvn_cache;

/*ARGSUSED*/
static int
segvn_cache_constructor(void *buf, void *cdrarg, int kmflags)
{
	struct segvn_data *svd = buf;

	rw_init(&svd->lock, NULL, RW_DEFAULT, NULL);
	return (0);
}

/*ARGSUSED1*/
static void
segvn_cache_destructor(void *buf, void *cdrarg)
{
	struct segvn_data *svd = buf;

	rw_destroy(&svd->lock);
}

/*
 * Patching this variable to non-zero allows the system to run with
 * stacks marked as "not executable".  It's a bit of a kludge, but is
 * provided as a tweakable for platforms that export those ABIs
 * (e.g. sparc V8) that have executable stacks enabled by default.
 * There are also some restrictions for platforms that don't actually
 * implement 'noexec' protections.
 *
 * Once enabled, the system is (therefore) unable to provide a fully
 * ABI-compliant execution environment, though practically speaking,
 * most everything works.  The exceptions are generally some interpreters
 * and debuggers that create executable code on the stack and jump
 * into it (without explicitly mprotecting the address range to include
 * PROT_EXEC).
 *
 * One important class of applications that are disabled are those
 * that have been transformed into malicious agents using one of the
 * numerous "buffer overflow" attacks.  See 4007890.
 */
int noexec_user_stack = 0;
int noexec_user_stack_log = 1;

/*
 * Initialize segvn data structures
 */
void
segvn_init(void)
{
	segvn_cache = kmem_cache_create("segvn_cache",
		sizeof (struct segvn_data), 0,
		segvn_cache_constructor, segvn_cache_destructor, NULL,
		NULL, NULL, 0);
}

int
segvn_create(struct seg *seg, void *argsp)
{
	struct segvn_crargs *a = (struct segvn_crargs *)argsp;
	struct segvn_data *svd;
	size_t swresv = 0;
	struct cred *cred;
	struct anon_map *amp;
	int error = 0;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	if (a->type != MAP_PRIVATE && a->type != MAP_SHARED)
		cmn_err(CE_PANIC, "segvn_create type");

	/*
	 * Check arguments.  If a shared anon structure is given then
	 * it is illegal to also specify a vp.
	 */
	if (a->amp != NULL && a->vp != NULL)
		cmn_err(CE_PANIC, "segvn_create anon_map");

	/* MAP_NORESERVE on a MAP_SHARED segment is meaningless. */
	if (a->type == MAP_SHARED)
		a->flags &= ~MAP_NORESERVE;

	/*
	 * If segment may need private pages, reserve them now.
	 */
	if (!(a->flags & MAP_NORESERVE) && ((a->vp == NULL && a->amp == NULL) ||
	    (a->type == MAP_PRIVATE && (a->prot & PROT_WRITE)))) {
		if (anon_resv(seg->s_size) == 0)
			return (EAGAIN);
		swresv = seg->s_size;
		TRACE_5(TR_FAC_VM, TR_ANON_PROC, "anon proc:%u %p %lu %lu %u",
			curproc->p_pid, seg, seg->s_base, swresv, 1);
	}

	/*
	 * Reserve any mapping structures that may be required.
	 */
	hat_map(seg->s_as->a_hat, seg->s_base, seg->s_size, HAT_MAP);

	if (a->cred) {
		cred = a->cred;
		crhold(cred);
	} else {
		crhold(cred = CRED());
	}

	/* Inform the vnode of the new mapping */
	if (a->vp) {
		error = VOP_ADDMAP(a->vp, a->offset & PAGEMASK,
		    seg->s_as, seg->s_base, seg->s_size, a->prot,
		    a->maxprot, a->type, cred);
		if (error) {
			if (swresv != 0) {
				anon_unresv(swresv);
				TRACE_5(TR_FAC_VM, TR_ANON_PROC,
					"anon proc:%u %p %lu %lu %u",
					curproc->p_pid, seg, seg->s_base,
					swresv, 0);
			}
			crfree(cred);
			hat_unload(seg->s_as->a_hat, seg->s_base,
				seg->s_size, HAT_UNLOAD_UNMAP);
			return (error);
		}
	}

	/*
	 * If more than one segment in the address space, and
	 * they're adjacent virtually, try to concatenate them.
	 * Don't concatenate if an explicit anon_map structure
	 * was supplied (e.g., SystemV shared memory).
	 */
	if (a->amp == NULL) {
		struct seg *pseg, *nseg;

		/* first, try to concatenate the previous and new segments */
		pseg = seg->s_prev;
		if (pseg != NULL &&
		    pseg->s_base + pseg->s_size == seg->s_base &&
		    pseg->s_ops == &segvn_ops &&
		    (pseg->s_size + seg->s_size <= segvn_comb_thrshld ||
			((struct segvn_data *)pseg->s_data)->amp == NULL) &&
		    segvn_extend_prev(pseg, seg, a, swresv) == 0) {
			/* success! now try to concatenate with following seg */
			crfree(cred);
			nseg = AS_SEGP(pseg->s_as, pseg->s_next);
			if (nseg != NULL &&
			    nseg != pseg && nseg->s_ops == &segvn_ops &&
			    pseg->s_base + pseg->s_size == nseg->s_base)
				(void) segvn_concat(pseg, nseg);
			return (0);
		}
		/* failed, so try to concatenate with following seg */
		nseg = AS_SEGP(seg->s_as, seg->s_next);
		if (nseg != NULL &&
		    seg->s_base + seg->s_size == nseg->s_base &&
		    nseg->s_ops == &segvn_ops &&
		    segvn_extend_next(seg, nseg, a, swresv) == 0) {
			crfree(cred);
			return (0);
		}
	}

	if (a->vp != NULL)
		VN_HOLD(a->vp);
	svd = kmem_cache_alloc(segvn_cache, KM_SLEEP);

	seg->s_ops = &segvn_ops;
	seg->s_data = (void *)svd;

	svd->vp = a->vp;
	svd->offset = a->offset & PAGEMASK;
	svd->prot = a->prot;
	svd->maxprot = a->maxprot;
	svd->pageprot = 0;
	svd->type = a->type;
	svd->vpage = NULL;
	svd->cred = cred;
	svd->advice = MADV_NORMAL;
	svd->pageadvice = 0;
	svd->flags = (uchar_t)a->flags;
	svd->softlockcnt = 0;

	amp = a->amp;		/* XXX - for locknest */
	if ((svd->amp = amp) == NULL) {
		svd->anon_index = 0;
		if (svd->type == MAP_SHARED) {
			svd->swresv = 0;
			/*
			 * Shared mappings to a vp need no other setup.
			 * If we have a shared mapping to an anon_map object
			 * which hasn't been allocated yet,  allocate the
			 * struct now so that it will be properly shared
			 * by remembering the swap reservation there.
			 */
			if (a->vp == NULL)
				svd->amp = anonmap_alloc(seg->s_size, swresv);
		} else {
			/*
			 * Private mapping (with or without a vp).
			 * Allocate anon_map when needed.
			 */
			svd->swresv = swresv;
		}
	} else {
		pgcnt_t anon_num;

		/*
		 * Mapping to an existing anon_map structure without a vp.
		 * For now we will insure that the segment size isn't larger
		 * than the size - offset gives us.  Later on we may wish to
		 * have the anon array dynamically allocated itself so that
		 * we don't always have to allocate all the anon pointer slots.
		 * This of course involves adding extra code to check that we
		 * aren't trying to use an anon pointer slot beyond the end
		 * of the currently allocated anon array.
		 */
		if ((amp->size - a->offset) < seg->s_size)
			cmn_err(CE_PANIC, "segvn_create anon_map size");

		anon_num = btopr(a->offset);

		if (a->type == MAP_SHARED) {
			/*
			 * SHARED mapping to a given anon_map.
			 */
			mutex_enter(&amp->lock);
			amp->refcnt++;
			mutex_exit(&amp->lock);
			svd->anon_index = anon_num;
			svd->swresv = 0;
		} else {
			/*
			 * PRIVATE mapping to a given anon_map.
			 * Make sure that all the needed anon
			 * structures are created (so that we will
			 * share the underlying pages if nothing
			 * is written by this mapping) and then
			 * duplicate the anon array as is done
			 * when a privately mapped segment is dup'ed.
			 */
			struct anon *ap;
			caddr_t addr;
			caddr_t eaddr;
			ulong_t	anon_idx;

			svd->amp = anonmap_alloc(seg->s_size, 0);
			svd->anon_index = 0;
			svd->swresv = swresv;

			/*
			 * Acquire the "serialization" lock before
			 * the anon_map lock to prevent 2 threads
			 * from allocating anon slots simultaneously.
			 */
			mutex_enter(&amp->serial_lock);
			eaddr = seg->s_base + seg->s_size;

			for (anon_idx = anon_num, addr = seg->s_base;
			    addr < eaddr; addr += PAGESIZE, anon_idx++) {
				page_t *pp;

				if ((ap = anon_get_ptr(amp->ahp,
							anon_idx)) != NULL)
					continue;
				/*
				 * Allocate the anon struct now.
				 * Might as well load up translation
				 * to the page while we're at it...
				 */
				pp = anon_zero(seg, addr, &ap, cred);
				if (ap == NULL || pp == NULL)
					cmn_err(CE_PANIC,
						"segvn_create anon_zero");

				/*
				 * Re-acquire the anon_map lock and
				 * initialize the anon array entry.
				 */
				mutex_enter(&amp->lock);
				ASSERT(anon_get_ptr(amp->ahp,
							anon_idx) == NULL);
				(void) anon_set_ptr(amp->ahp, anon_idx, ap,
					ANON_SLEEP);
				mutex_exit(&amp->lock);

				hat_memload(seg->s_as->a_hat, addr, pp,
				    svd->prot & ~PROT_WRITE, HAT_LOAD);
				page_unlock(pp);
			}
			mutex_enter(&amp->lock);
			anon_dup(amp->ahp, anon_num, svd->amp->ahp,
						0, seg->s_size);
			mutex_exit(&amp->lock);
			mutex_exit(&amp->serial_lock);
		}
	}
	return (0);
}

/*
 * Concatenate two existing segments, if possible.
 * Return 0 on success.
 */
static int
segvn_concat(struct seg *seg1, struct seg *seg2)
{
	struct segvn_data *svd1, *svd2;
	size_t size;
	struct anon_map *amp1, *amp2;
	struct vpage *vpage1, *vpage2;

	ASSERT(seg1->s_as && seg2->s_as && seg1->s_as == seg2->s_as);
	ASSERT(AS_WRITE_HELD(seg1->s_as, &seg1->s_as->a_lock));

	svd1 = (struct segvn_data *)seg1->s_data;
	svd2 = (struct segvn_data *)seg2->s_data;

	/* both segments exist, try to merge them */
#define	incompat(x)	(svd1->x != svd2->x)
	if (incompat(vp) || incompat(maxprot) ||
	    (!svd1->pageadvice && !svd2->pageadvice && incompat(advice)) ||
	    (!svd1->pageprot && !svd2->pageprot && incompat(prot)) ||
	    incompat(type) || incompat(cred) || incompat(flags))
		return (-1);
#undef incompat

	/* vp == NULL implies zfod, offset doesn't matter */
	if (svd1->vp != NULL &&
	    svd1->offset + seg1->s_size != svd2->offset)
		return (-1);
	amp1 = svd1->amp;
	amp2 = svd2->amp;
	/* XXX - for now, reject if any private pages.  could merge. */
	if (amp1 != NULL || amp2 != NULL)
		return (-1);

	/* if either seg has vpages, create new merged vpages */
	vpage1 = svd1->vpage;
	vpage2 = svd2->vpage;
	if (vpage1 != NULL || vpage2 != NULL) {
		pgcnt_t npages1, npages2;
		struct vpage *vp, *new_vpage;

		npages1 = seg_pages(seg1);
		npages2 = seg_pages(seg2);
		new_vpage =
		    kmem_zalloc(vpgtob(npages1 + npages2), KM_NOSLEEP);
		if (new_vpage == NULL)
			return (-1);
		if (vpage1 != NULL)
			bcopy(vpage1, new_vpage, vpgtob(npages1));
		if (vpage2 != NULL)
			bcopy(vpage2, new_vpage + npages1,
			    vpgtob(npages2));

		for (vp = new_vpage; vp < new_vpage + npages1; vp++) {
			if (svd2->pageprot && !svd1->pageprot)
				VPP_SETPROT(vp, svd1->prot);
			if (svd2->pageadvice && !svd1->pageadvice)
				VPP_SETADVICE(vp, svd1->advice);
		}
		for (vp = new_vpage + npages1;
		    vp < new_vpage + npages1 + npages2; vp++) {
			if (svd1->pageprot && !svd2->pageprot)
				VPP_SETPROT(vp, svd2->prot);
			if (svd1->pageadvice && !svd2->pageadvice)
				VPP_SETADVICE(vp, svd2->advice);
		}

		/* Now free the old vpage structures */
		if (vpage1 != NULL)
			kmem_free(vpage1, vpgtob(npages1));

		if (vpage2 != NULL) {
			svd2->vpage = NULL;
			kmem_free(vpage2, vpgtob(npages2));
		}

		if (svd2->pageprot)
			svd1->pageprot = 1;
		if (svd2->pageadvice)
			svd1->pageadvice = 1;
		svd1->vpage = new_vpage;
	}

	/* all looks ok, merge second into first */
	svd1->swresv += svd2->swresv;
	svd2->swresv = 0;	/* so seg_free doesn't release swap space */
	size = seg2->s_size;
	seg_free(seg2);
	seg1->s_size += size;
	return (0);
}

/*
 * Extend the previous segment (seg1) to include the
 * new segment (seg2 + a), if possible.
 * Return 0 on success.
 */
static int
segvn_extend_prev(seg1, seg2, a, swresv)
	struct seg *seg1, *seg2;
	struct segvn_crargs *a;
	size_t swresv;
{
	struct segvn_data *svd1 = (struct segvn_data *)seg1->s_data;
	size_t size;
	struct anon_map *amp1;
	struct vpage *new_vpage;

	/*
	 * We don't need any segment level locks for "segvn" data
	 * since the address space is "write" locked.
	 */
	ASSERT(seg1->s_as && AS_WRITE_HELD(seg1->s_as, &seg1->s_as->a_lock));

	/* second segment is new, try to extend first */
	/* XXX - should also check cred */
	if (svd1->vp != a->vp || svd1->maxprot != a->maxprot ||
	    (!svd1->pageprot && (svd1->prot != a->prot)) ||
	    svd1->type != a->type || svd1->flags != a->flags)
		return (-1);

	/* vp == NULL implies zfod, offset doesn't matter */
	if (svd1->vp != NULL &&
	    svd1->offset + seg1->s_size != (a->offset & PAGEMASK))
		return (-1);

	amp1 = svd1->amp;
	if (amp1) {
		/*
		 * Segment has private pages, can data structures
		 * be expanded?
		 *
		 * Acquire the anon_map lock to prevent it from changing,
		 * if it is shared.  This ensures that the anon_map
		 * will not change while a thread which has a read/write
		 * lock on an address space references it.
		 * XXX - Don't need the anon_map lock at all if "refcnt"
		 * is 1.
		 *
		 * Can't grow a MAP_SHARED segment with an anonmap because
		 * there may be existing anon slots where we want to extend
		 * the segment and we wouldn't know what to do with them
		 * (e.g., for tmpfs right thing is to just leave them there,
		 * for /dev/zero they should be cleared out).
		 */
		mutex_enter(&amp1->lock);
		if (amp1->refcnt > 1 || svd1->type == MAP_SHARED) {
			mutex_exit(&amp1->lock);
			return (-1);
		}
		if (amp1->size - ptob(svd1->anon_index) <
		    seg1->s_size + seg2->s_size) {
			struct anon_hdr *nahp;
			size_t asize;

			/*
			 * We need a bigger anon array.  Allocate a new
			 * one with anon_slop worth of slop at the
			 * end so it will be easier to expand in
			 * place the next time we need to do this.
			 */
			asize = seg1->s_size + seg2->s_size + anon_slop;
			nahp = anon_create(btop(asize), ANON_NOSLEEP);
			if (nahp == NULL) {
				mutex_exit(&amp1->lock);
				return (-1);
			}
			if (anon_copy_ptr(amp1->ahp, svd1->anon_index,
			    nahp, 0, btop(seg1->s_size), ANON_NOSLEEP)
			    == ENOMEM) {
				anon_release(nahp, btop(asize));
				mutex_exit(&amp1->lock);
				return (-1);
			}
			anon_release(amp1->ahp, btop(amp1->size));
			amp1->ahp = nahp;
			amp1->size = asize;
			svd1->anon_index = 0;
		}
		mutex_exit(&amp1->lock);
	}
	if (svd1->vpage != NULL) {
		new_vpage =
		    kmem_zalloc(vpgtob(seg_pages(seg1) + seg_pages(seg2)),
			KM_NOSLEEP);
		if (new_vpage == NULL)
			return (-1);
		bcopy(svd1->vpage, new_vpage, vpgtob(seg_pages(seg1)));
		kmem_free(svd1->vpage, vpgtob(seg_pages(seg1)));
		svd1->vpage = new_vpage;
		if (svd1->pageprot) {
			struct vpage *vp, *evp;

			vp = new_vpage + seg_pages(seg1);
			evp = vp + seg_pages(seg2);
			for (; vp < evp; vp++)
				VPP_SETPROT(vp, a->prot);
		}
	}
	size = seg2->s_size;
	seg_free(seg2);
	seg1->s_size += size;
	svd1->swresv += swresv;
	return (0);
}

/*
 * Extend the next segment (seg2) to include the
 * new segment (seg1 + a), if possible.
 * Return 0 on success.
 */
static int
segvn_extend_next(
	struct seg *seg1,
	struct seg *seg2,
	struct segvn_crargs *a,
	size_t swresv)
{
	struct segvn_data *svd2 = (struct segvn_data *)seg2->s_data;
	size_t size;
	struct anon_map *amp2;
	struct vpage *new_vpage;

	/*
	 * We don't need any segment level locks for "segvn" data
	 * since the address space is "write" locked.
	 */
	ASSERT(seg2->s_as && AS_WRITE_HELD(seg2->s_as, &seg2->s_as->a_lock));

	/* first segment is new, try to extend second */
	/* XXX - should also check cred */
	if (svd2->vp != a->vp || svd2->maxprot != a->maxprot ||
	    (!svd2->pageprot && (svd2->prot != a->prot)) ||
	    svd2->type != a->type || svd2->flags != a->flags)
		return (-1);
	/* vp == NULL implies zfod, offset doesn't matter */
	if (svd2->vp != NULL &&
	    (a->offset & PAGEMASK) + seg1->s_size != svd2->offset)
		return (-1);

	amp2 = svd2->amp;
	if (amp2) {
		/*
		 * Segment has private pages, can data structures
		 * be expanded?
		 *
		 * Acquire the anon_map lock to prevent it from changing,
		 * if it is shared.  This ensures that the anon_map
		 * will not change while a thread which has a read/write
		 * lock on an address space references it.
		 *
		 * XXX - Don't need the anon_map lock at all if "refcnt"
		 * is 1.
		 */
		mutex_enter(&amp2->lock);
		if (amp2->refcnt > 1 || svd2->type == MAP_SHARED) {
			mutex_exit(&amp2->lock);
			return (-1);
		}
		if (ptob(svd2->anon_index) < seg1->s_size) {
			struct anon_hdr *nahp;
			size_t	asize;

			/*
			 * We need a bigger anon array.  Allocate a new
			 * one with anon_slop worth of slop at the
			 * beginning so it will be easier to expand in
			 * place the next time we need to do this.
			 */
			asize = seg1->s_size + seg2->s_size + anon_slop;
			nahp = anon_create(btop(asize), ANON_NOSLEEP);
			if (nahp == NULL) {
				mutex_exit(&amp2->lock);
				return (-1);
			}
			if (anon_copy_ptr(amp2->ahp, svd2->anon_index,
				nahp, btop(anon_slop) + seg_pages(seg1),
				btop(seg2->s_size), ANON_NOSLEEP) == ENOMEM) {
				anon_release(nahp, btop(asize));
				mutex_exit(&amp2->lock);
				return (-1);
			}
			anon_release(amp2->ahp, btop(amp2->size));

			amp2->ahp = nahp;
			amp2->size = asize;
			svd2->anon_index = btop(anon_slop);
		} else {
			/* Can just expand anon array in place. */
			svd2->anon_index -= seg_pages(seg1);
		}
		mutex_exit(&amp2->lock);
	}
	if (svd2->vpage != NULL) {
		new_vpage =
		    kmem_zalloc(vpgtob(seg_pages(seg1) + seg_pages(seg2)),
			KM_NOSLEEP);
		if (new_vpage == NULL) {
			/* Not merging segments so adjust anon_index back */
			if (amp2)
				svd2->anon_index += seg_pages(seg1);
			return (-1);
		}
		bcopy(svd2->vpage, new_vpage + seg_pages(seg1),
		    vpgtob(seg_pages(seg2)));
		kmem_free(svd2->vpage, vpgtob(seg_pages(seg2)));
		svd2->vpage = new_vpage;
		if (svd2->pageprot) {
			struct vpage *vp, *evp;

			vp = new_vpage;
			evp = vp + seg_pages(seg1);
			for (; vp < evp; vp++)
				VPP_SETPROT(vp, a->prot);
		}
	}
	size = seg1->s_size;
	seg_free(seg1);
	seg2->s_size += size;
	seg2->s_base -= size;
	svd2->offset -= size;
	svd2->swresv += swresv;
	return (0);
}

static int
segvn_dup(struct seg *seg, struct seg *newseg)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	struct segvn_data *newsvd;
	pgcnt_t npages = seg_pages(seg);
	int error = 0;
	uint_t prot;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * If segment has anon reserved, reserve more for the new seg.
	 * For a MAP_NORESERVE segment swresv will be a count of all the
	 * allocated anon slots; thus we reserve for the child as many slots
	 * as the parent has allocated. This semantic prevents the child or
	 * parent from dieing during a copy-on-write fault caused by trying
	 * to write a shared pre-existing anon page.
	 */
	if (svd->swresv && anon_resv(svd->swresv) == 0)
		return (ENOMEM);

#if defined(TRACE)
	if (svd->swresv) {
		TRACE_5(TR_FAC_VM, TR_ANON_PROC, "anon proc:%u %p %lu %lu %u",
			curproc->p_child->p_pid, newseg,
			newseg->s_base, svd->swresv, 1);
	}
#endif

	newsvd = kmem_cache_alloc(segvn_cache, KM_SLEEP);

	newseg->s_ops = &segvn_ops;
	newseg->s_data = (void *)newsvd;

	if ((newsvd->vp = svd->vp) != NULL)
		VN_HOLD(svd->vp);
	newsvd->offset = svd->offset;
	newsvd->prot = svd->prot;
	newsvd->maxprot = svd->maxprot;
	newsvd->pageprot = svd->pageprot;
	newsvd->type = svd->type;
	newsvd->cred = svd->cred;
	crhold(newsvd->cred);
	newsvd->advice = svd->advice;
	newsvd->pageadvice = svd->pageadvice;
	newsvd->swresv = svd->swresv;
	newsvd->flags = svd->flags;
	newsvd->softlockcnt = 0;
	if ((newsvd->amp = svd->amp) == NULL) {
		/*
		 * Not attaching to a shared anon object.
		 */
		newsvd->anon_index = 0;
	} else {
		struct anon_map *amp;		/* XXX - for locknest */

		amp = svd->amp;
		if (svd->type == MAP_SHARED) {
			mutex_enter(&amp->lock);
			amp->refcnt++;
			mutex_exit(&amp->lock);
			newsvd->anon_index = svd->anon_index;
		} else {
			int reclaim = 1;

			/*
			 * Allocate and initialize new anon_map structure.
			 */
			newsvd->amp = anonmap_alloc(newseg->s_size, 0);
			newsvd->anon_index = 0;

			/*
			 * We don't have to acquire the anon_map lock
			 * for the new segment (since it belongs to an
			 * address space that is still not associated
			 * with any process), or the segment in the old
			 * address space (since all threads in it
			 * are stopped while duplicating the address space).
			 */

			/*
			 * The goal of the following code is to make sure that
			 * softlocked pages do not end up as copy on write
			 * pages.  This would cause problems where one
			 * thread writes to a page that is COW and a different
			 * thread in the same process has softlocked it.  The
			 * softlock lock would move away from this process
			 * because the write would cause this process to get
			 * a copy (without the softlock).
			 *
			 * The strategy here is to just break the
			 * sharing on pages that could possibly be
			 * softlocked.
			 */
retry:
			if (svd->softlockcnt) {
				struct anon *ap, *newap;
				size_t i;
				uint_t vpprot;
				page_t *anon_pl[1+1], *pp;
				caddr_t addr;
				ulong_t anon_idx = 0;

				/*
				 * The softlock count might be non zero
				 * because some pages are still stuck in the
				 * cache for lazy reclaim. Flush the cache
				 * now. This should drop the count to zero.
				 * [or there is really I/O going on to these
				 * pages]. Note, we have the writers lock so
				 * nothing gets inserted during the flush.
				 */
				if (reclaim == 1) {
					segvn_purge(seg);
					reclaim = 0;
					goto retry;
				}
				i = btopr(seg->s_size);
				addr = seg->s_base;
				while (i-- > 0) {
					if (ap = anon_get_ptr(amp->ahp,
							anon_idx)) {
					    error = anon_getpage(&ap, &vpprot,
						anon_pl, PAGESIZE, seg,
						addr, S_READ, svd->cred);
					    if (error) {
						newsvd->vpage = NULL;
						goto out;
					    }
					/*
					 * prot need not be computed
					 * below 'cause anon_private is
					 * going to ignore it anyway
					 * as child doesn't inherit
					 * pagelock from parent.
					 */
					    prot = svd->pageprot ?
						VPP_PROT(&svd->vpage[seg_page
								(seg, addr)]) :
							svd->prot;
					    pp = anon_private(&newap,
						newseg, addr, prot, anon_pl[0],
						0, newsvd->cred);
					    if (pp == NULL) { /* no mem abort */
						newsvd->vpage = NULL;
						error = ENOMEM;
						goto out;
					    }
					    (void) anon_set_ptr(
							newsvd->amp->ahp,
							anon_idx, newap,
							ANON_SLEEP);
					    page_unlock(pp);
					}
					addr += PAGESIZE;
					anon_idx++;
				}
			} else {	/* common case */
				anon_dup(amp->ahp, svd->anon_index,
					newsvd->amp->ahp, 0, seg->s_size);

				hat_clrattr(seg->s_as->a_hat, seg->s_base,
				    seg->s_size, PROT_WRITE);
			}
		}
	}
	/*
	 * If necessary, create a vpage structure for the new segment.
	 * Do not copy any page lock indications.
	 */
	if (svd->vpage != NULL) {
		uint_t i;
		struct vpage *ovp = svd->vpage;
		struct vpage *nvp;

		nvp = newsvd->vpage =
		    kmem_alloc(vpgtob(npages), KM_SLEEP);
		for (i = 0; i < npages; i++) {
			*nvp = *ovp++;
			VPP_CLRPPLOCK(nvp++);
		}
	} else
		newsvd->vpage = NULL;

	/* Inform the vnode of the new mapping */
	if (newsvd->vp)
		error = VOP_ADDMAP(newsvd->vp, (offset_t)newsvd->offset,
		    newseg->s_as, newseg->s_base, newseg->s_size, newsvd->prot,
		    newsvd->maxprot, newsvd->type, newsvd->cred);
out:
	return (error);
}

static int
segvn_unmap(struct seg *seg, caddr_t addr, size_t len)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	struct segvn_data *nsvd;
	struct seg *nseg;
	pgcnt_t	opages;		/* old segment size in pages */
	pgcnt_t	npages;		/* new segment size in pages */
	pgcnt_t	dpages;		/* pages being deleted (unmapped) */
	caddr_t nbase;
	size_t nsize;
	struct anon_map *amp;		/* XXX - for locknest */
	size_t oswresv;
	int reclaim = 1;

	/*
	 * We don't need any segment level locks for "segvn" data
	 * since the address space is "write" locked.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Fail the unmap if pages are SOFTLOCKed through this mapping.
	 * softlockcnt is protected from change by the as write lock.
	 */
retry:
	if (svd->softlockcnt > 0) {
		/*
		 * since we do have the writers lock nobody can fill
		 * the cache during the purge. The flush either succeeds
		 * or we still have pending I/Os.
		 */
		if (reclaim == 1) {
			segvn_purge(seg);
			reclaim = 0;
			goto retry;
		}
		return (EAGAIN);
	}

	/*
	 * Check for bad sizes
	 */
	if (addr < seg->s_base || addr + len > seg->s_base + seg->s_size ||
	    (len & PAGEOFFSET) || ((uintptr_t)addr & PAGEOFFSET))
		cmn_err(CE_PANIC, "segvn_unmap");

	/* Inform the vnode of the unmapping. */
	if (svd->vp) {
		VOP_DELMAP(svd->vp,
			(offset_t)svd->offset + (addr - seg->s_base),
			seg->s_as, addr, len, svd->prot, svd->maxprot,
			svd->type, svd->cred);
	}
	/*
	 * Remove any page locks set through this mapping.
	 */
	(void) segvn_lockop(seg, addr, len, 0, MC_UNLOCK, NULL, 0);

	/*
	 * Unload any hardware translations in the range to be taken out.
	 */
	hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD_UNMAP);

	/*
	 * Check for entire segment
	 */
	if (addr == seg->s_base && len == seg->s_size) {
		seg_free(seg);
		return (0);
	}

	opages = seg_pages(seg);
	dpages = btop(len);
	npages = opages - dpages;
	amp = svd->amp;			/* XXX - for locknest */

	/*
	 * Check for beginning of segment
	 */
	if (addr == seg->s_base) {
		if (svd->vpage != NULL) {
			size_t nbytes;
			struct vpage *ovpage;

			ovpage = svd->vpage;	/* keep pointer to vpage */

			nbytes = vpgtob(npages);
			svd->vpage = kmem_alloc(nbytes, KM_SLEEP);
			bcopy(&ovpage[dpages], svd->vpage, nbytes);

			/* free up old vpage */
			kmem_free(ovpage, vpgtob(opages));
		}
		if (amp != NULL) {
			mutex_enter(&amp->lock);
			if (amp->refcnt == 1 || svd->type == MAP_PRIVATE) {
				/*
				 * Free up now unused parts of anon_map array.
				 */
				anon_free(amp->ahp, svd->anon_index, len);

				/*
				 * Unreserve swap space for the unmapped chunk
				 * of this segment in case it's MAP_SHARED
				 */
				if (svd->type == MAP_SHARED) {
					anon_unresv(len);
					amp->swresv -= len;
				}
			}
			mutex_exit(&amp->lock);
			svd->anon_index += dpages;
		}
		if (svd->vp != NULL) {
			free_vp_pages(svd->vp, svd->offset, len);
			svd->offset += len;
		}

		if (svd->swresv) {
			if (svd->flags & MAP_NORESERVE) {
				ASSERT(amp);
				oswresv = svd->swresv;

				svd->swresv = ptob(anon_pages(amp->ahp,
						svd->anon_index, npages));
				anon_unresv(oswresv - svd->swresv);
			} else {
				anon_unresv(len);
				svd->swresv -= len;
			}
			TRACE_5(TR_FAC_VM, TR_ANON_PROC,
				"anon proc:%u %p %lu %lu %u",
				curproc->p_pid, seg, addr, len, 0);
		}

		seg->s_base += len;
		seg->s_size -= len;
		return (0);
	}

	/*
	 * Check for end of segment
	 */
	if (addr + len == seg->s_base + seg->s_size) {
		if (svd->vpage != NULL) {
			size_t nbytes;
			struct vpage *ovpage;

			ovpage = svd->vpage;	/* keep pointer to vpage */

			nbytes = vpgtob(npages);
			svd->vpage = kmem_alloc(nbytes, KM_SLEEP);
			bcopy(ovpage, svd->vpage, nbytes);

			/* free up old vpage */
			kmem_free(ovpage, vpgtob(opages));

		}
		if (amp != NULL) {
			mutex_enter(&amp->lock);
			if (amp->refcnt == 1 || svd->type == MAP_PRIVATE) {
				/*
				 * Free up now unused parts of anon_map array
				 */
				anon_free(amp->ahp, svd->anon_index + npages,
								len);

				/*
				 * Unreserve swap space for the unmapped chunk
				 * of this segment in case it's MAP_SHARED
				 */
				if (svd->type == MAP_SHARED) {
					anon_unresv(len);
					amp->swresv -= len;
				}
			}
			mutex_exit(&amp->lock);
		}

		if (svd->swresv) {
			if (svd->flags & MAP_NORESERVE) {
				ASSERT(amp);
				oswresv = svd->swresv;
				svd->swresv = ptob(anon_pages(amp->ahp,
					svd->anon_index, npages));
				anon_unresv(oswresv - svd->swresv);
			} else {
				anon_unresv(len);
				svd->swresv -= len;
			}
			TRACE_5(TR_FAC_VM, TR_ANON_PROC,
				"anon proc:%u %p %lu %lu %u",
				curproc->p_pid, seg, addr, len, 0);
		}

		seg->s_size -= len;
		if (svd->vp != NULL)
			free_vp_pages(svd->vp, svd->offset + seg->s_size, len);
		return (0);
	}

	/*
	 * The section to go is in the middle of the segment,
	 * have to make it into two segments.  nseg is made for
	 * the high end while seg is cut down at the low end.
	 */
	nbase = addr + len;				/* new seg base */
	nsize = (seg->s_base + seg->s_size) - nbase;	/* new seg size */
	seg->s_size = addr - seg->s_base;		/* shrink old seg */
	nseg = seg_alloc(seg->s_as, nbase, nsize);
	if (nseg == NULL)
		cmn_err(CE_PANIC, "segvn_unmap seg_alloc");

	nsvd = kmem_cache_alloc(segvn_cache, KM_SLEEP);

	nseg->s_ops = seg->s_ops;
	nseg->s_data = (void *)nsvd;
	nsvd->pageprot = svd->pageprot;
	nsvd->prot = svd->prot;
	nsvd->maxprot = svd->maxprot;
	nsvd->type = svd->type;
	nsvd->vp = svd->vp;
	nsvd->cred = svd->cred;
	nsvd->offset = svd->offset + nseg->s_base - seg->s_base;
	nsvd->swresv = 0;
	nsvd->advice = svd->advice;
	nsvd->pageadvice = svd->pageadvice;
	nsvd->flags = svd->flags;
	nsvd->softlockcnt = 0;
	if (svd->vp != NULL) {
		free_vp_pages(svd->vp, svd->offset + seg->s_size, len);
		VN_HOLD(nsvd->vp);
	}
	crhold(svd->cred);

	if (svd->vpage == NULL)
		nsvd->vpage = NULL;
	else {
		/* need to split vpage into two arrays */
		size_t nbytes;
		struct vpage *ovpage;

		ovpage = svd->vpage;		/* keep pointer to vpage */

		npages = seg_pages(seg);	/* seg has shrunk */
		nbytes = vpgtob(npages);
		svd->vpage = kmem_alloc(nbytes, KM_SLEEP);

		bcopy(ovpage, svd->vpage, nbytes);

		npages = seg_pages(nseg);
		nbytes = vpgtob(npages);
		nsvd->vpage = kmem_alloc(nbytes, KM_SLEEP);

		bcopy(&ovpage[opages - npages], nsvd->vpage, nbytes);

		/* free up old vpage */
		kmem_free(ovpage, vpgtob(opages));
	}

	if (amp == NULL) {
		nsvd->amp = NULL;
		nsvd->anon_index = 0;
	} else {
		/*
		 * Share the same anon_map structure.
		 */
		opages = btop(addr - seg->s_base);
		npages = btop(nseg->s_base - seg->s_base);

		mutex_enter(&amp->lock);
		if (amp->refcnt == 1 || svd->type == MAP_PRIVATE) {
			/*
			 * Free up now unused parts of anon_map array
			 */
			anon_free(amp->ahp, svd->anon_index + opages, len);

			/*
			 * Unreserve swap space for the unmapped chunk
			 * of this segment in case it's MAP_SHARED
			 */
			if (svd->type == MAP_SHARED) {
				anon_unresv(len);
				amp->swresv -= len;
			}
		}

		amp->refcnt++;
		mutex_exit(&amp->lock);

		nsvd->amp = amp;
		nsvd->anon_index = svd->anon_index + npages;
	}
	if (svd->swresv) {
		if (svd->flags & MAP_NORESERVE) {
			ASSERT(amp);
			oswresv = svd->swresv;
			svd->swresv = ptob(anon_pages(amp->ahp,
				svd->anon_index, btop(seg->s_size)));
			nsvd->swresv = ptob(anon_pages(amp->ahp,
				nsvd->anon_index, btop(nseg->s_size)));

			ASSERT(oswresv >= (svd->swresv + nsvd->swresv));
			anon_unresv(oswresv - (svd->swresv + nsvd->swresv));
		} else {
			if (seg->s_size + nseg->s_size + len != svd->swresv) {
				cmn_err(CE_PANIC,
				"segvn_unmap: cannot split swap reservation");
			}
			anon_unresv(len);
			svd->swresv = seg->s_size;
			nsvd->swresv = nseg->s_size;
		}
		TRACE_5(TR_FAC_VM, TR_ANON_PROC, "anon proc:%u %p %lu %lu %u",
			curproc->p_pid, seg, addr, len, 0);
	}

	return (0);			/* I'm glad that's all over with! */
}

static void
segvn_free(struct seg *seg)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	pgcnt_t npages = seg_pages(seg);
	struct anon_map *amp;		/* XXX - for locknest */

	/*
	 * We don't need any segment level locks for "segvn" data
	 * since the address space is "write" locked.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Be sure to unlock pages. XXX Why do things get free'ed instead
	 * of unmapped? XXX
	 */
	(void) segvn_lockop(seg, seg->s_base, seg->s_size,
	    0, MC_UNLOCK, NULL, 0);

	/*
	 * Deallocate the vpage and anon pointers if necessary and possible.
	 */
	if (svd->vpage != NULL) {
		kmem_free(svd->vpage, vpgtob(npages));
		svd->vpage = NULL;
	}
	if ((amp = svd->amp) != NULL) {
		/*
		 * If there are no more references to this anon_map
		 * structure, then deallocate the structure after freeing
		 * up all the anon slot pointers that we can.
		 */
		mutex_enter(&amp->lock);
		if (--amp->refcnt == 0) {
			if (svd->type == MAP_PRIVATE) {
				/*
				 * Private - we only need to anon_free
				 * the part that this segment refers to.
				 */
				anon_free(amp->ahp, svd->anon_index,
								seg->s_size);
			} else {
				/*
				 * Shared - anon_free the entire
				 * anon_map's worth of stuff and
				 * release any swap reservation.
				 */
				anon_free(amp->ahp, 0, amp->size);
				if (amp->swresv) {
					anon_unresv(amp->swresv);
					TRACE_5(TR_FAC_VM, TR_ANON_PROC,
						"anon proc:%u %p %lu %lu %u",
						curproc->p_pid, seg,
						seg->s_base, amp->swresv, 0);
				}
			}
			svd->amp = NULL;
			mutex_exit(&amp->lock);
			anonmap_free(amp);
		} else if (svd->type == MAP_PRIVATE) {
			/*
			 * We had a private mapping which still has
			 * a held anon_map so just free up all the
			 * anon slot pointers that we were using.
			 */
			anon_free(amp->ahp, svd->anon_index, seg->s_size);
			mutex_exit(&amp->lock);
		} else {
			mutex_exit(&amp->lock);
		}
	}

	/*
	 * Release swap reservation.
	 */
	if (svd->swresv) {
		anon_unresv(svd->swresv);
		TRACE_5(TR_FAC_VM, TR_ANON_PROC, "anon proc:%u %p %lu %lu %u",
			curproc->p_pid, seg, seg->s_base, svd->swresv, 0);
		svd->swresv = 0;
	}
	/*
	 * Release claim on vnode, credentials, and finally free the
	 * private data.
	 */
	if (svd->vp != NULL) {
		free_vp_pages(svd->vp, svd->offset, seg->s_size);
		VN_RELE(svd->vp);
		svd->vp = NULL;
	}
	crfree(svd->cred);
	svd->cred = NULL;

	seg->s_data = NULL;
	kmem_cache_free(segvn_cache, svd);
}

/*
 * Do a F_SOFTUNLOCK call over the range requested.
 * The range must have already been F_SOFTLOCK'ed.
 */
static void
segvn_softunlock(struct seg *seg, caddr_t addr, size_t len, enum seg_rw rw)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	page_t *pp;
	caddr_t adr;
	struct vnode *vp;
	u_offset_t offset;
	ulong_t anon_index;
	struct anon_map *amp;		/* XXX - for locknest */
	struct anon *ap = NULL;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);

	if ((amp = svd->amp) != NULL)
		anon_index = svd->anon_index + seg_page(seg, addr);

	hat_unlock(seg->s_as->a_hat, addr, len);
	for (adr = addr; adr < addr + len; adr += PAGESIZE) {
		if (amp != NULL) {
			mutex_enter(&amp->lock);
			if ((ap = anon_get_ptr(amp->ahp, anon_index++))
								!= NULL) {
				swap_xlate(ap, &vp, &offset);
			} else {
				vp = svd->vp;
				offset = svd->offset + (adr - seg->s_base);
			}
			mutex_exit(&amp->lock);
		} else {
			vp = svd->vp;
			offset = svd->offset + (adr - seg->s_base);
		}

		/*
		 * Use page_find() instead of page_lookup() to
		 * find the page since we know that it has a
		 * "shared" lock.
		 */
		pp = page_find(vp, offset);
		if (pp == NULL) {
			cmn_err(CE_PANIC,
			    "segvn_softunlock: addr %p, ap %p, vp %p, off %llx",
			    (void *)adr, (void *)ap, (void *)vp, offset);
		}

		if (rw == S_WRITE) {
			hat_setrefmod(pp);
			if (seg->s_as->a_vbits)
				hat_setstat(seg->s_as, adr, PAGESIZE,
				    P_REF | P_MOD);
		} else if (rw != S_OTHER) {
			hat_setref(pp);
			if (seg->s_as->a_vbits)
				hat_setstat(seg->s_as, adr, PAGESIZE, P_REF);
		}
		TRACE_3(TR_FAC_VM, TR_SEGVN_FAULT,
			"segvn_fault:pp %p vp %p offset %llx", pp, vp, offset);
		page_unlock(pp);
	}
	mutex_enter(&freemem_lock); /* for availrmem */
	availrmem += btop(len);
	svd->softlockcnt -= btop(len);
	mutex_exit(&freemem_lock);
	if (svd->softlockcnt == 0) {
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
	SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
}

#define	PAGE_HANDLED	((page_t *)-1)

/*
 * Release all the pages in the NULL terminated ppp list
 * which haven't already been converted to PAGE_HANDLED.
 */
static void
segvn_pagelist_rele(page_t **ppp)
{
	for (; *ppp != NULL; ppp++) {
		if (*ppp != PAGE_HANDLED)
			page_unlock(*ppp);
	}
}

static int stealcow = 1;

/*
 * Workaround for viking chip bug.  See bug id 1220902.
 * To fix this down in pagefault() would require importing so
 * much as and segvn code as to be unmaintainable.
 */
int enable_mbit_wa = 0;

/*
 * Handles all the dirty work of getting the right
 * anonymous pages and loading up the translations.
 * This routine is called only from segvn_fault()
 * when looping over the range of addresses requested.
 *
 * The basic algorithm here is:
 * 	If this is an anon_zero case
 *		Call anon_zero to allocate page
 *		Load up translation
 *		Return
 *	endif
 *	If this is an anon page
 *		Use anon_getpage to get the page
 *	else
 *		Find page in pl[] list passed in
 *	endif
 *	If not a cow
 *		Load up the translation to the page
 *		return
 *	endif
 *	Call anon_private to handle cow
 *	Load up (writable) translation to new page
 */
static int
segvn_faultpage(
	struct hat *hat,		/* the hat to use for mapping */
	struct seg *seg,		/* seg_vn of interest */
	caddr_t addr,			/* address in as */
	u_offset_t off,			/* offset in vp */
	struct vpage *vpage,		/* pointer to vpage for vp, off */
	page_t *pl[],			/* object source page pointer */
	uint_t vpprot,			/* access allowed to object pages */
	enum fault_type type,		/* type of fault */
	enum seg_rw rw)			/* type of access at fault */
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	page_t *pp, **ppp;
	uint_t pageflags = 0;
	page_t *anon_pl[1 + 1];
	page_t *opp = NULL;		/* original page */
	uint_t prot;
	int err;
	int cow;
	int map_shared;
	int steal = 0;
	ulong_t anon_index;
	struct anon *ap, *oldap;
	struct anon_map *amp;		/* XXX - for locknest */

	ASSERT(SEGVN_READ_HELD(seg->s_as, &svd->lock));

	/*
	 * Initialize protection value for this page.
	 * If we have per page protection values check it now.
	 */
	if (svd->pageprot) {
		uint_t protchk;

		switch (rw) {
		case S_READ:
			protchk = PROT_READ;
			break;
		case S_WRITE:
			protchk = PROT_WRITE;
			break;
		case S_EXEC:
			protchk = PROT_EXEC;
			break;
		case S_OTHER:
		default:
			protchk = PROT_READ | PROT_WRITE | PROT_EXEC;
			break;
		}

		prot = VPP_PROT(vpage);
		if ((prot & protchk) == 0)
			return (FC_PROT);	/* illegal access type */
	} else {
		prot = svd->prot;
	}

	if (type == F_SOFTLOCK) {
		mutex_enter(&freemem_lock);
		if (availrmem <= tune.t_minarmem) {
			mutex_exit(&freemem_lock);
			return (FC_MAKE_ERR(ENOMEM));	/* out of real memory */
		} else {
			svd->softlockcnt++;
			availrmem--;
		}
		mutex_exit(&freemem_lock);
	}
	if ((amp = svd->amp) != NULL)
		anon_index = svd->anon_index + seg_page(seg, addr);

	if (svd->vp == NULL && amp != NULL) {

		/*
		 * Always acquire the "serialization" lock before
		 * the anon_map lock to prevent 2 threads from
		 * allocating separate anon slots for the same "addr".
		 *
		 * We've already grabbed the "serialization" lock in
		 * segvn_fault() if this is a MAP_PRIVATE segment.
		 */
		map_shared = 0;
		if (svd->type == MAP_SHARED) {
			mutex_enter(&amp->serial_lock);
			map_shared = 1;
		}

		if ((ap = anon_get_ptr(amp->ahp, anon_index)) == NULL) {
			/*
			 * Allocate a (normally) writable anonymous page of
			 * zeroes. If no advance reservations, reserve now.
			 */
			if (svd->flags & MAP_NORESERVE) {
				if (anon_resv(ptob(1))) {
					svd->swresv += ptob(1);
				} else {
					err = ENOMEM;
					if (map_shared)
						mutex_exit(&amp->serial_lock);
					goto out;
				}
			}
			if ((pp = anon_zero(seg, addr, &ap,
			    svd->cred)) == NULL) {
				err = ENOMEM;
				if (map_shared)
					mutex_exit(&amp->serial_lock);
				goto out;	/* out of swap space */
			}
			/*
			 * Re-acquire the anon_map lock and
			 * initialize the anon array entry.
			 */
			mutex_enter(&amp->lock);
			(void) anon_set_ptr(amp->ahp, anon_index, ap,
				ANON_SLEEP);
			mutex_exit(&amp->lock);
			if (enable_mbit_wa) {
				if (rw == S_WRITE)
					hat_setmod(pp);
				else if (!hat_ismod(pp))
					prot &= ~PROT_WRITE;
			}
			if (type == F_SOFTLOCK) {
				/*
				 * Load up the translation keeping it
				 * locked and don't unlock the page.
				 */
				hat_memload(hat, addr, pp, prot, HAT_LOAD_LOCK);
			} else {
				hat_memload(hat, addr, pp, prot, HAT_LOAD);
				page_unlock(pp);
			}
			if (map_shared)
				mutex_exit(&amp->serial_lock);
			return (0);
		}
		if (map_shared)
			mutex_exit(&amp->serial_lock);
	}

	/*
	 * Obtain the page structure via anon_getpage() if it is
	 * a private copy of an object (the result of a previous
	 * copy-on-write).
	 */
	if (amp != NULL) {
		if ((ap = anon_get_ptr(amp->ahp, anon_index)) != NULL) {
			err = anon_getpage(&ap, &vpprot, anon_pl, PAGESIZE,
			    seg, addr, rw, svd->cred);
			if (err)
				goto out;

			if (svd->type == MAP_SHARED) {
				/*
				 * If this is a shared mapping to an
				 * anon_map, then ignore the write
				 * permissions returned by anon_getpage().
				 * They apply to the private mappings
				 * of this anon_map.
				 */
				vpprot |= PROT_WRITE;
			}
			opp = anon_pl[0];
		}
	}

	/*
	 * Search the pl[] list passed in if it is from the
	 * original object (i.e., not a private copy).
	 */
	if (opp == NULL) {
		/*
		 * Find original page.  We must be bringing it in
		 * from the list in pl[].
		 */
		for (ppp = pl; (opp = *ppp) != NULL; ppp++) {
			if (opp == PAGE_HANDLED)
				continue;
			ASSERT(opp->p_vnode == svd->vp); /* XXX */
			if (opp->p_offset == off)
				break;
		}
		if (opp == NULL)
			cmn_err(CE_PANIC, "segvn_faultpage not found");
		*ppp = PAGE_HANDLED;

	}

	ASSERT(PAGE_LOCKED(opp));

	TRACE_3(TR_FAC_VM, TR_SEGVN_FAULT,
		"segvn_fault:pp %p vp %p offset %llx",
		opp, opp->p_vnode, opp->p_offset);

	/*
	 * The fault is treated as a copy-on-write fault if a
	 * write occurs on a private segment and the object
	 * page (i.e., mapping) is write protected.  We assume
	 * that fatal protection checks have already been made.
	 */

	cow = BREAK_COW_SHARE(rw, type, svd->type) &&
	    ((vpprot & PROT_WRITE) == 0);

	/*
	 * If not a copy-on-write case load the translation
	 * and return.
	 */
	if (cow == 0) {
		if (enable_mbit_wa) {
			if (rw == S_WRITE)
				hat_setmod(opp);
			else if (!hat_ismod(opp))
				prot &= ~PROT_WRITE;
		}
		if (type == F_SOFTLOCK) {
			/*
			 * Load up the translation keeping it
			 * locked and don't unlock the page.
			 */
			hat_memload(hat, addr, opp, prot & vpprot,
				HAT_LOAD_LOCK);
		} else {
			hat_memload(hat, addr, opp, prot & vpprot, HAT_LOAD);
			page_unlock(opp);
		}
		return (0);
	}

	hat_setref(opp);

	ASSERT(amp != NULL && MUTEX_HELD(&amp->serial_lock));

	/*
	 * Steal the page only if it isn't a private page
	 * since stealing a private page is not worth the effort.
	 */
	if ((ap = anon_get_ptr(amp->ahp, anon_index)) == NULL)
		steal = 1;

	/*
	 * Steal the original page if the following conditions are true:
	 *
	 * We are low on memory, the page is not private, page is not
	 * shared, not modified, not `locked' or if we have it `locked'
	 * (i.e., p_cowcnt == 1 and p_lckcnt == 0, which also implies
	 * that the page is not shared) and if it doesn't have any
	 * translations. page_struct_lock isn't needed to look at p_cowcnt
	 * and p_lckcnt because we first get exclusive lock on page.
	 */
	(void) hat_pagesync(opp, HAT_SYNC_DONTZERO | HAT_SYNC_STOPON_MOD);

	if (stealcow && freemem < minfree && steal &&
	    page_tryupgrade(opp) && !hat_ismod(opp) &&
	    ((opp->p_lckcnt == 0 && opp->p_cowcnt == 0) ||
	    (opp->p_lckcnt == 0 && opp->p_cowcnt == 1 &&
	    vpage != NULL && VPP_ISPPLOCK(vpage)))) {
		/*
		 * Check if this page has other translations
		 * after unloading our translation.
		 */
		if (hat_page_is_mapped(opp)) {
			hat_unload(seg->s_as->a_hat, addr, PAGESIZE,
				HAT_UNLOAD);
		}

		/*
		 * hat_unload() might sync back someone else's recent
		 * modification, so check again.
		 */
		if (!hat_ismod(opp) && !hat_page_is_mapped(opp))
			pageflags |= STEAL_PAGE;
	}

	/*
	 * If we have a vpage pointer, see if it indicates that we have
	 * ``locked'' the page we map -- if so, tell anon_private to
	 * transfer the locking resource to the new page.
	 *
	 * See Statement at the beginning of segvn_lockop regarding
	 * the way lockcnts/cowcnts are handled during COW.
	 *
	 */
	if (vpage != NULL && VPP_ISPPLOCK(vpage))
		pageflags |= LOCK_PAGE;

	/*
	 * Allocate a private page and perform the copy.
	 * For MAP_NORESERVE reserve swap space now, unless this
	 * is a cow fault on an existing anon page in which case
	 * MAP_NORESERVE will have made advance reservations.
	 */
	if ((svd->flags & MAP_NORESERVE) && (ap == NULL)) {
		if (anon_resv(ptob(1))) {
			svd->swresv += ptob(1);
		} else {
			page_unlock(opp);
			err = ENOMEM;
			goto out;
		}
	}
	oldap = ap;
	pp = anon_private(&ap, seg, addr, prot, opp, pageflags, svd->cred);
	if (pp == NULL) {
		err = ENOMEM;	/* out of swap space */
		goto out;
	}

	/*
	 * Re-acquire the anon_map lock and initialize
	 * the anon array entry.
	 */
	mutex_enter(&amp->lock);

	/*
	 * If we copied away from an anonymous page, then
	 * we are one step closer to freeing up an anon slot.
	 *
	 * NOTE:  The original anon slot must be released while
	 * holding the "anon_map" lock.  This is necessary to prevent
	 * other threads from obtaining a pointer to the anon slot
	 * which may be freed if its "refcnt" is 1.
	 */
	if (oldap != NULL)
		anon_decref(oldap);

	(void) anon_set_ptr(amp->ahp, anon_index, ap, ANON_SLEEP);
	mutex_exit(&amp->lock);

	if (enable_mbit_wa) {
		if (rw == S_WRITE)
			hat_setmod(pp);
		else if (!hat_ismod(pp))
			prot &= ~PROT_WRITE;
	}

	if (type == F_SOFTLOCK) {
		/*
		 * Load up the translation keeping it
		 * locked and don't unlock the page.
		 */
		hat_memload(hat, addr, pp, prot, HAT_LOAD_LOCK);
	} else {
		hat_memload(hat, addr, pp, prot, HAT_LOAD);
		page_unlock(pp);
	}

	return (0);
out:
	if (type == F_SOFTLOCK) {
		mutex_enter(&freemem_lock);
		availrmem++;
		svd->softlockcnt--;
		mutex_exit(&freemem_lock);
	}
	return (FC_MAKE_ERR(err));
}

int fltadvice = 1;	/* set to free behind pages for sequential access */

/*
 * This routine is called via a machine specific fault handling routine.
 * It is also called by software routines wishing to lock or unlock
 * a range of addresses.
 *
 * Here is the basic algorithm:
 *	If unlocking
 *		Call segvn_softunlock
 *		Return
 *	endif
 *	Checking and set up work
 *	If we will need some non-anonymous pages
 *		Call VOP_GETPAGE over the range of non-anonymous pages
 *	endif
 *	Loop over all addresses requested
 *		Call segvn_faultpage passing in page list
 *		    to load up translations and handle anonymous pages
 *	endloop
 *	Load up translation to any additional pages in page list not
 *	    already handled that fit into this segment
 */
static faultcode_t
segvn_fault(struct hat *hat, struct seg *seg, caddr_t addr, size_t len,
    enum fault_type type, enum seg_rw rw)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	page_t **plp, **ppp, *pp;
	u_offset_t off;
	caddr_t a;
	struct vpage *vpage;
	uint_t vpprot, prot;
	int err;
	page_t *pl[PVN_GETPAGE_NUM + 1];
	size_t plsz, pl_alloc_sz;
	size_t page;
	int serial_lock;
	ulong_t anon_index;
	struct anon_map *amp;		/* XXX - For locknest */
	int dogetpage = 0;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * First handle the easy stuff
	 */
	if (type == F_SOFTUNLOCK) {
		segvn_softunlock(seg, addr, len, rw);
		return (0);
	}

top:
	SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);

	/*
	 * If we have the same protections for the entire segment,
	 * insure that the access being attempted is legitimate.
	 */

	if (svd->pageprot == 0) {
		uint_t protchk;

		switch (rw) {
		case S_READ:
			protchk = PROT_READ;
			break;
		case S_WRITE:
			protchk = PROT_WRITE;
			break;
		case S_EXEC:
			protchk = PROT_EXEC;
			break;
		case S_OTHER:
		default:
			protchk = PROT_READ | PROT_WRITE | PROT_EXEC;
			break;
		}

		if ((svd->prot & protchk) == 0) {
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			return (FC_PROT);	/* illegal access type */
		}
	}

	/*
	 * Check to see if we need to allocate an anon_map structure.
	 */
	if (svd->amp == NULL && (svd->vp == NULL ||
	    BREAK_COW_SHARE(rw, type, svd->type))) {
		/*
		 * Drop the "read" lock on the segment and acquire
		 * the "write" version since we have to allocate the
		 * anon_map.
		 */
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_WRITER);

		if (svd->amp == NULL)
			svd->amp = anonmap_alloc(seg->s_size, 0);
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);

		/*
		 * Start all over again since segment protections
		 * may have changed after we dropped the "read" lock.
		 */
		goto top;
	}

	page = seg_page(seg, addr);
	if ((amp = svd->amp) != NULL) {
		anon_index = svd->anon_index + page;

		if ((type == F_PROT) && (rw == S_READ) &&
		    svd->type == MAP_PRIVATE && svd->pageprot == 0) {
			size_t index = anon_index;
			struct anon *ap;

			mutex_enter(&amp->lock);
			/*
			 * The fast path could apply to S_WRITE also, except
			 * that the protection fault could be caused by lazy
			 * tlb flush when ro->rw. In this case, the pte is
			 * RW already. But RO in the other cpu's tlb causes
			 * the fault. Since hat_chgprot won't do anything if
			 * pte doesn't change, we may end up faulting
			 * indefinitely until the RO tlb entry gets replaced.
			 */
			for (a = addr; a < addr + len; a += PAGESIZE, index++) {
				ap = anon_get_ptr(amp->ahp, index);
				if ((ap == NULL) || (ap->an_refcnt != 1)) {
					mutex_exit(&amp->lock);
					goto slow;
				}
			}
			hat_chgprot(seg->s_as->a_hat, addr, len, svd->prot);
			mutex_exit(&amp->lock);
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			return (0);
		}
	}
slow:

	if (svd->vpage == NULL)
		vpage = NULL;
	else
		vpage = &svd->vpage[page];

	off = svd->offset + (addr - seg->s_base);

	/*
	 * If MADV_SEQUENTIAL has been set for the particular page we
	 * are faulting on, free behind all pages in the segment and put
	 * them on the free list.
	 */
	if ((page != 0) && fltadvice) {	/* not if first page in segment */
		struct vpage *vpp;
		ulong_t fanon_index;
		size_t fpage;
		u_offset_t pgoff, fpgoff;
		struct vnode *fvp;
		struct anon *fap = NULL;

		if (svd->advice == MADV_SEQUENTIAL ||
		    (svd->pageadvice &&
		    VPP_ADVICE(vpage) == MADV_SEQUENTIAL)) {
			pgoff = off - PAGESIZE;
			fpage = page - 1;
			if (vpage != NULL)
				vpp = &svd->vpage[fpage];
			if (amp != NULL)
				fanon_index = svd->anon_index + fpage;

			while (pgoff > svd->offset) {
				if (svd->advice != MADV_SEQUENTIAL &&
				    (!svd->pageadvice || (vpage &&
				    VPP_ADVICE(vpp) != MADV_SEQUENTIAL)))
					break;

				/*
				 * If this is an anon page, we must find the
				 * correct <vp, offset> for it
				 */
				fap = NULL;
				if (amp != NULL) {
					mutex_enter(&amp->lock);
					fap = anon_get_ptr(amp->ahp,
								fanon_index);
					if (fap != NULL) {
						swap_xlate(fap, &fvp, &fpgoff);
					} else {
						fpgoff = pgoff;
						fvp = svd->vp;
					}
					mutex_exit(&amp->lock);
				} else {
					fpgoff = pgoff;
					fvp = svd->vp;
				}
				if (fvp == NULL)
					break;	/* XXX */
				/*
				 * Skip pages that are free or have an
				 * "exclusive" lock.
				 */
				pp = page_lookup_nowait(fvp, fpgoff, SE_SHARED);
				if (pp == NULL)
					break;
				/*
				 * We don't need the page_struct_lock to test
				 * as this is only advisory; even if we
				 * acquire it someone might race in and lock
				 * the page after we unlock and before the
				 * PUTPAGE, then VOP_PUTPAGE will do nothing.
				 */
				if (pp->p_lckcnt == 0 && pp->p_cowcnt == 0) {
					/*
					 * Hold the vnode before releasing
					 * the page lock to prevent it from
					 * being freed and re-used by some
					 * other thread.
					 */
					VN_HOLD(fvp);
					page_unlock(pp);
					/*
					 * We should build a page list
					 * to kluster putpages XXX
					 */
					(void) VOP_PUTPAGE(fvp,
					    (offset_t)fpgoff, PAGESIZE,
					    (B_DONTNEED|B_FREE|B_ASYNC),
					    svd->cred);
					VN_RELE(fvp);
				} else {
					/*
					 * XXX - Should the loop terminate if
					 * the page is `locked'?
					 */
					page_unlock(pp);
				}
				--vpp;
				--fanon_index;
				pgoff -= PAGESIZE;
			}
		}
	}

	plp = pl;
	*plp = NULL;
	pl_alloc_sz = 0;

	/*
	 * See if we need to call VOP_GETPAGE for
	 * *any* of the range being faulted on.
	 * We can skip all of this work if there
	 * was no original vnode.
	 */
	if (svd->vp != NULL) {
		u_offset_t vp_off;
		size_t vp_len;
		struct anon *ap;

		vp_off = off;
		vp_len = len;

		if (amp == NULL)
			dogetpage = 1;
		else {
			mutex_enter(&amp->lock);
			ap = anon_get_ptr(amp->ahp, anon_index);

			if (len <= PAGESIZE)
				/* inline non_anon() */
				dogetpage = (ap == NULL);
			else
				dogetpage = non_anon(amp->ahp, anon_index,
							&vp_off, &vp_len);
			mutex_exit(&amp->lock);
		}

		if (dogetpage) {
			enum seg_rw arw;
			struct as *as = seg->s_as;

			if (len > ptob((sizeof (pl) / sizeof (pl[0])) - 1)) {
				/*
				 * Page list won't fit in local array,
				 * allocate one of the needed size.
				 */
				pl_alloc_sz =
				    (btop(len) + 1) * sizeof (page_t *);
				plp = kmem_alloc(pl_alloc_sz, KM_SLEEP);
				plp[0] = NULL;
				plsz = len;
			} else if (rw == S_WRITE && svd->type == MAP_PRIVATE ||
				rw == S_OTHER ||
				(((size_t)(addr + PAGESIZE) <
				(size_t)(seg->s_base + seg->s_size)) &&
				hat_probe(as->a_hat, addr + PAGESIZE))) {
				/*
				 * Ask VOP_GETPAGE to return the exact number
				 * of pages if
				 * (a) this is a COW fault, or
				 * (b) this is a software fault, or
				 * (c) next page is already mapped.
				 */
				plsz = len;
			} else {
				/*
				 * Ask VOP_GETPAGE to return adjacent pages
				 * within the segment.
				 */
				plsz = MIN((size_t)PVN_GETPAGE_SZ, (size_t)
					((seg->s_base + seg->s_size) - addr));
				ASSERT((addr + plsz) <=
				    (seg->s_base + seg->s_size));
			}

			/*
			 * Need to get some non-anonymous pages.
			 * We need to make only one call to GETPAGE to do
			 * this to prevent certain deadlocking conditions
			 * when we are doing locking.  In this case
			 * non_anon() should have picked up the smallest
			 * range which includes all the non-anonymous
			 * pages in the requested range.  We have to
			 * be careful regarding which rw flag to pass in
			 * because on a private mapping, the underlying
			 * object is never allowed to be written.
			 */
			if (rw == S_WRITE && svd->type == MAP_PRIVATE) {
				arw = S_READ;
			} else {
				arw = rw;
			}
			TRACE_3(TR_FAC_VM, TR_SEGVN_GETPAGE,
				"segvn_getpage:seg %p addr %p vp %p",
				seg, addr, svd->vp);
			err = VOP_GETPAGE(svd->vp, (offset_t)vp_off, vp_len,
			    &vpprot, plp, plsz, seg, addr + (vp_off - off), arw,
			    svd->cred);
			if (err) {
				SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
				segvn_pagelist_rele(plp);
				if (pl_alloc_sz)
					kmem_free(plp, pl_alloc_sz);
				return (FC_MAKE_ERR(err));
			}
			if (svd->type == MAP_PRIVATE)
				vpprot &= ~PROT_WRITE;
		}
	}

	/*
	 * N.B. at this time the plp array has all the needed non-anon
	 * pages in addition to (possibly) having some adjacent pages.
	 */

	serial_lock = 0;
	/*
	 * Always acquire the "serialization" lock before
	 * the anon_map lock to prevent 2 threads from
	 * allocating separate anon slots for the same "addr".
	 *
	 * If this is a copy-on-write fault and we don't already
	 * have the "serialization" lock, acquire it to prevent the
	 * fault routine from handling multiple copy-on-write faults
	 * on the same "addr" in the same address space.
	 *
	 * Only one thread should deal with the fault since after
	 * it is handled, the other threads can acquire a translation
	 * to the newly created private page.  This prevents two or
	 * more threads from creating different private pages for the
	 * same fault.
	 *
	 * We grab "serialization" lock here if this is a MAP_PRIVATE segment
	 * to prevent deadlock between this thread and another thread
	 * which has soft-locked this page and wants to acquire serial_lock.
	 * ( bug 4026339 )
	 */
	if (amp != NULL && svd->type == MAP_PRIVATE) {
		mutex_enter(&amp->serial_lock);
		serial_lock = 1;
	}

	/*
	 * Ok, now loop over the address range and handle faults
	 */
	for (a = addr; a < addr + len; a += PAGESIZE, off += PAGESIZE) {
		err = segvn_faultpage(hat, seg, a, off, vpage, plp, vpprot,
		    type, rw);
		if (err) {
			if (serial_lock)
				mutex_exit(&amp->serial_lock);
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			if (type == F_SOFTLOCK && a > addr)
				segvn_softunlock(seg, addr, (a - addr),
				    S_OTHER);
			segvn_pagelist_rele(plp);
			if (pl_alloc_sz)
				kmem_free(plp, pl_alloc_sz);
			return (err);
		}
		if (vpage)
			vpage++;
	}

	/* Didn't get pages from the underlying fs so we're done */
	if (!dogetpage)
		goto done;

	/*
	 * Now handle any other pages in the list returned.
	 * If the page can be used, load up the translations now.
	 * Note that the for loop will only be entered if "plp"
	 * is pointing to a non-NULL page pointer which means that
	 * VOP_GETPAGE() was called and vpprot has been initialized.
	 */
	if (svd->pageprot == 0)
		prot = svd->prot & vpprot;

	/*
	 * Acquire the anon map "serial" lock to prevent other
	 * threads in the address space from creating private pages
	 * (i.e., allocating anon slots) while we are in the process
	 * of loading translations to additional pages returned by
	 * the underlying object.
	 *
	 * We've already acquired "serial" lock if this is a MAP_PRIVATE
	 * segment. If this is a MAP_SHARED segment, we must grab it now.
	 */
	if (amp != NULL && svd->type == MAP_SHARED) {
		mutex_enter(&amp->serial_lock);
		serial_lock = 1;
	}

	/*
	 * Large Files: diff should be unsigned value because we started
	 * supporting > 2GB segment sizes from 2.5.1 and when a
	 * large file of size > 2GB gets mapped to address space
	 * the diff value can be > 2GB.
	 */

	for (ppp = plp; (pp = *ppp) != NULL; ppp++) {
		size_t diff;
		struct anon *ap;

		if (pp == PAGE_HANDLED)
			continue;

		if (pp->p_offset >=  svd->offset &&
			(pp->p_offset < svd->offset + seg->s_size)) {

			diff = pp->p_offset - svd->offset;

			/*
			 * Large Files: Following is the assertion
			 * validating the above cast.
			 */
			ASSERT(svd->vp == pp->p_vnode);

			page = btop(diff);
			if (svd->pageprot)
				prot = VPP_PROT(&svd->vpage[page]) & vpprot;

			if (amp != NULL) {
				ap = anon_get_ptr(amp->ahp,
						svd->anon_index + page);
			}
			if ((amp == NULL) || (ap == NULL)) {
				if (enable_mbit_wa) {
					if (rw == S_WRITE)
						hat_setmod(pp);
					else if (!hat_ismod(pp))
						prot &= ~PROT_WRITE;
				}
				if (prot & PROT_READ) {
					hat_memload(hat, seg->s_base + diff,
						pp, prot, HAT_LOAD_ADV);
				}
			}
		}
		page_unlock(pp);
	}
done:
	if (serial_lock)
		mutex_exit(&amp->serial_lock);

	SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
	if (pl_alloc_sz)
		kmem_free(plp, pl_alloc_sz);
	return (0);
}

/*
 * This routine is used to start I/O on pages asynchronously.
 */
static faultcode_t
segvn_faulta(struct seg *seg, caddr_t addr)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	int err;
	struct anon_map *amp;		/* XXX - for locknest */

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);
	if ((amp = svd->amp) != NULL) {
		struct anon *ap;

		if ((ap = anon_get_ptr(amp->ahp,
			svd->anon_index + seg_page(seg, addr))) != NULL) {

			err = anon_getpage(&ap, NULL, NULL,
			    0, seg, addr, S_READ, svd->cred);
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			if (err)
				return (FC_MAKE_ERR(err));
			return (0);
		}
	}

	if (svd->vp == NULL) {
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		return (0);			/* zfod page - do nothing now */
	}

	TRACE_3(TR_FAC_VM, TR_SEGVN_GETPAGE,
		"segvn_getpage:seg %p addr %p vp %p", seg, addr, svd->vp);
	err = VOP_GETPAGE(svd->vp,
	    (offset_t)(svd->offset + (addr - seg->s_base)),
	    PAGESIZE, NULL, NULL, 0, seg, addr,
	    S_OTHER, svd->cred);

	SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
	if (err)
		return (FC_MAKE_ERR(err));
	return (0);
}

static int
segvn_setprot(struct seg *seg, caddr_t addr, size_t len, uint_t prot)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	struct vpage *svp, *evp;
	struct vnode *vp;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	if ((svd->maxprot & prot) != prot)
		return (EACCES);			/* violated maxprot */

	SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_WRITER);

	/*
	 * Since we change protections we first have to flush the cache.
	 * This makes sure all the pagelock calls have to recheck
	 * protections.
	 */
	if (svd->softlockcnt > 0) {
		/*
		 * Since we do have the segvn writers lock nobody can fill
		 * the cache with entries belonging to this seg during
		 * the purge. The flush either succeeds or we still have
		 * pending I/Os.
		 */
		segvn_purge(seg);
		if (svd->softlockcnt > 0) {
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			return (EAGAIN);
		}
	}

	/*
	 * If it's a private mapping and we're making it writable
	 * and no swap space has been reserved, have to reserve
	 * it all now.  If it's a private mapping to a file (i.e., vp != NULL)
	 * and we're removing write permission on the entire segment and
	 * we haven't modified any pages, we can release the swap space.
	 */
	if (svd->type == MAP_PRIVATE) {
		if (prot & PROT_WRITE) {
			if (svd->swresv == 0 && !(svd->flags & MAP_NORESERVE)) {
				if (anon_resv(seg->s_size) == 0) {
					SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
					return (IE_NOMEM);
				}
				svd->swresv = seg->s_size;
				TRACE_5(TR_FAC_VM, TR_ANON_PROC,
					"anon proc:%u %p %lx %lu %u",
					curproc->p_pid, seg, seg->s_base,
					seg->s_size, 1);
			}
		} else {
			/*
			 * Swap space is released only if this segment
			 * does not map anonymous memory, since read faults
			 * on such segments still need an anon slot to read
			 * in the data.
			 */
			if (svd->swresv != 0 && svd->vp != NULL &&
			    svd->amp == NULL && addr == seg->s_base &&
			    len == seg->s_size && svd->pageprot == 0) {
				anon_unresv(svd->swresv);
				svd->swresv = 0;
				TRACE_5(TR_FAC_VM, TR_ANON_PROC,
					"anon proc:%u %p %lu %lu %u",
					curproc->p_pid, seg, seg->s_base,
					svd->swresv, 0);
			}
		}
	}

	if (addr == seg->s_base && len == seg->s_size && svd->pageprot == 0) {
		if (svd->prot == prot) {
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			return (0);			/* all done */
		}
		svd->prot = (uchar_t)prot;
	} else {
		struct anon *ap = NULL;
		page_t *pp;
		u_offset_t offset, off;
		struct anon_map *amp;		/* XXX - for locknest */
		ulong_t anon_idx = 0;

		/*
		 * A vpage structure exists or else the change does not
		 * involve the entire segment.  Establish a vpage structure
		 * if none is there.  Then, for each page in the range,
		 * adjust its individual permissions.  Note that write-
		 * enabling a MAP_PRIVATE page can affect the claims for
		 * locked down memory.  Overcommitting memory terminates
		 * the operation.
		 */
		segvn_vpage(seg);
		if ((amp = svd->amp) != NULL) {
			mutex_enter(&amp->lock);
			anon_idx = svd->anon_index + seg_page(seg, addr);
		}

		offset = svd->offset + (addr - seg->s_base);
		evp = &svd->vpage[seg_page(seg, addr + len)];

		/*
		 * See Statement at the beginning of segvn_lockop regarding
		 * the way cowcnts and lckcnts are handled.
		 */
		for (svp = &svd->vpage[seg_page(seg, addr)]; svp < evp; svp++) {

			if (amp != NULL)
				ap = anon_get_ptr(amp->ahp, anon_idx++);

			if (VPP_ISPPLOCK(svp) && (VPP_PROT(svp) != prot) &&
			    (svd->type == MAP_PRIVATE)) {

				if (amp == NULL || ap == NULL) {
					vp = svd->vp;
					off = offset;
				} else
					swap_xlate(ap, &vp, &off);

				if ((pp = page_lookup(vp, off,
				    SE_SHARED)) == NULL)
					cmn_err(CE_PANIC,
					    "segvn_setprot: no page");
				if ((VPP_PROT(svp) ^ prot) & PROT_WRITE) {
					if (prot & PROT_WRITE) {
						if (!page_addclaim(pp)) {
							page_unlock(pp);
							break;
						}
					} else {
						if (!page_subclaim(pp)) {
							page_unlock(pp);
							break;
						}
					}
				}
				page_unlock(pp);
			}
			VPP_SETPROT(svp, prot);
			offset += PAGESIZE;
		}
		if (amp != NULL)
			mutex_exit(&amp->lock);

		/*
		 * Did we terminate prematurely?  If so, simply unload
		 * the translations to the things we've updated so far.
		 */
		if (svp != evp) {
			len = (svp - &svd->vpage[seg_page(seg, addr)]) *
			    PAGESIZE;
			if (len != 0)
				hat_unload(seg->s_as->a_hat, addr,
				    len, HAT_UNLOAD);
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			return (IE_NOMEM);
		}
	}

	if ((prot & PROT_WRITE) != 0 || (prot & ~PROT_USER) == PROT_NONE) {
		/*
		 * Either private or shared data with write access (in
		 * which case we need to throw out all former translations
		 * so that we get the right translations set up on fault
		 * and we don't allow write access to any copy-on-write pages
		 * that might be around or to prevent write access to pages
		 * representing holes in a file), or we don't have permission
		 * to access the memory at all (in which case we have to
		 * unload any current translations that might exist).
		 */
		hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD);
	} else {
		/*
		 * A shared mapping or a private mapping in which write
		 * protection is going to be denied - just change all the
		 * protections over the range of addresses in question.
		 * segvn does not support any other attributes other
		 * than prot so we can use hat_chgattr.
		 */
		hat_chgattr(seg->s_as->a_hat, addr, len, prot);
	}
	SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
	return (0);
}

static int
segvn_checkprot(struct seg *seg, caddr_t addr, size_t len, uint_t prot)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	struct vpage *vp, *evp;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);
	/*
	 * If segment protection can be used, simply check against them.
	 */
	if (svd->pageprot == 0) {
		int err;

		err = ((svd->prot & prot) != prot) ? EACCES : 0;
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		return (err);
	}

	/*
	 * Have to check down to the vpage level.
	 */
	evp = &svd->vpage[seg_page(seg, addr + len)];
	for (vp = &svd->vpage[seg_page(seg, addr)]; vp < evp; vp++) {
		if ((VPP_PROT(vp) & prot) != prot) {
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			return (EACCES);
		}
	}
	SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
	return (0);
}

static int
segvn_getprot(struct seg *seg, caddr_t addr, size_t len, uint_t *protv)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	size_t pgno = seg_page(seg, addr + len) - seg_page(seg, addr) + 1;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	if (pgno != 0) {
		SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);
		if (svd->pageprot == 0) {
			do
				protv[--pgno] = svd->prot;
			while (pgno != 0);
		} else {
			size_t pgoff = seg_page(seg, addr);

			do {
				pgno--;
				protv[pgno] = VPP_PROT(&svd->vpage[pgno+pgoff]);
			} while (pgno != 0);
		}
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
	}
	return (0);
}

/*ARGSUSED*/
static u_offset_t
segvn_getoffset(struct seg *seg, caddr_t addr)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (svd->offset);
}

/*ARGSUSED*/
static int
segvn_gettype(struct seg *seg, caddr_t addr)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (svd->type);
}

/*ARGSUSED*/
static int
segvn_getvp(struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	*vpp = svd->vp;
	return (0);
}

/*
 * Check to see if it makes sense to do kluster/read ahead to
 * addr + delta relative to the mapping at addr.  We assume here
 * that delta is a signed PAGESIZE'd multiple (which can be negative).
 *
 * For segvn, we currently "approve" of the action if we are
 * still in the segment and it maps from the same vp/off,
 * or if the advice stored in segvn_data or vpages allows it.
 * Currently, klustering is not allowed only if MADV_RANDOM is set.
 */
static int
segvn_kluster(struct seg *seg, caddr_t addr, ssize_t delta)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	struct anon *oap, *ap;
	ssize_t pd;
	size_t page;
	struct vnode *vp1, *vp2;
	u_offset_t off1, off2;
	struct anon_map *amp;		/* XXX - for locknest */

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
	ASSERT(SEGVN_LOCK_HELD(seg->s_as, &svd->lock));

	if (addr + delta < seg->s_base ||
	    addr + delta >= (seg->s_base + seg->s_size))
		return (-1);		/* exceeded segment bounds */

	pd = delta / (ssize_t)PAGESIZE;	/* divide to preserve sign bit */
	page = seg_page(seg, addr);

	/*
	 * Check to see if either of the pages addr or addr + delta
	 * have advice set that prevents klustering (if MADV_RANDOM advice
	 * is set for entire segment, or MADV_SEQUENTIAL is set and delta
	 * is negative).
	 */
	if (svd->advice == MADV_RANDOM ||
	    svd->advice == MADV_SEQUENTIAL && delta < 0)
		return (-1);
	else if (svd->pageadvice && svd->vpage) {
		struct vpage *bvpp, *evpp;

		bvpp = &svd->vpage[page];
		evpp = &svd->vpage[page + pd];
		if (VPP_ADVICE(bvpp) == MADV_RANDOM ||
		    VPP_ADVICE(evpp) == MADV_SEQUENTIAL && delta < 0)
			return (-1);
		if (VPP_ADVICE(bvpp) != VPP_ADVICE(evpp) &&
		    VPP_ADVICE(evpp) == MADV_RANDOM)
			return (-1);
	}

	if (svd->type == MAP_SHARED)
		return (0);		/* shared mapping - all ok */

	if ((amp = svd->amp) == NULL)
		return (0);		/* off original vnode */

	page += svd->anon_index;

	mutex_enter(&amp->lock);
	oap = anon_get_ptr(svd->amp->ahp, page);
	ap = anon_get_ptr(svd->amp->ahp, page + pd);

	if ((oap == NULL && ap != NULL) || (oap != NULL && ap == NULL)) {
		mutex_exit(&amp->lock);
		return (-1);		/* one with and one without an anon */
	}

	if (oap == NULL) {		/* implies that ap == NULL */
		mutex_exit(&amp->lock);
		return (0);		/* off original vnode */
	}

	/*
	 * Now we know we have two anon pointers - check to
	 * see if they happen to be properly allocated.
	 */

	/*
	 * XXX We cheat here and don't lock the anon slots. We can't because
	 * we may have been called from the anon layer which might already
	 * have locked them. We are holding a refcnt on the slots so they
	 * can't disappear. The worst that will happen is we'll get the wrong
	 * names (vp, off) for the slots and make a poor klustering decision.
	 */
	swap_xlate(ap, &vp1, &off1);
	swap_xlate(oap, &vp2, &off2);
	mutex_exit(&amp->lock);

	if (!VOP_CMP(vp1, vp2) || off1 - off2 != delta)
		return (-1);
	return (0);
}

/*
 * Swap the pages of seg out to secondary storage, returning the
 * number of bytes of storage freed.
 *
 * The basic idea is first to unload all translations and then to call
 * VOP_PUTPAGE() for all newly-unmapped pages, to push them out to the
 * swap device.  Pages to which other segments have mappings will remain
 * mapped and won't be swapped.  Our caller (as_swapout) has already
 * performed the unloading step.
 *
 * The value returned is intended to correlate well with the process's
 * memory requirements.  However, there are some caveats:
 * 1)	When given a shared segment as argument, this routine will
 *	only succeed in swapping out pages for the last sharer of the
 *	segment.  (Previous callers will only have decremented mapping
 *	reference counts.)
 * 2)	We assume that the hat layer maintains a large enough translation
 *	cache to capture process reference patterns.
 */
static size_t
segvn_swapout(struct seg *seg)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	struct anon_map *amp;
	pgcnt_t pgcnt = 0;
	pgcnt_t npages;
	pfn_t page;
	ulong_t anon_index;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);
	/*
	 * Find pages unmapped by our caller and force them
	 * out to the virtual swap device.
	 */
	if ((amp = svd->amp) != NULL)
		anon_index = svd->anon_index;
	npages = seg->s_size >> PAGESHIFT;
	for (page = 0; page < npages; page++) {
		page_t *pp;
		struct anon *ap;
		struct vnode *vp;
		u_offset_t off;

		/*
		 * Obtain <vp, off> pair for the page, then look it up.
		 *
		 * Note that this code is willing to consider regular
		 * pages as well as anon pages.  Is this appropriate here?
		 */
		ap = NULL;
		if (amp != NULL) {
			mutex_enter(&amp->lock);
			ap = anon_get_ptr(amp->ahp, anon_index + page);
			if (ap != NULL) {
				swap_xlate(ap, &vp, &off);
			} else {
				vp = svd->vp;
				off = svd->offset + ptob(page);
			}
			mutex_exit(&amp->lock);
		} else {
			vp = svd->vp;
			off = svd->offset + ptob(page);
		}
		if (vp == NULL) {		/* untouched zfod page */
			ASSERT(ap == NULL);
			continue;
		}

		pp = page_lookup_nowait(vp, off, SE_SHARED);
		if (pp == NULL)
			continue;


		/*
		 * Examine the page to see whether it can be tossed out,
		 * keeping track of how many we've found.
		 */
		if (!page_tryupgrade(pp)) {
			/*
			 * If the page has an i/o lock and no mappings,
			 * it's very likely that the page is being
			 * written out as a result of klustering.
			 * Assume this is so and take credit for it here.
			 */
			if (!page_io_trylock(pp)) {
				if (!hat_page_is_mapped(pp))
					pgcnt++;
			} else {
				page_io_unlock(pp);
			}
			page_unlock(pp);
			continue;
		}
		ASSERT(!page_iolock_assert(pp));


		/*
		 * Skip if page is locked or has mappings.
		 * We don't need the page_struct_lock to look at lckcnt
		 * and cowcnt because the page is exclusive locked.
		 */
		if (pp->p_lckcnt != 0 || pp->p_cowcnt != 0 ||
		    hat_page_is_mapped(pp)) {
			page_unlock(pp);
			continue;
		}

		/*
		 * No longer mapped -- we can toss it out.  How
		 * we do so depends on whether or not it's dirty.
		 */
		if (hat_ismod(pp) && pp->p_vnode) {
			/*
			 * We must clean the page before it can be
			 * freed.  Setting B_FREE will cause pvn_done
			 * to free the page when the i/o completes.
			 * XXX:	This also causes it to be accounted
			 *	as a pageout instead of a swap: need
			 *	B_SWAPOUT bit to use instead of B_FREE.
			 *
			 * Hold the vnode before releasing the page lock
			 * to prevent it from being freed and re-used by
			 * some other thread.
			 */
			VN_HOLD(vp);
			page_unlock(pp);

			/*
			 * Queue all i/o requests for the pageout thread
			 * to avoid saturating the pageout devices.
			 */
			if (!queue_io_request(vp, off))
				VN_RELE(vp);
		} else {
			/*
			 * The page was clean, free it.
			 *
			 * XXX:	Can we ever encounter modified pages
			 *	with no associated vnode here?
			 */
			ASSERT(pp->p_vnode != NULL);
			/*LINTED: constant in conditional context*/
			VN_DISPOSE(pp, B_FREE, 0, kcred);
		}

		/*
		 * Credit now even if i/o is in progress.
		 */
		pgcnt++;
	}
	SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);

	/*
	 * Wakeup pageout to initiate i/o on all queued requests.
	 */
	cv_signal_pageout();
	return (ptob(pgcnt));
}

/*
 * Synchronize primary storage cache with real object in virtual memory.
 *
 * XXX - Anonymous pages should not be sync'ed out at all.
 */
static int
segvn_sync(struct seg *seg, caddr_t addr, size_t len, int attr, uint_t flags)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	struct vpage *vpp;
	page_t *pp;
	u_offset_t offset;
	struct vnode *vp;
	u_offset_t off;
	caddr_t eaddr;
	int bflags;
	int err = 0;
	int segtype;
	int pageprot;
	int prot;
	ulong_t anon_index;
	struct anon_map *amp;		/* XXX - for locknest */
	struct anon *ap;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);

	if (svd->softlockcnt > 0) {
		/*
		 * flush all pages from seg cache
		 * otherwise we may deadlock in swap_putpage
		 * for B_INVAL page (4175402).
		 */
		segvn_purge(seg);
		if (svd->softlockcnt > 0) {
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			return (EAGAIN);
		}
	}

	vpp = svd->vpage;
	offset = svd->offset + (addr - seg->s_base);
	bflags = ((flags & MS_ASYNC) ? B_ASYNC : 0) |
	    ((flags & MS_INVALIDATE) ? B_INVAL : 0);

	if (attr) {
		pageprot = attr & ~(SHARED|PRIVATE);
		segtype = (attr & SHARED) ? MAP_SHARED : MAP_PRIVATE;

		/*
		 * We are done if the segment types don't match
		 * or if we have segment level protections and
		 * they don't match.
		 */
		if (svd->type != segtype) {
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			return (0);
		}
		if (vpp == NULL) {
			if (svd->prot != pageprot) {
				SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
				return (0);
			}
			prot = svd->prot;
		} else
			vpp = &svd->vpage[seg_page(seg, addr)];

	} else if (svd->vp && svd->amp == NULL &&
	    (flags & MS_INVALIDATE) == 0) {

		/*
		 * No attributes, no anonymous pages and MS_INVALIDATE flag
		 * is not on, just use one big request.
		 */
		err = VOP_PUTPAGE(svd->vp, (offset_t)offset, len,
		    bflags, svd->cred);
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		return (err);
	}

	if ((amp = svd->amp) != NULL)
		anon_index = svd->anon_index + seg_page(seg, addr);

	for (eaddr = addr + len; addr < eaddr; addr += PAGESIZE) {
		ap = NULL;
		if (amp != NULL) {
			mutex_enter(&amp->lock);
			ap = anon_get_ptr(amp->ahp, anon_index++);
			if (ap != NULL) {
				swap_xlate(ap, &vp, &off);
			} else {
				vp = svd->vp;
				off = offset;
			}
			mutex_exit(&amp->lock);
		} else {
			vp = svd->vp;
			off = offset;
		}
		offset += PAGESIZE;

		if (vp == NULL)		/* untouched zfod page */
			continue;

		if (attr) {
			if (vpp) {
				prot = VPP_PROT(vpp);
				vpp++;
			}
			if (prot != pageprot) {
				continue;
			}
		}

		/*
		 * See if any of these pages are locked --  if so, then we
		 * will have to truncate an invalidate request at the first
		 * locked one. We don't need the page_struct_lock to test
		 * as this is only advisory; even if we acquire it someone
		 * might race in and lock the page after we unlock and before
		 * we do the PUTPAGE, then PUTPAGE simply does nothing.
		 */
		if (flags & MS_INVALIDATE) {
			if ((pp = page_lookup(vp, off, SE_SHARED)) != NULL) {
				if (pp->p_lckcnt != 0 || pp->p_cowcnt != 0) {
					page_unlock(pp);
					SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
					return (EBUSY);
				}
				page_unlock(pp);
			}
		}
		/*
		 * XXX - Should ultimately try to kluster
		 * calls to VOP_PUTPAGE() for performance.
		 */
		VN_HOLD(vp);
		err = VOP_PUTPAGE(vp, (offset_t)off, PAGESIZE,
		    bflags, svd->cred);
		VN_RELE(vp);
		if (err)
			break;
	}
	SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
	return (err);
}

/*
 * Determine if we have data corresponding to pages in the
 * primary storage virtual memory cache (i.e., "in core").
 */
static size_t
segvn_incore(struct seg *seg, caddr_t addr, size_t len, char *vec)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	struct vnode *vp;
	u_offset_t offset;
	size_t p, ep;
	int ret;
	struct vpage *vpp;
	page_t *pp;
	uint_t start;
	struct anon_map *amp;		/* XXX - for locknest */
	struct anon *ap;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);
	if (svd->amp == NULL && svd->vp == NULL) {
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		bzero(vec, btopr(len));
		return (len);	/* no anonymous pages created yet */
	}

	p = seg_page(seg, addr);
	ep = seg_page(seg, addr + len);
	vpp = (svd->vpage) ? &svd->vpage[p]: NULL;
	start = svd->vp ? 0x10 : 0;

	amp = svd->amp;
	for (; p < ep; p++, addr += PAGESIZE) {
		ret = start;
		ap = NULL;
		if (amp != NULL) {
			mutex_enter(&amp->lock);
			ap = anon_get_ptr(amp->ahp, svd->anon_index + p);
			if (ap != NULL) {
				swap_xlate(ap, &vp, &offset);
				ret |= 0x20;
			} else {
				vp = svd->vp;
				offset = svd->offset + (addr - seg->s_base);
			}
			mutex_exit(&amp->lock);
		} else {
			vp = svd->vp;
			offset = svd->offset + (addr - seg->s_base);
		}

		if (vp == NULL)
			pp = NULL;	/* untouched zfod page */
		else {
			/*
			 * Try to obtain a "shared" lock on the page
			 * without blocking.  If this fails, determine
			 * if the page is in memory.
			 */
			pp = page_lookup_nowait(vp, offset, SE_SHARED);
			if (pp == NULL)
				ret |= (page_exists(vp, offset) != NULL);
		}

		/*
		 * Don't get page_struct lock for lckcnt and cowcnt, since
		 * this is purely advisory.
		 */
		if (pp != NULL) {
			ret |= 0x1;
			if (pp->p_lckcnt)
				ret |= 0x8;
			if (pp->p_cowcnt)
				ret |= 0x4;
			page_unlock(pp);
		}
		if (vpp) {
			if (VPP_ISPPLOCK(vpp))
				ret |= 0x2;
			vpp++;
		}
		*vec++ = (char)ret;
	}
	SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
	return (len);
}

/*
 * Statement for p_cowcnts/p_lckcnts.
 *
 * p_cowcnt is updated while mlock/munlocking MAP_PRIVATE and PROT_WRITE region
 * irrespective of the following factors or anything else:
 *
 *	(1) anon slots are populated or not
 *	(2) cow is broken or not
 *	(3) refcnt on ap is 1 or greater than 1
 *
 * If it's not MAP_PRIVATE and PROT_WRITE, p_lckcnt is updated during mlock
 * and munlock.
 *
 *
 * Handling p_cowcnts/p_lckcnts during copy-on-write fault:
 *
 *	if vpage has PROT_WRITE
 *		transfer cowcnt on the oldpage -> cowcnt on the newpage
 *	else
 *		transfer lckcnt on the oldpage -> lckcnt on the newpage
 *
 *	During copy-on-write, decrement p_cowcnt on the oldpage and increment
 *	p_cowcnt on the newpage *if* the corresponding vpage has PROT_WRITE.
 *
 *	We may also break COW if softlocking on read access in the physio case.
 *	In this case, vpage may not have PROT_WRITE. So, we need to decrement
 *	p_lckcnt on the oldpage and increment p_lckcnt on the newpage *if* the
 *	vpage doesn't have PROT_WRITE.
 *
 *
 * Handling p_cowcnts/p_lckcnts during mprotect on mlocked region:
 *
 * 	If a MAP_PRIVATE region loses PROT_WRITE, we decrement p_cowcnt and
 *	increment p_lckcnt by calling page_subclaim() which takes care of
 * 	availrmem accounting and p_lckcnt overflow.
 *
 *	If a MAP_PRIVATE region gains PROT_WRITE, we decrement p_lckcnt and
 *	increment p_cowcnt by calling page_addclaim() which takes care of
 *	availrmem availability and p_cowcnt overflow.
 */

/*
 * Lock down (or unlock) pages mapped by this segment.
 */
static int
segvn_lockop(struct seg *seg, caddr_t addr, size_t len,
    int attr, int op, ulong_t *lockmap, size_t pos)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	struct vpage *vpp;
	struct vpage *evp;
	page_t *pp;
	u_offset_t offset;
	u_offset_t off;
	int segtype;
	int pageprot;
	int claim;
	struct vnode *vp;
	ulong_t anon_index;
	struct anon_map *amp;		/* XXX - for locknest */
	struct anon *ap;
	struct vattr va;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_WRITER);
	if (attr) {
		pageprot = attr & ~(SHARED|PRIVATE);
		segtype = attr & SHARED ? MAP_SHARED : MAP_PRIVATE;

		/*
		 * We are done if the segment types don't match
		 * or if we have segment level protections and
		 * they don't match.
		 */
		if (svd->type != segtype) {
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			return (0);
		}
		if (svd->pageprot == 0 && svd->prot != pageprot) {
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			return (0);
		}
	}

	/*
	 * If we're locking, then we must create a vpage structure if
	 * none exists.  If we're unlocking, then check to see if there
	 * is a vpage --  if not, then we could not have locked anything.
	 */

	if ((vpp = svd->vpage) == NULL) {
		if (op == MC_LOCK)
			segvn_vpage(seg);
		else {
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			return (0);
		}
	}

	/*
	 * The anonymous data vector (i.e., previously
	 * unreferenced mapping to swap space) can be allocated
	 * by lazily testing for its existence.
	 */
	if (op == MC_LOCK && svd->amp == NULL && svd->vp == NULL)
		svd->amp = anonmap_alloc(seg->s_size, 0);

	if ((amp = svd->amp) != NULL)
		anon_index = svd->anon_index + seg_page(seg, addr);

	offset = svd->offset + (addr - seg->s_base);
	evp = &svd->vpage[seg_page(seg, addr + len)];

	/*
	 * Loop over all pages in the range.  Process if we're locking and
	 * page has not already been locked in this mapping; or if we're
	 * unlocking and the page has been locked.
	 */
	for (vpp = &svd->vpage[seg_page(seg, addr)]; vpp < evp;
	    vpp++, pos++, addr += PAGESIZE, offset += PAGESIZE, anon_index++) {
		if ((attr == 0 || VPP_PROT(vpp) == pageprot) &&
		    ((op == MC_LOCK && !VPP_ISPPLOCK(vpp)) ||
		    (op == MC_UNLOCK && VPP_ISPPLOCK(vpp)))) {

			/*
			 * If this isn't a MAP_NORESERVE segment and
			 * we're locking, allocate anon slots if they
			 * don't exist.  The page is brought in later on.
			 */
			if (op == MC_LOCK && svd->vp == NULL &&
			    ((svd->flags & MAP_NORESERVE) == 0) &&
			    amp != NULL &&
			    ((ap = anon_get_ptr(amp->ahp, anon_index))
								== NULL)) {
				mutex_enter(&amp->serial_lock);
					if ((ap = anon_get_ptr(amp->ahp,
						anon_index)) == NULL) {
					pp = anon_zero(seg, addr, &ap,
					    svd->cred);
					if (pp == NULL) {
						mutex_exit(&amp->serial_lock);
						SEGVN_LOCK_EXIT(seg->s_as,
						    &svd->lock);
						return (ENOMEM);
					}
					mutex_enter(&amp->lock);
					ASSERT(anon_get_ptr(amp->ahp,
						anon_index) == NULL);
					(void) anon_set_ptr(amp->ahp,
						anon_index, ap, ANON_SLEEP);
					mutex_exit(&amp->lock);
					page_unlock(pp);
				}
				mutex_exit(&amp->serial_lock);
			}

			/*
			 * Get name for page, accounting for
			 * existence of private copy.
			 */
			ap = NULL;
			if (amp != NULL) {
				mutex_enter(&amp->lock);
				ap = anon_get_ptr(amp->ahp, anon_index);
				if (ap != NULL) {
					swap_xlate(ap, &vp, &off);
				} else {
					if (svd->vp == NULL &&
					    (svd->flags & MAP_NORESERVE)) {
						mutex_exit(&amp->lock);
						continue;
					}
					vp = svd->vp;
					off = offset;
				}
				mutex_exit(&amp->lock);
			} else {
				vp = svd->vp;
				off = offset;
			}

			/*
			 * Get page frame.  It's ok if the page is
			 * not available when we're unlocking, as this
			 * may simply mean that a page we locked got
			 * truncated out of existence after we locked it.
			 *
			 * Invoke VOP_GETPAGE() to obtain the page struct
			 * since we may need to read it from disk if its
			 * been paged out.
			 */
			if (op != MC_LOCK)
				pp = page_lookup(vp, off, SE_SHARED);
			else {
				page_t *pl[1 + 1];
				int error;

				ASSERT(vp != NULL);

				error = VOP_GETPAGE(vp, (offset_t)off, PAGESIZE,
				    (uint_t *)NULL, pl, PAGESIZE, seg, addr,
				    S_OTHER, svd->cred);

				/*
				 * If the error is EDEADLK then we must bounce
				 * up and drop all vm subsystem locks and then
				 * retry the operation later
				 * This behavior is a temporary measure because
				 * ufs/sds logging is badly designed and will
				 * deadlock if we don't allow this bounce to
				 * happen.  The real solution is to re-design
				 * the logging code to work properly.  See bug
				 * 4125102 for details of the problem.
				 */
				if (error == EDEADLK) {
					SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
					return (error);
				}
				/*
				 * Quit if we fail to fault in the page.  Treat
				 * the failure as an error, unless the addr
				 * is mapped beyond the end of a file.
				 */
				if (error && svd->vp) {
					va.va_mask = AT_SIZE;
					if (VOP_GETATTR(svd->vp, &va, 0,
					    svd->cred) != 0) {
						SEGVN_LOCK_EXIT(seg->s_as,
						    &svd->lock);
						return (EIO);
					}
					if (btopr(va.va_size) >=
					    btopr(off + 1)) {
						SEGVN_LOCK_EXIT(seg->s_as,
						    &svd->lock);
						return (EIO);
					}
					SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
					return (0);
				} else if (error) {
					SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
					return (EIO);
				}
				pp = pl[0];
				ASSERT(pp != NULL);
			}

			/*
			 * See Statement at the beginning of this routine.
			 *
			 * claim is always set if MAP_PRIVATE and PROT_WRITE
			 * irrespective of following factors:
			 *
			 * (1) anon slots are populated or not
			 * (2) cow is broken or not
			 * (3) refcnt on ap is 1 or greater than 1
			 *
			 * See 4140683 for details
			 */
			claim = ((VPP_PROT(vpp) & PROT_WRITE) &&
				(svd->type == MAP_PRIVATE));

			/*
			 * Perform page-level operation appropriate to
			 * operation.  If locking, undo the SOFTLOCK
			 * performed to bring the page into memory
			 * after setting the lock.  If unlocking,
			 * and no page was found, account for the claim
			 * separately.
			 */
			if (op == MC_LOCK) {
				int ret = 1;	/* Assume success */

				/*
				 * Make sure another thread didn't lock
				 * the page after we released the segment
				 * lock.
				 */
				if ((attr == 0 || VPP_PROT(vpp) == pageprot) &&
				    !VPP_ISPPLOCK(vpp)) {
					ret = page_pp_lock(pp, claim, 0);
					if (ret != 0) {
						VPP_SETPPLOCK(vpp);
						if (lockmap != (ulong_t *)NULL)
							BT_SET(lockmap, pos);
					}
				}
				page_unlock(pp);
				if (ret == 0) {
					SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
					return (EAGAIN);
				}
			} else {
				if (pp != NULL) {
					if ((attr == 0 ||
					    VPP_PROT(vpp) == pageprot) &&
					    VPP_ISPPLOCK(vpp))
						page_pp_unlock(pp, claim, 0);
					page_unlock(pp);
				}
				VPP_CLRPPLOCK(vpp);
			}
		}
	}
	SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
	return (0);
}

/*
 * Set advice from user for specified pages
 * There are 5 types of advice:
 *	MADV_NORMAL	- Normal (default) behavior (whatever that is)
 *	MADV_RANDOM	- Random page references
 *				do not allow readahead or 'klustering'
 *	MADV_SEQUENTIAL	- Sequential page references
 *				Pages previous to the one currently being
 *				accessed (determined by fault) are 'not needed'
 *				and are freed immediately
 *	MADV_WILLNEED	- Pages are likely to be used (fault ahead in mctl)
 *	MADV_DONTNEED	- Pages are not needed (synced out in mctl)
 *	MADV_FREE	- Contents can be discarded
 */
static int
segvn_advise(struct seg *seg, caddr_t addr, size_t len, uint_t behav)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	size_t page;
	int err = 0;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * In case of MADV_FREE, we won't be modifying any segment private
	 * data structures; so, we only need to grab READER's lock
	 */
	if (behav != MADV_FREE)
		SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_WRITER);
	else
		SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);

	if (behav == MADV_SEQUENTIAL) {
		/*
		 * Since we are going to unload hat mappings
		 * we first have to flush the cache. Otherwise
		 * this might lead to system panic if another
		 * thread is doing physio on the range whose
		 * mappings are unloaded by madvise(3C).
		 */
		if (svd->softlockcnt > 0) {
			/*
			 * Since we do have the segvn writers lock
			 * nobody can fill the cache with entries
			 * belonging to this seg during the purge.
			 * The flush either succeeds or we still
			 * have pending I/Os. In the later case,
			 * madvise(3C) fails.
			 */
			segvn_purge(seg);
			if (svd->softlockcnt > 0) {
				/*
				 * Since madvise(3C) is advisory and
				 * it's not part of UNIX98, madvise(3C)
				 * failure here doesn't cause any hardship.
				 * Note that we don't block in "as" layer.
				 */
				SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
				return (EAGAIN);
			}
		}
	}

	if (behav == MADV_FREE) {
		struct anon_map *amp = svd->amp;

		/*
		 * MADV_FREE is not supported for segments with
		 * underlying object; if anonmap is NULL, anon slots
		 * are not yet populated and there is nothing for
		 * us to do. As MADV_FREE is advisory, we don't
		 * return error in either case.
		 */
		if (svd->vp || amp == NULL) {
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
			return (0);
		}

		page = seg_page(seg, addr);
		mutex_enter(&amp->lock);
		anon_disclaim(amp->ahp, svd->anon_index + page, len);
		mutex_exit(&amp->lock);
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		return (0);
	}

	/*
	 * If advice is to be applied to entire segment,
	 * use advice field in seg_data structure
	 * otherwise use appropriate vpage entry.
	 */
	if ((addr == seg->s_base) && (len == seg->s_size)) {
		switch (behav) {
		case MADV_SEQUENTIAL:

			/*
			 * unloading mapping guarantees
			 * detection in segvn_fault
			 */
			hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD);
			/* FALLTHROUGH */
		case MADV_NORMAL:
		case MADV_RANDOM:
			svd->advice = (uchar_t)behav;
			svd->pageadvice = 0;
			break;
		case MADV_WILLNEED:	/* handled in memcntl */
		case MADV_DONTNEED:	/* handled in memcntl */
		case MADV_FREE:		/* handled above */
			break;
		default:
			err = EINVAL;
		}
	} else {
		page = seg_page(seg, addr);

		segvn_vpage(seg);

		switch (behav) {
			struct vpage *bvpp, *evpp;

		case MADV_SEQUENTIAL:
			hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD);
			/* FALLTHROUGH */
		case MADV_NORMAL:
		case MADV_RANDOM:
			bvpp = &svd->vpage[page];
			evpp = &svd->vpage[page + (len >> PAGESHIFT)];
			for (; bvpp < evpp; bvpp++)
				VPP_SETADVICE(bvpp, behav);
			svd->advice = MADV_NORMAL;
			break;
		case MADV_WILLNEED:	/* handled in memcntl */
		case MADV_DONTNEED:	/* handled in memcntl */
		case MADV_FREE:		/* handled above */
			break;
		default:
			err = EINVAL;
		}
	}
	SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
	return (err);
}

/*
 * Create a vpage structure for this seg.
 */
static void
segvn_vpage(struct seg *seg)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	struct vpage *vp, *evp;

	ASSERT(SEGVN_WRITE_HELD(seg->s_as, &svd->lock));

	/*
	 * If no vpage structure exists, allocate one.  Copy the protections
	 * and the advice from the segment itself to the individual pages.
	 */
	if (svd->vpage == NULL) {
		svd->pageprot = 1;
		svd->pageadvice = 1;
		svd->vpage = kmem_zalloc(seg_pages(seg) * sizeof (struct vpage),
		    KM_SLEEP);
		evp = &svd->vpage[seg_page(seg, seg->s_base + seg->s_size)];
		for (vp = svd->vpage; vp < evp; vp++) {
			VPP_SETPROT(vp, svd->prot);
			VPP_SETADVICE(vp, svd->advice);
		}
	}
}

/*
 * Dump the pages belonging to this segvn segment.
 */
static void
segvn_dump(struct seg *seg)
{
	struct segvn_data *svd;
	page_t *pp;
	struct anon_map *amp;
	ulong_t	anon_index;
	struct vnode *vp;
	u_offset_t off, offset;
	pfn_t pfn;
	pgcnt_t page, npages;
	caddr_t addr;

	npages = seg_pages(seg);
	svd = (struct segvn_data *)seg->s_data;
	vp = svd->vp;
	off = offset = svd->offset;
	addr = seg->s_base;

	if ((amp = svd->amp) != NULL)
		anon_index = svd->anon_index;

	for (page = 0; page < npages; page++, offset += PAGESIZE) {
		struct anon *ap;
		int we_own_it = 0;

		if (amp && (ap = anon_get_ptr(svd->amp->ahp, anon_index++))) {
			swap_xlate_nopanic(ap, &vp, &off);
		} else {
			vp = svd->vp;
			off = offset;
		}

		/*
		 * If pp == NULL, the page either does not exist
		 * or is exclusively locked.  So determine if it
		 * exists before searching for it.
		 */

		if ((pp = page_lookup_nowait(vp, off, SE_SHARED)))
			we_own_it = 1;
		else
			pp = page_exists(vp, off);

		if (pp) {
			pfn = page_pptonum(pp);
			dump_addpage(seg->s_as, addr, pfn);
			if (we_own_it)
				page_unlock(pp);
		}
		addr += PAGESIZE;
		dump_timeleft = dump_timeout;
	}
}

/*
 * lock/unlock anon pages over a given range. Return shadow list
 */
static int
segvn_pagelock(struct seg *seg, caddr_t addr, size_t len, struct page ***ppp,
    enum lock_type type, enum seg_rw rw)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	size_t np, npages = (len >> PAGESHIFT);
	ulong_t anon_index;
	uint_t protchk;
	struct anon_map *amp;
	struct page **pplist, **pl, *pp;
	caddr_t a;
	size_t page;

	TRACE_2(TR_FAC_PHYSIO, TR_PHYSIO_SEGVN_START,
		"segvn_pagelock: start seg %p addr %p", seg, addr);

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	if (type == L_PAGEUNLOCK) {

		/*
		 * update hat ref bits for /proc. We need to make sure
		 * that threads tracing the ref and mod bits of the
		 * address space get the right data.
		 * Note: page ref and mod bits are updated at reclaim time
		 */
		if (seg->s_as->a_vbits) {
			for (a = addr; a < addr + len; a += PAGESIZE) {
				if (rw == S_WRITE) {
					hat_setstat(seg->s_as, a,
					    PAGESIZE, P_REF | P_MOD);
				} else {
					hat_setstat(seg->s_as, a,
					    PAGESIZE, P_REF);
				}
			}
		}
		SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);
		seg_pinactive(seg, addr, len, *ppp, rw, segvn_reclaim);

		/*
		 * If someone is blocked while unmapping, we purge
		 * segment page cache and thus reclaim pplist synchronously
		 * without waiting for seg_pasync_thread. This speeds up
		 * unmapping in cases where munmap(2) is called, while
		 * raw async i/o is still in progress or where a thread
		 * exits on data fault in a multithreaded application.
		 */
		if (AS_ISUNMAPWAIT(seg->s_as) && (svd->softlockcnt > 0)) {
			segvn_purge(seg);
		}
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		TRACE_2(TR_FAC_PHYSIO, TR_PHYSIO_SEGVN_UNLOCK_END,
			"segvn_pagelock: unlock seg %p addr %p", seg, addr);
		return (0);
	} else if (type == L_PAGERECLAIM) {
		SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);
		segvn_reclaim(seg, addr, len, *ppp, rw);
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		TRACE_2(TR_FAC_PHYSIO, TR_PHYSIO_SEGVN_UNLOCK_END,
			"segvn_pagelock: reclaim seg %p addr %p", seg, addr);
		return (0);
	}

	SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_WRITER);

	/*
	 * try to find pages in segment page cache
	 */
	pplist = seg_plookup(seg, addr, len, rw);
	if (pplist != NULL) {
		segplckstat.cache_hit.value.ul++;
		*ppp = pplist;
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		TRACE_2(TR_FAC_PHYSIO, TR_PHYSIO_SEGVN_HIT_END,
			"segvn_pagelock: cache hit seg %p addr %p", seg, addr);
		return (0);
	} else {
		segplckstat.cache_miss.value.ul++;
	}

	/*
	 * for now we only support pagelock to anon memory. We've to check
	 * protections for vnode objects and call into the vnode driver.
	 * That's too much for a fast path. Let the fault entry point handle it.
	 */
	if (svd->vp != NULL) {
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		*ppp = NULL;
		TRACE_2(TR_FAC_PHYSIO, TR_PHYSIO_SEGVN_MISS_END,
		    "segvn_pagelock: mapped vnode seg %p addr %p", seg, addr);
		return (ENOTSUP);
	}

	if (rw == S_READ) {
		protchk = PROT_READ;
	} else {
		protchk = PROT_WRITE;
	}

	if (svd->pageprot == 0) {
		if ((svd->prot & protchk) == 0) {
			goto out;
		}
	} else {
		/*
		 * check page protections
		 */
		for (a = addr; a < addr + len; a += PAGESIZE) {
			struct vpage *vp;

			vp = &svd->vpage[seg_page(seg, a)];
			if ((VPP_PROT(vp) & protchk) == 0) {
				goto out;
			}
		}
	}

	amp = svd->amp;
	if (amp == NULL) {
		/*
		 * We already have the "write" lock
		 */
		svd->amp = amp = anonmap_alloc(seg->s_size, 0);
	}

	mutex_enter(&freemem_lock);
	if (availrmem < tune.t_minarmem + npages) {
		mutex_exit(&freemem_lock);
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		*ppp = NULL;
		return (ENOMEM);
	} else {
		svd->softlockcnt += npages;
		availrmem -= npages;
		segplckstat.active_pages.value.ul += npages;
	}
	mutex_exit(&freemem_lock);

	pplist = kmem_alloc(sizeof (page_t *) * npages, KM_SLEEP);
	*ppp = pl = pplist;

	page = seg_page(seg, addr);
	anon_index = svd->anon_index + page;
	mutex_enter(&amp->lock);
	for (a = addr; a < addr + len; a += PAGESIZE, anon_index++) {
		struct anon *ap;
		struct vnode *vp;
		u_offset_t off;

		ap = anon_get_ptr(amp->ahp, anon_index);
		if (ap == NULL) {
			break;
		} else {
			/*
			 * We must never use seg_pcache for COW pages
			 * because we might end up with original page still
			 * lying in seg_pcache even after private page is
			 * created. This leads to data corruption as
			 * aio_write refers to the page still in cache
			 * while all other accesses refer to the private
			 * page.
			 */
			if (ap->an_refcnt != 1) {
				break;
			}
		}
		swap_xlate(ap, &vp, &off);
		pp = page_lookup_nowait(vp, off, SE_SHARED);
		if (pp == NULL) {
			break;
		}
		*pplist++ = pp;
	}
	mutex_exit(&amp->lock);

	if (a >= addr + len) {
		(void) seg_pinsert(seg, addr, len, pl, rw, SEGP_ASYNC_FLUSH,
			segvn_reclaim);
		SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
		TRACE_2(TR_FAC_PHYSIO, TR_PHYSIO_SEGVN_FILL_END,
		    "segvn_pagelock: cache fill seg %p addr %p", seg, addr);
		return (0);
	}
	pplist = pl;
	np = ((a - addr) >> PAGESHIFT);
	while (np > (uint_t)0) {
		page_unlock(*pplist);
		np--;
		pplist++;
	}
	kmem_free(pl, sizeof (page_t *) * npages);
	mutex_enter(&freemem_lock);
	svd->softlockcnt -= npages;
	availrmem += npages;
	segplckstat.active_pages.value.ul -= npages;
	mutex_exit(&freemem_lock);
	if (svd->softlockcnt <= 0) {
		if (AS_ISUNMAPWAIT(seg->s_as)) {
			mutex_enter(&seg->s_as->a_contents);
			if (AS_ISUNMAPWAIT(seg->s_as)) {
				AS_CLRUNMAPWAIT(seg->s_as);
				cv_broadcast(&seg->s_as->a_cv);
			}
			mutex_exit(&seg->s_as->a_contents);
		}
	}
out:
	SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);
	*ppp = NULL;
	TRACE_2(TR_FAC_PHYSIO, TR_PHYSIO_SEGVN_MISS_END,
		"segvn_pagelock: cache miss seg %p addr %p", seg, addr);
	return (EFAULT);
}

/*
 * purge any cached pages in the I/O page cache
 */
static void
segvn_purge(struct seg *seg)
{
	seg_ppurge(seg);
}

static void
segvn_reclaim(struct seg *seg, caddr_t addr, size_t len, struct page **pplist,
	enum seg_rw rw)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	pgcnt_t np, npages;
	struct page **pl;

#ifdef lint
	addr = addr;
#endif

	npages = np = (len >> PAGESHIFT);
	ASSERT(npages);
	pl = pplist;

	while (np > (uint_t)0) {
		if (rw == S_WRITE) {
			hat_setrefmod(*pplist);
		} else {
			hat_setref(*pplist);
		}
		page_unlock(*pplist);
		np--;
		pplist++;
	}
	kmem_free(pl, sizeof (page_t *) * npages);

	mutex_enter(&freemem_lock);
	availrmem += npages;
	svd->softlockcnt -= npages;
	segplckstat.active_pages.value.ul -= npages;
	mutex_exit(&freemem_lock);
	if (svd->softlockcnt <= 0) {
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
 * get a memory ID for an addr in a given segment
 */
static int
segvn_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	struct segvn_data *svd = (struct segvn_data *)seg->s_data;
	struct anon 	*ap = NULL;
	ulong_t		anon_index;
	struct anon_map	*amp;

	if (svd->type == MAP_PRIVATE) {
		memidp->val[0] = (u_longlong_t)seg->s_as;
		memidp->val[1] = (u_longlong_t)addr;
		return (0);
	}

	if (svd->type == MAP_SHARED) {
		if (svd->vp) {
			memidp->val[0] = (u_longlong_t)svd->vp;
			memidp->val[1] = (u_longlong_t)svd->offset
						+ (addr - seg->s_base);
			return (0);
		} else {

			SEGVN_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);
			if ((amp = svd->amp) != NULL) {
				anon_index = svd->anon_index +
						seg_page(seg, addr);
			}
			SEGVN_LOCK_EXIT(seg->s_as, &svd->lock);

			ASSERT(amp != NULL);

			mutex_enter(&amp->serial_lock);
			ap = anon_get_ptr(amp->ahp, anon_index);
			if (ap == NULL) {
				page_t		*pp;

				pp = anon_zero(seg, addr, &ap, svd->cred);
				if (pp == NULL) {
					mutex_exit(&amp->serial_lock);
					return (ENOMEM);
				}
				mutex_enter(&amp->lock);
				ASSERT(anon_get_ptr(amp->ahp, anon_index)
								== NULL);
				(void) anon_set_ptr(amp->ahp, anon_index,
							ap, ANON_SLEEP);
				mutex_exit(&amp->lock);
				page_unlock(pp);
			}
			mutex_exit(&amp->serial_lock);

			memidp->val[0] = (u_longlong_t)ap;
			memidp->val[1] = (u_longlong_t)((long)addr
							& PAGEOFFSET);
			return (0);
		}
	}
	return (EINVAL);
}
