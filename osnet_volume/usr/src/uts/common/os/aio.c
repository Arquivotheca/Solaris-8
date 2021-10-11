/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)aio.c	1.71	99/12/06 SMI"

/*
 * Kernel asynchronous I/O.
 * This is only for raw devices now (as of Nov. 1993).
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/fs/snode.h>
#include <sys/unistd.h>
#include <sys/cmn_err.h>
#include <vm/as.h>
#include <vm/faultcode.h>
#include <sys/sysmacros.h>
#include <sys/procfs.h>
#include <sys/kmem.h>
#include <sys/autoconf.h>
#include <sys/ddi_impldefs.h>
#include <sys/sunddi.h>
#include <sys/aio_impl.h>
#include <sys/debug.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vmsystm.h>
#include <sys/fs/pxfs_ki.h>

/*
 * external entry point.
 */
#ifdef _LP64
static int64_t kaioc(long, long, long, long, long, long);
#endif
static int kaio(ulong_t *, rval_t *);


#define	AIO_64	0
#define	AIO_32	1
#define	AIO_LARGEFILE	2

/*
 * implementation specific functions (private)
 */
#ifdef _LP64
static int alio(int, int, aiocb_t **, int, struct sigevent *);
#endif
static int aionotify(void);
static int aioinit(void);
static int aiostart(void);
static void alio_cleanup(aio_t *, aiocb_t **, int, int);
static int (*check_vp(struct vnode *, int))(vnode_t *, struct aio_req *,
    cred_t *);
static void lio_set_error(aio_req_t *);
static aio_t *aio_aiop_alloc();
static int aio_req_alloc(aio_req_t **, aio_result_t *);
static int aio_lio_alloc(aio_lio_t **);
static aio_req_t *aio_req_done(void *);
static aio_req_t *aio_req_remove(aio_req_t *);
static int aio_req_find(aio_result_t *, aio_req_t **);
static int aio_hash_insert(struct aio_req_t *, aio_t *);
static int aio_req_setup(aio_req_t **, aio_t *, aiocb_t *, aio_result_t *);
static int aio_cleanup_thread(aio_t *);
static aio_lio_t *aio_list_get(aio_result_t *);
static void lio_set_uerror(void *, int);
extern void aio_zerolen(aio_req_t *);
static int aiowait(struct timeval *, int, long	*);
static int aiosuspend(void *, int, struct  timespec *, int,
    long	*, int);
static int aliowait(int, void *, int, void *, int);
static int aioerror(void *, int);
static int aio_cancel(int, void *, long	*, int);
static int arw(int, int, char *, int, offset_t, aio_result_t *, int);
static int aiorw(int, void *, int, int);

static int alioLF(int, void *, int, void *);
static int aio_req_setupLF(aio_req_t **, aio_t *, aiocb64_32_t *,
    aio_result_t *);
static int alio32(int, void *, int, void *);
extern	int sulword(void *, ulong_t);
static int driver_aio_write(vnode_t *vp, struct aio_req *aio, cred_t *cred_p);
static int driver_aio_read(vnode_t *vp, struct aio_req *aio, cred_t *cred_p);

#ifdef  _SYSCALL32_IMPL
static void aiocb_LFton(aiocb64_32_t *, aiocb_t *);
void	aiocb_32ton(aiocb32_t *, aiocb_t *);
#endif /* _SYSCALL32_IMPL */

/*
 * implementation specific functions (external)
 */
void aio_req_free(aio_t *, aio_req_t *);

/*
 * timer conversion routines
 */
static int timeval2tick(struct timeval *, clock_t *, int *);
static int timespec2tick(struct timespec *, clock_t *, int *);


/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>
#include <sys/syscall.h>

#ifdef _LP64

static struct sysent kaio_sysent = {
	6,
	SE_NOUNLOAD | SE_64RVAL | SE_ARGC,
	(int (*)())kaioc
};

#ifdef _SYSCALL32_IMPL
static struct sysent kaio_sysent32 = {
	7,
	SE_NOUNLOAD | SE_64RVAL,
	kaio
};
#endif  /* _SYSCALL32_IMPL */

#else   /* _LP64 */

static struct sysent kaio_sysent = {
	7,
	SE_NOUNLOAD | SE_32RVAL1,
	kaio
};

#endif  /* _LP64 */

/*
 * Module linkage information for the kernel.
 */

static struct modlsys modlsys = {
	&mod_syscallops,
	"kernel Async I/O",
	&kaio_sysent
};

#ifdef  _SYSCALL32_IMPL
static struct modlsys modlsys32 = {
	&mod_syscallops32,
	"kernel Async I/O for 32 bit compatibility",
	&kaio_sysent32
};
#endif  /* _SYSCALL32_IMPL */


static struct modlinkage modlinkage = {
	MODREV_1,
	&modlsys,
#ifdef  _SYSCALL32_IMPL
	&modlsys32,
#endif
	NULL
};

int
_init(void)
{
	int retval;

	if ((retval = mod_install(&modlinkage)) != 0)
		return (retval);

	return (0);
}

int
_fini(void)
{
	int retval;

	retval = mod_remove(&modlinkage);

	return (retval);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

#ifdef	_LP64
static int64_t
kaioc(
	long	a0,
	long	a1,
	long	a2,
	long	a3,
	long	a4,
	long	a5)
{
	int	error;
	long	rval = 0;

	switch ((int)a0 & ~AIO_POLL_BIT) {
	case AIOREAD:
		error = arw((int)a0, (int)a1, (char *)a2, (int)a3,
		    (offset_t)a4, (aio_result_t *)a5, FREAD);
		break;
	case AIOWRITE:
		error = arw((int)a0, (int)a1, (char *)a2, (int)a3,
		    (offset_t)a4, (aio_result_t *)a5, FWRITE);
		break;
	case AIOWAIT:
		error = aiowait((struct timeval *)a1, (int)a2, &rval);
		break;
	case AIONOTIFY:
		error = aionotify();
		break;
	case AIOINIT:
		error = aioinit();
		break;
	case AIOSTART:
		error = aiostart();
		break;
	case AIOLIO:
		error = alio((int)a0, (int)a1, (aiocb_t **)a2, (int)a3,
		    (struct sigevent *)a4);
		break;
	case AIOLIOWAIT:
		error = aliowait((int)a1, (void *)a2, (int)a3,
		    (struct sigevent *)a4, AIO_64);
		break;
	case AIOSUSPEND:
		error = aiosuspend((void *)a1, (int)a2, (struct timespec *)a3,
		    (int)a4, &rval, AIO_64);
		break;
	case AIOERROR:
		error = aioerror((void *)a1, AIO_64);
		break;
	case AIOAREAD:
		error = aiorw((int)a0, (void *)a1, FREAD, AIO_64);
		break;
	case AIOAWRITE:
		error = aiorw((int)a0, (void *)a1, FWRITE, AIO_64);
		break;
	case AIOCANCEL:
		error = aio_cancel((int)a1, (void *)a2, &rval, AIO_64);
		break;

	/*
	 * The large file related stuff is valid only for
	 * 32 bit kernel and not for 64 bit kernel
	 * On 64 bit kernel we convert large file calls
	 * to regular 64bit calls.
	 */

	default:
		error = EINVAL;
	}
	if (error)
		return ((int64_t)set_errno(error));
	return (rval);
}
#endif

static int
kaio(
	ulong_t *uap,
	rval_t *rvp)
{
	long rval = 0;
	int	error = 0;
	offset_t	off;


		rvp->r_vals = 0;
#if defined(_LITTLE_ENDIAN)
	off = ((u_offset_t)uap[5] << 32) | (u_offset_t)uap[4];
#else
	off = ((u_offset_t)uap[4] << 32) | (u_offset_t)uap[5];
#endif

	switch (uap[0] & ~AIO_POLL_BIT) {
	/*
	 * It must be the 32 bit system call on 64 bit kernel
	 */
	case AIOREAD:
		return (arw((int)uap[0], (int)uap[1], (char *)uap[2],
		    (int)uap[3], off, (aio_result_t *)uap[6], FREAD));
	case AIOWRITE:
		return (arw((int)uap[0], (int)uap[1], (char *)uap[2],
		    (int)uap[3], off, (aio_result_t *)uap[6], FWRITE));
	case AIOWAIT:
		error = aiowait((struct	timeval *)uap[1], (int)uap[2],
		    &rval);
		break;
	case AIONOTIFY:
		return (aionotify());
	case AIOINIT:
		return (aioinit());
	case AIOSTART:
		return (aiostart());
	case AIOLIO:
		return (alio32((int)uap[1], (void *)uap[2], (int)uap[3],
		    (void *)uap[4]));
	case AIOLIOWAIT:
		return (aliowait((int)uap[1], (void *)uap[2],
		    (int)uap[3], (struct sigevent *)uap[4], AIO_32));
	case AIOSUSPEND:
		error = aiosuspend((void *)uap[1], (int)uap[2],
		    (struct timespec *)uap[3], (int)uap[4],
		    &rval, AIO_32);
		break;
	case AIOERROR:
		return (aioerror((void *)uap[1], AIO_32));
	case AIOAREAD:
		return (aiorw((int)uap[0], (void *)uap[1],
		    FREAD, AIO_32));
	case AIOAWRITE:
		return (aiorw((int)uap[0], (void *)uap[1],
		    FWRITE, AIO_32));
	case AIOCANCEL:
		error = (aio_cancel((int)uap[1], (void *)uap[2], &rval,
		    AIO_32));
		break;
	case AIOLIO64:
		return (alioLF((int)uap[1], (void *)uap[2],
		    (int)uap[3], (void *)uap[4]));
	case AIOLIOWAIT64:
		return (aliowait(uap[1], (void *)uap[2],
		    (int)uap[3], (void *)uap[4], AIO_LARGEFILE));
	case AIOSUSPEND64:
		error = aiosuspend((void *)uap[1], (int)uap[2],
		    (struct timespec *)uap[3], (int)uap[4], &rval,
		    AIO_LARGEFILE);
		break;
	case AIOERROR64:
		return (aioerror((void *)uap[1], AIO_LARGEFILE));
	case AIOAREAD64:
		return (aiorw((int)uap[0], (void *)uap[1], FREAD,
		    AIO_LARGEFILE));
	case AIOAWRITE64:
		return (aiorw((int)uap[0], (void *)uap[1], FWRITE,
		    AIO_LARGEFILE));
	case AIOCANCEL64:
		error = (aio_cancel((int)uap[1], (void *)uap[2],
		    &rval, AIO_LARGEFILE));
		break;
	default:
		return (EINVAL);
	}

	rvp->r_val1 = rval;
	return (error);
}

/*
 * wake up LWPs in this process that are sleeping in
 * aiowait().
 */
static int
aionotify(void)
{
	aio_t	*aiop;

	aiop = curproc->p_aio;
	if (aiop == NULL)
		return (0);

	mutex_enter(&aiop->aio_mutex);
	aiop->aio_notifycnt++;
	cv_broadcast(&aiop->aio_waitcv);
	mutex_exit(&aiop->aio_mutex);

	return (0);
}

static int
timeval2tick(struct timeval *timout, clock_t *ticks, int *block)
{
#ifdef	_SYSCALL32_IMPL
	struct timeval32 wait_time_32;
#endif
	int error;
	struct timeval wait_time;
	model_t	model = get_udatamodel();

	/*
	 * don't block if caller wants to poll, blocking should
	 * be set to zero.
	 */
	if (timout == NULL) {
		/*
		 * If timeval NULL, wait indefinitely
		 */
		*block = 1;
	} else if ((int)timout == -1) {
		*block = 0;
	} else {
		if (model == DATAMODEL_NATIVE) {
			if (copyin(timout, &wait_time, sizeof (wait_time)))
				return (EFAULT);
		}
#ifdef	_SYSCALL32_IMPL
		else {
			if (copyin(timout, &wait_time_32,
			    sizeof (wait_time_32)))
				return (EFAULT);
			TIMEVAL32_TO_TIMEVAL(&wait_time, &wait_time_32);
		}
#endif  /* _SYSCALL32_IMPL */

		if (wait_time.tv_sec > 0 || wait_time.tv_usec > 0) {

			/*
			 * If timeval > 0, wait for specified time
			 */
			if (error = itimerfix(&wait_time)) {
				/*
				 * This is not good. itimerfix() will
				 * return EINVAL if wait_time is
				 * incorrect, but we do not specify
				 * EINVAL as a valid return value.
				 */
				return (error);
			}
			*block = 1;
			*ticks = lbolt + TIMEVAL_TO_TICK(&wait_time);
		} else {
			/*
			 * timeval == 0, so poll
			 */
			*block = 0;
		}
	}
	return (0);
}

static int
timespec2tick(struct timespec *timout, clock_t *ticks, int *block)
{
#ifdef	_SYSCALL32_IMPL
	struct timespec32 wait_time_32;
#endif
	int error;
	struct timespec wait_time;
	model_t	model = get_udatamodel();

	/*
	 * first copy the timeval struct into the kernel.
	 * if the caller is polling, the caller will not
	 * block and "blocking" should be zero.
	 */
	if (timout) {
		if (model == DATAMODEL_NATIVE) {
			if (copyin(timout, &wait_time, sizeof (wait_time))) {
				return (EFAULT);
			}
		}
#ifdef	_SYSCALL32_IMPL
		else {
			if (copyin(timout, &wait_time_32,
			    sizeof (wait_time_32))) {
				return (EFAULT);
			}
			TIMESPEC32_TO_TIMESPEC(&wait_time, &wait_time_32);
		}
#endif  /* _SYSCALL32_IMPL */

		if (wait_time.tv_sec > 0 || wait_time.tv_nsec > 0) {
			if (error = itimerspecfix(&wait_time)) {
				/*
				 * This is not good. This will return
				 * EINVAL, which is not documented.
				 */
				return (error);
			}
			*block = 1;
			*ticks = lbolt + TIMESTRUC_TO_TICK(&wait_time);
		} else
			*block = 0;
	} else
		*block = 1;

	return (0);
}

/*ARGSUSED*/
static int
aiowait(
	struct timeval	*timout,
	int	dontblockflg,
	long	*rval)
{
	int 		error = 0;
	aio_t		*aiop;
	aio_req_t	*reqp;
	clock_t		ticks = 0;
	clock_t		status;
	int		blocking;
	int		timechk = 0;

	aiop = curproc->p_aio;
	if (aiop == NULL)
		return (EINVAL);

	mutex_enter(&aiop->aio_mutex);
	for (;;) {
		/* process requests on poll queue */
		if (aiop->aio_pollq) {
			mutex_exit(&aiop->aio_mutex);
			aio_cleanup(0);
			mutex_enter(&aiop->aio_mutex);
		}
		if (reqp = aio_req_remove(NULL)) {
			*rval = (long)reqp->aio_req_resultp;
			break;
		}
		/* user-level done queue might not be empty */
		if (aiop->aio_notifycnt > 0) {
			aiop->aio_notifycnt--;
			*rval = 1;
			error = 0;
			break;
		}
		/* don't block if no outstanding aio */
		if (aiop->aio_outstanding == 0 && dontblockflg) {
			error = EINVAL;
			break;
		}
		if (!timechk) {
			error = timeval2tick(timout, &ticks, &blocking);
			if (error != 0)
				break;
			timechk++;
		}
		if (blocking) {
			if (ticks)
				status = cv_timedwait_sig(&aiop->aio_waitcv,
				    &aiop->aio_mutex, ticks);
			else
				status = cv_wait_sig(&aiop->aio_waitcv,
				    &aiop->aio_mutex);

			if (status > 0)
				/* check done queue again */
				continue;
			else if (status == 0) {
				/* interrupted by a signal */
				error = EINTR;
				*rval = -1;
			} else if (status == -1)
				/* timer expired */
				error = ETIME;
		}
		break;
	}
	mutex_exit(&aiop->aio_mutex);
	if (reqp) {
		aphysio_unlock(reqp);
		aio_copyout_result(reqp);
		mutex_enter(&aiop->aio_mutex);
		aio_req_free(aiop, reqp);
		mutex_exit(&aiop->aio_mutex);
	}
	return (error);
}

/*ARGSUSED*/
static int
aiosuspend(
	void	*aiocb,
	int	nent,
	struct	timespec	*timout,
	int	flag,
	long	*rval,
	int	run_mode)
{
	int 		error = 0;
	aio_t		*aiop;
	aio_req_t	*reqp, *found, *next;
	caddr_t		cbplist = NULL;
	aiocb_t		*cbp, **ucbp;
#ifdef	_SYSCALL32_IMPL
	aiocb32_t	*cbp32;
	caddr32_t	*ucbp32;
#endif  /* _SYSCALL32_IMPL */
	aiocb64_32_t	*cbp64;
	clock_t		ticks = 0;
	clock_t		rv;
	int		blocking;
	int		i;
	size_t		ssize;
	model_t		model = get_udatamodel();

	aiop = curproc->p_aio;
	if (aiop == NULL || nent <= 0)
		return (EINVAL);

	error = timespec2tick(timout, &ticks, &blocking);
	if (error)
		return (error);

	/*
	 * If we are not blocking and there's no IO complete
	 * skip aiocb copyin.
	 */
	if (!blocking && (aiop->aio_pollq == NULL) &&
	    (aiop->aio_doneq == NULL)) {
		return (EAGAIN);
	}

	if (model == DATAMODEL_NATIVE)
		ssize = (sizeof (aiocb_t *) * nent);
#ifdef	_SYSCALL32_IMPL
	else
		ssize = (sizeof (caddr32_t) * nent);
#endif  /* _SYSCALL32_IMPL */

	cbplist = kmem_alloc(ssize, KM_NOSLEEP);
	if (cbplist == NULL)
		return (ENOMEM);

	if (copyin(aiocb, cbplist, ssize)) {
		error = EFAULT;
		goto done;
	}

	found = NULL;
	mutex_enter(&aiop->aio_mutex);
	for (;;) {
		/* push requests on poll queue to done queue */
		if (aiop->aio_pollq) {
			mutex_exit(&aiop->aio_mutex);
			aio_cleanup(0);
			mutex_enter(&aiop->aio_mutex);
		}
		/* check for requests on done queue */
		if (aiop->aio_doneq) {
			if (model == DATAMODEL_NATIVE)
				ucbp = (aiocb_t **)cbplist;
#ifdef	_SYSCALL32_IMPL
			else
				ucbp32 = (caddr32_t *)cbplist;
#endif  /* _SYSCALL32_IMPL */
			for (i = 0; i < nent; i++) {
				if (model == DATAMODEL_NATIVE) {
					if ((cbp = *ucbp++) == NULL)
						continue;
					if (run_mode != AIO_LARGEFILE)
						reqp = aio_req_done(
						    &cbp->aio_resultp);
					else {
						cbp64 = (aiocb64_32_t *)cbp;
						reqp = aio_req_done(
						&cbp64->aio_resultp);
					}
				}
#ifdef	_SYSCALL32_IMPL
				else {
					if (run_mode == AIO_32) {
						if ((cbp32 = (aiocb32_t *)
						    *ucbp32++) == NULL)
							continue;
						reqp = aio_req_done(
						    &cbp32->aio_resultp);
					} else if (run_mode == AIO_LARGEFILE) {
						if ((cbp64 =
						    (aiocb64_32_t *)
						    *ucbp32++) == NULL)
							continue;
						    reqp = aio_req_done(
						    &cbp64->aio_resultp);
					}

				}
#endif  /* _SYSCALL32_IMPL */
				if (reqp) {
					reqp->aio_req_next = found;
					found = reqp;
				}
				if (aiop->aio_doneq == NULL)
					break;
			}
			if (found)
				break;
		}
		if (aiop->aio_notifycnt > 0) {
			/*
			 * nothing on the kernel's queue. the user
			 * has notified the kernel that it has items
			 * on a user-level queue.
			 */
			aiop->aio_notifycnt--;
			*rval = 1;
			error = 0;
			break;
		}
		/* don't block if nothing is outstanding */
		if (aiop->aio_outstanding == 0) {
			error = EAGAIN;
			break;
		}
		if (blocking) {
			if (ticks)
				rv = cv_timedwait_sig(&aiop->aio_waitcv,
				    &aiop->aio_mutex, ticks);
			else
				rv = cv_wait_sig(&aiop->aio_waitcv,
				    &aiop->aio_mutex);
			if (rv > 0)
				/* check done queue again */
				continue;
			else if (rv == 0)
				/* interrupted by a signal */
				error = EINTR;
			else if (rv == -1)
				/* timer expired */
				error = ETIME;
		} else
			error = EAGAIN;
		break;
	}
	mutex_exit(&aiop->aio_mutex);
	for (reqp = found; reqp != NULL; reqp = next) {
		next = reqp->aio_req_next;
		aphysio_unlock(reqp);
		aio_copyout_result(reqp);
		mutex_enter(&aiop->aio_mutex);
		aio_req_free(aiop, reqp);
		mutex_exit(&aiop->aio_mutex);
	}
done:
	kmem_free(cbplist, ssize);
	return (error);
}

/*
 * initialize aio by allocating an aio_t struct for this
 * process.
 */
static int
aioinit(void)
{
	proc_t *p = curproc;
	aio_t *aiop;
	mutex_enter(&p->p_lock);
	if ((aiop = p->p_aio) == NULL) {
		aiop = aio_aiop_alloc();
		p->p_aio = aiop;
	}
	mutex_exit(&p->p_lock);
	if (aiop == NULL)
		return (ENOMEM);
	return (0);
}

/*
 * start a special thread that will cleanup after aio requests
 * that are preventing a segment from being unmapped. as_unmap()
 * blocks until all phsyio to this segment is completed. this
 * doesn't happen until all the pages in this segment are not
 * SOFTLOCKed. Some pages will be SOFTLOCKed when there are aio
 * requests still outstanding. this special thread will make sure
 * that these SOFTLOCKed pages will eventually be SOFTUNLOCKed.
 *
 * this function will return an error if the process has only
 * one LWP. the assumption is that the caller is a separate LWP
 * that remains blocked in the kernel for the life of this process.
 */
static int
aiostart(void)
{
	proc_t *p = curproc;
	aio_t *aiop;
	int first, error = 0;

	if (p->p_lwpcnt == 1)
		return (EDEADLK);
	mutex_enter(&p->p_lock);
	if ((aiop = p->p_aio) == NULL)
		error = EINVAL;
	else {
		first = aiop->aio_ok;
		if (aiop->aio_ok == 0)
			aiop->aio_ok = 1;
	}
	mutex_exit(&p->p_lock);
	if (error == 0 && first == 0) {
		return (aio_cleanup_thread(aiop));
		/* should never return */
	}
	return (error);
}

#ifdef _LP64

/*
 * Asynchronous list IO. A chain of aiocb's are copied in
 * one at a time. If the aiocb is invalid, it is skipped.
 * For each aiocb, the appropriate driver entry point is
 * called. Optimize for the common case where the list
 * of requests is to the same file descriptor.
 *
 * One possible optimization is to define a new driver entry
 * point that supports a list of IO requests. Whether this
 * improves performance depends somewhat on the driver's
 * locking strategy. Processing a list could adversely impact
 * the driver's interrupt latency.
 */
/*ARGSUSED*/
static int
alio(
	int	opcode,
	int	mode_arg,
	aiocb_t	**aiocb_arg,
	int	nent,
	struct	sigevent *sigev)

{
	file_t		*fp;
	file_t		*prev_fp = NULL;
	int		prev_mode = -1;
	struct vnode	*vp;
	aio_lio_t	*head;
	aio_req_t	*reqp;
	aio_t		*aiop;
	caddr_t		cbplist;
	aiocb_t		*cbp, **ucbp;
	aiocb_t		cb;
	aiocb_t		*aiocb = &cb;
	struct sigevent sigevk;
	sigqueue_t	*sqp;
	int		(*aio_func)();
	int		mode;
	int		error = 0, aio_errors = 0;
	int		i;
	size_t		ssize;
	int		deadhead = 0;
	int		aio_notsupported = 0;

	aiop = curproc->p_aio;
	if (aiop == NULL || nent <= 0 || nent > _AIO_LISTIO_MAX)
		return (EINVAL);

	ssize = (sizeof (aiocb_t *) * nent);
	cbplist = kmem_alloc(ssize, KM_SLEEP);
	ucbp = (aiocb_t **)cbplist;

	if (copyin(aiocb_arg, cbplist, sizeof (aiocb_t *) * nent)) {
		error = EFAULT;
		goto done;
	}

	if (sigev) {
		if (copyin(sigev, &sigevk, sizeof (struct sigevent))) {
			error = EFAULT;
			goto done;
		}
	}

	/*
	 * a list head should be allocated if notification is
	 * enabled for this list.
	 */
	head = NULL;
	if ((mode_arg == LIO_WAIT) || sigev) {
		mutex_enter(&aiop->aio_mutex);
		error = aio_lio_alloc(&head);
		mutex_exit(&aiop->aio_mutex);
		if (error) {
			goto done;
		}
		deadhead = 1;
		head->lio_nent = nent;
		head->lio_refcnt = nent;
		if (sigev && (sigevk.sigev_notify == SIGEV_SIGNAL) &&
		    (sigevk.sigev_signo > 0 && sigevk.sigev_signo < NSIG)) {
			sqp = kmem_zalloc(sizeof (sigqueue_t),
			    KM_NOSLEEP);
			if (sqp == NULL) {
				error = EAGAIN;
				goto done;
			}
			sqp->sq_func = NULL;
			sqp->sq_next = NULL;
			sqp->sq_info.si_code = SI_ASYNCIO;
			sqp->sq_info.si_pid = curproc->p_pid;
			sqp->sq_info.si_uid = curproc->p_cred->cr_uid;
			sqp->sq_info.si_signo = sigevk.sigev_signo;
			sqp->sq_info.si_value = sigevk.sigev_value;
			head->lio_sigqp = sqp;
		} else
			head->lio_sigqp = NULL;
	}

	for (i = 0; i < nent; i++, ucbp++) {

		cbp = *ucbp;
		/* skip entry if it can't be copied. */
		if (cbp == NULL || copyin(cbp, aiocb, sizeof (aiocb_t))) {
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}

		/* skip if opcode for aiocb is LIO_NOP */

		mode = aiocb->aio_lio_opcode;
		if (mode == LIO_NOP) {
			cbp = NULL;
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}

		/* increment file descriptor's ref count. */
		if ((fp = getf(aiocb->aio_fildes)) == NULL) {
			lio_set_uerror(&cbp->aio_resultp, EBADF);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			aio_errors++;
			continue;
		}

		/*
		 * check the permission of the partition
		 */
		mode = aiocb->aio_lio_opcode;
		if ((fp->f_flag & mode) == 0) {
			releasef(aiocb->aio_fildes);
			lio_set_uerror(&cbp->aio_resultp, EBADF);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			aio_errors++;
			continue;
		}

		/*
		 * common case where requests are to the same fd for the
		 * same r/w operation.
		 * for UFS, need to set ENOTSUP
		 */
		if ((fp != prev_fp) || (mode != prev_mode)) {
			vp = fp->f_vnode;
			aio_func = check_vp(vp, mode);
			if (aio_func == NULL) {
				prev_fp = NULL;
				releasef(aiocb->aio_fildes);
				lio_set_uerror(&cbp->aio_resultp, ENOTSUP);
				aio_notsupported++;
				if (head) {
					mutex_enter(&aiop->aio_mutex);
					head->lio_nent--;
					head->lio_refcnt--;
					mutex_exit(&aiop->aio_mutex);
				}
				continue;
			} else {
				prev_fp = fp;
				prev_mode = mode;
			}
		}
		if (error = aio_req_setup(&reqp, aiop, aiocb,
		    &cbp->aio_resultp)) {
			releasef(aiocb->aio_fildes);
			lio_set_uerror(&cbp->aio_resultp, error);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			aio_errors++;
			continue;
		}

		reqp->aio_req_lio = head;
		deadhead = 0;

		/*
		 * Set the errno field now before sending the request to
		 * the driver to avoid a race condition
		 */
		(void) suword32(&cbp->aio_resultp.aio_errno,
			    EINPROGRESS);

		if (aiocb->aio_nbytes == 0) {
			clear_active_fd(aiocb->aio_fildes);
			aio_zerolen(reqp);
			continue;
		}

		/*
		 * send the request to driver.
		 * Clustering: If PXFS vnode, call PXFS function.
		 */
		error = (*aio_func)(vp, (aio_req_t *)&reqp->aio_req,
		    CRED());
		/*
		 * the fd's ref count is not decremented until the IO has
		 * completed unless there was an error.
		 */
		if (error) {
			releasef(aiocb->aio_fildes);
			lio_set_uerror(&cbp->aio_resultp, error);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			if (error == ENOTSUP)
				aio_notsupported++;
			else
				aio_errors++;
			lio_set_error(reqp);
		} else {
			clear_active_fd(aiocb->aio_fildes);
		}
	}

	if (aio_notsupported) {
		error = ENOTSUP;
	} else if (aio_errors) {
		/*
		 * return EIO if any request failed
		 */
		error = EIO;
	}

	if (mode_arg == LIO_WAIT) {
		mutex_enter(&aiop->aio_mutex);
		while (head->lio_refcnt > 0) {
			if (!cv_wait_sig(&head->lio_notify, &aiop->aio_mutex)) {
				mutex_exit(&aiop->aio_mutex);
				error = EINTR;
				goto done;
			}
		}
		mutex_exit(&aiop->aio_mutex);
		alio_cleanup(aiop, (aiocb_t **)cbplist, nent, AIO_64);
	}

done:
	kmem_free(cbplist, ssize);
	if (deadhead) {
		if (head->lio_sigqp)
			kmem_free(head->lio_sigqp, sizeof (sigqueue_t));
		kmem_free(head, sizeof (aio_lio_t));
	}
	return (error);
}

#endif /* _LP64 */

/*
 * Asynchronous list IO.
 * If list I/O is called with LIO_WAIT it can still return
 * before all the I/O's are completed if a signal is caught
 * or if the list include UFS I/O requests. If this happens,
 * libaio will call aliowait() to wait for the I/O's to
 * complete
 */
/*ARGSUSED*/
static int
aliowait(
	int	mode,
	void	*aiocb,
	int	nent,
	void	*sigev,
	int	run_mode)
{
	aio_lio_t	*head;
	aio_t		*aiop;
	caddr_t		cbplist;
	aiocb_t		*cbp, **ucbp;
#ifdef	_SYSCALL32_IMPL
	aiocb32_t	*cbp32;
	caddr32_t	*ucbp32;
	aiocb64_32_t	*cbp64;
#endif
	int		error = 0;
	int		i;
	size_t		ssize = 0;
	model_t		model = get_udatamodel();

	aiop = curproc->p_aio;
	if (aiop == NULL || nent <= 0 || nent > _AIO_LISTIO_MAX)
		return (EINVAL);

	if (model == DATAMODEL_NATIVE)
		ssize = (sizeof (aiocb_t *) * nent);
#ifdef	_SYSCALL32_IMPL
	else
		ssize = (sizeof (caddr32_t) * nent);
#endif  /* _SYSCALL32_IMPL */

	if (ssize == 0)
		return (EINVAL);

	cbplist = kmem_alloc(ssize, KM_SLEEP);

	if (model == DATAMODEL_NATIVE)
		ucbp = (aiocb_t **)cbplist;
#ifdef	_SYSCALL32_IMPL
	else
		ucbp32 = (caddr32_t *)cbplist;
#endif  /* _SYSCALL32_IMPL */

	if (copyin(aiocb, cbplist, ssize)) {
		error = EFAULT;
		goto done;
	}

	/*
	 * To find the list head, we go through the
	 * list of aiocb structs, find the request
	 * its for, then get the list head that reqp
	 * points to
	 */
	head = NULL;

	for (i = 0; i < nent; i++) {
		if (model == DATAMODEL_NATIVE) {
			/*
			 * Since we are only checking for a NULL pointer
			 * Following should work on both native data sizes
			 * as well as for largefile aiocb.
			 */
			if ((cbp = *ucbp++) == NULL)
				continue;
			if (run_mode != AIO_LARGEFILE)
				if (head = aio_list_get(&cbp->aio_resultp))
					break;
			else {
				/*
				 * This is a case when largefile call is
				 * made on 32 bit kernel.
				 * Treat each pointer as pointer to
				 * aiocb64_32
				 */
				if (head = aio_list_get((aio_result_t *)
				    &(((aiocb64_32_t *)cbp)->aio_resultp)))
					break;
			}
		}
#ifdef	_SYSCALL32_IMPL
		else {
			if (run_mode == AIO_LARGEFILE) {
				if ((cbp64 = (aiocb64_32_t *)*ucbp32++) == NULL)
					continue;
				if (head = aio_list_get((aio_result_t *)
					&cbp64->aio_resultp))
					break;
			} else if (run_mode == AIO_32) {
				if ((cbp32 = (aiocb32_t *)*ucbp32++) == NULL)
					continue;
				if (head = aio_list_get((aio_result_t *)
					&cbp32->aio_resultp))
					break;
			}
		}
#endif	/* _SYSCALL32_IMPL */
	}

	if (head == NULL) {
		error = EINVAL;
		goto done;
	}

	mutex_enter(&aiop->aio_mutex);
	while (head->lio_refcnt > 0) {
		if (!cv_wait_sig(&head->lio_notify, &aiop->aio_mutex)) {
			mutex_exit(&aiop->aio_mutex);
			error = EINTR;
			goto done;
		}
	}
	mutex_exit(&aiop->aio_mutex);
	alio_cleanup(aiop, (aiocb_t **)cbplist, nent, run_mode);
done:
	kmem_free(cbplist, ssize);
	return (error);
}

aio_lio_t *
aio_list_get(aio_result_t *resultp)
{
	aio_lio_t	*head = NULL;
	aio_t		*aiop;
	aio_req_t 	**bucket;
	aio_req_t 	*reqp;
	long		index;

	aiop = curproc->p_aio;
	if (aiop == NULL)
		return (NULL);

	if (resultp) {
		index = AIO_HASH(resultp);
		bucket = &aiop->aio_hash[index];
		for (reqp = *bucket; reqp != NULL;
			reqp = reqp->aio_hash_next) {
			if (reqp->aio_req_resultp == resultp) {
				head = reqp->aio_req_lio;
				return (head);
			}
		}
	}
	return (NULL);
}


static void
lio_set_uerror(void *resultp, int error)
{
	/*
	 * the resultp field is a pointer to where the
	 * error should be written out to the user's
	 * aiocb.
	 *
	 */
	if (get_udatamodel() == DATAMODEL_NATIVE) {
		(void) suword32(&((aio_result_t *)resultp)->aio_errno, error);
		(void) sulword(&((aio_result_t *)resultp)->aio_return,
		    (ssize_t)-1);
	}
#ifdef	_SYSCALL32_IMPL
	else {
		(void) suword32(&((aio_result32_t *)resultp)->aio_errno, error);
		(void) suword32(&((aio_result32_t *)resultp)->aio_return,
		    (uint_t)-1);
	}
#endif  /* _SYSCALL32_IMPL */
}

/*
 * do cleanup completion for all requests in list. memory for
 * each request is also freed.
 */
static void
alio_cleanup(aio_t *aiop, aiocb_t **cbp, int nent, int run_mode)
{
	int i;
	aio_req_t *reqp;
	aio_result_t *resultp;
	aiocb64_32_t	*aiocb_64;

	for (i = 0; i < nent; i++) {
		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (cbp[i] == NULL)
				continue;
			if (run_mode == AIO_LARGEFILE) {
				aiocb_64 = (aiocb64_32_t *)cbp[i];
				resultp = (aio_result_t *)&aiocb_64->
				    aio_resultp;
			} else
				resultp = &cbp[i]->aio_resultp;
		}
#ifdef	_SYSCALL32_IMPL
		else {
			aiocb32_t	*aiocb_32;
			caddr32_t	*cbp32;

			cbp32 = (caddr32_t *)cbp;
			if (cbp32[i] == NULL)
				continue;
			if (run_mode == AIO_32) {
				aiocb_32 = (aiocb32_t *)cbp32[i];
				resultp = (aio_result_t *)&aiocb_32->
				    aio_resultp;
			} else if (run_mode == AIO_LARGEFILE) {
				aiocb_64 = (aiocb64_32_t *)cbp32[i];
				resultp = (aio_result_t *)&aiocb_64->
				    aio_resultp;
			}
		}
#endif  /* _SYSCALL32_IMPL */
		mutex_enter(&aiop->aio_mutex);
		reqp = aio_req_done(resultp);
		mutex_exit(&aiop->aio_mutex);
		if (reqp != NULL) {
			aphysio_unlock(reqp);
			aio_copyout_result(reqp);
			mutex_enter(&aiop->aio_mutex);
			aio_req_free(aiop, reqp);
			mutex_exit(&aiop->aio_mutex);
		}
	}
}

/*
 * write out the results for an aio request that is
 * done.
 */
static int
aioerror(void *cb, int run_mode)
{
	aio_result_t *resultp;
	aio_t *aiop;
	aio_req_t *reqp;
	int retval;

	aiop = curproc->p_aio;
	if (aiop == NULL || cb == NULL)
		return (EINVAL);

	if (get_udatamodel() == DATAMODEL_NATIVE) {
		if (run_mode == AIO_LARGEFILE)
			resultp = (aio_result_t *)&((aiocb64_32_t *)cb)->
			    aio_resultp;
		else
			resultp = &((aiocb_t *)cb)->aio_resultp;
	}
#ifdef	_SYSCALL32_IMPL
	else {
		if (run_mode == AIO_LARGEFILE)
			resultp = (aio_result_t *)&((aiocb64_32_t *)cb)->
			    aio_resultp;
		else if (run_mode == AIO_32)
			resultp = (aio_result_t *)&((aiocb32_t *)cb)->
			    aio_resultp;
	}
#endif  /* _SYSCALL32_IMPL */
	mutex_enter(&aiop->aio_mutex);
	retval = aio_req_find(resultp, &reqp);
	mutex_exit(&aiop->aio_mutex);
	if (retval == 0) {
		aphysio_unlock(reqp);
		aio_copyout_result(reqp);
		mutex_enter(&aiop->aio_mutex);
		aio_req_free(aiop, reqp);
		mutex_exit(&aiop->aio_mutex);
		return (0);
	} else if (retval == 1)
		return (EINPROGRESS);
		else if (retval == 2)
			return (EINVAL);
	return (0);
}

/*
 * 	aio_cancel - if no requests outstanding,
 *			return AIO_ALLDONE
 *			else
 *			return AIO_NOTCANCELED
 */
static int
aio_cancel(
	int	fildes,
	void 	*cb,
	long	*rval,
	int	run_mode)
{
	aio_t *aiop;
	void *resultp;
	int index;
	aio_req_t **bucket;
	aio_req_t *ent;


	/*
	 * Verify valid file descriptor
	 */
	if ((getf(fildes)) == NULL) {
		return (EBADF);
	}
	releasef(fildes);

	aiop = curproc->p_aio;
	if (aiop == NULL)
		return (EINVAL);

	if (aiop->aio_outstanding == 0) {
		*rval = AIO_ALLDONE;
		return (0);
	}

	mutex_enter(&aiop->aio_mutex);
	if (cb != NULL) {
		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (run_mode == AIO_LARGEFILE)
				resultp = (aio_result_t *)&((aiocb64_32_t *)cb)
				    ->aio_resultp;
			else
				resultp = &((aiocb_t *)cb)->aio_resultp;
		}
#ifdef	_SYSCALL32_IMPL
		else {
			if (run_mode == AIO_LARGEFILE)
				resultp = (aio_result_t *)&((aiocb64_32_t *)cb)
				    ->aio_resultp;
			else if (run_mode == AIO_32)
				resultp = (aio_result_t *)&((aiocb32_t *)cb)
				    ->aio_resultp;
		}
#endif  /* _SYSCALL32_IMPL */
		index = AIO_HASH(resultp);
		bucket = &aiop->aio_hash[index];
		for (ent = *bucket; ent != NULL; ent = ent->aio_hash_next) {
			if (ent->aio_req_resultp == resultp) {
				if ((ent->aio_req_flags & AIO_PENDING) == 0) {
					mutex_exit(&aiop->aio_mutex);
					*rval = AIO_ALLDONE;
					return (0);
				}
				mutex_exit(&aiop->aio_mutex);
				*rval = AIO_NOTCANCELED;
				return (0);
			}
		}
		mutex_exit(&aiop->aio_mutex);
		*rval = AIO_ALLDONE;
		return (0);
	}

	for (index = 0; index < AIO_HASHSZ; index++) {
		bucket = &aiop->aio_hash[index];
		for (ent = *bucket; ent != NULL; ent = ent->aio_hash_next) {
			if (ent->aio_req_fd == fildes) {
				if ((ent->aio_req_flags & AIO_PENDING) != 0) {
					mutex_exit(&aiop->aio_mutex);
					*rval = AIO_NOTCANCELED;
					return (0);
				}
			}
		}
	}
	mutex_exit(&aiop->aio_mutex);
	*rval = AIO_ALLDONE;
	return (0);
}

/*
 * solaris version of asynchronous read and write
 */
static int
arw(
	int	opcode,
	int	fdes,
	char	*bufp,
	int	bufsize,
	offset_t	offset,
	aio_result_t	*resultp,
	int		mode)
{
	file_t		*fp;
	int		error;
	struct vnode	*vp;
	aio_req_t	*reqp;
	aio_t		*aiop;
	int		(*aio_func)();
#ifdef _LP64
	aiocb_t		aiocb;
#else
	aiocb64_32_t	aiocb64;
#endif

	aiop = curproc->p_aio;
	if (aiop == NULL)
		return (EINVAL);

	if ((fp = getf(fdes)) == NULL) {
		return (EBADF);
	}

	/*
	 * check the permission of the partition
	 */
	if ((fp->f_flag & mode) == 0) {
		releasef(fdes);
		return (EBADF);
	}

	vp = fp->f_vnode;
	aio_func = check_vp(vp, mode);
	if (aio_func == NULL) {
		releasef(fdes);
		return (ENOTSUP);
	}
#ifdef _LP64
	aiocb.aio_fildes = fdes;
	aiocb.aio_buf = bufp;
	aiocb.aio_nbytes = bufsize;
	aiocb.aio_offset = offset;
	aiocb.aio_sigevent.sigev_notify = 0;
	error = aio_req_setup(&reqp, aiop, &aiocb, resultp);
#else
	aiocb64.aio_fildes = fdes;
	aiocb64.aio_buf = (caddr32_t)bufp;
	aiocb64.aio_nbytes = bufsize;
	aiocb64.aio_offset = offset;
	aiocb64.aio_sigevent.sigev_notify = 0;
	error = aio_req_setupLF(&reqp, aiop, &aiocb64, resultp);
#endif
	if (error) {
		releasef(fdes);
		return (error);
	}

	/*
	 * enable polling on this request if the opcode has
	 * the AIO poll bit set
	 */
	if (opcode & AIO_POLL_BIT)
		reqp->aio_req_flags |= AIO_POLL;

	if (bufsize == 0) {
		clear_active_fd(fdes);
		aio_zerolen(reqp);
		return (0);
	}
	/*
	 * send the request to driver.
	 * Clustering: If PXFS vnode, call PXFS function.
	 */
	error = (*aio_func)(vp, (aio_req_t *)&reqp->aio_req,
	    CRED());
	/*
	 * the fd is stored in the aio_req_t by aio_req_setup(), and
	 * is released by the aio_cleanup_thread() when the IO has
	 * completed.
	 */
	if (error) {
		releasef(fdes);
		mutex_enter(&aiop->aio_mutex);
		aio_req_free(aiop, reqp);
		aiop->aio_pending--;
		mutex_exit(&aiop->aio_mutex);
		return (error);
	}
	clear_active_fd(fdes);
	return (0);
}

/*
 * posix version of asynchronous read and write
 */
static	int
aiorw(
	int		opcode,
	void		*aiocb_arg,
	int		mode,
	int		run_mode)
{
#ifdef _SYSCALL32_IMPL
	aiocb32_t	aiocb32;
#endif
	aiocb64_32_t	aiocb64;
	aiocb_t		aiocb;
	file_t		*fp;
	int		error, fd;
	size_t		bufsize;
	struct vnode	*vp;
	aio_req_t	*reqp;
	aio_t		*aiop;
	int		(*aio_func)();
	aio_result_t	*resultp;
	model_t		model = get_udatamodel();
	aiop = curproc->p_aio;
	if (aiop == NULL)
		return (EINVAL);

	if (model == DATAMODEL_NATIVE) {
		if (run_mode != AIO_LARGEFILE) {
			if (copyin(aiocb_arg, &aiocb, sizeof (aiocb_t)))
				return (EFAULT);
			bufsize = aiocb.aio_nbytes;
			resultp = &(((aiocb_t *)aiocb_arg)->aio_resultp);
			if ((fp = getf(fd = aiocb.aio_fildes)) == NULL) {
				return (EBADF);
			}
		} else {
			/*
			 * We come here only when we make largefile
			 * call on 32 bit kernel using 32 bit library.
			 */
			if (copyin(aiocb_arg, &aiocb64, sizeof (aiocb64_32_t)))
				return (EFAULT);
			bufsize = aiocb64.aio_nbytes;
			resultp = (aio_result_t *)&(((aiocb64_32_t *)aiocb_arg)
			    ->aio_resultp);
			if ((fp = getf(fd = aiocb64.aio_fildes)) == NULL) {
				return (EBADF);
			}
		}
	}
#ifdef	_SYSCALL32_IMPL
	else {
		if (run_mode == AIO_32) {
			/* 32 bit system call is being made on 64 bit kernel */
			if (copyin(aiocb_arg, &aiocb32, sizeof (aiocb32_t)))
				return (EFAULT);

			bufsize = aiocb32.aio_nbytes;
			aiocb_32ton(&aiocb32, &aiocb);
			resultp = (aio_result_t *)&(((aiocb32_t *)aiocb_arg)->
			    aio_resultp);
			if ((fp = getf(fd = aiocb32.aio_fildes)) == NULL) {
				return (EBADF);
			}
		} else if (run_mode == AIO_LARGEFILE) {
			/*
			 * We come here only when we make largefile
			 * call on 64 bit kernel using 32 bit library.
			 */
			if (copyin(aiocb_arg, &aiocb64, sizeof (aiocb64_32_t)))
				return (EFAULT);
			bufsize = aiocb64.aio_nbytes;
			aiocb_LFton(&aiocb64, &aiocb);
			resultp = (aio_result_t *)&(((aiocb64_32_t *)aiocb_arg)
			    ->aio_resultp);
			if ((fp = getf(fd = aiocb64.aio_fildes)) == NULL)
				return (EBADF);
		}
	}
#endif  /* _SYSCALL32_IMPL */

	/*
	 * check the permission of the partition
	 */

	if ((fp->f_flag & mode) == 0) {
		releasef(fd);
		return (EBADF);
	}

	vp = fp->f_vnode;
	aio_func = check_vp(vp, mode);
	if (aio_func == NULL) {
		releasef(fd);
		return (ENOTSUP);
	}
	if ((model == DATAMODEL_NATIVE) && (run_mode == AIO_LARGEFILE))
		error = aio_req_setupLF(&reqp, aiop, &aiocb64, resultp);
	else
		error = aio_req_setup(&reqp, aiop, &aiocb, resultp);

	if (error) {
		releasef(fd);
		return (error);
	}
	/*
	 * enable polling on this request if the opcode has
	 * the AIO poll bit set
	 */
	if (opcode & AIO_POLL_BIT)
		reqp->aio_req_flags |= AIO_POLL;

	if (bufsize == 0) {
		clear_active_fd(fd);
		aio_zerolen(reqp);
		return (0);
	}

	/*
	 * send the request to driver.
	 * Clustering: If PXFS vnode, call PXFS function.
	 */
	error = (*aio_func)(vp, (aio_req_t *)&reqp->aio_req,
	    CRED());
	/*
	 * the fd is stored in the aio_req_t by aio_req_setup(), and
	 * is released by the aio_cleanup_thread() when the IO has
	 * completed.
	 */
	if (error) {
		releasef(fd);
		mutex_enter(&aiop->aio_mutex);
		aio_req_free(aiop, reqp);
		aiop->aio_pending--;
		mutex_exit(&aiop->aio_mutex);
		return (error);
	}
	clear_active_fd(fd);
	return (0);
}

/*
 * set error for a list IO entry that failed.
 */
static void
lio_set_error(aio_req_t *reqp)
{
	aio_t *aiop = curproc->p_aio;

	if (aiop == NULL)
		return;

	mutex_enter(&aiop->aio_mutex);
	aiop->aio_pending--;
	/* request failed, AIO_PHYSIODONE set to aviod physio cleanup. */
	reqp->aio_req_flags |= AIO_PHYSIODONE;
	/*
	 * Need to free the request now as its never
	 * going to get on the done queue
	 *
	 * Note: aio_outstanding is decremented in
	 *	 aio_req_free()
	 */
	aio_req_free(aiop, reqp);
	mutex_exit(&aiop->aio_mutex);
}

/*
 * check if a specified request is done, and remove it from
 * the done queue. otherwise remove anybody from the done queue
 * if NULL is specified.
 */
static aio_req_t *
aio_req_done(void *resultp)
{
	aio_req_t **bucket;
	aio_req_t *ent;
	aio_t *aiop = curproc->p_aio;
	long index;

	ASSERT(MUTEX_HELD(&aiop->aio_mutex));

	if (resultp) {
		index = AIO_HASH(resultp);
		bucket = &aiop->aio_hash[index];
		for (ent = *bucket; ent != NULL; ent = ent->aio_hash_next) {
			if (ent->aio_req_resultp == (aio_result_t *)resultp) {
				if (ent->aio_req_flags & AIO_DONEQ) {
					return (aio_req_remove(ent));
				}
				return (NULL);
			}
		}
		/* no match, resultp is invalid */
		return (NULL);
	}
	return (aio_req_remove(NULL));
}

/*
 * determine if a user-level resultp pointer is associated with an
 * active IO request. Zero is returned when the request is done,
 * and the request is removed from the done queue. Only when the
 * return value is zero, is the "reqp" pointer valid. One is returned
 * when the request is inprogress. Two is returned when the request
 * is invalid.
 */
static int
aio_req_find(aio_result_t *resultp, aio_req_t **reqp)
{
	aio_req_t **bucket;
	aio_req_t *ent;
	aio_t *aiop = curproc->p_aio;
	long index;

	ASSERT(MUTEX_HELD(&aiop->aio_mutex));

	index = AIO_HASH(resultp);
	bucket = &aiop->aio_hash[index];
	for (ent = *bucket; ent != NULL; ent = ent->aio_hash_next) {
		if (ent->aio_req_resultp == resultp) {
			if (ent->aio_req_flags & AIO_DONEQ) {
				*reqp = aio_req_remove(ent);
				return (0);
			}
			return (1);
		}
	}
	/* no match, resultp is invalid */
	return (2);
}

/*
 * remove a request from the done queue.
 */
static aio_req_t *
aio_req_remove(aio_req_t *reqp)
{
	aio_t *aiop = curproc->p_aio;
	aio_req_t *head;

	ASSERT(MUTEX_HELD(&aiop->aio_mutex));

	if (reqp) {
		ASSERT(reqp->aio_req_flags & AIO_DONEQ);
		if (reqp->aio_req_next == reqp)
			/* only one request on queue */
			aiop->aio_doneq = NULL;
		else {
			reqp->aio_req_next->aio_req_prev = reqp->aio_req_prev;
			reqp->aio_req_prev->aio_req_next = reqp->aio_req_next;
			if (reqp == aiop->aio_doneq)
				aiop->aio_doneq = reqp->aio_req_next;
		}
		reqp->aio_req_flags &= ~AIO_DONEQ;
		return (reqp);
	}
	if (aiop->aio_doneq) {
		head = aiop->aio_doneq;
		ASSERT(head->aio_req_flags & AIO_DONEQ);
		if (head == head->aio_req_next) {
			/* only one request on queue */
			aiop->aio_doneq = NULL;
		} else {
			head->aio_req_prev->aio_req_next = head->aio_req_next;
			head->aio_req_next->aio_req_prev = head->aio_req_prev;
			aiop->aio_doneq = head->aio_req_next;
		}
		head->aio_req_flags &= ~AIO_DONEQ;
		return (head);
	}
	return (NULL);
}

static int
aio_req_setup(
	aio_req_t	**reqpp,
	aio_t 		*aiop,
	aiocb_t 	*arg,
	aio_result_t 	*resultp)
{
	aio_req_t 	*reqp;
	sigqueue_t	*sqp;
	struct uio 	*uio;

	struct sigevent *sigev;
	int error;

	sigev = &arg->aio_sigevent;
	if ((sigev->sigev_notify == SIGEV_SIGNAL) &&
	    (sigev->sigev_signo > 0 && sigev->sigev_signo < NSIG)) {
		sqp = kmem_zalloc(sizeof (sigqueue_t), KM_NOSLEEP);
		if (sqp == NULL)
			return (EAGAIN);
		sqp->sq_func = NULL;
		sqp->sq_next = NULL;
		sqp->sq_info.si_code = SI_ASYNCIO;
		sqp->sq_info.si_pid = curproc->p_pid;
		sqp->sq_info.si_uid = curproc->p_cred->cr_uid;
		sqp->sq_info.si_signo = sigev->sigev_signo;
		sqp->sq_info.si_value.sival_int =
		    sigev->sigev_value.sival_int;
	} else
		sqp = NULL;

	mutex_enter(&aiop->aio_mutex);
	/*
	 * get an aio_reqp from the free list or allocate one
	 * from dynamic memory.
	 */
	if (error = aio_req_alloc(&reqp, resultp)) {
		mutex_exit(&aiop->aio_mutex);
		if (sqp)
			kmem_free(sqp, sizeof (sigqueue_t));
		return (error);
	}
	aiop->aio_pending++;
	aiop->aio_outstanding++;
	mutex_exit(&aiop->aio_mutex);
	/*
	 * initialize aio request.
	 */
	reqp->aio_req_flags = AIO_PENDING;
	reqp->aio_req_fd = arg->aio_fildes;
	reqp->aio_req_sigqp = sqp;
	uio = reqp->aio_req.aio_uio;
	uio->uio_iovcnt = 1;
	uio->uio_iov->iov_base = (caddr_t)arg->aio_buf;
	uio->uio_iov->iov_len = arg->aio_nbytes;
	uio->uio_loffset = arg->aio_offset;
	*reqpp = reqp;
	return (0);
}

/*
 * Allocate p_aio struct.
 */
static aio_t *
aio_aiop_alloc(void)
{
	aio_t	*aiop;

	ASSERT(MUTEX_HELD(&curproc->p_lock));

	aiop = kmem_zalloc(sizeof (struct aio), KM_NOSLEEP);
	if (aiop) {
		mutex_init(&aiop->aio_mutex, NULL, MUTEX_DEFAULT, NULL);
	}
	return (aiop);
}

/*
 * Allocate an aio_req struct.
 */
static int
aio_req_alloc(aio_req_t **nreqp, aio_result_t *resultp)
{
	aio_req_t *reqp;
	aio_t *aiop = curproc->p_aio;

	ASSERT(MUTEX_HELD(&aiop->aio_mutex));

	if ((reqp = aiop->aio_free) != NULL) {
		reqp->aio_req_flags = 0;
		aiop->aio_free = reqp->aio_req_next;
		/*
		 * Clustering:This field has to be specifically
		 * set to null so that the right thing can be
		 * done in aphysio()
		 */
		reqp->aio_req_buf.b_iodone = NULL;
	} else {
		/*
		 * Check whether memory is getting tight.
		 * This is a temporary mechanism to avoid memory
		 * exhaustion by a single process until we come up
		 * with a per process solution such as setrlimit().
		 */
		if (freemem < desfree)
			return (EAGAIN);

		reqp = kmem_zalloc(sizeof (struct aio_req_t), KM_NOSLEEP);
		if (reqp == NULL)
			return (EAGAIN);
		reqp->aio_req.aio_uio = &(reqp->aio_req_uio);
		reqp->aio_req.aio_uio->uio_iov = &(reqp->aio_req_iov);
		reqp->aio_req.aio_private = reqp;
	}

	reqp->aio_req_resultp = resultp;
	if (aio_hash_insert(reqp, aiop)) {
		reqp->aio_req_next = aiop->aio_free;
		aiop->aio_free = reqp;
		return (EINVAL);
	}
	*nreqp = reqp;
	return (0);
}

/*
 * Allocate an aio_lio_t struct.
 */
static int
aio_lio_alloc(aio_lio_t **head)
{
	aio_lio_t *liop;
	aio_t *aiop = curproc->p_aio;

	ASSERT(MUTEX_HELD(&aiop->aio_mutex));

	if ((liop = aiop->aio_lio_free) != NULL) {
		aiop->aio_lio_free = liop->lio_next;
	} else {
		/*
		 * Check whether memory is getting tight.
		 * This is a temporary mechanism to avoid memory
		 * exhaustion by a single process until we come up
		 * with a per process solution such as setrlimit().
		 */
		if (freemem < desfree)
			return (EAGAIN);

		liop = kmem_zalloc(sizeof (aio_lio_t), KM_NOSLEEP);
		if (liop == NULL)
			return (EAGAIN);
	}
	*head = liop;
	return (0);
}

/*
 * this is a special per-process thread that is only activated if
 * the process is unmapping a segment with outstanding aio. normally,
 * the process will have completed the aio before unmapping the
 * segment. If the process does unmap a segment with outstanding aio,
 * this special thread will guarentee that the locked pages due to
 * aphysio() are released, thereby permitting the segment to be
 * unmapped.
 */
static int
aio_cleanup_thread(aio_t *aiop)
{
	proc_t *p = curproc;
	struct as *as = p->p_as;
	int poked = 0;
	kcondvar_t *cvp;

	sigfillset(&curthread->t_hold);
	for (;;) {
		/*
		 * if a segment is being unmapped, and the current
		 * process's done queue is not empty, then every request
		 * on the doneq with locked resources should be forced
		 * to release their locks. By moving the doneq request
		 * to the cleanupq, aio_cleanup() will process the cleanupq,
		 * and place requests back onto the doneq. All requests
		 * processed by aio_cleanup() will have their physical
		 * resources unlocked.
		 */
		mutex_enter(&aiop->aio_mutex);
		aiop->aio_flags |= AIO_CLEANUP;
		if (AS_ISUNMAPWAIT(as) && aiop->aio_doneq) {
			aio_req_t *doneqhead = aiop->aio_doneq;
			aiop->aio_doneq = NULL;
			aio_cleanupq_concat(aiop, doneqhead, AIO_DONEQ);
		}
		mutex_exit(&aiop->aio_mutex);
		aio_cleanup(0);
		/*
		 * thread should block on the cleanupcv while
		 * AIO_CLEANUP is set.
		 */
		cvp = &aiop->aio_cleanupcv;
		mutex_enter(&aiop->aio_mutex);
		mutex_enter(&as->a_contents);
		if (aiop->aio_pollq == NULL) {
			/*
			 * AIO_CLEANUP determines when the cleanup thread
			 * should be active. this flag is only set when
			 * the cleanup thread is awakened by as_unmap().
			 * the flag is cleared when the blocking as_unmap()
			 * that originally awakened us is allowed to
			 * complete. as_unmap() blocks when trying to
			 * unmap a segment that has SOFTLOCKed pages. when
			 * the segment's pages are all SOFTUNLOCKed,
			 * as->a_flags & AS_UNMAPWAIT should be zero. The flag
			 * shouldn't be cleared right away if the cleanup thread
			 * was interrupted because the process is forking.
			 * this happens when cv_wait_sig() returns zero,
			 * because it was awakened by a pokelwps(), if
			 * the process is not exiting, it must be forking.
			 */
			if (AS_ISUNMAPWAIT(as) == 0 && !poked) {
				aiop->aio_flags &= ~AIO_CLEANUP;
				cvp = &as->a_cv;
			}
			mutex_exit(&aiop->aio_mutex);
			if (poked) {
				/*
				 * If the process is exiting/killed, return
				 * immediately without waiting for
				 * pending I/O's as aio_cleanup_exit()
				 * will deal with them.
				 */
				if (p->p_flag & (EXITLWPS|SKILLED))
					break;
				else if (p->p_flag &
				    (HOLDFORK|HOLDFORK1|HOLDWATCH)) {
					/*
					 * hold LWP until it
					 * is continued.
					 */
					mutex_exit(&as->a_contents);
					mutex_enter(&p->p_lock);
					stop(PR_SUSPENDED,
					    SUSPEND_NORMAL);
					mutex_exit(&p->p_lock);
					poked = 0;
					continue;
				}
			} else {
				while (AS_ISUNMAPWAIT(as) == 0 && !poked) {
					poked = !cv_wait_sig(cvp,
						&as->a_contents);
					if (AS_ISUNMAPWAIT(as) == 0)
						cv_signal(cvp);
				}
			}
		} else
			mutex_exit(&aiop->aio_mutex);

		mutex_exit(&as->a_contents);
	}
exit:
	mutex_exit(&as->a_contents);
	ASSERT((curproc->p_flag & (EXITLWPS|SKILLED)));
	return (0);
}

/*
 * save a reference to a user's outstanding aio in a hash list.
 */
static int
aio_hash_insert(
	aio_req_t *aio_reqp,
	aio_t *aiop)
{
	long index;
	aio_result_t *resultp = aio_reqp->aio_req_resultp;
	aio_req_t *current;
	aio_req_t **nextp;

	index = AIO_HASH(resultp);
	nextp = &aiop->aio_hash[index];
	while ((current = *nextp) != NULL) {
		if (current->aio_req_resultp == resultp)
			return (DUPLICATE);
		nextp = &current->aio_hash_next;
	}
	*nextp = aio_reqp;
	aio_reqp->aio_hash_next = NULL;
	return (0);
}

static int
(*check_vp(struct vnode *vp, int mode))(vnode_t *, struct aio_req *,
    cred_t *)
{
	struct snode *sp;
	dev_t		dev;
	struct cb_ops  	*cb;
	major_t		major;
	int		(*aio_func)();

	dev = vp->v_rdev;
	major = getmajor(dev);

	/*
	 * return NULL for requests to files and STREAMs so
	 * that libaio takes care of them.
	 */
	if (vp->v_type == VCHR) {
		/* no stream device for kaio */
		if (STREAMSTAB(major)) {
			return (NULL);
		}
	} else {
		return (NULL);
	}

	/*
	 * Check old drivers which do not have async I/O entry points.
	 */
	if (devopsp[major]->devo_rev < 3)
		return (NULL);

	cb = devopsp[major]->devo_cb_ops;

	if (cb->cb_rev < 1)
		return (NULL);

	/*
	 * Check whether this device is a block device.
	 * Kaio is not supported for devices like tty.
	 */
	if (cb->cb_strategy == nodev || cb->cb_strategy == NULL)
		return (NULL);

	/*
	 * Clustering: If vnode is a PXFS vnode, then the device may be remote.
	 * We cannot call the driver directly. Instead return the
	 * PXFS functions.
	 */

	if (IS_PXFSVP(vp)) {
		if (mode & FREAD)
			return (clpxfs_aio_read);
		else
			return (clpxfs_aio_write);
	}
	if (mode & FREAD)
		aio_func = (cb->cb_aread == nodev) ? NULL : driver_aio_read;
	else
		aio_func = (cb->cb_awrite == nodev) ? NULL : driver_aio_write;

	/*
	 * Do we need this ?
	 * nodev returns ENXIO anyway.
	 */
	if (aio_func == nodev)
		return (NULL);

	sp = VTOS(vp);
	smark(sp, SACC);
	return (aio_func);
}

/*
 * Clustering: We want check_vp to return a function prototyped
 * correctly that will be common to both PXFS and regular case.
 * We define this intermediate function that will do the right
 * thing for driver cases.
 */

static int
driver_aio_write(vnode_t *vp, struct aio_req *aio, cred_t *cred_p)
{
	dev_t dev;
	struct cb_ops  	*cb;

	ASSERT(vp->v_type == VCHR);
	ASSERT(!IS_PXFSVP(vp));
	dev = VTOS(vp)->s_dev;
	ASSERT(STREAMSTAB(getmajor(dev)) == NULL);

	cb = devopsp[getmajor(dev)]->devo_cb_ops;

	ASSERT(cb->cb_awrite != nodev);
	return ((*cb->cb_awrite)(dev, aio, cred_p));
}

/*
 * Clustering: We want check_vp to return a function prototyped
 * correctly that will be common to both PXFS and regular case.
 * We define this intermediate function that will do the right
 * thing for driver cases.
 */

static int
driver_aio_read(vnode_t *vp, struct aio_req *aio, cred_t *cred_p)
{
	dev_t dev;
	struct cb_ops  	*cb;

	ASSERT(vp->v_type == VCHR);
	ASSERT(!IS_PXFSVP(vp));
	dev = VTOS(vp)->s_dev;
	ASSERT(!STREAMSTAB(getmajor(dev)));

	cb = devopsp[getmajor(dev)]->devo_cb_ops;

	ASSERT(cb->cb_aread != nodev);
	return ((*cb->cb_aread)(dev, aio, cred_p));
}

/*
 * This routine is  specificaly for large file support.
 * Gets called only when largefile call is made by a 32bit
 * process on ILP32 kernel. All 64bit processes are large file
 * by definition, so such a process calls alio() instead.
 */

static int
alioLF(
	int		mode_arg,
	void		*aiocb_arg,
	int		nent,
	void		*sigev)
{
	file_t		*fp;
	file_t		*prev_fp = NULL;
	int		prev_mode = -1;
	struct vnode	*vp;
	aio_lio_t	*head;
	aio_req_t	*reqp;
	aio_t		*aiop;
	caddr_t		cbplist;
	aiocb64_32_t	*cbp;
	caddr32_t	*ucbp;
	aiocb64_32_t	cb64;
	aiocb64_32_t	*aiocb = &cb64;
#ifdef _LP64
	aiocb_t		aiocb_n;
#endif
	struct sigevent32	sigevk;
	sigqueue_t	*sqp;
	int		(*aio_func)();
	int		mode;
	int		error = 0, aio_errors = 0;
	int		i;
	size_t		ssize;
	int		deadhead = 0;
	int		aio_notsupported = 0;

	aiop = curproc->p_aio;
	if (aiop == NULL || nent <= 0 || nent > _AIO_LISTIO_MAX)
		return (EINVAL);

	ASSERT(get_udatamodel() == DATAMODEL_ILP32);

	ssize = (sizeof (caddr32_t) * nent);
	cbplist = kmem_alloc(ssize, KM_SLEEP);
	ucbp = (caddr32_t *)cbplist;

	if (copyin(aiocb_arg, cbplist, ssize)) {
		error = EFAULT;
		goto done;
	}

	if (sigev) {
		if (copyin(sigev, &sigevk, sizeof (sigevk))) {
			error = EFAULT;
			goto done;
		}
	}

	/*
	 * a list head should be allocated if notification is
	 * enabled for this list.
	 */
	head = NULL;
	if ((mode_arg == LIO_WAIT) || sigev) {
		mutex_enter(&aiop->aio_mutex);
		error = aio_lio_alloc(&head);
		mutex_exit(&aiop->aio_mutex);
		if (error) {
			goto done;
		}
		deadhead = 1;
		head->lio_nent = nent;
		head->lio_refcnt = nent;
		if (sigev && (sigevk.sigev_notify == SIGEV_SIGNAL) &&
		    (sigevk.sigev_signo > 0 && sigevk.sigev_signo < NSIG)) {
			sqp = kmem_zalloc(sizeof (sigqueue_t),
			    KM_NOSLEEP);
			if (sqp == NULL) {
				error = EAGAIN;
				goto done;
			}
			sqp->sq_func = NULL;
			sqp->sq_next = NULL;
			sqp->sq_info.si_code = SI_ASYNCIO;
			sqp->sq_info.si_pid = curproc->p_pid;
			sqp->sq_info.si_uid = curproc->p_cred->cr_uid;
			sqp->sq_info.si_signo = sigevk.sigev_signo;
			sqp->sq_info.si_value.sival_int =
				sigevk.sigev_value.sival_int;
			head->lio_sigqp = sqp;
		} else
			head->lio_sigqp = NULL;
	}

	for (i = 0; i < nent; i++, ucbp++) {

		cbp = (aiocb64_32_t *)*ucbp;
		/* skip entry if it can't be copied. */
		if (cbp == NULL || copyin(cbp, aiocb, sizeof (aiocb64_32_t))) {
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}

		/* skip if opcode for aiocb is LIO_NOP */

		mode = aiocb->aio_lio_opcode;
		if (mode == LIO_NOP) {
			cbp = NULL;
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}

		/* increment file descriptor's ref count. */
		if ((fp = getf(aiocb->aio_fildes)) == NULL) {
			lio_set_uerror(&cbp->aio_resultp, EBADF);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			aio_errors++;
			continue;
		}

		/*
		 * check the permission of the partition
		 */
		mode = aiocb->aio_lio_opcode;
		if ((fp->f_flag & mode) == 0) {
			releasef(aiocb->aio_fildes);
			lio_set_uerror(&cbp->aio_resultp, EBADF);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			aio_errors++;
			continue;
		}

		/*
		 * common case where requests are to the same fd
		 * for the same r/w operation
		 * for UFS, need to set ENOTSUP
		 */
		if ((fp != prev_fp) || (mode != prev_mode)) {
			vp = fp->f_vnode;
			aio_func = check_vp(vp, mode);
			if (aio_func == NULL) {
				prev_fp = NULL;
				releasef(aiocb->aio_fildes);
				lio_set_uerror(&cbp->aio_resultp, ENOTSUP);
				aio_notsupported++;
				if (head) {
					mutex_enter(&aiop->aio_mutex);
					head->lio_nent--;
					head->lio_refcnt--;
					mutex_exit(&aiop->aio_mutex);
				}
				continue;
			} else {
				prev_fp = fp;
				prev_mode = mode;
			}
		}
#ifdef	_LP64
		aiocb_LFton(aiocb, &aiocb_n);
		error = aio_req_setup(&reqp, aiop, &aiocb_n,
		    (aio_result_t *)&cbp->aio_resultp);
#else
		error = aio_req_setupLF(&reqp, aiop, aiocb,
		    (aio_result_t *)&cbp->aio_resultp);
#endif  /* _LP64 */
		if (error) {
			releasef(aiocb->aio_fildes);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			aio_errors++;
			continue;
		}

		reqp->aio_req_lio = head;
		deadhead = 0;

		/*
		 * Set the errno field now before sending the request to
		 * the driver to avoid a race condition
		 */
		(void) suword32(&cbp->aio_resultp.aio_errno,
			    EINPROGRESS);

		if (aiocb->aio_nbytes == 0) {
			clear_active_fd(aiocb->aio_fildes);
			aio_zerolen(reqp);
			continue;
		}

		/*
		 * send the request to driver.
		 * Clustering: If PXFS vnode, call PXFS function.
		 */
		error = (*aio_func)(vp, (aio_req_t *)&reqp->aio_req,
		    CRED());
		/*
		 * the fd's ref count is not decremented until the IO has
		 * completed unless there was an error.
		 */
		if (error) {
			releasef(aiocb->aio_fildes);
			lio_set_uerror(&cbp->aio_resultp, error);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			if (error == ENOTSUP)
				aio_notsupported++;
			else
				aio_errors++;
			lio_set_error(reqp);
		} else {
			clear_active_fd(aiocb->aio_fildes);
		}
	}

	if (aio_notsupported) {
		error = ENOTSUP;
	} else if (aio_errors) {
		/*
		 * return EIO if any request failed
		 */
		error = EIO;
	}

	if (mode_arg == LIO_WAIT) {
		mutex_enter(&aiop->aio_mutex);
		while (head->lio_refcnt > 0) {
			if (!cv_wait_sig(&head->lio_notify, &aiop->aio_mutex)) {
				mutex_exit(&aiop->aio_mutex);
				error = EINTR;
				goto done;
			}
		}
		mutex_exit(&aiop->aio_mutex);
		alio_cleanup(aiop, (aiocb_t **)cbplist, nent, AIO_LARGEFILE);
	}

done:
	kmem_free(cbplist, ssize);
	if (deadhead) {
		if (head->lio_sigqp)
			kmem_free(head->lio_sigqp, sizeof (sigqueue_t));
		kmem_free(head, sizeof (aio_lio_t));
	}
	return (error);
}

#ifdef  _SYSCALL32_IMPL
static void
aiocb_LFton(aiocb64_32_t *src, aiocb_t *dest)
{

	dest->aio_fildes = src->aio_fildes;
	dest->aio_buf = (void *)src->aio_buf;
	dest->aio_nbytes = (size_t)src->aio_nbytes;
	dest->aio_offset = (off_t)src->aio_offset;
	dest->aio_reqprio = src->aio_reqprio;
	dest->aio_sigevent.sigev_notify = src->aio_sigevent.sigev_notify;
	dest->aio_sigevent.sigev_signo = src->aio_sigevent.sigev_signo;
	dest->aio_sigevent.sigev_value.sival_ptr =
		(caddr_t)src->aio_sigevent.sigev_value.sival_ptr;
	dest->aio_sigevent.sigev_notify_function =
		(void (*)(union sigval))src->aio_sigevent.sigev_notify_function;
	dest->aio_sigevent.sigev_notify_attributes =
	    (pthread_attr_t *)src->aio_sigevent.sigev_notify_attributes;
	dest->aio_sigevent.__sigev_pad2 = src->aio_sigevent.__sigev_pad2;
	dest->aio_lio_opcode = src->aio_lio_opcode;
	dest->aio_state = src->aio_state;
	dest->aio__pad[0] = src->aio__pad[0];
}
#endif

/*
 * This function is used only by largefile call made by
 * 32 bit application on 32 bit kernel
 */
static int
aio_req_setupLF(
	aio_req_t	**reqpp,
	aio_t		*aiop,
	aiocb64_32_t	*arg,
	aio_result_t	*resultp)
{
	aio_req_t	*reqp;
	sigqueue_t	*sqp;
	struct	uio	*uio;

	struct	sigevent	*sigev;
	int error;

	sigev = (struct	sigevent *)&arg->aio_sigevent;
	if ((sigev->sigev_notify == SIGEV_SIGNAL) &&
	    (sigev->sigev_signo > 0 && sigev->sigev_signo < NSIG)) {
		sqp = kmem_zalloc(sizeof (sigqueue_t), KM_NOSLEEP);
		if (sqp == NULL)
			return (EAGAIN);
		sqp->sq_func = NULL;
		sqp->sq_next = NULL;
		sqp->sq_info.si_code = SI_ASYNCIO;
		sqp->sq_info.si_pid = curproc->p_pid;
		sqp->sq_info.si_uid = curproc->p_cred->cr_uid;
		sqp->sq_info.si_signo = sigev->sigev_signo;
		sqp->sq_info.si_value.sival_int =
			sigev->sigev_value.sival_int;
	} else
		sqp = NULL;

	mutex_enter(&aiop->aio_mutex);
	/*
	 * get an aio_reqp from the free list or allocate one
	 * from dynamic memory.
	 */
	if (error = aio_req_alloc(&reqp, resultp)) {
		mutex_exit(&aiop->aio_mutex);
		if (sqp)
			kmem_free(sqp, sizeof (sigqueue_t));
		return (error);
	}
	aiop->aio_pending++;
	aiop->aio_outstanding++;
	mutex_exit(&aiop->aio_mutex);
	/*
	 * initialize aio request.
	 */
	reqp->aio_req_flags = AIO_PENDING;
	reqp->aio_req_fd = arg->aio_fildes;
	reqp->aio_req_sigqp = sqp;
	uio = reqp->aio_req.aio_uio;
	uio->uio_iovcnt = 1;
	uio->uio_iov->iov_base = (caddr_t)arg->aio_buf;
	uio->uio_iov->iov_len = arg->aio_nbytes;
	uio->uio_loffset = arg->aio_offset;
	*reqpp = reqp;
	return (0);
}

/*
 * This routine is  specificaly for ILP32 data size .
 * Gets called only when ILP32 size call is made on ILP32 and LP64 kernel
 */

static int
alio32(
	int		mode_arg,
	void		*aiocb_arg,
	int		nent,
	void		*sigev_arg)
{
	file_t		*fp;
	file_t		*prev_fp = NULL;
	int		prev_mode = -1;
	struct vnode	*vp;
	aio_lio_t	*head;
	aio_req_t	*reqp;
	aio_t		*aiop;
	aiocb_t		cb;
	aiocb_t		*aiocb = &cb;
	caddr_t		cbplist;
#ifdef	_LP64
	aiocb32_t	*cbp;
	caddr32_t	*ucbp;
	aiocb32_t	cb32;
	aiocb32_t	*aiocb32 = &cb32;
	struct sigevent32	sigev;
#else
	aiocb_t		*cbp, **ucbp;
	struct sigevent	sigev;
#endif
	sigqueue_t	*sqp;
	int		(*aio_func)();
	int		mode;
	int		error = 0, aio_errors = 0;
	int		i;
	size_t		ssize;
	int		deadhead = 0;
	int		aio_notsupported = 0;

	aiop = curproc->p_aio;
	if (aiop == NULL || nent <= 0 || nent > _AIO_LISTIO_MAX)
		return (EINVAL);

#ifdef	_LP64
	ssize = (sizeof (caddr32_t) * nent);
#else
	ssize = (sizeof (aiocb_t *) * nent);
#endif
	cbplist = kmem_alloc(ssize, KM_SLEEP);
	ucbp = (void *)cbplist;

	if (copyin(aiocb_arg, cbplist, ssize)) {
		error = EFAULT;
		goto done;
	}

	if (sigev_arg) {
		if (copyin(sigev_arg, &sigev, sizeof (struct sigevent32))) {
			error = EFAULT;
			goto done;
		}
	}

	/*
	 * a list head should be allocated if notification is
	 * enabled for this list.
	 */
	head = NULL;
	if ((mode_arg == LIO_WAIT) || sigev_arg) {
		mutex_enter(&aiop->aio_mutex);
		error = aio_lio_alloc(&head);
		mutex_exit(&aiop->aio_mutex);
		if (error) {
			goto done;
		}
		deadhead = 1;
		head->lio_nent = nent;
		head->lio_refcnt = nent;
		if (sigev_arg && (sigev.sigev_notify == SIGEV_SIGNAL) &&
		    (sigev.sigev_signo > 0 && sigev.sigev_signo < NSIG)) {
			sqp = kmem_zalloc(sizeof (sigqueue_t),
			    KM_NOSLEEP);
			if (sqp == NULL) {
				error = EAGAIN;
				goto done;
			}
			sqp->sq_func = NULL;
			sqp->sq_next = NULL;
			sqp->sq_info.si_code = SI_ASYNCIO;
			sqp->sq_info.si_pid = curproc->p_pid;
			sqp->sq_info.si_uid = curproc->p_cred->cr_uid;
			sqp->sq_info.si_signo = sigev.sigev_signo;
			sqp->sq_info.si_value.sival_int =
				sigev.sigev_value.sival_int;
			head->lio_sigqp = sqp;
		} else
			head->lio_sigqp = NULL;
	}

	for (i = 0; i < nent; i++, ucbp++) {

		/* skip entry if it can't be copied. */
#ifdef	_LP64
		cbp = (aiocb32_t *)*ucbp;
		if (cbp == NULL || copyin(cbp, aiocb32, sizeof (aiocb32_t)))
#else
		cbp = (aiocb_t *)*ucbp;
		if (cbp == NULL || copyin(cbp, aiocb, sizeof (aiocb_t)))
#endif
		{
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}
#ifdef	_LP64
		/*
		 * copy 32 bit structure into 64 bit structure
		 */
		aiocb_32ton(aiocb32, aiocb);
#endif /* _LP64 */

		/* skip if opcode for aiocb is LIO_NOP */

		mode = aiocb->aio_lio_opcode;
		if (mode == LIO_NOP) {
			cbp = NULL;
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}

		/* increment file descriptor's ref count. */
		if ((fp = getf(aiocb->aio_fildes)) == NULL) {
			lio_set_uerror(&cbp->aio_resultp, EBADF);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			aio_errors++;
			continue;
		}

		/*
		 * check the permission of the partition
		 */
		mode = aiocb->aio_lio_opcode;
		if ((fp->f_flag & mode) == 0) {
			releasef(aiocb->aio_fildes);
			lio_set_uerror(&cbp->aio_resultp, EBADF);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			aio_errors++;
			continue;
		}

		/*
		 * common case where requests are to the same fd
		 * for the same r/w operation
		 * for UFS, need to set ENOTSUP
		 */
		if ((fp != prev_fp) || (mode != prev_mode)) {
			vp = fp->f_vnode;
			aio_func = check_vp(vp, mode);
			if (aio_func == NULL) {
				prev_fp = NULL;
				releasef(aiocb->aio_fildes);
				lio_set_uerror(&cbp->aio_resultp,
				    ENOTSUP);
				aio_notsupported++;
				if (head) {
					mutex_enter(&aiop->aio_mutex);
					head->lio_nent--;
					head->lio_refcnt--;
					mutex_exit(&aiop->aio_mutex);
				}
				continue;
			} else {
				prev_fp = fp;
				prev_mode = mode;
			}
		}
		if (error = aio_req_setup(&reqp, aiop, aiocb,
		    (aio_result_t *)&cbp->aio_resultp)) {
			releasef(aiocb->aio_fildes);
			lio_set_uerror(&cbp->aio_resultp, error);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			aio_errors++;
			continue;
		}

		reqp->aio_req_lio = head;
		deadhead = 0;

		/*
		 * Set the errno field now before sending the request to
		 * the driver to avoid a race condition
		 */
		(void) suword32(&cbp->aio_resultp.aio_errno,
			    EINPROGRESS);

		if (aiocb->aio_nbytes == 0) {
			clear_active_fd(aiocb->aio_fildes);
			aio_zerolen(reqp);
			continue;
		}

		/*
		 * send the request to driver.
		 * Clustering: If PXFS vnode, call PXFS function.
		 */
		error = (*aio_func)(vp, (aio_req_t *)&reqp->aio_req,
		    CRED());
		/*
		 * the fd's ref count is not decremented until the IO has
		 * completed unless there was an error.
		 */
		if (error) {
			releasef(aiocb->aio_fildes);
			lio_set_uerror(&cbp->aio_resultp, error);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			if (error == ENOTSUP)
				aio_notsupported++;
			else
				aio_errors++;
			lio_set_error(reqp);
		} else {
			clear_active_fd(aiocb->aio_fildes);
		}
	}

	if (aio_notsupported) {
		error = ENOTSUP;
	} else if (aio_errors) {
		/*
		 * return EIO if any request failed
		 */
		error = EIO;
	}

	if (mode_arg == LIO_WAIT) {
		mutex_enter(&aiop->aio_mutex);
		while (head->lio_refcnt > 0) {
			if (!cv_wait_sig(&head->lio_notify, &aiop->aio_mutex)) {
				mutex_exit(&aiop->aio_mutex);
				error = EINTR;
				goto done;
			}
		}
		mutex_exit(&aiop->aio_mutex);
		alio_cleanup(aiop, (aiocb_t **)cbplist, nent, AIO_32);
	}

done:
	kmem_free(cbplist, ssize);
	if (deadhead) {
		if (head->lio_sigqp)
			kmem_free(head->lio_sigqp, sizeof (sigqueue_t));
		kmem_free(head, sizeof (aio_lio_t));
	}
	return (error);
}


#ifdef  _SYSCALL32_IMPL
void
aiocb_32ton(aiocb32_t *src, aiocb_t *dest)
{

	dest->aio_fildes = src->aio_fildes;
	dest->aio_buf = (caddr_t)src->aio_buf;
	dest->aio_nbytes = (size_t)src->aio_nbytes;
	dest->aio_offset = (off_t)src->aio_offset;
	dest->aio_reqprio = src->aio_reqprio;
	dest->aio_sigevent.sigev_notify = src->aio_sigevent.sigev_notify;
	dest->aio_sigevent.sigev_signo = src->aio_sigevent.sigev_signo;
	dest->aio_sigevent.sigev_value.sival_ptr =
		(caddr_t)src->aio_sigevent.sigev_value.sival_ptr;
	dest->aio_sigevent.sigev_notify_function =
		(void (*)(union sigval))src->aio_sigevent.sigev_notify_function;
	dest->aio_sigevent.sigev_notify_attributes =
	    (pthread_attr_t *)src->aio_sigevent.sigev_notify_attributes;
	dest->aio_sigevent.__sigev_pad2 = src->aio_sigevent.__sigev_pad2;
	dest->aio_lio_opcode = src->aio_lio_opcode;
	dest->aio_state = src->aio_state;
	dest->aio__pad[0] = src->aio__pad[0];
}
#endif /* _SYSCALL32_IMPL */
