/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1990-1997 by Sun Microsystems, Inc. */
/*	  All rights reserved.  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fifosubr.c	1.73	99/10/27 SMI"

/*
 * The routines defined in this file are supporting routines for FIFOFS
 * file sytem type.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kmem.h>
#include <sys/inline.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mode.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/flock.h>
#include <sys/stream.h>
#include <sys/fs/fifonode.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#include <sys/cmn_err.h>
#include <fs/fs_subr.h>
#include <sys/ddi.h>


#if FIFODEBUG
int Fifo_fastmode = 1;		/* pipes/fifos will be opened in fast mode */
int Fifo_verbose = 0;		/* msg when switching out of fast mode */
int Fifohiwat = FIFOHIWAT;	/* Modifiable FIFO high water mark */
#endif

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern struct qinit fifo_strdata;

extern struct vfsops fifovfsops;

static struct vfssw vfw = {
	"fifofs",
	fifoinit,
	&fifovfsops,
	0
};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_fsops;

static struct modlfs modlfs = {
	&mod_fsops, "filesystem for fifo", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlfs, NULL
};


_init()
{
	return (mod_install(&modlinkage));
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Define data structures within this file.
 * XXX should the hash size be configurable ?
 */
#define	FIFOSHFT	5
#define	FIFO_HASHSZ	63

#if ((FIFO_HASHSZ & (FIFO_HASHSZ - 1)) == 0)
#define	FIFOHASH(vp) (((uint_t)(vp) >> FIFOSHFT) & (FIFO_HASHSZ - 1))
#else
#define	FIFOHASH(vp) (((uint_t)(vp) >> FIFOSHFT) % FIFO_HASHSZ)
#endif

fifonode_t	*fifoalloc[FIFO_HASHSZ];
dev_t		fifodev;
struct vfs	*fifovfsp;
int		fifofstype;

struct vfsops fifovfsops = {
	fs_nosys,	/* mount */
	fs_nosys,	/* umount */
	fs_nosys,	/* root */
	fs_nosys,	/* statvfs */
	fs_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys,	/* swapvp */
	fs_freevfs
};

kmutex_t ftable_lock;
static kmutex_t fino_lock;
struct kmem_cache *fnode_cache;
struct kmem_cache *pipe_cache;

static void fifoinsert(fifonode_t *);
static fifonode_t *fifofind(vnode_t *);
static int fifo_connld(struct vnode **, int, cred_t *);
static void fifo_fastturnoff(fifonode_t *);

/*
 * Constructor/destructor routines for fifos and pipes.
 *
 * In the interest of code sharing, we define a common fifodata structure
 * which consists of a fifolock and one or two fnodes.  A fifo contains
 * one fnode; a pipe contains two.  The fifolock is shared by the fnodes,
 * each of which points to it:
 *
 *	--> -->	---------  --- ---
 *	|   |	| lock	|   |	|
 *	|   |	---------   |	|
 *	|   |	|	|  fifo	|
 *	|   --- | fnode	|   |	|
 *	|	|	|   |  pipe
 *	|	---------  ---	|
 *	|	|	|	|
 *	------- | fnode	|	|
 *		|	|	|
 *		---------      ---
 *
 * Since the fifolock is at the beginning of the fifodata structure,
 * the fifolock address is the same as the fifodata address.  Thus,
 * we can determine the fifodata address from any of its member fnodes.
 * This is essential for fifo_inactive.
 *
 * The fnode constructor is designed to handle any fifodata struture,
 * deducing the number of fnodes from the total size.  Thus, the fnode
 * constructor does most of the work for the pipe constructor.
 */
/*ARGSUSED1*/
static int
fnode_constructor(void *buf, void *cdrarg, int kmflags)
{
	fifodata_t *fdp = buf;
	fifolock_t *flp = &fdp->fifo_lock;
	fifonode_t *fnp = &fdp->fifo_fnode[0];
	size_t size = (int)cdrarg;

	mutex_init(&flp->flk_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&flp->flk_wait_cv, NULL, CV_DEFAULT, NULL);
	flp->flk_ocsync = 0;

	while ((char *)fnp < (char *)buf + size) {

		vnode_t *vp = FTOV(fnp);

		fnp->fn_lock = flp;
		fnp->fn_open = 0;
		fnp->fn_dest = fnp;
		fnp->fn_mp = NULL;
		fnp->fn_count = 0;
		fnp->fn_rsynccnt = 0;
		fnp->fn_wsynccnt = 0;
		fnp->fn_insync = 0;

		cv_init(&fnp->fn_wait_cv, NULL, CV_DEFAULT, NULL);
		mutex_init(&vp->v_lock, NULL, MUTEX_DEFAULT, NULL);
		cv_init(&vp->v_cv, NULL, CV_DEFAULT, NULL);

		vp->v_op = &fifo_vnodeops;
		vp->v_stream = NULL;
		vp->v_type = VFIFO;
		vp->v_data = (caddr_t)fnp;
		vp->v_flag = VNOMAP | VNOSWAP;

		fnp++;
	}
	return (0);
}

static void
fnode_destructor(void *buf, void *cdrarg)
{
	fifodata_t *fdp = buf;
	fifolock_t *flp = &fdp->fifo_lock;
	fifonode_t *fnp = &fdp->fifo_fnode[0];
	size_t size = (int)cdrarg;

	mutex_destroy(&flp->flk_lock);
	cv_destroy(&flp->flk_wait_cv);
	ASSERT(flp->flk_ocsync == 0);

	while ((char *)fnp < (char *)buf + size) {

		vnode_t *vp = FTOV(fnp);

		ASSERT(fnp->fn_mp == NULL);
		ASSERT(fnp->fn_count == 0);
		ASSERT(fnp->fn_lock == flp);
		ASSERT(fnp->fn_open == 0);
		ASSERT(fnp->fn_insync == 0);
		ASSERT(fnp->fn_rsynccnt == 0 && fnp->fn_wsynccnt == 0);
		ASSERT(vp->v_op == &fifo_vnodeops);
		ASSERT(vp->v_stream == NULL);
		ASSERT(vp->v_type == VFIFO);
		ASSERT(vp->v_data == (caddr_t)fnp);
		ASSERT(vp->v_flag == (VNOMAP | VNOSWAP));

		cv_destroy(&fnp->fn_wait_cv);
		mutex_destroy(&vp->v_lock);
		cv_destroy(&vp->v_cv);

		fnp++;
	}
}

static int
pipe_constructor(void *buf, void *cdrarg, int kmflags)
{
	fifodata_t *fdp = buf;
	fifonode_t *fnp1 = &fdp->fifo_fnode[0];
	fifonode_t *fnp2 = &fdp->fifo_fnode[1];
	vnode_t *vp1 = FTOV(fnp1);
	vnode_t *vp2 = FTOV(fnp2);

	(void) fnode_constructor(buf, cdrarg, kmflags);

	vp1->v_vfsp	= vp2->v_vfsp		= fifovfsp;
	vp1->v_rdev	= vp2->v_rdev		= fifodev;
	fnp1->fn_realvp	= fnp2->fn_realvp	= NULL;
	fnp1->fn_dest	= fnp2;
	fnp2->fn_dest	= fnp1;

	return (0);
}

static void
pipe_destructor(void *buf, void *cdrarg)
{
#ifdef DEBUG
	fifodata_t *fdp = buf;
	fifonode_t *fnp1 = &fdp->fifo_fnode[0];
	fifonode_t *fnp2 = &fdp->fifo_fnode[1];
	vnode_t *vp1 = FTOV(fnp1);
	vnode_t *vp2 = FTOV(fnp2);

	ASSERT(vp1->v_vfsp == fifovfsp);
	ASSERT(vp2->v_vfsp == fifovfsp);
	ASSERT(vp1->v_rdev == fifodev);
	ASSERT(vp2->v_rdev == fifodev);
#endif
	fnode_destructor(buf, cdrarg);
}

/*
 * Save file system type/index, initialize vfs operations vector, get
 * unique device number for FIFOFS and initialize the FIFOFS hash.
 * Create and initialize a "generic" vfs pointer that will be placed
 * in the v_vfsp field of each pipe's vnode.
 */
int
fifoinit(struct vfssw *vswp, int fstype)
{
	major_t dev;

	fifofstype = fstype;
	vswp->vsw_vfsops = &fifovfsops;
	if ((dev = getudev()) == (major_t)-1) {
		cmn_err(CE_WARN, "fifoinit: can't get unique device number");
		dev = 0;
	}
	fifodev = makedevice(dev, 0);

	fifovfsp = kmem_zalloc(sizeof (struct vfs), KM_SLEEP);
	fifovfsp->vfs_next = NULL;
	fifovfsp->vfs_op = &fifovfsops;
	fifovfsp->vfs_vnodecovered = NULL;
	fifovfsp->vfs_flag = 0;
	fifovfsp->vfs_bsize = 1024;
	fifovfsp->vfs_fstype = fifofstype;
	vfs_make_fsid(&fifovfsp->vfs_fsid, fifodev, fifofstype);
	fifovfsp->vfs_data = NULL;
	fifovfsp->vfs_dev = fifodev;
	fifovfsp->vfs_bcount = 0;

	mutex_init(&ftable_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&fino_lock, NULL, MUTEX_DEFAULT, NULL);

	/*
	 * vnodes are cached aligned
	 */
	fnode_cache = kmem_cache_create("fnode_cache",
		sizeof (fifodata_t) - sizeof (fifonode_t), 32,
		fnode_constructor, fnode_destructor, NULL,
		(void *)(sizeof (fifodata_t) - sizeof (fifonode_t)), NULL, 0);

	pipe_cache = kmem_cache_create("pipe_cache", sizeof (fifodata_t), 32,
		pipe_constructor, pipe_destructor, NULL,
		(void *)(sizeof (fifodata_t)), NULL, 0);

#if FIFODEBUG
	if (Fifohiwat < FIFOHIWAT)
		Fifohiwat = FIFOHIWAT;
#endif /* FIFODEBUG */
	fifo_strdata.qi_minfo->mi_hiwat = Fifohiwat;

	return (0);
}

/*
 * Provide a shadow for a vnode. If vp already has a shadow in the hash list,
 * return its shadow.  Otherwise, create a vnode to shadow vp, hash the
 * new vnode and return its pointer to the caller.
 */
vnode_t *
fifovp(vnode_t *vp, cred_t *crp)
{
	fifonode_t *fnp;
	fifodata_t *fdp;
	vnode_t *newvp;
	struct vattr va;

	ASSERT(vp != NULL);

	fdp = kmem_cache_alloc(fnode_cache, KM_SLEEP);

	mutex_enter(&ftable_lock);

	if ((fnp = fifofind(vp)) != NULL) {
		mutex_exit(&ftable_lock);
#if DEBUG
		fdp->fifo_fnode[0].fn_wcnt = 0;
		fdp->fifo_fnode[0].fn_rcnt = 0;
#endif
		kmem_cache_free(fnode_cache, fdp);
		return (FTOV(fnp));
	}

	fdp->fifo_lock.flk_ref = 1;
	fnp = &fdp->fifo_fnode[0];
	fnp->fn_realvp	= vp;
	fnp->fn_wcnt	= 0;
	fnp->fn_rcnt	= 0;
#if FIFODEBUG
	if (! Fifo_fastmode) {
		fnp->fn_flag	= 0;
	} else {
		fnp->fn_flag	= FIFOFAST;
	}
#else /* FIFODEBUG */
	fnp->fn_flag	= FIFOFAST;
#endif /* FIFODEBUG */

	/*
	 * initialize the times from vp.
	 */
	va.va_mask = AT_TIMES;
	if (VOP_GETATTR(vp, &va, 0, crp) == 0) {
		fnp->fn_atime = va.va_atime.tv_sec;
		fnp->fn_mtime = va.va_mtime.tv_sec;
		fnp->fn_ctime = va.va_ctime.tv_sec;
	} else {
		fnp->fn_atime = 0;
		fnp->fn_mtime = 0;
		fnp->fn_ctime = 0;
	}

	VN_HOLD(vp);
	newvp = FTOV(fnp);
	newvp->v_count		= 1;
	newvp->v_vfsmountedhere	= NULL;
	newvp->v_vfsp		= vp->v_vfsp;
	newvp->v_pages		= NULL;
	newvp->v_rdev		= vp->v_rdev;
	newvp->v_filocks	= NULL;
	newvp->v_shrlocks	= NULL;

	fifoinsert(fnp);
	mutex_exit(&ftable_lock);

	return (FTOV(fnp));
}

/*
 * Create a pipe end by...
 * allocating a vnode-fifonode pair, initializing the fifonode,
 * assigning a unique ino to the fifonode.
 */
void
makepipe(vnode_t **vpp1, vnode_t **vpp2)
{
	fifonode_t *fnp1;
	fifonode_t *fnp2;
	vnode_t *nvp1;
	vnode_t *nvp2;
	fifodata_t *fdp;
	time_t now = hrestime.tv_sec;

	fdp = kmem_cache_alloc(pipe_cache, KM_SLEEP);
	fdp->fifo_lock.flk_ref = 2;
	fnp1 = &fdp->fifo_fnode[0];
	fnp2 = &fdp->fifo_fnode[1];

	fnp1->fn_wcnt	= fnp2->fn_wcnt		= 1;
	fnp1->fn_rcnt	= fnp2->fn_rcnt		= 1;
#if FIFODEBUG
	if (! Fifo_fastmode) {
		fnp1->fn_flag	= fnp2->fn_flag		= ISPIPE;
	} else {
		fnp1->fn_flag	= fnp2->fn_flag		= ISPIPE | FIFOFAST;
	}
#else /* FIFODEBUG */
	fnp1->fn_flag	= fnp2->fn_flag		= ISPIPE | FIFOFAST;
#endif /* FIFODEBUG */
	fnp1->fn_atime	= fnp2->fn_atime	= now;
	fnp1->fn_mtime	= fnp2->fn_mtime	= now;
	fnp1->fn_ctime	= fnp2->fn_ctime	= now;

	*vpp1 = nvp1 = FTOV(fnp1);
	*vpp2 = nvp2 = FTOV(fnp2);
	nvp1->v_count		= nvp2->v_count		= 1;
	nvp1->v_vfsmountedhere	= nvp2->v_vfsmountedhere = NULL;
	nvp1->v_pages		= nvp2->v_pages		= NULL;
	nvp1->v_filocks		= nvp2->v_filocks	= NULL;
	nvp1->v_shrlocks	= nvp2->v_shrlocks	= NULL;

}

/*
 * Attempt to establish a unique pipe id.  Only un-named pipes use this
 * routine.
 */
ino_t
fifogetid(void)
{
	static ino_t fifo_ino = 0;
	ino_t fino;

	mutex_enter(&fino_lock);
	fino = fifo_ino++;
	mutex_exit(&fino_lock);
	return (fino);
}


/*
 * Stream a pipe/FIFO.
 * The FIFOCONNLD flag is used when CONNLD has been pushed on the stream.
 * If the flag is set, a new vnode is created by calling fifo_connld().
 * Connld logic was moved to fifo_connld() to speed up the open
 * operation, simplify the connld/fifo interaction, and remove inherent
 * race conditions between the connld module and fifos.
 * This routine is single threaded for two reasons.
 * 1) connld requests are synchronous; that is, they must block
 *    until the server does an I_RECVFD (oh, well).  Single threading is
 *    the simplest way to accomplish this.
 * 2) fifo_close() must not send M_HANGUP or M_ERROR while we are
 *    in stropen. Stropen() has a tendency to reset things and
 *    we would like streams to remember that a hangup occurred.
 */
int
fifo_stropen(vnode_t **vpp, int flag, cred_t *crp, int dotwist, int lockheld)
{
	int error = 0;
	vnode_t *oldvp = *vpp;
	fifonode_t *fnp = VTOF(*vpp);
	dev_t pdev = 0;
	int firstopen = 0;
	fifolock_t *fn_lock;

	fn_lock = fnp->fn_lock;
	if (!lockheld)
		mutex_enter(&fn_lock->flk_lock);
	ASSERT(MUTEX_HELD(&fnp->fn_lock->flk_lock));

	/*
	 * FIFO is in the process of opening. Wait for it
	 * to complete before starting another open on it
	 * This prevents races associated with connld open
	 */
	while (fnp->fn_flag & FIFOOPEN) {
		if (!cv_wait_sig(&fnp->fn_wait_cv, &fn_lock->flk_lock)) {
			fifo_cleanup(oldvp, flag);
			if (!lockheld)
				mutex_exit(&fn_lock->flk_lock);
			return (EINTR);
		}
	}

	/*
	 * The other end of the pipe is almost closed so
	 * reject any other open on this end of the pipe
	 * This only happens with a pipe mounted under namefs
	 */
	if ((fnp->fn_flag & (FIFOCLOSE|ISPIPE)) == (FIFOCLOSE|ISPIPE)) {
		fifo_cleanup(oldvp, flag);
		cv_broadcast(&fnp->fn_wait_cv);
		if (!lockheld)
			mutex_exit(&fn_lock->flk_lock);
		return (ENXIO);
	}

	fnp->fn_flag |= FIFOOPEN;

	/*
	 * can't allow close to happen while we are
	 * in the middle of stropen().
	 * M_HANGUP and M_ERROR could leave the stream in a strange state
	 */
	while (fn_lock->flk_ocsync)
		cv_wait(&fn_lock->flk_wait_cv, &fn_lock->flk_lock);

	fn_lock->flk_ocsync = 1;

	if (fnp->fn_flag & FIFOCONNLD) {
		/*
		 * This is a reopen, so we should release the fifo lock
		 * just in case some strange module pushed on connld
		 * has some odd side effect.
		 * Note: this stropen is on the oldvp.  It will
		 * have no impact on the connld vp returned and
		 * strclose() will only be called when we release
		 * flk_ocsync
		 */
		mutex_exit(&fn_lock->flk_lock);
		if ((error = stropen(oldvp, &pdev, flag, 0, crp)) != 0) {
			mutex_enter(&fn_lock->flk_lock);
			fifo_cleanup(oldvp, flag);
			fn_lock->flk_ocsync = 0;
			cv_broadcast(&fn_lock->flk_wait_cv);
			goto out;
		}
		/*
		 * streams open done, allow close on other end if
		 * required.  Do this now.. it could
		 * be a very long time before fifo_connld returns.
		 */
		mutex_enter(&fn_lock->flk_lock);
		/*
		 * we need to fake an open here so that if this
		 * end of the pipe closes, we don't loose the
		 * stream head (kind of like single threading
		 * open and close for this end of the pipe)
		 * We'll need to call fifo_close() to do clean
		 * up in case this end of the pipe was closed
		 * down while we were in fifo_connld()
		 */
		ASSERT(fnp->fn_open > 0);
		fnp->fn_open++;
		fn_lock->flk_ocsync = 0;
		cv_broadcast(&fn_lock->flk_wait_cv);
		mutex_exit(&fn_lock->flk_lock);
		/*
		 * Connld has been pushed onto the pipe
		 * Create new pipe on behalf of connld
		 */
		if (error = fifo_connld(vpp, flag, crp)) {
			(void) fifo_close(oldvp, flag, 1, 0, crp);
			mutex_enter(&fn_lock->flk_lock);
			goto out;
		}
		/*
		 * undo fake open.  We need to call fifo_close
		 * because some other thread could have done
		 * a close and detach of the named pipe while
		 * we were in fifo_connld(), so
		 * we want to make sure the close completes (yuk)
		 */
		(void) fifo_close(oldvp, flag, 1, 0, crp);
		/*
		 * fifo_connld has changed the vp, so we
		 * need to re-initialize locals
		 */
		fnp = VTOF(*vpp);
		fn_lock = fnp->fn_lock;
		mutex_enter(&fn_lock->flk_lock);
	} else {
		/*
		 * release lock in case there are modules pushed that
		 * could have some strange side effect
		 */

		mutex_exit(&fn_lock->flk_lock);

		/*
		 * If this is the first open of a fifo (dotwist
		 * will be non-zero) we will need to twist the queues.
		 */
		if (oldvp->v_stream == NULL)
			firstopen = 1;


		/*
		 * normal open of pipe/fifo
		 */

		if ((error = stropen(oldvp, &pdev, flag, 0, crp)) != 0) {
			mutex_enter(&fn_lock->flk_lock);
			fifo_cleanup(oldvp, flag);
			ASSERT(fnp->fn_open != 0 || oldvp->v_stream == NULL);
			fn_lock->flk_ocsync = 0;
			cv_broadcast(&fn_lock->flk_wait_cv);
			goto out;
		}
		mutex_enter(&fn_lock->flk_lock);

		/*
		 * twist the ends of the fifo together
		 */

		if (dotwist && firstopen) {
			queue_t *wq = strvp2wq(*vpp);
			(void) str_mate(wq, wq);
		}
		/*
		 * Show that this open has succeeded
		 * and allow closes or other opens to proceed
		 */
		fnp->fn_open++;
		fn_lock->flk_ocsync = 0;
		cv_broadcast(&fn_lock->flk_wait_cv);
	}
out:
	fnp->fn_flag &= ~FIFOOPEN;
	if (error == 0) {
		fnp->fn_flag |= FIFOISOPEN;
		/*
		 * If this is a FIFO and has the close flag set
		 * and there are now writers, clear the close flag
		 * Note: close flag only gets set when last writer
		 * on a FIFO goes away.
		 */
		if (((fnp->fn_flag & (ISPIPE|FIFOCLOSE)) == FIFOCLOSE) &&
		    fnp->fn_wcnt > 0)
			fnp->fn_flag &= ~FIFOCLOSE;
	}
	cv_broadcast(&fnp->fn_wait_cv);
	if (!lockheld)
		mutex_exit(&fn_lock->flk_lock);
	return (error);
}

/*
 * Clean up the state of a FIFO and/or mounted pipe in the
 * event that a fifo_open() was interrupted while the
 * process was blocked.
 */
void
fifo_cleanup(vnode_t *vp, int flag)
{
	fifonode_t *fnp = VTOF(vp);

	ASSERT(MUTEX_HELD(&fnp->fn_lock->flk_lock));

	cleanlocks(vp, curproc->p_pid, 0);
	cleanshares(vp, curproc->p_pid);
	if (flag & FREAD) {
		fnp->fn_rcnt--;
	}
	if (flag & FWRITE) {
		fnp->fn_wcnt--;
	}
	cv_broadcast(&fnp->fn_wait_cv);
}


/*
 * Insert a fifonode-vnode pair onto the fifoalloc hash list.
 */
static void
fifoinsert(fifonode_t *fnp)
{
	int idx = FIFOHASH(fnp->fn_realvp);

	/*
	 * We don't need to hold fn_lock since we're holding ftable_lock and
	 * this routine is only called right after we've allocated an fnode.
	 * FIFO is inserted at head of NULL terminated doubly linked list.
	 */

	ASSERT(MUTEX_HELD(&ftable_lock));
	fnp->fn_backp = NULL;
	fnp->fn_nextp = fifoalloc[idx];
	fifoalloc[idx] = fnp;
	if (fnp->fn_nextp)
		fnp->fn_nextp->fn_backp = fnp;
}

/*
 * Find a fifonode-vnode pair on the fifoalloc hash list.
 * vp is a vnode to be shadowed. If it's on the hash list,
 * it already has a shadow, therefore return its corresponding
 * fifonode.
 */
static fifonode_t *
fifofind(vnode_t *vp)
{
	fifonode_t *fnode;

	ASSERT(MUTEX_HELD(&ftable_lock));
	for (fnode = fifoalloc[FIFOHASH(vp)]; fnode; fnode = fnode->fn_nextp) {
		if (fnode->fn_realvp == vp) {
			VN_HOLD(FTOV(fnode));
			return (fnode);
		}
	}
	return (NULL);
}

/*
 * Remove a fifonode-vnode pair from the fifoalloc hash list.
 * This routine is called from the fifo_inactive() routine when a
 * FIFO is being released.
 * If the link to be removed is the only link, set fifoalloc to NULL.
 */
void
fiforemove(fifonode_t *fnp)
{
	int idx = FIFOHASH(fnp->fn_realvp);
	fifonode_t *fnode;

	ASSERT(MUTEX_HELD(&ftable_lock));
	fnode = fifoalloc[idx];
	/*
	 * fast path... only 1 FIFO in this list entry
	 */
	if (fnode != NULL && fnode == fnp &&
		!fnode->fn_nextp && !fnode->fn_backp) {
			fifoalloc[idx] = NULL;
	} else {

		for (;  fnode;  fnode = fnode->fn_nextp) {
			if (fnode == fnp) {
				/*
				 * if we are first entry
				 */
				if (fnp == fifoalloc[idx])
					fifoalloc[idx] = fnp->fn_nextp;
				if (fnode->fn_nextp)
					fnode->fn_nextp->fn_backp =
						fnode->fn_backp;
				if (fnode->fn_backp)
					fnode->fn_backp->fn_nextp =
						fnode->fn_nextp;
				break;
			}
		}
	}
}

/*
 * Flush all data from a fifo's message queue
 */

void
fifo_fastflush(fifonode_t *fnp)
{
	mblk_t *bp;
	ASSERT(MUTEX_HELD(&fnp->fn_lock->flk_lock));

	if ((bp = fnp->fn_mp) != NULL) {
		fnp->fn_mp = NULL;
		fnp->fn_count = 0;
		freemsg(bp);
	}
	fifo_wakewriter(fnp->fn_dest, fnp->fn_lock);
}

/*
 * Note:  This routine is single threaded
 *  Protected by FIFOOPEN flag (i.e. flk_lock is not held)
 *  Upon successful completion, the original fifo is unlocked
 *  and FIFOOPEN is cleared for the original vpp.
 *  The new fifo returned has FIFOOPEN set.
 */
static int
fifo_connld(struct vnode **vpp, int flag, cred_t *crp)
{
	struct vnode *vp1;
	struct vnode *vp2;
	struct fifonode *oldfnp;
	struct fifonode *fn_dest;
	int error;
	struct file *filep;
	struct fifolock *fn_lock;
	cred_t *c;

	/*
	 * Get two vnodes that will represent the pipe ends for the new pipe.
	 */
	makepipe(&vp1, &vp2);

	/*
	 * Allocate a file descriptor and file pointer for one of the pipe
	 * ends. The file descriptor will be used to send that pipe end to
	 * the process on the other end of this stream. Note that we get
	 * the file structure only, there is no file list entry allocated.
	 */
	if (error = falloc(vp1, FWRITE|FREAD, &filep, NULL)) {
		VN_RELE(vp1);
		VN_RELE(vp2);
		return (error);
	}
	mutex_exit(&filep->f_tlock);
	oldfnp = VTOF(*vpp);
	fn_lock = oldfnp->fn_lock;
	fn_dest = oldfnp->fn_dest;

	/*
	 * Create two new stream heads and attach them to the two vnodes for
	 * the new pipe.
	 */
	if ((error = fifo_stropen(&vp1, FREAD|FWRITE, filep->f_cred, 0, 0)) !=
	    0 ||
	    (error = fifo_stropen(&vp2, flag, filep->f_cred, 0, 0)) != 0) {
#if DEBUG
		cmn_err(CE_NOTE, "fifo stropen failed error 0x%x",
			error);
#endif
		/*
		 * this will call fifo_close and VN_RELE on vp1
		 */
		(void) closef(filep);
		VN_RELE(vp2);
		return (error);
	}

	/*
	 * twist the ends of the pipe together
	 */

	(void) str_mate(strvp2wq(vp1), strvp2wq(vp2));

	/*
	 * Set our end to busy in open
	 * Note: Don't need lock around this because we're the only
	 * one who knows about it
	 */
	VTOF(vp2)->fn_flag |= FIFOOPEN;

	mutex_enter(&fn_lock->flk_lock);

	fn_dest->fn_flag |= FIFOSEND;
	/*
	 * check to make sure neither end of pipe has gone away
	 */
	if (!(fn_dest->fn_flag & FIFOISOPEN)) {
		error = ENXIO;
		fn_dest->fn_flag &= ~FIFOSEND;
		mutex_exit(&fn_lock->flk_lock);
		/*
		 * this will call fifo_close and VN_RELE on vp1
		 */
		goto out;
	}
	mutex_exit(&fn_lock->flk_lock);

	/*
	 * send the file descriptor to other end of pipe
	 */
	if (error = do_sendfp((*vpp)->v_stream, filep, crp)) {
		mutex_enter(&fn_lock->flk_lock);
		fn_dest->fn_flag &= ~FIFOSEND;
		mutex_exit(&fn_lock->flk_lock);
		/*
		 * this will call fifo_close and VN_RELE on vp1
		 */
		goto out;
	}

	mutex_enter(&fn_lock->flk_lock);
	/*
	 * Wait for other end to receive file descriptor
	 * FIFOCLOSE indicates that one or both sides of the pipe
	 * have gone away.
	 */
	while ((fn_dest->fn_flag & (FIFOCLOSE | FIFOSEND)) == FIFOSEND) {
		if (!cv_wait_sig(&oldfnp->fn_wait_cv, &fn_lock->flk_lock)) {
			error = EINTR;
			fn_dest->fn_flag &= ~FIFOSEND;
			mutex_exit(&fn_lock->flk_lock);
			goto out;
		}
	}
	/*
	 * If either end of pipe has gone away and the other end did not
	 * receive pipe, reject the connld open
	 */
	if ((fn_dest->fn_flag & FIFOSEND)) {
		error = ENXIO;
		fn_dest->fn_flag &= ~FIFOSEND;
		mutex_exit(&fn_lock->flk_lock);
		goto out;
	}

	oldfnp->fn_flag &= ~FIFOOPEN;
	cv_broadcast(&oldfnp->fn_wait_cv);
	mutex_exit(&fn_lock->flk_lock);

	VN_RELE(*vpp);
	*vpp = vp2;
	(void) closef(filep);
	return (0);
out:
	c = crdup(filep->f_cred);
	(void) closef(filep);
	VTOF(vp2)->fn_flag &= ~FIFOOPEN;
	(void) fifo_close(vp2, flag, 1, (offset_t)0, c);
	crfree(c);
	VN_RELE(vp2);
	return (error);
}

/*
 * Disable fastpath mode.
 */
void
fifo_fastoff(fifonode_t *fnp)
{
	ASSERT(MUTEX_HELD(&fnp->fn_lock->flk_lock));
	ASSERT(FTOV(fnp)->v_stream);


	if (!(fnp->fn_flag & FIFOFAST))
		return;
#if FIFODEBUG
	if (Fifo_verbose)
		cmn_err(CE_NOTE, "Fifo reverting to streams mode\n");
#endif

	fifo_fastturnoff(fnp);
	if (fnp->fn_flag & ISPIPE) {
		fifo_fastturnoff(fnp->fn_dest);
	}
}


/*
 * flk_lock must be held while calling fifo_fastturnoff() to
 * preserve data ordering (no reads or writes allowed)
 */

static void
fifo_fastturnoff(fifonode_t *fnp)
{
	fifonode_t *fn_dest = fnp->fn_dest;
	mblk_t	*fn_mp;
	int	fn_flag;

	ASSERT(MUTEX_HELD(&fnp->fn_lock->flk_lock));
	/*
	 * Note: This end can't be closed if there
	 * is stuff in fn_mp
	 */
	while ((fn_mp = fnp->fn_mp) != NULL) {
		ASSERT(fnp->fn_flag & FIFOISOPEN);
		ASSERT(FTOV(fnp)->v_stream != NULL);
		ASSERT(FTOV(fnp)->v_stream->sd_wrq != NULL);
		ASSERT(RD(FTOV(fnp)->v_stream->sd_wrq) != NULL);
		ASSERT(strvp2wq(FTOV(fnp)) != NULL);
		fnp->fn_mp = NULL;
		fnp->fn_count = 0;
		mutex_exit(&fnp->fn_lock->flk_lock);
		put(RD(strvp2wq(FTOV(fnp))), fn_mp);
		mutex_enter(&fnp->fn_lock->flk_lock);
	}

	/*
	 * Need to re-issue any pending poll requests
	 * so that the STREAMS framework sees them
	 * Writers would be waiting on fnp and readers on fn_dest
	 */
	if ((fnp->fn_flag & (FIFOISOPEN | FIFOPOLLW)) ==
	    (FIFOISOPEN | FIFOPOLLW)) {
		strpollwakeup(FTOV(fnp), POLLWRNORM);
	}
	fn_flag = fn_dest->fn_flag;
	if ((fn_flag & FIFOISOPEN) == FIFOISOPEN) {
		if ((fn_flag & (FIFOPOLLR | FIFOPOLLRBAND))) {
			strpollwakeup(FTOV(fn_dest), POLLIN|POLLRDNORM);
		}
	}
	/*
	 * wake up any sleeping processes so they can notice we went
	 * to streams mode
	 */
	fnp->fn_flag &= ~(FIFOFAST|FIFOWANTW|FIFOWANTR);
	cv_broadcast(&fnp->fn_wait_cv);
}

/*
 * Alternative version of fifo_fastoff()
 * optimized for putmsg/getmsg.
 */
void
fifo_vfastoff(vnode_t *vp)
{
	fifonode_t	*fnp = VTOF(vp);

	if (!(fnp->fn_flag & FIFOFAST))
		return;

	mutex_enter(&fnp->fn_lock->flk_lock);
	fifo_fastoff(fnp);
	mutex_exit(&fnp->fn_lock->flk_lock);
}

/*
 * Wake any sleeping writers, poll and send signals if necessary
 * This module is only called when we drop below the hi water mark
 * FIFOWANTW indicates that a process is sleeping in fifo_write()
 * FIFOHIWATW indicates that we have either attempted a poll or
 * non-blocking write and were over the high water mark
 * This routine assumes a low water mark of 0.
 */

void
fifo_wakewriter(fifonode_t *fn_dest, fifolock_t *fn_lock)
{
	int fn_dflag = fn_dest->fn_flag;

	ASSERT(MUTEX_HELD(&fn_lock->flk_lock));
	ASSERT(fn_dest->fn_dest->fn_count < Fifohiwat);
	if ((fn_dflag & FIFOWANTW)) {
		cv_broadcast(&fn_dest->fn_wait_cv);
	}
	if ((fn_dflag & (FIFOHIWATW | FIFOISOPEN)) ==
	    (FIFOHIWATW | FIFOISOPEN)) {
		if (fn_dflag & FIFOSETSIG) {
			str_sendsig(FTOV(fn_dest), S_WRNORM, 0, 0);
		}
		if (fn_dflag & FIFOPOLLW) {
			strpollwakeup(FTOV(fn_dest), POLLWRNORM);
		}
	}
	/*
	 * FIFOPOLLW can't be set without setting FIFOHIWAT
	 * This allows us to clear both here.
	 */
	fn_dest->fn_flag = fn_dflag & ~(FIFOWANTW | FIFOHIWATW | FIFOPOLLW);
}

/*
 * wake up any sleeping readers, poll or send signal if needed
 * FIFOWANTR indicates that a process is waiting in fifo_read() for data
 * FIFOSETSIG indicates that SIGPOLL should be sent to process
 * FIFOPOLLR indicates that a poll request for reading on the fifo was made
 */

void
fifo_wakereader(fifonode_t *fn_dest, fifolock_t *fn_lock)
{
	int fn_dflag = fn_dest->fn_flag;

	ASSERT(MUTEX_HELD(&fn_lock->flk_lock));
	if (fn_dflag & FIFOWANTR) {
		cv_broadcast(&fn_dest->fn_wait_cv);
	}
	if (fn_dflag & FIFOISOPEN) {
		if (fn_dflag & FIFOSETSIG) {
			str_sendsig(FTOV(fn_dest), S_INPUT, 0, 0);
			str_sendsig(FTOV(fn_dest), S_RDNORM, 0, 0);
		}
		if (fn_dflag & FIFOPOLLR) {
			strpollwakeup(FTOV(fn_dest), POLLIN|POLLRDNORM);
		}
	}
	fn_dest->fn_flag = fn_dflag & ~(FIFOWANTR | FIFOPOLLR);
}
