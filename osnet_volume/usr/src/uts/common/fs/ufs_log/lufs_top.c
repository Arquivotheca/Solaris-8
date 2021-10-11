
#pragma ident	"@(#)lufs_top.c	1.25	99/12/04 SMI"

/*
 * Copyright (c) 1992, 1993, 1994, 1996-1999 by Sun Microsystems, Inc.
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
#include <sys/taskq.h>
#include <sys/cmn_err.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_filio.h>
#include <sys/fs/ufs_log.h>

/*
 * FILE SYSTEM INTERFACE TO TRANSACTION OPERATIONS (TOP; like VOP)
 */


/*
 * declare a delta
 */
static void
top_delta(
	struct ufstrans *ufstrans,
	offset_t mof,
	off_t nb,
	delta_t dtyp,
	int (*func)(),
	ulong_t arg)
{
	ml_unit_t		*ul	= (ml_unit_t *)ufstrans->ut_data;

	ASSERT(ufstrans->ut_dev == ul->un_dev);
	ASSERT(nb);
	ASSERT(((ul->un_debug & (MT_TRANSACT|MT_MATAMAP)) == 0) ||
		top_delta_debug(ul, mof, nb, dtyp));

	deltamap_add(ul->un_deltamap, mof, nb, dtyp, func, arg);

	/*
	 * needed for the roll thread's heuristic
	 */
	ul->un_logmap->mtm_ref = 1;
}
/*
 * declare a userdata delta
 */
static int
top_ud_delta(
	struct ufstrans *ufstrans,
	offset_t mof,
	off_t nb,
	delta_t dtyp,
	int (*func)(),
	ulong_t arg)
{
	ml_unit_t	*ul	= (ml_unit_t *)ufstrans->ut_data;
	mt_map_t	*mtm	= ul->un_logmap;

	ASSERT(ufstrans->ut_dev == ul->un_dev);
	ASSERT(nb);
	ASSERT(dtyp == DT_UD);

	/*
	 * can't log userdata if previous write of same data not canceled
	 */
	if (logmap_overlap(ul->un_udmap, mof, nb))
		return (0);

	/*
	 * can't log userdata if the current transaction is full
	 */
	mutex_enter(&mtm->mtm_lock);
	if ((nb + ul->un_resv) > (ul->un_maxresv - (ul->un_maxresv >> 2))) {
		mutex_exit(&mtm->mtm_lock);
		return (0);
	}
	ul->un_resv += nb;
	mutex_exit(&mtm->mtm_lock);

	/*
	 * log the userdata
	 */
	ASSERT(((ul->un_debug & (MT_TRANSACT|MT_MATAMAP)) == 0) ||
		top_delta_debug(ul, mof, nb, dtyp));
	deltamap_add(ul->un_deltamap, mof, nb, dtyp, func, arg);

	/*
	 * needed for the roll thread's heuristic
	 */
	ul->un_logmap->mtm_ref = 1;
	return (1);
}


/*
 * cancel a delta
 */
static void
top_cancel(struct ufstrans *ufstrans, offset_t mof, off_t nb)
{
	ml_unit_t	*ul	= (ml_unit_t *)ufstrans->ut_data;

	ASSERT(ufstrans->ut_dev == ul->un_dev);
	ASSERT(nb);
	ASSERT(((ul->un_debug & (MT_TRANSACT|MT_MATAMAP)) == 0) ||
		top_delta_debug(ul, mof, nb, DT_CANCEL));

	deltamap_del(ul->un_deltamap, mof, nb);

	logmap_cancel(ul, mof, nb);

	/*
	 * needed for the roll thread's heuristic
	 */
	ul->un_logmap->mtm_ref = 1;
}

/*
 * check if this delta has been canceled (metadata -> userdata)
 */
static int
top_iscancel(struct ufstrans *ufstrans, offset_t mof, off_t nb)
{
	ml_unit_t	*ul	= (ml_unit_t *)ufstrans->ut_data;

	ASSERT(ufstrans->ut_dev == ul->un_dev);
	ASSERT(nb);
	if (logmap_overlap(ul->un_udmap, mof, nb))
		return (1);
	if (logmap_iscancel(ul->un_logmap, mof, nb))
		return (1);
	if (ul->un_flags & LDL_ERROR)
		return (1);
	return (0);
}

/*
 * put device into error state
 */
static void
top_seterror(struct ufstrans *ufstrans)
{
	ml_unit_t	*ul	= (ml_unit_t *)ufstrans->ut_data;

	ASSERT(ufstrans->ut_dev == ul->un_dev);
	ldl_seterror(ul, "ufs is forcing a ufs log error");
}

/*
 * check device's error state
 */
static int
top_iserror(struct ufstrans *ufstrans)
{
	ml_unit_t	*ul	= (ml_unit_t *)ufstrans->ut_data;

	ASSERT(ufstrans->ut_dev == ul->un_dev);
	return (ul->un_flags & LDL_ERROR);
}

/*
 * issue a empty sync op to help empty the delta/log map or the log
 */
static void
top_issue_sync(void *ufstrans)
{
	int	error;

	if ((curthread->t_flag & T_DONTBLOCK) == 0)
		curthread->t_flag |= T_DONTBLOCK;
	(*((struct ufstrans *)ufstrans)->ut_ops->trans_begin_sync)
		((struct ufstrans *)ufstrans, TOP_COMMIT_ASYNC, 0);
	(*((struct ufstrans *)ufstrans)->ut_ops->trans_end_sync)
		((struct ufstrans *)ufstrans, &error, TOP_COMMIT_ASYNC, 0);
}

/*
 * MOBY TRANSACTION ROUTINES
 * begin a moby transaction
 *	sync ops enter until first sync op finishes
 *	async ops enter until last sync op finishes
 * end a moby transaction
 *		outstanding deltas are pushed thru metatrans device
 *		log buffer is committed (incore only)
 *		next trans is open to async ops
 *		log buffer is committed on the log
 *		next trans is open to sync ops
 */

/*ARGSUSED*/
void
top_begin_sync(struct ufstrans *ufstrans, top_t topid, ulong_t size)
{
	ml_unit_t	*ul	= (ml_unit_t *)ufstrans->ut_data;
	mt_map_t	*mtm	= ul->un_logmap;

	ASSERT(ufstrans->ut_dev == ul->un_dev);

	mutex_enter(&mtm->mtm_lock);
retry:
	mtm->mtm_ref = 1;
	/*
	 * current transaction closed to sync ops; try for next transaction
	 */
	if (mtm->mtm_closed & TOP_SYNC && !panicstr) {
		ulong_t		resv;
		ushort_t	seq;
		/*
		 * next transaction is full; try for next transaction
		 */
		resv = size + ul->un_resv_wantin + ul->un_resv;
		if (resv > ul->un_maxresv) {
			cv_wait(&mtm->mtm_cv_commit, &mtm->mtm_lock);
			goto retry;
		}
		/*
		 * we are in the next transaction; wait for it to start
		 */
		mtm->mtm_wantin++;
		ul->un_resv_wantin += size;
		/*
		 * The corresponding cv_broadcast wakes up
		 * all threads that have been validated to go into
		 * the next transaction. However, because spurious
		 * cv_wait wakeups are possible we use a sequence
		 * number to check that the commit and cv_broadcast
		 * has really occurred. We couldn't use mtm_tid
		 * because on error that doesn't get incremented.
		 */
		seq = mtm->mtm_seq;
		do {
			cv_wait(&mtm->mtm_cv_commit, &mtm->mtm_lock);
		} while (seq == mtm->mtm_seq);
	} else {
		/*
		 * if the current transaction is full; try the next one
		 */
		if (size && (ul->un_resv && ((size + ul->un_resv) >
		    ul->un_maxresv)) && !panicstr) {
			/*
			 * log is over reserved and no one will unresv the space
			 *	so generate empty sync op to unresv the space
			 */
			if (mtm->mtm_activesync == 0) {
				mutex_exit(&mtm->mtm_lock);
				top_issue_sync(ufstrans);
				mutex_enter(&mtm->mtm_lock);
				goto retry;
			}
			cv_wait(&mtm->mtm_cv_commit, &mtm->mtm_lock);
			goto retry;
		}
		/*
		 * we are in the current transaction
		 */
		mtm->mtm_active++;
		mtm->mtm_activesync++;
		ul->un_resv += size;
	}

	ASSERT(mtm->mtm_active > 0);
	ASSERT(mtm->mtm_activesync > 0);
	mutex_exit(&mtm->mtm_lock);

	ASSERT(((ul->un_debug & MT_TRANSACT) == 0) ||
		top_begin_debug(ul, topid, size));
}

extern taskq_t *log_sync_tq;
int tryfail_cnt;
int disable_tryasync;
int getpage_cnt;

/*ARGSUSED*/
static int
top_begin_async(struct ufstrans *ufstrans, top_t topid, ulong_t size, int try)
{
	ml_unit_t	*ul	= (ml_unit_t *)ufstrans->ut_data;
	mt_map_t	*mtm	= ul->un_logmap;

	ASSERT(ufstrans->ut_dev == ul->un_dev);

	mutex_enter(&mtm->mtm_lock);
	if (topid == TOP_GETPAGE)
		getpage_cnt++;
retry:
	mtm->mtm_ref = 1;
	/*
	 * current transaction closed to async ops; try for next transaction
	 */
	if ((mtm->mtm_closed & TOP_ASYNC) && !panicstr) {
		if (try && !disable_tryasync) {
			tryfail_cnt++;
			mutex_exit(&mtm->mtm_lock);
			return (EWOULDBLOCK);
		}
		cv_wait(&mtm->mtm_cv_next, &mtm->mtm_lock);
		goto retry;
	}

	/*
	 * if the current transaction is full; try the next one
	 */
	if ((ul->un_resv && ((size + ul->un_resv) > ul->un_maxresv)) &&
	    !panicstr) {
		/*
		 * log is overreserved and no one will unresv the space
		 *	so generate empty sync op to unresv the space
		 */
		if (mtm->mtm_activesync == 0) {
			mutex_exit(&mtm->mtm_lock);
			(void) taskq_dispatch(log_sync_tq, top_issue_sync,
				ufstrans, KM_SLEEP);
			mutex_enter(&mtm->mtm_lock);
		}
		if (try && !disable_tryasync) {
			tryfail_cnt++;
			mutex_exit(&mtm->mtm_lock);
			return (EWOULDBLOCK);
		}
		cv_wait(&mtm->mtm_cv_next, &mtm->mtm_lock);
		goto retry;
	}
	/*
	 * we are in the current transaction
	 */
	mtm->mtm_active++;
	ul->un_resv += size;

	ASSERT(mtm->mtm_active > 0);
	mutex_exit(&mtm->mtm_lock);

	ASSERT(((ul->un_debug & MT_TRANSACT) == 0) ||
		top_begin_debug(ul, topid, size));
	return (0);
}

/*ARGSUSED*/
void
top_end_sync(struct ufstrans *ufstrans, int *ep, top_t topid, ulong_t size)
{
	ml_unit_t	*ul	= (ml_unit_t *)ufstrans->ut_data;
	mt_map_t	*mtm	= ul->un_logmap;
	uint32_t	tid;

	ASSERT(ufstrans->ut_dev == ul->un_dev);

	ASSERT(((ul->un_debug & MT_TRANSACT) == 0) ||
		top_end_debug_1(ul, mtm, topid, size));

	mutex_enter(&mtm->mtm_lock);
	mtm->mtm_ref = 1;

	mtm->mtm_activesync--;
	mtm->mtm_active--;
	/*
	 * wait for last syncop to complete
	 */
	if (mtm->mtm_activesync || panicstr) {
		ushort_t seq;

		/* close current transaction to sync ops */
		mtm->mtm_closed |= TOP_SYNC;

		seq = mtm->mtm_seq;
		do {
			cv_wait(&mtm->mtm_cv_commit, &mtm->mtm_lock);
		} while (seq == mtm->mtm_seq);
		goto out;
	}
	/*
	 * last syncop; close current transaction to all ops
	 */
	mtm->mtm_closed |= (TOP_SYNC|TOP_ASYNC);

	/*
	 * unreserve the log space
	 *	must be done here for top_begin_sync's un_resv check
	 */
	ul->un_resv = 0;

	/*
	 * wait for last asyncop to finish
	 */
	while (mtm->mtm_active)
		cv_wait(&mtm->mtm_cv_eot, &mtm->mtm_lock);

	/*
	 * push dirty metadata thru the metatrans device
	 */
	deltamap_push(ul);

	/*
	 * asynchronously write the commit record
	 */
	logmap_commit(ul);
	tid = mtm->mtm_tid;

	ASSERT(((ul->un_debug & MT_FORCEROLL) == 0) ||
		top_roll_debug(ul));

	/*
	 * allow async ops
	 */
	ASSERT(mtm->mtm_active == 0);
	mtm->mtm_closed = TOP_SYNC;
	cv_broadcast(&mtm->mtm_cv_next);
	mutex_exit(&mtm->mtm_lock);

	/*
	 * wait for outstanding log writes (e.g., commits) to finish
	 */
	ldl_waito(ul);

	/*
	 * if the logmap is getting full; roll something
	 */
	if (logmap_need_roll_sync(mtm))
		logmap_forceroll(mtm);

	/*
	 * now, allow all ops
	 */
	mutex_enter(&mtm->mtm_lock);
	mtm->mtm_active += mtm->mtm_wantin;
	ul->un_resv += ul->un_resv_wantin;
	mtm->mtm_activesync = mtm->mtm_wantin;
	mtm->mtm_wantin = 0;
	mtm->mtm_closed = 0;
	ul->un_resv_wantin = 0;
	mtm->mtm_committid = tid;
	mtm->mtm_seq++;
	cv_broadcast(&mtm->mtm_cv_commit);

out:
	mutex_exit(&mtm->mtm_lock);

	ASSERT(((ul->un_debug & MT_TRANSACT) == 0) ||
		top_end_debug_2());

	if (ul->un_flags & LDL_ERROR)
		*ep = EIO;
}

/*ARGSUSED*/
static void
top_end_async(struct ufstrans *ufstrans, top_t topid, ulong_t size)
{
	ml_unit_t	*ul	= (ml_unit_t *)ufstrans->ut_data;
	mt_map_t	*mtm	= ul->un_logmap;

	ASSERT(ufstrans->ut_dev == ul->un_dev);

	ASSERT(((ul->un_debug & MT_TRANSACT) == 0) ||
		top_end_debug_1(ul, mtm, topid, size));

	mutex_enter(&mtm->mtm_lock);
	mtm->mtm_ref = 1;

	if (--mtm->mtm_active == 0)
		cv_broadcast(&mtm->mtm_cv_eot);
	mutex_exit(&mtm->mtm_lock);

	ASSERT(((ul->un_debug & MT_TRANSACT) == 0) || top_end_debug_2());

	/*
	 * Generate a sync op if the log, logmap, or deltamap are heavily used.
	 * Unless we are possibly holding any VM locks, since if we are holding
	 * any VM locks and we issue a top_end_sync(), we could deadlock.
	 */
	if (mtm->mtm_activesync == 0)
		if ((deltamap_need_commit(ul->un_deltamap) ||
		    logmap_need_commit(mtm) ||
		    ldl_need_commit(ul)) && topid != TOP_GETPAGE)
			top_issue_sync(ufstrans);
	/*
	 * roll something from the log if the logmap is too full
	 */
	if (logmap_need_roll_async(mtm))
		logmap_forceroll(mtm);
}

/*
 * Called from roll thread;
 *	buffer set for reading master
 *	adjusts b_addr, b_blkno, b_bcount, and b_bufsize
 */
void
top_read_roll(struct buf *bp, ml_unit_t *ul, uint16_t *secmap)
{
	mapentry_t	*age;
	mt_map_t	*logmap	= ul->un_logmap;
	offset_t	mof	= ldbtob(bp->b_blkno);
	off_t		nb	= bp->b_bcount;
	char		*va	= bp->b_un.b_addr;

	/*
	 * get a linked list of overlaping deltas
	 */
	logmap_list_get_roll(logmap, mof, nb, &age);

	/*
	 * no overlapping deltas were found, nothing to roll
	 */
	if (age == NULL) {
		logmap_list_put(logmap, age);
		if (ul->un_flags & LDL_ERROR) {
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
		} else
			bp->b_flags |= B_INVAL;
		biodone(bp);
		return;
	}

	/*
	 * generate bit map of valid metadata sectors in bp for roll thread
	 */
	logmap_secmap_roll(age, mof, secmap);

	/*
	 * sync read the data from master
	 *	errors are returned in bp
	 */
	logmap_read_mstr(ul, bp);

	/*
	 * sync read the data from the log
	 */
	if (ldl_read(ul, va, mof, nb, age)) {
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
	}

	/*
	 * unlist the deltas
	 */
	logmap_list_put(logmap, age);

	/*
	 * all done
	 */
	if (ul->un_flags & LDL_ERROR) {
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
	}
	biodone(bp);
}


/*
 * move deltas from deltamap into the log
 */
static void
top_log(struct ufstrans *ufstrans, char *va, offset_t mof, off_t nb)
{
	ml_unit_t	*ul	= (ml_unit_t *)ufstrans->ut_data;
	mapentry_t	*me;

	/*
	 * needed for the roll thread's heuristic
	 */
	ul->un_logmap->mtm_ref = 1;

	/*
	 * if there are deltas
	 */
	if (me = deltamap_remove(ul->un_deltamap, mof, nb)) {

		/*
		 * move to logmap
		 */
		logmap_add(ul, va, mof, me);
	}

	ASSERT((ul->un_matamap == NULL) ||
		matamap_within(ul->un_matamap, mof, nb));

	ASSERT(((ul->un_debug & MT_WRITE_CHECK) == 0) ||
		top_check_debug(va, mof, nb, ul));
}
struct ufstransops ufstransops = {
	top_begin_sync,
	top_begin_async,
	top_end_sync,
	top_end_async,
	top_delta,
	top_ud_delta,
	top_cancel,
	top_log,
	top_iscancel,
	top_seterror,
	top_iserror,
	NULL,
	NULL,
	NULL,
};
void
top_snarf(ml_unit_t *ul)
{
	struct ufstransops	*tops;

	if (ul == NULL) {
		ul->un_ut = NULL;
		return;
	}

	tops = &ufstransops;

	ASSERT(((ul->un_debug & (MT_MATAMAP | MT_NOASYNC)) == 0) ||
		top_snarf_debug(ul, &tops, &ufstransops));

	ul->un_ut = ufs_trans_set(ul->un_dev, tops, (void *)ul);
}

/*ARGSUSED*/
void
top_unsnarf(ml_unit_t *ul)
{
	struct ufstransops	*tops;

	if (ul->un_ut == NULL)
		return;

	tops = ul->un_ut->ut_ops;

	ufs_trans_reset(ul->un_dev);
	if (tops != &ufstransops)
		trans_free(tops, sizeof (*tops));

	ul->un_ut = NULL;
}

void
_init_top(void)
{
	ASSERT(top_init_debug());

	/*
	 * set up the delta layer
	 */
	_init_map();
}
