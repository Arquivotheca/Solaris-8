
#pragma ident "@(#)lufs_map.c	1.35	99/12/06 SMI"

/*
 * Copyright (c) 1992, 1993, 1994, 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <sys/systm.h>
#include <sys/types.h>
#include <sys/vnode.h>
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
#include <sys/fs/ufs_bio.h>
#include <sys/inttypes.h>
#include <sys/taskq.h>

/*
 * externs
 */
extern pri_t minclsyspri;
extern struct kmem_cache *lufs_bp;

/*
 * globals
 */
kmutex_t	map_mutex;		/* global mutex */
mapentry_t	*mapentry_free_list;	/* free map entries */

/*
 * forwards
 */
static	void	logmap_ud_cancel(ml_unit_t *ul);

/*
 * COMMON MAPENTRY ROUTINES
 */
/*
 * allocate a mapentry
 */
static mapentry_t *
mapentry_alloc(mt_map_t *mtm)
{
	mapentry_t	*me;

	ASSERT((mtm == NULL) || MUTEX_HELD(&mtm->mtm_mutex));

	/*
	 * fastest case: get an entry off the global free list
	 */
	mutex_enter(&map_mutex);
	if ((me = mapentry_free_list) != NULL) {
		mapentry_free_list = me->me_hash;
		me->me_flags = 0;
		mutex_exit(&map_mutex);
		return (me);
	}
	mutex_exit(&map_mutex);

	/*
	 * faster case: kmem_alloc an entry
	 */
	if (mtm == NULL)
		return ((mapentry_t *)trans_zalloc(sizeof (mapentry_t)));

	/*
	 * slowest case: rel/acq the map lock if necessary
	 */
	me = (mapentry_t *)trans_zalloc_nosleep(sizeof (mapentry_t));
	if (me == NULL) {
		mutex_exit(&mtm->mtm_mutex);
		me = (mapentry_t *)trans_zalloc(sizeof (mapentry_t));
		mutex_enter(&mtm->mtm_mutex);
	}
	return (me);
}

static void
mapentry_free(mapentry_t *me)
{
	ASSERT(!(me->me_flags & (ME_FREE|ME_HASH|ME_CANCEL|ME_AGE|ME_LIST)));

	me->me_flags = ME_FREE;
	mutex_enter(&map_mutex);
	me->me_hash = mapentry_free_list;
	mapentry_free_list = me;
	mutex_exit(&map_mutex);
}

/*
 * GENERIC MAP ROUTINES
 */
/*
 * free up all the mapentries for a map
 */
void
map_free_entries(mt_map_t *mtm)
{
	int		i;
	mapentry_t	*me;

	while ((me = mtm->mtm_next) != (mapentry_t *)mtm) {
		me->me_next->me_prev = me->me_prev;
		me->me_prev->me_next = me->me_next;
		me->me_flags = 0;
		mapentry_free(me);
	}
	for (i = 0; i < mtm->mtm_nhash; i++)
		mtm->mtm_hash[i] = NULL;
	mtm->mtm_nme = 0;
	mtm->mtm_nmet = 0;
	mtm->mtm_nsud = 0;
}
/*
 * done with map; free if necessary
 */
mt_map_t *
map_put(mt_map_t *mtm)
{
	/*
	 * free up the map's memory
	 */
	map_free_entries(mtm);
	ASSERT(map_put_debug(mtm));
	trans_free(mtm->mtm_hash,
		(size_t) (sizeof (mapentry_t *) * mtm->mtm_nhash));
	mutex_destroy(&mtm->mtm_mutex);
	mutex_destroy(&mtm->mtm_scan_mutex);
	cv_destroy(&mtm->mtm_cv);
	rw_destroy(&mtm->mtm_rwlock);
	mutex_destroy(&mtm->mtm_lock);
	cv_destroy(&mtm->mtm_cv_commit);
	cv_destroy(&mtm->mtm_cv_next);
	cv_destroy(&mtm->mtm_cv_eot);
	trans_free(mtm, sizeof (mt_map_t));

	return (NULL);
}
/*
 * Allocate a map;
 */
mt_map_t *
map_get(ml_unit_t *ul, enum maptypes maptype, int nh)
{
	mt_map_t	*mtm;

	/*
	 * assume the map is not here and allocate the necessary structs
	 */
	mtm = (mt_map_t *)trans_zalloc(sizeof (mt_map_t));
	mutex_init(&mtm->mtm_mutex, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&mtm->mtm_scan_mutex, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&mtm->mtm_cv, NULL, CV_DEFAULT, NULL);
	rw_init(&mtm->mtm_rwlock, NULL, RW_DEFAULT, NULL);
	mtm->mtm_next = (mapentry_t *)mtm;
	mtm->mtm_prev = (mapentry_t *)mtm;
	mtm->mtm_hash = (mapentry_t **)trans_zalloc(
					(size_t) (sizeof (mapentry_t *) * nh));
	mtm->mtm_nhash = nh;
	mtm->mtm_debug = ul->un_debug;
	mtm->mtm_type = maptype;

	/*
	 * for scan test
	 */
	mtm->mtm_ul = ul;

	/*
	 * Initialize moby transaction stuff
	 */
	mtm->mtm_tid = UINT32_C(0);
	mtm->mtm_committid = UINT32_C(0);
	mtm->mtm_closed = 0;
	mtm->mtm_wantin = 0;
	mtm->mtm_active = 0;
	mtm->mtm_activesync = 0;
	mutex_init(&mtm->mtm_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&mtm->mtm_cv_commit, NULL, CV_DEFAULT, NULL);
	cv_init(&mtm->mtm_cv_next, NULL, CV_DEFAULT, NULL);
	cv_init(&mtm->mtm_cv_eot, NULL, CV_DEFAULT, NULL);
	ASSERT(map_get_debug(ul, mtm));

	return (mtm);
}

/*
 * DELTAMAP ROUTINES
 */
/*
 * deltamap tuning constants
 */
long	deltamap_maxnme	= 1024;	/* global so it can be set */

int
deltamap_need_commit(mt_map_t *mtm)
{
	return (mtm->mtm_nme > deltamap_maxnme);
}

/*
 * put a delta into a deltamap; may sleep on memory
 */
void
deltamap_add(
	mt_map_t *mtm,
	offset_t mof,
	off_t nb,
	delta_t dtyp,
	int (*func)(),
	ulong_t arg)
{
	int32_t		hnb;
	mapentry_t	*me;
	mapentry_t	**mep;

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	mutex_enter(&mtm->mtm_mutex);
	for (hnb = 0; nb; nb -= hnb, mof += hnb) {
		hnb = MAPBLOCKSIZE - (mof & MAPBLOCKOFF);
		if (hnb > nb)
			hnb = nb;
		/*
		 * search for dup entry
		 */
		mep = MAP_HASH(mof, mtm);
		for (me = *mep; me; me = me->me_hash) {
			if (DATAwithinME(mof, hnb, me)) {
				break;
			}
			ASSERT((dtyp == DT_CANCEL) ||
				(!DATAoverlapME(mof, hnb, me)) ||
				MEwithinDATA(me, mof, hnb));
		}

		/*
		 * already in map
		 */
		if (me)
			continue;

		/*
		 * get a mapentry
		 *	might rel/acq the mtm_mutex
		 */
		me = mapentry_alloc(mtm);

		/*
		 * initialize and put in deltamap
		 */
		me->me_mof = mof;
		me->me_nb = hnb;
		me->me_func = func;
		me->me_arg = arg;
		me->me_dt = dtyp;
		me->me_flags |= ME_HASH;
		me->me_tid = mtm->mtm_tid;

		me->me_hash = *mep;
		*mep = me;
		me->me_next = (mapentry_t *)mtm;
		me->me_prev = mtm->mtm_prev;
		mtm->mtm_prev->me_next = me;
		mtm->mtm_prev = me;
		if (me->me_dt == DT_UD)
			mtm->mtm_nud++;
		mtm->mtm_nme++;
	}
	mutex_exit(&mtm->mtm_mutex);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));
}

/*
 * remove deltas within (mof, nb) and return as linked list
 */
mapentry_t *
deltamap_remove(mt_map_t *mtm, offset_t mof, off_t nb)
{
	off_t		hnb;
	mapentry_t	*me;
	mapentry_t	**mep;
	mapentry_t	*mer;

	if (mtm == NULL)
		return (NULL);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	mutex_enter(&mtm->mtm_mutex);
	for (mer = NULL, hnb = 0; nb; nb -= hnb, mof += hnb) {
		hnb = MAPBLOCKSIZE - (mof & MAPBLOCKOFF);
		if (hnb > nb)
			hnb = nb;
		/*
		 * remove entries from hash and return as a aged linked list
		 */
		mep = MAP_HASH(mof, mtm);
		while ((me = *mep) != 0) {
			if (MEwithinDATA(me, mof, hnb)) {
				*mep = me->me_hash;
				me->me_next->me_prev = me->me_prev;
				me->me_prev->me_next = me->me_next;
				me->me_hash = mer;
				mer = me;
				me->me_flags |= ME_LIST;
				me->me_flags &= ~ME_HASH;
				if (me->me_dt == DT_UD)
					mtm->mtm_nud--;
				mtm->mtm_nme--;
			} else
				mep = &me->me_hash;
		}
	}
	mutex_exit(&mtm->mtm_mutex);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	return (mer);
}

/*
 * delete entries within (mof, nb)
 */
void
deltamap_del(mt_map_t *mtm, offset_t mof, off_t nb)
{
	mapentry_t	*me;
	mapentry_t	*menext;

	menext = deltamap_remove(mtm, mof, nb);
	while ((me = menext) != 0) {
		menext = me->me_hash;
		me->me_flags &= ~ME_LIST;
		mapentry_free(me);
	}
}

/*
 * call the indicated function to cause deltas to move to the logmap
 */
void
deltamap_push(ml_unit_t *ul)
{
	delta_t		dtyp;
	int		(*func)();
	ulong_t		arg;
	mapentry_t	*me;
	offset_t	mof;
	off_t		nb;
	mt_map_t	*mtm	= ul->un_deltamap;

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	/*
	 * for every entry in the deltamap
	 */
	mutex_enter(&mtm->mtm_mutex);
	while ((me = mtm->mtm_next) != (mapentry_t *)mtm) {
		ASSERT(me->me_func);
		func = me->me_func;
		dtyp = me->me_dt;
		arg = me->me_arg;
		mof = me->me_mof;
		nb = me->me_nb;
		mutex_exit(&mtm->mtm_mutex);
		if (func == NULL)
			ldl_seterror(ul, "Internal ufs log error");
		if ((ul->un_flags & LDL_ERROR) || (*func)(ul->un_ut, dtyp, arg))
			deltamap_del(mtm, mof, nb);
		mutex_enter(&mtm->mtm_mutex);
	}
	mutex_exit(&mtm->mtm_mutex);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));
}

/*
 * LOGMAP ROUTINES
 */
/*
 * logmap tuning constants
 */
long	logmap_maxnme_commit	= 1024;
long	logmap_maxnme_async	= 2048;
long	logmap_maxnme_sync	= 3072;
long	logmap_maxnme		= 1536;

int
logmap_need_commit(mt_map_t *mtm)
{
	return (mtm->mtm_nmet > logmap_maxnme_commit);
}

int
logmap_need_roll_async(mt_map_t *mtm)
{
	return (mtm->mtm_nme > logmap_maxnme_async);
}

int
logmap_need_roll_sync(mt_map_t *mtm)
{
	return (mtm->mtm_nme > logmap_maxnme_sync);
}

int
logmap_need_roll(mt_map_t *mtm)
{
	return (mtm->mtm_nme > logmap_maxnme);
}

kmutex_t	log_tq_create_mutex;
taskq_t		*log_sync_tq;

/*
 * Create a task queue to issue sync transactions to the log.
 */
void
logmap_start_sync()
{
	mutex_enter(&log_tq_create_mutex);
	if (log_sync_tq == NULL) {
		log_sync_tq = taskq_create("log_sync_taskq", 2, minclsyspri,
			2, 4, TASKQ_PREPOPULATE);
	}
	mutex_exit(&log_tq_create_mutex);
}


void
logmap_start_roll(ml_unit_t *ul)
{
	mt_map_t	*logmap	= ul->un_logmap;

	mutex_enter(&logmap->mtm_mutex);

	if ((logmap->mtm_flags & MTM_ROLL_RUNNING) == 0) {
		logmap->mtm_flags |= MTM_ROLL_RUNNING;
		logmap->mtm_flags &= ~(MTM_FORCE_ROLL | MTM_ROLL_EXIT);
		(void) thread_create(NULL, 0, trans_roll, (caddr_t)ul,
			0, &p0, TS_RUN, minclsyspri);
	}
	mutex_exit(&logmap->mtm_mutex);
}

void
logmap_kill_roll(ml_unit_t *ul)
{
	mt_map_t	*mtm	= ul->un_logmap;
	if (mtm == NULL)
		return;

	mutex_enter(&mtm->mtm_mutex);

	while (mtm->mtm_flags & MTM_ROLL_RUNNING) {
		mtm->mtm_flags |= MTM_ROLL_EXIT;
		cv_broadcast(&mtm->mtm_cv);
		cv_wait(&mtm->mtm_cv, &mtm->mtm_mutex);
	}
	mutex_exit(&mtm->mtm_mutex);
}

void
logmap_forceroll(mt_map_t *mtm)
{
	mutex_enter(&mtm->mtm_mutex);
	/*
	 * if we do the broadcast in the `while' loop then several
	 * processes will become CPU bound inside the loop because
	 * they end up waking up each other and not giving the
	 * roll thread a chance to run
	 */
	if ((mtm->mtm_flags & MTM_FORCE_ROLL) == 0)
		cv_broadcast(&mtm->mtm_cv);
	/*
	 * wait for the roll thread to finish a cycle
	 */
	mtm->mtm_flags |= MTM_FORCE_ROLL;
	while (mtm->mtm_flags & MTM_FORCE_ROLL) {
		if ((mtm->mtm_flags & MTM_ROLL_RUNNING) == 0) {
			mtm->mtm_flags &= ~MTM_FORCE_ROLL;
			goto out;
		}
		cv_wait(&mtm->mtm_cv, &mtm->mtm_mutex);
	}
out:
	mutex_exit(&mtm->mtm_mutex);
}

/*
 * check for overlap
 */
int
logmap_overlap(mt_map_t *mtm, offset_t mof, off_t nb)
{
	off_t		hnb;
	mapentry_t	*me;
	mapentry_t	**mep;

	if (mtm == NULL)
		return (0);

	mutex_enter(&mtm->mtm_mutex);
	for (hnb = 0; nb; nb -= hnb, mof += hnb) {
		hnb = MAPBLOCKSIZE - (mof & MAPBLOCKOFF);
		if (hnb > nb)
			hnb = nb;
		/*
		 * search for dup entry
		 */
		mep = MAP_HASH(mof, mtm);
		for (me = *mep; me; me = me->me_hash) {
			if (DATAoverlapME(mof, hnb, me)) {
				/*
				 * overlap detected
				 */
				mutex_exit(&mtm->mtm_mutex);
				return (1);
			}
		}
	}
	mutex_exit(&mtm->mtm_mutex);
	return (0);
}

/*
 * remove rolled deltas within (mof, nb) and free them
 */
void
logmap_remove_roll(mt_map_t *mtm, offset_t mof, off_t nb)
{
	int		dolock = 0;
	off_t		hnb;
	mapentry_t	*me;
	mapentry_t	**mep;
	offset_t	savmof	= mof;
	off_t		savnb	= nb;

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

again:
	if (dolock)
		rw_enter(&mtm->mtm_rwlock, RW_WRITER);
	mutex_enter(&mtm->mtm_mutex);
	for (hnb = 0; nb; nb -= hnb, mof += hnb) {
		hnb = MAPBLOCKSIZE - (mof & MAPBLOCKOFF);
		if (hnb > nb)
			hnb = nb;
		/*
		 * remove and free the rolled entries
		 */
		mep = MAP_HASH(mof, mtm);
		while ((me = *mep) != 0) {
			if ((me->me_flags & ME_ROLL) &&
			    (MEwithinDATA(me, mof, hnb))) {
				if (me->me_flags & ME_AGE) {
					ASSERT(dolock == 0);
					dolock = 1;
					mutex_exit(&mtm->mtm_mutex);
					mof = savmof;
					nb = savnb;
					goto again;
				}
				*mep = me->me_hash;
				me->me_next->me_prev = me->me_prev;
				me->me_prev->me_next = me->me_next;
				me->me_flags &= ~(ME_HASH|ME_ROLL);
				mtm->mtm_nme--;
				/*
				 * userdata roll
				 */
				if (me->me_dt == DT_SUD) {
					ASSERT(mtm->mtm_nsud > 0);
					mtm->mtm_nsud--;
				}
				/*
				 * cancelled entries are handled by someone else
				 */
				if ((me->me_flags & ME_CANCEL) == 0)
					mapentry_free(me);
			} else
				mep = &me->me_hash;
		}
	}
	mutex_exit(&mtm->mtm_mutex);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	if (dolock)
		rw_exit(&mtm->mtm_rwlock);
}

/*
 * return args from next delta for roll
 */
int
logmap_next_roll(mt_map_t *mtm, offset_t *mofp)
{
	mapentry_t	*me;

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	/*
	 * remove rollable entry from hash and return
	 */
	mutex_enter(&mtm->mtm_mutex);
	for (me = mtm->mtm_next; me != (mapentry_t *)mtm; me = me->me_next) {
		/* already rolled */
		if (me->me_flags & ME_ROLL)
			continue;

		/* part of currently busy transaction; stop */
		if (me->me_tid == mtm->mtm_tid) {
			me = NULL;
			break;
		}

		/* part of commit-in-progress transaction; stop */
		if (me->me_tid == mtm->mtm_committid) {
			me = NULL;
			break;
		}

		/* userdata; ignore */
		if (me->me_dt == DT_UD)
			continue;

		*mofp = me->me_mof;
		break;
	}
	mutex_exit(&mtm->mtm_mutex);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	return (me && (me != (mapentry_t *)mtm));
}

/*
 * put mapentry on sorted age list
 */
static void
logmap_list_age(mapentry_t **age, mapentry_t *meadd)
{
	mapentry_t	*me;

	ASSERT(!(meadd->me_flags & (ME_AGE|ME_FREE|ME_LIST)));

	for (me = *age; me; age = &me->me_agenext, me = *age) {
		if (me->me_age > meadd->me_age)
			break;
	}
	meadd->me_agenext = me;
	meadd->me_flags |= ME_AGE;
	*age = meadd;
}

/*
 * get a list of deltas
 *	returns with mtm_rwlock held
 */
void
logmap_list_get(
	mt_map_t *mtm,
	offset_t mof,
	off_t nb,
	mapentry_t **age)
{
	off_t		hnb;
	int		busy;
	mapentry_t	*me;
	mapentry_t	**mep;
	int		rwtype	= RW_READER;
	offset_t	savmof	= mof;
	off_t		savnb	= nb;

	mtm->mtm_ref = 1;
again:

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	rw_enter(&mtm->mtm_rwlock, rwtype);
	*age = NULL;
	mutex_enter(&mtm->mtm_mutex);
	for (busy = 0, hnb = 0; nb && busy == 0; nb -= hnb, mof += hnb) {
		hnb = MAPBLOCKSIZE - (mof & MAPBLOCKOFF);
		if (hnb > nb)
			hnb = nb;
		/*
		 * find overlapping entries
		 */
		mep = MAP_HASH(mof, mtm);
		for (me = *mep; me && busy == 0; me = me->me_hash) {
			if (me->me_dt == DT_CANCEL)
				continue;
			if (!DATAoverlapME(mof, hnb, me))
				continue;
			if (me->me_flags & ME_AGE) {
				++busy;
			} else
				logmap_list_age(age, me);
		}
	}
	/*
	 * if mapentry is busy
	 *	unlist the deltas, upgrade the lock, and try again
	 */
	if (busy) {
		for (me = *age; me; me = *age) {
			*age = me->me_agenext;
			me->me_flags &= ~ME_AGE;
		}
		mutex_exit(&mtm->mtm_mutex);
		rw_exit(&mtm->mtm_rwlock);
		rwtype = RW_WRITER;
		mof = savmof;
		nb = savnb;
		goto again;
	}
	mutex_exit(&mtm->mtm_mutex);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));
	ASSERT(RW_LOCK_HELD(&mtm->mtm_rwlock));
}

/*
 * get a list of deltas for rolling
 *	returns with mtm_rwlock held
 */
void
logmap_list_get_roll(
	mt_map_t *mtm,
	offset_t mof,
	off_t nb,
	mapentry_t **age)
{
	int		busy;
	mapentry_t	*me;
	mapentry_t	**mep;
	int		rwtype	= RW_READER;
	offset_t	savmof	= mof;
	off_t		savnb	= nb;

again:

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));
	ASSERT((mof & MAPBLOCKOFF) == 0);
	ASSERT(nb <= MAPBLOCKSIZE);

	rw_enter(&mtm->mtm_rwlock, rwtype);
	*age = NULL;

	/*
	 * find overlapping entries
	 */
	mutex_enter(&mtm->mtm_mutex);
	mep = MAP_HASH(mof, mtm);
	for (busy = 0, me = *mep; me && busy == 0; me = me->me_hash) {
		if (me->me_tid == mtm->mtm_tid)
			continue;
		if (me->me_tid == mtm->mtm_committid)
			continue;
		if (me->me_dt == DT_CANCEL)
			continue;
		/* don't roll user data */
		if (me->me_dt == DT_UD)
			continue;
		if (!DATAoverlapME(mof, nb, me))
			continue;

		if (me->me_flags & ME_AGE) {
			++busy;
		} else
			logmap_list_age(age, me);
	}

	/*
	 * if mapentry is busy
	 *	unlist the deltas, upgrade the lock, and try again
	 */
	if (busy) {
		for (me = *age; me; me = *age) {
			*age = me->me_agenext;
			me->me_flags &= ~ME_AGE;
		}
		mutex_exit(&mtm->mtm_mutex);
		rw_exit(&mtm->mtm_rwlock);
		rwtype = RW_WRITER;
		mof = savmof;
		nb = savnb;
		goto again;
	}

	/*
	 * these deltas are being rolled
	 */
	for (me = *age; me; me = me->me_agenext)
		me->me_flags |= ME_ROLL;

	mutex_exit(&mtm->mtm_mutex);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	ASSERT(((mtm->mtm_debug & MT_SCAN) == 0) ||
		logmap_logscan_debug(mtm, *age));
	ASSERT(RW_LOCK_HELD(&mtm->mtm_rwlock));
}

void
logmap_list_put(mt_map_t *mtm, mapentry_t *age)
{
	mapentry_t	*me;

	ASSERT(RW_LOCK_HELD(&mtm->mtm_rwlock));
	mutex_enter(&mtm->mtm_mutex);
	for (me = age; me; me = age) {
		age = me->me_agenext;
		me->me_flags &= ~ME_AGE;
	}
	mutex_exit(&mtm->mtm_mutex);
	rw_exit(&mtm->mtm_rwlock);
}

void
logmap_read_mstr(ml_unit_t *ul, struct buf *bp)
{
	int		(*saviodone)();
	klwp_t		*lwp = ttolwp(curthread);

	saviodone = bp->b_iodone;
	bp->b_iodone = trans_not_done;
	ub.ub_mreads.value.ul++;
	(void) bdev_strategy(bp);
	if (lwp != NULL)
		lwp->lwp_ru.inblock++;
	if (trans_not_wait(bp))
		ldl_seterror(ul, "Error reading master");
	bp->b_iodone = saviodone;
}

/*
 * create a bitmap of metadata sectors in bp for the roll thread
 */
void
logmap_secmap_roll(mapentry_t *age, offset_t mof, uint16_t *secmap)
{
	ulong_t		i;
	ulong_t		begsec;
	ulong_t		endsec;
	mapentry_t	*me;

	/*
	 * create a bitmap of valid metasectors in a master block
	 * for the roll thread
	 */
	*secmap = 0;
	for (me = age; me; me = me->me_agenext) {
		begsec = lbtodb(me->me_mof - mof);
		endsec = lbtodb((me->me_mof + me->me_nb + (DEV_BSIZE - 1))
				- mof);
		for (i = begsec; i < endsec; ++i)
			*secmap |= (UINT16_C(1) << i);
	}
}

/*
 * Abort the load of a set of log map delta's.
 * ie,
 * Clear out all mapentries on this unit's log map
 * which have a tid (transaction id) equal to the
 * current tid.   Walk the cancel list, taking everything
 * off it, too.
 */
static void
logmap_abort(ml_unit_t *ul)
{
	struct mt_map	*mtm = ul->un_logmap;	/* Log map */
	mapentry_t	*me,
			**mep;
	int		i;

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	/*
	 * wait for any outstanding reads to finish; lock out future reads
	 */
	rw_enter(&mtm->mtm_rwlock, RW_WRITER);

	mutex_enter(&mtm->mtm_mutex);
	/* Take everything off cancel list */
	while ((me = mtm->mtm_cancel) != NULL) {
		mtm->mtm_cancel = me->me_cancel;
		me->me_flags &= ~ME_CANCEL;
		me->me_cancel = NULL;
	}

	/* Now take out all mapentries with current tid */
	for (i = 0; i < mtm->mtm_nhash; i++) {
		mep = &mtm->mtm_hash[i];
		while ((me = *mep) != NULL) {
			if (me->me_tid == mtm->mtm_tid) {
				*mep = me->me_hash;
				me->me_next->me_prev = me->me_prev;
				me->me_prev->me_next = me->me_next;
				me->me_flags &= ~ME_HASH;
				if (me->me_dt == DT_SUD)
					mtm->mtm_nsud--;
				if (me->me_dt == DT_UD)
					mtm->mtm_nud--;
				mtm->mtm_nme--;
				mapentry_free(me);
				continue;
			}
			mep = &me->me_hash;
		}
	}
	mutex_exit(&mtm->mtm_mutex);
	rw_exit(&mtm->mtm_rwlock);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

}

static void
logmap_wait_space(mt_map_t *mtm, ml_unit_t *ul, mapentry_t *me)
{
	ASSERT(MUTEX_HELD(&ul->un_log_mutex));

	while (!ldl_has_space(ul, me)) {
		mutex_exit(&ul->un_log_mutex);
		logmap_forceroll(mtm);
		mutex_enter(&ul->un_log_mutex);
		if (ul->un_flags & LDL_ERROR)
			break;
	}

	ASSERT(MUTEX_HELD(&ul->un_log_mutex));
}

/*
 * put a list of deltas into a logmap
 * If va == NULL, don't write to the log.
 */
void
logmap_add(
	ml_unit_t *ul,
	char *va,			/* Ptr to buf w/deltas & data */
	offset_t vamof,			/* Offset on master of buf start */
	mapentry_t *melist)		/* Entries to add */
{
	offset_t	mof;
	off_t		nb;
	mapentry_t	*me;
	mapentry_t	**mep;
	mapentry_t	**savmep;
	uint32_t	tid;
	mt_map_t	*mtm	= ul->un_logmap;

	mutex_enter(&ul->un_log_mutex);
	if (va)
		logmap_wait_space(mtm, ul, melist);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	mtm->mtm_ref = 1;
	mtm->mtm_dirty++;
	tid = mtm->mtm_tid;
	while (melist) {
		mof = melist->me_mof;
		nb  = melist->me_nb;

		/*
		 * search for overlaping entries
		 */
		savmep = mep = MAP_HASH(mof, mtm);
		mutex_enter(&mtm->mtm_mutex);
		while ((me = *mep) != 0) {
			/*
			 * data consumes old map entry; cancel map entry
			 */
			if (MEwithinDATA(me, mof, nb) &&
			    ((me->me_flags & (ME_ROLL|ME_CANCEL)) == 0)) {
				if (tid == me->me_tid &&
				    ((me->me_flags & ME_AGE) == 0)) {
					*mep = me->me_hash;
					me->me_next->me_prev = me->me_prev;
					me->me_prev->me_next = me->me_next;
					me->me_flags &= ~ME_HASH;
					if (me->me_dt == DT_SUD)
						mtm->mtm_nsud--;
					mtm->mtm_nme--;
					mapentry_free(me);
					continue;
				}
				me->me_cancel = mtm->mtm_cancel;
				mtm->mtm_cancel = me;
				me->me_flags |= ME_CANCEL;
			}
			mep = &(*mep)->me_hash;
		}
		mutex_exit(&mtm->mtm_mutex);

		/*
		 * remove from list
		 */
		me = melist;
		melist = melist->me_hash;
		me->me_flags &= ~ME_LIST;
		/*
		 * If va != NULL, put in the log.
		 */
		if (va)
			ldl_write(ul, va, vamof, me);
		if (ul->un_flags & LDL_ERROR) {
			mapentry_free(me);
			continue;
		}
		ASSERT((va == NULL) ||
			((mtm->mtm_debug & MT_LOG_WRITE_CHECK) == 0) ||
			map_check_ldl_write(ul, va, vamof, me));
		ASSERT(me->me_dt != DT_UD);

		/*
		 * put on hash
		 */
		mutex_enter(&mtm->mtm_mutex);
		me->me_hash = *savmep;
		*savmep = me;
		me->me_next = (mapentry_t *)mtm;
		me->me_prev = mtm->mtm_prev;
		mtm->mtm_prev->me_next = me;
		mtm->mtm_prev = me;
		me->me_flags |= ME_HASH;
		me->me_tid = tid;
		me->me_age = mtm->mtm_age++;
		mtm->mtm_nme++;
		mtm->mtm_nmet++;
		mutex_exit(&mtm->mtm_mutex);
	}

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));
	mutex_exit(&ul->un_log_mutex);
}

/*
 * put a list of userdata deltas into a logmap
 */
void
logmap_add_ud(
	ml_unit_t *ul,
	char *va,			/* ptr to buf w/data */
	offset_t vamof,			/* offset on master of buf start */
	mapentry_t *melist)		/* entries to add */
{
	offset_t	mof;
	mapentry_t	*me;
	mapentry_t	**mep;
	mt_map_t	*mtm	= ul->un_logmap;

	mutex_enter(&ul->un_log_mutex);
	if (va)
		logmap_wait_space(mtm, ul, melist);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	mtm->mtm_ref = 1;
	mtm->mtm_dirty++;
	while (melist) {
		mof = melist->me_mof;

		/*
		 * remove from list
		 */
		me = melist;
		melist = melist->me_hash;
		me->me_flags &= ~(ME_LIST);

		/*
		 * put in the log.
		 */
		if (va)
			ldl_write(ul, va, vamof, me);
		if (ul->un_flags & LDL_ERROR) {
			mapentry_free(me);
			continue;
		}
		ASSERT((va == NULL) ||
			((mtm->mtm_debug & MT_LOG_WRITE_CHECK) == 0) ||
			map_check_ldl_write(ul, va, vamof, me));

		/*
		 * put on hash
		 */
		mep = MAP_HASH(mof, mtm);
		mutex_enter(&mtm->mtm_mutex);
		me->me_hash = *mep;
		*mep = me;
		me->me_next = (mapentry_t *)mtm;
		me->me_prev = mtm->mtm_prev;
		mtm->mtm_prev->me_next = me;
		mtm->mtm_prev = me;
		me->me_flags |= ME_HASH;
		me->me_tid = mtm->mtm_tid;
		me->me_age = mtm->mtm_age++;
		mtm->mtm_nme++;
		mtm->mtm_nmet++;
		ASSERT(me->me_dt == DT_UD || me->me_dt == DT_SUD);
		if (me->me_dt == DT_UD)
			mtm->mtm_nud++;
		else
			mtm->mtm_nsud++;
		mutex_exit(&mtm->mtm_mutex);
	}

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));
	mutex_exit(&ul->un_log_mutex);
}

/*
 * free up any cancelled deltas
 */
static void
logmap_free_cancel(mt_map_t *mtm)
{
	int		dolock	= 0;
	mapentry_t	*me;
	mapentry_t	**mep;

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

again:
	if (dolock)
		rw_enter(&mtm->mtm_rwlock, RW_WRITER);

	/*
	 * At EOT, cancel the indicated deltas
	 */
	mutex_enter(&mtm->mtm_mutex);
	while ((me = mtm->mtm_cancel) != 0) {
		/*
		 * roll forward or read collision; wait and try again
		 */
		if (me->me_flags & ME_AGE) {
			ASSERT(dolock == 0);
			mutex_exit(&mtm->mtm_mutex);
			dolock = 1;
			goto again;
		}
		/*
		 * remove from cancel list
		 */
		mtm->mtm_cancel = me->me_cancel;
		me->me_cancel = NULL;
		me->me_flags &= ~(ME_CANCEL);

		/*
		 * logmap_remove_roll handles ME_ROLL entries later
		 *	we leave them around for logmap_iscancel
		 *	XXX is this necessary?
		 */
		if (me->me_flags & ME_ROLL)
			continue;

		/*
		 * remove from hash (if necessary)
		 */
		if (me->me_flags & ME_HASH) {
			mep = MAP_HASH(me->me_mof, mtm);
			while (*mep) {
				if (*mep == me) {
					*mep = me->me_hash;
					me->me_next->me_prev = me->me_prev;
					me->me_prev->me_next = me->me_next;
					me->me_flags &= ~(ME_HASH);
					mtm->mtm_nme--;
					break;
				} else
					mep = &(*mep)->me_hash;
			}
		}
		/*
		 * put the entry on the free list
		 */
		if (me->me_dt == DT_SUD)
			mtm->mtm_nsud--;
		mapentry_free(me);
	}
	mutex_exit(&mtm->mtm_mutex);
	if (dolock)
		rw_exit(&mtm->mtm_rwlock);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));
}

void
logmap_commit(ml_unit_t *ul)
{
	mapentry_t	me;
	mt_map_t	*mtm	= ul->un_logmap;

	/*
	 * commit (i.e., cancel) any completed userdata writes
	 */
	logmap_ud_cancel(ul);

	/*
	 * async'ly write a commit rec into the log
	 */
	if (mtm->mtm_dirty) {
		/*
		 * put commit record into log
		 */
		me.me_mof = mtm->mtm_tid;
		me.me_dt = DT_COMMIT;
		me.me_nb = 0;
		me.me_hash = NULL;
		mutex_enter(&ul->un_log_mutex);
		logmap_wait_space(mtm, ul, &me);
		ldl_write(ul, NULL, (offset_t)0, &me);
		ldl_round_commit(ul);
		/*
		 * free up any cancelled deltas
		 */
		logmap_free_cancel(mtm);
		/*
		 * abort on error; else reset dirty flag, inc tid
		 */
		if (ul->un_flags & LDL_ERROR)
			logmap_abort(ul);
		else {
			mtm->mtm_dirty = 0;
			mtm->mtm_nmet = 0;
			mtm->mtm_tid++;
		}
		/* push commit */
		ldl_push_commit(ul);
		mutex_exit(&ul->un_log_mutex);
	}
}

void
logmap_sethead(mt_map_t *mtm, ml_unit_t *ul)
{
	off_t		lof;
	uint32_t	tid;
	mapentry_t	*me;

	/*
	 * move the head forward so the log knows how full it is
	 */
	mutex_enter(&ul->un_log_mutex);
	mutex_enter(&mtm->mtm_mutex);
	if ((me = mtm->mtm_next) == (mapentry_t *)mtm)
		lof = -1;
	else {
		lof = me->me_lof;
		tid = me->me_tid;
	}
	mutex_exit(&mtm->mtm_mutex);
	ldl_sethead(ul, lof, tid);
	if (lof == -1)
		mtm->mtm_age = 0;
	mutex_exit(&ul->un_log_mutex);
}

static void
logmap_settail(mt_map_t *mtm, ml_unit_t *ul)
{
	off_t		lof;
	size_t		nb;

	/*
	 * set the tail after the logmap_abort
	 */
	mutex_enter(&ul->un_log_mutex);
	mutex_enter(&mtm->mtm_mutex);
	if (mtm->mtm_prev == (mapentry_t *)mtm)
		lof = -1;
	else {
		/*
		 * set the tail to the end of the last commit
		 */
		lof = mtm->mtm_tail_lof;
		nb = mtm->mtm_tail_nb;
	}
	mutex_exit(&mtm->mtm_mutex);
	ldl_settail(ul, lof, nb);
	mutex_exit(&ul->un_log_mutex);
}

/*
 * when reseting (metaclearing) a device; roll the log until every
 * delta for the metatrans device has been rolled forward
 */
void
logmap_roll_dev(ml_unit_t *ul)
{
	mt_map_t	*mtm	= ul->un_logmap;
	mapentry_t	*me;


again:
	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));
	if (ul->un_flags & LDL_ERROR)
		return;

	/*
	 * look for deltas for this metatrans device
	 */
	mutex_enter(&mtm->mtm_mutex);
	for (me = mtm->mtm_next; me != (mapentry_t *)mtm; me = me->me_next) {
		if (me->me_flags & ME_ROLL)
			break;
		if (me->me_tid == mtm->mtm_tid)
			continue;
		if (me->me_tid == mtm->mtm_committid)
			continue;
		break;
	}

	/*
	 * found a delta; kick the roll thread
	 * but only if the thread is running... (jmh)
	 */
	if (me != (mapentry_t *)mtm) {
		mutex_exit(&mtm->mtm_mutex);
		logmap_forceroll(mtm);
		goto again;
	}

	/*
	 * no more deltas, return
	 */
	mutex_exit(&mtm->mtm_mutex);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));
}

/*
 * if some scanned-userdata overlaps a write; roll the scanned userdata
 */
void
logmap_roll_sud(mt_map_t *mtm, ml_unit_t *ul, offset_t mof, off_t nb)
{
	while (mtm->mtm_nsud && logmap_overlap(mtm, mof, nb)) {
		logmap_forceroll(mtm);
		if (ul->un_flags & LDL_ERROR)
			break;
	}
}

/*
 * cancel userdata entries in a logmap (entries are freed at EOT)
 *
 *	CALLED AT BIODONE (I.E., at interrupt)
 *
 *	Remove mapentry from the hash so that the roll thread will not
 *	be looping waiting for the userdata to be written.  We can remove
 *	it now since the userdata is safely on the master dev (userdata is
 *	not really `transacted'.)  A cancel record will be added to the next
 *	transaction so that a future write of this data won't be overwritten
 *	during a logscan.
 */
int
logmap_ud_done(struct buf *cb)
{
	ml_unit_t	*ul	= NULL;
	offset_t	mof	= ldbtob(cb->b_blkno);
	off_t		nb	= cb->b_bcount;
	mt_map_t	*mtm	= ul->un_logmap;
	mt_map_t	*udmtm	= ul->un_udmap;
	off_t		hnb;
	mapentry_t	*me;
	mapentry_t	**mep;
	mapentry_t	**udmep;
	lufs_buf_t	*lbp;
	buf_t		*bp;

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));
	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(udmtm));

	for (hnb = 0; nb; nb -= hnb, mof += hnb) {
		/*
		 * break up the write into hashed sizes
		 */
		hnb = MAPBLOCKSIZE - (mof & MAPBLOCKOFF);
		if (hnb > nb)
			hnb = nb;
		/*
		 * scan for overlapping entries
		 */
		mep = MAP_HASH(mof, mtm);
		mutex_enter(&mtm->mtm_mutex);
		while ((me = *mep) != NULL) {
			if (DATAoverlapME(mof, hnb, me))
				break;
			mep = &me->me_hash;
		}
		if (me == NULL) {
			ASSERT(ul->un_flags & LDL_ERROR);
			mutex_exit(&mtm->mtm_mutex);
			continue;
		}
		ASSERT(me);
		ASSERT(MEwithinDATA(me, mof, hnb));
		ASSERT(me->me_dt == DT_UD);
		ASSERT(mtm->mtm_nud > 0);

		/*
		 * remove from logmap
		 */
		*mep = me->me_hash;
		me->me_next->me_prev = me->me_prev;
		me->me_prev->me_next = me->me_next;
		mtm->mtm_nud--;
		mtm->mtm_nme--;
		if (mtm->mtm_nud == 0)
			cv_broadcast(&mtm->mtm_cv);
		mutex_exit(&mtm->mtm_mutex);

		/*
		 * add to udmap
		 */
		udmep = MAP_HASH(mof, udmtm);
		mutex_enter(&udmtm->mtm_mutex);
		me->me_hash = *udmep;
		*udmep = me;
		me->me_next = (mapentry_t *)udmtm;
		me->me_prev = udmtm->mtm_prev;
		udmtm->mtm_prev->me_next = me;
		udmtm->mtm_prev = me;
		udmtm->mtm_nud++;
		udmtm->mtm_nme++;
		mutex_exit(&udmtm->mtm_mutex);
	}

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));
	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(udmtm));

	/*
	 * now allow buffer to go thru normal done processing
	 */

	lbp = (lufs_buf_t *)cb;
	bp = (buf_t *)lbp->lb_ptr;

	if (cb->b_flags & B_ERROR) {
		bp->b_flags |= B_ERROR;
		bp->b_error = cb->b_error;
	}

	kmem_cache_free(lufs_bp, lbp);
	biodone(bp);
	return (0);
}

/*
 * wait for any async userdata write to finish
 */
void
logmap_ud_wait(mt_map_t *mtm)
{
	if (mtm->mtm_nud) {
		mutex_enter(&mtm->mtm_mutex);
		while (mtm->mtm_nud) {
			cv_wait(&mtm->mtm_cv, &mtm->mtm_mutex);
		}
		mutex_exit(&mtm->mtm_mutex);
	}
}

static void
logmap_cancel_delta(ml_unit_t *ul, offset_t mof, int32_t nb)
{
	mapentry_t	*me;
	mapentry_t	**mep;
	mt_map_t	*mtm	= ul->un_logmap;

	/*
	 * map has been referenced and is dirty
	 */
	mtm->mtm_ref = 1;
	mtm->mtm_dirty++;

	/*
	 * get a mapentry
	 */
	me = mapentry_alloc(NULL);

	/*
	 * initialize cancel record and put in logmap
	 */
	me->me_mof = mof;
	me->me_nb = nb;
	me->me_dt = DT_CANCEL;
	me->me_tid = mtm->mtm_tid;
	me->me_hash = NULL;

	/*
	 * write delta to log
	 */
	mutex_enter(&ul->un_log_mutex);
	logmap_wait_space(mtm, ul, me);
	ldl_write(ul, NULL, (offset_t)0, me);
	if (ul->un_flags & LDL_ERROR) {
		mapentry_free(me);
		mutex_exit(&ul->un_log_mutex);
		return;
	}

	/*
	 * put in hash and on cancel list
	 */
	mep = MAP_HASH(mof, mtm);
	mutex_enter(&mtm->mtm_mutex);
	me->me_age = mtm->mtm_age++;
	me->me_hash = *mep;
	*mep = me;
	me->me_next = (mapentry_t *)mtm;
	me->me_prev = mtm->mtm_prev;
	mtm->mtm_prev->me_next = me;
	mtm->mtm_prev = me;
	me->me_cancel = mtm->mtm_cancel;
	mtm->mtm_cancel = me;
	mtm->mtm_nme++;
	mtm->mtm_nmet++;
	me->me_flags |= (ME_HASH|ME_CANCEL);
	mutex_exit(&mtm->mtm_mutex);

	mutex_exit(&ul->un_log_mutex);
}

/*
 * At EOT; commit (i.e., cancel) any completed userdata deltas
 */
static void
logmap_ud_cancel(ml_unit_t *ul)
{
	offset_t	mof;
	int32_t		nb;
	mapentry_t	*me;
	mapentry_t	**mep;
	int		dolock	= 0;
	mt_map_t	*mtm	= ul->un_udmap;
	mt_map_t	*logmap	= ul->un_logmap;

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	/*
	 * At EOT, cancel the userdata deltas
	 */
again:
	/*
	 * A physio request could have encountered this userdata delta while
	 * it lived on the logmap (ME_AGE will be set).  Acquiring this lock
	 * will synchronize with that read as necessary.
	 */
	if (dolock)
		rw_enter(&logmap->mtm_rwlock, RW_WRITER);

	mutex_enter(&mtm->mtm_mutex);
	while ((me = mtm->mtm_next) != (mapentry_t *)mtm) {
		ASSERT(me->me_dt == DT_UD);

		if (me->me_flags & ME_AGE) {
			dolock = 1;
			mutex_exit(&mtm->mtm_mutex);
			goto again;
		}

		ASSERT((me->me_flags & ~ME_HASH) == 0);

		mof = me->me_mof;
		nb = me->me_nb;

		/*
		 * put a cancel record into the log
		 */
		mutex_exit(&mtm->mtm_mutex);
		logmap_cancel_delta(ul, mof, nb);
		mutex_enter(&mtm->mtm_mutex);

		/*
		 * find the entry in the hash
		 */
		mep = MAP_HASH(mof, mtm);
		while ((me = *mep) != NULL) {
			if (me == mtm->mtm_next)
				break;
			mep = &me->me_hash;
		}
		/*
		 * remove from hash
		 */
		ASSERT(me);
		*mep = me->me_hash;
		me->me_next->me_prev = me->me_prev;
		me->me_prev->me_next = me->me_next;
		me->me_flags &= ~ME_HASH;
		mtm->mtm_nud--;
		mtm->mtm_nme--;

		/*
		 * free the mapentry
		 */
		mapentry_free(me);
	}
	mutex_exit(&mtm->mtm_mutex);

	if (dolock)
		rw_exit(&logmap->mtm_rwlock);

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));
}

/*
 * cancel entries in a logmap (entries are freed at EOT)
 */
void
logmap_cancel(ml_unit_t *ul, offset_t mof, off_t nb)
{
	int32_t		hnb;
	mapentry_t	*me;
	mapentry_t	**mep;
	mt_map_t	*mtm	= ul->un_logmap;

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));

	for (hnb = 0; nb; nb -= hnb, mof += hnb) {
		hnb = MAPBLOCKSIZE - (mof & MAPBLOCKOFF);
		if (hnb > nb)
			hnb = nb;
		/*
		 * find overlapping entries
		 */
		mep = MAP_HASH(mof, mtm);
		mutex_enter(&mtm->mtm_mutex);
		for (me = *mep; me; me = me->me_hash) {
			if (!DATAoverlapME(mof, hnb, me))
				continue;

			ASSERT(MEwithinDATA(me, mof, hnb));

			if ((me->me_flags & ME_CANCEL) == 0) {
				me->me_cancel = mtm->mtm_cancel;
				mtm->mtm_cancel = me;
				me->me_flags |= ME_CANCEL;
			}
		}
		mutex_exit(&mtm->mtm_mutex);

		/*
		 * put a cancel record into the log
		 */
		logmap_cancel_delta(ul, mof, hnb);
	}

	ASSERT(((mtm->mtm_debug & MT_CHECK_MAP) == 0) ||
		map_check_linkage(mtm));
}

/*
 * check for overlap w/cancel delta
 */
int
logmap_iscancel(mt_map_t *mtm, offset_t mof, off_t nb)
{
	off_t		hnb;
	mapentry_t	*me;
	mapentry_t	**mep;

	mutex_enter(&mtm->mtm_mutex);
	for (hnb = 0; nb; nb -= hnb, mof += hnb) {
		hnb = MAPBLOCKSIZE - (mof & MAPBLOCKOFF);
		if (hnb > nb)
			hnb = nb;
		/*
		 * search for dup entry
		 */
		mep = MAP_HASH(mof, mtm);
		for (me = *mep; me; me = me->me_hash) {
			if (((me->me_flags & ME_ROLL) == 0) &&
			    (me->me_dt != DT_CANCEL))
				continue;
			if (DATAoverlapME(mof, hnb, me))
				break;
		}

		/*
		 * overlap detected
		 */
		if (me) {
			mutex_exit(&mtm->mtm_mutex);
			return (1);
		}
	}
	mutex_exit(&mtm->mtm_mutex);
	return (0);
}

static int
logmap_logscan_add(ml_unit_t *ul, struct delta *dp, off_t lof, size_t *nbp)
{
	mapentry_t	*me;
	int		error;
	mt_map_t	*mtm	= ul->un_logmap;

	/*
	 * verify delta header; failure == mediafail
	 */
	error = 0;
	/* delta type */
	if ((dp->d_typ <= DT_NONE) || (dp->d_typ >= DT_MAX))
		error = EINVAL;
	if (dp->d_typ == DT_COMMIT) {
		if (dp->d_nb != INT32_C(0) && dp->d_nb != INT32_C(-1))
			error = EINVAL;
	} else {
		/* length of delta */
		if ((dp->d_nb < INT32_C(0)) ||
		    (dp->d_nb > INT32_C(MAPBLOCKSIZE)))
			error = EINVAL;

		/* offset on master device */
		if (dp->d_mof < INT64_C(0))
			error = EINVAL;
	}

	if (error) {
		ldl_seterror(ul, "Error processing ufs log data during scan");
		return (error);
	}

	/*
	 * process commit record
	 */
	if (dp->d_typ == DT_COMMIT) {
		if (mtm->mtm_dirty) {
			ASSERT(dp->d_nb == INT32_C(0));
			logmap_free_cancel(mtm);
			mtm->mtm_dirty = 0;
			mtm->mtm_nmet = 0;
			mtm->mtm_tid++;
			mtm->mtm_committid = mtm->mtm_tid;
			ASSERT(((mtm->mtm_debug & MT_SCAN) == 0) ||
				logmap_logscan_commit_debug(lof, mtm));
		}
		/*
		 * return #bytes to next sector (next delta header)
		 */
		*nbp = ldl_logscan_nbcommit(lof);
		mtm->mtm_tail_lof = lof;
		mtm->mtm_tail_nb = *nbp;
		return (0);
	}

	/*
	 * add delta to logmap
	 */
	me = mapentry_alloc(NULL);
	me->me_lof = lof;
	me->me_mof = dp->d_mof;
	me->me_nb = dp->d_nb;
	me->me_tid = mtm->mtm_tid;
	me->me_dt = dp->d_typ;
	me->me_hash = NULL;
	me->me_flags = ME_LIST;
	switch (dp->d_typ) {
	case DT_UD:
		me->me_dt = DT_SUD;
		logmap_add_ud(ul, NULL, 0, me);
		break;
	case DT_CANCEL:
		logmap_add(ul, NULL, 0, me);
		me->me_flags |= ME_CANCEL;
		me->me_cancel = mtm->mtm_cancel;
		mtm->mtm_cancel = me;
		break;
	default:
		logmap_add(ul, NULL, 0, me);
		ASSERT(((mtm->mtm_debug & MT_SCAN) == 0) ||
			logmap_logscan_add_debug(dp, mtm));
		break;
	}

sizeofdelta:
	/*
	 * return #bytes till next delta header
	 */
	if ((dp->d_typ == DT_CANCEL) || (dp->d_typ == DT_ABZERO))
		*nbp = 0;
	else
		*nbp = dp->d_nb;
	return (0);
}

void
logmap_logscan(ml_unit_t *ul)
{
	size_t		nb, nbd;
	off_t		lof;
	struct delta	delta;
	mt_map_t	*logmap	= ul->un_logmap;

	ASSERT(ul->un_deltamap->mtm_next == (mapentry_t *)ul->un_deltamap);

	/*
	 * prepare the log for a logscan
	 */
	ldl_logscan_begin(ul);

	/*
	 * prepare the logmap for a logscan
	 */
	(void) map_free_entries(logmap);
	logmap->mtm_tid = 0;
	logmap->mtm_committid = UINT32_C(0);
	logmap->mtm_age = 0;
	logmap->mtm_dirty = 0;
	logmap->mtm_ref = 0;
	logmap->mtm_nsud = 0;
	logmap->mtm_nud = 0;

	/*
	 * while not at end of log
	 *	read delta header
	 *	add to logmap
	 *	seek to beginning of next delta
	 */
	lof = ul->un_head_lof;
	nbd = sizeof (delta);
	while (lof != ul->un_tail_lof) {

		/* read delta header */
		if (ldl_logscan_read(ul, &lof, nbd, (caddr_t)&delta))
			break;

		/* add to logmap */
		if (logmap_logscan_add(ul, &delta, lof, &nb))
			break;

		/* seek to next header (skip data) */
		if (ldl_logscan_read(ul, &lof, nb, NULL))
			break;
	}

	/*
	 * remove the last partial transaction from the logmap
	 */
	logmap_abort(ul);

	/*
	 * set tail -> to the sector following the last transaction
	 */
	logmap_settail(logmap, ul);

	ldl_logscan_end(ul);
}

void
_init_map(void)
{
	/*
	 * global mutex that protects
	 *	free mapentry list
	 *	list of logmaps
	 */
	mutex_init(&map_mutex, NULL, MUTEX_DEFAULT, NULL);
	/*
	 * init tasq creation protection mutex
	 */
	mutex_init(&log_tq_create_mutex, NULL, MUTEX_DEFAULT, NULL);
}
