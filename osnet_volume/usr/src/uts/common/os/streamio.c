/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)streamio.c	1.255	99/12/15 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/mode.h>
#include <sys/proc.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/stream.h>
#include <sys/session.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#include <sys/tihdr.h>
#include <sys/var.h>
#include <sys/poll.h>
#include <sys/termio.h>
#include <sys/ttold.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/cmn_err.h>
#include <sys/sad.h>
#include <sys/priocntl.h>
#include <sys/jioctl.h>
#include <sys/procset.h>
#include <sys/session.h>
#include <sys/kmem.h>
#include <sys/filio.h>
#include <sys/vtrace.h>
#include <sys/debug.h>
#include <sys/strredir.h>
#include <sys/fs/fifonode.h>
#include <sys/fs/snode.h>
#include <sys/strlog.h>

/*
 * Can't include header because we don't want all the
 * other ddi crud
 */
extern cred_t	*ddi_get_cred();
extern minor_t	ddi_getiminor(dev_t);
extern int	drv_priv(cred_t *);

/*
 * id value used to distinguish between different ioctl messages
 */
static uint32_t ioc_id;

extern int kill(pid_t, int);
static void putback(struct stdata *, queue_t *, mblk_t *, int);
static void strcleanall(struct vnode *);
extern void strsignal_nolock(stdata_t *stp, int sig, int32_t band);

/*
 *  Qinit structure and Module_info structures
 *	for stream head read and write queues
 */
int	strrput(), strwsrv();
struct 	module_info strm_info = { 0, "strrhead", 0, INFPSZ, STRHIGH, STRLOW };
struct  module_info stwm_info = { 0, "strwhead", 0, 0, 0, 0 };
struct	qinit strdata = { strrput, NULL, NULL, NULL, NULL, &strm_info, NULL };
struct	qinit stwdata = { NULL, strwsrv, NULL, NULL, NULL, &stwm_info, NULL };
struct	module_info fiform_info = { 0, "fifostrrhead", 0, PIPE_BUF, FIFOHIWAT,
		FIFOLOWAT };
struct  module_info fifowm_info = { 0, "fifostrwhead", 0, 0, 0, 0 };
struct	qinit fifo_strdata = { strrput, NULL, NULL, NULL, NULL, &fiform_info,
		NULL };
struct	qinit fifo_stwdata = { NULL, strwsrv, NULL, NULL, NULL, &fifowm_info,
		NULL };

extern kmutex_t	strresources;
extern kmutex_t muxifier;
extern kmutex_t sad_lock;

static boolean_t msghasdata(mblk_t *bp);
#define	msgnodata(bp) (!msghasdata(bp))


/*
 * Stream head locking notes:
 *	There are four monitors associated with the stream head:
 *	1. v_stream monitor: in stropen() and strclose() v_lock
 *		is held while the association of vnode and stream
 *		head is established or tested for.
 *	2. open/close/push/pop monitor: sd_lock is held while each
 *		thread bids for exclusive access to this monitor
 *		for opening or closing a stream.  In addition, this
 *		monitor is entered during pushes and pops.  This
 *		guarantees that during plumbing operations there
 *		is only one thread trying to change the plumbing.
 *		Any other threads present in the stream are only
 *		using the plumbing.
 *	3. read/write monitor: in the case of read, a thread holds
 *		sd_lock while trying to get data from the stream
 *		head queue.  if there is none to fulfill a read
 *		request, it sets RSLEEP and calls cv_wait_sig() down
 *		in strwaitq() to await the arrival of new data.
 *		when new data arrives in strrput(), sd_lock is acquired
 *		before testing for RSLEEP and calling cv_broadcast().
 *		the behavior of strwrite(), strwsrv(), and WSLEEP
 *		mirror this.
 *	4. ioctl monitor: sd_lock is gotten to ensure that only one
 *		thread is doing an ioctl at a time.
 */

int allow_unpriv_cons_term_open = 0;
extern vnode_t *rconsvp;

/*
 * Open a stream device.
 * sflag should be either 0, CLONEOPEN, or CONSOPEN.
 */
int
stropen(vnode_t *vp, dev_t *devp, int flag, int sflag, cred_t *crp)
{
	struct stdata *stp;
	queue_t *qp;
	int s;
	dev_t dummydev;
	struct autopush *ap;
	int error = 0;
	ssize_t	rmin, rmax;
	int consopen = sflag & CONSOPEN;

	/*
	 * strip CONSOPEN out of sflag; this is a private interface between
	 * spec_open() and stropen().
	 */
	sflag &= ~CONSOPEN;
#ifdef C2_AUDIT
	if (audit_active)
		audit_stropen(vp, devp, flag, crp);
#endif

	/*
	 * If the stream already exists, wait for any open in progress
	 * to complete, then call the open function of each module and
	 * driver in the stream.  Otherwise create the stream.
	 */
	TRACE_1(TR_FAC_STREAMS_FR,
		TR_STROPEN, "stropen:%X", vp);
retry:
	mutex_enter(&vp->v_lock);
	if ((stp = vp->v_stream) != NULL) {

		/*
		 * Waiting for stream to be created to device
		 * due to another open.
		 */

		/*
		 * This closes a security hole; we don't want users to be able
		 * to open the stream representing the console directly.  They
		 * should only be able to open it via /dev/console.  We let
		 * root in, however.
		 *
		 * spec_open() knows the score: it examines the vnode to open
		 * before following the commonvp ptr.  If the vnode is rconsvp
		 * it passes the CONSOPEN flag to stropen in the sflags field.
		 * We can't do this via a normal open flag because then a normal
		 * user could specify this flag.
		 *
		 * This code also knows a lot: this path is taken if a stream
		 * is already wired into the commonvp presented.  This is always
		 * true for rconsvp's common snode.
		 *
		 * The code checks if rconsvp is null as a defensive measure.
		 */

	    if ((rconsvp != NULL) && (stp == rconsvp->v_stream) &&
		!consopen && (drv_priv(crp) == EPERM) &&
		(allow_unpriv_cons_term_open == 0)) {
		    mutex_exit(&vp->v_lock);

		    mutex_enter(&stp->sd_lock); /* just like ckreturn below. */
		    cv_broadcast(&stp->sd_monitor);
		    mutex_exit(&stp->sd_lock);
		    return (EACCES);
	    }
	    mutex_exit(&vp->v_lock);

	    if (STRMATED(stp)) {
		struct stdata *strmatep = stp->sd_mate;

		STRLOCKMATES(stp);
		if (strmatep->sd_flag & (STWOPEN|STRCLOSE|STRPLUMB)) {
			if (flag & (FNDELAY|FNONBLOCK)) {
				error = EAGAIN;
				mutex_exit(&strmatep->sd_lock);
				goto ckreturn;
			}
			mutex_exit(&stp->sd_lock);
			if (!cv_wait_sig(&strmatep->sd_monitor,
			    &strmatep->sd_lock)) {
				error = EINTR;
				mutex_exit(&strmatep->sd_lock);
				mutex_enter(&stp->sd_lock);
				goto ckreturn;
			}
			mutex_exit(&strmatep->sd_lock);
			goto retry;
		}
		if (stp->sd_flag & (STWOPEN|STRCLOSE|STRPLUMB)) {
			if (flag & (FNDELAY|FNONBLOCK)) {
				error = EAGAIN;
				mutex_exit(&strmatep->sd_lock);
				goto ckreturn;
			}
			mutex_exit(&strmatep->sd_lock);
			if (!cv_wait_sig(&stp->sd_monitor, &stp->sd_lock)) {
				error = EINTR;
				goto ckreturn;
			}
			mutex_exit(&stp->sd_lock);
			goto retry;
		}

		if (stp->sd_flag & (STRDERR|STWRERR)) {
			error = EIO;
			mutex_exit(&strmatep->sd_lock);
			goto ckreturn;
		}

		stp->sd_flag |= STWOPEN;
		STRUNLOCKMATES(stp);
	    } else {
		mutex_enter(&stp->sd_lock);
		if (stp->sd_flag & (STWOPEN|STRCLOSE|STRPLUMB)) {
			if (flag & (FNDELAY|FNONBLOCK)) {
				error = EAGAIN;
				goto ckreturn;
			}
			if (!cv_wait_sig(&stp->sd_monitor, &stp->sd_lock)) {
				error = EINTR;
				goto ckreturn;
			}
			mutex_exit(&stp->sd_lock);
			goto retry;  /* could be clone! */
		}

		if (stp->sd_flag & (STRDERR|STWRERR)) {
			error = EIO;
			goto ckreturn;
		}

		stp->sd_flag |= STWOPEN;
		mutex_exit(&stp->sd_lock);
	    }

		/*
		 * Open all modules and devices down stream to notify
		 * that another user is streaming.  For modules, set the
		 * last argument to MODOPEN and do not pass any open flags.
		 * Ignore dummydev since this is not the first open.
		 */
	    claimstr(stp->sd_wrq);
	    qp = stp->sd_wrq;
	    while (_SAMESTR(qp)) {
		qp = qp->q_next;
		if ((error = qreopen(_RD(qp), devp, flag, crp)) != 0)
			break;
	    }
	    releasestr(stp->sd_wrq);
	    mutex_enter(&stp->sd_lock);
	    stp->sd_flag &= ~(STRHUP|STWOPEN|STRDERR|STWRERR);
	    stp->sd_rerror = 0;
	    stp->sd_werror = 0;
ckreturn:
	    cv_broadcast(&stp->sd_monitor);
	    mutex_exit(&stp->sd_lock);
	    return (error);
	}

	/*
	 * This vnode isn't streaming.  SPECFS already
	 * checked for multiple vnodes pointing to the
	 * same stream, so create a stream to the driver.
	 */
	qp = allocq();
	stp = shalloc(qp);

	/*
	 * Initialize stream head.  shalloc() has given us
	 * exclusive access, and we have the vnode locked;
	 * we can do whatever we want with stp.
	 */
	stp->sd_flag = STWOPEN;
	stp->sd_siglist = NULL;
	stp->sd_pollist.ph_list = NULL;
	stp->sd_sigflags = 0;
	stp->sd_mark = NULL;
	stp->sd_closetime = STRTIMOUT;
	stp->sd_sidp = NULL;
	stp->sd_pgidp = NULL;
	stp->sd_vnode = vp;
	stp->sd_rerror = 0;
	stp->sd_werror = 0;
	stp->sd_wroff = 0;
	stp->sd_iocblk = NULL;
	stp->sd_pushcnt = 0;
	stp->sd_qn_minpsz = 0;
	stp->sd_qn_maxpsz = INFPSZ - 1;	/* used to check for intitailization */
	stp->sd_vnfifo = NULL;
	stp->sd_maxblk = INFPSZ;
	qp->q_ptr = _WR(qp)->q_ptr = stp;
	STREAM(qp) = STREAM(_WR(qp)) = stp;
	vp->v_stream = stp;
	mutex_exit(&vp->v_lock);
	if (vp->v_type == VFIFO) {
		stp->sd_flag |= OLDNDELAY;
		/*
		 * This means, both for pipes and fifos
		 * strwrite will send SIGPIPE if the other
		 * end is closed. For putmsg it depends
		 * on whether it is a XPG4_2 application
		 * or not
		 */
		stp->sd_wput_opt = SW_SIGPIPE;

		/* setq might sleep in kmem_alloc - avoid holding locks. */
		setq(qp, &fifo_strdata, &fifo_stwdata, NULL, NULL,
			QMTSAFE, SQ_CI|SQ_CO, 0);

		set_qend(qp);
		stp->sd_strtab = (struct streamtab *)fifo_getinfo();
		_WR(qp)->q_nfsrv = _WR(qp);
		qp->q_nfsrv = qp;
		_WR(qp)->q_nbsrv = qp->q_nbsrv = NULL;
		/*
		 * Wake up others that are waiting for stream to be created.
		 */
		mutex_enter(&stp->sd_lock);
		/*
		 * nothing is be pushed on stream yet, so
		 * optimized stream head packetsizes are just that
		 * of the read queue
		 */
		stp->sd_qn_minpsz = qp->q_minpsz;
		stp->sd_qn_maxpsz = qp->q_maxpsz;
		stp->sd_flag &= ~STWOPEN;
		goto fifo_opendone;
	}
	/* setq might sleep in kmem_alloc - avoid holding locks. */
	setq(qp, &strdata, &stwdata, NULL, NULL, QMTSAFE, SQ_CI|SQ_CO, 0);

	set_qend(qp);

	/*
	 * Open driver and create stream to it (via qattach).
	 */
	if (error = qattach(qp, devp, flag, sflag, CDEVSW,
				getmajor(*devp), crp, B_FALSE)) {
		mutex_enter(&vp->v_lock);
		vp->v_stream = NULL;
		mutex_exit(&vp->v_lock);
		mutex_enter(&stp->sd_lock);
		cv_broadcast(&stp->sd_monitor);
		mutex_exit(&stp->sd_lock);
		freeq(_RD(qp));
		shfree(stp);
		return (error);
	}
	/*
	 * Set sd_strtab after open in order to handle clonable drivers
	 */
	stp->sd_strtab = STREAMSTAB(getmajor(*devp));

	/*
	 * Historical note: dummydev used to be be prior to the initial
	 * open (via qattach above), which made the value seen
	 * inconsistent between an I_PUSH and an autopush of a module.
	 */
	dummydev = *devp;

	/*
	 * check for autopush
	 */
	mutex_enter(&sad_lock);
	ap = strphash(getemajor(*devp));
#define	DEVT(ap)	makedevice(ap->ap_major, ap->ap_minor)
#define	DEVLT(ap)	makedevice(ap->ap_major, ap->ap_lastminor)

	while (ap) {
		if (ap->ap_major == (getemajor(*devp))) {
			if (ap->ap_type == SAP_ALL)
				break;
			else if ((ap->ap_type == SAP_ONE) &&
			    (ddi_getiminor(DEVT(ap)) == getminor(*devp)))
				break;
			else if (ap->ap_type == SAP_RANGE &&
			    getminor(*devp) >= ddi_getiminor(DEVT(ap)) &&
			    getminor(*devp) <= ddi_getiminor(DEVLT(ap)))
				break;
		}
		ap = ap->ap_nextp;
	}
	if (ap == NULL) {
		mutex_exit(&sad_lock);
		goto opendone;
	}
	ap->ap_cnt++;
	mutex_exit(&sad_lock);
	for (s = 0; s < ap->ap_npush; s++) {

		if (stp->sd_flag & (STRHUP|STRDERR|STWRERR)) {
			error = (stp->sd_flag & STRHUP) ? ENXIO : EIO;
			break;
		}
		if (stp->sd_pushcnt >= nstrpush) {
			error = EINVAL;
			break;
		}

		/*
		 * Note: findmodbyindex() holds the module which
		 * is released later during the pop of this module.
		 */
		if (!findmodbyindex(ap->ap_list[s])) {
			stp->sd_flag |= STREOPENFAIL;
			error = EINVAL;
			break;
		}
		/*
		* push new module and call its open routine via qattach
		*/
		if (error = qattach(qp, &dummydev, 0, 0, FMODSW,
		    ap->ap_list[s], crp, B_FALSE)) {
			(void) fmod_release(ap->ap_list[s]);
			break;
		} else {
			mutex_enter(&stp->sd_lock);
			stp->sd_pushcnt++;
			mutex_exit(&stp->sd_lock);
		}

		/*
		* If flow control is on, don't break it - enable
		* first back queue with svc procedure
		*/
		if (_RD(stp->sd_wrq)->q_flag & QWANTW) {
			/* Note: no setqback here - use pri -1. */
			backenable(_RD(stp->sd_wrq->q_next), -1);
		}

		/*
		 * Check to see if caller wants a STREAMS anchor
		 * put at this place in the stream, and add if so.
		 */
		mutex_enter(&stp->sd_lock);
		if (ap->ap_anchor == stp->sd_pushcnt)
			stp->sd_anchor = stp->sd_pushcnt;
		mutex_exit(&stp->sd_lock);

	} /* for */
	mutex_enter(&sad_lock);
	if (--(ap->ap_cnt) <= 0)
		ap_free(ap);
	mutex_exit(&sad_lock);

	/*
	 * let specfs know that open failed part way through
	 */

	if (error) {
		mutex_enter(&stp->sd_lock);
		stp->sd_flag |= STREOPENFAIL;
		mutex_exit(&stp->sd_lock);
	}

opendone:

	/*
	 * Wake up others that are waiting for stream to be created.
	 */
	mutex_enter(&stp->sd_lock);
	stp->sd_flag &= ~STWOPEN;

	/*
	 * As a performance concern we are caching the values of
	 * q_minpsz and q_maxpsz of the module below the stream
	 * head in the stream head.
	 */
	mutex_enter(QLOCK(stp->sd_wrq->q_next));
	rmin = stp->sd_wrq->q_next->q_minpsz;
	rmax = stp->sd_wrq->q_next->q_maxpsz;
	mutex_exit(QLOCK(stp->sd_wrq->q_next));

	/* do this processing here as a performance concern */
	if (strmsgsz != 0) {
		if (rmax == INFPSZ)
			rmax = strmsgsz;
		else
			rmax = MIN(strmsgsz, rmax);
	}

	mutex_enter(QLOCK(stp->sd_wrq));
	stp->sd_qn_minpsz = rmin;
	stp->sd_qn_maxpsz = rmax;
	mutex_exit(QLOCK(stp->sd_wrq));

fifo_opendone:
	cv_broadcast(&stp->sd_monitor);
	mutex_exit(&stp->sd_lock);
	return (error);
}

static int strsink(queue_t *, mblk_t *);
static struct qinit deadrend = {
	strsink, NULL, NULL, NULL, NULL, &strm_info, NULL
};
static struct qinit deadwend = {
	NULL, NULL, NULL, NULL, NULL, &stwm_info, NULL
};

/*
 * Close a stream.
 * This is called from closef() on the last close of an open stream.
 * Strclean() will already have removed the siglist and pollist
 * information, so all that remains is to remove all multiplexor links
 * for the stream, pop all the modules (and the driver), and free the
 * stream structure.
 */

int
strclose(struct vnode *vp, int flag, cred_t *crp)
{
	struct stdata *stp;
	queue_t *qp;
	int rval;
	int freestp = 1;
	int mid;
	queue_t *rmq;
	struct vnode *vnfifo;

#ifdef C2_AUDIT
	if (audit_active)
		audit_strclose(vp, flag, crp);
#endif

	TRACE_1(TR_FAC_STREAMS_FR,
		TR_STRCLOSE, "strclose:%X", vp);
	ASSERT(vp->v_stream);

	stp = vp->v_stream;
	ASSERT(!(stp->sd_flag & STPLEX));
	qp = stp->sd_wrq;

	/*
	 * Needed so that strpoll will return non-zero for this fd.
	 * Note that with POLLNOERR STRHUP does still cause POLLHUP.
	 */
	mutex_enter(&stp->sd_lock);
	stp->sd_flag |= STRHUP;
	mutex_exit(&stp->sd_lock);

	/*
	 * Since we call pollwakeup in close() now, the poll list should
	 * be empty in most cases. The only exception is the layered devices
	 * (e.g. the console drivers with redirection modules pushed on top
	 * of it).
	 */
	if (stp->sd_pollist.ph_list != NULL) {
		pollwakeup(&stp->sd_pollist, POLLERR);
		pollhead_clean(&stp->sd_pollist);
	}
	ASSERT(stp->sd_pollist.ph_list == NULL);
	ASSERT(stp->sd_sidp == NULL);
	ASSERT(stp->sd_pgidp == NULL);

	/*
	 * If the registered process or process group did not have an
	 * open instance of this stream then strclean would not be
	 * called. Thus at the time of closing all remaining siglist entries
	 * are removed.
	 */
	if (stp->sd_siglist != NULL)
		strcleanall(vp);

	ASSERT(stp->sd_siglist == NULL);
	ASSERT(stp->sd_sigflags == 0);

	if (STRMATED(stp)) {
	    struct stdata *strmatep = stp->sd_mate;
	    int waited = 1;

	    STRLOCKMATES(stp);
	    while (waited) {
		waited = 0;
		while (stp->sd_flag & (STWOPEN|STRCLOSE|STRPLUMB)) {
			mutex_exit(&strmatep->sd_lock);
			cv_wait(&stp->sd_monitor, &stp->sd_lock);
			mutex_exit(&stp->sd_lock);
			STRLOCKMATES(stp);
			waited = 1;
		}
		while (strmatep->sd_flag & (STWOPEN|STRCLOSE|STRPLUMB)) {
			mutex_exit(&stp->sd_lock);
			cv_wait(&strmatep->sd_monitor, &strmatep->sd_lock);
			mutex_exit(&strmatep->sd_lock);
			STRLOCKMATES(stp);
			waited = 1;
		}
	    }
	    stp->sd_flag |= STRCLOSE;
	    STRUNLOCKMATES(stp);
	} else {
		mutex_enter(&stp->sd_lock);
		stp->sd_flag |= STRCLOSE;
		mutex_exit(&stp->sd_lock);
	}

	ASSERT(qp->q_first == NULL);	/* No more delayed write */

	/* Check if an I_LINK was ever done on this stream */
	if (stp->sd_flag & STRHASLINKS) {
		(void) munlinkall(stp, LINKCLOSE|LINKNORMAL, crp, &rval);
	}

	while (_SAMESTR(qp)) {
		/*
		 * Holding sd_lock prevents q_next from changing in
		 * this stream.
		 */
		mutex_enter(&stp->sd_lock);
		if (!(flag & (FNDELAY|FNONBLOCK)) && (stp->sd_closetime > 0)) {

			/*
			 * sleep until awakened by strwsrv() or timeout
			 */
			while (qp->q_next->q_mblkcnt) {
				stp->sd_flag |= WSLEEP;

				/* ensure strwsrv gets enabled */
				mutex_enter(QLOCK(qp->q_next));
				qp->q_next->q_flag |= QWANTW;
				mutex_exit(QLOCK(qp->q_next));
				/* get out if we timed out or recv'd a signal */
				if (str_cv_wait(&qp->q_wait, &stp->sd_lock,
				    stp->sd_closetime, 0) <= 0) {
					break;
				}
			}
			stp->sd_flag &= ~WSLEEP;
			mutex_exit(&stp->sd_lock);
		}
		else
			mutex_exit(&stp->sd_lock);
		rmq = qp->q_next;
		if (!(rmq->q_flag & QISDRV)) {
			mutex_enter(&fmodsw_lock);
			mid = findmodbyname(rmq->q_qinfo->qi_minfo->mi_idname);
			mutex_exit(&fmodsw_lock);
			qdetach(_RD(rmq), 1, flag, crp, B_FALSE);
			if (mid != -1) {
				(void) fmod_release(mid);
			}
		} else {
			ASSERT(!_SAMESTR(rmq));
			qdetach(_RD(rmq), 1, flag, crp, B_FALSE);
		}
	}

	/* Prevent qenable from re-enabling the stream head queue */
	disable_svc(_RD(qp));

	/*
	 * Remove queues from runlist, if they are enabled.
	 */
	remove_runlist(_RD(qp));

	/*
	 * Wait until service procedure of each queue is
	 * run, if QINSERVICE is set.
	 */
	wait_svc(_RD(qp));

	/*
	 * Now, flush both queues.
	 * Note that flushq handles M_PASSFP by using freemsg_flush instead
	 * of freemsg.
	 */
	flushq(_RD(qp), FLUSHALL);
	flushq(qp, FLUSHALL);

	/*
	 * If the write queue of the stream head is pointing to a
	 * read queue, we have a twisted stream.  If the read queue
	 * is alive, convert the stream head queues into a dead end.
	 * If the read queue is dead, free the dead pair.
	 */
	if (qp->q_next && !_SAMESTR(qp)) {
		if (qp->q_next->q_qinfo == &deadrend) {	/* half-closed pipe */
			flushq(qp->q_next, FLUSHALL); /* ensure no message */
			shfree(qp->q_next->q_stream);
			freeq(qp->q_next);
			freeq(_RD(qp));
		} else if (qp->q_next == _RD(qp)) {	/* fifo */
			freeq(_RD(qp));
		} else {				/* pipe */
			freestp = 0;
			/*
			 * The q_info pointers are never accessed when
			 * SQLOCK is held.
			 */
			ASSERT(qp->q_syncq == _RD(qp)->q_syncq);
			mutex_enter(SQLOCK(qp->q_syncq));
			qp->q_qinfo = &deadwend;
			_RD(qp)->q_qinfo = &deadrend;
			mutex_exit(SQLOCK(qp->q_syncq));
		}
	} else {
		freeq(_RD(qp)); /* free stream head queue pair */
	}

	mutex_enter(&vp->v_lock);
	if (stp->sd_iocblk) {
		if (stp->sd_iocblk != (mblk_t *)-1) {
			freemsg(stp->sd_iocblk);
		}
		stp->sd_iocblk = NULL;
	}
	stp->sd_vnode = NULL;
	vp->v_stream = NULL;
	mutex_exit(&vp->v_lock);
	mutex_enter(&stp->sd_lock);
	vnfifo = stp->sd_vnfifo;
	stp->sd_vnfifo = NULL;
	stp->sd_flag &= ~STRCLOSE;
	cv_broadcast(&stp->sd_monitor);
	if (stp->sd_kcp) {
		/* deallocate the kernel map cache for zero-copy */
		kmap_cache_free(stp->sd_kcp);
		stp->sd_kcp = NULL;
	}
	mutex_exit(&stp->sd_lock);
	/*
	 * Release held FIFO vnode
	 * Note: VN_RELE acquires v_lock.
	 */
	if (vnfifo) {
		VN_RELE(vnfifo);
	}

	if (freestp)
		shfree(stp);
	return (0);
}

static int
strsink(queue_t *q, mblk_t *bp)
{
	struct copyresp *resp;

	switch (bp->b_datap->db_type) {
	case M_FLUSH:
		if ((*bp->b_rptr & FLUSHW) && !(bp->b_flag & MSGNOLOOP)) {
			*bp->b_rptr &= ~FLUSHR;
			bp->b_flag |= MSGNOLOOP;
			/*
			 * Protect against the driver passing up
			 * messages after it has done a qprocsoff.
			 */
			if (_OTHERQ(q)->q_next == NULL)
				freemsg(bp);
			else
				qreply(q, bp);
		} else {
			freemsg(bp);
		}
		break;

	case M_COPYIN:
	case M_COPYOUT:
		if (bp->b_cont) {
			freemsg(bp->b_cont);
			bp->b_cont = NULL;
		}
		bp->b_datap->db_type = M_IOCDATA;
		bp->b_wptr = bp->b_rptr + sizeof (struct copyresp);
		resp = (struct copyresp *)bp->b_rptr;
		resp->cp_rval = (caddr_t)1;	/* failure */
		/*
		 * Protect against the driver passing up
		 * messages after it has done a qprocsoff.
		 */
		if (_OTHERQ(q)->q_next == NULL)
			freemsg(bp);
		else
			qreply(q, bp);
		break;

	case M_IOCTL:
		if (bp->b_cont) {
			freemsg(bp->b_cont);
			bp->b_cont = NULL;
		}
		bp->b_datap->db_type = M_IOCNAK;
		/*
		 * Protect against the driver passing up
		 * messages after it has done a qprocsoff.
		 */
		if (_OTHERQ(q)->q_next == NULL)
			freemsg(bp);
		else
			qreply(q, bp);
		break;

	default:
		freemsg(bp);
		break;
	}

	return (0);
}

/*
 * Clean up after a process when it closes a stream.  This is called
 * from closef for all closes, whereas strclose is called only for the
 * last close on a stream.  The siglist is scanned for entries for the
 * current process, and these are removed.
 */
void
strclean(struct vnode *vp)
{
	strsig_t *ssp, *pssp, *tssp;
	stdata_t *stp;
	int update = 0;

	TRACE_1(TR_FAC_STREAMS_FR,
		TR_STRCLEAN, "strclean:%X", vp);
	stp = vp->v_stream;
	pssp = NULL;
	mutex_enter(&stp->sd_lock);
	ssp = stp->sd_siglist;
	while (ssp) {
		if (ssp->ss_pidp == curproc->p_pidp) {
			tssp = ssp->ss_next;
			if (pssp)
				pssp->ss_next = tssp;
			else
				stp->sd_siglist = tssp;
			mutex_enter(&pidlock);
			PID_RELE(ssp->ss_pidp);
			mutex_exit(&pidlock);
			kmem_cache_free(strsig_cache, ssp);
			update = 1;
			ssp = tssp;
		} else {
			pssp = ssp;
			ssp = ssp->ss_next;
		}
	}
	if (update) {
		stp->sd_sigflags = 0;
		for (ssp = stp->sd_siglist; ssp; ssp = ssp->ss_next)
			stp->sd_sigflags |= ssp->ss_events;
	}
	mutex_exit(&stp->sd_lock);
}

/*
 * Used on the last close to remove any remaining items on the siglist.
 * These could be present on the siglist due to I_ESETSIG calls that
 * use process groups or processed that do not have an open file descriptor
 * for this stream (Such entries would not be removed by strclean).
 */
static void
strcleanall(struct vnode *vp)
{
	strsig_t *ssp, *nssp;
	stdata_t *stp;

	stp = vp->v_stream;
	mutex_enter(&stp->sd_lock);
	ssp = stp->sd_siglist;
	stp->sd_siglist = NULL;
	while (ssp) {
		nssp = ssp->ss_next;
		mutex_enter(&pidlock);
		PID_RELE(ssp->ss_pidp);
		mutex_exit(&pidlock);
		kmem_cache_free(strsig_cache, ssp);
		ssp = nssp;
	}
	stp->sd_sigflags = 0;
	mutex_exit(&stp->sd_lock);
}

/*
 * Retrieve the next message from the logical stream head read queue
 * using either rwnext (if sync stream) or getq_noenab.
 * It is the callers responsibility to call qbackenable after
 * it is finished with the message. The caller should not call
 * qbackenable until after any putback calls to avoid spurious backenabling.
 */
mblk_t *
strget(struct stdata *stp, queue_t *q, struct uio *uiop, int first,
    int *errorp)
{
	mblk_t *bp;
	int error;

	ASSERT(MUTEX_HELD(&stp->sd_lock));
	/* Holding sd_lock prevents the read queue from changing  */

	if (uiop != NULL && stp->sd_struiordq != NULL &&
	    q->q_first == NULL &&
	    (!first || (stp->sd_wakeq & RSLEEP))) {
		/*
		 * Stream supports rwnext() for the read side.
		 * If this is the first time we're called by e.g. strread
		 * only do the downcall if there is a deferred wakeup
		 * (registered in sd_wakeq).
		 */
		struiod_t uiod;

		if (first)
			stp->sd_wakeq &= ~RSLEEP;

		(void) uiodup(uiop, &uiod.d_uio, uiod.d_iov,
			sizeof (uiod.d_iov) / sizeof (*uiod.d_iov));
		uiod.d_mp = 0;
		/*
		 * Mark that a thread is in rwnext on the read side
		 * to prevent strrput from nacking ioctls immediately.
		 * When the last concurrent rwnext returns
		 * the ioctls are nack'ed.
		 */
		ASSERT(MUTEX_HELD(&stp->sd_lock));
		stp->sd_struiodnak++;
		/*
		 * Note: rwnext will drop sd_lock.
		 */
		error = rwnext(q, &uiod);
		ASSERT(MUTEX_NOT_HELD(&stp->sd_lock));
		mutex_enter(&stp->sd_lock);
		stp->sd_struiodnak--;
		while (stp->sd_struiodnak == 0 &&
		    ((bp = stp->sd_struionak) != NULL)) {
			stp->sd_struionak = bp->b_next;
			bp->b_next = NULL;
			bp->b_datap->db_type = M_IOCNAK;
			/*
			 * Protect against the driver passing up
			 * messages after it has done a qprocsoff.
			 */
			if (_OTHERQ(q)->q_next == NULL)
				freemsg(bp);
			else {
				mutex_exit(&stp->sd_lock);
				qreply(q, bp);
				mutex_enter(&stp->sd_lock);
			}
		}
		ASSERT(MUTEX_HELD(&stp->sd_lock));
		if (error == 0 || error == EWOULDBLOCK) {
			if ((bp = uiod.d_mp) != NULL) {
				*errorp = 0;
				ASSERT(MUTEX_HELD(&stp->sd_lock));
				return (bp);
			}
			error = 0;
		} else if (error == EINVAL) {
			/*
			 * The stream plumbing must have
			 * changed while we were away, so
			 * just turn off rwnext()s.
			 */
			error = 0;
		} else if (error == EBUSY) {
			/*
			 * The module might have data in transit using putnext
			 * Fall back on waiting + getq.
			 */
			error = 0;
		} else {
			*errorp = error;
			ASSERT(MUTEX_HELD(&stp->sd_lock));
			return (NULL);
		}
		/*
		 * Try a getq in case a rwnext() generated mblk
		 * has bubbled up via strrput().
		 */
	}
	*errorp = 0;
	ASSERT(MUTEX_HELD(&stp->sd_lock));
	return (getq_noenab(q));
}

/*
 * Copyout the message to the uio including handling messages that
 * have already been copied out by sync streams.
 * If the message does not fit in the uio the remainder of it is returned;
 * if the whole message is consumed NULL is returned.
 */
static mblk_t *
struiocopyout(struct stdata *stp, mblk_t *bp, struct uio *uiop, int *errorp)
{
	int error = 0;
	dblk_t *dp;
	ptrdiff_t n;
#ifdef ZC_TEST
	hrtime_t start;
#endif

	ASSERT(bp->b_wptr >= bp->b_rptr);

	do {	/* while (bp != NULL && uiop->uio_resid > 0); */

		dp = bp->b_datap;

		if ((dp->db_struioflag & STRUIO_SPEC) &&
		    stp->sd_struiordq != NULL &&
		    bp->b_rptr <= dp->db_struiobase &&
		    bp->b_wptr >= dp->db_struiolim) {
			/*
			 * This is an mblk that may have had
			 * part of it uiomove()ed already, so
			 * we have to handle up to three cases:
			 *
			 * 1) data prefixed to the uio data
			 *	rptr through (uiobase - 1)
			 * 2) uio data already uiomove()ed
			 *	uiobase through (uioptr - 1)
			 * 3) uio data not uiomove()ed and
			 *	data suffixed to the uio data
			 *	uioptr through (wptr - 1)
			 *
			 * That is, this mblk may be processed
			 * up to three times, one for each case.
			 */
			/* LINTED - statement has no conseq */
			if ((n = dp->db_struiobase - bp->b_rptr) > 0) {
				/*
				 * Prefixed data.
				 */
				;
			} else if ((n = dp->db_struioptr -
			    dp->db_struiobase) > 0) {
				/*
				 * Uio data already uiomove()ed.
				 */
				ASSERT(n <= uiop->uio_resid);
				ASSERT(dp->db_struiobase == bp->b_rptr);
				dp->db_struiobase += n;
				uioskip(uiop, n);
				goto skip;
			} else if ((n = bp->b_wptr - dp->db_struioptr) > 0) {
				/*
				 * Uio data not uiomove()ed and/or
				 * suffixed data.
				 */
				ASSERT(dp->db_struioptr == bp->b_rptr);
				dp->db_struiobase += n;
				dp->db_struioptr += n;
			} else if ((n = bp->b_wptr - bp->b_rptr) > 0) {
				/*
				 * If n is +, one of the above
				 * branches should have been
				 * taken.  If it is -, this
				 * is a bad thing!
				 */
				cmn_err(CE_PANIC,
				    "struiocopyout: STRUIO %p %lx",
				    (void *)bp, uiop->uio_resid);
			} else {
				/*
				 * Zero length mblk, skip it.
				 */
				goto skip;
			}
		} else {
			n = bp->b_wptr - bp->b_rptr;
		}
		if ((n = MIN(uiop->uio_resid, n)) != 0) {
			ASSERT(n > 0);

#ifdef ZC_TEST
			if ((zcperf & 1) && (n & PAGEOFFSET) == 0) {
				start = gethrtime();
			} else start = 0ll;
#endif
			/*
			 * Should we attempt zero-copy (page-flipping)?
			 */
			if ((n >= PAGESIZE) &&
			    (stp->sd_flag & STREMAPOK) &&
			    (dp->db_struioflag & STRUIO_ZC) &&
			    (dp->db_ref == 1)) {
				error = uiopageflip((char *)bp->b_rptr,
						n, uiop);
				if (error == ENOTSUP) {
					/*
					 * For ENOTSUP, uiopageflip()
					 * will have copied all the bytes
					 * already. So no need to call
					 * uiomove().
					 */
					mutex_enter(&stp->sd_lock);
					/* Disable zercopy */
					stp->sd_flag &= ~STREMAPOK;
					mutex_exit(&stp->sd_lock);
					error = 0;
				}
			} else {
				error = uiomove((char *)bp->b_rptr,
					n, UIO_READ, uiop);
			}
#ifdef ZC_TEST
			if (start) {
				zckstat->zc_count.value.ul += n >> PAGESHIFT;
				zckstat->zc_hrtime.value.ull +=
					gethrtime() - start;
			}
#endif
		}

		if (error) {
			freemsg(bp);
			*errorp = error;
			return (NULL);
		}
	skip:;
		bp->b_rptr += n;
		while (bp && (bp->b_rptr >= bp->b_wptr)) {
			mblk_t *nbp;

			nbp = bp;
			bp = bp->b_cont;
			freeb(nbp);
		}
	} while (bp != NULL && uiop->uio_resid > 0);

	*errorp = 0;
	return (bp);
}

/*
 * Read a stream according to the mode flags in sd_flag:
 *
 * (default mode)		- Byte stream, msg boundaries are ignored
 * RD_MSGDIS (msg discard)	- Read on msg boundaries and throw away
 *				any data remaining in msg
 * RD_MSGNODIS (msg non-discard) - Read on msg boundaries and put back
 *				any remaining data on head of read queue
 *
 * Consume readable messages on the front of the queue until
 * ttolwp(curthread)->lwp_count
 * is satisfied, the readable messages are exhausted, or a message
 * boundary is reached in a message mode.  If no data was read and
 * the stream was not opened with the NDELAY flag, block until data arrives.
 * Otherwise return the data read and update the count.
 *
 * In default mode a 0 length message signifies end-of-file and terminates
 * a read in progress.  The 0 length message is removed from the queue
 * only if it is the only message read (no data is read).
 *
 * An attempt to read an M_PROTO or M_PCPROTO message results in an
 * EBADMSG error return, unless either RD_PROTDAT or RD_PROTDIS are set.
 * If RD_PROTDAT is set, M_PROTO and M_PCPROTO messages are read as data.
 * If RD_PROTDIS is set, the M_PROTO and M_PCPROTO parts of the message
 * are unlinked from and M_DATA blocks in the message, the protos are
 * thrown away, and the data is read.
 */
/* ARGSUSED */
int
strread(struct vnode *vp, struct uio *uiop, cred_t *crp)
{
	struct stdata *stp;
	mblk_t *bp, *nbp;
	queue_t *q;
	int error = 0;
	uint_t old_sd_flag;
	int first;
	char rflg;
	uint_t mark;		/* Contains MSG*MARK and _LASTMARK */
#define	_LASTMARK	0x8000	/* Distinct from MSG*MARK */
	short delim;
	unsigned char pri = 0;
	char waitflag;
	unsigned char type;

	TRACE_1(TR_FAC_STREAMS_FR,
		TR_STRREAD_ENTER, "strread:%X", vp);
	ASSERT(vp->v_stream);
	stp = vp->v_stream;

	if (stp->sd_sidp != NULL && stp->sd_vnode->v_type != VFIFO)
		if (error = straccess(stp, JCREAD))
			return (error);

	mutex_enter(&stp->sd_lock);
	if (stp->sd_flag & (STRDERR|STPLEX)) {
		error = strgeterr(stp, STRDERR|STPLEX, 0);
		if (error != 0) {
			mutex_exit(&stp->sd_lock);
			return (error);
		}
	}

	/*
	 * Loop terminates when uiop->uio_resid == 0.
	 */
	rflg = 0;
	waitflag = READWAIT;
	q = _RD(stp->sd_wrq);
	for (;;) {
		ASSERT(MUTEX_HELD(&stp->sd_lock));
		old_sd_flag = stp->sd_flag;
		mark = 0;
		delim = 0;
		first = 1;
		while ((bp = strget(stp, q, uiop, first, &error)) == NULL) {
			int done = 0;

			ASSERT(MUTEX_HELD(&stp->sd_lock));

			if (error != 0)
				goto oops;

			if (stp->sd_flag & (STRHUP|STREOF)) {
				goto oops;
			}
			if (rflg && !(stp->sd_flag & STRDELIM)) {
				goto oops;
			}
			/*
			 * If a read(fd,buf,0) has been done, there is no
			 * need to sleep. We always have zero bytes to
			 * return.
			 */
			if (uiop->uio_resid == 0) {
				goto oops;
			}

			qbackenable(q, 0);

			TRACE_3(TR_FAC_STREAMS_FR,
				TR_STRREAD_WAIT,
				"strread calls strwaitq:%X, %X, %X",
						vp, uiop, crp);
			if ((error = strwaitq(stp, waitflag, uiop->uio_resid,
			    uiop->uio_fmode, -1, &done)) != 0 || done) {
				TRACE_3(TR_FAC_STREAMS_FR,
					TR_STRREAD_DONE,
					"strread error or done:%X, %X, %X",
					vp, uiop, crp);
				if ((uiop->uio_fmode & FNDELAY) &&
				    (stp->sd_flag & OLDNDELAY) &&
				    (error == EAGAIN))
					error = 0;
				goto oops;
			}
			TRACE_3(TR_FAC_STREAMS_FR, TR_STRREAD_AWAKE,
				"strread awakes:%X, %X, %X", vp, uiop, crp);
			if (stp->sd_sidp != NULL &&
			    stp->sd_vnode->v_type != VFIFO) {
				mutex_exit(&stp->sd_lock);
				if (error = straccess(stp, JCREAD))
					goto oops1;
				mutex_enter(&stp->sd_lock);
			}
			first = 0;
		}
		ASSERT(MUTEX_HELD(&stp->sd_lock));
		ASSERT(bp);
		pri = bp->b_band;
		/*
		 * Extract any mark information. If the message is not
		 * completely consumed this information will be put in the mblk
		 * that is putback.
		 * If MSGMARKNEXT is set and the message is completely consumed
		 * the STRATMARK flag will be set below. Likewise, if
		 * MSGNOTMARKNEXT is set and the message is
		 * completely consumed STRNOTATMARK will be set.
		 *
		 * For some unknown reason strread only breaks the read at the
		 * last mark.
		 */
		mark = bp->b_flag & (MSGMARK | MSGMARKNEXT | MSGNOTMARKNEXT);
		ASSERT((mark & (MSGMARKNEXT|MSGNOTMARKNEXT)) !=
			(MSGMARKNEXT|MSGNOTMARKNEXT));
		if (mark != 0 && bp == stp->sd_mark) {
			if (rflg) {
				putback(stp, q, bp, pri);
				goto oops;
			}
			mark |= _LASTMARK;
			stp->sd_mark = NULL;
		}
		if ((stp->sd_flag & STRDELIM) && (bp->b_flag & MSGDELIM))
			delim = 1;
		mutex_exit(&stp->sd_lock);

		if (qready())
			queuerun();

		type = bp->b_datap->db_type;

		switch (type) {

		case M_DATA:
ismdata:
			if (msgnodata(bp)) {
				if (mark || delim) {
					freemsg(bp);
				} else if (rflg) {

					/*
					 * If already read data put zero
					 * length message back on queue else
					 * free msg and return 0.
					 */
					bp->b_band = pri;
					mutex_enter(&stp->sd_lock);
					putback(stp, q, bp, pri);
					mutex_exit(&stp->sd_lock);
				} else {
					freemsg(bp);
				}
				error =  0;
				goto oops1;
			}

			rflg = 1;
			waitflag |= NOINTR;
			bp = struiocopyout(stp, bp, uiop, &error);
			if (error) {
				freemsg(bp);
				goto oops1;
			}

			mutex_enter(&stp->sd_lock);
			if (bp) {
				/*
				 * Have remaining data in message.
				 * Free msg if in discard mode.
				 */
				if (stp->sd_read_opt & RD_MSGDIS) {
					freemsg(bp);
				} else {
					bp->b_band = pri;
					if ((mark & _LASTMARK) &&
					    (stp->sd_mark == NULL))
						stp->sd_mark = bp;
					bp->b_flag |= mark & ~_LASTMARK;
					if (delim)
						bp->b_flag |= MSGDELIM;
					nbp = bp;
					do {
						nbp->b_datap->db_struioflag = 0;
					} while ((nbp = nbp->b_cont) != NULL);
					if (msgnodata(bp))
						freemsg(bp);
					else
						putback(stp, q, bp, pri);
				}
			} else {
				/*
				 * Consumed the complete message.
				 * Move the MSG*MARKNEXT information
				 * to the stream head just in case
				 * the read queue becomes empty.
				 *
				 * If the stream head was at the mark
				 * (STRATMARK) before we dropped sd_lock above
				 * and some data was consumed then we have
				 * moved past the mark thus STRATMARK is
				 * cleared. However, if a message arrived in
				 * strrput during the copyout above causing
				 * STRATMARK to be set we can not clear that
				 * flag.
				 */
				if (mark &
				    (MSGMARKNEXT|MSGNOTMARKNEXT|MSGMARK)) {
					if (mark & MSGMARKNEXT) {
						stp->sd_flag &= ~STRNOTATMARK;
						stp->sd_flag |= STRATMARK;
					} else if (mark & MSGNOTMARKNEXT) {
						stp->sd_flag &= ~STRATMARK;
						stp->sd_flag |= STRNOTATMARK;
					} else {
						stp->sd_flag &=
						    ~(STRATMARK|STRNOTATMARK);
					}
				} else if (rflg && (old_sd_flag & STRATMARK)) {
					stp->sd_flag &= ~STRATMARK;
				}
			}

			/*
			 * Check for signal messages at the front of the read
			 * queue and generate the signal(s) if appropriate.
			 * The only signal that can be on queue is M_SIG at
			 * this point.
			 */
			while ((((bp = q->q_first)) != NULL) &&
				(bp->b_datap->db_type == M_SIG)) {
				bp = getq_noenab(q);
				/*
				 * sd_lock is held so the content of the
				 * read queue can not change.
				 */
				ASSERT(bp != NULL &&
					bp->b_datap->db_type == M_SIG);
				mutex_exit(&stp->sd_lock);
				strsignal(stp, *bp->b_rptr,
					(int32_t)bp->b_band);
				freemsg(bp);
				if (qready()) {
					queuerun();
				}
				mutex_enter(&stp->sd_lock);
			}

			if ((uiop->uio_resid == 0) || (mark & _LASTMARK) ||
			    delim ||
			    (stp->sd_read_opt & (RD_MSGDIS|RD_MSGNODIS))) {
				goto oops;
			}
			continue;

		case M_SIG:
			strsignal(stp, *bp->b_rptr, (int32_t)bp->b_band);
			freemsg(bp);
			mutex_enter(&stp->sd_lock);
			continue;

		case M_PROTO:
		case M_PCPROTO:
			/*
			 * Only data messages are readable.
			 * Any others generate an error, unless
			 * RD_PROTDIS or RD_PROTDAT is set.
			 */
			if (stp->sd_read_opt & RD_PROTDAT) {
				for (nbp = bp; nbp; nbp = nbp->b_next) {
				    if ((nbp->b_datap->db_type == M_PROTO) ||
					(nbp->b_datap->db_type == M_PCPROTO))
					nbp->b_datap->db_type = M_DATA;
				    else
					break;
				}
				/*
				 * clear stream head hi pri flag based on
				 * first message
				 */
				if (type == M_PCPROTO) {
					mutex_enter(&stp->sd_lock);
					stp->sd_flag &= ~STRPRI;
					mutex_exit(&stp->sd_lock);
				}
				goto ismdata;
			} else if (stp->sd_read_opt & RD_PROTDIS) {
				/*
				 * discard non-data messages
				 */
				while (bp &&
				    ((bp->b_datap->db_type == M_PROTO) ||
				    (bp->b_datap->db_type == M_PCPROTO))) {
					nbp = unlinkb(bp);
					freeb(bp);
					bp = nbp;
				}
				/*
				 * clear stream head hi pri flag based on
				 * first message
				 */
				if (type == M_PCPROTO) {
					mutex_enter(&stp->sd_lock);
					stp->sd_flag &= ~STRPRI;
					mutex_exit(&stp->sd_lock);
				}
				if (bp) {
					bp->b_band = pri;
					goto ismdata;
				} else {
					break;
				}
			}
			/* FALLTHRU */
		case M_PASSFP:
			if ((bp->b_datap->db_type == M_PASSFP) &&
			    (stp->sd_read_opt & RD_PROTDIS)) {
				file_t *fp =
				    ((struct k_strrecvfd *)bp->b_rptr)->fp;

				(void) closef(fp);
				freemsg(bp);
				break;
			}
			mutex_enter(&stp->sd_lock);
			putback(stp, q, bp, pri);
			mutex_exit(&stp->sd_lock);
			if (rflg == 0)
				error = EBADMSG;
			goto oops1;

		default:
			/*
			 * Garbage on stream head read queue.
			 */
			cmn_err(CE_WARN, "bad %x found at stream head\n",
				bp->b_datap->db_type);
			freemsg(bp);
			goto oops1;
		}
		mutex_enter(&stp->sd_lock);
	}
oops:
	mutex_exit(&stp->sd_lock);
oops1:
	qbackenable(q, pri);
	return (error);
#undef	_LASTMARK
}

/*
 * Default processing of M_PROTO/M_PCPROTO messages.
 * Determine which wakeups and signals are needed.
 * This can be replaced by a user-specified procedure for kernel users
 * of STREAMS.
 */
/* ARGSUSED */
mblk_t *
strrput_proto(vnode_t *vp, mblk_t *mp,
    strwakeup_t *wakeups, strsigset_t *firstmsgsigs,
    strsigset_t *allmsgsigs, strpollset_t *pollwakeups)
{
	*wakeups = RSLEEP;
	*allmsgsigs = 0;

	switch (mp->b_datap->db_type) {
	case M_PROTO:
		if (mp->b_band == 0) {
			*firstmsgsigs = S_INPUT | S_RDNORM;
			*pollwakeups = POLLIN | POLLRDNORM;
		} else {
			*firstmsgsigs = S_INPUT | S_RDBAND;
			*pollwakeups = POLLIN | POLLRDBAND;
		}
		break;
	case M_PCPROTO:
		*firstmsgsigs = S_HIPRI;
		*pollwakeups = POLLPRI;
		break;
	}
	return (mp);
}

/*
 * Default processing of everything but M_DATA, M_PROTO, M_PCPROTO and
 * M_PASSFP messages.
 * Determine which wakeups and signals are needed.
 * This can be replaced by a user-specified procedure for kernel users
 * of STREAMS.
 */
/* ARGSUSED */
mblk_t *
strrput_misc(vnode_t *vp, mblk_t *mp,
    strwakeup_t *wakeups, strsigset_t *firstmsgsigs,
    strsigset_t *allmsgsigs, strpollset_t *pollwakeups)
{
	*wakeups = 0;
	*firstmsgsigs = 0;
	*allmsgsigs = 0;
	*pollwakeups = 0;
	return (mp);
}

/*
 * Stream read put procedure.  Called from downstream driver/module
 * with messages for the stream head.  Data, protocol, and in-stream
 * signal messages are placed on the queue, others are handled directly.
 */
int
strrput(queue_t *q, mblk_t *bp)
{
	struct stdata	*stp;
	ulong_t		rput_opt;
	strwakeup_t	wakeups;
	strsigset_t	firstmsgsigs;	/* Signals if first message on queue */
	strsigset_t	allmsgsigs;	/* Signals for all messages */
	strsigset_t	signals;	/* Signals events to generate */
	strpollset_t	pollwakeups;
	mblk_t		*nextbp;
	uchar_t		band = 0;
	int		hipri_sig;

	stp = (struct stdata *)q->q_ptr;
	/*
	 * Use rput_opt for optimized access to the SR_ flags except
	 * SR_POLLIN. That flag has to be checked under sd_lock since it
	 * is modified by strpoll().
	 */
	rput_opt = stp->sd_rput_opt;

	ASSERT(qclaimed(q));
	TRACE_4(TR_FAC_STREAMS_FR,
		TR_STRRPUT_ENTER,
		"strrput called with message type:q %X bp %X type %X flag %X",
		q, bp, bp->b_datap->db_type, bp->b_flag);

	/*
	 * Perform initial processing and pass to the parameterized functions.
	 */
	ASSERT(bp->b_next == NULL);

	switch (bp->b_datap->db_type) {
	case M_DATA:
		if ((rput_opt & SR_IGN_ZEROLEN) &&
		    bp->b_rptr == bp->b_wptr && msgnodata(bp)) {
			/*
			 * Ignore zero-length M_DATA messages. These might be
			 * generated by some transports.
			 * The zero-length M_DATA messages, even if they
			 * are ignored, should effect the atmark tracking and
			 * should wake up a thread sleeping in strwaitmark.
			 */
			mutex_enter(&stp->sd_lock);
			if (bp->b_flag & MSGMARKNEXT) {
				/*
				 * Record the position of the mark either
				 * in q_last or in STRATMARK.
				 */
				if (q->q_last != NULL) {
					q->q_last->b_flag &= ~MSGNOTMARKNEXT;
					q->q_last->b_flag |= MSGMARKNEXT;
				} else {
					stp->sd_flag &= ~STRNOTATMARK;
					stp->sd_flag |= STRATMARK;
				}
			} else if (bp->b_flag & MSGNOTMARKNEXT) {
				/*
				 * Record that this is not the position of
				 * the mark either in q_last or in
				 * STRNOTATMARK.
				 */
				if (q->q_last != NULL) {
					q->q_last->b_flag &= ~MSGMARKNEXT;
					q->q_last->b_flag |= MSGNOTMARKNEXT;
				} else {
					stp->sd_flag &= ~STRATMARK;
					stp->sd_flag |= STRNOTATMARK;
				}
			}
			if (stp->sd_flag & RSLEEP) {
				stp->sd_flag &= ~RSLEEP;
				cv_broadcast(&q->q_wait);
			}
			mutex_exit(&stp->sd_lock);
			freemsg(bp);
			return (0);
		}
		wakeups = RSLEEP;
		if (bp->b_band == 0) {
			firstmsgsigs = S_INPUT | S_RDNORM;
			pollwakeups = POLLIN | POLLRDNORM;
		} else {
			firstmsgsigs = S_INPUT | S_RDBAND;
			pollwakeups = POLLIN | POLLRDBAND;
		}
		if (rput_opt & SR_SIGALLDATA)
			allmsgsigs = firstmsgsigs;
		else
			allmsgsigs = 0;

		mutex_enter(&stp->sd_lock);
		if ((rput_opt & SR_CONSOL_DATA) &&
		    (bp->b_flag & (MSGMARK|MSGDELIM)) == 0) {
			/*
			 * Consolidate on M_DATA message onto an M_DATA,
			 * M_PROTO, or M_PCPROTO by merging it with q_last.
			 * The consolidation does not take place if
			 * the old message is marked with either of the
			 * marks or the delim flag or if the new
			 * message is marked with MSGMARK. The MSGMARK
			 * check is needed to handle the odd semantics of
			 * MSGMARK where essentially the whole message
			 * is to be treated as marked.
			 * Carry any MSGMARKNEXT  and MSGNOTMARKNEXT from the
			 * new message to the front of the b_cont chain.
			 */
			mblk_t *lbp;

			lbp = q->q_last;
			if (lbp != NULL &&
			    (lbp->b_datap->db_type == M_DATA ||
			    lbp->b_datap->db_type == M_PROTO ||
			    lbp->b_datap->db_type == M_PCPROTO) &&
			    !(lbp->b_flag & (MSGDELIM|MSGMARK|
			    MSGMARKNEXT))) {
				rmvq_noenab(q, lbp);
				/*
				 * The first message in the b_cont list
				 * tracks MSGMARKNEXT and MSGNOTMARKNEXT.
				 * We need to handle the case where we
				 * are appending
				 *
				 * 1) a MSGMARKNEXT to a MSGNOTMARKNEXT.
				 * 2) a MSGMARKNEXT to a plain message.
				 * 3) a MSGNOTMARKNEXT to a plain message
				 * 4) a MSGNOTMARKNEXT to a MSGNOTMARKNEXT
				 *    message.
				 *
				 * Thus we never append a MSGMARKNEXT or
				 * MSGNOTMARKNEXT to a MSGMARKNEXT message.
				 */
				if (bp->b_flag & MSGMARKNEXT) {
					lbp->b_flag |= MSGMARKNEXT;
					lbp->b_flag &= ~MSGNOTMARKNEXT;
					bp->b_flag &= ~MSGMARKNEXT;
				} else if (bp->b_flag & MSGNOTMARKNEXT) {
					lbp->b_flag |= MSGNOTMARKNEXT;
					bp->b_flag &= ~MSGNOTMARKNEXT;
				}

				linkb(lbp, bp);
				bp = lbp;
				/*
				 * The new message logically isn't the first
				 * even though the q_first check below thinks
				 * it is. Clear the firstmsgsigs to make it
				 * not appear to be first.
				 */
				firstmsgsigs = 0;
			}
		}
		break;

	case M_PASSFP:
		wakeups = RSLEEP;
		allmsgsigs = 0;
		if (bp->b_band == 0) {
			firstmsgsigs = S_INPUT | S_RDNORM;
			pollwakeups = POLLIN | POLLRDNORM;
		} else {
			firstmsgsigs = S_INPUT | S_RDBAND;
			pollwakeups = POLLIN | POLLRDBAND;
		}
		mutex_enter(&stp->sd_lock);
		break;

	case M_PROTO:
	case M_PCPROTO:
		ASSERT(stp->sd_rprotofunc != NULL);
		bp = (stp->sd_rprotofunc)(stp->sd_vnode, bp,
			&wakeups, &firstmsgsigs, &allmsgsigs, &pollwakeups);
#define	ALLSIG	(S_INPUT|S_HIPRI|S_OUTPUT|S_MSG|S_ERROR|S_HANGUP|S_RDNORM|\
		S_WRNORM|S_RDBAND|S_WRBAND|S_BANDURG)
#define	ALLPOLL	(POLLIN|POLLPRI|POLLOUT|POLLRDNORM|POLLWRNORM|POLLRDBAND|\
		POLLWRBAND)

		ASSERT((wakeups & ~(RSLEEP|WSLEEP)) == 0);
		ASSERT((firstmsgsigs & ~ALLSIG) == 0);
		ASSERT((allmsgsigs & ~ALLSIG) == 0);
		ASSERT((pollwakeups & ~ALLPOLL) == 0);

		mutex_enter(&stp->sd_lock);
		break;

	default:
		ASSERT(stp->sd_rmiscfunc != NULL);
		bp = (stp->sd_rmiscfunc)(stp->sd_vnode, bp,
			&wakeups, &firstmsgsigs, &allmsgsigs, &pollwakeups);
		ASSERT((wakeups & ~(RSLEEP|WSLEEP)) == 0);
		ASSERT((firstmsgsigs & ~ALLSIG) == 0);
		ASSERT((allmsgsigs & ~ALLSIG) == 0);
		ASSERT((pollwakeups & ~ALLPOLL) == 0);
#undef	ALLSIG
#undef	ALLPOLL
		mutex_enter(&stp->sd_lock);
		break;
	}
	ASSERT(MUTEX_HELD(&stp->sd_lock));

	/* By default generate superset of signals */
	signals = (firstmsgsigs | allmsgsigs);

	/*
	 * The  proto and misc functions can return multiple messages
	 * as a b_next chain. Such messages are processed separately.
	 */
one_more:
	hipri_sig = 0;
	if (bp == NULL) {
		nextbp = NULL;
	} else {
		nextbp = bp->b_next;
		bp->b_next = NULL;

		switch (bp->b_datap->db_type) {
		case M_PCPROTO:
			/*
			 * Only one priority protocol message is allowed at the
			 * stream head at a time.
			 */
			if (stp->sd_flag & STRPRI) {
				TRACE_0(TR_FAC_STREAMS_FR, TR_STRRPUT_PROTERR,
				    "M_PCPROTO already at head");
				freemsg(bp);
				mutex_exit(&stp->sd_lock);
				goto done;
			}
			stp->sd_flag |= STRPRI;
			hipri_sig = 1;
			/* FALLTHRU */
		case M_DATA:
		case M_PROTO:
		case M_PASSFP:
			band = bp->b_band;
			/*
			 * Marking doesn't work well when messages
			 * are marked in more than one band.  We only
			 * remember the last message received, even if
			 * it is placed on the queue ahead of other
			 * marked messages.
			 */
			if (bp->b_flag & MSGMARK)
				stp->sd_mark = bp;
			(void) putq(q, bp);

			/*
			 * If message is a PCPROTO message, always use
			 * firstmsgsigs to determine if a signal should be
			 * sent as strrput is the only place to send
			 * signals for PCPROTO. Other messages are based on
			 * the STRGETINPROG flag. The flag determines if
			 * strrput or (k)strgetmsg will be responsible for
			 * sending the signals, in the firstmsgsigs case.
			 */
			if ((hipri_sig == 1) ||
			    (((stp->sd_flag & STRGETINPROG) == 0) &&
			    (q->q_first == bp)))
				signals = (firstmsgsigs | allmsgsigs);
			else
				signals = allmsgsigs;
			break;

		default:
			mutex_exit(&stp->sd_lock);
			(void) strrput_nondata(q, bp);
			mutex_enter(&stp->sd_lock);
			break;
		}
	}
	ASSERT(MUTEX_HELD(&stp->sd_lock));
	/*
	 * Wake sleeping read/getmsg and cancel deferred wakeup
	 */
	if (wakeups & RSLEEP)
		stp->sd_wakeq &= ~RSLEEP;

	wakeups &= stp->sd_flag;
	if (wakeups & RSLEEP) {
		stp->sd_flag &= ~RSLEEP;
		cv_broadcast(&q->q_wait);
	}
	if (wakeups & WSLEEP) {
		stp->sd_flag &= ~WSLEEP;
		cv_broadcast(&_WR(q)->q_wait);
	}

	/*
	 * strsendsig can handle multiple signals with a
	 * single call.
	 */
	if (stp->sd_sigflags & signals)
		strsendsig(stp->sd_siglist, signals, band, 0);

	if (pollwakeups == (POLLIN | POLLRDNORM)) {
		/*
		 * Can't use rput_opt since it was not
		 * read when sd_lock was held and SR_POLLIN is changed
		 * by strpoll() under sd_lock.
		 */
		if (stp->sd_rput_opt & SR_POLLIN) {
			stp->sd_rput_opt &= ~SR_POLLIN;
			mutex_exit(&stp->sd_lock);
			pollwakeup(&stp->sd_pollist, pollwakeups);
		} else
			mutex_exit(&stp->sd_lock);
	} else if (pollwakeups != 0) {
		mutex_exit(&stp->sd_lock);
		pollwakeup(&stp->sd_pollist, pollwakeups);
	} else {
		mutex_exit(&stp->sd_lock);
	}

done:
	if (nextbp != NULL) {
		/*
		 * Any signals were handled the first time.
		 * Wakeups and pollwakeups are redone to avoid any race
		 * conditions - all the messages are not queued until the
		 * last message has been processed by strrput.
		 */
		bp = nextbp;
		signals = firstmsgsigs = allmsgsigs = 0;
		mutex_enter(&stp->sd_lock);
		goto one_more;
	}
	return (0);
}

int
strrput_nondata(queue_t *q, mblk_t *bp)
{
	struct stdata *stp;
	struct iocblk *iocbp;
	struct stroptions *sop;
	struct copyreq *reqp;
	struct copyresp *resp;
	unsigned char bpri;

	stp = (struct stdata *)q->q_ptr;

	ASSERT(!(stp->sd_flag & STPLEX));
	ASSERT(qclaimed(q));

	switch (bp->b_datap->db_type) {
	case M_ERROR:
		/*
		 * An error has occurred downstream, the errno is in the first
		 * bytes of the message.
		 */
		if ((bp->b_wptr - bp->b_rptr) == 2) {	/* New flavor */
			unsigned char rw = 0;

			mutex_enter(&stp->sd_lock);
			if (*bp->b_rptr != NOERROR) {	/* read error */
				if (*bp->b_rptr != 0) {
					stp->sd_flag |= STRDERR;
					rw |= FLUSHR;
				} else {
					stp->sd_flag &= ~STRDERR;
				}
				stp->sd_rerror = *bp->b_rptr;
			}
			bp->b_rptr++;
			if (*bp->b_rptr != NOERROR) {	/* write error */
				if (*bp->b_rptr != 0) {
					stp->sd_flag |= STWRERR;
					rw |= FLUSHW;
				} else {
					stp->sd_flag &= ~STWRERR;
				}
				stp->sd_werror = *bp->b_rptr;
			}
			if (rw) {
				TRACE_2(TR_FAC_STREAMS_FR,
					TR_STRRPUT_WAKE,
					"strrput cv_broadcast:q %X, bp %X",
					q, bp);
				cv_broadcast(&q->q_wait); /* readers */
				cv_broadcast(&_WR(q)->q_wait); /* writers */
				cv_broadcast(&stp->sd_monitor); /* ioctllers */

				if (stp->sd_sigflags & S_ERROR)
					strsendsig(stp->sd_siglist, S_ERROR, 0,
					    ((rw & FLUSHR) ? stp->sd_rerror :
					    stp->sd_werror));
				mutex_exit(&stp->sd_lock);
				pollwakeup(&stp->sd_pollist, POLLERR);

				bp->b_datap->db_type = M_FLUSH;
				*bp->b_rptr = rw;
				bp->b_wptr = bp->b_rptr + 1;
				/*
				 * Protect against the driver passing up
				 * messages after it has done a qprocsoff.
				 */
				if (_OTHERQ(q)->q_next == NULL)
					freemsg(bp);
				else
					qreply(q, bp);
				return (0);
			} else
				mutex_exit(&stp->sd_lock);
		} else if (*bp->b_rptr != 0) {		/* Old flavor */
			mutex_enter(&stp->sd_lock);
			stp->sd_flag |= (STRDERR|STWRERR);
			stp->sd_rerror = *bp->b_rptr;
			stp->sd_werror = *bp->b_rptr;
			TRACE_2(TR_FAC_STREAMS_FR,
				TR_STRRPUT_WAKE2,
				"strrput wakeup #2:q %X, bp %X", q, bp);
			cv_broadcast(&q->q_wait); /* the readers */
			cv_broadcast(&_WR(q)->q_wait); /* the writers */
			cv_broadcast(&stp->sd_monitor); /* the ioctllers */

			if (stp->sd_sigflags & S_ERROR)
				strsendsig(stp->sd_siglist, S_ERROR, 0,
				    (stp->sd_werror ? stp->sd_werror :
				    stp->sd_rerror));
			mutex_exit(&stp->sd_lock);
			pollwakeup(&stp->sd_pollist, POLLERR);

			bp->b_datap->db_type = M_FLUSH;
			*bp->b_rptr = FLUSHRW;
			/*
			 * Protect against the driver passing up
			 * messages after it has done a qprocsoff.
			 */
			if (_OTHERQ(q)->q_next == NULL)
				freemsg(bp);
			else
				qreply(q, bp);
			return (0);
		}
		freemsg(bp);
		return (0);

	case M_HANGUP:

		freemsg(bp);
		mutex_enter(&stp->sd_lock);
		stp->sd_werror = ENXIO;
		stp->sd_flag |= STRHUP;
		stp->sd_flag &= ~(WSLEEP|RSLEEP);

		/*
		 * send signal if controlling tty
		 */

		if (stp->sd_sidp) {
			prsignal(stp->sd_sidp, SIGHUP);
			if (stp->sd_sidp != stp->sd_pgidp)
				pgsignal(stp->sd_pgidp, SIGTSTP);
		}

		/*
		 * wake up read, write, and exception pollers and
		 * reset wakeup mechanism.
		 */
		cv_broadcast(&q->q_wait);	/* the readers */
		cv_broadcast(&_WR(q)->q_wait);	/* the writers */
		cv_broadcast(&stp->sd_monitor);	/* the ioctllers */
		mutex_exit(&stp->sd_lock);
		strhup(stp);
		return (0);

	case M_UNHANGUP:
		freemsg(bp);
		mutex_enter(&stp->sd_lock);
		stp->sd_werror = 0;
		stp->sd_flag &= ~STRHUP;
		mutex_exit(&stp->sd_lock);
		return (0);

	case M_SIG:
		/*
		 * Someone downstream wants to post a signal.  The
		 * signal to post is contained in the first byte of the
		 * message.  If the message would go on the front of
		 * the queue, send a signal to the process group
		 * (if not SIGPOLL) or to the siglist processes
		 * (SIGPOLL).  If something is already on the queue,
		 * OR if we are delivering a delayed suspend (*sigh*
		 * another "tty" hack) and there's no one sleeping already,
		 * just enqueue the message.
		 */
		mutex_enter(&stp->sd_lock);
		if (q->q_first || (*bp->b_rptr == SIGTSTP &&
		    !(stp->sd_flag & RSLEEP))) {
			(void) putq(q, bp);
			mutex_exit(&stp->sd_lock);
			return (0);
		}
		mutex_exit(&stp->sd_lock);
		/* FALLTHRU */

	case M_PCSIG:
		/*
		 * Don't enqueue, just post the signal.
		 */
		strsignal(stp, *bp->b_rptr, 0L);
		freemsg(bp);
		return (0);

	case M_FLUSH:
		/*
		 * Flush queues.  The indication of which queues to flush
		 * is in the first byte of the message.  If the read queue
		 * is specified, then flush it.  If FLUSHBAND is set, just
		 * flush the band specified by the second byte of the message.
		 *
		 * If a module has issued a M_SETOPT to not flush hi
		 * priority messages off of the stream head, then pass this
		 * flag into the flushq code to preserve such messages.
		 *
		 * Note that flushq/flushband handles M_PASSFP by using
		 * freemsg_flush instead of freemsg.
		 */

		if (*bp->b_rptr & FLUSHR) {
			mutex_enter(&stp->sd_lock);
			if (*bp->b_rptr & FLUSHBAND) {
				ASSERT((bp->b_wptr - bp->b_rptr) >= 2);
				flushband(q, *(bp->b_rptr + 1), FLUSHALL);
			} else
				flushq_common(q, FLUSHALL,
				    stp->sd_read_opt & RFLUSHPCPROT);
			if ((q->q_first == NULL) ||
			    (q->q_first->b_datap->db_type < QPCTL))
				stp->sd_flag &= ~STRPRI;
			else {
				ASSERT(stp->sd_flag & STRPRI);
			}
			mutex_exit(&stp->sd_lock);
		}
		if ((*bp->b_rptr & FLUSHW) && !(bp->b_flag & MSGNOLOOP)) {
			*bp->b_rptr &= ~FLUSHR;
			bp->b_flag |= MSGNOLOOP;
			/*
			 * Protect against the driver passing up
			 * messages after it has done a qprocsoff.
			 */
			if (_OTHERQ(q)->q_next == NULL)
				freemsg(bp);
			else
				qreply(q, bp);
			return (0);
		}
		freemsg(bp);
		return (0);

	case M_IOCACK:
	case M_IOCNAK:
		iocbp = (struct iocblk *)bp->b_rptr;
		/*
		 * If not waiting for ACK or NAK then just free msg.
		 * If incorrect id sequence number then just free msg.
		 * If already have ACK or NAK for user then this is a
		 *    duplicate, display a warning and free the msg.
		 */
		mutex_enter(&stp->sd_lock);
		if ((stp->sd_flag & IOCWAIT) == 0 || stp->sd_iocblk ||
		    (stp->sd_iocid != iocbp->ioc_id)) {
			/*
			 * If the ACK/NAK is a dup, display a message
			 * Dup is when sd_iocid == ioc_id, and
			 * sd_iocblk == <valid ptr> or -1 (the former
			 * is when an ioctl has been put on the stream
			 * head, but has not yet been consumed, the
			 * later is when it has been consumed).
			 */
			if ((stp->sd_iocid == iocbp->ioc_id) &&
			    (stp->sd_iocblk != NULL)) {
				short mid = q->q_qinfo->qi_minfo->mi_idnum;
				short sid = 0;
				(void) strlog(mid, sid, (char)1,
				    SL_CONSOLE|SL_TRACE|SL_ERROR,
				    "Warning: %d Recieved duplicate M_IOCACK",
				    stp);
			}
			freemsg(bp);
			mutex_exit(&stp->sd_lock);
			return (0);
		}

		/*
		 * Assign ACK or NAK to user and wake up.
		 */
		stp->sd_iocblk = bp;
		cv_broadcast(&stp->sd_monitor);
		mutex_exit(&stp->sd_lock);
		return (0);

	case M_COPYIN:
	case M_COPYOUT:
		reqp = (struct copyreq *)bp->b_rptr;

		/*
		 * If not waiting for ACK or NAK then just fail request.
		 * If already have ACK, NAK, or copy request, then just
		 * fail request.
		 * If incorrect id sequence number then just fail request.
		 */
		mutex_enter(&stp->sd_lock);
		if ((stp->sd_flag & IOCWAIT) == 0 || stp->sd_iocblk ||
		    (stp->sd_iocid != reqp->cq_id)) {
			if (bp->b_cont) {
				freemsg(bp->b_cont);
				bp->b_cont = NULL;
			}
			bp->b_datap->db_type = M_IOCDATA;
			bp->b_wptr = bp->b_rptr + sizeof (struct copyresp);
			resp = (struct copyresp *)bp->b_rptr;
			resp->cp_rval = (caddr_t)1;	/* failure */
			mutex_exit(&stp->sd_lock);
			putnext(stp->sd_wrq, bp);
			return (0);
		}

		/*
		 * Assign copy request to user and wake up.
		 */
		stp->sd_iocblk = bp;
		cv_broadcast(&stp->sd_monitor);
		mutex_exit(&stp->sd_lock);
		return (0);

	case M_SETOPTS:
		/*
		 * Set stream head options (read option, write offset,
		 * min/max packet size, and/or high/low water marks for
		 * the read side only).
		 */

		bpri = 0;
		sop = (struct stroptions *)bp->b_rptr;
		mutex_enter(&stp->sd_lock);
		if (sop->so_flags & SO_READOPT) {
			switch (sop->so_readopt & RMODEMASK) {
			case RNORM:
				stp->sd_read_opt &= ~(RD_MSGDIS | RD_MSGNODIS);
				break;

			case RMSGD:
				stp->sd_read_opt =
				    ((stp->sd_read_opt & ~RD_MSGNODIS) |
				    RD_MSGDIS);
				break;

			case RMSGN:
				stp->sd_read_opt =
				    ((stp->sd_read_opt & ~RD_MSGDIS) |
				    RD_MSGNODIS);
				break;
			}
			switch (sop->so_readopt & RPROTMASK) {
			case RPROTNORM:
				stp->sd_read_opt &= ~(RD_PROTDAT | RD_PROTDIS);
				break;

			case RPROTDAT:
				stp->sd_read_opt =
				    ((stp->sd_read_opt & ~RD_PROTDIS) |
				    RD_PROTDAT);
				break;

			case RPROTDIS:
				stp->sd_read_opt =
				    ((stp->sd_read_opt & ~RD_PROTDAT) |
				    RD_PROTDIS);
				break;
			}
			switch (sop->so_readopt & RFLUSHMASK) {
			case RFLUSHPCPROT:
				/*
				 * This sets the stream head to NOT flush
				 * M_PCPROTO messages.
				 */
				stp->sd_read_opt |= RFLUSHPCPROT;
				break;
			}
		}
		if (sop->so_flags & SO_ERROPT) {
			switch (sop->so_erropt & RERRMASK) {
			case RERRNORM:
				stp->sd_flag &= ~STRDERRNONPERSIST;
				break;
			case RERRNONPERSIST:
				stp->sd_flag |= STRDERRNONPERSIST;
				break;
			}
			switch (sop->so_erropt & WERRMASK) {
			case WERRNORM:
				stp->sd_flag &= ~STWRERRNONPERSIST;
				break;
			case WERRNONPERSIST:
				stp->sd_flag |= STWRERRNONPERSIST;
				break;
			}
		}
		if (sop->so_flags & SO_COPYOPT) {
			if (sop->so_copyopt & MAPINOK)
				stp->sd_flag |= STMAPINOK;
			else if (sop->so_copyopt & NOMAPIN)
				stp->sd_flag &= ~STMAPINOK;
			if (sop->so_copyopt & REMAPOK)
				stp->sd_flag |= STREMAPOK;
			else if (sop->so_copyopt & NOREMAP)
				stp->sd_flag &= ~STREMAPOK;
		}
		if (sop->so_flags & SO_WROFF)
			stp->sd_wroff = sop->so_wroff;
		if (sop->so_flags & SO_MINPSZ)
			q->q_minpsz = sop->so_minpsz;
		if (sop->so_flags & SO_MAXPSZ)
			q->q_maxpsz = sop->so_maxpsz;
		if (sop->so_flags & SO_MAXBLK)
			stp->sd_maxblk = sop->so_maxblk;
		if (sop->so_flags & SO_HIWAT) {
		    if (sop->so_flags & SO_BAND) {
			if (strqset(q, QHIWAT, sop->so_band, sop->so_hiwat))
				cmn_err(CE_WARN,
				    "strrput: could not allocate qband\n");
			else
				bpri = sop->so_band;
		    } else {
			q->q_hiwat = sop->so_hiwat;
		    }
		}
		if (sop->so_flags & SO_LOWAT) {
		    if (sop->so_flags & SO_BAND) {
			if (strqset(q, QLOWAT, sop->so_band, sop->so_lowat))
				cmn_err(CE_WARN,
				    "strrput: could not allocate qband\n");
			else
				bpri = sop->so_band;
		    } else {
			q->q_lowat = sop->so_lowat;
		    }
		}
		if (sop->so_flags & SO_MREADON)
			stp->sd_flag |= SNDMREAD;
		if (sop->so_flags & SO_MREADOFF)
			stp->sd_flag &= ~SNDMREAD;
		if (sop->so_flags & SO_NDELON)
			stp->sd_flag |= OLDNDELAY;
		if (sop->so_flags & SO_NDELOFF)
			stp->sd_flag &= ~OLDNDELAY;
		if (sop->so_flags & SO_ISTTY)
			stp->sd_flag |= STRISTTY;
		if (sop->so_flags & SO_ISNTTY)
			stp->sd_flag &= ~STRISTTY;
		if (sop->so_flags & SO_TOSTOP)
			stp->sd_flag |= STRTOSTOP;
		if (sop->so_flags & SO_TONSTOP)
			stp->sd_flag &= ~STRTOSTOP;
		if (sop->so_flags & SO_DELIM)
			stp->sd_flag |= STRDELIM;
		if (sop->so_flags & SO_NODELIM)
			stp->sd_flag &= ~STRDELIM;

		mutex_exit(&stp->sd_lock);
		freemsg(bp);

		/* Check backenable in case the water marks changed */
		qbackenable(q, bpri);
		return (0);

	/*
	 * The following set of cases deal with situations where two stream
	 * heads are connected to each other (twisted streams).  These messages
	 * have no meaning at the stream head.
	 */
	case M_BREAK:
	case M_CTL:
	case M_DELAY:
	case M_START:
	case M_STOP:
	case M_IOCDATA:
	case M_STARTI:
	case M_STOPI:
		freemsg(bp);
		return (0);

	case M_IOCTL:
		/*
		 * Always NAK this condition
		 * (makes no sense)
		 * If there is one or more threads in the read side
		 * rwnext we have to defer the nacking until that thread
		 * returns (in strget).
		 */
		mutex_enter(&stp->sd_lock);
		if (stp->sd_struiodnak != 0) {
			/*
			 * Defer NAK to the streamhead. Queue at the end
			 * the list.
			 */
			mblk_t *mp = stp->sd_struionak;

			while (mp && mp->b_next)
				mp = mp->b_next;
			if (mp)
				mp->b_next = bp;
			else
				stp->sd_struionak = bp;
			bp->b_next = NULL;
			mutex_exit(&stp->sd_lock);
			return (0);
		}
		mutex_exit(&stp->sd_lock);

		bp->b_datap->db_type = M_IOCNAK;
		/*
		 * Protect against the driver passing up
		 * messages after it has done a qprocsoff.
		 */
		if (_OTHERQ(q)->q_next == NULL)
			freemsg(bp);
		else
			qreply(q, bp);
		return (0);

	default:
#ifdef DEBUG
		cmn_err(CE_WARN,
			"bad message type %x received at stream head\n",
			bp->b_datap->db_type);
#endif
		freemsg(bp);
		return (0);
	}

	/* NOTREACHED */
}

/*
 * Copyin and send data down a stream.
 * The caller will allocate and copyin any control part that precedes the
 * message and pass than in as mctl.
 *
 * Caller should *not* hold sd_lock.
 * When EWOULDBLOCK is returned the caller has to redo the canputnext
 * under sd_lock in order to avoid missing a backenabling wakeup.
 *
 * Use iosize = -1 to not send any M_DATA. iosize = 0 sends zero-length M_DATA.
 *
 * Set MSG_IGNFLOW in flags to ignore flow control for hipri messages.
 * For sync streams we can only ignore flow control by reverting to using
 * putnext.
 *
 * If sd_maxblk is less than *iosize this routine might return without
 * transferring all of *iosize. In all cases, on return *iosize will contain
 * the amount of data that was transferred.
 */
static int
strput(struct stdata *stp, mblk_t *mctl, struct uio *uiop, ssize_t *iosize,
    int b_flag, int pri, int flags)
{
	struiod_t uiod;
	mblk_t *mp;
	queue_t *wqp = stp->sd_wrq;
	int error = 0;
	ssize_t count = *iosize;

	ASSERT(MUTEX_NOT_HELD(&stp->sd_lock));
	ASSERT(!(flags & (STRUIO_POSTPONE|STRUIO_MAPIN)));

	if (uiop != NULL && count >= 0) {
		/*
		 * Should we attempt zero-copy?
		 * We insist iov_base to be on page boundary to avoid sending
		 * small packets.
		 * Note that syncstream is disabled when zero-copy is attempted.
		 */
		if ((stp->sd_flag & STMAPINOK) &&
		    (uiop->uio_resid >= strzc_write_threshold) &&
		    ((uintptr_t)uiop->uio_iov->iov_base & PAGEOFFSET) == 0 &&
		    (uiop->uio_segflg != UIO_SYSSPACE) && (count >= PAGESIZE)) {

			/*
			 * Round count down to an integral number of
			 * the page size
			 */
			count &= ~PAGEOFFSET;
			*iosize = count;
			flags |= STRUIO_MAPIN;
		} else {
			flags |= stp->sd_struiowrq ? STRUIO_POSTPONE : 0;
#ifdef ZC_TEST
			/* Test only - disable syncstream */
			if (syncstream)
				flags &= ~STRUIO_POSTPONE;
#endif
		}
	}

	if (!(flags & STRUIO_POSTPONE)) {
		/*
		 * Use regular canputnext, strmakedata, putnext sequence.
		 */
		if (pri == 0) {
			if (!canputnext(wqp) && !(flags & MSG_IGNFLOW)) {
				freemsg(mctl);
				return (EWOULDBLOCK);
			}
		} else {
			if (!(flags & MSG_IGNFLOW) && !bcanputnext(wqp, pri)) {
				freemsg(mctl);
				return (EWOULDBLOCK);
			}
		}

		if ((error = strmakedata(iosize, uiop, stp, flags,
					&mp)) != 0) {
			freemsg(mctl);
			/*
			 * need to change return code to ENOMEM
			 * so that this is not confused with
			 * flow control, EAGAIN.
			 */

			if (error == EAGAIN)
				return (ENOMEM);
			else
				return (error);
		}
		if (mctl != NULL) {
			if (mctl->b_cont == NULL)
				mctl->b_cont = mp;
			else if (mp != NULL)
				linkb(mctl, mp);
			mp = mctl;
			/*
			 * Note that for interrupt thread, the CRED() is
			 * NULL.  db_uid is initialized to -1 when a
			 * dblk structure is allocated.
			 */
			if (CRED() != NULL) {
				mp->b_datap->db_uid = CRED()->cr_uid;
			}
		} else if (mp == NULL)
			return (0);

		mp->b_flag |= b_flag;
		mp->b_band = (uchar_t)pri;

		if (flags & MSG_IGNFLOW) {
			/*
			 * XXX Hack: Don't get stuck running service
			 * procedures. This is needed for sockfs when
			 * sending the unbind message out of the rput
			 * procedure - we don't want a put procedure
			 * to run service procedures.
			 */
			putnext(wqp, mp);
		} else {
			run_queues = 1;
			putnext(wqp, mp);
			queuerun();
		}
		return (0);
	}
	/*
	 * Stream supports rwnext() for the write side.
	 */
	if ((error = strmakedata(iosize, uiop, stp, flags, &mp)) != 0) {
		freemsg(mctl);
		/*
		 * need to change return code to ENOMEM
		 * so that this is not confused with
		 * flow control, EAGAIN.
		 */
		if (error == EAGAIN)
			return (ENOMEM);
		else
			return (error);

	}
	if (mctl != NULL) {
		if (mctl->b_cont == NULL)
			mctl->b_cont = mp;
		else if (mp != NULL)
			linkb(mctl, mp);
		mp = mctl;
		/*
		 * Note that for interrupt thread, the CRED() is
		 * NULL.  db_uid is initialized to -1 when a
		 * dblk structure is allocated.
		 */
		if (CRED() != NULL) {
			mp->b_datap->db_uid = CRED()->cr_uid;
		}
	} else if (mp == NULL)
		return (0);

	mp->b_flag |= b_flag;
	mp->b_band = (uchar_t)pri;

	(void) uiodup(uiop, &uiod.d_uio, uiod.d_iov,
		sizeof (uiod.d_iov) / sizeof (*uiod.d_iov));
	uiod.d_uio.uio_offset = 0;
	uiod.d_mp = mp;
	error = rwnext(wqp, &uiod);
	if (! uiod.d_mp) {
		uioskip(uiop, *iosize);
		return (error);
	}
	ASSERT(mp == uiod.d_mp);
	if (error == EINVAL) {
		/*
		 * The stream plumbing must have changed while
		 * we were away, so just turn off rwnext()s.
		 */
		error = 0;
	} else if (error == EBUSY || error == EWOULDBLOCK) {
		/*
		 * Couldn't enter a perimeter or took a page fault,
		 * so fall-back to putnext().
		 */
		error = 0;
	} else {
		freemsg(mp);
		return (error);
	}
	/* Have to check canput before consuming data from the uio */
	if (pri == 0) {
		if (!canputnext(wqp) && !(flags & MSG_IGNFLOW)) {
			freemsg(mp);
			return (EWOULDBLOCK);
		}
	} else {
		if (!bcanputnext(wqp, pri) && !(flags & MSG_IGNFLOW)) {
			freemsg(mp);
			return (EWOULDBLOCK);
		}
	}
	ASSERT(mp == uiod.d_mp);
	/* Copyin data from the uio */
	if ((error = struioget(wqp, mp, &uiod, 0)) != 0) {
		freemsg(mp);
		return (error);
	}
	uioskip(uiop, *iosize);
	if (flags & MSG_IGNFLOW) {
		/*
		 * XXX Hack: Don't get stuck running service procedures.
		 * This is needed for sockfs when sending the unbind message
		 * out of the rput procedure - we don't want a put procedure
		 * to run service procedures.
		 */
		putnext(wqp, mp);
	} else {
		run_queues = 1;
		putnext(wqp, mp);
		queuerun();
	}
	return (0);
}

/*
 * Write attempts to break the write request into messages conforming
 * with the minimum and maximum packet sizes set downstream.
 *
 * Write will not block if downstream queue is full and
 * O_NDELAY is set, otherwise it will block waiting for the queue to get room.
 *
 * A write of zero bytes gets packaged into a zero length message and sent
 * downstream like any other message.
 *
 * If buffers of the requested sizes are not available, the write will
 * sleep until the buffers become available.
 *
 * Write (if specified) will supply a write offset in a message if it
 * makes sense. This can be specified by downstream modules as part of
 * a M_SETOPTS message.  Write will not supply the write offset if it
 * cannot supply any data in a buffer.  In other words, write will never
 * send down an empty packet due to a write offset.
 */
/* ARGSUSED2 */
int
strwrite(struct vnode *vp, struct uio *uiop, cred_t *crp)
{
	struct stdata *stp;
	struct queue *wqp;
	ssize_t rmin, rmax;
	ssize_t iosize;
	char waitflag;
	int tempmode;
	int error = 0;
	int b_flag;

	ASSERT(vp->v_stream);
	stp = vp->v_stream;

	if (stp->sd_sidp != NULL && stp->sd_vnode->v_type != VFIFO)
		if (error = straccess(stp, JCWRITE))
			return (error);

	/* Fast check of flags before acquiring the lock */
	if (stp->sd_flag & (STWRERR|STRHUP|STPLEX)) {
		mutex_enter(&stp->sd_lock);
		/*
		 * For modem support, POSIX states that if
		 * a disconnect is detected EIO should be
		 * returned
		 */
		if ((stp->sd_flag & (STPLEX|STRHUP)) == STRHUP)
			error = EIO;
		else
			error = strgeterr(stp, STWRERR|STRHUP|STPLEX, 0);
		mutex_exit(&stp->sd_lock);
		if (error != 0) {
			if (!(stp->sd_flag & STPLEX) &&
			    (stp->sd_wput_opt & SW_SIGPIPE)) {
				psignal(ttoproc(curthread), SIGPIPE);
				error = EPIPE;
			}
			return (error);
		}
	}
	wqp = stp->sd_wrq;

	/* get these values from them cached in the stream head */
	rmin = stp->sd_qn_minpsz;
	rmax = stp->sd_qn_maxpsz;

	/*
	 * Check the min/max packet size constraints.  If min packet size
	 * is non-zero, the write cannot be split into multiple messages
	 * and still guarantee the size constraints.
	 */
	TRACE_1(TR_FAC_STREAMS_FR, TR_STRWRITE_IN, "strwrite in:q %X", wqp);

	ASSERT((rmax >= 0) || (rmax == INFPSZ));
	if (rmax == 0) {
		return (0);
	}
	if (rmin > 0) {
		if (uiop->uio_resid < rmin) {
			TRACE_3(TR_FAC_STREAMS_FR, TR_STRWRITE_OUT,
				"strwrite out:q %X out %d error %d",
				wqp, 0, ERANGE);
			return (ERANGE);
		}
		if ((rmax != INFPSZ) && (uiop->uio_resid > rmax)) {
			TRACE_3(TR_FAC_STREAMS_FR, TR_STRWRITE_OUT,
				"strwrite out:q %X out %d error %d",
				wqp, 1, ERANGE);
			return (ERANGE);
		}
	}

	/*
	 * Do until count satisfied or error.
	 */
	waitflag = WRITEWAIT;
	if (stp->sd_flag & OLDNDELAY)
		tempmode = uiop->uio_fmode & ~FNDELAY;
	else
		tempmode = uiop->uio_fmode;

	if (rmax == INFPSZ)
		rmax = uiop->uio_resid;

	/*
	 * Note that tempmode does not get used in strput/strmakedata
	 * but only in strwaitq. The other routines use uio_fmode
	 * unmodified.
	 */

	/* LINTED: constant in conditional context */
	while (1) {	/* breaks when uio_resid reaches zero */
		/*
		 * Determine the size of the next message to be
		 * packaged.  May have to break write into several
		 * messages based on max packet size.
		 */
		iosize = MIN(uiop->uio_resid, rmax);

		/*
		 * Put block downstream when flow control allows it.
		 */
		if ((stp->sd_flag & STRDELIM) && (uiop->uio_resid == iosize))
			b_flag = MSGDELIM;
		else
			b_flag = 0;

		for (;;) {
			int done = 0;

			error = strput(stp, NULL, uiop, &iosize, b_flag,
				0, 0);
			if (error == 0)
				break;
			if (error != EWOULDBLOCK)
				goto out;

			mutex_enter(&stp->sd_lock);
			/*
			 * Check for a missed wakeup.
			 * Needed since strput did not hold sd_lock across
			 * the canputnext.
			 */
			if (canputnext(wqp)) {
				/* Try again */
				mutex_exit(&stp->sd_lock);
				continue;
			}
			TRACE_1(TR_FAC_STREAMS_FR, TR_STRWRITE_WAIT,
				"strwrite wait:q %X wait", wqp);
			if ((error = strwaitq(stp, waitflag, (ssize_t)0,
			    tempmode, -1, &done)) != 0 || done) {
				mutex_exit(&stp->sd_lock);
				if ((vp->v_type == VFIFO) &&
				    (uiop->uio_fmode & FNDELAY) &&
				    (error == EAGAIN))
					error = 0;
				goto out;
			}
			TRACE_1(TR_FAC_STREAMS_FR, TR_STRWRITE_WAKE,
				"strwrite wake:q %X awakes", wqp);
			mutex_exit(&stp->sd_lock);
			if (stp->sd_sidp != NULL &&
			    stp->sd_vnode->v_type != VFIFO)
				if (error = straccess(stp, JCWRITE))
					goto out;
		}
		waitflag |= NOINTR;
		TRACE_2(TR_FAC_STREAMS_FR, TR_STRWRITE_RESID,
			"strwrite resid:q %X resid %d", wqp, uiop->uio_resid);
		if (uiop->uio_resid) {
			/* Recheck for errors - needed for sockets */
			if (stp->sd_flag & (STWRERR|STRHUP|STPLEX) &&
			    stp->sd_wput_opt & SW_RECHECK_ERR) {
				mutex_enter(&stp->sd_lock);
				error = strgeterr(stp,
						STWRERR|STRHUP|STPLEX, 0);
				mutex_exit(&stp->sd_lock);
				if (error != 0) {
					if (!(stp->sd_flag & STPLEX) &&
					    (stp->sd_wput_opt & SW_SIGPIPE))
						psignal(ttoproc(curthread),
							SIGPIPE);
					return (error);
				}
			}
			continue;
		}
		break;
	}

out:
	/*
	 * For historic reasons, applications expect EAGAIN
	 * when data mblk could not be allocated. so change
	 * ENOMEM back to EAGAIN
	 */
	if (error == ENOMEM)
		error = EAGAIN;
	TRACE_3(TR_FAC_STREAMS_FR, TR_STRWRITE_OUT,
		"strwrite out:q %X out %d error %d", wqp, 2, error);
	return (error);
}

/*
 * Stream head write service routine.
 * Its job is to wake up any sleeping writers when a queue
 * downstream needs data (part of the flow control in putq and getq).
 * It also must wake anyone sleeping on a poll().
 * For stream head right below mux module, it must also invoke put procedure
 * of next downstream module.
 */
int
strwsrv(queue_t *q)
{
	struct stdata *stp;
	queue_t *tq;
	qband_t *qbp;
	int i;
	qband_t *myqbp;
	int isevent;
	unsigned char	qbf[NBAND];	/* band flushing backenable flags */

	TRACE_1(TR_FAC_STREAMS_FR,
		TR_STRWSRV, "strwsrv:q %X", q);
	stp = (struct stdata *)q->q_ptr;
	ASSERT(qclaimed(q));
	mutex_enter(&stp->sd_lock);
	ASSERT(!(stp->sd_flag & STPLEX));

	if (stp->sd_flag & WSLEEP) {
		stp->sd_flag &= ~WSLEEP;
		cv_broadcast(&q->q_wait);
	}
	mutex_exit(&stp->sd_lock);

	/* The other end of a stream pipe went away. */
	if ((tq = q->q_next) == NULL) {
		return (0);
	}

	/* Find the next module forward that has a service procedure */
	claimstr(q);
	tq = q->q_nfsrv;
	ASSERT(tq != NULL);

	if ((q->q_flag & QBACK)) {
		if ((tq->q_flag & QFULL)) {
			mutex_enter(QLOCK(tq));
			if (!(tq->q_flag & QFULL)) {
				mutex_exit(QLOCK(tq));
				goto wakeup;
			}
			/*
			 * The queue must have become full again. Set QWANTW
			 * again so strwsrv will be back enabled when
			 * the queue becomes non-full next time.
			 */
			tq->q_flag |= QWANTW;
			mutex_exit(QLOCK(tq));
		} else {
		wakeup:
			mutex_enter(&stp->sd_lock);
			if (stp->sd_sigflags & S_WRNORM)
				strsendsig(stp->sd_siglist, S_WRNORM, 0, 0);
			mutex_exit(&stp->sd_lock);
			pollwakeup(&stp->sd_pollist, POLLWRNORM);
		}
	}

	isevent = 0;
	i = 1;
	bzero((caddr_t)qbf, NBAND);
	mutex_enter(QLOCK(tq));
	if ((myqbp = q->q_bandp) != NULL)
		for (qbp = tq->q_bandp; qbp && myqbp; qbp = qbp->qb_next) {
			ASSERT(myqbp);
			if ((myqbp->qb_flag & QB_BACK)) {
				if (qbp->qb_flag & QB_FULL) {
					/*
					 * The band must have become full again.
					 * Set QB_WANTW again so strwsrv will
					 * be back enabled when the band becomes
					 * non-full next time.
					 */
					qbp->qb_flag |= QB_WANTW;
				} else {
					isevent = 1;
					qbf[i] = 1;
				}
			}
			myqbp = myqbp->qb_next;
			i++;
		}
	mutex_exit(QLOCK(tq));

	if (isevent) {
	    for (i = tq->q_nband; i; i--) {
		if (qbf[i]) {
			mutex_enter(&stp->sd_lock);
			if (stp->sd_sigflags & S_WRBAND)
				strsendsig(stp->sd_siglist, S_WRBAND,
					(uchar_t)i, 0);
			mutex_exit(&stp->sd_lock);
			pollwakeup(&stp->sd_pollist, POLLWRBAND);
		}
	    }
	}

	releasestr(q);
	return (0);
}

/*
 * XXX - The kbd and ms ioctls have been
 * hard-coded in the stream head.  This should
 * be fixed later on.
 */
#define	FORNOW
#ifdef FORNOW
#include <sys/kbio.h>
#include <sys/msio.h>
#include <sys/tty.h>
#include <sys/ptyvar.h>
#include <sys/vuid_event.h>
#endif

/*
 * Special case of strcopyin/strcopyout for copying
 * struct strioctl that can deal with both data
 * models.
 */

#ifdef	_LP64

static int
strcopyin_strioctl(void *from, void *to, int flag, int copyflag)
{
	struct	strioctl32 strioc32;
	struct	strioctl *striocp;

	if (copyflag & U_TO_K) {
		ASSERT((copyflag & K_TO_K) == 0);

		if ((flag & FMODELS) == DATAMODEL_ILP32) {
			if (copyin(from, &strioc32, sizeof (strioc32)))
				return (EFAULT);

			striocp = (struct strioctl *)to;
			striocp->ic_cmd	= strioc32.ic_cmd;
			striocp->ic_timout = strioc32.ic_timout;
			striocp->ic_len	= strioc32.ic_len;
			striocp->ic_dp	= (char *)strioc32.ic_dp;

		} else { /* NATIVE data model */
			if (copyin(from, to, sizeof (struct strioctl))) {
				return (EFAULT);
			} else {
				return (0);
			}
		}
	} else {
		ASSERT(copyflag & K_TO_K);
		bcopy(from, to, sizeof (struct strioctl));
	}
	return (0);
}

static int
strcopyout_strioctl(void *from, void *to, int flag, int copyflag)
{
	struct	strioctl32 strioc32;
	struct	strioctl *striocp;

	if (copyflag & U_TO_K) {
		ASSERT((copyflag & K_TO_K) == 0);

		if ((flag & FMODELS) == DATAMODEL_ILP32) {
			striocp = (struct strioctl *)from;
			strioc32.ic_cmd	= striocp->ic_cmd;
			strioc32.ic_timout = striocp->ic_timout;
			strioc32.ic_len	= striocp->ic_len;
			strioc32.ic_dp	= (caddr32_t)striocp->ic_dp;
			ASSERT((char *)strioc32.ic_dp == striocp->ic_dp);

			if (copyout(&strioc32, to, sizeof (strioc32)))
				return (EFAULT);

		} else { /* NATIVE data model */
			if (copyout(from, to, sizeof (struct strioctl))) {
				return (EFAULT);
			} else {
				return (0);
			}
		}
	} else {
		ASSERT(copyflag & K_TO_K);
		bcopy(from, to, sizeof (struct strioctl));
	}
	return (0);
}

#else	/* ! _LP64 */

/* ARGSUSED2 */
static int
strcopyin_strioctl(void *from, void *to, int flag, int copyflag)
{
	return (strcopyin(from, to, sizeof (struct strioctl),
	    STRIOCTL, copyflag));
}

/* ARGSUSED2 */
static int
strcopyout_strioctl(void *from, void *to, int flag, int copyflag)
{
	return (strcopyout(from, to, sizeof (struct strioctl),
	    STRIOCTL, copyflag));
}

#endif	/* _LP64 */

/*
 * ioctl for streams
 */
int
strioctl(struct vnode *vp, int cmd, intptr_t arg, int flag, int copyflag,
    cred_t *crp, int *rvalp)
{
	struct stdata *stp;
	struct strioctl strioc;
	struct uio uio;
	struct iovec iov;
	enum jcaccess access;
	mblk_t *mp;
	int error = 0;
	int done = 0;
	char *fmt = NULL;
	ssize_t	rmin, rmax;
	queue_t *wrq;
	queue_t *rdq;

	if (flag & FKIOCTL)
		copyflag = K_TO_K;
	ASSERT(vp->v_stream);
	ASSERT(copyflag == U_TO_K || copyflag == K_TO_K);
	stp = vp->v_stream;

	TRACE_3(TR_FAC_STREAMS_FR,
		TR_IOCTL_ENTER,
		"strioctl:stp %X cmd %X arg %lX", stp, cmd, arg);

#ifdef C2_AUDIT
	if (audit_active)
		audit_strioctl(vp, cmd, arg, flag, copyflag, crp, rvalp);
#endif

	/*
	 * If the copy is kernel to kernel, make sure that the FNATIVE
	 * flag is set.  After this it would be a serious error to have
	 * no model flag.
	 */
	if (copyflag == K_TO_K)
		flag = (flag & ~FMODELS) | FNATIVE;

	ASSERT((flag & FMODELS) != 0);

	wrq = stp->sd_wrq;
	rdq = _RD(wrq);

	switch (cmd) {
	case I_RECVFD:
	case I_E_RECVFD:
		access = JCREAD;
		break;

	case I_FDINSERT:
	case I_SENDFD:
	case TIOCSTI:
		access = JCWRITE;
		break;

	case TCGETA:
	case TCGETS:
	case TIOCGETP:
	case TIOCGPGRP:
	case JWINSIZE:
	case TIOCGSID:
	case TIOCMGET:
	case LDGETT:
	case TIOCGETC:
	case TIOCLGET:
	case TIOCGLTC:
	case TIOCGETD:
	case TIOCGWINSZ:
	case LDGMAP:
	case I_CANPUT:
	case I_NREAD:
	case FIONREAD:
	case FIORDCHK:
	case I_FIND:
	case I_LOOK:
	case I_GRDOPT:
	case I_GETSIG:
	case I_PEEK:
	case I_GWROPT:
	case I_LIST:
	case I_CKBAND:
	case I_GETBAND:
	case I_GETCLTIME:
		access = JCGETP;
		break;

	/* We should never see these here, should be handled by iwscn */
	case SRIOCSREDIR:
	case SRIOCISREDIR:
		return (EINVAL);

	default:
		access = JCSETP;
		break;
	}


	if (stp->sd_sidp != NULL && stp->sd_vnode->v_type != VFIFO)
		if (error = straccess(stp, access))
			return (error);

	mutex_enter(&stp->sd_lock);

	switch (cmd) {
	case I_RECVFD:
	case I_E_RECVFD:
	case I_PEEK:
	case I_NREAD:
	case FIONREAD:
	case FIORDCHK:
	case I_ATMARK:
	case FIONBIO:
	case FIOASYNC:
		if (stp->sd_flag & (STRDERR|STPLEX)) {
			error = strgeterr(stp, STRDERR|STPLEX, 0);
			if (error != 0) {
				mutex_exit(&stp->sd_lock);
				return (error);
			}
		}
		break;

	default:
		if (stp->sd_flag & (STRDERR|STWRERR|STPLEX)) {
			error = strgeterr(stp, STRDERR|STWRERR|STPLEX, 0);
			if (error != 0) {
				mutex_exit(&stp->sd_lock);
				return (error);
			}
		}
	}

	mutex_exit(&stp->sd_lock);

	switch (cmd) {
	default:
		if (((cmd & IOCTYPE) == LDIOC) ||
		    ((cmd & IOCTYPE) == tIOC) ||
#ifndef FORNOW
		    ((cmd & IOCTYPE) == TIOC))
#else
		    ((cmd & IOCTYPE) == TIOC) ||
		    ((cmd & IOCTYPE) == KIOC) ||
		    ((cmd & IOCTYPE) == MSIOC) ||
		    ((cmd & IOCTYPE) == VUIOC))
#endif
		{

			/*
			 * The ioctl is a tty ioctl - set up strioc buffer
			 * and call strdoioctl() to do the work.
			 */
			if (stp->sd_flag & STRHUP)
				return (ENXIO);
			strioc.ic_cmd = cmd;
			strioc.ic_timout = INFTIM;

			switch (cmd) {
#ifdef FORNOW
			case TIOCLBIS:  /* ttcompat */
			case TIOCLBIC:  /* ttcompat */
			case TIOCLSET:  /* ttcompat */
				strioc.ic_len = TRANSPARENT;
				strioc.ic_dp = (char *)&arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, STRINT, crp, rvalp));
#endif

			case TCXONC:
			case TCSBRK:
			case TCFLSH:
			case TCDSET:
				{
				int native_arg = (int)arg;
				strioc.ic_len = sizeof (int);
				strioc.ic_dp = (char *)&native_arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
				    K_TO_K, STRINT, crp, rvalp));
				}

			case TCSETA:
			case TCSETAW:
			case TCSETAF:
				strioc.ic_len = sizeof (struct termio);
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, STRTERMIO, crp, rvalp));

			case TCSETS:
			case TCSETSW:
			case TCSETSF:
				strioc.ic_len = sizeof (struct termios);
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, STRTERMIOS, crp, rvalp));

			case LDSETT:
				strioc.ic_len = sizeof (struct termcb);
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, STRTERMCB, crp, rvalp));

			case TIOCSETP:
				strioc.ic_len = sizeof (struct sgttyb);
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, STRSGTTYB, crp, rvalp));

			case TIOCSTI:
				if (crp->cr_uid) {
					if ((flag & FREAD) == 0)
						return (EPERM);
					if (stp->sd_sidp !=
					    ttoproc(curthread)->p_sessp->s_sidp)
						return (EACCES);
				}
				strioc.ic_len = sizeof (char);
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, "c", crp, rvalp));
#ifdef FORNOW
			case TIOCLGET:  /* ttcompat */
				strioc.ic_len = TRANSPARENT;
				strioc.ic_dp = (char *)&arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, STRINT, crp, rvalp));
#endif


			case TCGETA:
				fmt = STRTERMIO;
				/* FALLTHRU */
			case TCGETS:
				if (fmt == NULL)
					fmt = STRTERMIOS;
				/* FALLTHRU */
			case LDGETT:
				if (fmt == NULL)
					fmt = STRTERMCB;
				/* FALLTHRU */
			case TIOCGETP:
				if (fmt == NULL)
					fmt = STRSGTTYB;
				strioc.ic_len = 0;
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, fmt, crp, rvalp));

#ifdef FORNOW
			/*
			 * XXX - The kbd and ms ioctls have been
			 * hard-coded in the stream head.  This should
			 * be fixed later on.
			 */
			case TIOCSWINSZ:
				strioc.ic_len = sizeof (struct winsize);
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, (char *)NULL, crp, rvalp));

			case TIOCSSIZE:
				strioc.ic_len = sizeof (struct ttysize);
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, (char *)NULL, crp, rvalp));

			case TIOCSSOFTCAR:
			case KIOCTRANS:
			case KIOCTRANSABLE:
			case KIOCCMD:
			case KIOCSDIRECT:
			case KIOCSCOMPAT:
			case KIOCSKABORTEN:
			case VUIDSFORMAT:
			case TIOCSPPS:
				strioc.ic_len = sizeof (int);
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, (char *)NULL, crp, rvalp));

			case KIOCSETKEY:
			case KIOCGETKEY:
				strioc.ic_len = sizeof (struct kiockey);
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, (char *)NULL, crp, rvalp));

			case KIOCSKEY:
			case KIOCGKEY:
				strioc.ic_len = sizeof (struct kiockeymap);
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, (char *)NULL, crp, rvalp));

			case KIOCSLED:
				/* arg is a pinter to char */
				strioc.ic_len = sizeof (char);
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, (char *)NULL, crp, rvalp));

			case MSIOSETPARMS:
				strioc.ic_len = sizeof (Ms_parms);
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, (char *)NULL, crp, rvalp));

			case VUIDSADDR:
			case VUIDGADDR:
				strioc.ic_len = sizeof (struct vuid_addr_probe);
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, (char *)NULL, crp, rvalp));

			case TIOCGSOFTCAR:
			case TIOCGWINSZ:
			case TIOCGSIZE:
			case KIOCGTRANS:
			case KIOCGTRANSABLE:
			case KIOCTYPE:
			case KIOCGDIRECT:
			case KIOCGCOMPAT:
			case KIOCLAYOUT:
			case KIOCGLED:
			case MSIOGETPARMS:
			case MSIOBUTTONS:
			case VUIDGFORMAT:
			case TIOCGPPS:
			case TIOCGPPSEV:
				strioc.ic_len = 0;
				strioc.ic_dp = (char *)arg;
				return (strdoioctl(stp, &strioc, NULL, flag,
					copyflag, (char *)NULL, crp, rvalp));
#endif
			}
		}

		/*
		 * Unknown cmd - send down request to support
		 * transparent ioctls.
		 */

		strioc.ic_cmd = cmd;
		strioc.ic_timout = INFTIM;
		strioc.ic_len = TRANSPARENT;
		strioc.ic_dp = (char *)&arg;

		return (strdoioctl(stp, &strioc, NULL, flag, copyflag,
		    (char *)NULL, crp, rvalp));

	/*
	 * These are flow trace ioctl's, might as well handle them here
	 *
	 * I_STRFTON:	Turns on Streams Flow Trace (or more accurately
	 *		dissables non-tracing)
	 * I_STRFTOFF:	Opposite of above/
	 * I_STRFTSTR:	Dissables tracing of all streams, and enables
	 *		tracing of this stream.
	 * I_STRFTALL:	Enables tracing of all streams
	 * I_STRFTZERO: Zero's the trace buffers.
	 *
	 * TBD: Ability to start and stop tracing on a single stream.
	 *	Ability to define a trace buffer (probably done with
	 *	   an extension called /dev/strft)
	 *	Define any I_STRFT arguments (currently only the above).
	 *	Possible extension to have a single command that calls
	 *	   a strft function for these features (and remove the
	 *	   complexity in strioctl()).
	 */
	case I_STRFT:
		/*
		 * Generic case.  Put here so STRFT can be more easily
		 * extended.  The arg is an integer specifying the
		 * task intended.  Included are the standard I_STRFT*
		 * ioctl cmds.
		 */
		if ((error = drv_priv(ddi_get_cred())) != 0)
			return (error);

		return (-1);
	case I_STRFTON:
		if ((error = drv_priv(ddi_get_cred())) != 0)
			return (error);

		str_ftnever = 0;
		return (0);

	case I_STRFTOFF:
		if ((error = drv_priv(ddi_get_cred())) != 0)
			return (error);

		str_ftnever = 1;
		return (0);

	case I_STRFTSTR:
		if ((error = drv_priv(ddi_get_cred())) != 0)
			return (error);

		stp->sd_ftflags &= STRFT_TRACE;
		str_ftall = 0;
		return (0);

	case I_STRFTALL:
		if ((error = drv_priv(ddi_get_cred())) != 0)
			return (error);

		str_ftall = 1;
		return (0);

	case I_STRFTZERO:
		/*
		 * Not yet implemented, but needed to be here.
		 */
		if ((error = drv_priv(ddi_get_cred())) != 0)
			return (error);

		return (-1);

	case I_STR:
		/*
		 * Stream ioctl.  Read in an strioctl buffer from the user
		 * along with any data specified and send it downstream.
		 * Strdoioctl will wait allow only one ioctl message at
		 * a time, and waits for the acknowledgement.
		 */

		if (stp->sd_flag & STRHUP)
			return (ENXIO);

		error = strcopyin_strioctl((void *)arg, &strioc, flag,
		    copyflag);

		if (error)
			return (error);
		if ((strioc.ic_len < 0) || (strioc.ic_timout < -1))
			return (EINVAL);
		error = strdoioctl(stp, &strioc, NULL, flag, copyflag,
		    (char *)NULL, crp, rvalp);
		if (error == 0) {
			error = strcopyout_strioctl(&strioc, (caddr_t)arg,
			    flag, copyflag);
		}
		return (error);

	case I_NREAD:
		/*
		 * Return number of bytes of data in first message
		 * in queue in "arg" and return the number of messages
		 * in queue in return value.
		 */
	    {
		size_t  size = 0;
		int	retval;
		int	count = 0;

		mutex_enter(QLOCK(rdq));
		for (mp = rdq->q_first; mp != NULL; mp = mp->b_next) {
			if (!(mp->b_flag & MSGNOGET))
				break;
		}
		if (mp) {
			size = msgdsize(mp);
			for (; mp != NULL; mp = mp->b_next)
				if (!(mp->b_flag & MSGNOGET))
					count++;
		}
		mutex_exit(QLOCK(rdq));
		if (stp->sd_struiordq) {
			infod_t infod;

			infod.d_cmd = INFOD_COUNT;
			infod.d_count = 0;
			if (count == 0) {
				infod.d_cmd |= INFOD_FIRSTBYTES;
				infod.d_bytes = 0;
			}
			infod.d_res = 0;
			(void) infonext(rdq, &infod);
			count += infod.d_count;
			if (infod.d_res & INFOD_FIRSTBYTES)
				size = infod.d_bytes;
		}

		/*
		 * Drop down from size_t to the "int" required by the
		 * interface.  Cap at INT_MAX.
		 */
		retval = MIN(size, INT_MAX);
		error = strcopyout(&retval, (caddr_t)arg, sizeof (retval),
		    STRINT, copyflag);
		if (!error)
			*rvalp = count;
		return (error);
	    }

	case FIONREAD:
		/*
		 * Return number of bytes of data in all data messages
		 * in queue in "arg".
		 */
	    {
		size_t	size = 0;
		int	retval;

		mutex_enter(QLOCK(rdq));
		if ((mp = rdq->q_first) != NULL) {
			while (mp) {
				if ((mp->b_flag & MSGNOGET) == 0)
					size += msgdsize(mp);
				mp = mp->b_next;
			}
		}
		mutex_exit(QLOCK(rdq));
		if (stp->sd_struiordq) {
			infod_t infod;

			infod.d_cmd = INFOD_BYTES;
			infod.d_res = 0;
			infod.d_bytes = 0;
			(void) infonext(rdq, &infod);
			size += infod.d_bytes;
		}

		/*
		 * Drop down from size_t to the "int" required by the
		 * interface.  Cap at INT_MAX.
		 */
		retval = MIN(size, INT_MAX);
		error = strcopyout(&retval, (caddr_t)arg, sizeof (retval),
		    STRINT, copyflag);

		*rvalp = 0;
		return (error);
	    }
	case FIORDCHK:
		/*
		 * FIORDCHK does not use arg value (like FIONREAD),
		 * instead a count is returned. I_NREAD value may
		 * not be accurate but safe. The real thing to do is
		 * to add the msgdsizes of all data  messages until
		 * a non-data message.
		 */
	    {
		size_t size = 0;

		mutex_enter(QLOCK(rdq));
		if ((mp = rdq->q_first) != NULL) {
			while (mp) {
				if ((mp->b_flag & MSGNOGET) == 0)
					size += msgdsize(mp);
				mp = mp->b_next;
			}
		}
		mutex_exit(QLOCK(rdq));
		if (stp->sd_struiordq) {
			infod_t infod;

			infod.d_cmd = INFOD_BYTES;
			infod.d_res = 0;
			infod.d_bytes = 0;
			(void) infonext(rdq, &infod);
			size += infod.d_bytes;
		}

		/*
		 * Since ioctl returns an int, and memory sizes under
		 * LP64 may not fit, we return INT_MAX if the count was
		 * actually greater.
		 */
		*rvalp = MIN(size, INT_MAX);
		return (0);
	    }

	case I_FIND:
		/*
		 * Get module name.
		 */
	    {
		char mname[FMNAMESZ + 1];
		queue_t *q;
		int i;

		error = (copyflag & U_TO_K ? copyinstr : copystr)((void *)arg,
		    mname, FMNAMESZ + 1, NULL);
		if (error)
			return ((error == ENAMETOOLONG) ? EINVAL : EFAULT);

		/*
		 * Return EINVAL if we're handed a bogus module name.
		 */
		if ((i = findmod(mname)) < 0) {
			TRACE_0(TR_FAC_STREAMS_FR,
				TR_I_CANT_FIND, "couldn't I_FIND");
			return (EINVAL);
		}
		(void) fmod_release(i);

		*rvalp = 0;

		/* Look downstream to see if module is there. */
		claimstr(stp->sd_wrq);
		for (q = stp->sd_wrq->q_next; q; q = q->q_next) {
			if (q->q_flag&QREADR) {
				q = NULL;
				break;
			}
			if (strcmp(mname, q->q_qinfo->qi_minfo->mi_idname) == 0)
				break;
		}
		releasestr(stp->sd_wrq);

		*rvalp = (q ? 1 : 0);
		return (error);
	    }

	case I_PUSH:
	case __I_PUSH_NOCTTY:
		/*
		 * Push a module.
		 * For the case __I_PUSH_NOCTTY push a module but
		 * do not allocate controlling tty. See bugid 4025044
		 */

	    {
		char mname[FMNAMESZ + 1];
		int i;
		dev_t dummydev;

		if (stp->sd_flag & STRHUP)
			return (ENXIO);
		mutex_enter(&stp->sd_lock);
		if (stp->sd_pushcnt >= nstrpush) {
			mutex_exit(&stp->sd_lock);
			return (EINVAL);
		}
		stp->sd_pushcnt++;
		mutex_exit(&stp->sd_lock);

		/*
		 * Get module name and look up in fmodsw.
		 */
		error = (copyflag & U_TO_K ? copyinstr : copystr)((void *)arg,
		    mname, FMNAMESZ + 1, NULL);
		if (error) {
			error = (error == ENAMETOOLONG) ? EINVAL : EFAULT;
			goto push_failed;
		}

		/*
		 * Note: findmod() holds the module which gets released
		 * later during the pop.
		 */

		if ((i = findmod(mname)) < 0) {
			error = EINVAL;
			goto push_failed;
		}

		TRACE_2(TR_FAC_STREAMS_FR,
			TR_I_PUSH,
			"I_PUSH:mod %d to %X", i, stp);

		if (error = strsyncplumb(stp, flag, cmd)) {
			(void) fmod_release(i);
			goto push_failed;
		}
		/*
		 * Push new module and call its open routine
		 * via qattach().  Modules don't change device
		 * numbers, so just ignore dummydev here.
		 */
		dummydev = vp->v_rdev;
		if ((error = qattach(rdq, &dummydev, 0, 0,
		    FMODSW, i, crp, B_FALSE)) == 0) {
			if (vp->v_type == VCHR && /* sorry, no pipes allowed */
			    (cmd == I_PUSH) && (stp->sd_flag & STRISTTY)) {
				/*
				 * try to allocate it as a controlling terminal
				 */
				strctty(stp);
			}
		} else {
			(void) fmod_release(i);
			mutex_enter(&stp->sd_lock);
			stp->sd_pushcnt--;
			mutex_exit(&stp->sd_lock);
		}
		/*
		 * If flow control is on, don't break it - enable
		 * first back queue with svc procedure.
		 */
		if (rdq->q_flag & QWANTW) {
			/* Note: no setqback here - use pri -1. */
			backenable(_RD(wrq->q_next), -1);
		}

		mutex_enter(&stp->sd_lock);
		stp->sd_flag &= ~STRPLUMB;

		/*
		 * As a performance concern we are caching the values of
		 * q_minpsz and q_maxpsz of the module below the stream
		 * head in the stream head.
		 */
		mutex_enter(QLOCK(stp->sd_wrq->q_next));
		rmin = stp->sd_wrq->q_next->q_minpsz;
		rmax = stp->sd_wrq->q_next->q_maxpsz;
		mutex_exit(QLOCK(stp->sd_wrq->q_next));

		/* Do this processing here as a performance concern */
		if (strmsgsz != 0) {
			if (rmax == INFPSZ)
				rmax = strmsgsz;
			else  {
				if (vp->v_type == VFIFO)
					rmax = MIN(PIPE_BUF, rmax);
				else	rmax = MIN(strmsgsz, rmax);
			}
		}

		mutex_enter(QLOCK(wrq));
		stp->sd_qn_minpsz = rmin;
		stp->sd_qn_maxpsz = rmax;
		mutex_exit(QLOCK(wrq));

		cv_broadcast(&stp->sd_monitor);
		mutex_exit(&stp->sd_lock);
		return (error);

		push_failed:
		mutex_enter(&stp->sd_lock);
		stp->sd_pushcnt--;
		mutex_exit(&stp->sd_lock);
		return (error);
	    }

	case I_POP:
	    {
		queue_t	*q;
		queue_t	*rmq;
		int	error;
		int	status;
		int	mid;

		if (stp->sd_flag & STRHUP)
			return (ENXIO);
		if (!wrq->q_next)	/* for broken pipes */
			return (EINVAL);

		/*
		 * If there is an anchor on this stream and popping
		 * the current module would attempt to pop through the
		 * anchor, then disallow the pop unless root.  Note
		 * that we take the caller's credentials from
		 * ddi_get_cred() since the credentials passed into
		 * strioctl() are those of the thread that did the
		 * VOP_OPEN().  See bugid 1238582 and PSARC/1998/528.
		 */
		if (drv_priv(ddi_get_cred()) == EPERM) {

			mutex_enter(&stp->sd_lock);
			if (stp->sd_anchor == stp->sd_pushcnt &&
			    stp->sd_vnode->v_type != VFIFO) {
				mutex_exit(&stp->sd_lock);
				return (EPERM);
			}
			mutex_exit(&stp->sd_lock);
		}

		if (status = strsyncplumb(stp, flag, cmd))
			return (status);

		q = wrq->q_next;
		TRACE_2(TR_FAC_STREAMS_FR,
			TR_I_POP,
			"I_POP:%X from %X", q, stp);
		if (q->q_next && !(q->q_flag & QREADR)) {
			rmq = wrq->q_next;
			if (!(rmq->q_flag & QISDRV)) {
				mutex_enter(&fmodsw_lock);
				mid = findmodbyname(
					rmq->q_qinfo->qi_minfo->mi_idname);
				mutex_exit(&fmodsw_lock);
				qdetach(_RD(rmq), 1, flag, crp, B_FALSE);
				if (mid != -1) {
					(void) fmod_release(mid);
				}
			} else {
				ASSERT(!_SAMESTR(rmq));
				qdetach(_RD(rmq), 1, flag, crp, B_FALSE);
			}
			mutex_enter(&stp->sd_lock);
			cv_broadcast(&stp->sd_monitor);
			stp->sd_pushcnt--;
			mutex_exit(&stp->sd_lock);
			error = 0;
		} else
			error = EINVAL;
		mutex_enter(&stp->sd_lock);
		stp->sd_flag &= ~STRPLUMB;

		/*
		 * As a performance concern we are caching the values of
		 * q_minpsz and q_maxpsz of the module below the stream
		 * head in the stream head.
		 */
		mutex_enter(QLOCK(wrq->q_next));
		rmin = wrq->q_next->q_minpsz;
		rmax = wrq->q_next->q_maxpsz;
		mutex_exit(QLOCK(wrq->q_next));

		/* Do this processing here as a performance concern */
		if (strmsgsz != 0) {
			if (rmax == INFPSZ)
				rmax = strmsgsz;
			else  {
				if (vp->v_type == VFIFO)
					rmax = MIN(PIPE_BUF, rmax);
				else	rmax = MIN(strmsgsz, rmax);
			}
		}

		mutex_enter(QLOCK(wrq));
		stp->sd_qn_minpsz = rmin;
		stp->sd_qn_maxpsz = rmax;
		mutex_exit(QLOCK(wrq));

		cv_broadcast(&stp->sd_monitor);

		/* If we popped through the anchor, then reset the anchor. */
		if (stp->sd_pushcnt < stp->sd_anchor)
			stp->sd_anchor = 0;

		mutex_exit(&stp->sd_lock);
		return (error);
	    }

	case _I_MUXID2FD:
	{
		/*
		 * Create a fd for a I_PLINK'ed lower stream with a given
		 * muxid.  With the fd, application can send down ioctls,
		 * like I_LIST, to the previously I_PLINK'ed stream.  Note
		 * that after getting the fd, the application has to do an
		 * I_PUNLINK on the muxid before it can do any operation
		 * on the lower stream.  This is required by spec1170.
		 *
		 * The fd used to do this ioctl should point to the same
		 * controlling device used to do the I_PLINK.  If it uses
		 * a different stream or an invalid muxid, I_MUXID2FD will
		 * fail.  The error code is set to EINVAL.
		 *
		 * The intended use of this interface is the following.
		 * An application I_PLINK'ed a stream and exits.  The fd
		 * to the lower stream is gone.  Another application
		 * wants to get a fd to the lower stream, it uses I_MUXID2FD.
		 */
		int muxid = (int)arg;
		int fd;
		linkinfo_t *linkp;
		struct file *fp;

		/*
		 * Do not allow the wildcard muxid.  This ioctl is not
		 * intended to find arbitrary link.
		 */
		if (muxid == 0) {
			return (EINVAL);
		}

		mutex_enter(&muxifier);
		linkp = findlinks(vp->v_stream, muxid, LINKIOCTL|LINKPERSIST);
		if (linkp == NULL) {
			mutex_exit(&muxifier);
			return (EINVAL);
		}

		if ((fd = ufalloc(0)) == -1) {
			mutex_exit(&muxifier);
			return (EMFILE);
		}
		fp = linkp->li_fpdown;
		mutex_enter(&fp->f_tlock);
		fp->f_count++;
		mutex_exit(&fp->f_tlock);
		mutex_exit(&muxifier);
		setf(fd, fp);
		*rvalp = fd;
		return (0);
	}

	case _I_INSERT:
	{
		/*
		 * To insert a module to a given position in a stream.
		 * In the first release, only allow privileged user
		 * to use this ioctl.
		 *
		 * Note that we do not plan to support this ioctl
		 * on pipes in the first release.  We want to learn more
		 * about the implications of these ioctls before extending
		 * their support.  And we do not think these features are
		 * valuable for pipes.
		 *
		 * Neither do we support QNEXTLESS stream.  Note that only
		 * the upper streams of TCP/IP stack are QNEXTLESS streams.
		 * The lower IP stream is not.
		 */
		STRUCT_DECL(strmodconf, strmodinsert);
		char mod_name[FMNAMESZ + 1];
		int mod_index;
		dev_t dummydev;
		queue_t *tmp_wrq;
		int pos;
		boolean_t is_insert;

		STRUCT_INIT(strmodinsert, flag);
		if (stp->sd_flag & STRHUP)
			return (ENXIO);
		if (STRMATED(stp) || (stp->sd_flag & STRQNEXTLESS))
			return (EINVAL);
		if ((error = drv_priv(ddi_get_cred())) != 0)
			return (error);

		error = strcopyin((caddr_t)arg, STRUCT_BUF(strmodinsert),
		    STRUCT_SIZE(strmodinsert), STRMODCONF, copyflag);
		if (error)
			return (error);

		/*
		 * Is this _I_INSERT just like an I_PUSH?  We need to know
		 * this because we do some optimizations if this is a
		 * module being pushed.
		 */
		pos = STRUCT_FGET(strmodinsert, pos);
		is_insert = (pos != 0);

		/*
		 * Make sure pos is valid.  Even though it is not an I_PUSH,
		 * we impose the same limit on the number of modules in a
		 * stream.
		 */
		mutex_enter(&stp->sd_lock);
		if (stp->sd_pushcnt >= nstrpush || pos < 0 ||
		    pos > stp->sd_pushcnt) {
			mutex_exit(&stp->sd_lock);
			return (EINVAL);
		}
		mutex_exit(&stp->sd_lock);

		/*
		 * Get module name and look up in fmodsw.
		 */
		error = (copyflag & U_TO_K ? copyinstr :
		    copystr)(STRUCT_FGETP(strmodinsert, mod_name),
		    mod_name, FMNAMESZ + 1, NULL);
		if (error)
			return ((error == ENAMETOOLONG) ? EINVAL : EFAULT);

		/*
		 * Note: findmod() holds the module which gets released
		 * later during the pop.
		 */
		if ((mod_index = findmod(mod_name)) < 0) {
			return (EINVAL);
		}

		if (error = strsyncplumb(stp, flag, cmd)) {
			(void) fmod_release(mod_index);
			return (error);
		}

		/*
		 * First find the correct position this module to
		 * be inserted.  We don't need to call claimstr()
		 * as the stream should not be changing at this point.
		 *
		 * Insert new module and call its open routine
		 * via qattach().  Modules don't change device
		 * numbers, so just ignore dummydev here.
		 */
		for (tmp_wrq = stp->sd_wrq; pos > 0;
		    tmp_wrq = tmp_wrq->q_next, pos--) {
			ASSERT(SAMESTR(tmp_wrq));
		}
		dummydev = vp->v_rdev;
		if ((error = qattach(_RD(tmp_wrq), &dummydev, 0, 0,
		    FMODSW, mod_index, crp, is_insert)) != 0) {
			goto insert_failed;
		}
		/*
		 * If flow control is on, don't break it - enable
		 * first back queue with svc procedure.
		 */
		if (_RD(tmp_wrq)->q_nfsrv->q_flag & QWANTW) {
			/*
			 * Note: no setqback here - use pri -1.
			 * tmp_wrq->q_next is the new module.  We need
			 * to backenable() the module below the new module.
			 */
			backenable(_RD(tmp_wrq->q_next->q_next), -1);
		}

		mutex_enter(&stp->sd_lock);
		stp->sd_flag &= ~STRPLUMB;

		/*
		 * As a performance concern we are caching the values of
		 * q_minpsz and q_maxpsz of the module below the stream
		 * head in the stream head.
		 */
		if (!is_insert) {
			mutex_enter(QLOCK(stp->sd_wrq->q_next));
			rmin = stp->sd_wrq->q_next->q_minpsz;
			rmax = stp->sd_wrq->q_next->q_maxpsz;
			mutex_exit(QLOCK(stp->sd_wrq->q_next));

			/* Do this processing here as a performance concern */
			if (strmsgsz != 0) {
				if (rmax == INFPSZ) {
					rmax = strmsgsz;
				} else  {
					rmax = MIN(strmsgsz, rmax);
				}
			}

			mutex_enter(QLOCK(wrq));
			stp->sd_qn_minpsz = rmin;
			stp->sd_qn_maxpsz = rmax;
			mutex_exit(QLOCK(wrq));
		}

		/*
		 * Need to update the anchor value if this module is
		 * inserted below the anchor point.
		 */
		if (stp->sd_anchor != 0) {
			if (stp->sd_pushcnt - stp->sd_anchor <
			    STRUCT_FGET(strmodinsert, pos))
				stp->sd_anchor++;
		}
		stp->sd_pushcnt++;
		cv_broadcast(&stp->sd_monitor);
		mutex_exit(&stp->sd_lock);
		return (error);

		insert_failed:
		(void) fmod_release(mod_index);
		mutex_enter(&stp->sd_lock);
		stp->sd_flag &= ~STRPLUMB;
		mutex_exit(&stp->sd_lock);
		return (error);
	}

	case _I_REMOVE:
	{
		/*
		 * To remove a module with a given name in a stream.  The
		 * caller of this ioctl needs to provide both the name and
		 * the position of the module to be removed.  This eliminates
		 * the ambiguity of removal if a module is inserted/pushed
		 * multiple times in a stream.  In the first release, only
		 * allow privileged user to use this ioctl.
		 *
		 * Note that we do not plan to support this ioctl
		 * on pipes in the first release.  We want to learn more
		 * about the implications of these ioctls before extending
		 * their support.  And we do not think these features are
		 * valuable for pipes.
		 *
		 * Neither do we support QNEXTLESS stream.  Note that only
		 * the upper streams of TCP/IP stack are QNEXTLESS streams.
		 * The lower IP stream is not.
		 *
		 * Also note that _I_REMOVE cannot be used to remove a
		 * driver or the stream head.
		 */
		STRUCT_DECL(strmodconf, strmodremove);
		queue_t	*q;
		int mod_index;
		int pos;
		char mod_name[FMNAMESZ + 1];
		boolean_t is_remove;

		STRUCT_INIT(strmodremove, flag);
		if (stp->sd_flag & STRHUP)
			return (ENXIO);
		if (STRMATED(stp) || (stp->sd_flag & STRQNEXTLESS))
			return (EINVAL);
		if ((error = drv_priv(ddi_get_cred())) != 0)
			return (error);

		error = strcopyin((caddr_t)arg, STRUCT_BUF(strmodremove),
		    STRUCT_SIZE(strmodremove), STRMODCONF, copyflag);
		if (error)
			return (error);

		error = (copyflag & U_TO_K ? copyinstr :
		    copystr)(STRUCT_FGETP(strmodremove, mod_name),
		    mod_name, FMNAMESZ + 1, NULL);
		if (error)
			return ((error == ENAMETOOLONG) ? EINVAL : EFAULT);

		if ((error = strsyncplumb(stp, flag, cmd)) != 0)
			return (error);

		/*
		 * Match the name of given module to the name of module at
		 * the given position.
		 */
		pos = STRUCT_FGET(strmodremove, pos);
		is_remove = (pos != 0);
		for (q = stp->sd_wrq->q_next; SAMESTR(q) && pos > 0;
		    q = q->q_next, pos--)
			;
		if (pos > 0 || ! SAMESTR(q) ||
		    strncmp(q->q_qinfo->qi_minfo->mi_idname, mod_name,
		    strlen(q->q_qinfo->qi_minfo->mi_idname)) != 0) {
			mutex_enter(&stp->sd_lock);
			stp->sd_flag &= ~STRPLUMB;
			mutex_exit(&stp->sd_lock);
			return (EINVAL);
		}

		ASSERT(! (q->q_flag & QREADR));
		mutex_enter(&fmodsw_lock);
		mod_index = findmodbyname(mod_name);
		mutex_exit(&fmodsw_lock);
		qdetach(_RD(q), 1, flag, crp, is_remove);
		if (mod_index != -1) {
			(void) fmod_release(mod_index);
		}

		mutex_enter(&stp->sd_lock);
		stp->sd_flag &= ~STRPLUMB;

		/*
		 * As a performance concern we are caching the values of
		 * q_minpsz and q_maxpsz of the module below the stream
		 * head in the stream head.
		 */
		if (!is_remove) {
			mutex_enter(QLOCK(wrq->q_next));
			rmin = wrq->q_next->q_minpsz;
			rmax = wrq->q_next->q_maxpsz;
			mutex_exit(QLOCK(wrq->q_next));

			/* Do this processing here as a performance concern */
			if (strmsgsz != 0) {
				if (rmax == INFPSZ)
					rmax = strmsgsz;
				else  {
					if (vp->v_type == VFIFO)
						rmax = MIN(PIPE_BUF, rmax);
					else	rmax = MIN(strmsgsz, rmax);
				}
			}

			mutex_enter(QLOCK(wrq));
			stp->sd_qn_minpsz = rmin;
			stp->sd_qn_maxpsz = rmax;
			mutex_exit(QLOCK(wrq));
		}

		/*
		 * Need to update the anchor value if this module is
		 * removed at or below the anchor point.  If the removed
		 * module is at the anchor point, remove the anchor for this
		 * stream if there is no module above the anchor point.
		 * Otherwise, decrement the anchor point by 1.  If the module
		 * is below the anchor point, decrement the anchor point by
		 * 1.
		 */
		if (stp->sd_anchor != 0) {
			int num_mods = stp->sd_pushcnt - stp->sd_anchor;

			ASSERT(num_mods >= 0);
			if (pos == num_mods) {
				if (num_mods == 0)
					stp->sd_anchor = 0;
				else
					stp->sd_anchor--;
			} else if (pos > num_mods)
				stp->sd_anchor--;
		}

		stp->sd_pushcnt--;
		cv_broadcast(&stp->sd_monitor);
		mutex_exit(&stp->sd_lock);
		return (error);
	}

	case I_ANCHOR:
		/*
		 * Set the anchor position on the stream to reside at
		 * the top module (in other words, the top module
		 * cannot be popped).  Anchors with a FIFO make no
		 * obvious sense, so they're not allowed.
		 */
		mutex_enter(&stp->sd_lock);

		if (stp->sd_vnode->v_type == VFIFO) {
			mutex_exit(&stp->sd_lock);
			return (EINVAL);
		}

		stp->sd_anchor = stp->sd_pushcnt;

		mutex_exit(&stp->sd_lock);
		return (0);

	case I_LOOK:
		/*
		 * Get name of first module downstream.
		 * If no module, return an error.
		 */
	    {
		claimstr(wrq);
		if (_SAMESTR(wrq) && wrq->q_next->q_next) {
			char *name = wrq->q_next->q_qinfo->qi_minfo->mi_idname;
			error = strcopyout(name, (char *)arg,
			    strlen(name) + 1, STRNAME, copyflag);
			releasestr(wrq);
			return (error);
		}
		releasestr(wrq);
		return (EINVAL);
	    }

	case I_LINK:
	case I_PLINK:
		/*
		 * Link a multiplexor.
		 */
		return (mlink(vp, cmd, (int)arg, crp, rvalp));

	case I_UNLINK:
	case I_PUNLINK:
		/*
		 * Unlink a multiplexor.
		 * If arg is -1, unlink all links for which this is the
		 * controlling stream.  Otherwise, arg is an index number
		 * for a link to be removed.
		 */
	    {
		struct linkinfo *linkp;
		int native_arg = (int)arg;
		int type;

		TRACE_1(TR_FAC_STREAMS_FR,
			TR_I_UNLINK, "I_UNLINK/I_PUNLINK:%X", stp);
		if (vp->v_type == VFIFO) {
			return (EINVAL);
		}
		if (cmd == I_UNLINK)
			type = LINKIOCTL|LINKNORMAL;
		else	/* I_PUNLINK */
			type = LINKIOCTL|LINKPERSIST;
		if (native_arg == 0) {
			return (EINVAL);
		}
		if (native_arg == MUXID_ALL)
			error = munlinkall(stp, type, crp, rvalp);
		else {
			mutex_enter(&muxifier);
			if (!(linkp = findlinks(stp, (int)arg, type))) {
				/* invalid user supplied index number */
				mutex_exit(&muxifier);
				return (EINVAL);
			}
			/* munlink drops the muxifier lock */
			error = munlink(stp, linkp, type, crp, rvalp);
		}
		return (error);
	    }

	case I_FLUSH:
		/*
		 * send a flush message downstream
		 * flush message can indicate
		 * FLUSHR - flush read queue
		 * FLUSHW - flush write queue
		 * FLUSHRW - flush read/write queue
		 */
		if (stp->sd_flag & STRHUP)
			return (ENXIO);
		if (arg & ~FLUSHRW)
			return (EINVAL);
		/*CONSTCOND*/
		while (1) {
			if (putnextctl1(stp->sd_wrq, M_FLUSH, (int)arg)) {
				break;
			}
			if (error = strwaitbuf(1, BPRI_HI)) {
				return (error);
			}
		}

		/*
		 * Send down an unsupported ioctl and wait for the nack
		 * in order to allow the M_FLUSH to propagate back
		 * up to the stream head.
		 * Replaces if (qready()) runqueues();
		 */
		strioc.ic_cmd = -1;	/* The unsupported ioctl */
		strioc.ic_timout = 0;
		strioc.ic_len = 0;
		strioc.ic_dp = (char *)NULL;
		(void) strdoioctl(stp, &strioc, NULL, flag, K_TO_K,
				"dummy", crp, rvalp);
		*rvalp = 0;
		return (0);

	case I_FLUSHBAND:
	    {
		struct bandinfo binfo;

		error = strcopyin((caddr_t)arg, (caddr_t)&binfo,
		    sizeof (struct bandinfo), STRBANDINFO, copyflag);
		if (error)
			return (error);
		if (stp->sd_flag & STRHUP)
			return (ENXIO);
		if (binfo.bi_flag & ~FLUSHRW)
			return (EINVAL);
		while (!(mp = allocb(2, BPRI_HI))) {
			if (error = strwaitbuf(2, BPRI_HI))
				return (error);
		}
		mp->b_datap->db_type = M_FLUSH;
		*mp->b_wptr++ = binfo.bi_flag | FLUSHBAND;
		*mp->b_wptr++ = binfo.bi_pri;
		putnext(stp->sd_wrq, mp);
		/*
		 * Send down an unsupported ioctl and wait for the nack
		 * in order to allow the M_FLUSH to propagate back
		 * up to the stream head.
		 * Replaces if (qready()) runqueues();
		 */
		strioc.ic_cmd = -1;	/* The unsupported ioctl */
		strioc.ic_timout = 0;
		strioc.ic_len = 0;
		strioc.ic_dp = (char *)NULL;
		(void) strdoioctl(stp, &strioc, NULL, flag, K_TO_K,
				"dummy", crp, rvalp);
		*rvalp = 0;
		return (0);
	    }

	case I_SRDOPT:
		/*
		 * Set read options
		 *
		 * RNORM - default stream mode
		 * RMSGN - message no discard
		 * RMSGD - message discard
		 * RPROTNORM - fail read with EBADMSG for M_[PC]PROTOs
		 * RPROTDAT - convert M_[PC]PROTOs to M_DATAs
		 * RPROTDIS - discard M_[PC]PROTOs and retain M_DATAs
		 */
		if (arg & ~(RMODEMASK | RPROTMASK))
			return (EINVAL);

		if ((arg & (RMSGD|RMSGN)) == (RMSGD|RMSGN))
			return (EINVAL);

		mutex_enter(&stp->sd_lock);
		switch (arg & RMODEMASK) {
		case RNORM:
			stp->sd_read_opt &= ~(RD_MSGDIS | RD_MSGNODIS);
			break;
		case RMSGD:
			stp->sd_read_opt = (stp->sd_read_opt & ~RD_MSGNODIS) |
			    RD_MSGDIS;
			break;
		case RMSGN:
			stp->sd_read_opt = (stp->sd_read_opt & ~RD_MSGDIS) |
			    RD_MSGNODIS;
			break;
		}

		switch (arg & RPROTMASK) {
		case RPROTNORM:
			stp->sd_read_opt &= ~(RD_PROTDAT | RD_PROTDIS);
			break;

		case RPROTDAT:
			stp->sd_read_opt = ((stp->sd_read_opt & ~RD_PROTDIS) |
			    RD_PROTDAT);
			break;

		case RPROTDIS:
			stp->sd_read_opt = ((stp->sd_read_opt & ~RD_PROTDAT) |
			    RD_PROTDIS);
			break;
		}
		mutex_exit(&stp->sd_lock);
		return (0);

	case I_GRDOPT:
		/*
		 * Get read option and return the value
		 * to spot pointed to by arg
		 */
	    {
		int rdopt;

		rdopt = ((stp->sd_read_opt & RD_MSGDIS) ? RMSGD :
		    ((stp->sd_read_opt & RD_MSGNODIS) ? RMSGN : RNORM));
		rdopt |= ((stp->sd_read_opt & RD_PROTDAT) ? RPROTDAT :
		    ((stp->sd_read_opt & RD_PROTDIS) ? RPROTDIS : RPROTNORM));

		error = strcopyout((caddr_t)&rdopt, (caddr_t)arg,
				sizeof (rdopt), STRINT, copyflag);
		return (error);
	    }

	case I_SERROPT:
		/*
		 * Set error options
		 *
		 * RERRNORM - persistent read errors
		 * RERRNONPERSIST - non-persistent read errors
		 * WERRNORM - persistent write errors
		 * WERRNONPERSIST - non-persistent write errors
		 */
		if (arg & ~(RERRMASK | WERRMASK))
			return (EINVAL);

		mutex_enter(&stp->sd_lock);
		switch (arg & RERRMASK) {
		case RERRNORM:
			stp->sd_flag &= ~STRDERRNONPERSIST;
			break;
		case RERRNONPERSIST:
			stp->sd_flag |= STRDERRNONPERSIST;
			break;
		}
		switch (arg & WERRMASK) {
		case WERRNORM:
			stp->sd_flag &= ~STWRERRNONPERSIST;
			break;
		case WERRNONPERSIST:
			stp->sd_flag |= STWRERRNONPERSIST;
			break;
		}
		mutex_exit(&stp->sd_lock);
		return (0);

	case I_GERROPT:
		/*
		 * Get error option and return the value
		 * to spot pointed to by arg
		 */
	    {
		int erropt = 0;

		erropt |= (stp->sd_flag & STRDERRNONPERSIST) ? RERRNONPERSIST :
			RERRNORM;
		erropt |= (stp->sd_flag & STWRERRNONPERSIST) ? WERRNONPERSIST :
			WERRNORM;
		error = strcopyout((caddr_t)&erropt, (caddr_t)arg,
				sizeof (erropt), STRINT, copyflag);
		return (error);
	    }

	case I_SETSIG:
		/*
		 * Register the calling proc to receive the SIGPOLL
		 * signal based on the events given in arg.  If
		 * arg is zero, remove the proc from register list.
		 */
	    {
		strsig_t *ssp, *pssp;
		struct pid *pidp;

		pssp = NULL;
		pidp = curproc->p_pidp;
		/*
		 * Hold sd_lock to prevent traversal of sd_siglist while
		 * it is modified.
		 */
		mutex_enter(&stp->sd_lock);
		for (ssp = stp->sd_siglist; ssp && (ssp->ss_pidp != pidp);
			pssp = ssp, ssp = ssp->ss_next)
			;

		if (arg) {
			if (arg & ~(S_INPUT|S_HIPRI|S_MSG|S_HANGUP|S_ERROR|
			    S_RDNORM|S_WRNORM|S_RDBAND|S_WRBAND|S_BANDURG)) {
				mutex_exit(&stp->sd_lock);
				return (EINVAL);
			}
			if ((arg & S_BANDURG) && !(arg & S_RDBAND)) {
				mutex_exit(&stp->sd_lock);
				return (EINVAL);
			}

			/*
			 * If proc not already registered, add it
			 * to list.
			 */
			if (!ssp) {
				ssp = kmem_cache_alloc(strsig_cache, KM_SLEEP);
				ssp->ss_pidp = pidp;
				ssp->ss_pid = pidp->pid_id;
				ssp->ss_next = NULL;
				if (pssp)
					pssp->ss_next = ssp;
				else
					stp->sd_siglist = ssp;
				mutex_enter(&pidlock);
				PID_HOLD(pidp);
				mutex_exit(&pidlock);
			}

			/*
			 * Set events.
			 */
			ssp->ss_events = (int)arg;
		} else {
			/*
			 * Remove proc from register list.
			 */
			if (ssp) {
				mutex_enter(&pidlock);
				PID_RELE(pidp);
				mutex_exit(&pidlock);
				if (pssp)
					pssp->ss_next = ssp->ss_next;
				else
					stp->sd_siglist = ssp->ss_next;
				kmem_cache_free(strsig_cache, ssp);
			} else {
				mutex_exit(&stp->sd_lock);
				return (EINVAL);
			}
		}

		/*
		 * Recalculate OR of sig events.
		 */
		stp->sd_sigflags = 0;
		for (ssp = stp->sd_siglist; ssp; ssp = ssp->ss_next)
			stp->sd_sigflags |= ssp->ss_events;
		mutex_exit(&stp->sd_lock);
		return (0);
	    }

	case I_GETSIG:
		/*
		 * Return (in arg) the current registration of events
		 * for which the calling proc is to be signaled.
		 */
	    {
		struct strsig *ssp;
		struct pid  *pidp;

		pidp = curproc->p_pidp;
		mutex_enter(&stp->sd_lock);
		for (ssp = stp->sd_siglist; ssp; ssp = ssp->ss_next)
			if (ssp->ss_pidp == pidp) {
				error = strcopyout((caddr_t)&ssp->ss_events,
				    (caddr_t)arg, sizeof (int), STRINT,
				    copyflag);
				mutex_exit(&stp->sd_lock);
				return (error);
			}
		mutex_exit(&stp->sd_lock);
		return (EINVAL);
	    }

	case I_ESETSIG:
		/*
		 * Register the ss_pid to receive the SIGPOLL
		 * signal based on the events is ss_events arg.  If
		 * ss_events is zero, remove the proc from register list.
		 */
	{
		struct strsig *ssp, *pssp;
		struct proc *proc;
		struct pid  *pidp;
		pid_t pid;
		struct strsigset ss;

		error = strcopyin((caddr_t)arg, (caddr_t)&ss,
		    sizeof (struct strsigset), STRINT, copyflag);
		if (error)
			return (error);

		pid = ss.ss_pid;

		if (ss.ss_events != 0) {
			/*
			 * Permissions check by sending signal 0.
			 * Note that when kill fails it does a set_errno
			 * causing the system call to fail.
			 */
			error = kill(pid, 0);
			if (error) {
				return (error);
			}
		}
		mutex_enter(&pidlock);
		if (pid == 0)
			proc = curproc;
		else if (pid < 0)
			proc = pgfind(-pid);
		else
			proc = prfind(pid);
		if (proc == NULL) {
			mutex_exit(&pidlock);
			return (ESRCH);
		}
		if (pid < 0)
			pidp = proc->p_pgidp;
		else
			pidp = proc->p_pidp;
		ASSERT(pidp);
		/*
		 * Get a hold on the pid structure while referencing it.
		 * There is a separate PID_HOLD should it be inserted
		 * in the list below.
		 */
		PID_HOLD(pidp);
		mutex_exit(&pidlock);

		pssp = NULL;
		/*
		 * Hold sd_lock to prevent traversal of sd_siglist while
		 * it is modified.
		 */
		mutex_enter(&stp->sd_lock);
		for (ssp = stp->sd_siglist; ssp && (ssp->ss_pid != pid);
				pssp = ssp, ssp = ssp->ss_next)
			;

		if (ss.ss_events) {
			if (ss.ss_events &
			    ~(S_INPUT|S_HIPRI|S_MSG|S_HANGUP|S_ERROR|
			    S_RDNORM|S_WRNORM|S_RDBAND|S_WRBAND|S_BANDURG)) {
				mutex_exit(&stp->sd_lock);
				mutex_enter(&pidlock);
				PID_RELE(pidp);
				mutex_exit(&pidlock);
				return (EINVAL);
			}
			if ((ss.ss_events & S_BANDURG) &&
			    !(ss.ss_events & S_RDBAND)) {
				mutex_exit(&stp->sd_lock);
				mutex_enter(&pidlock);
				PID_RELE(pidp);
				mutex_exit(&pidlock);
				return (EINVAL);
			}

			/*
			 * If proc not already registered, add it
			 * to list.
			 */
			if (!ssp) {
				ssp = kmem_cache_alloc(strsig_cache, KM_SLEEP);
				ssp->ss_pidp = pidp;
				ssp->ss_pid = pid;
				ssp->ss_next = NULL;
				if (pssp)
					pssp->ss_next = ssp;
				else
					stp->sd_siglist = ssp;
				mutex_enter(&pidlock);
				PID_HOLD(pidp);
				mutex_exit(&pidlock);
			}

			/*
			 * Set events.
			 */
			ssp->ss_events = ss.ss_events;
		} else {
			/*
			 * Remove proc from register list.
			 */
			if (ssp) {
				mutex_enter(&pidlock);
				PID_RELE(pidp);
				mutex_exit(&pidlock);
				if (pssp)
					pssp->ss_next = ssp->ss_next;
				else
					stp->sd_siglist = ssp->ss_next;
				kmem_cache_free(strsig_cache, ssp);
			} else {
				mutex_exit(&stp->sd_lock);
				mutex_enter(&pidlock);
				PID_RELE(pidp);
				mutex_exit(&pidlock);
				return (EINVAL);
			}
		}

		/*
		 * Recalculate OR of sig events.
		 */
		stp->sd_sigflags = 0;
		for (ssp = stp->sd_siglist; ssp; ssp = ssp->ss_next)
			stp->sd_sigflags |= ssp->ss_events;
		mutex_exit(&stp->sd_lock);
		mutex_enter(&pidlock);
		PID_RELE(pidp);
		mutex_exit(&pidlock);
		return (0);
	    }

	case I_EGETSIG:
		/*
		 * Return (in arg) the current registration of events
		 * for which the calling proc is to be signaled.
		 */
	    {
		struct strsig *ssp;
		struct proc *proc;
		pid_t pid;
		struct pid  *pidp;
		struct strsigset ss;

		error = strcopyin((caddr_t)arg, (caddr_t)&ss,
		    sizeof (struct strsigset), STRINT, copyflag);
		if (error)
			return (error);

		pid = ss.ss_pid;
		mutex_enter(&pidlock);
		if (pid == 0)
			proc = curproc;
		else if (pid < 0)
			proc = pgfind(-pid);
		else
			proc = prfind(pid);
		if (proc == NULL) {
			mutex_exit(&pidlock);
			return (ESRCH);
		}
		if (pid < 0)
			pidp = proc->p_pgidp;
		else
			pidp = proc->p_pidp;

		/* Prevent the pidp from being reassigned */
		PID_HOLD(pidp);
		mutex_exit(&pidlock);

		mutex_enter(&stp->sd_lock);
		for (ssp = stp->sd_siglist; ssp; ssp = ssp->ss_next)
			if (ssp->ss_pid == pid) {
				ss.ss_pid = ssp->ss_pid;
				ss.ss_events = ssp->ss_events;
				error = strcopyout((caddr_t)&ss,
				    (caddr_t)arg, sizeof (struct strsigset),
				    STRINT, copyflag);
				mutex_exit(&stp->sd_lock);
				mutex_enter(&pidlock);
				PID_RELE(pidp);
				mutex_exit(&pidlock);
				return (error);
			}
		mutex_exit(&stp->sd_lock);
		mutex_enter(&pidlock);
		PID_RELE(pidp);
		mutex_exit(&pidlock);
		return (EINVAL);
	    }

	case I_PEEK:
	    {
		STRUCT_DECL(strpeek, strpeek);
		size_t n;
		mblk_t *fmp, *tmp_mp = NULL;

		STRUCT_INIT(strpeek, flag);

		error = strcopyin((caddr_t)arg, STRUCT_BUF(strpeek),
		    STRUCT_SIZE(strpeek), STRPEEK, copyflag);
		if (error)
			return (error);

		mutex_enter(QLOCK(rdq));
		mp = rdq->q_first;
		/*
		 * Skip the invalid messages
		 */
		while (mp) {
			if (!(mp->b_flag & MSGNOGET) &&
			    (mp->b_datap->db_type != M_SIG))
				break;
			mp = mp->b_next;
		}
		/*
		 * If user has requested to peek at a high priority message
		 * and first message is not, return 0
		 */
		if (mp) {
			if ((STRUCT_FGET(strpeek, flags) & RS_HIPRI) &&
			    queclass(mp) == QNORM) {
				*rvalp = 0;
				mutex_exit(QLOCK(rdq));
				return (0);
			}
		} else if (stp->sd_struiordq == NULL ||
		    (STRUCT_FGET(strpeek, flags) & RS_HIPRI)) {
			/*
			 * No mblks to look at at the streamhead and
			 * 1). This isn't a synch stream or
			 * 2). This is a synch stream but caller wants high
			 *	priority messages which is not supported by
			 *	the synch stream. (it only supports QNORM)
			 */
			*rvalp = 0;
			mutex_exit(QLOCK(rdq));
			return (0);
		}

		fmp = mp;

		if (mp && mp->b_datap->db_type == M_PASSFP) {
			mutex_exit(QLOCK(rdq));
			return (EBADMSG);
		}

		ASSERT(mp == NULL || mp->b_datap->db_type == M_PCPROTO ||
		    mp->b_datap->db_type == M_PROTO ||
		    mp->b_datap->db_type == M_DATA);

		if (mp && mp->b_datap->db_type == M_PCPROTO) {
			STRUCT_FSET(strpeek, flags, RS_HIPRI);
		} else {
			STRUCT_FSET(strpeek, flags, 0);
		}


		if (mp && ((tmp_mp = dupmsg(mp)) == NULL)) {
			mutex_exit(QLOCK(rdq));
			return (ENOSR);
		}
		mutex_exit(QLOCK(rdq));

		/*
		 * set mp = tmp_mp, so that I_PEEK processing can continue.
		 * tmp_mp is used to free the dup'd message.
		 */
		mp = tmp_mp;

		uio.uio_fmode = 0;
		uio.uio_segflg = (copyflag == U_TO_K) ? UIO_USERSPACE :
		    UIO_SYSSPACE;
		uio.uio_limit = 0;
		/*
		 * First process PROTO blocks, if any.
		 * If user doesn't want to get ctl info by setting maxlen <= 0,
		 * then set len to -1/0 and skip control blocks part.
		 */
		if (STRUCT_FGET(strpeek, ctlbuf.maxlen) < 0)
			STRUCT_FSET(strpeek, ctlbuf.len, -1);
		else if (STRUCT_FGET(strpeek, ctlbuf.maxlen) == 0)
			STRUCT_FSET(strpeek, ctlbuf.len, 0);
		else {
			int	ctl_part = 0;

			iov.iov_base = STRUCT_FGETP(strpeek, ctlbuf.buf);
			iov.iov_len = STRUCT_FGET(strpeek, ctlbuf.maxlen);
			uio.uio_iov = &iov;
			uio.uio_resid = iov.iov_len;
			uio.uio_loffset = 0;
			uio.uio_iovcnt = 1;
			while (mp && mp->b_datap->db_type != M_DATA &&
			    uio.uio_resid >= 0) {
				ASSERT(STRUCT_FGET(strpeek, flags) == 0 ?
				    mp->b_datap->db_type == M_PROTO :
				    mp->b_datap->db_type == M_PCPROTO);

				if ((n = MIN(uio.uio_resid,
				    mp->b_wptr - mp->b_rptr)) != 0 &&
				    (error = uiomove((char *)mp->b_rptr, n,
				    UIO_READ, &uio)) != 0) {
					freemsg(tmp_mp);
					return (error);
				}
				ctl_part = 1;
				mp = mp->b_cont;
			}
			/* No ctl message */
			if (ctl_part == 0)
				STRUCT_FSET(strpeek, ctlbuf.len, -1);
			else
				STRUCT_FSET(strpeek, ctlbuf.len,
				    STRUCT_FGET(strpeek, ctlbuf.maxlen) -
				    uio.uio_resid);
		}

		/*
		 * Now process DATA blocks, if any.
		 * If user doesn't want to get data info by setting maxlen <= 0,
		 * then set len to -1/0 and skip data blocks part.
		 */
		if (STRUCT_FGET(strpeek, databuf.maxlen) < 0)
			STRUCT_FSET(strpeek, databuf.len, -1);
		else if (STRUCT_FGET(strpeek, databuf.maxlen) == 0)
			STRUCT_FSET(strpeek, databuf.len, 0);
		else {
			int	data_part = 0;

			iov.iov_base = STRUCT_FGETP(strpeek, databuf.buf);
			iov.iov_len = STRUCT_FGET(strpeek, databuf.maxlen);
			uio.uio_iov = &iov;
			uio.uio_resid = iov.iov_len;
			uio.uio_loffset = 0;
			uio.uio_iovcnt = 1;
			while (mp && uio.uio_resid) {
				if (mp->b_datap->db_type == M_DATA) {
					if ((n = MIN(uio.uio_resid,
					    mp->b_wptr - mp->b_rptr)) != 0 &&
					    (error = uiomove((char *)mp->b_rptr,
						n, UIO_READ, &uio)) != 0) {
						freemsg(tmp_mp);
						return (error);
					}
					data_part = 1;
				}
				ASSERT(data_part == 0 ||
				    mp->b_datap->db_type == M_DATA);
				mp = mp->b_cont;
			}
			/* No data message */
			if (data_part == 0)
				STRUCT_FSET(strpeek, databuf.len, -1);
			else
				STRUCT_FSET(strpeek, databuf.len,
				    STRUCT_FGET(strpeek, databuf.maxlen) -
				    uio.uio_resid);
		}
		freemsg(tmp_mp);

		/*
		 * It is a synch stream and user wants to get
		 * data (maxlen > 0).
		 * uio setup is done by the codes that process DATA
		 * blocks above.
		 */
		if ((fmp == NULL) && STRUCT_FGET(strpeek, databuf.maxlen) > 0) {
			infod_t infod;

			infod.d_cmd = INFOD_COPYOUT;
			infod.d_res = 0;
			infod.d_uiop = &uio;
			error = infonext(rdq, &infod);
			if (error == EINVAL || error == EBUSY)
				error = 0;
			if (error)
				return (error);
			STRUCT_FSET(strpeek, databuf.len, STRUCT_FGET(strpeek,
			    databuf.maxlen) - uio.uio_resid);
			if (STRUCT_FGET(strpeek, databuf.len) == 0) {
				/*
				 * No data found by the infonext().
				 */
				STRUCT_FSET(strpeek, databuf.len, -1);
			}
		}
		error = strcopyout((caddr_t)STRUCT_BUF(strpeek), (caddr_t)arg,
		    STRUCT_SIZE(strpeek), STRPEEK, copyflag);
		if (error) {
			return (error);
		}
		/*
		 * If there is no message retrieved, set return code to 0
		 * otherwise, set it to 1.
		 */
		if (STRUCT_FGET(strpeek, ctlbuf.len) == -1 &&
		    STRUCT_FGET(strpeek, databuf.len) == -1)
			*rvalp = 0;
		else
			*rvalp = 1;
		return (0);
	    }

	case I_FDINSERT:
	    {
		STRUCT_DECL(strfdinsert, strfdinsert);
		struct file *resftp;
		struct stdata *resstp;
		t_uscalar_t	ival;
		ssize_t msgsize;
		struct strbuf mctl;

		STRUCT_INIT(strfdinsert, flag);
		if (stp->sd_flag & STRHUP)
			return (ENXIO);
		/*
		 * STRDERR, STWRERR and STPLEX tested above.
		 */
		error = strcopyin((caddr_t)arg, STRUCT_BUF(strfdinsert),
		    STRUCT_SIZE(strfdinsert),
		    STRFDINSERT, copyflag);
		if (error)
			return (error);

		if (STRUCT_FGET(strfdinsert, offset) < 0 ||
		    (STRUCT_FGET(strfdinsert, offset) %
		    sizeof (t_uscalar_t)) != 0)
			return (EINVAL);
		if ((resftp = getf(STRUCT_FGET(strfdinsert, fildes))) != NULL) {
			if ((resstp = resftp->f_vnode->v_stream) == NULL) {
				releasef(STRUCT_FGET(strfdinsert, fildes));
				return (EINVAL);
			}
		} else
			return (EINVAL);

		mutex_enter(&resstp->sd_lock);
		if (resstp->sd_flag & (STRDERR|STWRERR|STRHUP|STPLEX)) {
			error = strgeterr(resstp,
					STRDERR|STWRERR|STRHUP|STPLEX, 0);
			if (error != 0) {
				mutex_exit(&resstp->sd_lock);
				releasef(STRUCT_FGET(strfdinsert, fildes));
				return (error);
			}
		}
		mutex_exit(&resstp->sd_lock);

#ifdef	_ILP32
		{
			queue_t	*q;
			queue_t	*mate = NULL;

			/* get read queue of stream terminus */
			claimstr(resstp->sd_wrq);
			for (q = resstp->sd_wrq->q_next; q->q_next != NULL;
			    q = q->q_next)
				if (!STRMATED(resstp) && STREAM(q) != resstp &&
				    mate == NULL) {
					ASSERT(q->q_qinfo->qi_srvp);
					ASSERT(_OTHERQ(q)->q_qinfo->qi_srvp);
					claimstr(q);
					mate = q;
				}
			q = _RD(q);
			if (mate)
				releasestr(mate);
			releasestr(resstp->sd_wrq);
			ival = (t_uscalar_t)q;
		}
#else
		ival = (t_uscalar_t)getminor(resftp->f_vnode->v_rdev);
#endif	/* _ILP32 */

		if (STRUCT_FGET(strfdinsert, ctlbuf.len) <
		    STRUCT_FGET(strfdinsert, offset) + sizeof (t_uscalar_t)) {
			releasef(STRUCT_FGET(strfdinsert, fildes));
			return (EINVAL);
		}

		/*
		 * Check for legal flag value.
		 */
		if (STRUCT_FGET(strfdinsert, flags) & ~RS_HIPRI) {
			releasef(STRUCT_FGET(strfdinsert, fildes));
			return (EINVAL);
		}

		/* get these values from those cached in the stream head */
		mutex_enter(QLOCK(stp->sd_wrq));
		rmin = stp->sd_qn_minpsz;
		rmax = stp->sd_qn_maxpsz;
		mutex_exit(QLOCK(stp->sd_wrq));

		/*
		 * Make sure ctl and data sizes together fall within
		 * the limits of the max and min receive packet sizes
		 * and do not exceed system limit.  A negative data
		 * length means that no data part is to be sent.
		 */
		ASSERT((rmax >= 0) || (rmax == INFPSZ));
		if (rmax == 0) {
			releasef(STRUCT_FGET(strfdinsert, fildes));
			return (ERANGE);
		}
		if ((msgsize = STRUCT_FGET(strfdinsert, databuf.len)) < 0)
			msgsize = 0;
		if ((msgsize < rmin) ||
		    ((msgsize > rmax) && (rmax != INFPSZ)) ||
		    (STRUCT_FGET(strfdinsert, ctlbuf.len) > strctlsz)) {
			releasef(STRUCT_FGET(strfdinsert, fildes));
			return (ERANGE);
		}

		mutex_enter(&stp->sd_lock);
		while (!(STRUCT_FGET(strfdinsert, flags) & RS_HIPRI) &&
		    !canputnext(stp->sd_wrq)) {
			if ((error = strwaitq(stp, WRITEWAIT, (ssize_t)0,
			    flag, -1, &done)) != 0 || done) {
				mutex_exit(&stp->sd_lock);
				releasef(STRUCT_FGET(strfdinsert, fildes));
				return (error);
			}
			if (stp->sd_sidp != NULL &&
			    stp->sd_vnode->v_type != VFIFO) {
				mutex_exit(&stp->sd_lock);
				if (error = straccess(stp, access)) {
					releasef(
					    STRUCT_FGET(strfdinsert, fildes));
					return (error);
				}
				mutex_enter(&stp->sd_lock);
			}
		}
		mutex_exit(&stp->sd_lock);

		/*
		 * Copy strfdinsert.ctlbuf into native form of
		 * ctlbuf to pass down into strmakemsg().
		 */
		mctl.maxlen = STRUCT_FGET(strfdinsert, ctlbuf.maxlen);
		mctl.len = STRUCT_FGET(strfdinsert, ctlbuf.len);
		mctl.buf = STRUCT_FGETP(strfdinsert, ctlbuf.buf);

		iov.iov_base = STRUCT_FGETP(strfdinsert, databuf.buf);
		iov.iov_len = STRUCT_FGET(strfdinsert, databuf.len);
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_loffset = 0;
		uio.uio_segflg = (copyflag == U_TO_K) ? UIO_USERSPACE :
		    UIO_SYSSPACE;
		uio.uio_fmode = 0;
		uio.uio_resid = iov.iov_len;
		if ((error = strmakemsg(&mctl,
		    &msgsize, &uio, stp,
		    STRUCT_FGET(strfdinsert, flags), &mp)) != 0 || !mp) {
			STRUCT_FSET(strfdinsert, databuf.len, msgsize);
			releasef(STRUCT_FGET(strfdinsert, fildes));
			return (error);
		}

		STRUCT_FSET(strfdinsert, databuf.len, msgsize);

		/*
		 * Place the possibly reencoded queue pointer 'offset' bytes
		 * from the start of the control portion of the message.
		 */
		*((t_uscalar_t *)(mp->b_rptr +
		    STRUCT_FGET(strfdinsert, offset))) = ival;

		/*
		 * Put message downstream.
		 */
		run_queues = 1;
		putnext(stp->sd_wrq, mp);
		queuerun();
		releasef(STRUCT_FGET(strfdinsert, fildes));
		return (error);
	    }

	case I_SENDFD:
	    {
		struct file *fp;

		if ((fp = getf((int)arg)) == NULL)
			return (EBADF);
		error = do_sendfp(stp, fp, crp);
#ifdef C2_AUDIT
		if (audit_active) {
			audit_fdsend((int)arg, fp, error);
		}
#endif
		releasef((int)arg);
		return (error);
	    }

	case I_RECVFD:
	case I_E_RECVFD:
	    {
		struct k_strrecvfd *srf;
		int i, fd;

		mutex_enter(&stp->sd_lock);
		while (!(mp = getq(rdq))) {
			if (stp->sd_flag & (STRHUP|STREOF)) {
				mutex_exit(&stp->sd_lock);
				return (ENXIO);
			}
			if ((error = strwaitq(stp, GETWAIT, (ssize_t)0,
			    flag, -1, &done)) != 0 || done) {
				mutex_exit(&stp->sd_lock);
				return (error);
			}
			if (stp->sd_sidp != NULL &&
			    stp->sd_vnode->v_type != VFIFO) {
				mutex_exit(&stp->sd_lock);
				if (error = straccess(stp, access))
					return (error);
				mutex_enter(&stp->sd_lock);
			}
		}
		if (mp->b_datap->db_type != M_PASSFP) {
			putback(stp, rdq, mp, mp->b_band);
			mutex_exit(&stp->sd_lock);
			return (EBADMSG);
		}
		mutex_exit(&stp->sd_lock);

		srf = (struct k_strrecvfd *)mp->b_rptr;
		if ((fd = ufalloc(0)) == -1) {
			mutex_enter(&stp->sd_lock);
			putback(stp, rdq, mp, mp->b_band);
			mutex_exit(&stp->sd_lock);
			return (EMFILE);
		}
		if (cmd == I_RECVFD) {
			struct o_strrecvfd	ostrfd;

			/* check to see if uid/gid values are too large. */

			if (srf->uid > (o_uid_t)USHRT_MAX ||
			    srf->gid > (o_gid_t)USHRT_MAX) {
				mutex_enter(&stp->sd_lock);
				putback(stp, rdq, mp, mp->b_band);
				mutex_exit(&stp->sd_lock);
				setf(fd, NULL);	/* release fd entry */
				return (EOVERFLOW);
			}

			ostrfd.fd = fd;
			ostrfd.uid = (o_uid_t)srf->uid;
			ostrfd.gid = (o_gid_t)srf->gid;

			/* Null the filler bits */
			for (i = 0; i < 8; i++)
				ostrfd.fill[i] = 0;

			if (error = strcopyout(&ostrfd, (caddr_t)arg,
			    sizeof (struct o_strrecvfd),
			    O_STRRECVFD, copyflag)) {

				setf(fd, NULL);	/* release fd entry */

				mutex_enter(&stp->sd_lock);
				putback(stp, rdq, mp, mp->b_band);
				mutex_exit(&stp->sd_lock);
				return (error);
			}
		} else {		/* I_E_RECVFD */
			struct strrecvfd	strfd;

			strfd.fd = fd;
			strfd.uid = srf->uid;
			strfd.gid = srf->gid;

			/* null the filler bits */
			for (i = 0; i < 8; i++)
				strfd.fill[i] = 0;

			if (error = strcopyout(&strfd, (caddr_t)arg,
			    sizeof (struct strrecvfd), STRRECVFD, copyflag)) {
				setf(fd, NULL);	/* release fd entry */
				mutex_enter(&stp->sd_lock);
				putback(stp, rdq, mp, mp->b_band);
				mutex_exit(&stp->sd_lock);
				return (error);
			}
		}

#ifdef C2_AUDIT
		if (audit_active) {
			audit_fdrecv(fd, srf->fp);
		}
#endif
		setf(fd, srf->fp);
		freemsg(mp);
		return (0);
	    }

	case I_SWROPT:
		/*
		 * Set/clear the write options. arg is a bit
		 * mask with any of the following bits set...
		 * 	SNDZERO - send zero length message
		 *	SNDPIPE - send sigpipe to process if
		 *		sd_werror is set and process is
		 *		doing a write or putmsg.
		 * The new stream head write options should reflect
		 * what is in arg.
		 */
		if (arg & ~(SNDZERO|SNDPIPE))
			return (EINVAL);

		mutex_enter(&stp->sd_lock);
		stp->sd_flag &= ~STRSNDZERO;
		stp->sd_wput_opt &= ~SW_SIGPIPE;
		if (arg & SNDZERO)
			stp->sd_flag |= STRSNDZERO;
		if (arg & SNDPIPE)
			stp->sd_wput_opt |= SW_SIGPIPE;
		mutex_exit(&stp->sd_lock);
		return (0);

	case I_GWROPT:
	    {
		int wropt = 0;

		if (stp->sd_flag & STRSNDZERO)
			wropt |= SNDZERO;
		if (stp->sd_wput_opt & SW_SIGPIPE)
			wropt |= SNDPIPE;
		error = strcopyout((caddr_t)&wropt, (caddr_t)arg,
		    sizeof (wropt), STRINT, copyflag);
		return (error);
	    }

	case I_LIST:
		/*
		 * Returns all the modules found on this stream,
		 * upto the driver. If argument is NULL, return the
		 * number of modules (including driver). If argument
		 * is not NULL, copy the names into the structure
		 * provided.
		 */

	    {
		queue_t *q;
		int num_modules, space_allocated;
		STRUCT_DECL(str_list, strlist);
		struct str_mlist *mlist_ptr;

		if (arg == NULL) { /* Return number of modules plus driver */
			q = stp->sd_wrq;
			if (stp->sd_vnode->v_type == VFIFO) {
				*rvalp = stp->sd_pushcnt;
			} else {
				*rvalp = stp->sd_pushcnt + 1;
			}
		} else {
			STRUCT_INIT(strlist, flag);

			error = strcopyin((caddr_t)arg, STRUCT_BUF(strlist),
				STRUCT_SIZE(strlist), STRLIST,
				copyflag);
			if (error)
				return (error);

			space_allocated = STRUCT_FGET(strlist, sl_nmods);
			if ((space_allocated) <= 0)
				return (EINVAL);
			claimstr(stp->sd_wrq);
			q = stp->sd_wrq;
			num_modules = 0;
			while (_SAMESTR(q) && (space_allocated != 0)) {
				char *name =
				    q->q_next->q_qinfo->qi_minfo->mi_idname;

				mlist_ptr = STRUCT_FGETP(strlist, sl_modlist);

				error = strcopyout(name, mlist_ptr,
				    strlen(name) + 1, STRNAME, copyflag);

				if (error) {
					releasestr(stp->sd_wrq);
					return (error);
				}
				q = q->q_next;
				space_allocated--;
				num_modules++;
				mlist_ptr =
				    (struct str_mlist *)((uintptr_t)mlist_ptr +
				    sizeof (struct str_mlist));
				STRUCT_FSETP(strlist, sl_modlist, mlist_ptr);
			}
			releasestr(stp->sd_wrq);
			error = strcopyout((caddr_t)&num_modules, (caddr_t)arg,
			    sizeof (int), STRINT, copyflag);
		}
		return (error);
	    }

	case I_CKBAND:
	    {
		queue_t *q;
		qband_t *qbp;

		/*
		 * Ignores MSGNOGET.
		 */
		if ((arg < 0) || (arg >= NBAND))
			return (EINVAL);
		q = _RD(stp->sd_wrq);
		mutex_enter(QLOCK(q));
		if (arg > (int)q->q_nband) {
			*rvalp = 0;
		} else {
			if (arg == 0) {
				if (q->q_first)
					*rvalp = 1;
				else
					*rvalp = 0;
			} else {
				qbp = q->q_bandp;
				while (--arg > 0)
					qbp = qbp->qb_next;
				if (qbp->qb_first)
					*rvalp = 1;
				else
					*rvalp = 0;
			}
		}
		mutex_exit(QLOCK(q));
		return (0);
	    }

	case I_GETBAND:
	    {
		int intpri;
		queue_t *q;

		/*
		 * Ignores MSGNOGET.
		 */
		q = _RD(stp->sd_wrq);
		mutex_enter(QLOCK(q));
		mp = q->q_first;
		if (!mp) {
			mutex_exit(QLOCK(q));
			return (ENODATA);
		}
		intpri = (int)mp->b_band;
		error = strcopyout((caddr_t)&intpri, (caddr_t)arg, sizeof (int),
		    STRINT, copyflag);
		mutex_exit(QLOCK(q));
		return (error);
	    }

	case I_ATMARK:
	    {
		queue_t *q;

		if (arg & ~(ANYMARK|LASTMARK))
			return (EINVAL);
		q = _RD(stp->sd_wrq);
		mutex_enter(&stp->sd_lock);
		if ((stp->sd_flag & STRATMARK) && (arg == ANYMARK)) {
			*rvalp = 1;
		} else {
			mutex_enter(QLOCK(q));
			mp = q->q_first;

			/*
			 * Hack for sockets compatibility.  We need to
			 * ignore any messages at the stream head that
			 * are marked MSGNOGET and are not marked MSGMARK.
			 */
			while (mp &&
			    ((mp->b_flag & (MSGNOGET|MSGMARK)) == MSGNOGET))
				mp = mp->b_next;

			if (!mp)
				*rvalp = 0;
			else if ((arg == ANYMARK) && (mp->b_flag & MSGMARK))
				*rvalp = 1;
			else if ((arg == LASTMARK) && (mp == stp->sd_mark))
				*rvalp = 1;
			else
				*rvalp = 0;
			mutex_exit(QLOCK(q));
		}
		mutex_exit(&stp->sd_lock);
		return (0);
	    }

	case I_CANPUT:
	    {
		char band;

		if ((arg < 0) || (arg >= NBAND))
			return (EINVAL);
		band = (char)arg;
		*rvalp = bcanputnext(stp->sd_wrq, band);
		return (0);
	    }

	case I_SETCLTIME:
	    {
		int closetime;

		error = strcopyin((caddr_t)arg, (caddr_t)&closetime,
		    sizeof (closetime), STRLONG, copyflag);
		if (error)
			return (error);
		if (closetime < 0)
			return (EINVAL);

		stp->sd_closetime = closetime;
		return (0);
	    }

	case I_GETCLTIME:
	    {
		int closetime;

		closetime = stp->sd_closetime;
		error = strcopyout((caddr_t)&closetime, (caddr_t)arg,
		    sizeof (closetime), STRLONG, copyflag);
		return (error);
	    }

	case TIOCSSID:
	{
		pid_t sid;

		if (error = strcopyin((caddr_t)arg, (caddr_t)&sid,
		    sizeof (pid_t), STRPIDT, copyflag))
			return (error);
		mutex_enter(&pidlock);
		if (stp->sd_sidp != ttoproc(curthread)->p_sessp->s_sidp) {
			mutex_exit(&pidlock);
			return (ENOTTY);
		}
		mutex_exit(&pidlock);
		return (realloctty(ttoproc(curthread), sid));
	}

	case TIOCGSID:
	{
		pid_t sid;

		mutex_enter(&pidlock);
		if (stp->sd_sidp == NULL) {
			mutex_exit(&pidlock);
			return (ENOTTY);
		}
		sid = stp->sd_sidp->pid_id;
		mutex_exit(&pidlock);
		return strcopyout((caddr_t)&sid,
		    (caddr_t)arg, sizeof (pid_t), STRPIDT, copyflag);
	}

	case TIOCSPGRP:
	{
		pid_t pgrp;
		proc_t *q;
		pid_t	sid, fg_pgid, bg_pgid;

		if (error = strcopyin((caddr_t)arg, (caddr_t)&pgrp,
		    sizeof (pid_t), STRPIDT, copyflag))
			return (error);
		mutex_enter(&stp->sd_lock);
		mutex_enter(&pidlock);
		if (stp->sd_sidp != ttoproc(curthread)->p_sessp->s_sidp) {
			mutex_exit(&pidlock);
			mutex_exit(&stp->sd_lock);
			return (ENOTTY);
		}
		if (pgrp == stp->sd_pgidp->pid_id) {
			mutex_exit(&pidlock);
			mutex_exit(&stp->sd_lock);
			return (0);
		}
		if (pgrp <= 0 || pgrp >= maxpid) {
			mutex_exit(&pidlock);
			mutex_exit(&stp->sd_lock);
			return (EINVAL);
		}
		if ((q = pgfind(pgrp)) == NULL ||
		    q->p_sessp != ttoproc(curthread)->p_sessp) {
			mutex_exit(&pidlock);
			mutex_exit(&stp->sd_lock);
			return (EPERM);
		}
		sid = stp->sd_sidp->pid_id;
		fg_pgid = q->p_pgrp;
		bg_pgid = stp->sd_pgidp->pid_id;
		CL_SET_PROCESS_GROUP(curthread, sid, bg_pgid, fg_pgid);
		PID_RELE(stp->sd_pgidp);
		stp->sd_pgidp = q->p_pgidp;
		PID_HOLD(stp->sd_pgidp);
		mutex_exit(&pidlock);
		mutex_exit(&stp->sd_lock);
		return (0);
	}

	case TIOCGPGRP:
	{
		pid_t pgrp;

		mutex_enter(&pidlock);
		if (stp->sd_sidp == NULL) {
			mutex_exit(&pidlock);
			return (ENOTTY);
		}
		pgrp = stp->sd_pgidp->pid_id;
		mutex_exit(&pidlock);
		return strcopyout((caddr_t)&pgrp,
		    (caddr_t)arg, sizeof (pid_t), STRPIDT, copyflag);
	}

	case FIONBIO:
	case FIOASYNC:
		return (0);	/* handled by the upper layer */
	}
}

/* ARGSUSED */
int
do_sendfp(struct stdata *stp, struct file *fp, struct cred *cr)
{
	queue_t *qp, *nextqp;
	struct k_strrecvfd *srf;
	mblk_t *mp;
	queue_t	*mate = NULL;
	syncq_t	*sq = NULL;

	if (stp->sd_flag & STRHUP)
		return (ENXIO);

	claimstr(stp->sd_wrq);

	/* Fastpath, we have a pipe, and we are already mated, use it. */
	if (STRMATED(stp)) {
	qp = _RD(stp->sd_mate->sd_wrq);
	claimstr(qp);
	mate = qp;
	} else { /* Not already mated. */

		/*
		 * Walk the stream to the end of this one.
		 * assumes that the claimstr() will prevent
		 * plumbing between the stream head and the
		 * driver from changing
		 */
		qp = stp->sd_wrq;

		/*
		 * Loop until we reach the end of this stream.
		 * On completion, qp points to the write queue
		 * at the end of the stream, or the read queue
		 * at the stream head if this is a fifo.
		 */
		while (((qp = qp->q_next) != NULL) && _SAMESTR(qp))
		;

		/*
		 * Just in case we get a q_next which is NULL, but
		 * not at the end of the stream.  This is actually
		 * broken, so we set an assert to catch it in
		 * debug, and set an error and return if not debug.
		 */
		ASSERT(qp);
		if (qp == NULL) {
			releasestr(stp->sd_wrq);
			return (EINVAL);
		}

		/*
		 * Enter the syncq for the driver, so (hopefully)
		 * the queue values will not change on us.
		 * XXXX - This will only prevent the race IFF only
		 *   the write side modifies the q_next member, and
		 *   the put procedure is protected by at least
		 *   MT_PERQ.
		 */
		if ((sq = qp->q_syncq) != NULL)
			entersq(sq, SQ_PUT);

		/* Now get the q_next value from this qp. */
		nextqp = qp->q_next;

		/*
		 * If nextqp exists and the other stream is different
		 * from this one claim the stream, set the mate, and
		 * get the read queue at the stream head of the other
		 * stream.  Assumes that nextqp was at least valid when
		 * we got it.  Hopefully the entersq of the driver
		 * will prevent it from changing on us.
		 */
		if ((nextqp != NULL) && (STREAM(nextqp) != stp)) {
			ASSERT(qp->q_qinfo->qi_srvp);
			ASSERT(_OTHERQ(qp)->q_qinfo->qi_srvp);
			ASSERT(_OTHERQ(qp->q_next)->q_qinfo->qi_srvp);
			claimstr(nextqp);

			/* Make sure we still have a q_next */
			if (nextqp != qp->q_next) {
				releasestr(stp->sd_wrq);
				releasestr(nextqp);
				return (EINVAL);
			}

			qp = _RD(STREAM(nextqp)->sd_wrq);
			mate = qp;
		}
		/* If we entered the synq above, leave it. */
		if (sq != NULL)
			leavesq(sq, SQ_PUT);
	} /*  STRMATED(STP)  */

	/* XXX prevents substitution of the ops vector */
	if (qp->q_qinfo != &strdata && qp->q_qinfo != &fifo_strdata) {
		ASSERT(mate == NULL);
		releasestr(stp->sd_wrq);
		if (mate)
			releasestr(mate);
		return (EINVAL);
	}

	if ((qp->q_flag & QFULL) ||
	    !(mp = allocb(sizeof (struct k_strrecvfd), BPRI_MED))) {
		releasestr(stp->sd_wrq);
		if (mate)
			releasestr(mate);
		return (EAGAIN);
	}
	srf = (struct k_strrecvfd *)mp->b_rptr;
	mp->b_wptr += sizeof (struct k_strrecvfd);
	mp->b_datap->db_type = M_PASSFP;
	srf->fp = fp;
	srf->uid = curthread->t_cred->cr_uid;
	srf->gid = curthread->t_cred->cr_gid;
	mutex_enter(&fp->f_tlock);
	fp->f_count++;
	mutex_exit(&fp->f_tlock);
	/*
	 * Should the M_PASSFP message get stuck on the syncq
	 * (due to a close off the other side) then flush_syncq will take care
	 * of freeing the M_PASSFP using freemsg_flush.
	 */
	put(qp, mp);
	releasestr(stp->sd_wrq);
	if (mate)
		releasestr(mate);
	return (0);
}

/*
 * Send an ioctl message downstream and wait for acknowledgement.
 * flags may be set to either U_TO_K or K_TO_K and a combination
 * of STR_NOERROR or STR_NOSIG
 * STR_NOSIG: Signals are essentially ignored or held and have
 *	no effect for the duration of the call.
 * STR_NOERROR: Ignores stream head read, write and hup errors.
 *	Additionally, if an existing ioctl times out, it is assumed
 *	lost and and this ioctl will continue as if the previous ioctl had
 *	finished.  ETIME may be returned if this ioctl times out (i.e.
 *	ic_timout is not INFTIM).  Non-stream head errors may be returned if
 *	the ioc_error indicates that the driver/module had problems,
 *	an EFAULT was found when accessing user data, a lack of
 * 	resources, etc.
 */
int
strdoioctl(
	struct stdata *stp,
	struct strioctl *strioc,
	mblk_t *ebp,		/* extended data for ioctl */
	int fflags,		/* file flags with model info */
	int flag,
	char *fmtp,
	cred_t *crp,
	int *rvalp)
{
	mblk_t *bp;
	struct iocblk *iocbp;
	struct copyreq *reqp;
	struct copyresp *resp;
	mblk_t *fmtbp;
	int id;
	int transparent = 0;
	int error = 0;
	int len = 0;
	caddr_t taddr;
	int copyflag = (flag & (U_TO_K | K_TO_K));
	int sigflag = (flag & STR_NOSIG);
	int errs;

	ASSERT(copyflag == U_TO_K || copyflag == K_TO_K);
	ASSERT((fflags & FMODELS) != 0);

	TRACE_2(TR_FAC_STREAMS_FR,
		TR_STRDOIOCTL,
		"strdoioctl:stp %lX cmd %X", stp, strioc->ic_cmd);
	if (strioc->ic_len == TRANSPARENT) {	/* send arg in M_DATA block */
		transparent = 1;
		strioc->ic_len = sizeof (intptr_t);
	}

	if ((strioc->ic_len < 0) ||
	    ((strmsgsz > 0) && (strioc->ic_len > strmsgsz))) {
		if (ebp)
			freeb(ebp);
		return (EINVAL);
	}

	if (!(bp = allocb_wait(sizeof (union ioctypes), BPRI_HI, sigflag,
	    &error))) {
			if (ebp)
				freeb(ebp);
			return (error);
	}

	bzero(bp->b_wptr, sizeof (union ioctypes));

	iocbp = (struct iocblk *)bp->b_wptr;
	iocbp->ioc_count = strioc->ic_len;
	iocbp->ioc_cmd = strioc->ic_cmd;
	iocbp->ioc_flag = (fflags & FMODELS);

	/*
	 * replace cred of open process with those of the current
	 */
	crp = ddi_get_cred();
	crhold(crp);
	iocbp->ioc_cr = crp;
	bp->b_datap->db_type = M_IOCTL;
	bp->b_datap->db_uid = crp->cr_uid;
	bp->b_wptr += sizeof (struct iocblk);

	if (flag & STR_NOERROR)
		errs = STPLEX;
	else
		errs = STRHUP|STRDERR|STWRERR|STPLEX;

	/*
	 * If there is data to copy into ioctl block, do so.
	 */
	if (iocbp->ioc_count) {
		if (transparent)
			/*
			 * Note: STR_NOERROR does not have an effect
			 * in putiocd()
			 */
			id = K_TO_K | sigflag;
		else
			id = flag;
		if (error = putiocd(bp, ebp, strioc->ic_dp, id, fmtp)) {
			freemsg(bp);
			if (ebp)
				freeb(ebp);
			crfree(crp);
			return (error);
		}

		/*
		 * We could have slept copying in user pages.
		 * Recheck the stream head state (the other end
		 * of a pipe could have gone away).
		 */
		mutex_enter(&stp->sd_lock);
		if (stp->sd_flag & errs) {
			error = strgeterr(stp, errs, 0);
			if (error != 0) {
				mutex_exit(&stp->sd_lock);
				freemsg(bp);
				crfree(crp);
				return (error);
			}
		}
	} else {
		bp->b_cont = ebp;
		mutex_enter(&stp->sd_lock);
	}
	if (transparent)
		iocbp->ioc_count = TRANSPARENT;

	/*
	 * Block for up to STRTIMOUT milliseconds if there is an outstanding
	 * ioctl for this stream already pending.  All processes
	 * sleeping here will be awakened as a result of an ACK
	 * or NAK being received for the outstanding ioctl, or
	 * as a result of the timer expiring on the outstanding
	 * ioctl (a failure), or as a result of any waiting
	 * process's timer expiring (also a failure).
	 */

	error = 0;

	while (stp->sd_flag & IOCWAIT) {
		clock_t cv_rval;

		TRACE_0(TR_FAC_STREAMS_FR,
			TR_STRDOIOCTL_WAIT,
			"strdoioctl sleeps - IOCWAIT");
		cv_rval = str_cv_wait(&stp->sd_iocmonitor, &stp->sd_lock,
		    STRTIMOUT, sigflag);
		if (cv_rval <= 0) {
			if (cv_rval == 0) {
				error = EINTR;
			} else {
				if (flag & STR_NOERROR) {
					/*
					 * Trashing pervious IOCWAIT
					 * We'll assume it got lost
					 */
					stp->sd_flag &= ~IOCWAIT;
				} else {
					/*
					 * pending ioctl has caused
					 * us to time out
					 */
					error =  ETIME;
				}
			}
		} else if ((stp->sd_flag & errs)) {
			error = strgeterr(stp, errs, 0);
		}
		if (error) {
			mutex_exit(&stp->sd_lock);
			freemsg(bp);
			crfree(crp);
			return (error);
		}
	}

	/*
	 * Have control of ioctl mechanism.
	 * Send down ioctl packet and wait for response.
	 */
	if (stp->sd_iocblk) {
		if (stp->sd_iocblk != (mblk_t *)-1) {
			freemsg(stp->sd_iocblk);
		}
		stp->sd_iocblk = NULL;
	}

	stp->sd_flag |= IOCWAIT;

	/*
	 * Assign sequence number.
	 */
	iocbp->ioc_id = stp->sd_iocid = getiocseqno();

	mutex_exit(&stp->sd_lock);

	TRACE_1(TR_FAC_STREAMS_FR,
		TR_STRDOIOCTL_PUT, "strdoioctl puts to:%lX",
			stp->sd_wrq->q_next);
	run_queues = 1;
	putnext(stp->sd_wrq, bp);
	queuerun();

	/*
	 * Timed wait for acknowledgment.  The wait time is limited by the
	 * timeout value, which must be a positive integer (number of
	 * milliseconds to wait, or 0 (use default value of STRTIMOUT
	 * milliseconds), or -1 (wait forever).  This will be awakened
	 * either by an ACK/NAK message arriving, the timer expiring, or
	 * the timer expiring on another ioctl waiting for control of the
	 * mechanism.
	 */
waitioc:
	mutex_enter(&stp->sd_lock);


	/*
	 * If the reply has already arrived, don't sleep.  If awakened from
	 * the sleep, fail only if the reply has not arrived by then.
	 * Otherwise, process the reply.
	 */
	while (!stp->sd_iocblk) {
		clock_t cv_rval;

		if (stp->sd_flag & errs) {
			error = strgeterr(stp, errs, 0);
			if (error != 0) {
				stp->sd_flag &= ~IOCWAIT;
				cv_broadcast(&stp->sd_iocmonitor);
				mutex_exit(&stp->sd_lock);
				crfree(crp);
				return (error);
			}
		}

		TRACE_0(TR_FAC_STREAMS_FR,
			TR_STRDOIOCTL_WAIT2,
			"strdoioctl sleeps awaiting reply");
		ASSERT(error == 0);

		cv_rval = str_cv_wait(&stp->sd_monitor, &stp->sd_lock,
		    (strioc->ic_timout ?
		    strioc->ic_timout * 1000 : STRTIMOUT), sigflag);
		/*
		 * note: STR_NOERROR does not protect
		 * us here.. use ic_timout < 0
		 */
		if (cv_rval <= 0) {
			if (cv_rval == 0) {
				error = EINTR;
			} else {
				error =  ETIME;
			}
			bp = NULL;
			/*
			 * A message could have come in after we were scheduled
			 * but before we were actually run.
			 */
			if (stp->sd_iocblk) {
				bp = stp->sd_iocblk;
				stp->sd_iocblk = NULL;
			}
			stp->sd_flag &= ~IOCWAIT;
			cv_broadcast(&stp->sd_iocmonitor);
			mutex_exit(&stp->sd_lock);
			if (bp) {
				if ((bp->b_datap->db_type == M_COPYIN) ||
				    (bp->b_datap->db_type == M_COPYOUT)) {
					if (bp->b_cont) {
						freemsg(bp->b_cont);
						bp->b_cont = NULL;
					}
					bp->b_datap->db_type = M_IOCDATA;
					bp->b_wptr = bp->b_rptr +
						sizeof (struct copyresp);
					resp = (struct copyresp *)bp->b_rptr;
					resp->cp_rval =
					    (caddr_t)1; /* failure */
					run_queues = 1;
					putnext(stp->sd_wrq, bp);
					queuerun();
				} else
					freemsg(bp);
			}
			crfree(crp);
			return (error);
		}
	}
	ASSERT(stp->sd_iocblk);
	TRACE_1(TR_FAC_STREAMS_FR,
		TR_STRDOIOCTL_ACK, "strdoioctl got reply:%X",
			bp->b_datap->db_type);
	bp = stp->sd_iocblk;
	if ((bp->b_datap->db_type == M_IOCACK) ||
	    (bp->b_datap->db_type == M_IOCNAK)) {
		stp->sd_iocblk = (mblk_t *)-1;
		stp->sd_flag &= ~IOCWAIT;
		cv_broadcast(&stp->sd_iocmonitor);
		mutex_exit(&stp->sd_lock);
	} else {
		stp->sd_iocblk = NULL;
		mutex_exit(&stp->sd_lock);
	}


	/*
	 * Have received acknowlegment.
	 */

	switch (bp->b_datap->db_type) {
	case M_IOCACK:
		/*
		 * Positive ack.
		 */
		iocbp = (struct iocblk *)bp->b_rptr;

		/*
		 * Set error if indicated.
		 */
		if (iocbp->ioc_error) {
			error = iocbp->ioc_error;
			break;
		}

		/*
		 * Set return value.
		 */
		*rvalp = iocbp->ioc_rval;

		/*
		 * Data may have been returned in ACK message (ioc_count > 0).
		 * If so, copy it out to the user's buffer.
		 */
		if (iocbp->ioc_count && !transparent) {
			if (strioc->ic_cmd == TCGETA ||
			    strioc->ic_cmd == TCGETS ||
			    strioc->ic_cmd == TIOCGETP ||
			    strioc->ic_cmd == LDGETT) {
				if (error = getiocd(bp, strioc->ic_dp,
				    copyflag, fmtp))
					break;
			} else if (error = getiocd(bp, strioc->ic_dp,
			    copyflag, (char *)NULL)) {
				break;
			}
		}
		if (!transparent) {
			if (len)	/* an M_COPYOUT was used with I_STR */
				strioc->ic_len = len;
			else
				strioc->ic_len = (int)iocbp->ioc_count;
		}
		break;

	case M_IOCNAK:
		/*
		 * Negative ack.
		 *
		 * The only thing to do is set error as specified
		 * in neg ack packet.
		 */
		iocbp = (struct iocblk *)bp->b_rptr;

		error = (iocbp->ioc_error ? iocbp->ioc_error : EINVAL);
		break;

	case M_COPYIN:
		/*
		 * Driver or module has requested user ioctl data.
		 */
		reqp = (struct copyreq *)bp->b_rptr;
		fmtbp = bp->b_cont;
		bp->b_cont = NULL;
		if (reqp->cq_flag & RECOPY) {
			/* redo I_STR copyin with canonical processing */
			ASSERT(fmtbp);
			reqp->cq_size = strioc->ic_len;
			error = putiocd(bp, NULL, strioc->ic_dp, flag,
			    (fmtbp ? (char *)fmtbp->b_rptr : (char *)NULL));
			if (fmtbp)
				freeb(fmtbp);
		} else if (reqp->cq_flag & STRCANON) {
			/* copyin with canonical processing */
			ASSERT(fmtbp);
			error = putiocd(bp, NULL, reqp->cq_addr, flag,
			    (fmtbp ? (char *)fmtbp->b_rptr : (char *)NULL));
			if (fmtbp)
				freeb(fmtbp);
		} else {
			ASSERT(fmtbp == NULL);
			freemsg(fmtbp);
			/* copyin raw data (i.e. no canonical processing) */
			error = putiocd(bp, NULL, reqp->cq_addr, flag,
			    (char *)NULL);
		}
		if (error && bp->b_cont) {
			freemsg(bp->b_cont);
			bp->b_cont = NULL;
		}

		bp->b_wptr = bp->b_rptr + sizeof (struct copyresp);
		bp->b_datap->db_type = M_IOCDATA;
		/*
		 * Note that for interrupt thread, the CRED() is
		 * NULL.  db_uid is initialized to -1 when a
		 * dblk structure is allocated.
		 */
		if (CRED() != NULL) {
			bp->b_datap->db_uid = CRED()->cr_uid;
		}
		resp = (struct copyresp *)bp->b_rptr;
		resp->cp_rval = (caddr_t)error;
		resp->cp_flag = (fflags & FMODELS);

		run_queues = 1;
		putnext(stp->sd_wrq, bp);
		queuerun();

		if (error) {
			mutex_enter(&stp->sd_lock);
			stp->sd_flag &= ~IOCWAIT;
			cv_broadcast(&stp->sd_iocmonitor);
			mutex_exit(&stp->sd_lock);
			crfree(crp);
			return (error);
		}

		goto waitioc;

	case M_COPYOUT:
		/*
		 * Driver or module has ioctl data for a user.
		 */
		reqp = (struct copyreq *)bp->b_rptr;
		ASSERT(bp->b_cont);

		/*
		 * Always (transparent or non-transparent )
		 * use the address specified in the request
		 */

		taddr = reqp->cq_addr;
		if (!transparent)
			len = (int)reqp->cq_size;

		if (reqp->cq_flag & STRCANON) {
			/* copyout with canonical processing */
			if ((fmtbp = bp->b_cont) != NULL) {
				bp->b_cont = fmtbp->b_cont;
				fmtbp->b_cont = NULL;
			}
			error = getiocd(bp, taddr, copyflag,
			    (fmtbp ? (char *)fmtbp->b_rptr : (char *)NULL));
			if (fmtbp)
				freeb(fmtbp);
		} else {
			/* copyout raw data (i.e. no canonical processing) */
			error = getiocd(bp, taddr, copyflag, (char *)NULL);
		}
		freemsg(bp->b_cont);
		bp->b_cont = NULL;

		bp->b_wptr = bp->b_rptr + sizeof (struct copyresp);
		bp->b_datap->db_type = M_IOCDATA;
		/*
		 * Note that for interrupt thread, the CRED() is
		 * NULL.  db_uid is initialized to -1 when a
		 * dblk structure is allocated.
		 */
		if (CRED() != NULL) {
			bp->b_datap->db_uid = CRED()->cr_uid;
		}
		resp = (struct copyresp *)bp->b_rptr;
		resp->cp_rval = (caddr_t)error;
		resp->cp_flag = (fflags & FMODELS);

		run_queues = 1;
		putnext(stp->sd_wrq, bp);
		queuerun();

		if (error) {
			mutex_enter(&stp->sd_lock);
			stp->sd_flag &= ~IOCWAIT;
			cv_broadcast(&stp->sd_iocmonitor);
			mutex_exit(&stp->sd_lock);
			crfree(crp);
			return (error);
		}
		goto waitioc;

	default:
		ASSERT(0);
		break;
	}

	freemsg(bp);
	crfree(crp);
	return (error);
}

/*
 * For the SunOS keyboard driver.
 * Return the next available "ioctl" sequence number.
 * Exported, so that streams modules can send "ioctl" messages
 * downstream from their open routine.
 */
int
getiocseqno(void)
{
	int	i;

	mutex_enter(&strresources);
	i = ++ioc_id;
	mutex_exit(&strresources);
	return (i);
}

/*
 * Get the next message from the read queue.  If the message is
 * priority, STRPRI will have been set by strrput().  This flag
 * should be reset only when the entire message at the front of the
 * queue as been consumed.
 *
 * NOTE: strgetmsg and kstrgetmsg have much of the logic in common.
 */
int
strgetmsg(
	struct vnode *vp,
	struct strbuf *mctl,
	struct strbuf *mdata,
	unsigned char *prip,
	int *flagsp,
	int fmode,
	rval_t *rvp)
{
	struct stdata *stp;
	mblk_t *bp, *nbp;
	mblk_t *savemp = NULL;
	mblk_t *savemptail = NULL;
	uint_t old_sd_flag;
	int flg;
	int more = 0;
	int error = 0;
	char first = 1;
	uint_t mark;		/* Contains MSG*MARK and _LASTMARK */
#define	_LASTMARK	0x8000	/* Distinct from MSG*MARK */
	unsigned char pri = 0;
	queue_t *q;
	int	pr = 0;			/* Partial read successful */
	struct uio uios;
	struct uio *uiop = &uios;
	struct iovec iovs;
	unsigned char type;

	TRACE_1(TR_FAC_STREAMS_FR, TR_STRGETMSG_ENTER,
		"strgetmsg:%X", vp);

	ASSERT(vp->v_stream);
	stp = vp->v_stream;
	rvp->r_val1 = 0;

	if (stp->sd_sidp != NULL && stp->sd_vnode->v_type != VFIFO)
		if (error = straccess(stp, JCREAD))
			return (error);

	/* Fast check of flags before acquiring the lock */
	if (stp->sd_flag & (STRDERR|STPLEX)) {
		mutex_enter(&stp->sd_lock);
		error = strgeterr(stp, STRDERR|STPLEX, 0);
		mutex_exit(&stp->sd_lock);
		if (error != 0)
			return (error);
	}

	switch (*flagsp) {
	case MSG_HIPRI:
		if (*prip != 0)
			return (EINVAL);
		break;

	case MSG_ANY:
	case MSG_BAND:
		break;

	default:
		return (EINVAL);
	}
	/*
	 * Setup uio and iov for data part
	 */
	iovs.iov_base = mdata->buf;
	iovs.iov_len = mdata->maxlen;
	uios.uio_iov = &iovs;
	uios.uio_iovcnt = 1;
	uios.uio_loffset = 0;
	uios.uio_segflg = UIO_USERSPACE;
	uios.uio_fmode = 0;
	uios.uio_resid = mdata->maxlen;
	uios.uio_offset = 0;

	q = _RD(stp->sd_wrq);
	mutex_enter(&stp->sd_lock);
	old_sd_flag = stp->sd_flag;
	mark = 0;
	for (;;) {
		int done = 0;
		mblk_t *q_first = q->q_first;

		/*
		 * Get the next message of appropriate priority
		 * from the stream head.  If the caller is interested
		 * in band or hipri messages, then they should already
		 * be enqueued at the stream head.  On the other hand
		 * if the user wants normal (band 0) messages, they
		 * might be deferred in a synchronous stream and they
		 * will need to be pulled up.
		 *
		 * After we have dequeued a message, we might find that
		 * it was a deferred signal that was enqueued at the
		 * stream head.  It must now be posted as part of the
		 * read.  At the time of this comment, we can have a
		 * SIGTSTP M_SIG message as the first message, or there
		 * can be messages that are tagged as MSG_NOGET that
		 * will be bypassed by getq_noenable() and the next
		 * consumable message might be a SIGPOLL M_SIG.
		 * However, the bases were all covered by posting
		 * any signal that might possibly be enqueued here.
		 *
		 * Also note that strrput does not enqueue an M_PCSIG,
		 * and there cannot be more than one hipri message,
		 * so there was no need to have the M_PCSIG case.
		 *
		 * At some time it might be nice to try and wrap the
		 * functionality of kstrgetmsg() and strgetmsg() into
		 * a common routine so to reduce the amount of replicated
		 * code (since they are extremely similar).
		 */
		if (!(*flagsp & (MSG_HIPRI|MSG_BAND))) {
			/* Asking for normal, band0 data */
			bp = strget(stp, q, uiop, first, &error);
			ASSERT(MUTEX_HELD(&stp->sd_lock));
			if (bp != NULL) {
				if (bp->b_datap->db_type == M_SIG) {
					strsignal_nolock(stp, *bp->b_rptr,
					    (int32_t)bp->b_band);
					continue;
				} else {
					break;
				}
			}
			if (error != 0) {
				goto getmout;
			}
		/*
		 * We can't depend on the value of STRPRI here because
		 * the stream head may be in transit. Therefore, we
		 * must look at the type of the first message to
		 * determine if a high priority messages is waiting
		 */
		} else if ((*flagsp & MSG_HIPRI) && q_first != NULL &&
			    q_first->b_datap->db_type >= QPCTL &&
			    (bp = getq_noenab(q)) != NULL) {
			/* Asked for HIPRI and got one */
			ASSERT(bp->b_datap->db_type >= QPCTL);
			break;
		} else if ((*flagsp & MSG_BAND) && q_first != NULL &&
			    ((q_first->b_band >= *prip) ||
			    q_first->b_datap->db_type >= QPCTL) &&
			    (bp = getq_noenab(q)) != NULL) {
			/*
			 * Asked for at least band "prip" and got either at
			 * least that band or a hipri message.
			 */
			ASSERT(bp->b_band >= *prip ||
				bp->b_datap->db_type >= QPCTL);
			if (bp->b_datap->db_type == M_SIG) {
				strsignal_nolock(stp, *bp->b_rptr,
				    (int32_t)bp->b_band);
				continue;
			} else {
				break;
			}
		}

		/* No data. Time to sleep? */
		qbackenable(q, 0);

		/*
		 * If STRHUP or STREOF, return 0 length control and data.
		 * If resid is 0, then a read(fd,buf,0) was done. Do not
		 * sleep to satisfy this request because by default we have
		 * zero bytes to return.
		 */
		if ((stp->sd_flag & (STRHUP|STREOF)) || (mctl->maxlen == 0 &&
		    mdata->maxlen == 0)) {
			mctl->len = mdata->len = 0;
			*flagsp = 0;
			mutex_exit(&stp->sd_lock);
			return (0);
		}
		TRACE_2(TR_FAC_STREAMS_FR, TR_STRGETMSG_WAIT,
			"strgetmsg calls strwaitq:%X, %X",
			vp, uiop);
		if (((error = strwaitq(stp, GETWAIT, (ssize_t)0, fmode, -1,
		    &done)) != 0) || done) {
			TRACE_2(TR_FAC_STREAMS_FR, TR_STRGETMSG_DONE,
				"strgetmsg error or done:%X, %X",
				vp, uiop);
			mutex_exit(&stp->sd_lock);
			return (error);
		}
		TRACE_2(TR_FAC_STREAMS_FR, TR_STRGETMSG_AWAKE,
			"strgetmsg awakes:%X, %X", vp, uiop);
		if (stp->sd_sidp != NULL && stp->sd_vnode->v_type != VFIFO) {
			mutex_exit(&stp->sd_lock);
			if (error = straccess(stp, JCREAD))
				return (error);
			mutex_enter(&stp->sd_lock);
		}
		first = 0;
	}
	ASSERT(bp != NULL);
	/*
	 * Extract any mark information. If the message is not completely
	 * consumed this information will be put in the mblk
	 * that is putback.
	 * If MSGMARKNEXT is set and the message is completely consumed
	 * the STRATMARK flag will be set below. Likewise, if
	 * MSGNOTMARKNEXT is set and the message is
	 * completely consumed STRNOTATMARK will be set.
	 */
	mark = bp->b_flag & (MSGMARK | MSGMARKNEXT | MSGNOTMARKNEXT);
	ASSERT((mark & (MSGMARKNEXT|MSGNOTMARKNEXT)) !=
		(MSGMARKNEXT|MSGNOTMARKNEXT));
	if (mark != 0 && bp == stp->sd_mark) {
		mark |= _LASTMARK;
		stp->sd_mark = NULL;
	}
	/*
	 * keep track of the original message type and priority
	 */
	pri = bp->b_band;
	type = bp->b_datap->db_type;
	if (type == M_PASSFP) {
		if ((mark & _LASTMARK) && (stp->sd_mark == NULL))
			stp->sd_mark = bp;
		bp->b_flag |= mark & ~_LASTMARK;
		putback(stp, q, bp, pri);
		qbackenable(q, pri);
		mutex_exit(&stp->sd_lock);
		return (EBADMSG);
	}
	ASSERT(type != M_SIG);

	/*
	 * Set this flag so strrput will not generate signals. Need to
	 * make sure this flag is cleared before leaving this routine
	 * else signals will stop being sent.
	 */
	stp->sd_flag |= STRGETINPROG;
	mutex_exit(&stp->sd_lock);

	if (qready())
		queuerun();

	/*
	 * Set HIPRI flag if message is priority.
	 */
	if (type >= QPCTL)
		flg = MSG_HIPRI;
	else
		flg = MSG_BAND;

	/*
	 * First process PROTO or PCPROTO blocks, if any.
	 */
	if (mctl->maxlen >= 0 && type != M_DATA) {
		size_t	n, bcnt;
		char	*ubuf;

		bcnt = mctl->maxlen;
		ubuf = mctl->buf;
		while (bp != NULL && bp->b_datap->db_type != M_DATA) {
			if ((n = MIN(bcnt, bp->b_wptr - bp->b_rptr)) != 0 &&
			    copyout(bp->b_rptr, ubuf, n)) {
				error = EFAULT;
				mutex_enter(&stp->sd_lock);
				/*
				 * clear stream head pri flag based on
				 * first message type
				 */
				if (type >= QPCTL) {
					ASSERT(type == M_PCPROTO);
					stp->sd_flag &= ~STRPRI;
				}
				more = 0;
				freemsg(bp);
				goto getmout;
			}
			ubuf += n;
			bp->b_rptr += n;
			if (bp->b_rptr >= bp->b_wptr) {
				nbp = bp;
				bp = bp->b_cont;
				freeb(nbp);
			}
			ASSERT(n <= bcnt);
			bcnt -= n;
			if (bcnt == 0)
				break;
		}
		mctl->len = mctl->maxlen - bcnt;
	} else
		mctl->len = -1;

	if (bp && bp->b_datap->db_type != M_DATA) {
		/*
		 * More PROTO blocks in msg.
		 */
		more |= MORECTL;
		savemp = bp;
		while (bp && bp->b_datap->db_type != M_DATA) {
			savemptail = bp;
			bp = bp->b_cont;
		}
		savemptail->b_cont = NULL;
	}

	/*
	 * Now process DATA blocks, if any.
	 */
	if (mdata->maxlen >= 0 && bp) {
		/*
		 * struiocopyout will consume a potential zero-length
		 * M_DATA even if uio_resid is zero (by using a
		 * do-while loop).
		 */
		size_t oldresid = uiop->uio_resid;

		bp = struiocopyout(stp, bp, uiop, &error);
		if (error) {
			mutex_enter(&stp->sd_lock);
			/*
			 * clear stream head hi pri flag based on
			 * first message
			 */
			if (type >= QPCTL) {
				ASSERT(type == M_PCPROTO);
				stp->sd_flag &= ~STRPRI;
			}
			more = 0;
			freemsg(bp);
			freemsg(savemp);
			goto getmout;
		}
		/*
		 * (pr == 1) indicates a partial read.
		 */
		if (oldresid > uiop->uio_resid)
			pr = 1;
		mdata->len = mdata->maxlen - uiop->uio_resid;
	} else
		mdata->len = -1;

	if (bp) {			/* more data blocks in msg */
		more |= MOREDATA;
		nbp = bp;
		do {
			nbp->b_datap->db_struioflag = 0;
		} while ((nbp = nbp->b_cont) != NULL);
		if (savemp)
			savemptail->b_cont = bp;
		else
			savemp = bp;
	}

	mutex_enter(&stp->sd_lock);
	if (savemp) {
		if (pr && (savemp->b_datap->db_type == M_DATA) &&
		    msgnodata(savemp)) {
			/*
			 * Avoid queuing a zero-length tail part of
			 * a message. pr=1 indicates that we read some of
			 * the message.
			 */
			freemsg(savemp);
			more &= ~MOREDATA;
			/*
			 * clear stream head hi pri flag based on
			 * first message
			 */
			if (type >= QPCTL) {
				ASSERT(type == M_PCPROTO);
				stp->sd_flag &= ~STRPRI;
			}
		} else {
			savemp->b_band = pri;
			/*
			 * if the message type we are going to putback
			 * is of a different type (hi pri vs normal)
			 * then the original message, clear the
			 * STRPRI flag
			 */
			if (((type >= QPCTL) ? QPCTL : QNORM) !=
			    queclass(savemp)) {
				if (type >= QPCTL) {
					ASSERT(type == M_PCPROTO);
					stp->sd_flag &= ~STRPRI;
				} else {
					/*
					 * The first message was not
					 * a hipri message, but the
					 * one we are about to putback is.
					 * XXX - We do not allow for
					 * hipri messages to be embedded
					 * in the message body.
					 * just force it to same
					 * type as first message
					 */
					ASSERT(type < QPCTL);
					ASSERT(type == M_DATA ||
					    type == M_PROTO);
					ASSERT(savemp->b_datap->db_type ==
					    M_PCPROTO);
					savemp->b_datap->db_type = type;
				}
			}
			if (mark != 0) {
				savemp->b_flag |= mark & ~_LASTMARK;
				if ((mark & _LASTMARK) &&
				    (stp->sd_mark == NULL)) {
					/*
					 * If another marked message arrived
					 * while sd_lock was not held sd_mark
					 * would be non-NULL.
					 */
					stp->sd_mark = savemp;
				}
			}
			putback(stp, q, savemp, pri);
		}
	} else {
		/*
		 * The complete message was consumed.
		 *
		 * If another M_PCPROTO arrived while sd_lock was not held
		 * it would have been discarded since STRPRI was still set.
		 *
		 * Move the MSG*MARKNEXT information
		 * to the stream head just in case
		 * the read queue becomes empty.
		 * clear stream head hi pri flag based on
		 * first message
		 *
		 * If the stream head was at the mark
		 * (STRATMARK) before we dropped sd_lock above
		 * and some data was consumed then we have
		 * moved past the mark thus STRATMARK is
		 * cleared. However, if a message arrived in
		 * strrput during the copyout above causing
		 * STRATMARK to be set we can not clear that
		 * flag.
		 */
		if (type >= QPCTL) {
			ASSERT(type == M_PCPROTO);
			stp->sd_flag &= ~STRPRI;
		}
		if (mark & (MSGMARKNEXT|MSGNOTMARKNEXT|MSGMARK)) {
			if (mark & MSGMARKNEXT) {
				stp->sd_flag &= ~STRNOTATMARK;
				stp->sd_flag |= STRATMARK;
			} else if (mark & MSGNOTMARKNEXT) {
				stp->sd_flag &= ~STRATMARK;
				stp->sd_flag |= STRNOTATMARK;
			} else {
				stp->sd_flag &= ~(STRATMARK|STRNOTATMARK);
			}
		} else if (pr && (old_sd_flag & STRATMARK)) {
			stp->sd_flag &= ~STRATMARK;
		}
	}

	*flagsp = flg;
	*prip = pri;

	/*
	 * Getmsg cleanup processing - if the state of the queue has changed
	 * some signals may need to be sent and/or poll awakened.
	 */
getmout:
	qbackenable(q, pri);

	/*
	 * We dropped the stream head lock above. Send all M_SIG messages
	 * before processing stream head for SIGPOLL messages.
	 */
	ASSERT(MUTEX_HELD(&stp->sd_lock));
	while ((bp = q->q_first) != NULL &&
	    (bp->b_datap->db_type == M_SIG)) {
		/*
		 * sd_lock is held so the content of the read queue can not
		 * change.
		 */
		bp = getq(q);
		ASSERT(bp != NULL && bp->b_datap->db_type == M_SIG);

		mutex_exit(&stp->sd_lock);
		strsignal(stp, *bp->b_rptr, (int32_t)bp->b_band);
		freemsg(bp);
		if (qready())
			queuerun();
		mutex_enter(&stp->sd_lock);
	}

	/*
	 * stream head cannot change while we make the determination
	 * whether or not to send a signal. Drop the flag to allow strrput
	 * to send firstmsgsigs again.
	 */
	stp->sd_flag &= ~STRGETINPROG;

	/*
	 * If the type of message at the front of the queue changed
	 * due to the receive the appropriate signals and pollwakeup events
	 * are generated. The type of changes are:
	 *	Processed a hipri message, q_first is not hipri.
	 *	Processed a band X message, and q_first is band Y.
	 * The generated signals and pollwakeups are identical to what
	 * strrput() generates should the message that is now on q_first
	 * arrive to an empty read queue.
	 *
	 * Note: only strrput will send a signal for a hipri message.
	 */
	if ((bp = q->q_first) != NULL && !(bp->b_flag & MSGNOGET) &&
	    !(stp->sd_flag & STRPRI)) {
		strsigset_t signals = 0;
		strpollset_t pollwakeups = 0;

		if (flg & MSG_HIPRI) {
			/*
			 * Removed a hipri message. Regular data at
			 * the front of  the queue.
			 */
			if (bp->b_band == 0) {
				signals = S_INPUT | S_RDNORM;
				pollwakeups = POLLIN | POLLRDNORM;
			} else {
				signals = S_INPUT | S_RDBAND;
				pollwakeups = POLLIN | POLLRDBAND;
			}
		} else if (pri != bp->b_band) {
			/*
			 * The band is different for the new q_first.
			 */
			if (bp->b_band == 0) {
				signals = S_RDNORM;
				pollwakeups = POLLIN | POLLRDNORM;
			} else {
				signals = S_RDBAND;
				pollwakeups = POLLIN | POLLRDBAND;
			}
		}
		if (stp->sd_sigflags & signals)
			strsendsig(stp->sd_siglist, signals, bp->b_band, 0);

		if (pollwakeups == (POLLIN | POLLRDNORM)) {
			if (stp->sd_rput_opt & SR_POLLIN) {
				stp->sd_rput_opt &= ~SR_POLLIN;
				mutex_exit(&stp->sd_lock);
				pollwakeup(&stp->sd_pollist, pollwakeups);
			} else
				mutex_exit(&stp->sd_lock);
		} else if (pollwakeups != 0) {
			mutex_exit(&stp->sd_lock);
			pollwakeup(&stp->sd_pollist, pollwakeups);
		} else {
			mutex_exit(&stp->sd_lock);
		}
	} else {
		mutex_exit(&stp->sd_lock);
	}

	rvp->r_val1 = more;
	return (error);
#undef	_LASTMARK
}

/*
 * Get the next message from the read queue.  If the message is
 * priority, STRPRI will have been set by strrput().  This flag
 * should be reset only when the entire message at the front of the
 * queue as been consumed.
 *
 * If uiop is NULL all data is returned in mctlp.
 * Note that a NULL uiop implies that FNDELAY and FNONBLOCK are assumed
 * not enabled.
 * The timeout parameter is in milliseconds; -1 for infinity.
 * This routine handles the consolidation private flags:
 *	MSG_IGNERROR	Ignore any stream head error except STPLEX.
 *	MSG_DELAYERROR	Defer the error check until the queue is empty.
 *	MSG_HOLDSIG	Hold signals while waiting for data.
 *	MSG_IPEEK	Only peek at messages.
 *	MSG_DISCARDTAIL	Discard the tail M_DATA part of the message
 *			that doesn't fit.
 *	MSG_NOMARK	If the message is marked leave it on the queue.
 *
 * NOTE: strgetmsg and kstrgetmsg have much of the logic in common.
 */
int
kstrgetmsg(
	struct vnode *vp,
	mblk_t **mctlp,
	struct uio *uiop,
	unsigned char *prip,
	int *flagsp,
	clock_t timout,
	rval_t *rvp)
{
	struct stdata *stp;
	mblk_t *bp, *nbp;
	mblk_t *savemp = NULL;
	mblk_t *savemptail = NULL;
	int flags;
	uint_t old_sd_flag;
	int flg;
	int more = 0;
	int error = 0;
	char first = 1;
	uint_t mark;		/* Contains MSG*MARK and _LASTMARK */
#define	_LASTMARK	0x8000	/* Distinct from MSG*MARK */
	unsigned char pri = 0;
	queue_t *q;
	int	pr = 0;			/* Partial read successful */
	unsigned char type;

	TRACE_1(TR_FAC_STREAMS_FR, TR_KSTRGETMSG_ENTER,
		"kstrgetmsg:%X", vp);

	ASSERT(vp->v_stream);
	stp = vp->v_stream;
	rvp->r_val1 = 0;

	if (stp->sd_sidp != NULL && stp->sd_vnode->v_type != VFIFO)
		if (error = straccess(stp, JCREAD))
			return (error);

	flags = *flagsp;
	/* Fast check of flags before acquiring the lock */
	if (stp->sd_flag & (STRDERR|STPLEX)) {
		if ((stp->sd_flag & STPLEX) ||
		    (flags & (MSG_IGNERROR|MSG_DELAYERROR)) == 0) {
			mutex_enter(&stp->sd_lock);
			error = strgeterr(stp, STRDERR|STPLEX,
					(flags & MSG_IPEEK));
			mutex_exit(&stp->sd_lock);
			if (error != 0)
				return (error);
		}
	}

	switch (flags & (MSG_HIPRI|MSG_ANY|MSG_BAND)) {
	case MSG_HIPRI:
		if (*prip != 0)
			return (EINVAL);
		break;

	case MSG_ANY:
	case MSG_BAND:
		break;

	default:
		return (EINVAL);
	}

retry:
	q = _RD(stp->sd_wrq);
	mutex_enter(&stp->sd_lock);
	old_sd_flag = stp->sd_flag;
	mark = 0;
	for (;;) {
		int done = 0;
		int waitflag;
		int fmode;
		mblk_t *q_first = q->q_first;

		/*
		 * This section of the code operates just like the code
		 * in strgetmsg().  There is a comment there about what
		 * is going on here.
		 */
		if (!(flags & (MSG_HIPRI|MSG_BAND))) {
			/* Asking for normal, band0 data */
			bp = strget(stp, q, uiop, first, &error);
			ASSERT(MUTEX_HELD(&stp->sd_lock));
			if (bp != NULL) {
				if (bp->b_datap->db_type == M_SIG) {
					strsignal_nolock(stp, *bp->b_rptr,
					    (int32_t)bp->b_band);
					continue;
				} else {
					break;
				}
			}
			if (error != 0) {
				goto getmout;
			}
		/*
		 * We can't depend on the value of STRPRI here because
		 * the stream head may be in transit. Therefore, we
		 * must look at the type of the first message to
		 * determine if a high priority messages is waiting
		 */
		} else if ((flags & MSG_HIPRI) && q_first != NULL &&
			    q_first->b_datap->db_type >= QPCTL &&
			    (bp = getq_noenab(q)) != NULL) {
			ASSERT(bp->b_datap->db_type >= QPCTL);
			break;
		} else if ((flags & MSG_BAND) && q_first != NULL &&
			    ((q_first->b_band >= *prip) ||
			    q_first->b_datap->db_type >= QPCTL) &&
			    (bp = getq_noenab(q)) != NULL) {
			/*
			 * Asked for at least band "prip" and got either at
			 * least that band or a hipri message.
			 */
			ASSERT(bp->b_band >= *prip ||
				bp->b_datap->db_type >= QPCTL);
			if (bp->b_datap->db_type == M_SIG) {
				strsignal_nolock(stp, *bp->b_rptr,
				    (int32_t)bp->b_band);
				continue;
			} else {
				break;
			}
		}

		/* No data. Time to sleep? */
		qbackenable(q, 0);

		/*
		 * Delayed error notification?
		 */
		if ((stp->sd_flag & (STRDERR|STPLEX)) &&
		    (flags & (MSG_IGNERROR|MSG_DELAYERROR)) == MSG_DELAYERROR) {
			error = strgeterr(stp, STRDERR|STPLEX,
					(flags & MSG_IPEEK));
			if (error != 0) {
				mutex_exit(&stp->sd_lock);
				return (error);
			}
		}

		/*
		 * If STRHUP or STREOF, return 0 length control and data.
		 * If a read(fd,buf,0) has been done, do not sleep, just
		 * return.
		 *
		 * If mctlp == NULL and uiop == NULL, then the code will
		 * do the strwaitq. This is an understood way of saying
		 * sleep "polling" until a message is received.
		 */
		if ((stp->sd_flag & (STRHUP|STREOF)) ||
		    (uiop != NULL && uiop->uio_resid == 0)) {
			if (mctlp != NULL)
				*mctlp = NULL;
			*flagsp = 0;
			mutex_exit(&stp->sd_lock);
			return (0);
		}

		waitflag = GETWAIT;
		if (flags &
		    (MSG_HOLDSIG|MSG_IGNERROR|MSG_IPEEK|MSG_DELAYERROR)) {
			if (flags & MSG_HOLDSIG)
				waitflag |= STR_NOSIG;
			if (flags & MSG_IGNERROR)
				waitflag |= STR_NOERROR;
			if (flags & MSG_IPEEK)
				waitflag |= STR_PEEK;
			if (flags & MSG_DELAYERROR)
				waitflag |= STR_DELAYERR;
		}
		if (uiop != NULL)
			fmode = uiop->uio_fmode;
		else
			fmode = 0;

		TRACE_2(TR_FAC_STREAMS_FR, TR_KSTRGETMSG_WAIT,
			"kstrgetmsg calls strwaitq:%X, %X",
			vp, uiop);
		if (((error = strwaitq(stp, waitflag, (ssize_t)0,
		    fmode, timout, &done)) != 0) || done) {
			TRACE_2(TR_FAC_STREAMS_FR, TR_KSTRGETMSG_DONE,
				"kstrgetmsg error or done:%X, %X",
				vp, uiop);
			mutex_exit(&stp->sd_lock);
			return (error);
		}
		TRACE_2(TR_FAC_STREAMS_FR, TR_KSTRGETMSG_AWAKE,
			"kstrgetmsg awakes:%X, %X", vp, uiop);
		if (stp->sd_sidp != NULL && stp->sd_vnode->v_type != VFIFO) {
			mutex_exit(&stp->sd_lock);
			if (error = straccess(stp, JCREAD))
				return (error);
			mutex_enter(&stp->sd_lock);
		}
		first = 0;
	}
	ASSERT(bp != NULL);
	/*
	 * Extract any mark information. If the message is not completely
	 * consumed this information will be put in the mblk
	 * that is putback.
	 * If MSGMARKNEXT is set and the message is completely consumed
	 * the STRATMARK flag will be set below. Likewise, if
	 * MSGNOTMARKNEXT is set and the message is
	 * completely consumed STRNOTATMARK will be set.
	 */
	mark = bp->b_flag & (MSGMARK | MSGMARKNEXT | MSGNOTMARKNEXT);
	ASSERT((mark & (MSGMARKNEXT|MSGNOTMARKNEXT)) !=
		(MSGMARKNEXT|MSGNOTMARKNEXT));
	pri = bp->b_band;
	if (mark != 0) {
		/*
		 * If the caller doesn't want the mark return.
		 * Used to implement MSG_WAITALL in sockets.
		 */
		if (flags & MSG_NOMARK) {
			putback(stp, q, bp, pri);
			qbackenable(q, pri);
			mutex_exit(&stp->sd_lock);
			return (EWOULDBLOCK);
		}
		if (bp == stp->sd_mark) {
			mark |= _LASTMARK;
			stp->sd_mark = NULL;
		}
	}

	/*
	 * keep track of the first message type
	 */
	type = bp->b_datap->db_type;

	if (bp->b_datap->db_type == M_PASSFP) {
		if ((mark & _LASTMARK) && (stp->sd_mark == NULL))
			stp->sd_mark = bp;
		bp->b_flag |= mark & ~_LASTMARK;
		putback(stp, q, bp, pri);
		qbackenable(q, pri);
		mutex_exit(&stp->sd_lock);
		return (EBADMSG);
	}
	ASSERT(type != M_SIG);

	if (flags & MSG_IPEEK) {
		/*
		 * Clear any struioflag - we do the uiomove over again
		 * when peeking since it simplifies the code.
		 *
		 * Dup the message and put the original back on the queue.
		 */
		nbp = bp;
		do {
			nbp->b_datap->db_struioflag = 0;
		} while ((nbp = nbp->b_cont) != NULL);

		if ((nbp = dupmsg(bp)) == NULL) {
			/*
			 * Restore the state of the stream head since we
			 * need to drop sd_lock (strwaitbuf is sleeping).
			 */
			size_t size = msgdsize(bp);

			if ((mark & _LASTMARK) && (stp->sd_mark == NULL))
				stp->sd_mark = bp;
			bp->b_flag |= mark & ~_LASTMARK;
			putback(stp, q, bp, pri);
			mutex_exit(&stp->sd_lock);
			error = strwaitbuf(size, BPRI_HI);
			if (error) {
				/*
				 * There is no net change to the queue thus
				 * no need to qbackenable.
				 */
				return (error);
			}
			goto retry;
		}

		if ((mark & _LASTMARK) && (stp->sd_mark == NULL))
			stp->sd_mark = bp;
		bp->b_flag |= mark & ~_LASTMARK;
		putback(stp, q, bp, pri);
		bp = nbp;
	}

	/*
	 * Set this flag so strrput will not generate signals. Need to
	 * make sure this flag is cleared before leaving this routine
	 * else signals will stop being sent.
	 */
	stp->sd_flag |= STRGETINPROG;
	mutex_exit(&stp->sd_lock);

	if (qready())
		queuerun();

	/*
	 * Set HIPRI flag if message is priority.
	 */
	if (type >= QPCTL)
		flg = MSG_HIPRI;
	else
		flg = MSG_BAND;

	/*
	 * First process PROTO or PCPROTO blocks, if any.
	 */
	if (mctlp != NULL && type != M_DATA) {
		mblk_t *nbp;

		*mctlp = bp;
		while (bp->b_cont && bp->b_cont->b_datap->db_type != M_DATA)
			bp = bp->b_cont;
		nbp = bp->b_cont;
		bp->b_cont = NULL;
		bp = nbp;
	}

	if (bp && bp->b_datap->db_type != M_DATA) {
		/*
		 * More PROTO blocks in msg. Will only happen if mctlp is NULL.
		 */
		more |= MORECTL;
		savemp = bp;
		while (bp && bp->b_datap->db_type != M_DATA) {
			savemptail = bp;
			bp = bp->b_cont;
		}
		savemptail->b_cont = NULL;
	}

	/*
	 * Now process DATA blocks, if any.
	 */
	if (uiop == NULL) {
		/* Append data to tail of mctlp */
		if (mctlp != NULL) {
			mblk_t **mpp = mctlp;

			while (*mpp != NULL)
				mpp = &((*mpp)->b_cont);
			*mpp = bp;
			bp = NULL;
		}
	} else if (uiop->uio_resid >= 0 && bp) {
		size_t oldresid = uiop->uio_resid;

		bp = struiocopyout(stp, bp, uiop, &error);
		if (error) {
			if (mctlp != NULL) {
				freemsg(*mctlp);
				*mctlp = NULL;
			} else
				freemsg(savemp);
			mutex_enter(&stp->sd_lock);
			/*
			 * clear stream head hi pri flag based on
			 * first message
			 */
			if (!(flags & MSG_IPEEK) && (type >= QPCTL)) {
				ASSERT(type == M_PCPROTO);
				stp->sd_flag &= ~STRPRI;
			}
			more = 0;
			freemsg(bp);
			goto getmout;
		}
		/*
		 * (pr == 1) indicates a partial read.
		 */
		if (oldresid > uiop->uio_resid)
			pr = 1;
	}

	if (bp) {			/* more data blocks in msg */
		more |= MOREDATA;
		nbp = bp;
		do {
			nbp->b_datap->db_struioflag = 0;
		} while ((nbp = nbp->b_cont) != NULL);
		if (savemp)
			savemptail->b_cont = bp;
		else
			savemp = bp;
	}

	mutex_enter(&stp->sd_lock);
	if (savemp) {
		if (flags & (MSG_IPEEK|MSG_DISCARDTAIL)) {
			/*
			 * When MSG_DISCARDTAIL is set or
			 * when peeking discard any tail. When peeking this
			 * is the tail of the dup that was copied out - the
			 * message has already been putback on the queue.
			 * Return MOREDATA to the caller even though the data
			 * is discarded. This is used by sockets (to
			 * set MSG_TRUNC).
			 */
			freemsg(savemp);
			if (!(flags & MSG_IPEEK) && (type >= QPCTL)) {
				ASSERT(type == M_PCPROTO);
				stp->sd_flag &= ~STRPRI;
			}
		} else if (pr && (savemp->b_datap->db_type == M_DATA) &&
			    msgnodata(savemp)) {
			/*
			 * Avoid queuing a zero-length tail part of
			 * a message. pr=1 indicates that we read some of
			 * the message.
			 */
			freemsg(savemp);
			more &= ~MOREDATA;
			if (type >= QPCTL) {
				ASSERT(type == M_PCPROTO);
				stp->sd_flag &= ~STRPRI;
			}
		} else {
			savemp->b_band = pri;
			if (((type >= QPCTL) ? QPCTL : QNORM) !=
			    queclass(savemp)) {
				/*
				 * If original message was hi pri
				 * clear the STRPRI
				 */
				if (type >= QPCTL) {
					ASSERT(type == M_PCPROTO);
					stp->sd_flag &= ~STRPRI;
				} else {
					/*
					 * XXX - remaining message is a hi pri
					 * message.  We don't support this
					 * so change it to the same message
					 * type as the orginal message
					 */
					ASSERT(type == M_PROTO ||
					    type == M_DATA);
					ASSERT(savemp->b_datap->db_type ==
					    M_PCPROTO);
					savemp->b_datap->db_type = type;
				}
			}
			if (mark != 0) {
				if ((mark & _LASTMARK) &&
				    (stp->sd_mark == NULL)) {
					/*
					 * If another marked message arrived
					 * while sd_lock was not held sd_mark
					 * would be non-NULL.
					 */
					stp->sd_mark = savemp;
				}
				savemp->b_flag |= mark & ~_LASTMARK;
			}
			putback(stp, q, savemp, pri);
		}
	} else if (!(flags & MSG_IPEEK)) {
		/*
		 * The complete message was consumed.
		 *
		 * If another M_PCPROTO arrived while sd_lock was not held
		 * it would have been discarded since STRPRI was still set.
		 *
		 * Move the MSG*MARKNEXT information
		 * to the stream head just in case
		 * the read queue becomes empty.
		 * clear stream head hi pri flag based on
		 * first message
		 *
		 * If the stream head was at the mark
		 * (STRATMARK) before we dropped sd_lock above
		 * and some data was consumed then we have
		 * moved past the mark thus STRATMARK is
		 * cleared. However, if a message arrived in
		 * strrput during the copyout above causing
		 * STRATMARK to be set we can not clear that
		 * flag.
		 * XXX A "perimeter" would help by single-threading strrput,
		 * strread, strgetmsg and kstrgetmsg.
		 */
		if (type >= QPCTL) {
			ASSERT(type == M_PCPROTO);
			stp->sd_flag &= ~STRPRI;
		}
		if (mark & (MSGMARKNEXT|MSGNOTMARKNEXT|MSGMARK)) {
			if (mark & MSGMARKNEXT) {
				stp->sd_flag &= ~STRNOTATMARK;
				stp->sd_flag |= STRATMARK;
			} else if (mark & MSGNOTMARKNEXT) {
				stp->sd_flag &= ~STRATMARK;
				stp->sd_flag |= STRNOTATMARK;
			} else {
				stp->sd_flag &= ~(STRATMARK|STRNOTATMARK);
			}
		} else if (pr && (old_sd_flag & STRATMARK)) {
			stp->sd_flag &= ~STRATMARK;
		}
	}

	*flagsp = flg;
	*prip = pri;

	/*
	 * Getmsg cleanup processing - if the state of the queue has changed
	 * some signals may need to be sent and/or poll awakened.
	 */
getmout:
	qbackenable(q, pri);

	/*
	 * We dropped the stream head lock above. Send all M_SIG messages
	 * before processing stream head for SIGPOLL messages.
	 */
	ASSERT(MUTEX_HELD(&stp->sd_lock));
	while ((bp = q->q_first) != NULL &&
	    (bp->b_datap->db_type == M_SIG)) {
		/*
		 * sd_lock is held so the content of the read queue can not
		 * change.
		 */
		bp = getq(q);
		ASSERT(bp != NULL && bp->b_datap->db_type == M_SIG);

		mutex_exit(&stp->sd_lock);
		strsignal(stp, *bp->b_rptr, (int32_t)bp->b_band);
		freemsg(bp);
		if (qready())
			queuerun();
		mutex_enter(&stp->sd_lock);
	}

	/*
	 * stream head cannot change while we make the determination
	 * whether or not to send a signal. Drop the flag to allow strrput
	 * to send firstmsgsigs again.
	 */
	stp->sd_flag &= ~STRGETINPROG;

	/*
	 * If the type of message at the front of the queue changed
	 * due to the receive the appropriate signals and pollwakeup events
	 * are generated. The type of changes are:
	 *	Processed a hipri message, q_first is not hipri.
	 *	Processed a band X message, and q_first is band Y.
	 * The generated signals and pollwakeups are identical to what
	 * strrput() generates should the message that is now on q_first
	 * arrive to an empty read queue.
	 *
	 * Note: only strrput will send a signal for a hipri message.
	 */
	if ((bp = q->q_first) != NULL && !(bp->b_flag & MSGNOGET) &&
	    !(stp->sd_flag & STRPRI)) {
		strsigset_t signals = 0;
		strpollset_t pollwakeups = 0;

		if (flg & MSG_HIPRI) {
			/*
			 * Removed a hipri message. Regular data at
			 * the front of  the queue.
			 */
			if (bp->b_band == 0) {
				signals = S_INPUT | S_RDNORM;
				pollwakeups = POLLIN | POLLRDNORM;
			} else {
				signals = S_INPUT | S_RDBAND;
				pollwakeups = POLLIN | POLLRDBAND;
			}
		} else if (pri != bp->b_band) {
			/*
			 * The band is different for the new q_first.
			 */
			if (bp->b_band == 0) {
				signals = S_RDNORM;
				pollwakeups = POLLIN | POLLRDNORM;
			} else {
				signals = S_RDBAND;
				pollwakeups = POLLIN | POLLRDBAND;
			}
		}
		if (stp->sd_sigflags & signals)
			strsendsig(stp->sd_siglist, signals, bp->b_band, 0);

		if (pollwakeups == (POLLIN | POLLRDNORM)) {
			if (stp->sd_rput_opt & SR_POLLIN) {
				stp->sd_rput_opt &= ~SR_POLLIN;
				mutex_exit(&stp->sd_lock);
				pollwakeup(&stp->sd_pollist, pollwakeups);
			} else
				mutex_exit(&stp->sd_lock);
		} else if (pollwakeups != 0) {
			mutex_exit(&stp->sd_lock);
			pollwakeup(&stp->sd_pollist, pollwakeups);
		} else {
			mutex_exit(&stp->sd_lock);
		}
	} else {
		mutex_exit(&stp->sd_lock);
	}

	rvp->r_val1 = more;
	return (error);
#undef	_LASTMARK
}

/*
 * Put a message downstream.
 *
 * NOTE: strputmsg and kstrputmsg have much of the logic in common.
 */
int
strputmsg(
	struct vnode *vp,
	struct strbuf *mctl,
	struct strbuf *mdata,
	unsigned char pri,
	int flag,
	int fmode)
{
	struct stdata *stp;
	queue_t *wqp;
	mblk_t *mp;
	ssize_t msgsize;
	ssize_t rmin, rmax;
	int error;
	struct uio uios;
	struct uio *uiop = &uios;
	struct iovec iovs;
	int xpg4 = 0;

	ASSERT(vp->v_stream);
	stp = vp->v_stream;
	wqp = stp->sd_wrq;

	/*
	 * If it is an XPG4 application, we need to send
	 * SIGPIPE below
	 */

	xpg4 = (flag & MSG_XPG4) ? 1 : 0;
	flag &= ~MSG_XPG4;

#ifdef C2_AUDIT
	if (audit_active)
		audit_strputmsg(vp, mctl, mdata, pri, flag, fmode);
#endif

	if (stp->sd_sidp != NULL && stp->sd_vnode->v_type != VFIFO)
		if (error = straccess(stp, JCWRITE))
			return (error);

	/* Fast check of flags before acquiring the lock */
	if (stp->sd_flag & (STWRERR|STRHUP|STPLEX)) {
		mutex_enter(&stp->sd_lock);
		error = strgeterr(stp, STWRERR|STRHUP|STPLEX, 0);
		mutex_exit(&stp->sd_lock);
		if (error != 0) {
			if (!(stp->sd_flag & STPLEX) &&
			    ((stp->sd_wput_opt & SW_SIGPIPE) && xpg4)) {
				/*
				 * Send SIGPIPE only if it has come
				 * from an XPG4 application
				 */
				psignal(ttoproc(curthread), SIGPIPE);
				error = EPIPE;
			}
			return (error);
		}
	}

	/*
	 * Check for legal flag value.
	 */
	switch (flag) {
	case MSG_HIPRI:
		if ((mctl->len < 0) || (pri != 0))
			return (EINVAL);
		break;
	case MSG_BAND:
		break;

	default:
		return (EINVAL);
	}

	TRACE_1(TR_FAC_STREAMS_FR, TR_STRPUTMSG_IN,
		"strputmsg in:q %X", stp->sd_wrq);

	/* get these values from those cached in the stream head */
	rmin = stp->sd_qn_minpsz;
	rmax = stp->sd_qn_maxpsz;

	/*
	 * Make sure ctl and data sizes together fall within the
	 * limits of the max and min receive packet sizes and do
	 * not exceed system limit.
	 */
	ASSERT((rmax >= 0) || (rmax == INFPSZ));
	if (rmax == 0) {
		return (ERANGE);
	}
	/*
	 * Use the MAXIMUM of sd_maxblk and q_maxpsz.
	 * Needed to prevent partial failures in the strmakedata loop.
	 */
	if (stp->sd_maxblk != INFPSZ && rmax != INFPSZ && rmax < stp->sd_maxblk)
		rmax = stp->sd_maxblk;

	if ((msgsize = mdata->len) < 0) {
		msgsize = 0;
		rmin = 0;	/* no range check for NULL data part */
	}
	if ((msgsize < rmin) ||
	    ((msgsize > rmax) && (rmax != INFPSZ)) ||
	    (mctl->len > strctlsz)) {
		return (ERANGE);
	}

	/*
	 * Setup uio and iov for data part
	 */
	iovs.iov_base = mdata->buf;
	iovs.iov_len = msgsize;
	uios.uio_iov = &iovs;
	uios.uio_iovcnt = 1;
	uios.uio_loffset = 0;
	uios.uio_segflg = UIO_USERSPACE;
	uios.uio_fmode = fmode;
	uios.uio_resid = msgsize;
	uios.uio_offset = 0;

	/* Ignore flow control in strput for HIPRI */
	if (flag & MSG_HIPRI)
		flag |= MSG_IGNFLOW;

	for (;;) {
		int done = 0;

		/*
		 * strput will always free the ctl mblk - even when strput
		 * fails.
		 */
		if ((error = strmakectl(mctl, flag, fmode, &mp)) != 0) {
			TRACE_3(TR_FAC_STREAMS_FR, TR_STRPUTMSG_OUT,
				"strputmsg out:q %X out %d error %d",
				stp->sd_wrq, 1, error);
			return (error);
		}
		/*
		 * Verify that the whole message can be transferred by
		 * strput.
		 */
		ASSERT(stp->sd_maxblk == INFPSZ ||
			stp->sd_maxblk >= mdata->len);

		msgsize = mdata->len;
		error = strput(stp, mp, uiop, &msgsize, 0, pri, flag);
		mdata->len = msgsize;

		if (error == 0)
			break;

		if (error != EWOULDBLOCK)
			goto out;

		mutex_enter(&stp->sd_lock);
		/*
		 * Check for a missed wakeup.
		 * Needed since strput did not hold sd_lock across
		 * the canputnext.
		 */
		if (bcanputnext(wqp, pri)) {
			/* Try again */
			mutex_exit(&stp->sd_lock);
			continue;
		}
		TRACE_2(TR_FAC_STREAMS_FR, TR_STRPUTMSG_WAIT,
			"strputmsg wait:q %X waits pri %d", stp->sd_wrq, pri);
		if (((error = strwaitq(stp, WRITEWAIT, (ssize_t)0, fmode, -1,
		    &done)) != 0) || done) {
			mutex_exit(&stp->sd_lock);
			TRACE_3(TR_FAC_STREAMS_FR, TR_STRPUTMSG_OUT,
				"strputmsg out:q %X out %d error %d",
				stp->sd_wrq, 0, error);
			return (error);
		}
		TRACE_1(TR_FAC_STREAMS_FR, TR_STRPUTMSG_WAKE,
			"strputmsg wake:q %X wakes", stp->sd_wrq);
		mutex_exit(&stp->sd_lock);
		if (stp->sd_sidp != NULL && stp->sd_vnode->v_type != VFIFO)
			if (error = straccess(stp, JCWRITE))
				return (error);
	}
out:
	/*
	 * For historic reasons, applications expect EAGAIN
	 * when data mblk could not be allocated. so change
	 * ENOMEM back to EAGAIN
	 */
	if (error == ENOMEM)
		error = EAGAIN;
	TRACE_3(TR_FAC_STREAMS_FR, TR_STRPUTMSG_OUT,
		"strputmsg out:q %X out %d error %d", stp->sd_wrq, 2, error);
	return (error);
}

/*
 * Put a message downstream.
 * Can send only an M_PROTO/M_PCPROTO by passing in a NULL uiop.
 * The fmode flag (NDELAY, NONBLOCK) is the or of the flags in the uio
 * and the fmode parameter.
 *
 * This routine handles the consolidation private flags:
 *	MSG_IGNERROR	Ignore any stream head error except STPLEX.
 *	MSG_HOLDSIG	Hold signals while waiting for data.
 *	MSG_IGNFLOW	Don't check streams flow control.
 *
 * NOTE: strputmsg and kstrputmsg have much of the logic in common.
 */
int
kstrputmsg(
	struct vnode *vp,
	mblk_t *mctl,
	struct uio *uiop,
	ssize_t msgsize,
	unsigned char pri,
	int flag,
	int fmode)
{
	struct stdata *stp;
	queue_t *wqp;
	ssize_t rmin, rmax;
	int error;

	ASSERT(vp->v_stream);
	stp = vp->v_stream;
	wqp = stp->sd_wrq;
#ifdef C2_AUDIT
	if (audit_active)
		audit_strputmsg(vp, NULL, NULL, pri, flag, fmode);
#endif

	if (stp->sd_sidp != NULL && stp->sd_vnode->v_type != VFIFO) {
		if (error = straccess(stp, JCWRITE)) {
			freemsg(mctl);
			return (error);
		}
	}

	/* Fast check of flags before acquiring the lock */
	if (stp->sd_flag & (STWRERR|STRHUP|STPLEX)) {
		if ((stp->sd_flag & STPLEX) ||
		    (flag & MSG_IGNERROR) == 0) {
			mutex_enter(&stp->sd_lock);
			error = strgeterr(stp, STWRERR|STRHUP|STPLEX, 0);
			mutex_exit(&stp->sd_lock);
			if (error != 0) {
				if (!(stp->sd_flag & STPLEX) &&
				    (stp->sd_wput_opt & SW_SIGPIPE))
					psignal(ttoproc(curthread), SIGPIPE);
				freemsg(mctl);
				return (error);
			}
		}
	}

	/*
	 * Check for legal flag value.
	 */
	switch (flag & (MSG_HIPRI|MSG_BAND|MSG_ANY)) {
	case MSG_HIPRI:
		if ((mctl == NULL) || (pri != 0)) {
			freemsg(mctl);
			return (EINVAL);
		}
		break;
	case MSG_BAND:
		break;

	default:
		freemsg(mctl);
		return (EINVAL);
	}

	TRACE_1(TR_FAC_STREAMS_FR, TR_KSTRPUTMSG_IN,
		"kstrputmsg in:q %X", stp->sd_wrq);

	/* get these values from those cached in the stream head */
	rmin = stp->sd_qn_minpsz;
	rmax = stp->sd_qn_maxpsz;

	/*
	 * Make sure ctl and data sizes together fall within the
	 * limits of the max and min receive packet sizes and do
	 * not exceed system limit.
	 */
	ASSERT((rmax >= 0) || (rmax == INFPSZ));
	if (rmax == 0) {
		freemsg(mctl);
		return (ERANGE);
	}
	/*
	 * Use the MAXIMUM of sd_maxblk and q_maxpsz.
	 * Needed to prevent partial failures in the strmakedata loop.
	 */
	if (stp->sd_maxblk != INFPSZ && rmax != INFPSZ && rmax < stp->sd_maxblk)
		rmax = stp->sd_maxblk;

	if (uiop == NULL) {
		msgsize = -1;
		rmin = -1;	/* no range check for NULL data part */
	} else {
		/* Use uio flags as well as the fmode parameter flags */
		fmode |= uiop->uio_fmode;

		if ((msgsize < rmin) ||
		    ((msgsize > rmax) && (rmax != INFPSZ))) {
			freemsg(mctl);
			return (ERANGE);
		}
	}

	/* Ignore flow control in strput for HIPRI */
	if (flag & MSG_HIPRI)
		flag |= MSG_IGNFLOW;

	for (;;) {
		int done = 0;
		int waitflag;
		mblk_t *mp;

		/*
		 * strput will always free the ctl mblk - even when strput
		 * fails. If MSG_IGNFLOW is set then any error returned
		 * will cause us to break the loop, so we don't need a copy
		 * of the message. If MSG_IGNFLOW is not set, then we can
		 * get hit by flow control and be forced to try again. In
		 * this case we need to have a copy of the message. We
		 * do this using copymsg since the message may get modified
		 * by something below us.
		 *
		 * We've observed that many TPI providers do not check db_ref
		 * on the control messages but blindly reuse them for the
		 * T_OK_ACK/T_ERROR_ACK. Thus using copymsg is more
		 * friendly to such providers than using dupmsg. Also, note
		 * that sockfs uses MSG_IGNFLOW for all TPI control messages.
		 * Only data messages are subject to flow control, hence
		 * subject to this copymsg.
		 */
		if (flag & MSG_IGNFLOW) {
			mp = mctl;
			mctl = NULL;
		} else {
			do {
				/*
				 * If a message has a free pointer, the message
				 * must be dupmsg to maintain this pointer.
				 * Code using this facility must be sure
				 * that modules below will not change the
				 * contents of the dblk without checking db_ref
				 * first. If db_ref is > 1, then the module
				 * needs to do a copymsg first. Otherwise,
				 * the contents of the dblk may become
				 * inconsistent because the freesmg/freeb below
				 * may end up calling atomic_add_32_nv.
				 * The atomic_add_32_nv in freeb (accessing
				 * all of db_ref, db_type, db_flags, and
				 * db_struioflag) does not prevent other threads
				 * from concurrently trying to modify e.g.
				 * db_type.
				 */
				if (mctl->b_datap->db_frtnp != NULL)
					mp = dupmsg(mctl);
				else
					mp = copymsg(mctl);

				if (mp != NULL || mctl == NULL)
					break;

				error = strwaitbuf(msgdsize(mctl), BPRI_MED);
				if (error) {
					freemsg(mctl);
					return (error);
				}
			} while (mp == NULL);
		}
		/*
		 * Verify that all of msgsize can be transferred by
		 * strput.
		 */
		ASSERT(stp->sd_maxblk == INFPSZ ||
			stp->sd_maxblk >= msgsize);
		error = strput(stp, mp, uiop, &msgsize, 0, pri, flag);
		if (error == 0)
			break;

		if (error != EWOULDBLOCK)
			goto out;

		/*
		 * IF MSG_IGNFLOW is set we should have broken out of loop
		 * above.
		 */
		ASSERT(!(flag & MSG_IGNFLOW));
		mutex_enter(&stp->sd_lock);
		/*
		 * Check for a missed wakeup.
		 * Needed since strput did not hold sd_lock across
		 * the canputnext.
		 */
		if (bcanputnext(wqp, pri)) {
			/* Try again */
			mutex_exit(&stp->sd_lock);
			continue;
		}
		TRACE_2(TR_FAC_STREAMS_FR, TR_KSTRPUTMSG_WAIT,
			"kstrputmsg wait:q %X waits pri %d", stp->sd_wrq, pri);

		waitflag = WRITEWAIT;
		if (flag & (MSG_HOLDSIG|MSG_IGNERROR)) {
			if (flag & MSG_HOLDSIG)
				waitflag |= STR_NOSIG;
			if (flag & MSG_IGNERROR)
				waitflag |= STR_NOERROR;
		}
		if (((error = strwaitq(stp, waitflag,
		    (ssize_t)0, fmode, -1, &done)) != 0) || done) {
			mutex_exit(&stp->sd_lock);
			TRACE_3(TR_FAC_STREAMS_FR, TR_KSTRPUTMSG_OUT,
				"kstrputmsg out:q %X out %d error %d",
				stp->sd_wrq, 0, error);
			freemsg(mctl);
			return (error);
		}
		TRACE_1(TR_FAC_STREAMS_FR, TR_KSTRPUTMSG_WAKE,
			"kstrputmsg wake:q %X wakes", stp->sd_wrq);
		mutex_exit(&stp->sd_lock);
		if (stp->sd_sidp != NULL && stp->sd_vnode->v_type != VFIFO) {
			if (error = straccess(stp, JCWRITE)) {
				freemsg(mctl);
				return (error);
			}
		}
	}
out:
	freemsg(mctl);
	/*
	 * For historic reasons, applications expect EAGAIN
	 * when data mblk could not be allocated. so change
	 * ENOMEM back to EAGAIN
	 */
	if (error == ENOMEM)
		error = EAGAIN;
	TRACE_3(TR_FAC_STREAMS_FR, TR_KSTRPUTMSG_OUT,
		"kstrputmsg out:q %X out %d error %d", stp->sd_wrq, 2, error);
	return (error);
}

/*
 * Determines whether the necessary conditions are set on a stream
 * for it to be readable, writeable, or have exceptions.
 *
 * strpoll handles the consolidation private events:
 *	POLLNOERR	Do not return POLLERR even if there are stream
 *			head errors.
 *			Used by sockfs.
 *	POLLRDDATA	Do not return POLLIN unless at least one message on
 *			the queue contains one or more M_DATA mblks. Thus
 *			when this flag is set a queue with only
 *			M_PROTO/M_PCPROTO mblks does not return POLLIN.
 *			Used by sockfs to ignore T_EXDATA_IND messages.
 *
 * Note: POLLRDDATA assumes that synch streams only return messages with
 * an M_DATA attached (i.e. not messages consisting of only
 * an M_PROTO/M_PCPROTO part).
 */
int
strpoll(
	struct stdata *stp,
	short events_arg,
	int anyyet,
	short *reventsp,
	struct pollhead **phpp)
{
	int events = (ushort_t)events_arg;
	int retevents = 0;
	mblk_t *mp;
	qband_t *qbp;
	long sd_flags = stp->sd_flag;
	int headlocked = 0;

	/*
	 * For performance, a single 'if' tests for most possible edge
	 * conditions in one shot
	 */
	if (sd_flags & (STPLEX | STRDERR | STWRERR)) {
		if (sd_flags & STPLEX) {
			*reventsp = POLLNVAL;
			return (EINVAL);
		}
		if (((events & (POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI)) &&
		    (sd_flags & STRDERR)) ||
		    ((events & (POLLOUT | POLLWRNORM | POLLWRBAND)) &&
		    (sd_flags & STWRERR))) {
			if (!(events & POLLNOERR)) {
				*reventsp = POLLERR;
				return (0);
			}
		}
	}
	if (sd_flags & STRHUP) {
		retevents |= POLLHUP;
	} else if (events & (POLLWRNORM | POLLWRBAND)) {
		queue_t *tq;
		queue_t	*qp = stp->sd_wrq;

		claimstr(qp);
		/* Find next module forward that has a service procedure */
		tq = qp->q_next->q_nfsrv;
		ASSERT(tq != NULL);

		polllock(&stp->sd_pollist, QLOCK(tq));
		if (events & POLLWRNORM) {
			queue_t *sqp;

			if (tq->q_flag & QFULL)
				/* ensure backq svc procedure runs */
				tq->q_flag |= QWANTW;
			else if ((sqp = stp->sd_struiowrq) != NULL) {
				/* Check sync stream barrier write q */
				mutex_exit(QLOCK(tq));
				polllock(&stp->sd_pollist, QLOCK(sqp));
				if (sqp->q_flag & QFULL)
					/* ensure pollwakeup() is done */
					sqp->q_flag |= QWANTWSYNC;
				else
					retevents |= POLLOUT;
				/* More write events to process ??? */
				if (! (events & POLLWRBAND)) {
					mutex_exit(QLOCK(sqp));
					releasestr(qp);
					goto chkrd;
				}
				mutex_exit(QLOCK(sqp));
				polllock(&stp->sd_pollist, QLOCK(tq));
			} else
				retevents |= POLLOUT;
		}
		if (events & POLLWRBAND) {
			qbp = tq->q_bandp;
			if (qbp) {
				while (qbp) {
					if (qbp->qb_flag & QB_FULL)
						qbp->qb_flag |= QB_WANTW;
					else
						retevents |= POLLWRBAND;
					qbp = qbp->qb_next;
				}
			} else {
				retevents |= POLLWRBAND;
			}
		}
		mutex_exit(QLOCK(tq));
		releasestr(qp);
	}
chkrd:
	if (sd_flags & STRPRI) {
		retevents |= (events & POLLPRI);
	} else if (events & (POLLRDNORM | POLLRDBAND | POLLIN)) {
		queue_t	*qp = _RD(stp->sd_wrq);
		int normevents = (events & (POLLIN | POLLRDNORM));

		/*
		 * Note: Need to do polllock() here since ps_lock may be
		 * held. See bug 4191544.
		 */
		polllock(&stp->sd_pollist, &stp->sd_lock);
		headlocked = 1;
		mp = qp->q_first;
		while (mp) {
			/*
			 * For POLLRDDATA we scan b_cont and b_next until we
			 * find an M_DATA.
			 */
			if ((events & POLLRDDATA) &&
			    mp->b_datap->db_type != M_DATA) {
				mblk_t *nmp = mp->b_cont;

				while (nmp != NULL &&
				    nmp->b_datap->db_type != M_DATA)
					nmp = nmp->b_cont;
				if (nmp == NULL) {
					mp = mp->b_next;
					continue;
				}
			}
			if (mp->b_band == 0)
				retevents |= normevents;
			else
				retevents |= (events & (POLLIN | POLLRDBAND));

			/*
			 * MSGNOGET is really only to make poll return
			 * the intended events when the module is really
			 * holding onto the data.  Yeah, it's a hack and
			 * we need a better solution.
			 */
			if (mp->b_flag & MSGNOGET)
				mp = mp->b_next;
			else
				break;
		}
		if (! (retevents & normevents) &&
		    (stp->sd_wakeq & RSLEEP)) {
			/*
			 * Sync stream barrier read queue has data.
			 */
			retevents |= normevents;
		}
		/* Treat eof as normal data */
		if (sd_flags & STREOF)
			retevents |= normevents;
	}

	*reventsp = (short)retevents;
	if (retevents) {
		if (headlocked)
			mutex_exit(&stp->sd_lock);
		return (0);
	}

	/*
	 * If poll() has not found any events yet, set up event cell
	 * to wake up the poll if a requested event occurs on this
	 * stream.  Check for collisions with outstanding poll requests.
	 */
	if (!anyyet) {
		*phpp = &stp->sd_pollist;
		if (headlocked == 0) {
			polllock(&stp->sd_pollist, &stp->sd_lock);
			headlocked = 1;
		}
		stp->sd_rput_opt |= SR_POLLIN;
	}
	if (headlocked)
		mutex_exit(&stp->sd_lock);
	return (0);
}

/*
 * The purpose of putback() is to assure sleeping polls/reads
 * are awakened when there are no new messages arriving at the,
 * stream head, and a message is placed back on the read queue.
 *
 * sd_lock must be held when messages are placed back on stream
 * head.  (getq() holds sd_lock when it removes messages from
 * the queue)
 */

static void
putback(struct stdata *stp, queue_t *q, mblk_t *bp, int band)
{
	ASSERT(MUTEX_HELD(&stp->sd_lock));
	(void) putbq(q, bp);
	/*
	 * A message may have come in when the sd_lock was dropped in the
	 * calling routine. If this is the case and STR*ATMARK info was
	 * received, need to move that from the stream head to the q_last
	 * so that SIOCATMARK can return the proper value.
	 */
	if (stp->sd_flag & (STRATMARK | STRNOTATMARK)) {
		if (stp->sd_flag & STRATMARK) {
			q->q_last->b_flag &= ~MSGNOTMARKNEXT;
			q->q_last->b_flag |= MSGMARKNEXT;
			stp->sd_flag &= ~STRATMARK;
		} else {
			q->q_last->b_flag &= ~MSGMARKNEXT;
			q->q_last->b_flag |= MSGNOTMARKNEXT;
			stp->sd_flag &= ~STRNOTATMARK;
		}
	}

#ifdef	DEBUG
	/*
	 * Make sure that the flags are not messed up.
	 */
	{
		mblk_t *mp;
		mp = q->q_last;
		while (mp != NULL) {
			ASSERT((mp->b_flag & (MSGMARKNEXT|MSGNOTMARKNEXT)) !=
			    (MSGMARKNEXT|MSGNOTMARKNEXT));
			mp = mp->b_cont;
		}
	}
#endif
	if (q->q_first == bp) {
		if (stp->sd_flag & RSLEEP) {
			stp->sd_flag &= ~RSLEEP;
			cv_broadcast(&q->q_wait);
		}
		if (stp->sd_flag & STRPRI) {
			mutex_exit(&stp->sd_lock);
			pollwakeup(&stp->sd_pollist, POLLPRI);
		} else {
			if (band == 0) {
				if (stp->sd_rput_opt & SR_POLLIN) {
					stp->sd_rput_opt &= ~SR_POLLIN;
					mutex_exit(&stp->sd_lock);
					pollwakeup(&stp->sd_pollist,
						POLLIN | POLLRDNORM);
				} else
					mutex_exit(&stp->sd_lock);
			} else {
				mutex_exit(&stp->sd_lock);
				pollwakeup(&stp->sd_pollist,
				    POLLIN | POLLRDBAND);
			}
		}
		mutex_enter(&stp->sd_lock);
	}
}
/*
 * Return the held vnode attached to the stream head of a
 * given queue
 * It is the responsibility of the calling routine to ensure
 * that the queue does not go away (e.g. pop).
 */
vnode_t *
strq2vp(queue_t *qp)
{
	vnode_t *vp;
	vp = STREAM(qp)->sd_vnode;
	ASSERT(vp != NULL);
	VN_HOLD(vp);
	return (vp);
}

/*
 * return the stream head write queue for the given vp
 * It is the responsibility of the calling routine to ensure
 * that the stream or vnode do not close.
 */
queue_t *
strvp2wq(vnode_t *vp)
{
	ASSERT(vp->v_stream != NULL);
	return (vp->v_stream->sd_wrq);
}

/*
 * pollwakeup stream head
 * It is the responsibility of the calling routine to ensure
 * that the stream or vnode do not close.
 */
void
strpollwakeup(vnode_t *vp, short event)
{
	ASSERT(vp->v_stream);
	pollwakeup(&vp->v_stream->sd_pollist, event);
}

/*
 *  No activity allowed on streams
 *  Input: 2 write driver end write queues
 *  Return 0 if error
 *  If wrq1 == wrq2 then we are doing a loop back,
 *  otherwise just mate two queues.
 *  If these queues are not the stream head they must have
 *  a service procedure
 *  It is up to caller to ensure that neither queue goes away.
 *  XXX str_mate() and str_unmate() should be moved to new file kstream.c
 *  XXX these routines need to be a little more general
 */
int
str_mate(queue_t *wrq1, queue_t *wrq2)
{
	/*
	 * Loop back?
	 */
	if (wrq2 == 0 || wrq1 == wrq2) {
		/*
		 * driver end of stream?
		 */
		if (! (wrq1->q_flag & QEND))
			return (EINVAL);

		ASSERT(wrq1->q_next == 0);	/* sanity */

		/*
		 * connect write queue to read queue
		 */
		wrq1->q_next = _RD(wrq1);

		/*
		 * If write queue does not have a service routine,
		 * assign the forward service procedure from the
		 * read queue
		 */

		if (! wrq1->q_qinfo->qi_srvp)
			wrq1->q_nfsrv = _RD(wrq1)->q_nfsrv;
		/*
		 * set back service procedure..
		 * XXX - note back service procedure is not implemented
		 * this may cause a race condition, breaking it
		 * a bit more.
		 */
		_RD(wrq1)->q_nbsrv = wrq1;
	} else {
		/*
		 * driver end of stream?
		 */
		if (! (wrq1->q_flag & QEND))
			return (EINVAL);
		if (! (wrq2->q_flag & QEND))
			return (EINVAL);

		ASSERT(wrq1->q_next == NULL);	/* sanity */
		ASSERT(wrq2->q_next == NULL);	/* sanity */


		/*
		 * if first queue is a stream head, second must
		 * must also be one
		 */
		if (! (_RD(wrq1)->q_flag & QEND)) {
			if (_RD(wrq2)->q_flag & QEND)
				return (EINVAL);
		} else if (! (_RD(wrq2)->q_flag & QEND))
			return (EINVAL);
		/*
		 * Twist the stream head queues so that the write queue
		 * points to the other stream's read queue.
		 */
		wrq1->q_next = _RD(wrq2);
		wrq2->q_next = _RD(wrq1);

		if (! wrq1->q_qinfo->qi_srvp)
			wrq1->q_nfsrv = _RD(wrq2)->q_nfsrv;
		if (! wrq2->q_qinfo->qi_srvp)
			wrq2->q_nfsrv = _RD(wrq1)->q_nfsrv;

		/*
		 * Nothing really uses the back service routines,
		 * but fill them in for completeness
		 */

		_RD(wrq1)->q_nbsrv = wrq2;
		_RD(wrq2)->q_nbsrv = wrq1;

		SETMATED(STREAM(wrq1), STREAM(wrq2));
	}
	return (0);
}
/*
 * XXX There are lot's of problems with this
 * There are several race conditions that must be addressed
 * (e.g. what happens when some other thread is in the other
 * queues q and we unmate.  would freezing the stream be enough?)
 * This routine may only be called from a modules close or service
 * routines.
 */
int
str_unmate(queue_t *wrq1, queue_t *wrq2)
{
	if (! (wrq2->q_flag & QEND) && (wrq1->q_flag & QEND))
		return (EINVAL);

	wrq1->q_next = 0;
	_RD(wrq1)->q_nbsrv = NULL;
	wrq1->q_nfsrv = wrq1;

	if (wrq1 == wrq2)
		return (0);

	wrq2->q_next = 0;
	_RD(wrq2)->q_nbsrv = NULL;
	wrq2->q_nfsrv = wrq2;

	SETUNMATED(STREAM(wrq1), STREAM(wrq2));
	return (0);
}
/*
 * XXX will go away when console is correctly fixed.
 * Clean up the console PIDS, from previous I_SETSIG,
 * called only for cnopen which never calls strclean().
 */
void
str_cn_clean(struct vnode *vp)
{
	strsig_t *ssp, *pssp, *tssp;
	struct stdata *stp;
	struct pid  *pidp;
	int update = 0;

	ASSERT(vp->v_stream);
	stp = vp->v_stream;
	pssp = NULL;
	mutex_enter(&stp->sd_lock);
	ssp = stp->sd_siglist;
	while (ssp) {
		mutex_enter(&pidlock);
		pidp = ssp->ss_pidp;
		/*
		 * Get rid of PID if the proc is gone.
		 */
		if (pidp->pid_prinactive) {
			tssp = ssp->ss_next;
			if (pssp)
				pssp->ss_next = tssp;
			else
				stp->sd_siglist = tssp;
			ASSERT(pidp->pid_ref <= 1);
			PID_RELE(ssp->ss_pidp);
			mutex_exit(&pidlock);
			kmem_cache_free(strsig_cache, ssp);
			update = 1;
			ssp = tssp;
			continue;
		} else
			mutex_exit(&pidlock);
		pssp = ssp;
		ssp = ssp->ss_next;
	}
	if (update) {
		stp->sd_sigflags = 0;
		for (ssp = stp->sd_siglist; ssp; ssp = ssp->ss_next)
			stp->sd_sigflags |= ssp->ss_events;
	}
	mutex_exit(&stp->sd_lock);
}

/*
 * Return B_TRUE if there is data in the message, B_FALSE otherwise.
 */
static boolean_t
msghasdata(mblk_t *bp)
{
	for (; bp; bp = bp->b_cont)
		if (bp->b_datap->db_type == M_DATA) {
			ASSERT(bp->b_wptr >= bp->b_rptr);
			if (bp->b_wptr > bp->b_rptr)
				return (B_TRUE);
		}
	return (B_FALSE);
}
