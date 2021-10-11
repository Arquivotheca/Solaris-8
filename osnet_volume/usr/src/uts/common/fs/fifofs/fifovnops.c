/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All rights reserved.  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fifovnops.c	1.93	99/08/17 SMI"

/*
 * FIFOFS file system vnode operations.  This file system
 * type supports STREAMS-based pipes and FIFOs.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/strsubr.h>
#include <sys/stream.h>
#include <sys/strredir.h>
#include <sys/fs/fifonode.h>
#include <sys/fs/namenode.h>
#include <sys/conf.h>
#include <sys/stropts.h>
#include <sys/proc.h>
#include <sys/unistd.h>
#include <sys/statvfs.h>
#include <sys/debug.h>
#include <fs/fs_subr.h>
#include <sys/filio.h>
#include <sys/termio.h>
#include <sys/ddi.h>
#include <sys/vtrace.h>


/*
 * Define the routines/data structures used in this file.
 */
static int fifo_read(vnode_t *, uio_t *, int, cred_t *);
static int fifo_write(vnode_t *, uio_t *, int, cred_t *);
static int fifo_getattr(vnode_t *, vattr_t *, int, cred_t *);
static int fifo_setattr(vnode_t *, vattr_t *, int, cred_t *);
static int fifo_realvp(vnode_t *, vnode_t **);
static int fifo_access(vnode_t *, int, int, cred_t *);
static int fifo_fid(vnode_t *, fid_t *);
static int fifo_fsync(vnode_t *, int, cred_t *);
static int fifo_seek(vnode_t *, offset_t, offset_t *);
static int fifo_ioctl(vnode_t *, int, intptr_t, int, cred_t *, int *);
static int fifo_fastioctl(vnode_t *, int, intptr_t, int, cred_t *, int *);
static int fifo_strioctl(vnode_t *, int, intptr_t, int, cred_t *, int *);
static int fifo_poll(vnode_t *, short, int, short *, pollhead_t **);
static int fifo_pathconf(vnode_t *, int, ulong_t *, cred_t *);
static void fifo_inactive(vnode_t *, cred_t *);
static void fifo_rwlock(vnode_t *, int);
static void fifo_rwunlock(vnode_t *, int);
static int fifo_setsecattr(struct vnode *, vsecattr_t *, int, struct cred *);
static int fifo_getsecattr(struct vnode *, vsecattr_t *, int, struct cred *);

/*
 * Define the data structures external to this file.
 */
extern	dev_t	fifodev;
extern struct qinit fifo_stwdata;
extern struct qinit fifo_strdata;
extern kmutex_t ftable_lock;

struct  streamtab fifoinfo = { &fifo_strdata, &fifo_stwdata, NULL, NULL };

struct vnodeops fifo_vnodeops = {
	fifo_open,
	fifo_close,
	fifo_read,
	fifo_write,
	fifo_ioctl,
	fs_setfl,
	fifo_getattr,
	fifo_setattr,
	fifo_access,
	fs_nosys,	/* lookup */
	fs_nosys,	/* create */
	fs_nosys,	/* remove */
	fs_nosys,	/* link */
	fs_nosys,	/* rename */
	fs_nosys,	/* mkdir */
	fs_nosys,	/* rmdir */
	fs_nosys,	/* readdir */
	fs_nosys,	/* symlink */
	fs_nosys,	/* readlink */
	fifo_fsync,
	fifo_inactive,
	fifo_fid,
	fifo_rwlock,
	fifo_rwunlock,
	fifo_seek,
	fs_cmp,
	fs_frlock,
	fs_nosys,	/* space */
	fifo_realvp,
	fs_nosys,	/* getpage */
	fs_nosys,	/* putpage */
	fs_nosys_map,	/* mmap */
	fs_nosys_addmap,	/* addmap */
	fs_nosys,	/* delmap */
	fifo_poll,
	fs_nosys,	/* dump */
	fifo_pathconf,
	fs_nosys,	/* pageio */
	fs_nosys,	/* dumpctl */
	fs_nodispose,
	fifo_setsecattr,
	fifo_getsecattr,
	fs_shrlock	/* shrlock */
};

/*
 * Return the fifoinfo structure.
 */
struct streamtab *
fifo_getinfo()
{
	return (&fifoinfo);
}



/*
 * Open and stream a FIFO.
 * If this is the first open of the file (FIFO is not streaming),
 * initialize the fifonode and attach a stream to the vnode.
 *
 * Each end of a fifo must be synchronized with the other end.
 * If not, the mated end may complete an open, I/O, close sequence
 * before the end waiting in open ever wakes up.
 * Note: namefs pipes come through this routine too.
 */
int
fifo_open(vnode_t **vpp, int flag, cred_t *crp)
{
	vnode_t		*vp		= *vpp;
	fifonode_t	*fnp		= VTOF(vp);
	fifolock_t	*fn_lock	= fnp->fn_lock;
	int		error;

	ASSERT(vp->v_type == VFIFO);
	ASSERT(vp->v_op == &fifo_vnodeops);

	mutex_enter(&fn_lock->flk_lock);
	/*
	 * If we are the first reader, wake up any writers that
	 * may be waiting around.  wait for all of them to
	 * wake up before proceeding (i.e. fn_wsynccnt == 0)
	 */
	if (flag & FREAD) {
		fnp->fn_rcnt++;		/* record reader present */
		if (! (fnp->fn_flag & ISPIPE))
			fnp->fn_rsynccnt++;	/* record reader in open */
	}

	/*
	 * If we are the first writer, wake up any readers that
	 * may be waiting around.  wait for all of them to
	 * wake up before proceeding (i.e. fn_rsynccnt == 0)
	 */

	if (flag & FWRITE) {
		fnp->fn_wcnt++;		/* record writer present */
		if (! (fnp->fn_flag & ISPIPE))
			fnp->fn_wsynccnt++;	/* record writer in open */
	}
	/*
	 * fifo_stropen will take care of twisting the queues on the first
	 * open.  The 1 being passed in means twist the queues on the first
	 * open.
	 */
	error = fifo_stropen(vpp, flag, crp, 1, 1);
	/*
	 * fifo_stropen() could have replaced vpp
	 * since fifo's are the only thing we need to sync up,
	 * everything else just returns;
	 * Note: don't need to hold lock since ISPIPE can't change
	 * and both old and new vp need to be pipes
	 */
	ASSERT(MUTEX_HELD(&VTOF(*vpp)->fn_lock->flk_lock));
	if (fnp->fn_flag & ISPIPE) {
		ASSERT(VTOF(*vpp)->fn_flag & ISPIPE);
		ASSERT(VTOF(*vpp)->fn_rsynccnt == 0);
		ASSERT(VTOF(*vpp)->fn_rsynccnt == 0);
		/*
		 * XXX note: should probably hold locks, but
		 * These values should not be changing
		 */
		ASSERT(fnp->fn_rsynccnt == 0);
		ASSERT(fnp->fn_wsynccnt == 0);
		mutex_exit(&VTOF(*vpp)->fn_lock->flk_lock);
		return (error);
	}
	/*
	 * vp can't change for FIFOS
	 */
	ASSERT(vp == *vpp);
	/*
	 * If we are opening for read (or writer)
	 *   indicate that the reader (or writer) is done with open
	 *   if there is a writer (or reader) waiting for us, wake them up
	 *	and indicate that at least 1 read (or write) open has occured
	 *	this is need in the event the read (or write) side closes
	 *	before the writer (or reader) has a chance to wake up
	 *	i.e. it sees that a reader (or writer) was once there
	 */
	if (flag & FREAD) {
		fnp->fn_rsynccnt--;	/* reader done with open */
		if (fnp->fn_flag & FIFOSYNC) {
			/*
			 * This indicates that a read open has occured
			 * Only need to set if writer is actually asleep
			 * Flag will be consumed by writer.
			 */
			fnp->fn_flag |= FIFOROCR;
			cv_broadcast(&fnp->fn_wait_cv);
		}
	}
	if (flag & FWRITE) {
		fnp->fn_wsynccnt--;	/* writer done with open */
		if (fnp->fn_flag & FIFOSYNC) {
			/*
			 * This indicates that a write open has occured
			 * Only need to set if reader is actually asleep
			 * Flag will be consumed by reader.
			 */
			fnp->fn_flag |= FIFOWOCR;
			cv_broadcast(&fnp->fn_wait_cv);
		}
	}

	fnp->fn_flag &= ~FIFOSYNC;

	/*
	 * errors don't wait around.. just return
	 * Note: XXX other end will wake up and continue despite error.
	 * There is no defined semantic on the correct course of option
	 * so we do what we've done in the past
	 */
	if (error != 0) {
		mutex_exit(&fnp->fn_lock->flk_lock);
		goto done;
	}
	ASSERT(fnp->fn_rsynccnt <= fnp->fn_rcnt);
	ASSERT(fnp->fn_wsynccnt <= fnp->fn_wcnt);
	/*
	 * FIFOWOCR (or FIFOROCR) indicates that the writer (or reader)
	 * has woken us up and is done with open (this way, if the other
	 * end has made it to close, we don't block forever in open)
	 * fn_wnct == fn_wsynccnt (or fn_rcnt == fn_rsynccnt) indicates
	 * that no writer (or reader) has yet made it through open
	 * This has the side benifit of that the first
	 * reader (or writer) will wait until the other end finishes open
	 */
	if (flag & FREAD) {
		while ((fnp->fn_flag & FIFOWOCR) == 0 &&
		    fnp->fn_wcnt == fnp->fn_wsynccnt) {
			if (flag & (FNDELAY|FNONBLOCK)) {
				mutex_exit(&fnp->fn_lock->flk_lock);
				goto done;
			}
			fnp->fn_insync++;
			fnp->fn_flag |= FIFOSYNC;
			if (!cv_wait_sig_swap(&fnp->fn_wait_cv,
			    &fnp->fn_lock->flk_lock)) {
				/*
				 * Last reader to wakeup clear writer
				 * Clear both writer and reader open
				 * occured flag incase other end is O_RDWR
				 */
				if (--fnp->fn_insync == 0 &&
				    fnp->fn_flag & FIFOWOCR) {
					fnp->fn_flag &= ~(FIFOWOCR|FIFOROCR);
				}
				mutex_exit(&fnp->fn_lock->flk_lock);
				(void) fifo_close(*vpp, ((uint_t)flag & FMASK),
				    1, 0, crp);
				error = EINTR;
				goto done;
			}
			/*
			 * Last reader to wakeup clear writer open occured flag
			 * Clear both writer and reader open occured flag
			 * incase other end is O_RDWR
			 */
			if (--fnp->fn_insync == 0 &&
			    fnp->fn_flag & FIFOWOCR) {
				fnp->fn_flag &= ~(FIFOWOCR|FIFOROCR);
				break;
			}
		}
	} else if (flag & FWRITE) {
		while ((fnp->fn_flag & FIFOROCR) == 0 &&
		    fnp->fn_rcnt == fnp->fn_rsynccnt) {
			if ((flag & (FNDELAY|FNONBLOCK)) && fnp->fn_rcnt == 0) {
				mutex_exit(&fnp->fn_lock->flk_lock);
				(void) fifo_close(*vpp, ((uint_t)flag & FMASK),
				    1, 0, crp);
				error = ENXIO;
				goto done;
			}
			fnp->fn_flag |= FIFOSYNC;
			fnp->fn_insync++;
			if (!cv_wait_sig_swap(&fnp->fn_wait_cv,
			    &fnp->fn_lock->flk_lock)) {
				/*
				 * Last writer to wakeup clear
				 * Clear both writer and reader open
				 * occured flag in case other end is O_RDWR
				 */
				if (--fnp->fn_insync == 0 &&
				    (fnp->fn_flag & FIFOROCR) != 0) {
					fnp->fn_flag &= ~(FIFOWOCR|FIFOROCR);
				}
				mutex_exit(&fnp->fn_lock->flk_lock);
				(void) fifo_close(*vpp, ((uint_t)flag & FMASK),
				    1, 0, crp);
				error = EINTR;
				goto done;
			}
			/*
			 * Last writer to wakeup clear reader open occured flag
			 * Clear both writer and reader open
			 * occured flag in case other end is O_RDWR
			 */
			if (--fnp->fn_insync == 0 &&
			    (fnp->fn_flag & FIFOROCR) != 0) {
				fnp->fn_flag &= ~(FIFOWOCR|FIFOROCR);
				break;
			}
		}
	}
	mutex_exit(&fn_lock->flk_lock);
done:
	return (error);
}

/*
 * Close down a stream.
 * Call cleanlocks() and strclean() on every close.
 * For last close send hangup message and force
 * the other end of a named pipe to be unmounted.
 * Mount guarantees that the mounted end will only call fifo_close()
 * with a count of 1 when the unmount occurs.
 * This routine will close down one end of a pipe or FIFO
 * and free the stream head via strclose()
 */
/*ARGSUSED*/
int
fifo_close(vnode_t *vp, int flag, int count, offset_t offset, cred_t *crp)
{
	fifonode_t	*fnp		= VTOF(vp);
	fifonode_t	*fn_dest	= fnp->fn_dest;
	int		error		= 0;
	fifolock_t	*fn_lock	= fnp->fn_lock;
	queue_t		*sd_wrq;
	vnode_t		*fn_dest_vp;
	int		senthang = 0;

	ASSERT(vp->v_stream != NULL);
	/*
	 * clean locks and clear events.
	 */
	(void) cleanlocks(vp, ttoproc(curthread)->p_pid, 0);
	cleanshares(vp, ttoproc(curthread)->p_pid);
	strclean(vp);

	/*
	 * If a file still has the pipe/FIFO open, return.
	 */
	if (count > 1)
		return (0);


	sd_wrq = strvp2wq(vp);
	mutex_enter(&fn_lock->flk_lock);

	/*
	 * wait for pending opens to finish up
	 * note: this also has the side effect of single threading closes
	 */
	while (fn_lock->flk_ocsync)
		cv_wait(&fn_lock->flk_wait_cv, &fn_lock->flk_lock);

	fn_lock->flk_ocsync = 1;

	if (flag & FREAD) {
		fnp->fn_rcnt--;
	}
	/*
	 * If we are last writer wake up sleeping readers
	 * (They'll figure out that there are no more writers
	 * and do the right thing)
	 * send hangup down stream so that stream head will do the
	 * right thing.
	 */
	if (flag & FWRITE) {
		if (--fnp->fn_wcnt == 0 && fn_dest->fn_rcnt > 0) {
			if ((fn_dest->fn_flag & (FIFOFAST | FIFOWANTR)) ==
			    (FIFOFAST | FIFOWANTR)) {
				/*
				 * While we're at it, clear FIFOWANTW too
				 * Wake up any sleeping readers or
				 * writers.
				 */
				fn_dest->fn_flag &= ~(FIFOWANTR | FIFOWANTW);
				cv_broadcast(&fn_dest->fn_wait_cv);
			}
			/*
			 * This is needed incase the other side
			 * was opened non-blocking.  It is the
			 * only way we can tell that wcnt is 0 because
			 * of close instead of never having a writer
			 */
			if (!(fnp->fn_flag & ISPIPE))
				fnp->fn_flag |= FIFOCLOSE;
			/*
			 * Note: sending hangup effectively shuts down
			 * both reader and writer at other end.
			 */
			(void) putnextctl_wait(sd_wrq, M_HANGUP);
			senthang = 1;
		}
	}

	/*
	 * For FIFOs we need to indicate to stream head that last reader
	 * has gone away so that an error is generated
	 * Pipes just need to wake up the other end so that it can
	 * notice this end has gone away.
	 */

	if (fnp->fn_rcnt == 0 && fn_dest->fn_wcnt > 0) {
		if ((fn_dest->fn_flag & (FIFOFAST | FIFOWANTW)) ==
		    (FIFOFAST | FIFOWANTW)) {
			/*
			 * wake up any sleeping writers
			 */
			fn_dest->fn_flag &= ~FIFOWANTW;
			cv_broadcast(&fn_dest->fn_wait_cv);
		}
	}

	/*
	 * if there are still processes with this FIFO open
	 *	clear open/close sync flag
	 *	and just return;
	 */
	if (--fnp->fn_open > 0) {
		ASSERT((fnp->fn_rcnt + fnp->fn_wcnt) != 0);
		fn_lock->flk_ocsync = 0;
		cv_broadcast(&fn_lock->flk_wait_cv);
		mutex_exit(&fn_lock->flk_lock);
		return (0);
	}

	/*
	 * Need to send HANGUP if other side is still open
	 * (fnp->fn_rcnt or fnp->fn_wcnt may not be zero (some thread
	 * on this end of the pipe may still be in fifo_open())
	 *
	 * Note: we can get here with fn_rcnt and fn_wcnt != 0 if some
	 * thread is blocked somewhere in the fifo_open() path prior to
	 * fifo_stropen() incrementing fn_open.  This can occur for
	 * normal FIFOs as well as named pipes.  fn_rcnt and
	 * fn_wcnt only indicate attempts to open. fn_open indicates
	 * successful opens. Partially opened FIFOs should proceed
	 * normally; i.e. they will appear to be new opens.  Partially
	 * opened pipes will probably fail.
	 */

	if (fn_dest->fn_open && senthang == 0)
		(void) putnextctl_wait(sd_wrq, M_HANGUP);


	/*
	 * If this a pipe and this is the first end to close,
	 * then we have a bit of cleanup work to do.
	 * 	Mark both ends of pipe as closed.
	 * 	Wake up anybody blocked at the other end and for named pipes,
	 *	Close down this end of the stream
	 *	Allow other opens/closes to continue
	 * 	force an unmount of other end.
	 * Otherwise if this is last close,
	 *	flush messages,
	 *	close down the stream
	 *	allow other opens/closes to continue
	 */
	fnp->fn_flag &= ~FIFOISOPEN;
	if ((fnp->fn_flag & ISPIPE) && !(fnp->fn_flag & FIFOCLOSE)) {
		fnp->fn_flag |= FIFOCLOSE;
		fn_dest->fn_flag |= FIFOCLOSE;
		if (fnp->fn_flag & FIFOFAST)
			fifo_fastflush(fnp);
		if (vp->v_stream != NULL) {
			mutex_exit(&fn_lock->flk_lock);
			(void) strclose(vp, flag, crp);
			mutex_enter(&fn_lock->flk_lock);
		}
		cv_broadcast(&fn_dest->fn_wait_cv);
		/*
		 * allow opens and closes to proceed
		 * Since this end is now closed down, any attempt
		 * to do anything with this end will fail
		 */
		fn_lock->flk_ocsync = 0;
		cv_broadcast(&fn_lock->flk_wait_cv);
		fn_dest_vp = FTOV(fn_dest);
		/*
		 * if other end of pipe has been opened and it's
		 * a named pipe, unmount it
		 */
		if (fn_dest_vp->v_stream &&
		    (fn_dest_vp->v_stream->sd_flag & STRMOUNT)) {
			/*
			 * We must hold the destination vnode because
			 * nm_unmountall() causes close to be called
			 * for the other end of named pipe.  This
			 * could free the vnode before we are ready.
			 */
			VN_HOLD(fn_dest_vp);
			mutex_exit(&fn_lock->flk_lock);
			error = nm_unmountall(fn_dest_vp, crp);
			ASSERT(error == 0);
			VN_RELE(fn_dest_vp);
		} else {
			ASSERT(vp->v_count >= 1);
			mutex_exit(&fn_lock->flk_lock);
		}
	} else {
		if (fnp->fn_flag & FIFOFAST)
			fifo_fastflush(fnp);
#if DEBUG
		fn_dest_vp = FTOV(fn_dest);
		if (fn_dest_vp->v_stream)
		    ASSERT((fn_dest_vp->v_stream->sd_flag & STRMOUNT) == 0);
#endif
		if (vp->v_stream != NULL) {
			mutex_exit(&fn_lock->flk_lock);
			(void) strclose(vp, flag, crp);
			mutex_enter(&fn_lock->flk_lock);
		}
		fn_lock->flk_ocsync = 0;
		cv_broadcast(&fn_lock->flk_wait_cv);
		cv_broadcast(&fn_dest->fn_wait_cv);
		mutex_exit(&fn_lock->flk_lock);
	}
	return (error);
}

/*
 * Read from a pipe or FIFO.
 * return 0 if....
 *    (1) user read request is 0 or no stream
 *    (2) broken pipe with no data
 *    (3) write-only FIFO with no data
 *    (4) no data and FNDELAY flag is set.
 * Otherwise return
 *	EAGAIN if FNONBLOCK is set and no data to read
 *	EINTR if signal recieved while waiting for data
 *
 * While there is no data to read....
 *   -  if the NDELAY/NONBLOCK flag is set, return 0/EAGAIN.
 *   -  wait for a write.
 *
 */
/*ARGSUSED*/

static int
fifo_read(struct vnode *vp, struct uio *uiop, int ioflag, struct cred *crp)
{
	fifonode_t	*fnp		= VTOF(vp);
	fifonode_t	*fn_dest;
	fifolock_t	*fn_lock	= fnp->fn_lock;
	int		error		= 0;
	int		size;
	mblk_t		*bp;
	int		startsize;

	ASSERT(vp->v_stream != NULL);
	if (uiop->uio_resid == 0)
		return (0);

	mutex_enter(&fn_lock->flk_lock);

	TRACE_2(TR_FAC_FIFO,
		TR_FIFOREAD_IN, "fifo_read in:%X fn_flag %X",
		vp, fnp->fn_flag);

	if (! (fnp->fn_flag & FIFOFAST))
		goto stream_mode;

	fn_dest	= fnp->fn_dest;
	/*
	 * Check for data on our input queue
	 */

	while (fnp->fn_count == 0) {
		/*
		 * No data on first attempt and no writer, then EOF
		 */
		if (fn_dest->fn_wcnt == 0 || fn_dest->fn_rcnt == 0) {
			mutex_exit(&fn_lock->flk_lock);
			return (0);
		}
		/*
		 * no data found.. if non-blocking, return EAGAIN
		 * otherwise 0.
		 */
		if (uiop->uio_fmode & (FNDELAY|FNONBLOCK)) {
			mutex_exit(&fn_lock->flk_lock);
			if (uiop->uio_fmode & FNONBLOCK)
				return (EAGAIN);
			return (0);
		}

		/*
		 * Note: FIFOs can get here with FIFOCLOSE set if
		 * write side is in the middle of opeining after
		 * it once closed. Pipes better not have FIFOCLOSE set
		 */
		ASSERT((fnp->fn_flag & (ISPIPE|FIFOCLOSE)) !=
		    (ISPIPE|FIFOCLOSE));
		/*
		 * wait for data
		 */
		fnp->fn_flag = fnp->fn_flag | FIFOWANTR;

		TRACE_1(TR_FAC_FIFO, TR_FIFOREAD_WAIT,
			"fiforead wait: %X", vp);

		if (!cv_wait_sig_swap(&fnp->fn_wait_cv,
		    &fn_lock->flk_lock)) {
			error = EINTR;
			goto done;
		}

		TRACE_1(TR_FAC_FIFO, TR_FIFOREAD_WAKE,
			"fiforead awake: %X", vp);

		/*
		 * check to make sure we are still in fast mode
		 */
		if (!(fnp->fn_flag & FIFOFAST))
			goto stream_mode;
	}
	size = MIN(uiop->uio_resid, fnp->fn_count);
	startsize = size;
	bp = fnp->fn_mp;
	while (bp && size > 0) {
		mblk_t *tmpbp;
		int tsize = bp->b_wptr - bp->b_rptr;

		error = uiomove((char *)bp->b_rptr,
		    MIN(tsize, size), UIO_READ, uiop);
		if (error) {
			/*
			 * we might have failed after the
			 * first iteration.  Restore fn_mp
			 * and fn_count to possibly new values
			 */
			fnp->fn_mp = bp;
			fnp->fn_count = msgdsize(bp);
			goto trywake;
		}
		if (tsize <= size) {
			tmpbp = bp;
			bp = bp->b_cont;
			freeb(tmpbp);
			size -= tsize;
		} else {
			bp->b_rptr += size;
			ASSERT(uiop->uio_resid == 0);
			break;
		}
	}
	fnp->fn_count -= startsize;
	fnp->fn_mp = bp;
	ASSERT(fnp->fn_count == 0 || uiop->uio_resid == 0);
trywake:
	/*
	 * wake up any blocked writers, processes
	 * sleeping on POLLWRNORM, or processes waiting for SIGPOLL
	 * Note: checking for fn_count < Fifohiwat emulates
	 * STREAMS functionality when low water mark is 0
	 */
	if (fn_dest->fn_flag & (FIFOWANTW | FIFOHIWATW) &&
	    fnp->fn_count < Fifohiwat) {
		fifo_wakewriter(fn_dest, fn_lock);
	}
	goto done;

	/*
	 * FIFO is in streams mode.. let the stream head handle it
	 */
stream_mode:

	mutex_exit(&fn_lock->flk_lock);
	TRACE_1(TR_FAC_FIFO,
		TR_FIFOREAD_STREAM, "fifo_read stream_mode:%X", vp);

	error = strread(vp, uiop, crp);

	mutex_enter(&fn_lock->flk_lock);

done:
	/*
	 * vnode update access time
	 */
	if (error == 0) {
		time_t now = hrestime.tv_sec;
		if (fnp->fn_flag & ISPIPE)
			fnp->fn_dest->fn_atime = now;
		fnp->fn_atime = now;
	}
	TRACE_2(TR_FAC_FIFO,
		TR_FIFOREAD_OUT, "fifo_read out:%X error %d",
		vp, error);
	mutex_exit(&fn_lock->flk_lock);
	return (error);
}

/*
 * send SIGPIPE and return EPIPE if ...
 *   (1) broken pipe (essentially, reader is gone)
 *   (2) FIFO is not open for reading
 * return 0 if...
 *   (1) no stream
 *   (2) user request is 0 and STRSNDZERO is not set
 *	Note: STRSNDZERO can't be set in fast mode
 * While the stream is flow controlled....
 *   -  if the NDELAY/NONBLOCK flag is set, return 0/EAGAIN.
 *   -  unlock the fifonode and sleep waiting for a reader.
 *   -  if a pipe and it has a mate, sleep waiting for its mate
 *	to read.
 */
/*ARGSUSED*/
static int
fifo_write(vnode_t *vp, uio_t *uiop, int ioflag, cred_t *crp)
{
	struct fifonode	*fnp, *fn_dest;
	fifolock_t	*fn_lock;
	struct stdata	*stp;
	int		error	= 0;
	int		write_size;
	int		size;
	int		fmode;
	mblk_t		*bp;
	int		dowake = 0;

	ASSERT(vp->v_stream);
	uiop->uio_loffset = 0;
	stp	= vp->v_stream;

	/*
	 * remember original number of bytes requested. Used to determine if
	 * we actually have written anything at all
	 */
	write_size = uiop->uio_resid;

	/*
	 * only send null messages if STRSNDZERO is set
	 * Note: we will be in streams mode if STRSNDZERO is set
	 * XXX this streams interface should not be exposed
	 */
	if ((write_size == 0) && !(stp->sd_flag & STRSNDZERO))
		return (0);

	fnp = VTOF(vp);
	fn_lock = fnp->fn_lock;
	fn_dest = fnp->fn_dest;

	mutex_enter(&fn_lock->flk_lock);

	TRACE_3(TR_FAC_FIFO,
		TR_FIFOWRITE_IN, "fifo_write in:%X fn_flag %X size %d",
		vp, fnp->fn_flag, write_size);


	/*
	 * oops, no readers, error
	 */
	if (fn_dest->fn_rcnt == 0 || fn_dest->fn_wcnt == 0) {
		goto epipe;
	}


	/*
	 * if we are not in fast mode, let streams handle it
	 */
	if (!(fnp->fn_flag & FIFOFAST))
		goto stream_mode;

	fmode = uiop->uio_fmode & (FNDELAY|FNONBLOCK);

	do  {
		/*
		 * check to make sure we are not over high water mark
		 */
		while (fn_dest->fn_count >= Fifohiwat) {
			/*
			 * Indicate that we have gone over high
			 * water mark
			 */
			/*
			 * if non-blocking, return
			 * only happens first time through loop
			 */
			if (fmode) {
				fnp->fn_flag |= FIFOHIWATW;
				if (uiop->uio_resid == write_size) {
					mutex_exit(&fn_lock->flk_lock);
					if (fmode & FNDELAY)
						return (0);
					else
						return (EAGAIN);
				}
				goto done;
			}
			/*
			 * wait for things to drain
			 */
			fnp->fn_flag |= FIFOWANTW;
			TRACE_1(TR_FAC_FIFO, TR_FIFOWRITE_WAIT,
				"fifo_write wait: %X", vp);
			if (!cv_wait_sig_swap(&fnp->fn_wait_cv,
			    &fn_lock->flk_lock)) {
				error = EINTR;
				goto done;
			}

			TRACE_1(TR_FAC_FIFO, TR_FIFOWRITE_WAKE,
				"fifo_write wake: %X", vp);

			/*
			 * check to make sure we're still in fast mode
			 */
			if (!(fnp->fn_flag & FIFOFAST))
				goto stream_mode;

			/*
			 * make sure readers didn't go away
			 */
			if (fn_dest->fn_rcnt == 0 || fn_dest->fn_wcnt == 0) {
				goto epipe;
			}
		}
		/*
		 * If the write will put us over the high water mark,
		 * then we must break the message up into PIPE_BUF
		 * chunks to stay compliant with STREAMS
		 */
		if (uiop->uio_resid + fn_dest->fn_count > Fifohiwat)
			size = MIN(uiop->uio_resid, PIPE_BUF);
		else
			size = uiop->uio_resid;

		ASSERT(size != 0);
		/*
		 * Align the mblk with the user data so that
		 * copying in the data can take advantage of
		 * the double word alignment
		 */
		if ((bp = allocb(size + 8, BPRI_MED)) == NULL) {
			mutex_exit(&fn_lock->flk_lock);

			error = strwaitbuf(size, BPRI_MED);

			mutex_enter(&fn_lock->flk_lock);
			if (error != 0) {
				goto done;
			}
			/*
			 * check to make sure we're still in fast mode
			 */
			if (!(fnp->fn_flag & FIFOFAST))
				goto stream_mode;

			/*
			 * make sure readers didn't go away
			 */
			if (fn_dest->fn_rcnt == 0 || fn_dest->fn_wcnt == 0) {
				goto epipe;
			}
			/*
			 * some other thread could have gotten in
			 * need to go back and check hi water mark
			 */
			continue;
		}
		bp->b_rptr += ((uintptr_t)uiop->uio_iov->iov_base & 0x7);
		if (error = uiomove((caddr_t)bp->b_rptr, size,
		    UIO_WRITE, uiop)) {
			freeb(bp);
			goto done;
		}
		/*
		 * If this is the first bit of data, we'll have to
		 * wake the reader
		 */
		if (fn_dest->fn_count == 0)
			dowake = 1;
		bp->b_wptr = bp->b_rptr + size;
		fn_dest->fn_count += size;
		if (fn_dest->fn_mp != NULL)
			fn_dest->fn_tail->b_cont = bp;
		else
			fn_dest->fn_mp = bp;
		fn_dest->fn_tail = bp;
		if (dowake) {
			dowake = 0;
			/*
			 * wake up an sleeping readers or
			 * processes blocked in poll or expecting a
			 * SIGPOLL
			 */
			fifo_wakereader(fn_dest, fn_lock);
		}
	} while (uiop->uio_resid != 0);

	goto done;

stream_mode:
	/*
	 * streams mode
	 *  let the stream head handle the write
	 */
	ASSERT(MUTEX_HELD(&fn_lock->flk_lock));

	mutex_exit(&fn_lock->flk_lock);
	TRACE_1(TR_FAC_FIFO,
		TR_FIFOWRITE_STREAM, "fifo_write stream_mode:%X", vp);

	error = strwrite(vp, uiop, crp);

	mutex_enter(&fn_lock->flk_lock);

done:
	/*
	 * update vnode modification and change times
	 * make sure there were no errors and some data was transfered
	 */
	if (error == 0 && write_size != uiop->uio_resid) {
		time_t now = hrestime.tv_sec;
		if (fnp->fn_flag & ISPIPE) {
			fn_dest->fn_mtime = fn_dest->fn_ctime = now;
		}
		fnp->fn_mtime = fnp->fn_ctime = now;
	} else if (fn_dest->fn_rcnt == 0 || fn_dest->fn_wcnt == 0) {
		goto epipe;
	}
	TRACE_3(TR_FAC_FIFO, TR_FIFOWRITE_OUT,
		"fifo_write out: vp %X error %d fn_flags %d",
		vp, error, fnp->fn_flag);
	mutex_exit(&fn_lock->flk_lock);
	return (error);
epipe:
	error = EPIPE;
	TRACE_3(TR_FAC_FIFO, TR_FIFOWRITE_OUT,
		"fifo_write out: vp %X error %d fn_flags %d",
		vp, error, fnp->fn_flag);
	mutex_exit(&fn_lock->flk_lock);
	psignal(ttoproc(curthread), SIGPIPE);
	return (error);
}


static int
fifo_ioctl(vnode_t *vp, int cmd, intptr_t arg, int mode,
	cred_t *cr, int *rvalp)
{
	/*
	 * Just a quick check
	 * Once we go to streams mode we don't ever revert back
	 * So we do this quick check so as not to incur the overhead
	 * associated with acquiring the lock
	 */
	return ((VTOF(vp)->fn_flag & FIFOFAST) ?
		fifo_fastioctl(vp, cmd, arg, mode, cr, rvalp) :
		fifo_strioctl(vp, cmd, arg, mode, cr, rvalp));
}

static int
fifo_fastioctl(vnode_t *vp, int cmd, intptr_t arg, int mode,
	cred_t *cr, int *rvalp)
{
	fifonode_t	*fnp		= VTOF(vp);
	fifonode_t	*fn_dest;
	int		error		= 0;
	fifolock_t	*fn_lock	= fnp->fn_lock;
	int		cnt;

	/*
	 * tty operations not allowed
	 */
	if (((cmd & IOCTYPE) == LDIOC) ||
	    ((cmd & IOCTYPE) == tIOC) ||
	    ((cmd & IOCTYPE) == TIOC)) {
		return (EINVAL);
	}

	mutex_enter(&fn_lock->flk_lock);

	if (!(fnp->fn_flag & FIFOFAST)) {
		goto stream_mode;
	}

	switch (cmd) {

	/*
	 * Things we can't handle
	 * These will switch us to streams mode.
	 */
	default:
	case I_STR:
	case I_SRDOPT:
	case I_PUSH:
	case I_FDINSERT:
	case I_SENDFD:
	case I_RECVFD:
	case I_E_RECVFD:
	case I_ATMARK:
	case I_CKBAND:
	case I_GETBAND:
	case I_SWROPT:
		goto turn_fastoff;

	/*
	 * Things that don't do damage
	 * These things don't adjust the state of the
	 * stream head (i_setcltime does, but we don't care)
	 */
	case I_FIND:
	case I_GETSIG:
	case FIONBIO:
	case FIOASYNC:
	case I_GRDOPT:	/* probably should not get this, but no harm */
	case I_GWROPT:
	case I_LIST:
	case I_SETCLTIME:
	case I_GETCLTIME:
		mutex_exit(&fn_lock->flk_lock);
		return (strioctl(vp, cmd, arg, mode, U_TO_K, cr, rvalp));

	case I_CANPUT:
		/*
		 * We can only handle normal band canputs.
		 * XXX : We could just always go to stream mode; after all
		 * canput is a streams semantics type thing
		 */
		if (arg != 0) {
			goto turn_fastoff;
		}
		*rvalp = (fnp->fn_dest->fn_count < Fifohiwat) ? 1 : 0;
		mutex_exit(&fn_lock->flk_lock);
		return (0);

	case I_NREAD:
		/*
		 * This may seem a bit silly for non-streams semantics,
		 * (After all, if they really want a message, they'll
		 * probably use getmsg() anyway). but it doesn't hurt
		 */
		error = copyout((caddr_t)&fnp->fn_count, (caddr_t)arg,
			sizeof (cnt));
		if (error == 0) {
			*rvalp = (fnp->fn_count == 0) ? 0 : 1;
		}
		break;

	case FIORDCHK:
		*rvalp = fnp->fn_count;
		break;

	case I_PEEK:
	    {
		STRUCT_DECL(strpeek, strpeek);
		struct uio	uio;
		struct iovec	iov;
		int		count;
		mblk_t		*bp;

		STRUCT_INIT(strpeek, mode);

		if (fnp->fn_count == 0) {
			*rvalp = 0;
			break;
		}

		error = copyin((caddr_t)arg, STRUCT_BUF(strpeek),
		    STRUCT_SIZE(strpeek));
		if (error)
			break;

		/*
		 * can't have any high priority message when in fast mode
		 */
		if (STRUCT_FGET(strpeek, flags) & RS_HIPRI) {
			*rvalp = 0;
			break;
		}

		iov.iov_base = STRUCT_FGETP(strpeek, databuf.buf);
		iov.iov_len = STRUCT_FGET(strpeek, databuf.maxlen);
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_loffset = 0;
		uio.uio_segflg = UIO_USERSPACE;
		uio.uio_fmode = 0;
		uio.uio_resid = iov.iov_len;
		count = fnp->fn_count;
		bp = fnp->fn_mp;
		while (count > 0 && uio.uio_resid) {
			cnt = MIN(uio.uio_resid, bp->b_wptr - bp->b_rptr);
			if ((error = uiomove((char *)bp->b_rptr, cnt,
			    UIO_READ, &uio)) != 0) {
				break;
			}
			count -= cnt;
			bp = bp->b_cont;
		}
		STRUCT_FSET(strpeek, databuf.len,
		    STRUCT_FGET(strpeek, databuf.maxlen) - uio.uio_resid);
		STRUCT_FSET(strpeek, flags, 0);
		STRUCT_FSET(strpeek, ctlbuf.len,
		    STRUCT_FGET(strpeek, ctlbuf.maxlen));

		error = copyout(STRUCT_BUF(strpeek), (caddr_t)arg,
		    STRUCT_SIZE(strpeek));
		if (error == 0)
			*rvalp = 1;
		break;
	    }

	case FIONREAD:
		/*
		 * let user know total number of bytes in message queue
		 */
		error = copyout((caddr_t)&fnp->fn_count, (caddr_t)arg,
			sizeof (fnp->fn_count));
		if (error == 0)
			*rvalp = 0;
		break;

	case I_SETSIG:
		/*
		 * let streams set up the signal masking for us
		 * we just check to see if it's set
		 * XXX : this interface should not be visible
		 *  i.e. STREAM's framework is exposed.
		 */
		error = strioctl(vp, cmd, arg, mode, U_TO_K, cr, rvalp);
		if (vp->v_stream->sd_sigflags & (S_INPUT|S_RDNORM|S_WRNORM))
			fnp->fn_flag |= FIFOSETSIG;
		else
			fnp->fn_flag &= ~FIFOSETSIG;
		break;

	case I_FLUSH:
		/*
		 * flush them message queues
		 */
		if (arg & ~FLUSHRW) {
			error = EINVAL;
			break;
		}
		if (arg & FLUSHR) {
			fifo_fastflush(fnp);
		}
		fn_dest = fnp->fn_dest;
		if ((arg & FLUSHW)) {
			fifo_fastflush(fn_dest);
		}
		/*
		 * wake up any sleeping readers or writers
		 * (waking readers probably doesn't make sense, but it
		 *  doesn't hurt; i.e. we just got rid of all the data
		 *  what's to read ?)
		 */
		if (fn_dest->fn_flag & (FIFOWANTW | FIFOWANTR)) {
			fn_dest->fn_flag &= ~(FIFOWANTW | FIFOWANTR);
			cv_broadcast(&fn_dest->fn_wait_cv);
		}
		*rvalp = 0;
		break;

	/*
	 * Since no band data can ever get on a fifo in fast mode
	 * just return 0.
	 */
	case I_FLUSHBAND:
		error = 0;
		*rvalp = 0;
		break;

	/*
	 * invalid calls for stream head or fifos
	 */

	case I_POP:		/* shouldn't happen */
	case I_LOOK:
	case I_LINK:
	case I_PLINK:
	case I_UNLINK:
	case I_PUNLINK:

	/*
	 * more invalid tty type of ioctls
	 */

	case SRIOCSREDIR:
	case SRIOCISREDIR:
		error = EINVAL;
		break;

	}
	mutex_exit(&fn_lock->flk_lock);
	return (error);

turn_fastoff:
	fifo_fastoff(fnp);

stream_mode:
	/*
	 * streams mode
	 */
	mutex_exit(&fn_lock->flk_lock);
	return (fifo_strioctl(vp, cmd, arg, mode, cr, rvalp));

}

/*
 * Fifo is in streams mode.  Streams framework does most of the work
 */
static int
fifo_strioctl(vnode_t *vp, int cmd, intptr_t arg, int mode,
	cred_t *cr, int *rvalp)
{
	queue_t		*sd_wrq;
	fifonode_t	*fnp		= VTOF(vp);
	int		error		= 0;
	fifolock_t	*fn_lock;

	switch (cmd) {

	default:
		return (strioctl(vp, cmd, arg, mode, U_TO_K, cr, rvalp));

	case I_FLUSH:

		sd_wrq = strvp2wq(vp);
		if (arg & ~FLUSHRW)
			return (EINVAL);
		/*
		 * prevent q_next from changing
		 */
		claimstr(sd_wrq);
		/*
		 * If there are modules on the stream, pass
		 * the flush request to the stream head.
		 * XXX this interface needs some work
		 */
		if (sd_wrq->q_next &&
			sd_wrq->q_next->q_qinfo != &fifo_strdata) {
				error = strioctl(vp, cmd, arg, mode, U_TO_K,
				    cr, rvalp);
				releasestr(sd_wrq);
				return (error);
		}
		/*
		 * flush the queues.
		 */
		if (arg & FLUSHR) {
			flushq(RD(sd_wrq), FLUSHALL);
		}
		if ((arg & FLUSHW) && (sd_wrq->q_next)) {
			flushq(sd_wrq->q_next, FLUSHALL);
		}
		releasestr(sd_wrq);
		break;
	/*
	 * The FIFOSEND flag is set to inform other processes that a file
	 * descriptor is pending at the stream head of this pipe.
	 * The flag is cleared and the sending process is awoken when
	 * this process has completed recieving the file descriptor.
	 * XXX This could become out of sync if the process does I_SENDFDs
	 * and opens on connld attached to the same pipe.
	 *
	 */
	case I_RECVFD:
	case I_E_RECVFD:
		if ((error = strioctl(vp, cmd, arg, mode, U_TO_K, cr,
		    rvalp)) == 0) {
			fn_lock = fnp->fn_lock;
			mutex_enter(&fn_lock->flk_lock);
			if (fnp->fn_flag & FIFOSEND) {
				fnp->fn_flag &= ~FIFOSEND;
				cv_broadcast(&fnp->fn_dest->fn_wait_cv);
			}
			mutex_exit(&fn_lock->flk_lock);
		}
		break;

	}
done:
	return (error);
}

/*
 * If shadowing a vnode (FIFOs), apply the VOP_GETATTR to the shadowed
 * vnode to Obtain the node information. If not shadowing (pipes), obtain
 * the node information from the credentials structure.
 */
int
fifo_getattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *crp)
{
	int		error		= 0;
	fifonode_t	*fnp		= VTOF(vp);
	queue_t		*qp;
	qband_t		*bandp;
	fifolock_t	*fn_lock	= fnp->fn_lock;

	if (fnp->fn_realvp) {
		/*
		 * for FIFOs or mounted pipes
		 */
		if (error = VOP_GETATTR(fnp->fn_realvp, vap, flags, crp))
			return (error);
		mutex_enter(&fn_lock->flk_lock);
		/* set current times from fnode, even if older than vnode */
		vap->va_atime.tv_sec = fnp->fn_atime;
		vap->va_atime.tv_nsec = 0;
		vap->va_mtime.tv_sec = fnp->fn_mtime;
		vap->va_mtime.tv_nsec = 0;
		vap->va_ctime.tv_sec = fnp->fn_ctime;
		vap->va_ctime.tv_nsec = 0;
	} else {
		/*
		 * for non-attached/ordinary pipes
		 */
		vap->va_mode = 0;
		mutex_enter(&fn_lock->flk_lock);
		vap->va_atime.tv_sec = fnp->fn_atime;
		vap->va_atime.tv_nsec = 0;
		vap->va_mtime.tv_sec = fnp->fn_mtime;
		vap->va_mtime.tv_nsec = 0;
		vap->va_ctime.tv_sec = fnp->fn_ctime;
		vap->va_ctime.tv_nsec = 0;
		vap->va_uid = crp->cr_uid;
		vap->va_gid = crp->cr_gid;
		vap->va_nlink = 0;
		vap->va_fsid = fifodev;
		vap->va_nodeid = (ino64_t)fnp->fn_ino;
		vap->va_rdev = 0;
	}
	vap->va_type = VFIFO;
	vap->va_blksize = PIPE_BUF;
	/*
	 * Size is number of un-read bytes at the stream head and
	 * nblocks is the unread bytes expressed in blocks.
	 */
	if (vp->v_stream && (fnp->fn_flag & FIFOISOPEN)) {
		if ((fnp->fn_flag & FIFOFAST)) {
			vap->va_size = (u_offset_t)fnp->fn_count;
		} else {
			qp = RD((strvp2wq(vp)));
			vap->va_size = (u_offset_t)qp->q_count;
			if (qp->q_nband != 0) {
				mutex_enter(QLOCK(qp));
				for (bandp = qp->q_bandp; bandp;
				    bandp = bandp->qb_next)
					vap->va_size += bandp->qb_count;
				mutex_exit(QLOCK(qp));
			}
		}
		vap->va_nblocks = (fsblkcnt64_t)btod(vap->va_size);
	} else {
		vap->va_size = (u_offset_t)0;
		vap->va_nblocks = (fsblkcnt64_t)0;
	}
	mutex_exit(&fn_lock->flk_lock);
	vap->va_vcode = 0;
	return (0);
}

/*
 * If shadowing a vnode, apply the VOP_SETATTR to it, and to the fnode.
 * Otherwise, set the time and return 0.
 */
int
fifo_setattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *crp)
{
	fifonode_t	*fnp	= VTOF(vp);
	int		error	= 0;
	fifolock_t	*fn_lock;

	if (fnp->fn_realvp)
		error = VOP_SETATTR(fnp->fn_realvp, vap, flags, crp);
	if (error == 0) {
		fn_lock = fnp->fn_lock;
		mutex_enter(&fn_lock->flk_lock);
		if (vap->va_mask & AT_ATIME)
			fnp->fn_atime = vap->va_atime.tv_sec;
		if (vap->va_mask & AT_MTIME)
			fnp->fn_mtime = vap->va_mtime.tv_sec;
		fnp->fn_ctime = hrestime.tv_sec;
		mutex_exit(&fn_lock->flk_lock);
	}
	return (error);
}

/*
 * If shadowing a vnode, apply VOP_ACCESS to it.
 * Otherwise, return 0 (allow all access).
 */
int
fifo_access(vnode_t *vp, int mode, int flags, cred_t *crp)
{

	if (VTOF(vp)->fn_realvp)
		return (VOP_ACCESS(VTOF(vp)->fn_realvp, mode, flags, crp));
	else
		return (0);
}

/*
 * If shadowing a vnode, apply the VOP_FSYNC to it.
 * Otherwise, return 0.
 */
int
fifo_fsync(vnode_t *vp, int syncflag, cred_t *crp)
{
	fifonode_t	*fnp	= VTOF(vp);
	vattr_t		va;

	if (fnp->fn_realvp == NULL)
		return (0);

	bzero((caddr_t)&va, sizeof (va));
	if (VOP_GETATTR(fnp->fn_realvp, &va, 0, crp) == 0) {
		if (fnp->fn_mtime > va.va_mtime.tv_sec) {
			va.va_mtime.tv_sec = fnp->fn_mtime;
			va.va_mask = AT_MTIME;
		}
		if (fnp->fn_atime > va.va_atime.tv_sec) {
			va.va_atime.tv_sec = fnp->fn_atime;
			va.va_mask |= AT_ATIME;
		}
		if (va.va_mask != 0)
			VOP_SETATTR(fnp->fn_realvp, &va, 0, crp);
	}
	return (VOP_FSYNC(fnp->fn_realvp, syncflag, crp));
}

/*
 * Called when the upper level no longer holds references to the
 * vnode. Sync the file system and free the fifonode.
 */
void
fifo_inactive(vnode_t *vp, cred_t *crp)
{
	fifonode_t	*fnp;
	fifolock_t	*fn_lock;

	mutex_enter(&ftable_lock);
	mutex_enter(&vp->v_lock);
	ASSERT(vp->v_count >= 1);
	if (--vp->v_count != 0) {
		/*
		 * Somebody accessed the fifo before we got a chance to
		 * remove it.  They will remove it when they do a vn_rele.
		 */
		mutex_exit(&vp->v_lock);
		mutex_exit(&ftable_lock);
		return;
	}
	mutex_exit(&vp->v_lock);

	fnp = VTOF(vp);

	/*
	 * remove fifo from fifo list so that no other process
	 * can grab it.
	 */
	if (fnp->fn_realvp) {
		(void) fiforemove(fnp);
		mutex_exit(&ftable_lock);
		(void) fifo_fsync(vp, FSYNC, crp);
		VN_RELE(fnp->fn_realvp);
	} else
		mutex_exit(&ftable_lock);

	fn_lock = fnp->fn_lock;

	mutex_enter(&fn_lock->flk_lock);
	ASSERT(vp->v_stream == NULL);
	ASSERT(vp->v_count == 0);
	/*
	 * if this is last reference to the lock, then we can
	 * free everything up.
	 */
	if (--fn_lock->flk_ref == 0) {
		mutex_exit(&fn_lock->flk_lock);
		ASSERT(fnp->fn_open == 0);
		ASSERT(fnp->fn_dest->fn_open == 0);
		if (fnp->fn_mp) {
			freemsg(fnp->fn_mp);
			fnp->fn_mp = NULL;
			fnp->fn_count = 0;
		}
		if (fnp->fn_flag & ISPIPE) {
			fifonode_t *fn_dest = fnp->fn_dest;

			vp = FTOV(fn_dest);
			if (fn_dest->fn_mp) {
				freemsg(fn_dest->fn_mp);
				fn_dest->fn_mp = NULL;
				fn_dest->fn_count = 0;
			}
			kmem_cache_free(pipe_cache, (fifodata_t *)fn_lock);
		} else
			kmem_cache_free(fnode_cache, (fifodata_t *)fn_lock);
	} else {
		mutex_exit(&fn_lock->flk_lock);
	}
}

/*
 * If shadowing a vnode, apply the VOP_FID to it.
 * Otherwise, return EINVAL.
 */
int
fifo_fid(vnode_t *vp, fid_t *fidfnp)
{
	if (VTOF(vp)->fn_realvp)
		return (VOP_FID(VTOF(vp)->fn_realvp, fidfnp));
	else
		return (EINVAL);
}

/*
 * Lock a fifonode.
 */
/* ARGSUSED */
void
fifo_rwlock(vnode_t *vp, int read)
{
}

/*
 * Unlock a fifonode.
 */
/* ARGSUSED */
void
fifo_rwunlock(vnode_t *vp, int read)
{
}

/*
 * Return error since seeks are not allowed on pipes.
 */
/*ARGSUSED*/
int
fifo_seek(vnode_t *vp, offset_t ooff, offset_t *noffp)
{
	return (ESPIPE);
}

/*
 * If there is a realvp associated with vp, return it.
 */
int
fifo_realvp(vnode_t *vp, vnode_t **vpp)
{
	vnode_t *rvp;

	if ((rvp = VTOF(vp)->fn_realvp) != NULL) {
		vp = rvp;
		if (VOP_REALVP(vp, &rvp) == 0)
			vp = rvp;
	}

	*vpp = vp;
	return (0);
}

/*
 * Poll for interesting events on a stream pipe
 */
int
fifo_poll(vnode_t *vp, short events, int anyyet, short *reventsp,
	pollhead_t **phpp)
{
	fifonode_t	*fnp, *fn_dest;
	fifolock_t	*fn_lock;
	int		retevents;
	struct stdata	*stp;

	ASSERT(vp->v_stream != NULL);

	stp = vp->v_stream;
	retevents	= 0;
	fnp		= VTOF(vp);
	fn_dest		= fnp->fn_dest;
	fn_lock		= fnp->fn_lock;

	polllock(&stp->sd_pollist, &fn_lock->flk_lock);

	/*
	 * see if FIFO/pipe open
	 */
	if ((fnp->fn_flag & FIFOISOPEN) == 0) {
		if (((events & (POLLIN | POLLRDNORM | POLLPRI | POLLRDBAND)) &&
		    fnp->fn_rcnt == 0) ||
		    ((events & (POLLWRNORM | POLLWRBAND)) &&
		    fnp->fn_wcnt == 0)) {
			mutex_exit(&fnp->fn_lock->flk_lock);
			*reventsp = POLLERR;
			return (0);
		}
	}

	/*
	 * if not in fast mode, let the stream head take care of it
	 */
	if (!(fnp->fn_flag & FIFOFAST)) {
		mutex_exit(&fnp->fn_lock->flk_lock);
		goto stream_mode;
	}

	/*
	 * If this is a pipe.. check to see if the other
	 * end is gone.  If we are a fifo, check to see
	 * if write end is gone.
	 */

	if ((fnp->fn_flag & ISPIPE) && (fn_dest->fn_open == 0)) {
		retevents = POLLHUP;
	} else if ((fnp->fn_flag & (FIFOCLOSE | ISPIPE)) == FIFOCLOSE &&
	    (fn_dest->fn_wcnt == 0)) {
		/*
		 * no writer at other end.
		 * it was closed (versus yet to be opened)
		 */
			retevents = POLLHUP;
	} else if (events & (POLLWRNORM | POLLWRBAND)) {
		if (events & POLLWRNORM) {
			if (fn_dest->fn_count < Fifohiwat)
				retevents = POLLWRNORM;
			else
				fnp->fn_flag |= FIFOHIWATW;
		}
		/*
		 * This is always true for fast pipes
		 * (Note: will go to STREAMS mode if band data is written)
		 */
		if (events & POLLWRBAND)
			retevents |= POLLWRBAND;
	}
	if (events & (POLLIN | POLLRDNORM)) {
		if (fnp->fn_count)
			retevents |= (events & (POLLIN | POLLRDNORM));
	}

	/*
	 * if we happened to get something, return
	 */

	if ((*reventsp = (short)retevents) != 0) {
		mutex_exit(&fnp->fn_lock->flk_lock);
		return (0);
	}

	/*
	 * If poll() has not found any events yet, set up event cell
	 * to wake up the poll if a requested event occurs on this
	 * pipe/fifo.
	 */
	if (!anyyet) {
		if (events & POLLWRNORM)
			fnp->fn_flag |= FIFOPOLLW;
		if (events & (POLLIN | POLLRDNORM))
			fnp->fn_flag |= FIFOPOLLR;
		if (events & POLLRDBAND)
			fnp->fn_flag |= FIFOPOLLRBAND;
		/*
		 * XXX Don't like exposing this from streams
		 */
		*phpp = &stp->sd_pollist;
	}
	mutex_exit(&fnp->fn_lock->flk_lock);
	return (0);
stream_mode:
	return (strpoll(stp, events, anyyet, reventsp, phpp));
}

/*
 * POSIX pathconf() support.
 */
/* ARGSUSED */
int
fifo_pathconf(vnode_t *vp, int cmd, ulong_t *valp, cred_t *cr)
{
	ulong_t val;
	int error = 0;

	switch (cmd) {

	case _PC_LINK_MAX:
		val = MAXLINK;
		break;

	case _PC_MAX_CANON:
		val = MAX_CANON;
		break;

	case _PC_MAX_INPUT:
		val = MAX_INPUT;
		break;

	case _PC_NAME_MAX:
		error = EINVAL;
		break;

	case _PC_PATH_MAX:
		val = MAXPATHLEN;
		break;

	case _PC_PIPE_BUF:
		val = PIPE_BUF;
		break;

	case _PC_NO_TRUNC:
		if (vp->v_vfsp->vfs_flag & VFS_NOTRUNC)
			val = 1;	/* NOTRUNC is enabled for vp */
		else
			val = (ulong_t)-1;
		break;

	case _PC_VDISABLE:
		val = _POSIX_VDISABLE;
		break;

	case _PC_CHOWN_RESTRICTED:
		if (rstchown)
			val = rstchown;		/* chown restricted enabled */
		else
			val = (ulong_t)-1;
		break;

	case _PC_FILESIZEBITS:
		val = (ulong_t)-1;
		break;

	default:
		error = EINVAL;
		break;
	}

	if (error == 0)
		*valp = val;
	return (error);
}

/*
 * If shadowing a vnode, apply VOP_SETSECATTR to it.
 * Otherwise, return NOSYS.
 */
int
fifo_setsecattr(struct vnode *vp, vsecattr_t *vsap, int flag, struct cred *crp)
{
	int error;

	/*
	 * The acl(2) system call tries to grab the write lock on the
	 * file when setting an ACL, but fifofs does not implement
	 * VOP_RWLOCK or VOP_RWUNLOCK, so we do it here instead.
	 */
	if (VTOF(vp)->fn_realvp) {
		VOP_RWLOCK(VTOF(vp)->fn_realvp, 1);
		error = VOP_SETSECATTR(VTOF(vp)->fn_realvp, vsap, flag, crp);
		VOP_RWUNLOCK(VTOF(vp)->fn_realvp, 1);
		return (error);
	} else
		return (fs_nosys());
}

/*
 * If shadowing a vnode, apply VOP_GETSECATTR to it. Otherwise, fabricate
 * an ACL from the permission bits that fifo_getattr() makes up.
 */
int
fifo_getsecattr(struct vnode *vp, vsecattr_t *vsap, int flag, struct cred *crp)
{
	if (VTOF(vp)->fn_realvp)
		return (VOP_GETSECATTR(VTOF(vp)->fn_realvp, vsap, flag, crp));
	else
		return (fs_fab_acl(vp, vsap, flag, crp));
}
