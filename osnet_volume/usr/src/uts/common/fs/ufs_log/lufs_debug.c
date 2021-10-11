#pragma ident	"@(#)lufs_debug.c	1.19	99/06/01 SMI"

/*
 * Copyright (c) 1992, 1993, 1994, 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifdef	DEBUG

#include <sys/systm.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/ddi.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/cmn_err.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_filio.h>
#include <sys/fs/ufs_log.h>


/*
 * DEBUG ROUTINES
 *	THESE ROUTINES ARE ONLY USED WHEN ASSERTS ARE ENABLED
 */

kmutex_t	lufs_mem_mutex;
size_t		trans_alloced;
size_t		trans_freed;
uint_t		lufs_debug;

void *
trans_alloc(size_t nb)
{
	mutex_enter(&lufs_mem_mutex);
	trans_alloced += nb;
	mutex_exit(&lufs_mem_mutex);
	return (kmem_alloc(nb, KM_SLEEP));
}

void *
trans_alloc_nosleep(size_t nb)
{
	void *	va;

	va = kmem_alloc(nb, KM_NOSLEEP);
	if (va != NULL) {
		mutex_enter(&lufs_mem_mutex);
		trans_alloced += nb;
		mutex_exit(&lufs_mem_mutex);
	}
	return (va);
}

void *
trans_zalloc(size_t nb)
{
	mutex_enter(&lufs_mem_mutex);
	trans_alloced += nb;
	mutex_exit(&lufs_mem_mutex);
	return (kmem_zalloc(nb, KM_SLEEP));
}

void *
trans_zalloc_nosleep(size_t nb)
{
	void *	va;

	va = kmem_zalloc(nb, KM_NOSLEEP);
	if (va != NULL) {
		mutex_enter(&lufs_mem_mutex);
		trans_alloced += nb;
		mutex_exit(&lufs_mem_mutex);
	}
	return (va);
}

void
trans_free(void *va, size_t nb)
{
	mutex_enter(&lufs_mem_mutex);
	trans_freed += nb;
	mutex_exit(&lufs_mem_mutex);
	kmem_free(va, nb);
}

static	kmutex_t	toptracelock;
static	int		toptraceindex;
int			toptracemax	= 1024;	/* global so it can be set */
struct toptrace {
	enum delta_type	dtyp;
	kthread_t	*thread;
	dev_t		dev;
	long		arg2;
	long		arg3;
	long long	arg1;
} *toptrace;

static void
top_trace(enum delta_type dtyp, dev_t dev, long long arg1, long arg2, long arg3)
{
	if (toptrace == NULL) {
		toptraceindex = 0;
		toptrace = (struct toptrace *)trans_zalloc(
			(size_t) (sizeof (struct toptrace) * toptracemax));
	}
	mutex_enter(&toptracelock);
	toptrace[toptraceindex].dtyp = dtyp;
	toptrace[toptraceindex].thread = curthread;
	toptrace[toptraceindex].dev = dev;
	toptrace[toptraceindex].arg1 = arg1;
	toptrace[toptraceindex].arg2 = arg2;
	toptrace[toptraceindex].arg3 = arg3;
	if (++toptraceindex == toptracemax)
		toptraceindex = 0;
	else {
		toptrace[toptraceindex].dtyp = (enum delta_type)-1;
		toptrace[toptraceindex].thread = (kthread_t *)-1;
		toptrace[toptraceindex].dev = (dev_t)-1;
		toptrace[toptraceindex].arg1 = -1;
		toptrace[toptraceindex].arg2 = -1;
	}

	mutex_exit(&toptracelock);
}

/*
 * add a range into the metadata map
 */
static void
top_mataadd(struct ufstrans *ufstrans, offset_t mof, off_t nb)
{
	ml_unit_t *ul = (ml_unit_t *)ufstrans->ut_data;

	ASSERT(ufstrans->ut_dev == ul->un_dev);
	deltamap_add(ul->un_matamap, mof, nb, 0, 0, 0);
}

/*
 * delete a range from the metadata map
 */
static void
top_matadel(struct ufstrans *ufstrans, offset_t mof, off_t nb)
{
	ml_unit_t *ul = (ml_unit_t *)ufstrans->ut_data;

	ASSERT(ufstrans->ut_dev == ul->un_dev);
	ASSERT(!matamap_overlap(ul->un_deltamap, mof, nb));
	deltamap_del(ul->un_matamap, mof, nb);
}

/*
 * clear the entries from the metadata map
 */
static void
top_mataclr(struct ufstrans *ufstrans)
{
	ml_unit_t *ul = (ml_unit_t *)ufstrans->ut_data;

	ASSERT(ufstrans->ut_dev == ul->un_dev);
	map_free_entries(ul->un_matamap);
	map_free_entries(ul->un_deltamap);
}

/*
 * stuff for maintaining per thread deltas
 */
static uint_t		topkey;

struct threadtrans {
	struct threadtrans	*next;
	uint_t			topid;
	ulong_t			esize;
	ulong_t			rsize;
	dev_t			dev;
};
static struct threadtrans	*threadtransfree;
static int			nthreadtrans;
static kmutex_t			threadtransfree_lock;

int
top_begin_debug(ml_unit_t *ul, top_t topid, ulong_t size)
{
	struct threadtrans	*tp;

	if (ul->un_debug & MT_TRACE)
		top_trace(DT_BOT, ul->un_dev,
				(long long)topid, (long)size, (long)0);

	ASSERT(curthread->t_flag & T_DONTBLOCK);

	ASSERT(tsd_get(topkey) == NULL);

	mutex_enter(&threadtransfree_lock);
	if ((tp = threadtransfree) != 0) {
		threadtransfree = tp->next;
		mutex_exit(&threadtransfree_lock);
	} else {
		nthreadtrans++;
		mutex_exit(&threadtransfree_lock);
		tp = (struct threadtrans *)trans_zalloc(
			sizeof (struct threadtrans));
	}

	tp->topid  = topid;
	tp->esize  = size;
	tp->rsize  = 0;
	tp->dev    = ul->un_dev;
	(void) tsd_set(topkey, tp);
	return (1);
}

int
top_end_debug_1(ml_unit_t *ul, mt_map_t *mtm, top_t topid, ulong_t size)
{
	struct threadtrans	*tp;

	ASSERT(curthread->t_flag & T_DONTBLOCK);

	ASSERT((tp = (struct threadtrans *)tsd_get(topkey)) != NULL);

	ASSERT((tp->dev == ul->un_dev) && (tp->topid == topid) &&
	    (tp->esize == size));

	ASSERT(((ul->un_debug & MT_SIZE) == 0) || (tp->rsize <= tp->esize));

	mtm->mtm_tops->mtm_top_num[topid]++;
	mtm->mtm_tops->mtm_top_size_etot[topid] += tp->esize;
	mtm->mtm_tops->mtm_top_size_rtot[topid] += tp->rsize;

	if (tp->rsize > mtm->mtm_tops->mtm_top_size_max[topid])
		mtm->mtm_tops->mtm_top_size_max[topid] = tp->rsize;
	if (mtm->mtm_tops->mtm_top_size_min[topid] == 0)
			mtm->mtm_tops->mtm_top_size_min[topid] =
			    tp->rsize;
	else
		if (tp->rsize < mtm->mtm_tops->mtm_top_size_min[topid])
			mtm->mtm_tops->mtm_top_size_min[topid] =
			    tp->rsize;

	if (ul->un_debug & MT_TRACE)
		top_trace(DT_EOT, ul->un_dev, (long long)topid,
		    (long)tp->rsize, (long)0);

	return (1);
}

int
top_end_debug_2(void)
{
	struct threadtrans	*tp;

	ASSERT((tp = (struct threadtrans *)tsd_get(topkey)) != NULL);

	(void) tsd_set(topkey, NULL);

	mutex_enter(&threadtransfree_lock);
	tp->next = threadtransfree;
	threadtransfree = tp;
	mutex_exit(&threadtransfree_lock);
	return (1);
}

int
top_delta_debug(
	ml_unit_t *ul,
	offset_t mof,
	off_t nb,
	delta_t dtyp)
{
	struct threadtrans	*tp;

	ASSERT(curthread->t_flag & T_DONTBLOCK);

	/*
	 * check for delta contained fully within matamap
	 */
	ASSERT((dtyp == DT_UD) || (ul->un_matamap == NULL) ||
		matamap_within(ul->un_matamap, mof, nb));

	/*
	 * check for userdata in deltamap, matamap, logmap, or udmap
	 */
	ASSERT((dtyp != DT_UD) || (ul->un_matamap == NULL) ||
		!matamap_overlap(ul->un_matamap, mof, nb));
	ASSERT((dtyp != DT_UD) ||
		!matamap_overlap(ul->un_deltamap, mof, nb));
	ASSERT((dtyp != DT_UD) ||
		!logmap_overlap(ul->un_logmap, mof, nb));
	ASSERT((dtyp != DT_UD) ||
		!logmap_overlap(ul->un_udmap, mof, nb));

	/*
	 * maintain transaction info
	 */
	if (ul->un_debug & MT_TRANSACT)
		ul->un_logmap->mtm_tops->mtm_delta_num[dtyp]++;

	/*
	 * check transaction stuff
	 */
	if (ul->un_debug & MT_TRANSACT) {
		tp = (struct threadtrans *)tsd_get(topkey);
		ASSERT(tp);
		switch (dtyp) {
		case DT_UD:
			break;
		case DT_CANCEL:
		case DT_ABZERO:
			if (!matamap_within(ul->un_deltamap, mof, nb))
				tp->rsize += sizeof (struct delta);
			break;
		default:
			if (!matamap_within(ul->un_deltamap, mof, nb))
				tp->rsize += nb + sizeof (struct delta);
			break;
		}
	} else
		return (1);

	if (ul->un_debug & MT_TRACE)
		top_trace(dtyp, ul->un_dev, mof, (long)nb, (long)0);

	return (1);
}

/*
 * called from lufs_write_strategy and top_log to check for delta stuff
 *	read current copy from trans device
 *	compare incore buf with copy
 */
int
top_check_debug(char *sva, offset_t smof, off_t snb, ml_unit_t *ul)
{
	struct buf	*bp	= NULL;
	char		*dva	= NULL;
	size_t		dnb;
	daddr_t		begbno;
	daddr_t		endbno;
	uchar_t		*sa;
	uchar_t		*da;
	int		i;
	int		nbad;

	/*
	 * range to check may not be sector aligned; make it so
	 */
	begbno = lbtodb(smof);
	endbno = lbtod(smof + snb);
	dnb = dbtob(endbno - begbno);

	/*
	 * buffer and buf header
	 */
	if ((bp = getrbuf(KM_NOSLEEP)) == NULL)
		goto out;
	if ((dva = (char *)trans_alloc_nosleep(dnb)) == NULL)
		goto out;

	/*
	 * read from metatrans device
	 */
	bp->b_blkno = begbno;
	bp->b_edev = ul->un_dev;
	bp->b_bcount = dnb;
	bp->b_un.b_addr = dva;
	bp->b_iodone = trans_not_done;
	bp->b_flags = B_KERNBUF | B_READ;

	lufs_strategy(ul, bp);

	if (trans_wait(bp))
		goto out;

	/*
	 * incore buffer should match what we read from the metatrans device
	 */
	nbad = 0;
	sa = (uchar_t *)sva;
	da = (uchar_t *)(dva + (smof & (DEV_BSIZE - 1)));
	for (i = 0; i < snb; ++i, ++sa, ++da)
		if (*sa != *da && nbad++ < 10) {
			if (nbad == 1) {
				struct timeval	tv;
				uniqtime(&tv);
				printf("++++++ top_check: %lld, %ld @%ld\n",
					smof, snb,
					tv.tv_sec);
			}
			printf("         : byte %d, %x != %x\n",
				i, *sa, *da);
		}
	if (nbad)
		printf("         : %d don't match\n", nbad);
	ASSERT(nbad == 0);
out:
	if (bp)
		freerbuf(bp);
	if (dva)
		trans_free(dva, dnb);
	return (1);
}
/*
 * called from lufs_write_strategy
 */
int
top_write_debug(ml_unit_t *ul, mapentry_t *me, offset_t mof, off_t nb)
{
	/*
	 * if this is a userdata delta
	 */
	if (me->me_dt == DT_UD) {
		while (me) {
			ASSERT(me->me_dt == DT_UD);
			nb -= me->me_nb;
			me = me->me_hash;
		}
		ASSERT(nb == 0);
	} else {
		ASSERT((ul->un_matamap == NULL) ||
			matamap_within(ul->un_matamap, mof, nb));
	}
	return (1);
}

static void
top_end_noasync(struct ufstrans *ufstrans, top_t topid, ulong_t size)
{
	int		error;

	top_end_sync(ufstrans, &error, topid, size);
}

/*ARGSUSED*/
static int
top_begin_noasync(struct ufstrans *ufstrans, top_t topid, ulong_t size,
	int try)
{
	top_begin_sync(ufstrans, topid, size);
	return (0);
}

int
top_roll_debug(ml_unit_t *ul)
{
	logmap_roll_dev(ul);
	return (1);
}

static void
threadtrans_destroy(void *voidp)
{
	struct threadtrans	*tpnext;
	struct threadtrans	*tp	= (struct threadtrans *)voidp;

	while (tp) {
		tpnext = tp->next;
		mutex_enter(&threadtransfree_lock);
		tp->next = threadtransfree;
		threadtransfree = tp;
		mutex_exit(&threadtransfree_lock);
		tp = tpnext;
	}

}

int
top_snarf_debug(
	ml_unit_t *ul,
	struct ufstransops **topsp,
	struct ufstransops *utops)
{
	struct ufstransops	*tops;

	tops = trans_zalloc(sizeof (*tops));
	bcopy(utops, tops, sizeof (*tops));
	if (ul->un_debug & MT_MATAMAP) {
		tops->trans_mataadd = top_mataadd;
		tops->trans_matadel = top_matadel;
		tops->trans_mataclr = top_mataclr;
	}
	if (ul->un_debug & MT_NOASYNC) {
		tops->trans_begin_async = top_begin_noasync;
		tops->trans_end_async = top_end_noasync;
	}
	*topsp = tops;

	return (1);
}

int
top_init_debug(void)
{
	mutex_init(&lufs_mem_mutex, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&threadtransfree_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&toptracelock, NULL, MUTEX_DEFAULT, NULL);
	tsd_create(&topkey, threadtrans_destroy);
	return (1);
}

struct topstats_link {
	struct topstats_link	*ts_next;
	dev_t			ts_dev;
	struct topstats		ts_stats;
};
struct topstats_link *topstats_anchor = NULL;

/*
 * DEBUG ROUTINES
 *	from debug portion of *_map.c
 */
/*
 * scan test support
 */
int
logmap_logscan_debug(mt_map_t *mtm, mapentry_t *age)
{
	mapentry_t	*me;
	ml_unit_t	*ul;
	off_t		head, trimroll, lof;

	/*
	 * remember location of youngest rolled delta
	 */
	mutex_enter(&mtm->mtm_mutex);
	ul = mtm->mtm_ul;
	head = ul->un_head_lof;
	trimroll = mtm->mtm_trimrlof;
	for (me = age; me; me = me->me_agenext) {
		lof = me->me_lof;
		if (trimroll == 0)
			trimroll = lof;
		if (lof >= head) {
			if (trimroll >= head && trimroll <= lof)
				trimroll = lof;
		} else {
			if (trimroll <= lof || trimroll >= head)
				trimroll = lof;
		}
	}
	mtm->mtm_trimrlof = trimroll;
	mutex_exit(&mtm->mtm_mutex);
	return (1);
}

/*
 * scan test support
 */
int
logmap_logscan_commit_debug(off_t lof, mt_map_t *mtm)
{
	off_t	oldtrimc, newtrimc, trimroll;

	trimroll = mtm->mtm_trimrlof;
	oldtrimc = mtm->mtm_trimclof;
	newtrimc = mtm->mtm_trimclof = dbtob(btod(lof));

	/*
	 * can't trim prior to transaction w/rolled delta
	 */
	if (trimroll)
		if (newtrimc >= oldtrimc) {
			if (trimroll <= newtrimc && trimroll >= oldtrimc)
				mtm->mtm_trimalof = newtrimc;
		} else {
			if (trimroll >= oldtrimc || trimroll <= newtrimc)
				mtm->mtm_trimalof = newtrimc;
		}
	return (1);
}

int
logmap_logscan_add_debug(struct delta *dp, mt_map_t *mtm)
{
	if ((dp->d_typ == DT_AB) || (dp->d_typ == DT_INODE))
		mtm->mtm_trimalof = mtm->mtm_trimclof;
	return (1);
}

/*
 * log-read after log-write
 */
int
map_check_ldl_write(ml_unit_t *ul, caddr_t va, offset_t vamof, mapentry_t *me)
{
	caddr_t		bufp;

	ASSERT(me->me_nb);
	ASSERT((me->me_flags & ME_AGE) == 0);

	/* Alloc a buf */
	bufp = trans_alloc(me->me_nb);

	/* Do the read */
	me->me_agenext = NULL;
	if (ldl_read(ul, bufp, me->me_mof, me->me_nb, me) == 0) {
		ASSERT(bcmp(bufp, va + (me->me_mof - vamof), me->me_nb) == 0);
	}

	trans_free(bufp, me->me_nb);
	return (1);
}

extern	kmutex_t	map_mutex;		/* global mutex */
extern	mapentry_t	*mapentry_free_list;	/* free map entries */
/*
 * Cleanup a map struct
 */
int
map_put_debug(mt_map_t *mtm)
{
	struct topstats_link	*tsl, **ptsl;

	if (mtm->mtm_tops == NULL)
		return (1);

	/* Don't free this, cause the next snarf will want it */
	if ((lufs_debug & MT_TRANSACT) != 0)
		return (1);

	ptsl = &topstats_anchor;
	tsl = topstats_anchor;
	while (tsl) {
		if (mtm->mtm_tops == &tsl->ts_stats) {
			mtm->mtm_tops = NULL;
			*ptsl = tsl->ts_next;
			trans_free(tsl, sizeof (*tsl));
			return (1);
		}
		ptsl = &tsl->ts_next;
		tsl = tsl->ts_next;
	}

	return (1);
}

int
map_get_debug(ml_unit_t *ul, mt_map_t *mtm)
{
	struct topstats_link	*tsl;

	if ((ul->un_debug & MT_TRANSACT) == 0)
		return (1);

	if (mtm->mtm_type != logmaptype)
		return (1);

	tsl = topstats_anchor;
	while (tsl) {
		if (tsl->ts_dev == ul->un_dev) {
			mtm->mtm_tops = &(tsl->ts_stats);
			return (1);
		}
		tsl = tsl->ts_next;
	}

	tsl = trans_zalloc(sizeof (*tsl));
	tsl->ts_dev = ul->un_dev;
	tsl->ts_next = topstats_anchor;
	topstats_anchor = tsl;
	mtm->mtm_tops = &tsl->ts_stats;
	return (1);
}

/*
 * check a map's list
 */
int
map_check_linkage(mt_map_t *mtm)
{
	int		i;
	int		hashed;
	int		nexted;
	int		preved;
	int		nfree;
	int		ncancel;
	int		nud;
	int		nsud;
	mapentry_t	*me;
	off_t		olof;
	off_t		firstlof;
	int		wrapped;

	mutex_enter(&mtm->mtm_mutex);
	/*
	 * these counters should never be negative
	 */
	ASSERT(mtm->mtm_nud >= 0);
	ASSERT(mtm->mtm_nsud >= 0);
	ASSERT(mtm->mtm_nme >= 0);

	/*
	 * verify the entries on the hash
	 */
	hashed = 0;
	nud = 0;
	nsud = 0;
	for (i = 0; i < mtm->mtm_nhash; ++i) {
		for (me = *(mtm->mtm_hash+i); me; me = me->me_hash) {
			++hashed;
			if (me->me_dt == DT_UD)
				++nud;
			if (me->me_dt == DT_SUD)
				++nsud;
			ASSERT(me->me_flags & ME_HASH);
			ASSERT((me->me_flags & (ME_FREE|ME_LIST)) == 0);
		}
	}
	ASSERT(nud == mtm->mtm_nud);
	ASSERT(nsud == mtm->mtm_nsud);
	ASSERT(hashed == mtm->mtm_nme);
	/*
	 * verify the doubly linked list of all entries
	 */
	nexted = 0;
	for (me = mtm->mtm_next; me != (mapentry_t *)mtm; me = me->me_next)
		nexted++;
	preved = 0;
	for (me = mtm->mtm_prev; me != (mapentry_t *)mtm; me = me->me_prev)
		preved++;
	ASSERT(nexted == preved);
	ASSERT(nexted == hashed);

	/*
	 * verify the cancel list
	 */
	ncancel = 0;
	for (me = mtm->mtm_cancel; me; me = me->me_cancel) {
		++ncancel;
		ASSERT(me->me_flags & ME_CANCEL);
		ASSERT((me->me_flags & ME_FREE) == 0);
		/*
		 * only userdata should be in the userdata map
		 */
		ASSERT((mtm->mtm_type != udmaptype) || (me->me_dt == DT_UD));
		ASSERT(ncancel <= 1000000);
	}
	/*
	 * verify the logmap's log offsets
	 */
	if (mtm->mtm_type == logmaptype) {
		olof = mtm->mtm_next->me_lof;
		firstlof = olof;
		wrapped = 0;
		for (me = mtm->mtm_next->me_next;
		    me != (mapentry_t *)mtm;
		    olof = me->me_lof, me = me->me_next) {

			ASSERT(me->me_lof != olof);

			if (wrapped) {
				ASSERT(me->me_lof > olof);
				ASSERT(me->me_lof < firstlof);
				continue;
			}
			if (me->me_lof < olof) {
				ASSERT(me->me_lof < firstlof);
				wrapped = 1;
				continue;
			}
			ASSERT(me->me_lof > firstlof);
		}
	}

	mutex_exit(&mtm->mtm_mutex);
	/*
	 * verify the free list
	 */
	nfree = 0;
	mutex_enter(&map_mutex);
	for (me = mapentry_free_list; me; me = me->me_hash) {
		++nfree;
		ASSERT(me->me_flags & ME_FREE);
		ASSERT((me->me_flags & ~ME_FREE) == 0);
		ASSERT(nfree <= 1000000);
	}
	mutex_exit(&map_mutex);
	return (1);
}

/*
 * check for overlap
 */
int
matamap_overlap(mt_map_t *mtm, offset_t mof, off_t nb)
{
	off_t		hnb;
	mapentry_t	*me;
	mapentry_t	**mep;

	for (hnb = 0; nb; nb -= hnb, mof += hnb) {

		hnb = MAPBLOCKSIZE - (mof & MAPBLOCKOFF);
		if (hnb > nb)
			hnb = nb;
		/*
		 * search for dup entry
		 */
		mep = MAP_HASH(mof, mtm);
		mutex_enter(&mtm->mtm_mutex);
		for (me = *mep; me; me = me->me_hash)
			if (DATAoverlapME(mof, hnb, me))
				break;
		mutex_exit(&mtm->mtm_mutex);

		/*
		 * overlap detected
		 */
		if (me)
			return (1);
	}
	return (0);
}
/*
 * check for within
 */
int
matamap_within(mt_map_t *mtm, offset_t mof, off_t nb)
{
	off_t		hnb;
	mapentry_t	*me;
	mapentry_t	**mep;
	int		scans	= 0;
	int		withins	= 0;

	for (hnb = 0; nb && scans == withins; nb -= hnb, mof += hnb) {
		scans++;

		hnb = MAPBLOCKSIZE - (mof & MAPBLOCKOFF);
		if (hnb > nb)
			hnb = nb;
		/*
		 * search for within entry
		 */
		mep = MAP_HASH(mof, mtm);
		mutex_enter(&mtm->mtm_mutex);
		for (me = *mep; me; me = me->me_hash)
			if (DATAwithinME(mof, hnb, me)) {
				withins++;
				break;
			}
		mutex_exit(&mtm->mtm_mutex);
	}
	return (scans == withins);
}

int
ldl_sethead_debug(ml_unit_t *ul)
{
	mt_map_t	*mtm	= ul->un_logmap;
	off_t		trimr	= mtm->mtm_trimrlof;
	off_t		head	= ul->un_head_lof;
	off_t		tail	= ul->un_tail_lof;

	if (head <= tail) {
		if (trimr < head || trimr >= tail)
			mtm->mtm_trimrlof = 0;
	} else {
		if (trimr >= tail && trimr < head)
			mtm->mtm_trimrlof = 0;
	}
	return (1);
}

int
lufs_initialize_debug(ml_odunit_t *ud)
{
	ud->od_debug = lufs_debug;
	return (1);
}
#else DEBUG
int lufs_dummy; /* get rid of compiler warnings */
#endif	DEBUG
