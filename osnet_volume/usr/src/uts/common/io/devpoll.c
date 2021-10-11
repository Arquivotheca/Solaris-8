/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)devpoll.c	1.7	99/11/08 SMI"

#include <sys/types.h>
#include <sys/devops.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/sunddi.h>
#include <sys/stat.h>
#include <sys/poll_impl.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/mkdev.h>
#include <sys/debug.h>
#include <sys/file.h>
#include <sys/sysmacros.h>
#include <sys/bitmap.h>
#include <sys/devpoll.h>


#define	RESERVED	1

extern int nodev();
extern int nulldev();

/* local data struct */
static	dp_entry_t	**devpolltbl; 	/* dev poll entries */
static	size_t		dptblsize;

static	kmutex_t	devpoll_lock;	/* lock protecting dev tbl */
int			devpoll_init;	/* is /dev/poll initialized already */

/* device local functions */

static int dpopen(dev_t *devp, int flag, int otyp, cred_t *credp);
static int dpwrite(dev_t dev, struct uio *uiop, cred_t *credp);
static int dpioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp,
    int *rvalp);
static int dppoll(dev_t dev, short events, int anyyet, short *reventsp,
    struct pollhead **phpp);
static int dpclose(dev_t dev, int flag, int otyp, cred_t *credp);
static dev_info_t *dpdevi;


static struct cb_ops    dp_cb_ops = {
	dpopen,			/* open */
	dpclose,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	dpwrite,		/* write */
	dpioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	dppoll,			/* poll */
	nodev,			/* prop_op */
	(struct streamtab *)0,	/* streamtab */
	D_NEW | D_MP		/* flags */
};

static int dpidentify(dev_info_t *);
static int dpattach(dev_info_t *, ddi_attach_cmd_t);
static int dpdetach(dev_info_t *, ddi_detach_cmd_t);
static int dpinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);

static struct dev_ops dp_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt */
	dpinfo,			/* info */
	dpidentify,		/* identify */
	nulldev,		/* probe */
	dpattach,		/* attach */
	dpdetach,		/* detach */
	nodev,			/* reset */
	&dp_cb_ops,		/* driver operations */
	(struct bus_ops *)NULL, /* bus operations */
	nulldev			/* power */
};


static struct modldrv modldrv = {
	&mod_driverops,		/* type of module - a driver */
	"Dev Poll driver",
	&dp_ops,
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

/*
 * Locking Design
 *
 * The /dev/poll driver shares most of its code with poll sys call whose
 * code is in common/syscall/poll.c. In poll(2) design, the pollcache
 * structure is per lwp. An implicit assumption is made there that some
 * portion of pollcache will never be touched by other lwps. E.g., in
 * poll(2) design, no lwp will ever need to grow bitmap of other lwp.
 * This assumption is not true for /dev/poll; hence the need for extra
 * locking.
 *
 * To allow more paralellism, each /dev/poll file descriptor (indexed by
 * minor number) has its own lock. Since read (dpioctl) is a much more
 * frequent operation than write, we want to allow multiple reads on same
 * /dev/poll fd. However, we prevent writes from being starved by giving
 * priority to write operation. Theoretically writes can starve reads as
 * well. But in pratical sense this is not important because (1) writes
 * happens less often than reads, and (2) write operation defines the
 * content of poll fd a cache set. If writes happens so often that they
 * can starve reads, that means the cached set is very unstable. It may
 * not make sense to read an unstable cache set anyway. Therefore, the
 * writers starving readers case is not handled in this design.
 */

int
_init()
{
	int	error;

	dptblsize = DEVPOLLSIZE;
	devpolltbl = kmem_zalloc(sizeof (caddr_t) * dptblsize, KM_SLEEP);
	mutex_init(&devpoll_lock, NULL, MUTEX_DEFAULT, NULL);
	devpoll_init = 1;
	if ((error = mod_install(&modlinkage)) != 0) {
		mutex_destroy(&devpoll_lock);
		kmem_free(devpolltbl, sizeof (caddr_t) * dptblsize);
		devpoll_init = 0;
	}
	return (error);
}

int
_fini()
{
	int error;

	if ((error = mod_remove(&modlinkage)) != 0) {
		return (error);
	}
	mutex_destroy(&devpoll_lock);
	kmem_free(devpolltbl, sizeof (caddr_t) * dptblsize);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
dpidentify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "poll") == 0) {
		return (DDI_IDENTIFIED);
	} else {
		return (DDI_NOT_IDENTIFIED);
	}
}

/*ARGSUSED*/
static int
dpattach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (ddi_create_minor_node(devi, "poll", S_IFCHR, 0, NULL, NULL)
	    == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	dpdevi = devi;
	return (DDI_SUCCESS);
}

static int
dpdetach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
dpinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)dpdevi;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*
 * dp_pcache_poll has similar logic to pcache_poll() in poll.c. The major
 * differences are: (1) /dev/poll requires scanning the bitmap starting at
 * where it was stopped last time, instead of always starting from 0,
 * (2) since user may not have cleaned up the cached fds when they are
 * closed, some polldats in cache may refer to closed or reused fds. We
 * need to check for those cases.
 *
 * NOTE: Upon closing an fd, automatic poll cache cleanup is done for
 *	 poll(2) caches but NOT for /dev/poll caches. So expect some
 *	 stale entries!
 */
static int
dp_pcache_poll(pollfd_t *pfdp, pollcache_t *pcp, nfds_t nfds, int *fdcntp)
{
	int		start, ostart, end;
	int		fdcnt, fd;
	boolean_t 	done;
	file_t		*fp;
	short		revent;
	boolean_t	no_wrap;
	pollhead_t	*php;
	polldat_t	*pdp;
	int		error = 0;

	ASSERT(MUTEX_HELD(&pcp->pc_lock));
	if (pcp->pc_bitmap == NULL) {
		/*
		 * No Need to search because no poll fd
		 * has been cached.
		 */
		return (error);
	}
retry:
	start = ostart = pcp->pc_mapstart;
	end = pcp->pc_mapend;
	php = NULL;

	if (start == 0) {
		/*
		 * started from every begining, no need to wrap around.
		 */
		no_wrap = B_TRUE;
	} else {
		no_wrap = B_FALSE;
	}
	done = B_FALSE;
	fdcnt = 0;
	while ((fdcnt < nfds) && !done) {
		php = NULL;
		revent = 0;
		/*
		 * Examine the bit map in a circular fashion
		 * to avoid starvation. Always resume from
		 * last stop. Scan till end of the map. Then
		 * wrap around.
		 */
		fd = bt_getlowbit(pcp->pc_bitmap, start, end);
		ASSERT(fd <= end);
		if (fd >= 0) {
			if (fd == end) {
				if (no_wrap) {
					done = B_TRUE;
				} else {
					start = 0;
					end = ostart - 1;
					no_wrap = B_TRUE;
				}
			} else {
				start = fd + 1;
			}
			pdp = pcache_lookup_fd(pcp, fd);
			ASSERT(pdp != NULL);
			ASSERT(pdp->pd_fd == fd);
			if (pdp->pd_fp == NULL) {
				/*
				 * The fd is POLLREMOVed. This fd is
				 * logically no longer cached. So move
				 * on to the next one.
				 */
				continue;
			}
			if ((fp = getf(fd)) == NULL) {
				/*
				 * The fd has been closed, but user has not
				 * done a POLLREMOVE on this fd yet. Instead
				 * of cleaning it here implicitly, we return
				 * POLLNVAL. This is consistent with poll(2)
				 * polling a closed fd. Hope this will remind
				 * user to do a POLLREMOVE.
				 */
				pfdp[fdcnt].fd = fd;
				pfdp[fdcnt].revents = POLLNVAL;
				fdcnt++;
				continue;
			}
			if (fp != pdp->pd_fp) {
				/*
				 * user is polling on a cached fd which was
				 * closed and then reused. Unfortunately
				 * there is no good way to inform user.
				 * If the file struct is also reused, we
				 * may not be able to detect the fd reuse
				 * at all.  As long as this does not
				 * cause system failure and/or memory leak,
				 * we will play along. Man page states if
				 * user does not clean up closed fds, polling
				 * results will be indeterministic.
				 *
				 * XXX - perhaps log the detection of fd
				 *	 reuse?
				 */
				pdp->pd_fp = fp;
			}
			/*
			 * XXX - pollrelock() logic needs to know which
			 * which pollcache lock to grab. It'd be a
			 * cleaner solution if we could pass pcp as
			 * an arguement in VOP_POLL interface instead
			 * of implicitly passing it using thread_t
			 * struct. On the other hand, changing VOP_POLL
			 * interface will require all driver/file system
			 * poll routine to change. May want to revisit
			 * the tradeoff later.
			 */
			curthread->t_pollcache = pcp;
			error = VOP_POLL(fp->f_vnode, pdp->pd_events, 0,
			    &revent, &php);
			curthread->t_pollcache = NULL;
			releasef(fd);
			if (error != 0) {
				break;
			}
			/*
			 * layered devices (e.g. console driver)
			 * may change the vnode and thus the pollhead
			 * pointer out from underneath us.
			 */
			if (php != NULL && pdp->pd_php != NULL &&
			    php != pdp->pd_php) {
				pollhead_delete(pdp->pd_php, pdp);
				pdp->pd_php = php;
				pollhead_insert(php, pdp);
				/*
				 * The bit should still be set.
				 */
				ASSERT(BT_TEST(pcp->pc_bitmap, fd));
				goto retry;
			}

			if (revent != 0) {
				pfdp[fdcnt].fd = fd;
				pfdp[fdcnt].events = pdp->pd_events;
				pfdp[fdcnt].revents = revent;
				fdcnt++;
			} else if (php != NULL) {
				/*
				 * We clear a bit or cache a poll fd if
				 * the driver returns a poll head ptr,
				 * which is expected in the case of 0
				 * revents. Some buggy driver may return
				 * NULL php pointer with 0 revents. In
				 * this case, we just treat the driver as
				 * "noncachable" and not clearing the bit
				 * in bitmap.
				 */
				if ((pdp->pd_php != NULL) &&
				    ((pcp->pc_flag & T_POLLWAKE) == 0)) {
					BT_CLEAR(pcp->pc_bitmap, fd);
				}
				if (pdp->pd_php == NULL) {
					pollhead_insert(php, pdp);
					pdp->pd_php = php;
				}
			}
		} else {
			/*
			 * No bit set in the range. Check for wrap around.
			 */
			if (!no_wrap) {
				start = 0;
				end = ostart - 1;
				no_wrap = B_TRUE;
			} else {
				done = B_TRUE;
			}
		}
	}

	if (!done) {
		pcp->pc_mapstart = start;
	}
	ASSERT(*fdcntp == 0);
	*fdcntp = fdcnt;
	return (error);
}

/*ARGSUSED*/
static int
dpopen(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	minor_t		minordev;
	dp_entry_t	*dpep;
	pollcache_t	*pcp;

	ASSERT(devpoll_init);
	ASSERT(dptblsize <= MAXMIN);
	mutex_enter(&devpoll_lock);
	for (minordev = 0; minordev < dptblsize; minordev++) {
		if (devpolltbl[minordev] == NULL) {
			devpolltbl[minordev] = (dp_entry_t *)RESERVED;
			break;
		}
	}
	if (minordev == dptblsize) {
		dp_entry_t	**newtbl;
		size_t		oldsize;

		/*
		 * Used up every entry in the existing devpoll table.
		 * Grow the table by DEVPOLLSIZE.
		 */
		if ((oldsize = dptblsize) >= MAXMIN) {
			mutex_exit(&devpoll_lock);
			return (ENXIO);
		}
		dptblsize += DEVPOLLSIZE;
		if (dptblsize > MAXMIN) {
			dptblsize = MAXMIN;
		}
		newtbl = kmem_zalloc(sizeof (caddr_t) * dptblsize, KM_SLEEP);
		bcopy(devpolltbl, newtbl, sizeof (caddr_t) * oldsize);
		kmem_free(devpolltbl, sizeof (caddr_t) * oldsize);
		devpolltbl = newtbl;
		devpolltbl[minordev] = (dp_entry_t *)RESERVED;
	}
	mutex_exit(&devpoll_lock);

	dpep = kmem_zalloc(sizeof (dp_entry_t), KM_SLEEP);
	/*
	 * allocate a pollcache skeleton here. Delay allocating bitmap
	 * structures until dpwrite() time, since we don't know the
	 * optimal size yet.
	 */
	pcp = pcache_alloc();
	dpep->dpe_pcache = pcp;
	pcp->pc_pid = curproc->p_pid;
	*devp = makedevice(getmajor(*devp), minordev);  /* clone the driver */
	mutex_enter(&devpoll_lock);
	ASSERT(minordev < dptblsize);
	ASSERT(devpolltbl[minordev] == (dp_entry_t *)RESERVED);
	devpolltbl[minordev] = dpep;
	mutex_exit(&devpoll_lock);
	return (0);
}

/*
 * Write to dev/poll add/remove fd's to/from a cached poll fd set,
 * or change poll events for a watched fd.
 */
/*ARGSUSED*/
static int
dpwrite(dev_t dev, struct uio *uiop, cred_t *credp)
{
	minor_t 	minor;
	dp_entry_t	*dpep;
	pollcache_t	*pcp;
	pollfd_t	*pollfdp, *pfdp;
	int		error;
	ssize_t		uiosize;
	nfds_t		pollfdnum;
	struct pollhead	*php = NULL;
	polldat_t	*pdp;
	int		fd;
	file_t		*fp;

	minor = getminor(dev);

	mutex_enter(&devpoll_lock);
	ASSERT(minor < dptblsize);
	dpep = devpolltbl[minor];
	ASSERT(dpep != NULL);
	mutex_exit(&devpoll_lock);
	pcp = dpep->dpe_pcache;
	if (curproc->p_pid != pcp->pc_pid) {
		return (EACCES);
	}
	/*
	 * copy in the pollfd array. walk thru the array and
	 * add each polled fd to the cached set.
	 */
	pollfdp = kmem_alloc(uiop->uio_resid, KM_SLEEP);
	uiosize = uiop->uio_resid;
	pollfdnum = uiosize / sizeof (pollfd_t);
	if (pollfdnum > (uint_t)P_CURLIMIT(curproc, RLIMIT_NOFILE)) {
		return (set_errno(EINVAL));
	}
	if (error = uiomove((caddr_t)pollfdp, uiosize, UIO_WRITE, uiop)) {
		kmem_free(pollfdp, uiosize);
		return (error);
	}
	/*
	 * We are about to enter the core portion of dpwrite(). Make sure this
	 * write has exclusive access in this portion of the code, i.e., no
	 * other writers in this code and no other readers in dpioctl.
	 */
	mutex_enter(&dpep->dpe_lock);
	dpep->dpe_writerwait++;
	while (dpep->dpe_refcnt != 0) {
		if (!cv_wait_sig_swap(&dpep->dpe_cv, &dpep->dpe_lock)) {
			dpep->dpe_writerwait--;
			mutex_exit(&dpep->dpe_lock);
			return (set_errno(EINTR));
		}
	}
	dpep->dpe_writerwait--;
	dpep->dpe_flag |= DP_WRITER_PRESENT;
	dpep->dpe_refcnt++;
	mutex_exit(&dpep->dpe_lock);

	mutex_enter(&pcp->pc_lock);
	if (pcp->pc_bitmap == NULL) {
		pcache_create(pcp, pollfdnum);
	}
	for (pfdp = pollfdp; pfdp < pollfdp + pollfdnum; pfdp++) {
		fd = pfdp->fd;
		if ((uint_t)fd >= P_FINFO(curproc)->fi_nfiles)
			continue;
		pdp = pcache_lookup_fd(pcp, fd);
		if (pfdp->events != POLLREMOVE) {
			if (pdp == NULL) {
				pdp = pcache_alloc_fd(0);
				pdp->pd_fd = fd;
				pdp->pd_pcache = pcp;
				pcache_insert_fd(pcp, pdp, pollfdnum);
			}
			ASSERT(pdp->pd_fd == fd);
			ASSERT(pdp->pd_pcache == pcp);
			if (fd >= pcp->pc_mapsize) {
				mutex_exit(&pcp->pc_lock);
				pcache_grow_map(pcp, fd);
				mutex_enter(&pcp->pc_lock);
			}
			if (fd > pcp->pc_mapend) {
				pcp->pc_mapend = fd;
			}
			if ((fp = getf(fd)) == NULL) {
				/*
				 * The fd is not valid. Since we can't pass
				 * this error back in the write() call, set
				 * the bit in bitmap to force DP_POLL ioctl
				 * to examine it.
				 */
				BT_SET(pcp->pc_bitmap, fd);
				pdp->pd_events |= pfdp->events;
				continue;
			}
			/*
			 * Don't do VOP_POLL for an already cached fd with
			 * same poll events.
			 */
			if ((pdp->pd_events == pfdp->events) &&
			    (pdp->pd_fp != NULL)) {
				/*
				 * the events are already cached
				 */
				releasef(fd);
				continue;
			}

			/*
			 * do VOP_POLL and cache this poll fd.
			 */
			/*
			 * XXX - pollrelock() logic needs to know which
			 * which pollcache lock to grab. It'd be a
			 * cleaner solution if we could pass pcp as
			 * an arguement in VOP_POLL interface instead
			 * of implicitly passing it using thread_t
			 * struct. On the other hand, changing VOP_POLL
			 * interface will require all driver/file system
			 * poll routine to change. May want to revisit
			 * the tradeoff later.
			 */
			curthread->t_pollcache = pcp;
			error = VOP_POLL(fp->f_vnode, pfdp->events, 0,
			    &pfdp->revents, &php);
			curthread->t_pollcache = NULL;
			/*
			 * We always set the bit when this fd is cached.
			 * So we don't have to worry about missing a
			 * pollwakeup between VOP_POLL and pollhead_insert.
			 * This forces the first DP_POLL to poll this fd.
			 * Real performance gain comes from subsequent
			 * DP_POLL.
			 */
			BT_SET(pcp->pc_bitmap, fd);
			if (error != 0) {
				releasef(fd);
				break;
			}
			pdp->pd_fp = fp;
			pdp->pd_events |= pfdp->events;
			if (php != NULL) {
				if (pdp->pd_php == NULL) {
					pollhead_insert(php, pdp);
					pdp->pd_php = php;
				} else {
					if (pdp->pd_php != php) {
						pollhead_delete(pdp->pd_php,
						    pdp);
						pollhead_insert(php, pdp);
						pdp->pd_php = php;
					}
				}

			}
			releasef(fd);
		} else {
			if (pdp == NULL) {
				continue;
			}
			ASSERT(pdp->pd_fd == fd);
			pdp->pd_fp = NULL;
			pdp->pd_events = 0;
			ASSERT(pdp->pd_thread == NULL);
			if (pdp->pd_php != NULL) {
				pollhead_delete(pdp->pd_php, pdp);
				pdp->pd_php = NULL;
			}
			BT_CLEAR(pcp->pc_bitmap, fd);
		}
	}
	mutex_exit(&pcp->pc_lock);
	mutex_enter(&dpep->dpe_lock);
	dpep->dpe_flag &= ~DP_WRITER_PRESENT;
	ASSERT(dpep->dpe_refcnt == 1);
	dpep->dpe_refcnt--;
	cv_broadcast(&dpep->dpe_cv);
	mutex_exit(&dpep->dpe_lock);
	kmem_free(pollfdp, uiosize);
	return (error);
}

/*ARGSUSED*/
static int
dpioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp, int *rvalp)
{
	time_t 		start_time;
	minor_t 	minor;
	dp_entry_t	*dpep;
	pollcache_t	*pcp;
	int 		error;
	timeout_id_t	id;
	STRUCT_DECL(dvpoll, dvpoll);

	start_time = lbolt;
	minor = getminor(dev);
	mutex_enter(&devpoll_lock);
	ASSERT(minor < dptblsize);
	dpep = devpolltbl[minor];
	mutex_exit(&devpoll_lock);
	ASSERT(dpep != NULL);
	pcp = dpep->dpe_pcache;
	if (curproc->p_pid != pcp->pc_pid) {
		return (EACCES);
	}

	mutex_enter(&dpep->dpe_lock);
	while ((dpep->dpe_flag & DP_WRITER_PRESENT) ||
	    (dpep->dpe_writerwait != 0)) {
		if (!cv_wait_sig_swap(&dpep->dpe_cv, &dpep->dpe_lock)) {
			mutex_exit(&dpep->dpe_lock);
			return (set_errno(EINTR));
		}
	}
	dpep->dpe_refcnt++;
	mutex_exit(&dpep->dpe_lock);

	switch (cmd) {
	case	DP_POLL:
	{
		nfds_t	size;

		STRUCT_INIT(dvpoll, mode);
		error = copyin((caddr_t)arg, STRUCT_BUF(dvpoll),
		    STRUCT_SIZE(dvpoll));
		if (error) {
			DP_REFRELE(dpep);
			return (set_errno(EFAULT));
		}
		if ((size = STRUCT_FGET(dvpoll, dp_nfds)) == 0) {
			/*
			 * user is using DP_POLL to sleep
			 */
			int done = 0;

			error = pollsleep(pcp, STRUCT_FGET(dvpoll, dp_timeout),
			    &done, start_time);
		} else {

			pollstate_t	*ps;
			int		fdcnt;

			/*
			 * XXX It'd be nice not to have to alloc each time.
			 * But it requires another per thread structure hook.
			 * Do it later if there is data suggest that.
			 */
			if ((ps = curthread->t_pollstate) == NULL) {
				curthread->t_pollstate = pollstate_create();
				ps = curthread->t_pollstate;
			}
			if (ps->ps_dpbufsize < size) {
				kmem_free(ps->ps_dpbuf, sizeof (pollfd_t) *
				    ps->ps_dpbufsize);
				ps->ps_dpbuf = kmem_zalloc(sizeof (pollfd_t) *
				    size, KM_SLEEP);
				ps->ps_dpbufsize = size;
			}
			mutex_enter(&pcp->pc_lock);
retry:
			pcp->pc_flag = 0;
			fdcnt = 0;
			error = dp_pcache_poll(ps->ps_dpbuf, pcp, size, &fdcnt);
			if ((fdcnt > 0) || (error != 0)) {
				mutex_exit(&pcp->pc_lock);
				goto dppollout;
			}

			/*
			 * A pollwake has happened since we polled cache.
			 */
			if (pcp->pc_flag & T_POLLWAKE) {
				goto retry;
			}
			/*
			 * check timeout value
			 */
			if (STRUCT_FGET(dvpoll, dp_timeout) >= 0) {
				clock_t hz_timo = MSEC_TO_TICK_ROUNDUP(
				    STRUCT_FGET(dvpoll, dp_timeout) -
				    (lbolt - start_time));

				if (hz_timo <= 0) {
					mutex_exit(&pcp->pc_lock);
					DP_REFRELE(dpep);
					return (0);
				}
				pcp->pc_flag |= T_POLLTIME;
				id = realtime_timeout(polltime, pcp, hz_timo);
			}

			if (!cv_wait_sig_swap(&pcp->pc_cv, &pcp->pc_lock)) {
				mutex_exit(&pcp->pc_lock);
				if (STRUCT_FGET(dvpoll, dp_timeout) >= 0) {
					(void) untimeout(id);
				}
				DP_REFRELE(dpep);
				return (set_errno(EINTR));
			}
			if (STRUCT_FGET(dvpoll, dp_timeout) < 0) {
				goto retry;
			}
			/*
			 * if T_POLLTIME is still set, we are waken up
			 * by a poll event notify. Go scan again.
			 */
			if (pcp->pc_flag & T_POLLTIME) {
				(void) untimeout(id);
				goto retry;
			}
			/*
			 * we are waken by timeout. Return normally.
			 */
			mutex_exit(&pcp->pc_lock);
dppollout:
			if ((error == 0) && (fdcnt > 0)) {
				if (copyout(ps->ps_dpbuf, STRUCT_FGETP(dvpoll,
				    dp_fds), sizeof (pollfd_t) * fdcnt)) {
					DP_REFRELE(dpep);
					return (set_errno(EFAULT));
				}
				*rvalp = fdcnt;
			}
		}
		break;
	}

	case	DP_ISPOLLED:
	{
		pollfd_t	pollfd;
		polldat_t	*pdp;

		STRUCT_INIT(dvpoll, mode);
		error = copyin((caddr_t)arg, &pollfd, sizeof (pollfd_t));
		if (error) {
			DP_REFRELE(dpep);
			return (set_errno(EFAULT));
		}
		mutex_enter(&pcp->pc_lock);
		if (pcp->pc_hash == NULL) {
			/*
			 * No Need to search because no poll fd
			 * has been cached.
			 */
			mutex_exit(&pcp->pc_lock);
			DP_REFRELE(dpep);
			return (error);
		}
		if (pollfd.fd < 0) {
			mutex_exit(&pcp->pc_lock);
			break;
		}
		pdp = pcache_lookup_fd(pcp, pollfd.fd);
		if ((pdp != NULL) && (pdp->pd_fd == pollfd.fd) &&
		    (pdp->pd_fp != NULL)) {
			pollfd.revents = pdp->pd_events;
			if (copyout(&pollfd, (caddr_t)arg, sizeof (pollfd_t))) {
				mutex_exit(&pcp->pc_lock);
				DP_REFRELE(dpep);
				return (set_errno(EFAULT));
			}
			*rvalp = 1;
		}
		mutex_exit(&pcp->pc_lock);
		break;
	}

	default:
		DP_REFRELE(dpep);
		return (set_errno(EINVAL));
	}
	DP_REFRELE(dpep);
	return (error);
}

/*ARGSUSED*/
static int
dppoll(dev_t dev, short events, int anyyet, short *reventsp,
    struct pollhead **phpp)
{
	/*
	 * Polling on a /dev/poll fd is not fully supported yet.
	 */
	*reventsp = POLLERR;
	return (0);
}

/*
 * devpoll close should do enough clean up before the pollcache is deleted,
 * i.e., it should ensure no one still references the pollcache later.
 * There is no "permission" check in here. Any process having the last
 * reference of this /dev/poll fd can close.
 */
/*ARGSUSED*/
static int
dpclose(dev_t dev, int flag, int otyp, cred_t *credp)
{
	minor_t 	minor;
	dp_entry_t	*dpep;
	pollcache_t	*pcp;
	int		i;
	polldat_t	**hashtbl;
	polldat_t	*pdp;

	minor = getminor(dev);

	mutex_enter(&devpoll_lock);
	dpep = devpolltbl[minor];
	ASSERT(dpep != NULL);
	devpolltbl[minor] = NULL;
	mutex_exit(&devpoll_lock);
	pcp = dpep->dpe_pcache;
	ASSERT(pcp != NULL);
	/*
	 * At this point, no other lwp can access this pollcache via the
	 * /dev/poll fd. This pollcache is going away, so do the clean
	 * up without the pc_lock.
	 */
	hashtbl = pcp->pc_hash;
	for (i = 0; i < pcp->pc_hashsize; i++) {
		for (pdp = hashtbl[i]; pdp; pdp = pdp->pd_hashnext) {
			if (pdp->pd_php != NULL) {
				pollhead_delete(pdp->pd_php, pdp);
				pdp->pd_php = NULL;
				pdp->pd_fp = NULL;
			}
		}
	}
	/*
	 * pollwakeup() may still interact with this pollcache. Wait until
	 * it is done.
	 */
	mutex_enter(&pcp->pc_no_exit);
	ASSERT(pcp->pc_busy >= 0);
	while (pcp->pc_busy > 0)
		cv_wait(&pcp->pc_busy_cv, &pcp->pc_no_exit);
	mutex_exit(&pcp->pc_no_exit);
	pcache_destroy(pcp);
	ASSERT(dpep->dpe_refcnt == 0);
	kmem_free(dpep, sizeof (dp_entry_t));
	return (0);
}
