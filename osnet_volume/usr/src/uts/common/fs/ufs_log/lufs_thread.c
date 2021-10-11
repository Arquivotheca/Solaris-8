
#pragma ident "@(#)lufs_thread.c	1.22	99/03/07 SMI"

/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
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
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_filio.h>
#include <sys/fs/ufs_log.h>
#include <sys/fs/ufs_bio.h>
#include <sys/inttypes.h>
#include <sys/callb.h>

/*
 * externs
 */
extern kmutex_t		ml_scan;
extern kcondvar_t	ml_scan_cv;
extern int		maxphys;

/*
 * KERNEL THREADS FOR METATRANS DEVICE
 */
/*
 * ROLL THREAD
 *	one per logmap
 */

static int	trans_roll_tics		= 0;
static void
trans_roll_wait(mt_map_t *logmap, callb_cpr_t *cprinfop)
{
	mutex_enter(&logmap->mtm_mutex);
	logmap->mtm_ref = 0;
	logmap->mtm_flags &= ~MTM_FORCE_ROLL;
	cv_broadcast(&logmap->mtm_cv);
	CALLB_CPR_SAFE_BEGIN(cprinfop);
	(void) cv_timedwait(&logmap->mtm_cv, &logmap->mtm_mutex,
			lbolt + trans_roll_tics);
	CALLB_CPR_SAFE_END(cprinfop, &logmap->mtm_mutex);
	mutex_exit(&logmap->mtm_mutex);
}

void
trans_roll(ml_unit_t *ul)
{
	int		i, j;
	int		nbuf;
	int		morewrites;
	int		error;
	size_t		pwsize;
	caddr_t		pwbuf;
	caddr_t		va;
	size_t		nmblk;
	offset_t	mof;
	daddr_t		mblkno;
	struct prewrite	*pws;
	struct prewrite	*pw;
	struct buf	*bps;
	struct buf	*bp;
	mt_map_t	*logmap;
	int		doingforceroll;
	int		nbits	= (NBBY * sizeof (pw->pw_secmap));
	callb_cpr_t	cprinfo;
	klwp_t		*lwp	= ttolwp(curthread);

	ASSERT((MAPBLOCKSIZE / DEV_BSIZE) <= nbits);

	/*
	 * grab the corresponding logmap
	 */
	logmap = ul->un_logmap;

	CALLB_CPR_INIT(&cprinfo, &logmap->mtm_mutex, callb_generic_cpr,
	    "trans_roll");

	/*
	 * setup some roll parameters
	 */
	if (trans_roll_tics == 0)
		trans_roll_tics = 5 * hz;
	pwsize = ldl_bufsize(ul);

	/*
	 * number of bufs in the prewrite area; first sector maintains state
	 */
	ASSERT(pwsize > DEV_BSIZE);
	nmblk = MIN(((pwsize - DEV_BSIZE) >> MAPBLOCKSHIFT),
		    (DEV_BSIZE / sizeof (struct prewrite)));
	ASSERT(nmblk);

	/*
	 * grab some memory for io
	 */
	pwbuf = (caddr_t)trans_zalloc(pwsize);
	pws = (void *)pwbuf;

	/*
	 * initialize the rw buf headers and the per buf prewrite structs
	 */
	bps = trans_zalloc((size_t) (sizeof (struct buf) * nmblk));
	for (i = 0, bp = bps, pw = pws; i < nmblk; ++i, ++bp, ++pw) {
		bp->b_iodone = trans_not_done;
		pw->pw_bufsize = MAPBLOCKSIZE;
	}

	doingforceroll = 0;

again:
	/*
	 * LOOP FOREVER
	 */
	/*
	 * exit on demand
	 */
	mutex_enter(&logmap->mtm_mutex);
	if ((ul->un_flags & LDL_ERROR) || logmap->mtm_flags & MTM_ROLL_EXIT) {
		trans_free((caddr_t)bps, (size_t)(sizeof (struct buf) * nmblk));
		trans_free((caddr_t)pwbuf, pwsize);
		logmap->mtm_flags &=
			~(MTM_FORCE_ROLL | MTM_ROLL_RUNNING | MTM_ROLL_EXIT);
		cv_broadcast(&logmap->mtm_cv);
		CALLB_CPR_EXIT(&cprinfo);
		thread_exit();
		/* NOTREACHED */
	}

	/*
	 * MT_SCAN debug mode
	 *	don't roll except in FORCEROLL situations
	 */
	if (logmap->mtm_debug & MT_SCAN)
		if ((logmap->mtm_flags & MTM_FORCE_ROLL) == 0) {
			mutex_exit(&logmap->mtm_mutex);
			trans_roll_wait(logmap, &cprinfo);
			goto again;
		}
	ASSERT(logmap->mtm_trimlof == 0);
	mutex_exit(&logmap->mtm_mutex);

	/*
	 * free up log space; if possible
	 */
	logmap_sethead(logmap, ul);

	/*
	 * if someone wants us to roll something; then do it
	 */
	if (doingforceroll) {
		doingforceroll = 0;
		mutex_enter(&logmap->mtm_mutex);
		logmap->mtm_flags &= ~MTM_FORCE_ROLL;
		cv_broadcast(&logmap->mtm_cv);
		mutex_exit(&logmap->mtm_mutex);
	}
	if (logmap->mtm_flags & MTM_FORCE_ROLL) {
		doingforceroll = 1;
		goto rollsomething;
	}

	/*
	 * userdata discovered during logscan; roll it now
	 */
	if (logmap->mtm_nsud) {
		goto rollsomething;
	}

	/*
	 * log is idle and is not empty; try to roll something
	 */
	if (!logmap->mtm_ref && !ldl_empty(ul)) {
		goto rollsomething;
	}

	/*
	 * log is busy but is getting full; try to roll something
	 */
	if (ldl_need_roll(ul)) {
		goto rollsomething;
	}

	/*
	 * log is busy but the logmap is getting full; try to roll something
	 */
	if (logmap_need_roll(logmap)) {
		goto rollsomething;
	}

	/*
	 * nothing to do; wait a bit and then start over
	 */
	trans_roll_wait(logmap, &cprinfo);
	goto again;

	/*
	 * ROLL SOMETHING
	 */

rollsomething:

	/*
	 * Make sure there is really something to do
	 */
	if (logmap->mtm_nud == 0 &&
	    !logmap_next_roll(logmap, &mof)) {
		trans_roll_wait(logmap, &cprinfo);
		goto again;
	}


	/*
	 * build some (master blocks + deltas) to roll forward
	 */
	error = 0;
	while (error == 0 && logmap_next_roll(logmap, &mof)) {
		/*
		 * either find a free block or reuse a previously read
		 * block if more deltas have been committed for that block.
		 */
		mof = mof & (offset_t)MAPBLOCKMASK;
		mblkno = lbtodb(mof);
		va = pwbuf + DEV_BSIZE;
		for (i = 0, bp = bps, pw = pws; i < nmblk; ++i, ++bp, ++pw) {
			if ((pw->pw_flags & PW_INUSE) == 0) {
				break;
			}
			if (pw->pw_blkno == mblkno) {
				break;
			}
			va += MAPBLOCKSIZE;
		}
		if (i == nmblk)
			break;
		/*
		 * read a master block + deltas
		 */
		bp->b_flags = B_KERNBUF | B_READ;
		bp->b_blkno = mblkno;
		bp->b_bcount = MAPBLOCKSIZE;
		bp->b_bufsize = MAPBLOCKSIZE;
		bp->b_un.b_addr = va;
		bp->b_edev = ul->un_dev;
		top_read_roll(bp, ul, &pw->pw_secmap);
		error = trans_wait(bp);

		/*
		 * keep prewrite info; B_INVAL means no deltas to roll
		 */
		if (bp->b_flags & B_INVAL) {
			pw->pw_flags = 0;
			pw->pw_secmap = UINT16_C(0);
		} else {
			if (error)
				pw->pw_secmap = UINT16_C(0);
			pw->pw_blkno = mblkno;
			pw->pw_flags |= (PW_INUSE | PW_REM);
		}
	}
	/*
	 * prepare for the prewrite and the write
	 */
	for (i = 0, bp = bps, pw = pws, nbuf = 0; i < nmblk; ++i, ++bp, ++pw) {
		if (pw->pw_flags & PW_INUSE)
			nbuf = i + 1;
		/* the goto-loop write loop below depends on this */
		bp->b_bcount = bp->b_bufsize = 0;
	}
	/*
	 * If there was nothing to roll; wait on a userdata write to finish
	 */
	if (nbuf == 0) {
		logmap_ud_wait(logmap);
		goto again;
	}

writemore:
	/*
	 * start the writes to the master device
	 */
	for (i = 0, bp = bps, pw = pws; i < nbuf; ++i, ++bp, ++pw) {
		if (pw->pw_secmap == UINT16_C(0))
			continue;
		/* skip over the previous write, if any */
		bp->b_blkno += btodb(bp->b_bcount);
		bp->b_un.b_addr += bp->b_bcount;
		bp->b_bcount = 0;
		bp->b_bufsize = 0;
		for (j = 0; j < nbits; ++j) {
			if (pw->pw_secmap & UINT16_C(1))
				break;
			pw->pw_secmap >>= 1;
			bp->b_un.b_addr += DEV_BSIZE;
			bp->b_blkno++;
		}
		for (; j < nbits; ++j) {
			if ((pw->pw_secmap & UINT16_C(1)) == 0)
				break;
			pw->pw_secmap >>= 1;
			bp->b_bcount += DEV_BSIZE;
			bp->b_bufsize += DEV_BSIZE;
		}

		bp->b_flags = B_KERNBUF | B_WRITE;
		pw->pw_flags |= PW_WAIT;
		ub.ub_rwrites.value.ul++;
		(void) bdev_strategy(bp);
		if (lwp != NULL)
			lwp->lwp_ru.oublock++;
	}
	/*
	 * wait for the writes to finish
	 */
	morewrites = 0;
	for (i = 0, bp = bps, pw = pws; i < nbuf; ++i, ++bp, ++pw) {
		if ((pw->pw_flags & PW_WAIT) == 0)
			continue;
		error = trans_wait(bp);
		if (error)
			ldl_seterror(ul,
			    "Error writing master during ufs log roll");
		pw->pw_flags &= ~PW_WAIT;
		if (pw->pw_secmap)
			morewrites = 1;
	}
	if (ul->un_flags & LDL_ERROR)
		goto again;
	if (morewrites)
		goto writemore;

	/*
	 * invalidate the prewrite area
	 */
	for (i = 0, pw = pws; i < nbuf; ++i, ++pw)
		pw->pw_flags &= ~PW_INUSE;

	/*
	 * free up the deltas in the delta map
	 */
	for (i = 0, bp = bps, pw = pws; i < nbuf; ++i, ++bp, ++pw) {
		if ((pw->pw_flags & PW_REM) == 0)
			continue;
		pw->pw_flags &= ~PW_REM;
		logmap_remove_roll(logmap, ldbtob(pw->pw_blkno),
			MAPBLOCKSIZE);
	}

	/*
	 * free up log space; if possible
	 */
	logmap_sethead(logmap, ul);

	/*
	 * LOOP
	 */
	goto again;
}
