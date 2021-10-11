/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved
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
 * 	Copyright (c) 1986-1989, 1995-1997 by Sun Microsystems, Inc.
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma ident	"@(#)seg_map.c	1.100	99/12/04 SMI"
/*	From:	SVr4.0	"kernel:vm/seg_map.c	1.33"		*/

/*
 * VM - generic vnode mapping segment.
 *
 * The segmap driver is used only by the kernel to get faster (than seg_vn)
 * mappings [lower routine overhead; more persistent cache] to random
 * vnode/offsets.  Note than the kernel may (and does) use seg_vn as well.
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/vtrace.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/thread.h>
#include <sys/dumphdr.h>
#include <sys/bitmap.h>

#include <vm/seg_kmem.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/rm.h>

/*
 * Private seg op routines.
 */
static void	segmap_free(struct seg *seg);
faultcode_t segmap_fault(struct hat *hat, struct seg *seg, caddr_t addr,
			size_t len, enum fault_type type, enum seg_rw rw);
static faultcode_t segmap_faulta(struct seg *seg, caddr_t addr);
static int	segmap_checkprot(struct seg *seg, caddr_t addr, size_t len,
			u_int prot);
static int	segmap_kluster(struct seg *seg, caddr_t addr, ssize_t);
static int	segmap_getprot(struct seg *seg, caddr_t addr, size_t len,
			u_int *protv);
static u_offset_t	segmap_getoffset(struct seg *seg, caddr_t addr);
static int	segmap_gettype(struct seg *seg, caddr_t addr);
static int	segmap_getvp(struct seg *seg, caddr_t addr, struct vnode **vpp);
static void	segmap_dump(struct seg *seg);
static int	segmap_pagelock(struct seg *seg, caddr_t addr, size_t len,
			struct page ***ppp, enum lock_type type,
			enum seg_rw rw);
static void	segmap_badop(void);
static int	segmap_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp);

#define	SEGMAP_BADOP(t)	(t(*)())segmap_badop

static struct seg_ops segmap_ops = {
	SEGMAP_BADOP(int),	/* dup */
	SEGMAP_BADOP(int),	/* unmap */
	segmap_free,
	segmap_fault,
	segmap_faulta,
	SEGMAP_BADOP(int),	/* setprot */
	segmap_checkprot,
	segmap_kluster,
	SEGMAP_BADOP(size_t),	/* swapout */
	SEGMAP_BADOP(int),	/* sync */
	SEGMAP_BADOP(size_t),	/* incore */
	SEGMAP_BADOP(int),	/* lockop */
	segmap_getprot,
	segmap_getoffset,
	segmap_gettype,
	segmap_getvp,
	SEGMAP_BADOP(int),	/* advise */
	segmap_dump,
	segmap_pagelock,	/* pagelock */
	segmap_getmemid,	/* getmemid */
};

/*
 * Private segmap routines.
 */
static void	segmap_unlock(struct hat *hat, struct seg *seg, caddr_t addr,
			size_t len, enum seg_rw rw, struct smap *smp);
static void	segmap_smapadd(struct smfree *sm, struct smap *smp);
static void	segmap_smapsub(struct smfree *sm, struct smap *smp, int locked);
static struct smap *segmap_hashin(struct smap *smp, struct vnode *vp,
			u_offset_t off, int hashid);
static void	segmap_hashout(struct smap *smp);


/*
 * Statistics for segmap operations.
 *
 * No explicit locking to protect these stats.
 */
struct segmapcnt segmapcnt = {
	{ "fault",		KSTAT_DATA_ULONG },
	{ "faulta",		KSTAT_DATA_ULONG },
	{ "getmap",		KSTAT_DATA_ULONG },
	{ "get_use",		KSTAT_DATA_ULONG },
	{ "get_reclaim",	KSTAT_DATA_ULONG },
	{ "get_reuse",		KSTAT_DATA_ULONG },
	{ "get_unused",		KSTAT_DATA_ULONG },
	{ "get_nofree",		KSTAT_DATA_ULONG },
	{ "rel_async",		KSTAT_DATA_ULONG },
	{ "rel_write",		KSTAT_DATA_ULONG },
	{ "rel_free",		KSTAT_DATA_ULONG },
	{ "rel_abort",		KSTAT_DATA_ULONG },
	{ "rel_dontneed",	KSTAT_DATA_ULONG },
	{ "release",		KSTAT_DATA_ULONG },
	{ "pagecreate",		KSTAT_DATA_ULONG },
	{ "free_notfree",	KSTAT_DATA_ULONG },
	{ "free_dirty",		KSTAT_DATA_ULONG },
	{ "free",		KSTAT_DATA_ULONG }
};

kstat_named_t *segmapcnt_ptr = (kstat_named_t *)&segmapcnt;
uint_t segmapcnt_ndata = sizeof (segmapcnt) / sizeof (kstat_named_t);

/*
 * Return number of map pages in segment.
 */
#define	MAP_PAGES(seg)		((seg)->s_size >> MAXBSHIFT)

/*
 * Translate addr into smap number within segment.
 */
#define	MAP_PAGE(seg, addr)  (((addr) - (seg)->s_base) >> MAXBSHIFT)

/*
 * Translate addr in seg into struct smap pointer.
 */
#define	GET_SMAP(seg, addr)	\
	&(((struct segmap_data *)((seg)->s_data))->smd_sm[MAP_PAGE(seg, addr)])

/*
 * Bit in map (16 bit bitmap).
 */
#define	SMAP_BIT_MASK(bitindex)	(1 << ((bitindex) & 0xf))

static int colormsk = 0;
static int hwshift = 0;
static u_int hwmsk = 0;
#ifdef DEBUG
static int *colors_used;
#endif
static struct smap *smd_smap;
static struct smap **smd_hash;
static struct smfree *smd_free;
static u_long smd_hashmsk = 0;

/*
 * There are three locks in seg_map: per color free list mutexes,
 * shash_mtx[] protects (vp, off) hash chains and smap_mtx[]
 * protects individual slots of virtual addresses inside
 * segmap segment.
 *
 * The lock ordering is to get smap_mtx[] to lock down the slot
 * first then the hash lock (for hash in/out (vp, off) list) or the
 * freelist lock to put the slot back on the free list.
 *
 * The hash search is done by only holding the hash lock, when a wanted
 * slot is found, we drop the hash lock then lock the slot so there
 * is no overlapping of shash_mtx and smap_mtx. After the slot is
 * locked, we verify again if the slot is still what we are looking
 * for.
 *
 * The search for a free slot is done by holding the freelist lock,
 * then walk down the free list until one can be locked. This is
 * in reversed lock order so mutex_tryenter() is used.
 *
 * Smap_mtx[] protects all fields in smap structure except for
 * the link fields for hash/free lists which are protected by
 * shash_mtx[] and free list lock.
 */
#if NCPU <= 4
#define	NSHASH_MTX	(64)
#else
#define	NSHASH_MTX	(64)
#endif

static kmutex_t shash_mtx[NSHASH_MTX];
#define	SHASHMTX(hashid)	(&shash_mtx[(hashid) & (NSHASH_MTX - 1)])

/*
 * Global smap mutexs protect individual smap structures.
 */
#if NCPU <= 4
#define	NSMAP_MTX	(64)
#else
#define	NSMAP_MTX	(64)
#endif

static kmutex_t smap_mtx[NSMAP_MTX];

#define	SMP2SMF(smp)		(&smd_free[(smp - smd_smap) & colormsk])

#define	SMAPMTX(smp) \
	(&smap_mtx[((smp) - smd_smap) & (NSMAP_MTX - 1)])

#define	SMAP_HASHFUNC(vp, off) \
	((((uintptr_t)(vp) >> 6) + ((uintptr_t)(vp) >> 3) + \
		((off) >> MAXBSHIFT)) & smd_hashmsk)


int
segmap_create(struct seg *seg, void *argsp)
{
	struct segmap_data *smd;
	struct smap *smp;
	struct smfree *sm;
	struct segmap_crargs *a = (struct segmap_crargs *)argsp;
	long i;
	size_t hashsz;

	ASSERT(seg->s_as && RW_WRITE_HELD(&seg->s_as->a_lock));

	if (((uintptr_t)seg->s_base | seg->s_size) & MAXBOFFSET)
		cmn_err(CE_PANIC, "segkmap not MAXBSIZE aligned");

	i = MAP_PAGES(seg);

	smd = kmem_zalloc(sizeof (struct segmap_data), KM_SLEEP);

	seg->s_data = (void *)smd;
	seg->s_ops = &segmap_ops;

	/*
	 * Allocate the smap strucutres now, for each virtual color desired
	 * a separate color structure is allocate for managing all objects
	 * of that color.  Each color has its own hash array and is locked
	 * separately.
	 */
	smd->smd_prot = a->prot;
	smd_smap = smd->smd_sm =
	    kmem_zalloc(sizeof (struct smap) * i, KM_SLEEP);

	/*
	 * Make sure nfreelist is of power of 2 and >= 1.
	 */
	if (a->nfreelist < 1)
		cmn_err(CE_PANIC, "no nfreelist %d", a->nfreelist);

	if (a->nfreelist & (a->nfreelist - 1)) {
		/* round down nfreelist to the next power of two. */
		a->nfreelist = 1 << (highbit(a->nfreelist)-1);
	}

	/*
	 * See how many free list we should maintain.
	 */
	if (a->shmsize) {
		hwshift = highbit(a->shmsize) - 1;
		hwmsk = a->shmsize - 1;
		smd->smd_nfree = ((a->shmsize) / MAXBSIZE) * a->nfreelist;
	} else {
		smd->smd_nfree = a->nfreelist;
	}

	colormsk = smd->smd_nfree - 1;
	if (smd->smd_nfree & (smd->smd_nfree - 1)) {
		/* ncolor is not power of 2 ? */
		cmn_err(CE_PANIC, "bad ncolor %d", smd->smd_nfree);
	}

	smd_free = smd->smd_free =
	    kmem_zalloc(smd->smd_nfree * sizeof (struct smfree), KM_SLEEP);

	/*
	 * Compute hash size rounding down to the next power of two.
	 */
	hashsz = (MAP_PAGES(seg)) / SMAP_HASHAVELEN;

	hashsz = 1 << (highbit(hashsz)-1);
	smd_hashmsk = hashsz - 1;

	smd_hash = smd->smd_hash =
	    kmem_zalloc(hashsz * sizeof (smd->smd_hash[0]), KM_SLEEP);

	for (i = 0; i < smd->smd_nfree; i++) {
		sm = &smd->smd_free[i];
		mutex_init(&sm->sm_mtx, NULL, MUTEX_DEFAULT, NULL);
	}

	for (i = 0; i < NSMAP_MTX; i++) {
		mutex_init(&smap_mtx[i], NULL, MUTEX_DEFAULT, NULL);
	}

	for (i = 0; i < NSHASH_MTX; i++) {
		mutex_init(&shash_mtx[i], NULL, MUTEX_DEFAULT, NULL);
	}

	/*
	 * Link all slots onto the appropriate colored freelist.
	 */
	for (smp = &smd->smd_sm[MAP_PAGES(seg) - 1];
	    smp >= smd->smd_sm; smp--) {
		kmutex_t *smtx;

		smtx = SMAPMTX(smp);
		mutex_enter(smtx);
		segmap_smapadd(SMP2SMF(smp), smp);
		mutex_exit(smtx);
	}

#ifdef DEBUG
	/*
	 * Keep track of which colors are used more often.
	 */
	colors_used = kmem_zalloc((colormsk+1) * sizeof (int), KM_SLEEP);
#endif DEBUG

	return (0);
}

static void
segmap_free(seg)
	struct seg *seg;
{
	ASSERT(seg->s_as && RW_WRITE_HELD(&seg->s_as->a_lock));
}

/*
 * Do a F_SOFTUNLOCK call over the range requested.
 * The range must have already been F_SOFTLOCK'ed.
 */
static void
segmap_unlock(
	struct hat *hat,
	struct seg *seg,
	caddr_t addr,
	size_t len,
	enum seg_rw rw,
	struct smap *smp)
{
	page_t *pp;
	caddr_t adr;
	u_offset_t off;
	struct vnode *vp;
	kmutex_t *smtx;

	ASSERT(smp->sm_refcnt > 0);

#ifdef lint
	seg = seg;
#endif

	vp = smp->sm_vp;
	off = smp->sm_off + (u_offset_t)((uintptr_t)addr & MAXBOFFSET);

	hat_unlock(hat, addr, roundup(len, PAGESIZE));
	for (adr = addr; adr < addr + len; adr += PAGESIZE, off += PAGESIZE) {
		u_short bitmask;

		/*
		 * Use page_find() instead of page_lookup() to
		 * find the page since we know that it has
		 * "shared" lock.
		 */
		pp = page_find(vp, off);
		if (pp == NULL)
			cmn_err(CE_PANIC, "segmap_unlock");
		if (rw == S_WRITE) {
			hat_setrefmod(pp);
		} else if (rw != S_OTHER) {
			TRACE_3(TR_FAC_VM, TR_SEGMAP_FAULT,
				"segmap_fault:pp %p vp %p offset %llx",
				pp, vp, off);
			hat_setref(pp);
		}

		/*
		 * Clear bitmap, if the bit corresponding to "off" is set,
		 * since the page and translation are being unlocked.
		 */
		bitmask = SMAP_BIT_MASK((off - smp->sm_off) / PAGESIZE);
		/*
		 * Large Files: Following assertion is to verify
		 * the correctness of the cast to (int) above.
		 */
		ASSERT((u_offset_t)(off - smp->sm_off) <= INT_MAX);
		smtx = SMAPMTX(smp);
		mutex_enter(smtx);
		if (smp->sm_bitmap & bitmask) {
			smp->sm_bitmap &= ~bitmask;
		}
		mutex_exit(smtx);

		page_unlock(pp);
	}
}

#define	MAXPPB	(MAXBSIZE/4096)	/* assumes minimum page size of 4k */

/*
 * This routine is called via a machine specific fault handling
 * routine.  It is also called by software routines wishing to
 * lock or unlock a range of addresses.
 */
faultcode_t
segmap_fault(
	struct hat *hat,
	struct seg *seg,
	caddr_t addr,
	size_t len,
	enum fault_type type,
	enum seg_rw rw)
{
	struct segmap_data *smd = (struct segmap_data *)seg->s_data;
	struct smap *smp;
	page_t *pp, **ppp;
	struct vnode *vp;
	u_offset_t off;
	page_t *pl[MAXPPB + 1];
	u_int prot;
	u_offset_t addroff;
	caddr_t adr;
	int err;
	u_offset_t sm_off;

	segmapcnt.smp_fault.value.ul++;
	smp = GET_SMAP(seg, addr);
	vp = smp->sm_vp;
	sm_off = smp->sm_off;

	if (vp == NULL)
		return (FC_MAKE_ERR(EIO));

	ASSERT(smp->sm_refcnt > 0);

	addroff = (u_offset_t)((uintptr_t)addr & MAXBOFFSET);
	if (addroff + len > MAXBSIZE)
		cmn_err(CE_PANIC, "segmap_fault length");
	off = sm_off + addroff;

	/*
	 * First handle the easy stuff
	 */
	if (type == F_SOFTUNLOCK) {
		segmap_unlock(hat, seg, addr, len, rw, smp);
		return (0);
	}

	TRACE_3(TR_FAC_VM, TR_SEGMAP_GETPAGE,
		"segmap_getpage:seg %p addr %p vp %p", seg, addr, vp);
	err = VOP_GETPAGE(vp, (offset_t)off, len, &prot, pl, MAXBSIZE,
	    seg, addr, rw, CRED());

	if (err)
		return (FC_MAKE_ERR(err));

	prot &= smd->smd_prot;

	/*
	 * Handle all pages returned in the pl[] array.
	 * This loop is coded on the assumption that if
	 * there was no error from the VOP_GETPAGE routine,
	 * that the page list returned will contain all the
	 * needed pages for the vp from [off..off + len].
	 */
	ppp = pl;
	while ((pp = *ppp++) != NULL) {
		ASSERT(pp->p_vnode == vp);
		/*
		 * Verify that the pages returned are within the range
		 * of this segmap region.  Note that it is theoretically
		 * possible for pages outside this range to be returned,
		 * but it is not very likely.  If we cannot use the
		 * page here, just release it and go on to the next one.
		 */
		if (pp->p_offset < sm_off ||
		    pp->p_offset >= sm_off + MAXBSIZE) {
			(void) page_release(pp, 1);
			continue;
		}

		adr = addr + (pp->p_offset - off);
		if (adr >= addr && adr < addr + len) {
			hat_setref(pp);
			TRACE_3(TR_FAC_VM, TR_SEGMAP_FAULT,
				"segmap_fault:pp %p vp %p offset %llx",
				pp, pp->p_vnode, pp->p_offset);
			if (type == F_SOFTLOCK) {
				/*
				 * Load up the translation keeping it
				 * locked and don't unlock the page.
				 */
				ASSERT(hat == kas.a_hat);
				hat_memload(hat, adr, pp, prot,
				    HAT_LOAD_LOCK);
				continue;
			}
		}
		/*
		 * Either it was a page outside the fault range or a
		 * page inside the fault range for a non F_SOFTLOCK -
		 * load up the hat translation and release the page lock.
		 */
		ASSERT(hat == kas.a_hat);
		hat_memload(hat, adr, pp, prot, HAT_LOAD);
		page_unlock(pp);
	}
	return (0);
}

/*
 * This routine is used to start I/O on pages asynchronously.
 */
static faultcode_t
segmap_faulta(struct seg *seg, caddr_t addr)
{
	struct smap *smp;
	struct vnode *vp;
	u_offset_t off;
	int err;

	segmapcnt.smp_faulta.value.ul++;
	smp = GET_SMAP(seg, addr);

	ASSERT(smp->sm_refcnt > 0);

	vp = smp->sm_vp;
	off = smp->sm_off;

	if (vp == NULL) {
		cmn_err(CE_WARN, "segmap_faulta - no vp");
		return (FC_MAKE_ERR(EIO));
	}

	TRACE_3(TR_FAC_VM, TR_SEGMAP_GETPAGE,
		"segmap_getpage:seg %p addr %p vp %p", seg, addr, vp);

	err = VOP_GETPAGE(vp, (offset_t)(off + ((offset_t)((uintptr_t)addr
	    & MAXBOFFSET))), PAGESIZE, (u_int *)NULL, (page_t **)NULL, 0,
	    seg, addr, S_READ, CRED());

	if (err)
		return (FC_MAKE_ERR(err));
	return (0);
}

/*ARGSUSED*/
static int
segmap_checkprot(struct seg *seg, caddr_t addr, size_t len, u_int prot)
{
	struct segmap_data *smd = (struct segmap_data *)seg->s_data;

	ASSERT(seg->s_as && RW_LOCK_HELD(&seg->s_as->a_lock));

	/*
	 * Need not acquire the segment lock since
	 * "smd_prot" is a read-only field.
	 */
	return (((smd->smd_prot & prot) != prot) ? EACCES : 0);
}

static int
segmap_getprot(struct seg *seg, caddr_t addr, size_t len, u_int *protv)
{
	struct segmap_data *smd = (struct segmap_data *)seg->s_data;
	size_t pgno = seg_page(seg, addr + len) - seg_page(seg, addr) + 1;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	if (pgno != 0) {
		do
			protv[--pgno] = smd->smd_prot;
		while (pgno != 0);
	}
	return (0);
}

/*ARGSUSED*/
static u_offset_t
segmap_getoffset(struct seg *seg, caddr_t addr)
{
	struct segmap_data *smd = (struct segmap_data *)seg->s_data;

	ASSERT(seg->s_as && RW_READ_HELD(&seg->s_as->a_lock));

	/* XXX - this doesn't make any sense */
	return ((u_offset_t)smd->smd_sm->sm_off);
}

/*ARGSUSED*/
static int
segmap_gettype(struct seg *seg, caddr_t addr)
{
	ASSERT(seg->s_as && RW_READ_HELD(&seg->s_as->a_lock));

	return (MAP_SHARED);
}

/*ARGSUSED*/
static int
segmap_getvp(struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	struct segmap_data *smd = (struct segmap_data *)seg->s_data;

	ASSERT(seg->s_as && RW_READ_HELD(&seg->s_as->a_lock));

	/* XXX - This doesn't make any sense */
	*vpp = smd->smd_sm->sm_vp;
	return (0);
}

/*
 * Check to see if it makes sense to do kluster/read ahead to
 * addr + delta relative to the mapping at addr.  We assume here
 * that delta is a signed PAGESIZE'd multiple (which can be negative).
 *
 * For segmap we always "approve" of this action from our standpoint.
 */
/*ARGSUSED*/
static int
segmap_kluster(struct seg *seg, caddr_t addr, ssize_t delta)
{
	return (0);
}

static void
segmap_badop()
{
	cmn_err(CE_PANIC, "segmap_badop");
	/*NOTREACHED*/
}

/*
 * Special private segmap operations
 */

/*
 * Add smp to the free list.  If the smp still has a vnode
 * association with it, then it is added to the end of the free list,
 * otherwise it is added to the front of the list.
 */
static void
segmap_smapadd(struct smfree *sm, struct smap *smp)
{
	struct smap *smpfreelist;

	ASSERT(MUTEX_HELD(SMAPMTX(smp)));

	mutex_enter(&sm->sm_mtx);
	if (smp->sm_refcnt != 0)
		cmn_err(CE_PANIC, "segmap_smapadd");

	smpfreelist = sm->sm_free;
	if (smpfreelist == 0) {
		smp->sm_next = smp->sm_prev = smp;
	} else {
		smp->sm_next = smpfreelist;
		smp->sm_prev = smpfreelist->sm_prev;
		smpfreelist->sm_prev = smp;
		smp->sm_prev->sm_next = smp;
	}

	if (smp->sm_vp == (struct vnode *)NULL)
		sm->sm_free = smp;
	else
		sm->sm_free = smp->sm_next;

	if (sm->sm_want) {
		cv_signal(&sm->sm_free_cv);
	}
	mutex_exit(&sm->sm_mtx);
}

/*
 * Remove smp from the free list.  The caller is responsible
 * for deleting old mappings, if any, in effect before using it.
 *
 * 'locked' indicates if the free list has already been locked.
 */
static void
segmap_smapsub(struct smfree *sm, struct smap *smp, int locked)
{
	struct smap *smpfreelist;

	ASSERT(MUTEX_HELD(SMAPMTX(smp)));

	if (!locked) {
		mutex_enter(&sm->sm_mtx);
	}

	smpfreelist = sm->sm_free;
	if (smpfreelist == smp)
		sm->sm_free = smp->sm_next;	/* go to next page */

	if (sm->sm_free == smp)
		sm->sm_free = NULL;		/* smp list is gone */
	else {
		smp->sm_prev->sm_next = smp->sm_next;
		smp->sm_next->sm_prev = smp->sm_prev;
	}
	smp->sm_prev = smp->sm_next = smp;	/* make smp a list of one */

	if (!locked) {
		mutex_exit(&sm->sm_mtx);
	}
}

static struct smap *
segmap_hashin(struct smap *smp, struct vnode *vp, u_offset_t off, int hashid)
{
	struct smap **hpp;
	struct smap *tmp;
	kmutex_t *hmtx;

	ASSERT(MUTEX_HELD(SMAPMTX(smp)));
	ASSERT(smp->sm_vp == NULL);
	ASSERT(smp->sm_hash == NULL);
	ASSERT(smp->sm_prev == smp);
	ASSERT(smp->sm_next == smp);
	ASSERT(hashid >= 0 && hashid <= smd_hashmsk);

	hmtx = SHASHMTX(hashid);

	mutex_enter(hmtx);
	/*
	 * First we need to verify that no one has created a smp
	 * with (vp,off) as its tag before we us.
	 */
	for (tmp = smd_hash[hashid]; tmp != NULL; tmp = tmp->sm_hash) {
		if (tmp->sm_vp == vp && tmp->sm_off == off) {
			break;
		}
	}

	if (tmp == NULL) {
		/*
		 * No one created one yet.
		 *
		 * Funniness here - we don't increment the ref count on the
		 * vnode * even though we have another pointer to it here.
		 * The reason for this is that we don't want the fact that
		 * a seg_map entry somewhere refers to a vnode to prevent the
		 * vnode * itself from going away.  This is because this
		 * reference to the vnode is a "soft one".  In the case where
		 * a mapping is being used by a rdwr [or directory routine?]
		 * there already has to be a non-zero ref count on the vnode.
		 * In the case where the vp has been freed and the the smap
		 * structure is on the free list, there are no pages in memory
		 * that can refer to the vnode.  Thus even if we reuse the same
		 * vnode/smap structure for a vnode which has the same
		 * address but represents a different object, we are ok.
		 */
		smp->sm_vp = vp;
		smp->sm_off = off;

		hpp = &smd_hash[hashid];
		smp->sm_hash = *hpp;
		*hpp = smp;
	}
	mutex_exit(hmtx);

	return (tmp);
}

static void
segmap_hashout(struct smap *smp)
{
	struct smap **hpp, *hp;
	struct vnode *vp;
	kmutex_t *mtx;
	int hashid;
	u_offset_t off;

	ASSERT(MUTEX_HELD(SMAPMTX(smp)));

	vp = smp->sm_vp;
	off = smp->sm_off;

	hashid = SMAP_HASHFUNC(vp, off);
	mtx = SHASHMTX(hashid);
	mutex_enter(mtx);

	hpp = &smd_hash[hashid];
	for (;;) {
		hp = *hpp;
		if (hp == NULL)
			cmn_err(CE_PANIC, "segmap_hashout");
		if (hp == smp)
			break;
		hpp = &hp->sm_hash;
	}

	*hpp = smp->sm_hash;
	smp->sm_hash = NULL;
	mutex_exit(mtx);

	smp->sm_vp = NULL;
	smp->sm_off = (u_offset_t)0;

}

/*
 * Attempt to free unmodified, unmapped, and non locked segmap
 * pages.
 */
void
segmap_pagefree(struct vnode *vp, u_offset_t off)
{
	u_offset_t pgoff;
	page_t  *pp;

	for (pgoff = off; pgoff < off + MAXBSIZE; pgoff += PAGESIZE) {

		if ((pp = page_lookup_nowait(vp, pgoff, SE_EXCL)) == NULL) {
			continue;
		}

		switch (page_release(pp, 1)) {
		case PGREL_NOTREL:
			segmapcnt.smp_free_notfree.value.ul++;
			break;
		case PGREL_MOD:
			segmapcnt.smp_free_dirty.value.ul++;
			break;
		case PGREL_CLEAN:
			segmapcnt.smp_free.value.ul++;
			break;
		}
	}
}


static struct smap *
get_free_smp(int color, kmutex_t **plock)
{
	struct smfree *sm;
	kmutex_t *fmtx, *smtx;
	struct smap *smp, *start;

	sm = &smd_free[color];
	fmtx = &sm->sm_mtx;

	mutex_enter(fmtx);
again:
	/*
	 * Allocate a new slot and set it up.
	 */

	smp = sm->sm_free;
	if (smp == NULL) {
		/* The freelist is empty. */
		goto no_free;
	}

	start = smp;
	do {
		/*
		 * Lock the smp.
		 */
		smtx = SMAPMTX(smp);
		if (mutex_tryenter(smtx)) {

			ASSERT(smp->sm_refcnt == 0);

			/* get it off free list. */
			segmap_smapsub(sm, smp, 1);
			mutex_exit(fmtx);

			if (smp->sm_vp != (struct vnode *)NULL) {
				struct vnode    *vp = smp->sm_vp;
				u_offset_t off  = smp->sm_off;
				/*
				 * Destroy old vnode association and
				 * unload any hardware translations to
				 * the old object.
				 */
				segmapcnt.smp_get_reuse.value.ul++;
				segmap_hashout(smp);

				/*
				 * This node is off freelist and hashlist,
				 * no one should be able to find it. Drop
				 * smtx for less hold time.
				 *
				 * XXXX worth it??
				 */
				mutex_exit(smtx);
				hat_unload(kas.a_hat, segkmap->s_base +
				    ((smp - smd_smap) * MAXBSIZE), MAXBSIZE,
				    HAT_UNLOAD);
				segmap_pagefree(vp, off);
				mutex_enter(smtx);
			}

			/* return smp locked. */
			ASSERT(SMAPMTX(smp) == smtx);
			ASSERT(MUTEX_HELD(smtx));

			*plock = smtx;
			return (smp);
		} else {
			/*
			 * Try the next one.
			 */
			smp = smp->sm_next;
		}
	} while (smp != start);

no_free:

	/* Nothing on the freelist. */
	segmapcnt.smp_get_nofree.value.ul++;
	sm->sm_want++;
	cv_wait(&sm->sm_free_cv, fmtx);
	sm->sm_want--;
	goto again;
}

/*
 * Special public segmap operations
 */

/*
 * Create pages (without using VOP_GETPAGE) and load up tranlations to them.
 * If softlock is TRUE, then set things up so that it looks like a call
 * to segmap_fault with F_SOFTLOCK.
 *
 * Returns 1, if a page is created by calling page_create_va(), or 0 otherwise.
 *
 * All fields in the generic segment (struct seg) are considered to be
 * read-only for "segmap" even though the kernel address space (kas) may
 * not be locked, hence no lock is needed to access them.
 */
int
segmap_pagecreate(struct seg *seg, caddr_t addr, size_t len, int softlock)
{
	struct segmap_data *smd = (struct segmap_data *)seg->s_data;
	page_t *pp;
	u_offset_t off;
	struct smap *smp;
	struct vnode *vp;
	caddr_t eaddr;
	int newpage = 0;
	u_int prot;
	kmutex_t *smtx;

	ASSERT(seg->s_as == &kas);

	segmapcnt.smp_pagecreate.value.ul++;

	eaddr = addr + len;
	addr = (caddr_t)((uintptr_t)addr & PAGEMASK);

	smp = GET_SMAP(seg, addr);

	/*
	 * We don't grab smp mutex here since we assume the smp
	 * has a refcnt set already which prevents the slot from
	 * changing its id.
	 */
	ASSERT(smp->sm_refcnt > 0);

	vp = smp->sm_vp;
	off = smp->sm_off + ((u_offset_t)((uintptr_t)addr & MAXBOFFSET));
	prot = smd->smd_prot;

	for (; addr < eaddr; addr += PAGESIZE, off += PAGESIZE) {
		pp = page_lookup(vp, off, SE_SHARED);
		if (pp == NULL) {
			u_short bitindex;

			if ((pp = page_create_va(vp, off,
			    PAGESIZE, PG_WAIT, seg, addr)) == NULL) {
				cmn_err(CE_PANIC,
				    "segmap_page_create page_create");
			}
			newpage = 1;
			page_io_unlock(pp);

			/*
			 * Since pages created here do not contain valid
			 * data until the caller writes into them, the
			 * "exclusive" lock will not be dropped to prevent
			 * other users from accessing the page.  We also
			 * have to lock the translation to prevent a fault
			 * from occuring when the virtual address mapped by
			 * this page is written into.  This is necessary to
			 * avoid a deadlock since we haven't dropped the
			 * "exclusive" lock.
			 */
			bitindex = (u_short)((off - smp->sm_off) / PAGESIZE);
			/*
			 * Large Files: The following assertion is to
			 * verify the cast to int above.
			 */
			ASSERT((u_offset_t)(off - smp->sm_off) <= INT_MAX);
			smtx = SMAPMTX(smp);
			mutex_enter(smtx);
			smp->sm_bitmap |= SMAP_BIT_MASK(bitindex);
			mutex_exit(smtx);

			hat_memload(kas.a_hat, addr, pp, prot, HAT_LOAD_LOCK);
		} else {
			if (softlock) {
				hat_memload(kas.a_hat, addr, pp, prot,
				    HAT_LOAD_LOCK);
			} else {
				hat_memload(kas.a_hat, addr, pp, prot,
				    HAT_LOAD);
				page_unlock(pp);
			}
		}
		TRACE_5(TR_FAC_VM, TR_SEGMAP_PAGECREATE,
		    "segmap_pagecreate:seg %p addr %p pp %p vp %p offset %llx",
		    seg, addr, pp, vp, off);
	}

	return (newpage);
}

void
segmap_pageunlock(struct seg *seg, caddr_t addr, size_t len, enum seg_rw rw)
{
	struct smap	*smp;
	u_short		bitmask;
	page_t		*pp;
	struct	vnode	*vp;
	u_offset_t	off;
	caddr_t		eaddr;
	kmutex_t	*smtx;

	ASSERT(seg->s_as == &kas);

	eaddr = addr + len;
	addr = (caddr_t)((uintptr_t)addr & PAGEMASK);

	smp = GET_SMAP(seg, addr);
	smtx = SMAPMTX(smp);

	ASSERT(smp->sm_refcnt > 0);

	vp = smp->sm_vp;
	off = smp->sm_off + ((u_offset_t)((uintptr_t)addr & MAXBOFFSET));

	for (; addr < eaddr; addr += PAGESIZE, off += PAGESIZE) {
		bitmask = SMAP_BIT_MASK((int)(off - smp->sm_off) / PAGESIZE);

		/*
		 * Large Files: Following assertion is to verify
		 * the correctness of the cast to (int) above.
		 */
		ASSERT((u_offset_t)(off - smp->sm_off) <= INT_MAX);

		/*
		 * If the bit corresponding to "off" is set,
		 * clear this bit in the bitmap, unlock translations,
		 * and release the "exclusive" lock on the page.
		 */
		if (smp->sm_bitmap & bitmask) {
			mutex_enter(smtx);
			smp->sm_bitmap &= ~bitmask;
			mutex_exit(smtx);

			hat_unlock(kas.a_hat, addr, PAGESIZE);

			/*
			 * Use page_find() instead of page_lookup() to
			 * find the page since we know that it has
			 * "exclusive" lock.
			 */
			pp = page_find(vp, off);
			if (pp == NULL)
				cmn_err(CE_PANIC, "segmap_pageunlock");
			if (rw == S_WRITE) {
				hat_setrefmod(pp);
			} else if (rw != S_OTHER) {
				hat_setref(pp);
			}

			page_unlock(pp);
		}
	}
}

caddr_t
segmap_getmap(struct seg *seg, struct vnode *vp, u_offset_t off)
{
	return (segmap_getmapflt(seg, vp, off, MAXBSIZE, 0, S_OTHER));
}

/*
 * This is the magic virtual address that offset 0 of an ELF
 * file gets mapped to in user space. This is used to pick
 * the vac color on the freelist.
 */
#define	ELF_OFFZERO_VA	(0x10000)
/*
 * segmap_getmap allocates a MAXBSIZE big slot to map the vnode vp
 * in the range <off, off + len). off doesn't need to be MAXBSIZE aligned.
 * The return address is  always MAXBSIZE aligned.
 *
 * If forcefault is nonzero and the MMU translations haven't yet been created,
 * segmap_getmap will call segmap_fault(..., F_INVAL, rw) to create them.
 */
caddr_t
segmap_getmapflt(
	struct seg *seg,
	struct vnode *vp,
	u_offset_t off,
	size_t len,
	int forcefault,
	enum seg_rw rw)
{
	struct smap *smp, *nsmp;
	extern struct vnode *common_specvp();
	caddr_t baseaddr;			/* MAXBSIZE aligned */
	u_offset_t baseoff;
	int newslot;
	caddr_t vaddr;
	int color, hashid;
	kmutex_t *hashmtx, *smapmtx;

	ASSERT(seg->s_as == &kas);
	ASSERT(seg == segkmap);

	baseoff = off & (offset_t)MAXBMASK;
	if (off + len > baseoff + MAXBSIZE)
		cmn_err(CE_PANIC, "segmap_getmap bad len");

	/*
	 * If this is a block device we have to be sure to use the
	 * "common" block device vnode for the mapping.
	 */
	if (vp->v_type == VBLK)
		vp = common_specvp(vp);

	hashid = SMAP_HASHFUNC(vp, baseoff);
	hashmtx = SHASHMTX(hashid);

retry_hash:
	mutex_enter(hashmtx);
	segmapcnt.smp_getmap.value.ul++;

	for (smp = smd_hash[hashid]; smp != NULL; smp = smp->sm_hash) {
		if (smp->sm_vp == vp && smp->sm_off == baseoff) {
			break;
		}
	}
	mutex_exit(hashmtx);

vrfy_smp:
	if (smp != NULL) {

		ASSERT(vp->v_count != 0);

		/*
		 * Get smap lock and recheck its tag. The hash lock
		 * is dropped since the hash is based on (vp, off)
		 * and (vp, off) won't change when we have smap mtx.
		 */
		smapmtx = SMAPMTX(smp);
		mutex_enter(smapmtx);
		if (smp->sm_vp != vp || smp->sm_off != baseoff) {
			mutex_exit(smapmtx);
			goto retry_hash;
		}

		if (smp->sm_refcnt == 0) {
			segmapcnt.smp_get_reclaim.value.ul++;
			/*
			 * Must be on the free list. Get it off
			 * free list.
			 */
			segmap_smapsub(SMP2SMF(smp), smp, 0);
		} else {
			segmapcnt.smp_get_use.value.ul++;
		}
		smp->sm_refcnt++;		/* another user */
		mutex_exit(smapmtx);

		newslot = 0;
	} else {

		if (hwshift == 0) {
			/*
			 * PAC machine- pick any freelist, just make
			 * sure we use each freelist evenly.
			 */
			color = ((((uintptr_t)vp << 2) + baseoff) >> MAXBSHIFT)
			    & colormsk;
		} else {
			/*
			 * VAC machine- pick color by offset in the file
			 * so we won't get VAC conflicts on elf files.
			 * On data files, color does not matter but we
			 * don't know what kind of file it is so we always
			 * pick color by offset. This of course causes
			 * color corresponding to ELF_OFFZERO_VA to be
			 * used more heavily.
			 */
			color = (((((uintptr_t)vp << (hwshift - 6)) & ~hwmsk) +
			    (CPU->cpu_id << hwshift) +
			    (baseoff + ELF_OFFZERO_VA)) >> MAXBSHIFT) &
			    colormsk;

		}

#ifdef DEBUG
		colors_used[color]++;
#endif DEBUG

		/*
		 * Get a locked smp slot from the free list.
		 */
		smp = get_free_smp(color, &smapmtx);

		ASSERT(smp->sm_vp == NULL);

		if ((nsmp = segmap_hashin(smp, vp, baseoff, hashid))
		    != NULL) {
			/*
			 * Failed to hashin, there exists one now.
			 * Return the smp we just allocated.
			 */
			segmap_smapadd(SMP2SMF(smp), smp);
			mutex_exit(smapmtx);

			smp = nsmp;
			goto vrfy_smp;
		}
		smp->sm_refcnt++;		/* another user */
		mutex_exit(smapmtx);

		newslot = 1;
	}

	baseaddr = seg->s_base + ((smp - smd_smap) * MAXBSIZE);
	TRACE_4(TR_FAC_VM, TR_SEGMAP_GETMAP,
	    "segmap_getmap:seg %p addr %p vp %p offset %llx",
	    seg, baseaddr, vp, baseoff);

	/*
	 * Prefault the translations
	 */
	vaddr = baseaddr + (off - baseoff);
	if (forcefault &&
	    (newslot || !hat_probe(kas.a_hat, vaddr))) {

		caddr_t pgaddr = (caddr_t)((uintptr_t)vaddr & PAGEMASK);

		(void) segmap_fault(kas.a_hat, seg, pgaddr,
		    (vaddr + len - pgaddr + PAGESIZE - 1) & PAGEMASK,
		    F_INVAL, rw);
	}

	return (baseaddr);
}

int
segmap_release(struct seg *seg, caddr_t addr, u_int flags)
{
	struct smap *smp;
	int 		error;
	int		bflags = 0;
	struct vnode	*vp;
	u_offset_t offset;
	kmutex_t	*smtx;

	if (addr < seg->s_base || addr >= seg->s_base + seg->s_size ||
	    ((uintptr_t)addr & MAXBOFFSET) != 0)
		cmn_err(CE_PANIC, "segmap_release addr");

	smp = GET_SMAP(seg, addr);
	TRACE_3(TR_FAC_VM, TR_SEGMAP_RELMAP,
		"segmap_relmap:seg %p addr %p refcnt %d",
		seg, addr, smp->sm_refcnt);

	smtx = SMAPMTX(smp);
	mutex_enter(smtx);

	ASSERT(smp->sm_refcnt > 0);

	/*
	 * Need to call VOP_PUTPAGE() if any flags (except SM_DONTNEED)
	 * are set.
	 */
	if ((flags & ~SM_DONTNEED) != 0) {
		if (flags & SM_WRITE)
			segmapcnt.smp_rel_write.value.ul++;
		if (flags & SM_ASYNC) {
			bflags |= B_ASYNC;
			segmapcnt.smp_rel_async.value.ul++;
		}
		if (flags & SM_INVAL) {
			bflags |= B_INVAL;
			segmapcnt.smp_rel_abort.value.ul++;
		}
		if (smp->sm_refcnt == 1) {
			/*
			 * We only bother doing the FREE and DONTNEED flags
			 * if no one else is still referencing this mapping.
			 */
			if (flags & SM_FREE) {
				bflags |= B_FREE;
				segmapcnt.smp_rel_free.value.ul++;
			}
			if (flags & SM_DONTNEED) {
				bflags |= B_DONTNEED;
				segmapcnt.smp_rel_dontneed.value.ul++;
			}
		}
	} else {
		segmapcnt.smp_release.value.ul++;
	}

	vp = smp->sm_vp;
	offset = smp->sm_off;

	if (--smp->sm_refcnt == 0) {
		if (flags & SM_INVAL) {
			segmap_hashout(smp);	/* remove map info */

			/*
			 * The "smp_mtx" can now be dropped since
			 * the smap entry is no longer hashed and is
			 * also not on the free list.
			 */
			mutex_exit(smtx);
			hat_unload(kas.a_hat, addr, MAXBSIZE, HAT_UNLOAD);
			mutex_enter(smtx);
		}

		segmap_smapadd(SMP2SMF(smp), smp);	/* add to free list */
	}
	mutex_exit(smtx);

	/*
	 * Now invoke VOP_PUTPAGE() if any flags (except SM_DONTNEED)
	 * are set.
	 */
	if ((flags & ~SM_DONTNEED) != 0) {
		error = VOP_PUTPAGE(vp, offset, MAXBSIZE,
		    bflags, CRED());
	} else {
		error = 0;
	}

	return (error);
}

/*
 * Dump the pages belonging to this segmap segment.
 */
static void
segmap_dump(struct seg *seg)
{
	struct segmap_data *smd;
	struct smap *smp, *smp_end;
	page_t *pp;
	u_int pfn;
	u_offset_t off;
	caddr_t addr;

	smd = (struct segmap_data *)seg->s_data;
	addr = seg->s_base;
	for (smp = smd->smd_sm, smp_end = smp + MAP_PAGES(segkmap);
	    smp < smp_end; smp++) {

		if (smp->sm_refcnt) {
			for (off = 0; off < MAXBSIZE; off += PAGESIZE) {
				int we_own_it = 0;

				/*
				 * If pp == NULL, the page either does
				 * not exist or is exclusively locked.
				 * So determine if it exists before
				 * searching for it.
				 */
				if ((pp = page_lookup_nowait(smp->sm_vp,
				    smp->sm_off + off, SE_SHARED)))
					we_own_it = 1;
				else
					pp = page_exists(smp->sm_vp,
					    smp->sm_off + off);

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
	}
}

/*ARGSUSED*/
static int
segmap_pagelock(struct seg *seg, caddr_t addr, size_t len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

static int
segmap_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	struct segmap_data *smd = (struct segmap_data *)seg->s_data;

	memidp->val[0] = (u_longlong_t)smd->smd_sm->sm_vp;
	memidp->val[1] = (u_longlong_t)(smd->smd_sm->sm_off +
						(addr - seg->s_base));
	return (0);
}
