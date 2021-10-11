/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)aio_subr.c	1.43	99/11/26 SMI"

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <vm/as.h>
#include <vm/page.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/aio_impl.h>
#include <sys/epm.h>
#include <sys/fs/snode.h>
#include <sys/siginfo.h>
#include <sys/cpuvar.h>
#include <sys/tnf_probe.h>

int aphysio(int (*)(), int (*)(), dev_t, int, void (*)(), struct aio_req *);
void aio_done(struct buf *);
void aphysio_unlock(aio_req_t *);
void aio_cleanup(int);
void aio_cleanup_exit(void);
extern int sulword(void *, ulong_t);

/*
 * private functions
 */
static void aio_sigev_send(proc_t *, sigqueue_t *);
static void aio_hash_delete(aio_t *, aio_req_t *);
static void aio_lio_free(aio_t *, aio_lio_t *);
static void aio_enq(aio_req_t **, aio_req_t *, int, int);
static void aio_cleanup_cleanupq(aio_t *, aio_req_t *, int);
static int aio_cleanup_notifyq(aio_t *, aio_req_t *, int);
static void aio_cleanup_pollq(aio_t *, aio_req_t *, int);

/*
 * async version of physio() that doesn't wait synchronously
 * for the driver's strategy routine to complete.
 */

int
aphysio(
	int (*strategy)(struct buf *),
	int (*cancel)(struct buf *),
	dev_t dev,
	int rw,
	void (*mincnt)(struct buf *),
	struct aio_req *aio)
{
	struct uio *uio = aio->aio_uio;
	aio_req_t *reqp = (aio_req_t *)aio->aio_private;
	struct buf *bp = &reqp->aio_req_buf;
	struct iovec *iov;
	struct as *as;
	char *a;
	int	error;
	size_t	c;
	struct page **pplist;

	/*
	 * Large Files: We do the check against SPEC_MAXOFFSET_T
	 * instead of MAXOFFSET_T because the value represents the
	 * maximum size that can be supported by specfs.
	 */

	if (uio->uio_loffset < 0 || uio->uio_loffset > SPEC_MAXOFFSET_T) {
		return (EINVAL);
	}

	TNF_PROBE_5(aphysio_start, "kaio", /* CSTYLED */,
		tnf_opaque, bp, bp,
		tnf_device, device, dev,
		tnf_offset, blkno, btodt(uio->uio_loffset),
		tnf_size, size, uio->uio_iov->iov_len,
		tnf_bioflags, rw, rw);

	if (rw == B_READ) {
		CPU_STAT_ADD_K(cpu_sysinfo.phread, 1);
	} else {
		CPU_STAT_ADD_K(cpu_sysinfo.phwrite, 1);
	}

	iov = uio->uio_iov;
	sema_init(&bp->b_sem, 0, NULL, SEMA_DEFAULT, NULL);
	sema_init(&bp->b_io, 0, NULL, SEMA_DEFAULT, NULL);

	bp->b_oerror = 0;		/* old error field */
	bp->b_error = 0;
	bp->b_flags = B_KERNBUF | B_BUSY | B_PHYS | B_ASYNC | rw;
	bp->b_edev = dev;
	bp->b_dev = cmpdev(dev);
	bp->b_lblkno = btodt(uio->uio_loffset);

	/*
	 * Clustering: Clustering can set the b_iodone, b_forw and
	 * b_proc fields to cluster-specifc values.
	 */
	if (bp->b_iodone == NULL) {
		bp->b_iodone = (int (*)()) aio_done;
		/* b_forw points at an aio_req_t structure */
		bp->b_forw = (struct buf *)reqp;
		bp->b_proc = curproc;
	}

	a = bp->b_un.b_addr = iov->iov_base;
	c = bp->b_bcount = iov->iov_len;

	(*mincnt)(bp);
	if (bp->b_bcount != iov->iov_len)
		return (ENOTSUP);

	as = bp->b_proc->p_as;

	error = as_pagelock(as, &pplist, a,
	    c, rw == B_READ? S_WRITE : S_READ);
	if (error != 0) {
		bp->b_flags |= B_ERROR;
		bp->b_error = error;
		bp->b_flags &= ~(B_BUSY|B_WANTED|B_PHYS|B_SHADOW);
		return (error);
	}
	bp->b_shadow = pplist;
	if (pplist != NULL) {
		bp->b_flags |= B_SHADOW;
	}

	if (cancel != anocancel)
		cmn_err(CE_PANIC,
		    "aphysio: cancellation not supported, use anocancel");

	reqp->aio_req_cancel = cancel;
	return ((*strategy)(bp));
}

/*ARGSUSED*/
int
anocancel(struct buf *bp)
{
	return (ENXIO);
}

/*
 * Called from biodone().
 * Notify process that a pending AIO has finished.
 */

/*
 * Clustering: This function is made non-static as it is used
 * by clustering s/w as contract private interface.
 */

void
aio_done(struct buf *bp)
{
	proc_t *p;
	struct as *as;
	aio_req_t *reqp;
	aio_lio_t *head;
	aio_t *aiop;
	sigqueue_t *sigev;
	sigqueue_t *lio_sigev = NULL;
	int fd;
	int cleanupqflag;
	int pollqflag;
	void (*func)();

	p = bp->b_proc;
	reqp = (aio_req_t *)bp->b_forw;
	fd = reqp->aio_req_fd;

	TNF_PROBE_5(aphysio_end, "kaio", /* CSTYLED */,
		tnf_opaque, bp, bp,
		tnf_device, device, bp->b_dev,
		tnf_offset, blkno, btodt(reqp->aio_req_uio.uio_loffset),
		tnf_size, size, reqp->aio_req_uio.uio_iov->iov_len,
		tnf_bioflags, rw, (bp->b_flags & (B_READ|B_WRITE)));

	/*
	 * mapout earlier so that more kmem is available when aio is
	 * heavily used. bug #1262082
	 */
	if (bp->b_flags & B_REMAPPED)
		bp_mapout(bp);

	/* decrement fd's ref count by one, now that aio request is done. */
	areleasef(fd, P_FINFO(p));

	aiop = p->p_aio;
	ASSERT(aiop != NULL);
	reqp->aio_req_next =  NULL;
	mutex_enter(&aiop->aio_mutex);
	ASSERT(aiop->aio_pending > 0);
	ASSERT(reqp->aio_req_flags & AIO_PENDING);
	aiop->aio_pending--;
	reqp->aio_req_flags &= ~AIO_PENDING;
	/*
	 * when the AIO_CLEANUP flag is enabled for this
	 * process, or when the AIO_POLL bit is set for
	 * this request, special handling is required.
	 * otherwise the request is put onto the doneq.
	 */
	cleanupqflag = (aiop->aio_flags & AIO_CLEANUP);
	pollqflag = (reqp->aio_req_flags & AIO_POLL);
	if (cleanupqflag | pollqflag) {
		if (cleanupqflag) {
			as = p->p_as;
			mutex_enter(&as->a_contents);
		}
		/*
		 *
		 * requests with their AIO_POLL bit set are put
		 * on the pollq, requests with sigevent structures
		 * or with listio heads are put on the notifyq, and
		 * the remaining requests don't require any special
		 * cleanup handling, so they're put onto the default
		 * cleanupq.
		 */
		if (pollqflag)
			aio_enq(&aiop->aio_pollq, reqp, AIO_POLLQ, 0);
		else if (reqp->aio_req_sigqp || reqp->aio_req_lio)
			aio_enq(&aiop->aio_notifyq, reqp, AIO_NOTIFYQ, 0);
		else
			aio_enq(&aiop->aio_cleanupq, reqp, AIO_CLEANUPQ, 0);

		if (cleanupqflag) {
			cv_signal(&aiop->aio_cleanupcv);
			mutex_exit(&as->a_contents);
			mutex_exit(&aiop->aio_mutex);
		} else {
			/*
			 * let the cleanup processing happen from an
			 * AST. set an AST on all threads in this process
			 * and wakeup anybody waiting in aiowait().
			 */
			ASSERT(pollqflag);
			cv_broadcast(&aiop->aio_waitcv);
			mutex_exit(&aiop->aio_mutex);
			mutex_enter(&p->p_lock);
			set_proc_ast(p);
			mutex_exit(&p->p_lock);
		}
	} else {
		/* put request on done queue. */
		aio_enq(&aiop->aio_doneq, reqp, AIO_DONEQ, 0);

		/*
		 * save req's sigevent pointer, and check its
		 * value after releasing aio_mutex lock.
		 */
		sigev = reqp->aio_req_sigqp;
		reqp->aio_req_sigqp = NULL;

		/*
		 * when list IO notification is enabled, a signal
		 * is sent only when all entries in the list are
		 * done.
		 */
		if ((head = reqp->aio_req_lio) != NULL) {
			ASSERT(head->lio_refcnt > 0);
			if (--head->lio_refcnt == 0) {
				cv_signal(&head->lio_notify);
				/*
				 * save lio's sigevent pointer, and check
				 * its value after releasing aio_mutex
				 * lock.
				 */
				lio_sigev = head->lio_sigqp;
				head->lio_sigqp = NULL;
			}
			mutex_exit(&aiop->aio_mutex);
			if (sigev)
				aio_sigev_send(p, sigev);
			if (lio_sigev)
				aio_sigev_send(p, lio_sigev);
		} else {
			cv_broadcast(&aiop->aio_waitcv);
			mutex_exit(&aiop->aio_mutex);
			if (sigev)
				aio_sigev_send(p, sigev);
			else {
				/*
				 * send a SIGIO signal when the process
				 * has a handler enabled.
				 */
				if ((func = p->p_user.u_signal[SIGIO - 1]) !=
				    SIG_DFL && (func != SIG_IGN))
					psignal(p, SIGIO);
			}
		}
	}
}

/*
 * send a queued signal to the specified process when
 * the event signal is non-NULL. A return value of 1
 * will indicate that a signal is queued, and 0 means that
 * no signal was specified, nor sent.
 */
static void
aio_sigev_send(proc_t *p, sigqueue_t *sigev)
{
	ASSERT(sigev != NULL);
	mutex_enter(&p->p_lock);
	sigaddqa(p, NULL, sigev);
	mutex_exit(&p->p_lock);
}

/*
 * special case handling for zero length requests. the aio request
 * short circuits the normal completion path since all that's required
 * to complete this request is to copyout a zero to the aio request's
 * return value.
 */
void
aio_zerolen(aio_req_t *reqp)
{

	struct buf *bp = &reqp->aio_req_buf;

	reqp->aio_req_flags |= AIO_ZEROLEN;

	bp->b_forw = (struct buf *)reqp;
	bp->b_proc = curproc;

	bp->b_resid = 0;
	bp->b_flags = 0;

	aio_done(bp);
}

/*
 * unlock pages previously locked by as_pagelock
 */
void
aphysio_unlock(aio_req_t *reqp)
{
	struct buf *bp;
	struct iovec *iov;
	int flags;

	if (reqp->aio_req_flags & AIO_PHYSIODONE)
		return;

	reqp->aio_req_flags |= AIO_PHYSIODONE;

	if (reqp->aio_req_flags & AIO_ZEROLEN)
		return;

	bp = &reqp->aio_req_buf;
	iov = reqp->aio_req_uio.uio_iov;
	flags = (((bp->b_flags & B_READ) == B_READ) ? S_WRITE : S_READ);
	as_pageunlock(curproc->p_as,
		bp->b_flags & B_SHADOW ? bp->b_shadow : NULL,
		iov->iov_base, iov->iov_len, flags);
	bp->b_flags &= ~(B_BUSY|B_WANTED|B_PHYS|B_SHADOW);
	bp->b_flags |= B_DONE;
}

/*
 * deletes a requests id from the hash table of outstanding
 * io.
 */
static void
aio_hash_delete(
	aio_t *aiop,
	struct aio_req_t *reqp)
{
	long index;
	aio_result_t *resultp = reqp->aio_req_resultp;
	aio_req_t *current;
	aio_req_t **nextp;

	index = AIO_HASH(resultp);
	nextp = (aiop->aio_hash + index);
	while ((current = *nextp) != NULL) {
		if (current->aio_req_resultp == resultp) {
			*nextp = current->aio_hash_next;
			return;
		}
		nextp = &current->aio_hash_next;
	}
}

/*
 * Put a list head struct onto its free list.
 */
static void
aio_lio_free(aio_t *aiop, aio_lio_t *head)
{
	ASSERT(MUTEX_HELD(&aiop->aio_mutex));

	if (head->lio_sigqp != NULL)
		kmem_free(head->lio_sigqp, sizeof (sigqueue_t));
	head->lio_next = aiop->aio_lio_free;
	aiop->aio_lio_free = head;
}

/*
 * Put a reqp onto the freelist.
 */
void
aio_req_free(aio_t *aiop, aio_req_t *reqp)
{
	aio_lio_t *liop;

	ASSERT(MUTEX_HELD(&aiop->aio_mutex));

	if ((liop = reqp->aio_req_lio) != NULL) {
		if (--liop->lio_nent == 0)
			aio_lio_free(aiop, liop);
		reqp->aio_req_lio = NULL;
	}
	if (reqp->aio_req_sigqp != NULL)
		kmem_free(reqp->aio_req_sigqp, sizeof (sigqueue_t));
	reqp->aio_req_next = aiop->aio_free;
	aiop->aio_free = reqp;
	aiop->aio_outstanding--;
	aio_hash_delete(aiop, reqp);
}

/*
 * put a completed request onto its appropiate done queue.
 */
/*ARGSUSED*/
static void
aio_enq(aio_req_t **qhead, aio_req_t *reqp, int qflg_new, int qflg_old)
{
	if (*qhead == NULL) {
		*qhead = reqp;
		reqp->aio_req_next = reqp;
		reqp->aio_req_prev = reqp;
	} else {
		reqp->aio_req_next = *qhead;
		reqp->aio_req_prev = (*qhead)->aio_req_prev;
		reqp->aio_req_prev->aio_req_next = reqp;
		(*qhead)->aio_req_prev = reqp;
	}

#ifdef DEBUG
	if (qflg_old) {
		ASSERT(reqp->aio_req_flags & qflg_old);
		reqp->aio_req_flags &= ~qflg_old;
	}
#endif

	reqp->aio_req_flags |= qflg_new;
}

/*
 * concatenate a specified queue with the cleanupq. the specified
 * queue is put onto the tail of the cleanupq. all elements on the
 * specified queue should have their aio_req_flags field cleared.
 */
/*ARGSUSED*/
void
aio_cleanupq_concat(aio_t *aiop, aio_req_t *q2, int qflg)
{
	aio_req_t *cleanupqhead, *q2tail;

#ifdef DEBUG
	aio_req_t *reqp = q2;

	do {
		ASSERT(reqp->aio_req_flags & qflg);
		reqp->aio_req_flags &= ~qflg;
		reqp->aio_req_flags |= AIO_CLEANUPQ;
	} while ((reqp = reqp->aio_req_next) != q2);
#endif

	cleanupqhead = aiop->aio_cleanupq;
	if (cleanupqhead == NULL)
		aiop->aio_cleanupq = q2;
	else {
		cleanupqhead->aio_req_prev->aio_req_next = q2;
		q2tail = q2->aio_req_prev;
		q2tail->aio_req_next = cleanupqhead;
		q2->aio_req_prev = cleanupqhead->aio_req_prev;
		cleanupqhead->aio_req_prev = q2tail;
	}
}

/*
 * cleanup aio requests that are on the per-process poll queue.
 */
void
aio_cleanup(int exitflg)
{
	aio_t *aiop = curproc->p_aio;
	aio_req_t *pollqhead, *cleanupqhead, *notifyqhead;
	int signalled = 0;
	int qflag = 0;
	void (*func)();

	ASSERT(aiop != NULL);

	/*
	 * take all the requests off the cleanupq, the notifyq,
	 * and the pollq.
	 */
	mutex_enter(&aiop->aio_mutex);
	if ((cleanupqhead = aiop->aio_cleanupq) != NULL) {
		aiop->aio_cleanupq = NULL;
		qflag++;
	}
	if ((notifyqhead = aiop->aio_notifyq) != NULL) {
		aiop->aio_notifyq = NULL;
		qflag++;
	}
	if ((pollqhead = aiop->aio_pollq) != NULL) {
		aiop->aio_pollq = NULL;
		qflag++;
	}
	mutex_exit(&aiop->aio_mutex);

	/*
	 * return immediately if cleanupq, pollq, and
	 * notifyq are all empty. someone else must have
	 * emptied them.
	 */
	if (!qflag)
		return;

	/*
	 * do cleanup for the various queues.
	 */
	if (cleanupqhead)
		aio_cleanup_cleanupq(aiop, cleanupqhead, exitflg);
	if (notifyqhead)
		signalled = aio_cleanup_notifyq(aiop, notifyqhead, exitflg);
	if (pollqhead)
		aio_cleanup_pollq(aiop, pollqhead, exitflg);

	if (exitflg)
		return;

	/*
	 * Only if the process wasn't already signalled,
	 * determine if a SIGIO signal should be delievered.
	 */
	if (!signalled &&
	    (func = curproc->p_user.u_signal[SIGIO - 1]) != SIG_DFL &&
	    func != SIG_IGN)
		psignal(curproc, SIGIO);
}

/*
 * do cleanup for every element of the cleanupq.
 */
static void
aio_cleanup_cleanupq(aio_t *aiop, aio_req_t *qhead, int exitflg)
{
	aio_req_t *reqp, *next;

	qhead->aio_req_prev->aio_req_next = NULL;
	for (reqp = qhead; reqp != NULL; reqp = next) {
		ASSERT(reqp->aio_req_flags & AIO_CLEANUPQ);
		next = reqp->aio_req_next;
		aphysio_unlock(reqp);
		if (exitflg) {
			/* reqp cann't be referenced after its freed */
			mutex_enter(&aiop->aio_mutex);
			aio_req_free(aiop, reqp);
			mutex_exit(&aiop->aio_mutex);
			continue;
		}
		mutex_enter(&aiop->aio_mutex);
		aio_enq(&aiop->aio_doneq, reqp, AIO_DONEQ, AIO_CLEANUPQ);
		mutex_exit(&aiop->aio_mutex);
	}
}

/*
 * do cleanup for every element of the notify queue.
 */
static int
aio_cleanup_notifyq(aio_t *aiop, aio_req_t *qhead, int exitflg)
{
	aio_req_t *reqp, *next;
	aio_lio_t *liohead;
	sigqueue_t *sigev, *lio_sigev = NULL;
	int signalled = 0;

	qhead->aio_req_prev->aio_req_next = NULL;
	for (reqp = qhead; reqp != NULL; reqp = next) {
		ASSERT(reqp->aio_req_flags & AIO_NOTIFYQ);
		next = reqp->aio_req_next;
		aphysio_unlock(reqp);
		if (exitflg) {
			/* reqp cann't be referenced after its freed */
			mutex_enter(&aiop->aio_mutex);
			aio_req_free(aiop, reqp);
			mutex_exit(&aiop->aio_mutex);
			continue;
		}
		mutex_enter(&aiop->aio_mutex);
		aio_enq(&aiop->aio_doneq, reqp, AIO_DONEQ, AIO_NOTIFYQ);
		sigev = reqp->aio_req_sigqp;
		reqp->aio_req_sigqp = NULL;
		/* check if list IO completion notification is required */
		if ((liohead = reqp->aio_req_lio) != NULL) {
			ASSERT(liohead->lio_refcnt > 0);
			if (--liohead->lio_refcnt == 0) {
				cv_signal(&liohead->lio_notify);
				lio_sigev = liohead->lio_sigqp;
				liohead->lio_sigqp = NULL;
			}
		}
		mutex_exit(&aiop->aio_mutex);
		if (sigev) {
			signalled++;
			aio_sigev_send(curproc, sigev);
		}
		if (lio_sigev) {
			signalled++;
			aio_sigev_send(curproc, lio_sigev);
		}
	}
	return (signalled);
}

/*
 * do cleanup for every element of the cleanup queue.
 */
static void
aio_cleanup_pollq(aio_t *aiop, aio_req_t *qhead, int exitflg)
{
	aio_req_t *reqp, *next;

	qhead->aio_req_prev->aio_req_next = NULL;
	for (reqp = qhead; reqp != NULL; reqp = next) {
		ASSERT(reqp->aio_req_flags & AIO_POLLQ);
		next = reqp->aio_req_next;
		aphysio_unlock(reqp);
		if (exitflg) {
			/* reqp cann't be referenced after its freed */
			mutex_enter(&aiop->aio_mutex);
			aio_req_free(aiop, reqp);
			mutex_exit(&aiop->aio_mutex);
			continue;
		}
		/* copy out request's result_t. */
		aio_copyout_result(reqp);
		mutex_enter(&aiop->aio_mutex);
		aio_enq(&aiop->aio_doneq, reqp, AIO_DONEQ, AIO_POLLQ);
		mutex_exit(&aiop->aio_mutex);
	}
}

/*
 * called by exit(). waits for all outstanding kaio to finish
 * before the kaio resources are freed.
 */
void
aio_cleanup_exit(void)
{
	proc_t *p = curproc;
	aio_t *aiop = p->p_aio;
	aio_req_t *reqp, *next, *head;
	aio_lio_t *nxtlio, *liop;

	/*
	 * wait for all outstanding kaio to complete. process
	 * is now single-threaded; no other kaio requests can
	 * happen once aio_pending is zero.
	 */
	mutex_enter(&aiop->aio_mutex);
	aiop->aio_flags |= AIO_CLEANUP;
	while (aiop->aio_pending != 0)
		cv_wait(&aiop->aio_cleanupcv, &aiop->aio_mutex);
	mutex_exit(&aiop->aio_mutex);

	/* cleanup the cleanup-thread queues. */
	aio_cleanup(1);

	/*
	 * free up the done queue's resources.
	 */
	if ((head = aiop->aio_doneq) != NULL) {
		head->aio_req_prev->aio_req_next = NULL;
		for (reqp = head; reqp != NULL; reqp = next) {
			next = reqp->aio_req_next;
			aphysio_unlock(reqp);
			kmem_free(reqp, sizeof (struct aio_req_t));
		}
	}
	/*
	 * release aio request freelist.
	 */
	for (reqp = aiop->aio_free; reqp != NULL; reqp = next) {
		next = reqp->aio_req_next;
		kmem_free(reqp, sizeof (struct aio_req_t));
	}

	/*
	 * release io list head freelist.
	 */
	for (liop = aiop->aio_lio_free; liop != NULL; liop = nxtlio) {
		nxtlio = liop->lio_next;
		kmem_free(liop, sizeof (aio_lio_t));
	}

	mutex_destroy(&aiop->aio_mutex);
	kmem_free(p->p_aio, sizeof (struct aio));
}

/*
 * copy out aio request's result to a user-level result_t buffer.
 */
void
aio_copyout_result(aio_req_t *reqp)
{
	struct buf *bp;
	struct iovec *iov;
	void	*resultp;
	int errno;
	size_t retval;

	if (reqp->aio_req_flags & AIO_COPYOUTDONE)
		return;

	reqp->aio_req_flags |= AIO_COPYOUTDONE;

	iov = reqp->aio_req_uio.uio_iov;
	bp = &reqp->aio_req_buf;
	/* "resultp" points to user-level result_t buffer */
	resultp = (void *)reqp->aio_req_resultp;
	if (bp->b_flags & B_ERROR) {
		if (bp->b_error)
			errno = bp->b_error;
		else
			errno = EIO;
		retval = (size_t)-1;
	} else {
		errno = 0;
		retval = iov->iov_len - bp->b_resid;
	}
#ifdef	_SYSCALL32_IMPL
	if (get_udatamodel() == DATAMODEL_NATIVE) {
		(void) suword32(&((aio_result_t *)resultp)->aio_errno, errno);
		(void) sulword(&((aio_result_t *)resultp)->aio_return, retval);
	} else {
		(void) suword32(&((aio_result32_t *)resultp)->aio_errno, errno);
		(void) suword32(&((aio_result32_t *)resultp)->aio_return,
		    (int)retval);
	}
#else
	(void) suword32(&((aio_result_t *)resultp)->aio_errno, errno);
	(void) suword32(&((aio_result_t *)resultp)->aio_return, retval);
#endif
}
