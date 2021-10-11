/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)slp.c	1.56	99/12/07 SMI"

#include "libthread.h"
#include "tdb_agent.h"

/*
 * Global variable
 */
struct slpq _slpq[NSLEEPQ];

/*
 * Static functions
 */
static	int _iswanted(struct thread *t, char *chan);
static	int _valid_slpq(int *bucket);


#define	Q_INVALID(rc, b) { *bucket = (b); return (rc); }

#define	ASSERT_SQ_VALID { \
int rc, b; \
if ((rc = _valid_slpq(&b)) != 1) {\
	printf("Invalid Sleepq, err code %d, bucket %d, chan 0x%x\n", \
	rc, b, chan); \
	_panic("Invalid Sleepq"); \
}\
}

#define	ASSERT_SQ_VALID1 { \
int rc, b; \
if ((rc = _valid_slpq(&b)) != 1) {\
	printf("Invalid Sleepq, err code %d, bucket %d\n", rc, b); \
	_panic("Invalid Sleepq"); \
}\
}

#define	MUTATOR_SUSPENDED(t) ( \
	(_suspendingallmutators || _suspendedallmutators) && \
	((t)->t_mutator ^ (t)->t_mutatormask) \
)

void
_t_block(caddr_t chan)
{
	struct slpq *spq;
	uthread_t *t = curthread;
	uthread_t *nt, *prev;
	int pri = DISP_PRIO(t);

	ASSERT(LOCK_HELD(&_schedlock.mutex_lockw));
	ASSERT(t->t_link == NULL);
	ASSERT(t->t_state == TS_ONPROC || t->t_state == TS_DISP);

	t->t_link = NULL;
	t->t_state = TS_SLEEP;
	t->t_wchan = (caddr_t)chan;
	spq = slpqhash(chan);
	if (spq->sq_first == NULL) {
		spq->sq_first = spq->sq_last = t;
	} else if (pri <= DISP_PRIO(spq->sq_last)) {
			spq->sq_last->t_link = t;
			spq->sq_last = t;
			t->t_link = NULL;
	} else {
		prev = nt = spq->sq_first;
		while (pri <= DISP_PRIO(nt)) {
			prev = nt;
			nt = nt->t_link;
		}
		if (nt == NULL) {
			t->t_link = NULL;
			prev->t_link = t;
			spq->sq_last = t;
		} else {
			if (prev == nt) {
				t->t_link = prev;
				spq->sq_first = t;
			} else {
				t->t_link = nt;
				prev->t_link = t;
			}
		}
	}
	/*
	 * delete from onproc q if blocking thread isn't bound
	 * and not already off the proc q due to preemption or stopping.
	 */
	if (!ISBOUND(t) && ONPROCQ(t)) {
		_onproc_deq(t);
	}
	if (__td_event_report(t, TD_SLEEP)) {
		t->t_td_evbuf.eventnum = TD_SLEEP;
		t->t_td_evbuf.eventdata = chan;
		tdb_event_sleep();
	}
}

int
_t_release(caddr_t chan, u_char *waiters, int sync_type)
{
	struct slpq *spq;
	struct thread *t, *pt, **prev, **first;
	struct thread *st, *spt, **sprev;
	int rval = 0;

	ITRACE_1(UTR_FAC_TLIB_DISP, UTR_T_RELEASE_START,
	    "_t_release_start:chan 0x%x", (u_long)chan);
	ASSERT(LOCK_HELD(&_schedlock.mutex_lockw));

	spq = slpqhash(chan);
	first = prev = &spq->sq_first;
	pt = NULL;
	while (t = *prev) {
		if (t->t_wchan == (caddr_t)chan) {

			/*
			 * If a cancellation is pending for this thread
			 * on a condition variable skip over it.
			 * If we we do not skip over such
			 * a thread, then it will be woken up here, which
			 * would consume the signal on the condition
			 * variable. A second thread waiting on the same
			 * condition would not be woken up as intended.
			 * See bugid 4214994.
			 */
			if (sync_type == T_WAITCV && CANCELABLE(t) &&
			    CANCELENABLE(t) && CANCELPENDING(t))
				goto skip;

			/*
			 * When a thread is found sleeping for the "chan",
			 * verify the type of chan the thread is asleep for.
			 * If _t_release() is called for a mutex (sync_type
			 * is 0), but the thread is asleep for a CV (T_WAITCV
			 * flag set), OR vice versa, return failure.
			 * The following depends on the sync_type being 0 for
			 * mutexes. Since the wchan represents either a cv or
			 * a mutex, it is enough to do the following binary
			 * check. The following condition will be typically
			 * false. It could be true in rare situtations such as:
			 * For a mutex, after the lock is cleared but before
			 * the wait bit is read, the lock memory could get
			 * re-used to now point to a CV, and the thread found
			 * here is sleeping for this CV, although the call to
			 * _t_release() has been made for a mutex.
			 * For a CV, it may happen that if a cond_signal
			 * occurs without holding the associated lock, the cv
			 * memory may get re-used and now could point to a mutex
			 * which some thread is waiting for here.
			 * Both conditions are rare, but if one occurs, return
			 * failure here.
			 * If the memory gets re-used to point to another object
			 * of the same type, the condition below is false but
			 * this will only result in a spurious wake-up.
			 */
			if (sync_type != (t->t_flag & T_WAITCV))
				return (0);
			t->t_wchan = 0;
			/* remove stopped threads. */
			if (t->t_stop || MUTATOR_SUSPENDED(t)) {
				if ((*prev = t->t_link) == NULL)
					spq->sq_last = pt;
				t->t_link = NULL;
				_setrq(t);
				continue;
			}
			if ((*prev = t->t_link) == NULL)
				spq->sq_last = pt;
			*waiters = _iswanted(t->t_link, chan);
			t->t_link = 0;
			/*
			 * Ensure that this wake up is not missed due to
			 * a cancel. See _canceloff() for more on this.
			 * Do NOT disable cancellation for mutexes that
			 * have cancellation pending, but instead wake the
			 * thread up and take the cancellation imediately.
			 * See bugid 4214994.
			 */
			if (!(sync_type != T_WAITCV && CANCELABLE(t) &&
			    CANCELENABLE(t) && CANCELPENDING(t)))
				t->t_cancelable = ~TC_CANCELABLE;
			_setrq(t);
			ITRACE_3(UTR_FAC_TLIB_DISP, UTR_T_RELEASE_END,
			    "_t_release end:chan 0x%x, tid 0x%x, waiters %d",
			    chan, t->t_tid, *waiters);
			return (1);
		}
skip:
		prev = &t->t_link;
		pt = t;
	}
	ITRACE_1(UTR_FAC_TLIB_DISP, UTR_T_RELEASE_END,
	    "_t_release end:chan 0x%x no thread released", chan);
	*waiters = 0;
	return (0);
}

void
_t_release_all(caddr_t chan)
{
	struct slpq *spq;
	uthread_t *t, *pt, **prev;

	ASSERT(LOCK_HELD(&_schedlock.mutex_lockw));

	spq = slpqhash(chan);
	prev = &spq->sq_first;
	pt = NULL;
	while (t = *prev) {
		ASSERT(t->t_link != t);
		if (t->t_wchan == (caddr_t)chan) {
			if ((*prev = t->t_link) == NULL)
				spq->sq_last = pt;
			t->t_wchan = 0;
			t->t_link = NULL;
			_setrq(t);
			continue;
		}
		prev = &t->t_link;
		pt = t;
	}
}

void
_unsleep(struct thread *t)
{
	struct slpq *spq;
	struct thread *tt, *pt, **prev;

	ASSERT(t->t_wchan != NULL && t->t_state == TS_SLEEP);
	spq = slpqhash(t->t_wchan);
	prev = &spq->sq_first;
	pt = NULL;
	ASSERT (*prev != NULL);
	while (tt = *prev) {
		if (tt == t) {
			*prev = t->t_link;
			if (*prev == NULL)
				spq->sq_last = pt;
			t->t_link = NULL;
			t->t_wchan = NULL;
			return;
		}
		prev = &tt->t_link;
		pt = tt;
	}
}

void
_setrun(struct thread *t)
{
	_sched_lock();
	if (t->t_state == TS_RUN || t->t_state == TS_ONPROC ||
	    t->t_state == TS_DISP) {
		/*
		 * Already on run queue.
		 */
		_sched_unlock();
		return;
	} else if (t->t_state == TS_SLEEP) {
		/*
		 * Take off sleep queue.
		 */
		_unsleep(t);
	}
	_setrq(t);
	_sched_unlock();
}

static int
_iswanted(struct thread *t, char *chan)
{
	while (t != NULL) {
		if (t->t_wchan == chan)
			return (1);
		t = t->t_link;
	}
	return (0);
}

#ifdef DEBUG
/*
 * checks the slpq for validity. Returns 1 if slpq is valid.
 * Returns numbers from 2 through 16 if the slpq is invalid, each
 * number represents a specific reason for the invalidity, e.g. a return
 * code of 9 means one of the sleep channels of the sleep q had a loop in its
 * linked list. The bucket number is returned in bucket.
 */

static int
_valid_slpq(int *bucket)
{
	int i;
	struct slpq *sq;
	struct thread *next, *prev, *t, *temp;

	ASSERT(LOCK_HELD(&_schedlock.mutex_lockw));
	sq = (struct slpq *)&_slpq[0];
	for (i = 0; i < NSLEEPQ; sq++, i++) {
		if (sq->sq_first == NULL && sq->sq_last != NULL)
			Q_INVALID(2, i)
		if (sq->sq_first != NULL && sq->sq_last == NULL)
			Q_INVALID(3, i)
		/*
		 * after this point, first and last are both either NULL or
		 * non-NULL.
		 */
		if (sq->sq_first == NULL) /* empty sleep channel */
			continue;
		if (sq->sq_first == sq->sq_last) /* one sleeper */
			if (sq->sq_first->t_link != NULL)
				Q_INVALID(4, i) /* wrong termination */
		if (sq->sq_last->t_link != NULL)
			Q_INVALID(5, i) /* wrong termination */
		if (sq->sq_first == sq->sq_last) /* and both have NULL t_link */
			if (sq != slpqhash(sq->sq_first->t_wchan))
				Q_INVALID(6, i) /* not in right bucket */
			else if (sq->sq_first->t_state != TS_SLEEP &&
				    sq->sq_first->t_state != TS_STOPPED)
				Q_INVALID(7, i)
			else
				continue;
		/*
		 * At this point, sq->sq_first != sq->sq_last
		 * Now detect circularity in linked list.
		 * Use the following O(n) algorithm :
		 * Traverse the list and rewrite each node to point to the
		 * previous node, thus reversing the list. If a NULL pointer
		 * is not encountered and the start of the list is encountered
		 * again, the list has a loop. If a NULL is encountered, check
		 * if the last element is equal to sq->last, then
		 * retrace the traversal, reversing the pointers once again
		 * and leaving the valid list as you found it.
		 */
		    prev = (struct thread *)sq;
		    t = sq->sq_first;
		    next = sq->sq_first->t_link;
		    if (next == NULL) /* if only one thread */
			Q_INVALID(8, i)
			/*
			 * at this point, the list should have at least 2
			 * threads.
			 */
		while (next != NULL && next != (struct thread *)sq) {
			t->t_link = prev;
			prev = t;
			t = next;
			next = t->t_link;
		}
		if (next == (struct thread *)sq)
			Q_INVALID(9, i) /* list has a loop */
		if (next == NULL && sq->sq_last == t) {
			/* list OK */
			if (sq != slpqhash(t->t_wchan))
				Q_INVALID(10, i) /* not in right bucket */
			else if (t->t_state != TS_SLEEP &&
				    t->t_state != TS_STOPPED)
				Q_INVALID(11, i)
			/* set-up for retrace */
			next = prev->t_link;
			temp = prev; /* swap prev and t */
			prev = t;
			t = temp;
			if (sq != slpqhash(t->t_wchan))
				Q_INVALID(12, i) /* not in right bucket */
			else if (t->t_state != TS_SLEEP &&
				    t->t_state != TS_STOPPED)
				Q_INVALID(13, i)
			while (next != (struct thread *)sq) { /* retrace */
				t->t_link = prev;
				prev = t;
				t = next;
				next = t->t_link;
				if (sq != slpqhash(t->t_wchan))
					Q_INVALID(14, i)
				else if (t->t_state != TS_SLEEP &&
					    t->t_state != TS_STOPPED)
					Q_INVALID(15, i)
					/* not in right bucket */
			}
			t->t_link = prev;
			continue; /* go to next sleep channel */
		} else
			Q_INVALID(16, i) /* last element, not match sq_last */
	}
	return (1); /* valid sleep q */
}
#endif
