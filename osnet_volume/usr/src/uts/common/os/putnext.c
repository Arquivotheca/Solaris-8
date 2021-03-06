/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)putnext.c	1.22	99/11/12 SMI"

/*
 *		UNIX Device Driver Interface functions
 *	This file contains the C-versions of putnext() and put().
 *	Assembly language versions exist for some architectures.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cpuvar.h>
#include <sys/debug.h>
#include <sys/t_lock.h>
#include <sys/stream.h>
#include <sys/thread.h>
#include <sys/strsubr.h>
#include <sys/ddi.h>
#include <sys/vtrace.h>
#include <sys/cmn_err.h>
#include <sys/strft.h>

boolean_t	UseFastlocks = B_FALSE;

/*
 * function: putnext()
 * purpose:  call the put routine of the queue linked to qp
 *
 * Note: this function is written to perform well on modern computer
 * architectures by e.g. preloading values into registers and "smearing" out
 * code.
 *
 * A note on the fastput mechanism.  The most significant bit of a
 * putcount is considered the "FASTPUT" bit.  If set, then there is
 * nothing stoping a concurrent put from occuring (note that putcounts
 * are only allowed on CIPUT perimiters).  If, however, it is cleared,
 * then we need to take the normal lock path by aquiring the SQLOCK.
 * This is a slowlock.  When a thread starts exclusiveness, e.g. wants
 * writer access, it will clear the FASTPUT bit, causing new threads
 * to take the slowlock path.  This assures that putcounts will not
 * increase in value, so the want-writer does not need to constantly
 * aquire the putlocks to sum the putcounts.  This does have the
 * possibility of having the count drop right after reading, but that
 * is no different than aquiring, reading and then releasing.  However,
 * in this mode, it cannot go up, so eventually they will drop to zero
 * and the want-writer can proceed.
 *
 * If the FASTPUT bit is set, or in the slowlock path we see that there
 * are no writers or want-writers, we make the choice of calling the
 * putproc, or a "fast-fill_syncq".  The fast-fill is a fill with
 * immediate intention to drain.  This is done because there are
 * messages already at the queue waiting to drain.  To preserve message
 * ordering, we need to put this message at the end, and pickup the
 * messages at the beginning.  We call the macro that actually
 * enqueues the message on the queue, and then call qdrain_syncq.  If
 * there is already a drainer, we just return.  We could make that
 * check before calling qdrain_syncq, but it is a little more clear
 * to have qdrain_syncq do this (we might try the above optimization
 * as this behavior evolves).  qdrain_syncq assumes that SQ_EXCL is set
 * already if this is a non-CIPUT perimiter, and that an appropriate
 * claim has been made.  So we do all that work before dropping the
 * SQLOCK with our claim.
 *
 * If we cannot proceed with the putproc/fast-fill, we just fall
 * through to the qfill_syncq, and then tail processing.  If state
 * has changed in that cycle, or wakeups are needed, it will occur
 * there.
 */
void
putnext(queue_t *qp, mblk_t *mp)
{
	queue_t		*fqp = qp; /* For strft tracing */
	syncq_t		*sq;
	uint16_t	flags;
	uint16_t	drain_mask;
	struct qinit	*qi;
	int		(*putproc)();
	struct stdata	*stp;
	int		ix;
	boolean_t	queued = B_FALSE;
	kmutex_t	*sdlock = NULL;
	kmutex_t	*sqciplock = NULL;
	ushort_t	*sqcipcount = NULL;

	TRACE_2(TR_FAC_STREAMS_FR, TR_PUTNEXT_START,
		"putnext_start:(%X, %X)", qp, mp);

	ASSERT(mp->b_datap->db_ref != 0);
	ASSERT(mp->b_next == NULL && mp->b_prev == NULL);
	stp = STREAM(qp);
	ASSERT(stp != NULL);
	if (stp->sd_ciputctrl != NULL) {
		ix = CPU->cpu_seqid & stp->sd_nciputctrl;
		sdlock = &stp->sd_ciputctrl[ix].ciputctrl_lock;
		mutex_enter(sdlock);
	} else {
		mutex_enter(sdlock = &stp->sd_lock);
	}
	qp = qp->q_next;
	sq = qp->q_syncq;
	ASSERT(sq != NULL);
	ASSERT(MUTEX_NOT_HELD(SQLOCK(sq)));
	qi = qp->q_qinfo;

	if (sq->sq_ciputctrl != NULL) {
		/* fastlock: */
		ASSERT(sq->sq_flags & SQ_CIPUT);
		ix = CPU->cpu_seqid & sq->sq_nciputctrl;
		sqciplock = &sq->sq_ciputctrl[ix].ciputctrl_lock;
		sqcipcount = &sq->sq_ciputctrl[ix].ciputctrl_count;
		mutex_enter(sqciplock);
		if (!((*sqcipcount) & SQ_FASTPUT) ||
		    (sq->sq_flags & (SQ_STAYAWAY|SQ_EXCL|SQ_EVENTS))) {
			mutex_exit(sqciplock);
			sqciplock = NULL;
			goto slowlock;
		}
		mutex_exit(sdlock);
		(*sqcipcount)++;
		ASSERT(*sqcipcount != 0);
		queued = qp->q_sqflags & Q_SQQUEUED;
		mutex_exit(sqciplock);
	} else {
	    slowlock:
		ASSERT(sqciplock == NULL);
		mutex_enter(SQLOCK(sq));
		mutex_exit(sdlock);
		flags = sq->sq_flags;
		/*
		 * We are going to drop SQLOCK, so make a claim to prevent syncq
		 * from closing.
		 */
		sq->sq_count++;
		ASSERT(sq->sq_count != 0);		/* Wraparound */
		/*
		 * If there are writers or exclusive waiters, there
		 * is not much we can do.  turn fastput off and let
		 * the slowpath handle it.
		 */
		if ((flags & (SQ_STAYAWAY|SQ_EXCL|SQ_EVENTS)) ||
		    (sq->sq_needexcl != 0)) {
			goto has_writers;
		}

		queued = qp->q_sqflags & Q_SQQUEUED;
		/*
		 * If not a concurrent perimiter, we need to acquire
		 * it exclusively.  It could not have been previously
		 * set since we held the SQLOCK before testing
		 * SQ_GOAWAY above (which includes SQ_EXCL).
		 * We do this here because we hold the SQLOCK, and need
		 * to make this state change BEFORE dropping it.
		 */
		if (!(flags & SQ_CIPUT)) {
			ASSERT((sq->sq_flags & SQ_EXCL) == 0);
			ASSERT(!(sq->sq_type & SQ_CIPUT));
			sq->sq_flags |= SQ_EXCL;
		}
		mutex_exit(SQLOCK(sq));
	}

	ASSERT((sq->sq_flags & (SQ_EXCL|SQ_CIPUT)));
	ASSERT(MUTEX_NOT_HELD(SQLOCK(sq)));

	/*
	 * We now have a claim on the syncq, we are either going to
	 * put the message on the syncq and then drain it, or we are
	 * going to call the putproc().
	 */
	putproc = qi->qi_putp;
	if (!queued) {
		STR_FTEVENT_MSG(mp, fqp, FTEV_PUTNEXT, mp->b_rptr -
		    mp->b_datap->db_base);
		(*putproc)(qp, mp);
		ASSERT(MUTEX_NOT_HELD(SQLOCK(sq)));
		ASSERT(MUTEX_NOT_HELD(QLOCK(qp)));
	} else {
		mutex_enter(QLOCK(qp));
		/*
		 * If there are no messages in front of us, just call putproc(),
		 * otherwise enqueue the message and drain the queue.
		 */
		if (qp->q_syncqmsgs == 0) {
			mutex_exit(QLOCK(qp));
			STR_FTEVENT_MSG(mp, fqp, FTEV_PUTNEXT, mp->b_rptr -
			    mp->b_datap->db_base);
			(*putproc)(qp, mp);
			ASSERT(MUTEX_NOT_HELD(SQLOCK(sq)));
		} else {
			/*
			 * We are doing a fill with the intent to
			 * drain (meaning we are filling because
			 * there are messages in front of us ane we
			 * need to preserve message ordering)
			 * Therefore, put the message on the queue
			 * and call qdrain_syncq (must be done with
			 * the QLOCK held).
			 */
			STR_FTEVENT_MSG(mp, fqp, FTEV_PUTNEXT,
			    mp->b_rptr - mp->b_datap->db_base);

#ifdef DEBUG
			/*
			 * These two values were in the original code for
			 * all syncq messages.  This is unnecessary in
			 * the current implementation, but was retained
			 * in debug mode as it is usefull to know where
			 * problems occur.
			 */
			mp->b_queue = qp;
			mp->b_prev = (mblk_t *)putproc;
#endif
			SQPUT_MP(qp, mp);
			qdrain_syncq(sq, qp);
			ASSERT(MUTEX_NOT_HELD(QLOCK(qp)));
		}
	}
	/*
	 * Before we release our claim, we need to see if any
	 * events were posted. If the syncq is SQ_EXCL && SQ_QUEUED,
	 * we were responsible for going exclusive and, therefore,
	 * are resposible for draining.
	 */
	if (sq->sq_flags & (SQ_EXCL)) {
		drain_mask = 0;
	} else {
		drain_mask = SQ_QUEUED;
	}

	if (sqciplock != NULL) {
		mutex_enter(sqciplock);
		flags = sq->sq_flags;
		ASSERT(flags & SQ_CIPUT);
		/* SQ_EXCL could have been set by qwriter_inner */
		if ((flags & (SQ_EXCL|SQ_TAIL)) || sq->sq_needexcl) {
			/*
			 * we need SQLOCK to handle
			 * wakeups/drains/flags change.  sqciplock
			 * is needed to decrement sqcipcount.
			 * SQLOCK has to be grabbed before sqciplock
			 * for lock ordering purposes.
			 * after sqcipcount is decremented some lock
			 * still needs to be held to make sure
			 * syncq won't get freed on us.
			 *
			 * To prevent deadlocks we try to grab SQLOCK and if it
			 * is held already we drop sqciplock, acquire SQLOCK and
			 * reacqwire sqciplock again.
			 */
			if (mutex_tryenter(SQLOCK(sq)) == 0) {
				mutex_exit(sqciplock);
				mutex_enter(SQLOCK(sq));
				mutex_enter(sqciplock);
			}
			flags = sq->sq_flags;
			ASSERT(*sqcipcount != 0);
			(*sqcipcount)--;
			mutex_exit(sqciplock);
		} else {
			ASSERT(*sqcipcount != 0);
			(*sqcipcount)--;
			mutex_exit(sqciplock);
			TRACE_3(TR_FAC_STREAMS_FR, TR_PUTNEXT_END,
			"putnext_end:(%X, %X, %X) done", qp, mp, sq);
			return;
		}
	} else {
		mutex_enter(SQLOCK(sq));
		flags = sq->sq_flags;
		ASSERT(sq->sq_count != 0);
		sq->sq_count--;
	}
	if ((flags & (SQ_TAIL)) || sq->sq_needexcl) {
		putnext_tail(sq, qp, (flags & ~drain_mask));
		/*
		 * The only purpose of this ASSERT is to preserve calling stack
		 * in DEBUG kernel.
		 */
		ASSERT(sq != NULL);
		return;
	}
	ASSERT((sq->sq_flags & (SQ_EXCL|SQ_CIPUT)) || queued);
	ASSERT((flags & (SQ_EXCL|SQ_CIPUT)) || queued);
	/*
	 * Safe to always drop SQ_EXCL:
	 *	Not SQ_CIPUT means we set SQ_EXCL above
	 *	For SQ_CIPUT SQ_EXCL will only be set if the put
	 *	procedure did a qwriter(INNER) in which case
	 *	nobody else is in the inner perimeter and we
	 *	are exiting.
	 */
	ASSERT((flags & (SQ_EXCL|SQ_CIPUT)) != (SQ_EXCL|SQ_CIPUT) ||
		sq->sq_count == 0);

	sq->sq_flags = flags & ~SQ_EXCL;
	mutex_exit(SQLOCK(sq));
	TRACE_3(TR_FAC_STREAMS_FR, TR_PUTNEXT_END,
	    "putnext_end:(%X, %X, %X) done", qp, mp, sq);
	return;
	/* NOTREACHED */


has_writers:
	/*
	 * There were writers, we couldn't call putnext, or even
	 * qfill_syncq with a claim, so we need to release
	 * or claim, grab the QLOCK, and call qfill_syncq
	 * The intention here is to not drain, though we will
	 * if the conditions change (in putnext_tail).
	 */
	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	/*
	 * To prevent deadlocks we try to acquire QLOCK and if it
	 * is held already we drop SQLOCK, acquire SQLOCK and
	 * reacqwire SQLOCK again.
	 */
	if (mutex_tryenter(QLOCK(qp)) == 0) {
		mutex_exit(SQLOCK(sq));
		mutex_enter(QLOCK(qp));
		mutex_enter(SQLOCK(sq));
	}
	sq->sq_count--;
	qfill_syncq(sq, qp, mp);

	TRACE_3(TR_FAC_STREAMS_FR, TR_PUTNEXT_END,
	    "putnext_end:(%X, %X, %X) SQ_EXCL fill", qp, mp, sq);
	/* In case waiters or events were posted */
	putnext_tail(sq, qp, 0);
	/*
	 * This ASSERT is located here to prevent stack frame consumption in the
	 * DEBUG code.
	 */
	ASSERT(sqciplock == NULL);
}


/*
 * wrapper for qi_putp entry in module ops vec.
 * implements asynchronous putnext().
 * Note, that unlike putnext(), this routine is NOT optimized for the
 * fastpath.  Calling this routine will grab whatever locks are necessary
 * to protect the stream head, q_next, and syncq's.  And, it will call
 * fill_syncq and drain_syncq (not the queue versions).
 * And since it is in the normal locks path, we do not use putlocks if
 * they exist (though this can be changed by swapping the value of
 * UseFastlocks).
 */
void
put(queue_t *qp, mblk_t *mp)
{
	queue_t		*fqp = qp; /* For strft tracing */
	syncq_t		*sq;
	uint16_t	flags;
	uint16_t	drain_mask;
	struct qinit	*qi;
	int		(*putproc)();
	int		ix;
	boolean_t	queued = B_FALSE;
	kmutex_t	*sqciplock = NULL;
	ushort_t	*sqcipcount = NULL;

	TRACE_2(TR_FAC_STREAMS_FR, TR_PUT_START,
		"put:(%X, %X)", qp, mp);
	ASSERT(mp->b_datap->db_ref != 0);
	ASSERT(mp->b_next == NULL && mp->b_prev == NULL);

	sq = qp->q_syncq;
	ASSERT(sq != NULL);
	qi = qp->q_qinfo;

	if (UseFastlocks && sq->sq_ciputctrl != NULL) {
		/* fastlock: */
		ASSERT(sq->sq_flags & SQ_CIPUT);
		ix = CPU->cpu_seqid & sq->sq_nciputctrl;
		sqciplock = &sq->sq_ciputctrl[ix].ciputctrl_lock;
		sqcipcount = &sq->sq_ciputctrl[ix].ciputctrl_count;
		mutex_enter(sqciplock);
		if (!((*sqcipcount) & SQ_FASTPUT) ||
		    (sq->sq_flags & (SQ_STAYAWAY|SQ_EXCL|SQ_EVENTS))) {
			mutex_exit(sqciplock);
			sqciplock = NULL;
			goto slowlock;
		}
		(*sqcipcount)++;
		ASSERT(*sqcipcount != 0);
		queued = qp->q_sqflags & Q_SQQUEUED;
		mutex_exit(sqciplock);
	} else {
	    slowlock:
		ASSERT(sqciplock == NULL);
		mutex_enter(SQLOCK(sq));
		flags = sq->sq_flags;
		/*
		 * We are going to drop SQLOCK, so make a claim to prevent syncq
		 * from closing.
		 */
		sq->sq_count++;
		ASSERT(sq->sq_count != 0);		/* Wraparound */
		/*
		 * If there are writers or exclusive waiters, there
		 * is not much we can do.  turn fastput off and let
		 * the slowpath handle it.
		 */
		if ((flags & (SQ_STAYAWAY|SQ_EXCL|SQ_EVENTS)) ||
		    (sq->sq_needexcl != 0)) {
			goto puthas_writers;
		}

		queued = qp->q_sqflags & Q_SQQUEUED;
		/*
		 * If not a concurrent perimiter, we need to acquire
		 * it exclusively.  It could not have been previously
		 * set since we held the SQLOCK before testing
		 * SQ_GOAWAY above (which includes SQ_EXCL).
		 * We do this here because we hold the SQLOCK, and need
		 * to make this state change BEFORE dropping it.
		 */
		if (!(flags & SQ_CIPUT)) {
			ASSERT((sq->sq_flags & SQ_EXCL) == 0);
			ASSERT(!(sq->sq_type & SQ_CIPUT));
			sq->sq_flags |= SQ_EXCL;
		}
		mutex_exit(SQLOCK(sq));
	}

	ASSERT((sq->sq_flags & (SQ_EXCL|SQ_CIPUT)));
	ASSERT(MUTEX_NOT_HELD(SQLOCK(sq)));

	/*
	 * We now have a claim on the syncq, we are either going to
	 * put the message on the syncq and then drain it, or we are
	 * going to call the putproc().
	 */
	putproc = qi->qi_putp;
	if (!queued) {
		STR_FTEVENT_MSG(mp, fqp, FTEV_PUTNEXT, mp->b_rptr -
		    mp->b_datap->db_base);
		(*putproc)(qp, mp);
		ASSERT(MUTEX_NOT_HELD(SQLOCK(sq)));
		ASSERT(MUTEX_NOT_HELD(QLOCK(qp)));
	} else {
		mutex_enter(QLOCK(qp));
		/*
		 * If there are no messages in front of us, just call putproc(),
		 * otherwise enqueue the message and drain the queue.
		 */
		if (qp->q_syncqmsgs == 0) {
			mutex_exit(QLOCK(qp));
			STR_FTEVENT_MSG(mp, fqp, FTEV_PUTNEXT, mp->b_rptr -
			    mp->b_datap->db_base);
			(*putproc)(qp, mp);
			ASSERT(MUTEX_NOT_HELD(SQLOCK(sq)));
		} else {
			/*
			 * We are doing a fill with the intent to
			 * drain (meaning we are filling because
			 * there are messages in front of us ane we
			 * need to preserve message ordering)
			 * Therefore, put the message on the queue
			 * and call qdrain_syncq (must be done with
			 * the QLOCK held).
			 */
			STR_FTEVENT_MSG(mp, fqp, FTEV_PUTNEXT,
			    mp->b_rptr - mp->b_datap->db_base);

#ifdef DEBUG
			/*
			 * These two values were in the original code for
			 * all syncq messages.  This is unnecessary in
			 * the current implementation, but was retained
			 * in debug mode as it is usefull to know where
			 * problems occur.
			 */
			mp->b_queue = qp;
			mp->b_prev = (mblk_t *)putproc;
#endif
			SQPUT_MP(qp, mp);
			qdrain_syncq(sq, qp);
			ASSERT(MUTEX_NOT_HELD(QLOCK(qp)));
		}
	}
	/*
	 * Before we release our claim, we need to see if any
	 * events were posted. If the syncq is SQ_EXCL && SQ_QUEUED,
	 * we were responsible for going exclusive and, therefore,
	 * are resposible for draining.
	 */
	if (sq->sq_flags & (SQ_EXCL)) {
		drain_mask = 0;
	} else {
		drain_mask = SQ_QUEUED;
	}

	if (sqciplock != NULL) {
		mutex_enter(sqciplock);
		flags = sq->sq_flags;
		ASSERT(flags & SQ_CIPUT);
		/* SQ_EXCL could have been set by qwriter_inner */
		if ((flags & (SQ_EXCL|SQ_TAIL)) || sq->sq_needexcl) {
			/*
			 * we need SQLOCK to handle
			 * wakeups/drains/flags change.  sqciplock
			 * is needed to decrement sqcipcount.
			 * SQLOCK has to be grabbed before sqciplock
			 * for lock ordering purposes.
			 * after sqcipcount is decremented some lock
			 * still needs to be held to make sure
			 * syncq won't get freed on us.
			 *
			 * To prevent deadlocks we try to grab SQLOCK and if it
			 * is held already we drop sqciplock, acquire SQLOCK and
			 * reacqwire sqciplock again.
			 */
			if (mutex_tryenter(SQLOCK(sq)) == 0) {
				mutex_exit(sqciplock);
				mutex_enter(SQLOCK(sq));
				mutex_enter(sqciplock);
			}
			flags = sq->sq_flags;
			ASSERT(*sqcipcount != 0);
			(*sqcipcount)--;
			mutex_exit(sqciplock);
		} else {
			ASSERT(*sqcipcount != 0);
			(*sqcipcount)--;
			mutex_exit(sqciplock);
			TRACE_3(TR_FAC_STREAMS_FR, TR_PUTNEXT_END,
			"putnext_end:(%X, %X, %X) done", qp, mp, sq);
			return;
		}
	} else {
		mutex_enter(SQLOCK(sq));
		flags = sq->sq_flags;
		ASSERT(sq->sq_count != 0);
		sq->sq_count--;
	}
	if ((flags & (SQ_TAIL)) || sq->sq_needexcl) {
		putnext_tail(sq, qp, (flags & ~drain_mask));
		/*
		 * The only purpose of this ASSERT is to preserve calling stack
		 * in DEBUG kernel.
		 */
		ASSERT(sq != NULL);
		return;
	}
	ASSERT((sq->sq_flags & (SQ_EXCL|SQ_CIPUT)) || queued);
	ASSERT((flags & (SQ_EXCL|SQ_CIPUT)) || queued);
	/*
	 * Safe to always drop SQ_EXCL:
	 *	Not SQ_CIPUT means we set SQ_EXCL above
	 *	For SQ_CIPUT SQ_EXCL will only be set if the put
	 *	procedure did a qwriter(INNER) in which case
	 *	nobody else is in the inner perimeter and we
	 *	are exiting.
	 */
	ASSERT((flags & (SQ_EXCL|SQ_CIPUT)) != (SQ_EXCL|SQ_CIPUT) ||
		sq->sq_count == 0);

	sq->sq_flags = flags & ~SQ_EXCL;
	mutex_exit(SQLOCK(sq));
	TRACE_3(TR_FAC_STREAMS_FR, TR_PUTNEXT_END,
	    "putnext_end:(%X, %X, %X) done", qp, mp, sq);
	return;
	/* NOTREACHED */


puthas_writers:
	/*
	 * There were writers, we couldn't call putnext, or even
	 * qfill_syncq with a claim, so we need to release
	 * or claim, grab the QLOCK, and call qfill_syncq
	 * The intention here is to not drain, though we will
	 * if the conditions change (in putnext_tail).
	 */
	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	/*
	 * To prevent deadlocks we try to acquire QLOCK and if it
	 * is held already we drop SQLOCK, acquire SQLOCK and
	 * reacqwire SQLOCK again.
	 */
	if (mutex_tryenter(QLOCK(qp)) == 0) {
		mutex_exit(SQLOCK(sq));
		mutex_enter(QLOCK(qp));
		mutex_enter(SQLOCK(sq));
	}
	sq->sq_count--;
	qfill_syncq(sq, qp, mp);

	TRACE_3(TR_FAC_STREAMS_FR, TR_PUTNEXT_END,
	    "putnext_end:(%X, %X, %X) SQ_EXCL fill", qp, mp, sq);
	/* In case waiters or events were posted */
	putnext_tail(sq, qp, 0);
	/*
	 * This ASSERT is located here to prevent stack frame consumption in the
	 * DEBUG code.
	 */
	ASSERT(sqciplock == NULL);
}
