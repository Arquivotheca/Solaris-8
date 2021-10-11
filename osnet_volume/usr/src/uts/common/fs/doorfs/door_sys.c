/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)door_sys.c	1.54	99/08/17 SMI"

/*
 * System call I/F to doors (outside of vnodes I/F) and misc support
 * routines
 */
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/door.h>
#include <sys/door_data.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <sys/class.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/stack.h>
#include <sys/debug.h>
#include <sys/cpuvar.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/sobject.h>
#include <sys/schedctl.h>

#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/vmsystm.h>
#include <vm/as.h>
#include <vm/hat.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <vm/seg_vn.h>

#include <sys/modctl.h>
#include <sys/syscall.h>
#include <sys/pathname.h>

#if defined(__ia64)

/*
 * IA64 needs to support IA32 applications, which is a bit of a problem.
 * The problem is that "long long" on IA32 is only 4-byte aligned,
 * instead of 8-byte aligned as it is on IA64 and SPARC (both v7 and v9).
 * "struct door_desc" and "struct door_info" need special handling when
 * being passed to or from IA32 applications because they contained
 * "long long"s that are not 8-byte aligned.  We use the functions below,
 * defined in door_support.c on Intel, to do the dirty work on ia64,
 * but direct the calls to the standard copyin/copyout functions as was
 * always done in the other kernels.
 */

int copyin_desc(const door_desc_t *uaddr, door_desc_t *kaddr, size_t count);
int copyout_desc(const door_desc_t *kaddr, door_desc_t *uaddr, size_t count);
int copyout_info(const door_info_t *kaddr, door_info_t *uaddr, size_t count);

#else

#define	copyin_desc	copyin
#define	copyout_desc	copyout
#define	copyout_info	copyout

#endif

/*
 * The maximum amount of data that will be transfered using a intermediate
 * kernel buffer. For some architectures (e.g. x86) it is actually faster
 * to map in the destination and perform a 1-copy transfer when
 * the data reaches a certain threshold (say 12k for x86), but other
 * architures such as fusion are always faster doing a 2-copy transfer,
 * but we cannot kmem_alloc huge buffers to hold the data.
 */
size_t	door_max_arg = 64 * 1024;

/*
 * Maximum amount of data that will be transfered in a reply to a
 * door_upcall.  Need to guard against a process returning huge amounts
 * of data and getting the kernel stuck in kmem_alloc.
 */
size_t	door_max_upcall_reply = 1024 * 1024;

/*
 * Maximum number of descriptors allowed to be passed in a single
 * door_call or door_return.  We need to allocate kernel memory
 * for all of them at once, so we can't let it scale without limit.
 */
uint_t door_max_desc = 1024;

/*
 * Definition of a door handle, used by other kernel subsystems when
 * calling door functions.  This is really a file structure but we
 * want to hide that fact.
 */
struct __door_handle {
	file_t dh_file;
};

#define	DHTOF(dh) ((file_t *)(dh))
#define	FTODH(fp) ((door_handle_t)(fp))

static int doorfs(long, long, long, long, long, long);

static struct sysent door_sysent = {
	6,
	SE_ARGC | SE_NOUNLOAD,
	(int (*)())doorfs,
};

static struct modlsys modlsys = {
	&mod_syscallops, "doors", &door_sysent
};

#ifdef _SYSCALL32_IMPL

static int
doorfs32(int32_t arg1, int32_t arg2, int32_t arg3, int32_t arg4,
    int32_t arg5, int32_t subcode);

static struct sysent door_sysent32 = {
	6,
	SE_ARGC | SE_NOUNLOAD,
	(int (*)())doorfs32,
};

static struct modlsys modlsys32 = {
	&mod_syscallops32,
	"32-bit door syscalls",
	&door_sysent32
};
#endif

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlsys,
#ifdef _SYSCALL32_IMPL
	&modlsys32,
#endif
	NULL
};

kcondvar_t door_cv;
dev_t	doordev;

extern	struct vfs door_vfs;
extern	struct vfsops door_vfsops;

int
_init(void)
{
	major_t major;

	mutex_init(&door_knob, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&door_cv, NULL, CV_DEFAULT, NULL);
	if ((major = getudev()) == (major_t)-1)
		return (ENXIO);
	doordev = makedevice(major, 0);

	/* Create a dummy vfs */
	door_vfs.vfs_op = &door_vfsops;
	door_vfs.vfs_flag = VFS_RDONLY;
	door_vfs.vfs_dev = doordev;
	vfs_make_fsid(&(door_vfs.vfs_fsid), doordev, 0);

	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * System call wrapper for all door related system calls
 */
static int
doorfs(long arg1, long arg2, long arg3,
	long arg4, long arg5, long subcode)
{
	switch (subcode) {
	case DOOR_CALL:
		return (door_call(arg1, (void *)arg2));
	case DOOR_RETURN:
		return (door_return((caddr_t)arg1, arg2, (door_desc_t *)arg3,
				arg4, (caddr_t)arg5));
	case DOOR_CREATE:
		return (door_create((void (*)())arg1, (void *)arg2, arg3));
	case DOOR_REVOKE:
		return (door_revoke(arg1));
	case DOOR_INFO:
		return (door_info(arg1, (struct door_info *)arg2));
	case DOOR_CRED:
		return (door_cred((struct door_cred *)arg1));
	case DOOR_BIND:
		return (door_bind(arg1));
	case DOOR_UNBIND:
		return (door_unbind());
	default:
		return (set_errno(EINVAL));
	}
}

#ifdef _SYSCALL32_IMPL
/*
 * System call wrapper for all door related system calls from 32-bit programs.
 * Needed at the moment because of the casts - they undo some damage
 * that truss causes (sign-extending the stack pointer) when truss'ing
 * a 32-bit program using doors.
 */
static int
doorfs32(int32_t arg1, int32_t arg2, int32_t arg3,
	int32_t arg4, int32_t arg5, int32_t subcode)
{
	switch (subcode) {
	case DOOR_CALL:
		return (door_call(arg1, (void *)(caddr32_t)arg2));
	case DOOR_RETURN:
		return (door_return((caddr_t)(caddr32_t)arg1, arg2,
		    (door_desc_t *)(caddr32_t)arg3, arg4,
		    (caddr_t)(caddr32_t)arg5));
	case DOOR_CREATE:
		return (door_create((void (*)())(caddr32_t)arg1,
		    (void *)(caddr32_t)arg2, arg3));
	case DOOR_REVOKE:
		return (door_revoke(arg1));
	case DOOR_INFO:
		return (door_info(arg1, (struct door_info *)(caddr32_t)arg2));
	case DOOR_CRED:
		return (door_cred((struct door_cred *)(caddr32_t)arg1));
	case DOOR_BIND:
		return (door_bind(arg1));
	case DOOR_UNBIND:
		return (door_unbind());
	default:
		return (set_errno(EINVAL));
	}
}
#endif

void shuttle_resume(kthread_t *, kmutex_t *);
void shuttle_swtch(kmutex_t *);
void shuttle_sleep(kthread_t *);

/*
 * Support routines
 */
static int door_create_common(void (*)(), void *, uint_t, proc_t *, int *,
    file_t **);
static int door_overflow(kthread_t *, caddr_t, size_t, door_desc_t *, uint_t);
static int door_args(kthread_t *);
static int door_results(kthread_t *, caddr_t, size_t, door_desc_t *, uint_t);
static int door_copy(struct as *, caddr_t, caddr_t, uint_t);
static void	door_server_exit(proc_t *, kthread_t *);
static void 	door_release_server(door_node_t *, kthread_t *);
static void	door_unref(void);
static kthread_t	*door_get_server(door_node_t *, int);
static door_node_t	*door_lookup(int, file_t **);
static int	door_translate_in(void);
static int	door_translate_out(void);
static void	door_fd_rele(door_desc_t *, uint_t, int);
static void	door_list_insert(door_node_t *);
static void	door_info_common(door_node_t *, door_info_t *, file_t *);

/*
 * System call to create a door
 */
int
door_create(void (*pc_cookie)(), void *data_cookie, uint_t attributes)
{
	int fd;
	proc_t *p = ttoproc(curthread);
	int err;

	if ((attributes & ~(DOOR_UNREF | DOOR_PRIVATE | DOOR_UNREF_MULTI)) ||
	    ((attributes & (DOOR_UNREF | DOOR_UNREF_MULTI)) ==
	    (DOOR_UNREF | DOOR_UNREF_MULTI)))
		return (set_errno(EINVAL));

	if ((err = door_create_common(pc_cookie, data_cookie, attributes, p,
	    &fd, NULL)) != 0)
		return (set_errno(err));

	f_setfd(fd, FD_CLOEXEC);
	return (fd);
}

/*
 * Common code for creating user and kernel doors.  If a door was
 * created, stores a file structure pointer in the location pointed
 * to by fpp (if fpp is non-NULL) and returns 0.  Also, if a non-NULL
 * pointer to a file descriptor is passed in as fdp, allocates a file
 * descriptor representing the door.  If a door could not be created,
 * returns an error.
 */
static int
door_create_common(void (*pc_cookie)(), void *data_cookie, uint_t attributes,
    proc_t *p, int *fdp, file_t **fpp)
{
	door_node_t	*dp;
	vnode_t		*vp;
	struct file	*fp;
	extern	struct vnodeops door_vnodeops;
	static door_id_t index = 0;

	dp = kmem_zalloc(sizeof (door_node_t), KM_SLEEP);

	dp->door_target = p;
	dp->door_data = data_cookie;
	dp->door_pc = pc_cookie;
	dp->door_flags = attributes;
	vp = DTOV(dp);
	mutex_init(&vp->v_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&vp->v_cv, NULL, CV_DEFAULT, NULL);
	vp->v_op = &door_vnodeops;
	vp->v_type = VDOOR;
	vp->v_vfsp = &door_vfs;
	vp->v_data = (caddr_t)vp;
	VN_HOLD(vp);
	mutex_enter(&door_knob);
	dp->door_index = index++;
	/* add to per-process door list */
	door_list_insert(dp);
	mutex_exit(&door_knob);

	if (falloc(vp, FREAD | FWRITE, &fp, fdp)) {
		/*
		 * If the file table is full, remove the door from the
		 * per-process list, free the door, and return NULL.
		 */
		mutex_enter(&door_knob);
		door_list_delete(dp);
		mutex_exit(&door_knob);
		kmem_free(dp, sizeof (door_node_t));
		return (EMFILE);
	}
	if (fdp != NULL)
		setf(*fdp, fp);
	mutex_exit(&fp->f_tlock);

	if (fpp != NULL)
		*fpp = fp;
	return (0);
}

/*
 * Door invocation.
 */
int
door_call(int did, void *args)
{
	/* Locals */
	door_node_t	*dp;
	kthread_t	*server_thread;
	int		error = 0;
	klwp_id_t	lwp;
	door_data_t	*ct;		/* curthread door_data */
	door_data_t	*st;		/* server thread door_data */
	/* destructor for data returned by a kernel server */
	void		(*destfn)() = NULL;
	void		*destarg;
	int		nonnative;

	lwp = ttolwp(curthread);
	nonnative = lwp_getdatamodel(lwp) != DATAMODEL_NATIVE;
	if ((ct = curthread->t_door) == NULL) {
		ct = curthread->t_door = kmem_zalloc(sizeof (door_data_t),
							KM_SLEEP);
	}
	/*
	 * Get the arguments
	 */
	if (args) {
		if (nonnative) {
			door_arg32_t    da32;

			if (copyin(args, &da32, sizeof (door_arg32_t)) != 0)
				return (set_errno(EFAULT));
			ct->d_args.data_ptr = (char *)da32.data_ptr;
			ct->d_args.data_size = da32.data_size;
			ct->d_args.desc_ptr = (door_desc_t *)da32.desc_ptr;
			ct->d_args.desc_num = da32.desc_num;
			ct->d_args.rbuf = (char *)da32.rbuf;
			ct->d_args.rsize = da32.rsize;
		} else {
			if (copyin(args, &ct->d_args, sizeof (door_arg_t)) != 0)
				return (set_errno(EFAULT));
		}
	} else {
		/* No arguments, and no results allowed */
		ct->d_noresults = 1;
		ct->d_args.data_size = 0;
		ct->d_args.desc_num = 0;
		ct->d_args.rsize = 0;
	}

	if ((dp = door_lookup(did, NULL)) == NULL)
		return (set_errno(EBADF));

	mutex_enter(&door_knob);
	if (DOOR_INVALID(dp)) {
		mutex_exit(&door_knob);
		error = EBADF;
		goto out;
	}

	/*
	 * Check for in-kernel door server.
	 */
	if (dp->door_target == &p0) {
		caddr_t rbuf = ct->d_args.rbuf;
		size_t rsize = ct->d_args.rsize;

		dp->door_active++;
		ct->d_kernel = 1;
		ct->d_error = DOOR_WAIT;
		mutex_exit(&door_knob);
		/* translate file descriptors to vnodes */
		if (ct->d_args.desc_num) {
			error = door_translate_in();
			if (error)
				goto out;
		}
		/*
		 * Call kernel door server.  Arguments are passed and
		 * returned as a door_arg pointer.  When called, data_ptr
		 * points to user data and desc_ptr points to a kernel list
		 * of door descriptors that have been converted to file
		 * structure pointers.  It's the server function's
		 * responsibility to copyin the data pointed to by data_ptr
		 * (this avoids extra copying in some cases).  On return,
		 * data_ptr points to a user buffer of data, and desc_ptr
		 * points to a kernel list of door descriptors representing
		 * files.  When a reference is passed to a kernel server,
		 * it is the server's responsibility to release the reference
		 * (by calling closef).  When the server includes a
		 * reference in its reply, it is released as part of the
		 * the call (the server must duplicate the reference if
		 * it wants to retain a copy).  The destfn, if set to
		 * non-NULL, is a destructor to be called when the returned
		 * kernel data (if any) is no longer needed (has all been
		 * translated and copied to user level).
		 */
		(*(dp->door_pc))(dp->door_data, &ct->d_args,
		    &destfn, &destarg, &error);
		mutex_enter(&door_knob);
		/* not implemented yet */
		if (--dp->door_active == 0 && (dp->door_flags & DOOR_DELAY))
			door_deliver_unref(dp);
		mutex_exit(&door_knob);
		if (error)
			goto out;

		/* translate vnodes to files */
		if (ct->d_args.desc_num) {
			error = door_translate_out();
			if (error)
				goto out;
		}
		ct->d_buf = ct->d_args.rbuf;
		ct->d_bufsize = ct->d_args.rsize;
		if (rsize < (ct->d_args.data_size +
		    (ct->d_args.desc_num * sizeof (door_desc_t)))) {
			/* handle overflow */
			error = door_overflow(curthread, ct->d_args.data_ptr,
			    ct->d_args.data_size, ct->d_args.desc_ptr,
			    ct->d_args.desc_num);
			if (error)
				goto out;
			/* door_overflow sets d_args rbuf and rsize */
		} else {
			ct->d_args.rbuf = rbuf;
			ct->d_args.rsize = rsize;
		}
		goto results;
	}

	/*
	 * Get a server thread from the target domain
	 */
	if ((server_thread = door_get_server(dp, 0)) == NULL) {
		mutex_exit(&door_knob);
		error = EAGAIN;
		goto out;
	}

	st = server_thread->t_door;
	if (ct->d_args.desc_num || ct->d_args.data_size) {
		/*
		 * Move data from client to server
		 */
		st->d_flag = DOOR_HOLD;
		mutex_exit(&door_knob);
		error = door_args(server_thread);
		mutex_enter(&door_knob);
		if (error) {
			/*
			 * We're not going to resume this thread after all
			 */
			st->d_flag = 0;
			door_release_server(dp, server_thread);
			shuttle_sleep(server_thread);
			mutex_exit(&door_knob);
			goto out;
		}
		if (st->d_flag & DOOR_WAITING)
			cv_signal(&st->d_cv);
		st->d_flag = 0;
	}

	dp->door_active++;
	ct->d_error = DOOR_WAIT;
	st->d_caller = curthread;
	st->d_active = dp;

	/* Feats don`t fail me now... */
	if (schedctl_check(curthread, SC_BLOCK)) {
		(void) schedctl_block(NULL);
		shuttle_resume(server_thread, &door_knob);
		schedctl_unblock();
	} else
		shuttle_resume(server_thread, &door_knob);

	mutex_enter(&door_knob);
shuttle_return:
	if ((error = ct->d_error) < 0) {
		/*
		 * Premature wakeup. Find out why (stop, fork, signal, exit ...)
		 */
		mutex_exit(&door_knob);		/* May block in ISSIG */
		if (ISSIG(curthread, FORREAL) ||
		    lwp->lwp_sysabort || MUSTRETURN(curproc, curthread)) {
			/* Signal, fork, ... */
			lwp->lwp_sysabort = 0;
			mutex_enter(&door_knob);
			error = EINTR;
			/*
			 * If the server hasn't exited,
			 * let it know we are not interested in any
			 * results. Send a SIGCANCEL.
			 */
			if (ct->d_error != DOOR_EXIT &&
			    st->d_caller == curthread) {
				proc_t	*p = ttoproc(server_thread);

				st->d_active = NULL;
				st->d_caller = NULL;
				st->d_flag = DOOR_HOLD;
				mutex_exit(&door_knob);

				mutex_enter(&p->p_lock);
				sigtoproc(p, server_thread, SIGCANCEL);
				mutex_exit(&p->p_lock);

				mutex_enter(&door_knob);
				st->d_flag = 0;
				cv_signal(&st->d_cv);
			}
		} else {
			/*
			 * Return from stop(), server exit...
			 *
			 * Note that the server could have done a
			 * door_return while the client was in stop state
			 * (ISSIG), in which case the error condition
			 * is updated by the server.
			 */
			mutex_enter(&door_knob);
			if (ct->d_error == DOOR_WAIT) {
				/* Still waiting for a reply */
				if (schedctl_check(curthread, SC_BLOCK)) {
					(void) schedctl_block(NULL);
					shuttle_swtch(&door_knob);
					schedctl_unblock();
				} else
					shuttle_swtch(&door_knob);
				mutex_enter(&door_knob);
				lwp->lwp_asleep = 0;
				goto	shuttle_return;
			} else if (ct->d_error == DOOR_EXIT) {
				/* Server exit */
				error = EINTR;
			} else {
				/* Server did a door_return during ISSIG */
				error = ct->d_error;
			}
		}
		/*
		 * Can't exit if the client is currently copying
		 * results for me
		 */
		while (ct->d_flag & DOOR_HOLD) {
			ct->d_flag |= DOOR_WAITING;
			cv_wait(&ct->d_cv, &door_knob);
		}
	}
	lwp->lwp_asleep = 0;		/* /proc */
	lwp->lwp_sysabort = 0;		/* /proc */
	if (--dp->door_active == 0 && (dp->door_flags & DOOR_DELAY))
		door_deliver_unref(dp);
	mutex_exit(&door_knob);

results:
	/*
	 * Move the results to userland (if any)
	 */
	if (error || ct->d_noresults)
		goto out;

	/*
	 * Copy back data if we haven't caused an overflow (already
	 * handled) and we are using a 2 copy transfer, or we are
	 * returning data from a kernel server.
	 */
	if (ct->d_args.data_size) {
		ct->d_args.data_ptr = ct->d_args.rbuf;
		if (ct->d_kernel || (!ct->d_overflow &&
		    ct->d_args.data_size <= door_max_arg)) {
			if (copyout(ct->d_buf, ct->d_args.rbuf,
			    ct->d_args.data_size)) {
				door_fp_close(ct->d_fpp, ct->d_args.desc_num);
				error = EFAULT;
				goto out;
			}
		}
	}

	/*
	 * stuff returned doors into our proc, copyout the descriptors
	 */
	if (ct->d_args.desc_num) {
		struct file 	**fpp;
		door_desc_t	*didpp;
		door_desc_t 	*start;
		uint_t		n = ct->d_args.desc_num;
		size_t 		dsize = n * sizeof (door_desc_t);

		start = didpp = kmem_alloc(dsize, KM_SLEEP);
		fpp = ct->d_fpp;

		while (n--) {
			if (door_insert(*fpp, didpp) == -1) {
				/* Cleanup newly created fd's */
				door_fd_close(start, didpp - start);
				/* Close remaining files */
				door_fp_close(fpp, n + 1);
				kmem_free(start, dsize);
				error = EMFILE;
				goto out;
			}
			fpp++; didpp++;
		}

		ct->d_args.desc_ptr = (door_desc_t *)(ct->d_args.rbuf +
		    roundup(ct->d_args.data_size, sizeof (door_desc_t)));

		if (copyout_desc(start, ct->d_args.desc_ptr, dsize)) {
			door_fd_close(start, ct->d_args.desc_num);
			error = EFAULT;
			kmem_free(start, dsize);
			goto out;
		}
		kmem_free(start, dsize);
	}
	/*
	 * Return the results
	 */
	if (nonnative) {
		door_arg32_t    da32;

		da32.data_ptr = (caddr32_t)ct->d_args.data_ptr;
		da32.data_size = ct->d_args.data_size;
		da32.desc_ptr = (caddr32_t)ct->d_args.desc_ptr;
		da32.desc_num = ct->d_args.desc_num;
		da32.rbuf = (caddr32_t)ct->d_args.rbuf;
		da32.rsize = ct->d_args.rsize;
		if (copyout(&da32, args, sizeof (door_arg32_t)) != 0) {
			error = EFAULT;
		}
	} else {
		if (copyout(&ct->d_args, args, sizeof (door_arg_t)) != 0)
			error = EFAULT;
	}

out:
	ct->d_noresults = ct->d_overflow = 0;

	/* call destructor */
	if (destfn) {
		ASSERT(ct->d_kernel);
		(*destfn)(dp->door_data, destarg);
		ct->d_buf = NULL;
		ct->d_bufsize = 0;
	}

	if (dp)
		releasef(did);

	if (ct->d_buf) {
		ASSERT(!ct->d_kernel);
		kmem_free(ct->d_buf, ct->d_bufsize);
		ct->d_buf = NULL;
		ct->d_bufsize = 0;
	}
	ct->d_kernel = 0;

	if (ct->d_fpp) {
		kmem_free(ct->d_fpp, ct->d_fpp_size);
		ct->d_fpp = NULL;
		ct->d_fpp_size = 0;
	}

	if (error)
		return (set_errno(error));

	return (0);
}

/*
 * Return the results (if any) to the caller (if any) and wait for the
 * next invocation on a door.
 *
 * Invoked as:
 *
 *	error = door_return(caddr_t data_ptr, int data_size,
 *			door_desc_t *dp, int int did_size, caddr_t stk_base)
 */
int
door_return(caddr_t data_ptr, size_t data_size,
	door_desc_t *desc_ptr, uint_t desc_num, caddr_t sp)
{
	kthread_t	*caller;
	klwp_t		*lwp;
	int		error = 0;
	door_node_t	*dp;
	door_data_t	*ct;		/* curthread door_data */
	door_data_t	*caller_t;	/* caller door_data */
	int		scblock;

	if ((ct = curthread->t_door) == NULL) {
		ct = curthread->t_door = kmem_zalloc(sizeof (door_data_t),
						KM_SLEEP);
	}

	/*
	 * If thread was bound to a door that no longer exists, return
	 * an error.  This can happen if a thread is bound to a door
	 * before the process calls fork(); in the child, the door
	 * doesn't exist and door_fork() sets the DOOR_INVBOUND flag.
	 */
	if (ct->d_flag & DOOR_INVBOUND)
		return (set_errno(EINVAL));

	ct->d_sp = sp;		/* Save base of stack. */

	/* Make sure the caller hasn't gone away */
	mutex_enter(&door_knob);
	if ((caller = ct->d_caller) == NULL)
		goto out;
	if ((caller_t = caller->t_door) == NULL)
		goto out;

	caller_t->d_args.data_size = data_size;
	caller_t->d_args.desc_num = desc_num;
	/*
	 * Transfer results, if any, to the client
	 */
	if (data_size != 0 || desc_num != 0) {
		/*
		 * Prevent the client from exiting until we have finished
		 * moving results.
		 */
		caller_t->d_flag = DOOR_HOLD;
		mutex_exit(&door_knob);
		error = door_results(caller, data_ptr, data_size,
				desc_ptr, desc_num);
		mutex_enter(&door_knob);
		if (caller_t->d_flag & DOOR_WAITING)
			cv_signal(&caller_t->d_cv);
		caller_t->d_flag = 0;
		/*
		 * Pass EOVERFLOW errors back to the client
		 */
		if (error && error != EOVERFLOW) {
			mutex_exit(&door_knob);
			return (set_errno(error));
		}
	}
out:
	/* Put ourselves on the available server thread list */
	door_release_server(ct->d_pool, curthread);

	/*
	 * Make sure the caller is still waiting to be resumed
	 */
	if ((scblock = schedctl_check(curthread, SC_BLOCK)) != 0)
		(void) schedctl_block(NULL);
	if (caller) {
		disp_lock_t *tlp;

		thread_lock(caller);
		caller_t->d_error = error;	/* Return any errors */
		if (caller->t_state == TS_SLEEP &&
		    SOBJ_TYPE(caller->t_sobj_ops) == SOBJ_SHUTTLE) {
			cpu_t *cp = CPU;

			tlp = caller->t_lockp;
			/*
			 * Setting t_disp_queue prevents erroneous preemptions
			 * if this thread is still in execution on another
			 * processor
			 */
			caller->t_disp_queue = &cp->cpu_disp;
			THREAD_ONPROC(caller, cp);
			/*
			 * Make sure we end up on the right CPU if we
			 * are dealing with bound CPU's or processor
			 * partitions
			 */
			if (caller->t_bound_cpu != NULL ||
			    caller->t_cpupart != cp->cpu_part) {
				aston(caller);
				cp->cpu_runrun = 1;
			}
			disp_lock_exit_high(tlp);
			shuttle_resume(caller, &door_knob);
		} else {
			/* May have been setrun or in stop state */
			thread_unlock(caller);
			shuttle_swtch(&door_knob);
		}
	} else {
		shuttle_swtch(&door_knob);
	}
	if (scblock)
		schedctl_unblock();

	/*
	 * We've sprung to life. Determine if we are part of a door
	 * invocation, or just interrupted
	 */
	lwp = ttolwp(curthread);
	mutex_enter(&door_knob);
	if ((dp = ct->d_active) != NULL) {
		/*
		 * Normal door invocation. Return any error condition
		 * encountered while trying to pass args to the server
		 * thread.
		 */
		lwp->lwp_asleep = 0;
		/*
		 * Prevent the caller from leaving us while we
		 * are copying out the arguments from it's buffer.
		 * If caller is NULL, this is a scheduler activations
		 * upcall and we won't worry about the caller or
		 * arguments.
		 */
		if (ct->d_caller != NULL) {
			caller_t = ct->d_caller->t_door;
			caller_t->d_flag = DOOR_HOLD;
		} else
			caller_t = NULL;
		mutex_exit(&door_knob);
		error = door_server_dispatch(caller_t, dp);
		mutex_enter(&door_knob);
		/*
		 * If the caller was trying to exit while we dispatched
		 * this thread, signal it now.
		 */
		if (caller_t != NULL) {
			if (caller_t->d_flag & DOOR_WAITING)
				cv_signal(&caller_t->d_cv);
			caller_t->d_flag = 0;
		}
		if (error) {
			caller = ct->d_caller;
			if (caller)
				caller_t = caller->t_door;
			else
				caller_t = NULL;
			goto out;
		}
		mutex_exit(&door_knob);
		return (0);
	} else {
		/*
		 * We are not involved in a door_invocation.
		 * Check for /proc related activity...
		 */
		ct->d_caller = NULL;
		door_server_exit(curproc, curthread);
		mutex_exit(&door_knob);
		if (ISSIG(curthread, FORREAL) ||
		    lwp->lwp_sysabort || MUSTRETURN(curproc, curthread)) {
			lwp->lwp_asleep = 0;
			lwp->lwp_sysabort = 0;
			return (set_errno(EINTR));
		}
		/* Go back and wait for another request */
		lwp->lwp_asleep = 0;
		mutex_enter(&door_knob);
		caller = NULL;
		goto out;
	}
}

/*
 * Revoke any future invocations on this door
 */
int
door_revoke(int did)
{
	door_node_t	*d;
	int		error;

	if ((d = door_lookup(did, NULL)) == NULL)
		return (set_errno(EBADF));

	mutex_enter(&door_knob);
	if (d->door_target != curproc) {
		mutex_exit(&door_knob);
		releasef(did);
		return (set_errno(EPERM));
	}
	d->door_flags |= DOOR_REVOKED;
	cv_broadcast(&door_cv);
	mutex_exit(&door_knob);
	releasef(did);
	/* Invalidate the descriptor */
	if ((error = closeandsetf(did, NULL)) != 0)
		return (set_errno(error));
	return (0);
}

int
door_info(int did, struct door_info *d_info)
{
	door_node_t	*dp;
	door_info_t	di;
	door_data_t	*ct;
	file_t		*fp = NULL;

	if (did == DOOR_QUERY) {
		/* Get information on door current thread is bound to */
		if ((ct = curthread->t_door) == NULL ||
		    (dp = ct->d_pool) == NULL)
			/* Thread isn't bound to a door */
			return (set_errno(EBADF));
	} else if ((dp = door_lookup(did, &fp)) == NULL) {
		/* Not a door */
		return (set_errno(EBADF));
	}

	door_info_common(dp, &di, fp);

	if (did != DOOR_QUERY)
		releasef(did);

	if (copyout_info(&di, d_info, sizeof (struct door_info)))
		return (set_errno(EFAULT));
	return (0);
}

/*
 * Common code for getting information about a door either via the
 * door_info system call or the door_ki_info kernel call.
 */
void
door_info_common(door_node_t *dp, struct door_info *dip, file_t *fp)
{
	int unref_count;

	bzero(dip, sizeof (door_info_t));

	mutex_enter(&door_knob);
	if (dp->door_target == NULL)
		dip->di_target = -1;
	else
		dip->di_target = dp->door_target->p_pid;

	dip->di_attributes = dp->door_flags & DOOR_ATTR_MASK;
	if (dp->door_target == curproc)
		dip->di_attributes |= DOOR_LOCAL;
	dip->di_proc = (door_ptr_t)dp->door_pc;
	dip->di_data = (door_ptr_t)dp->door_data;
	dip->di_uniquifier = dp->door_index;
	/*
	 * If this door is in the middle of having an unreferenced
	 * notification delivered, don't count the VN_HOLD by
	 * door_deliver_unref in determining if it is unreferenced.
	 * This handles the case where door_info is called from the
	 * thread delivering the unref notification.
	 */
	if (dp->door_flags & DOOR_UNREF_ACTIVE)
		unref_count = 2;
	else
		unref_count = 1;
	mutex_exit(&door_knob);

	if (fp == NULL) {
		/*
		 * If this thread is bound to the door, then we can just
		 * check the vnode; a ref count of 1 (or 2 if this is
		 * handling an unref notification) means that the hold
		 * from the door_bind is the only reference to the door
		 * (no file descriptor refers to it).
		 */
		if (DTOV(dp)->v_count == unref_count)
			dip->di_attributes |= DOOR_IS_UNREF;
	} else {
		/*
		 * If we're working from a file descriptor or door handle
		 * we need to look at the file structure count.  We don't
		 * need to hold the vnode lock since this is just a snapshot.
		 */
		mutex_enter(&fp->f_tlock);
		if (fp->f_count == 1 && DTOV(dp)->v_count == unref_count)
			dip->di_attributes |= DOOR_IS_UNREF;
		mutex_exit(&fp->f_tlock);
	}
}

/*
 * Return credentials of the door caller (if any) for this invocation
 */
int
door_cred(struct door_cred *d_cred)
{
	door_cred_t	dc;
	kthread_t	*caller;
	door_data_t	*ct;
	struct cred	*cred;
	struct proc	*p;

	mutex_enter(&door_knob);
	if ((ct = curthread->t_door) == NULL ||
	    (caller = ct->d_caller) == NULL) {
		mutex_exit(&door_knob);
		return (set_errno(EINVAL));
	}
	/* Prevent caller from exiting while we examine the cred */
	ct->d_flag = DOOR_HOLD;
	mutex_exit(&door_knob);

	/* Get the credentials of the calling process */
	p = ttoproc(caller);
	mutex_enter(&p->p_crlock);
	cred = p->p_cred;
	dc.dc_euid = cred->cr_uid;
	dc.dc_egid = cred->cr_gid;
	dc.dc_ruid = cred->cr_ruid;
	dc.dc_rgid = cred->cr_rgid;
	mutex_exit(&p->p_crlock);

	mutex_enter(&door_knob);
	if (ct->d_flag & DOOR_WAITING)
		cv_signal(&ct->d_cv);
	ct->d_flag = 0;
	dc.dc_pid = p->p_pidp->pid_id;
	mutex_exit(&door_knob);

	if (copyout(&dc, d_cred, sizeof (door_cred_t)))
		return (set_errno(EFAULT));
	return (0);
}

/*
 * Bind the current lwp to the server thread pool associated with 'did'
 */
int
door_bind(int did)
{
	door_node_t	*dp;
	door_data_t *ct;

	if ((dp = door_lookup(did, NULL)) == NULL) {
		/* Not a door */
		return (set_errno(EBADF));
	}

	/*
	 * Can't bind to a non-private door, and can't bind to a door
	 * served by another process.
	 */
	if ((dp->door_flags & DOOR_PRIVATE) == 0 ||
	    dp->door_target != curproc) {
		releasef(did);
		return (set_errno(EINVAL));
	}

	if ((ct = curthread->t_door) == NULL) {
		ct = curthread->t_door =
		    kmem_zalloc(sizeof (door_data_t), KM_SLEEP);
	}
	if (ct->d_pool)
		VN_RELE(DTOV(ct->d_pool));	/* Release old hold */
	ct->d_pool = dp;
	ct->d_flag = ct->d_flag & ~DOOR_INVBOUND;
	VN_HOLD(DTOV(dp));
	releasef(did);

	return (0);
}

/*
 * Unbind the current lwp from it's server thread pool
 */
int
door_unbind(void)
{
	door_data_t *ct;

	if ((ct = curthread->t_door) == NULL)
		return (set_errno(EBADF));
	if (ct->d_flag & DOOR_INVBOUND) {
		ASSERT(ct->d_pool == NULL);
		ct->d_flag = ct->d_flag & ~DOOR_INVBOUND;
		return (0);
	}
	if (ct->d_pool == NULL)
		return (set_errno(EBADF));
	VN_RELE(DTOV(ct->d_pool));
	ct->d_pool = NULL;
	return (0);
}

/*
 * Create a descriptor for the associated file and fill in the
 * attributes associated with it.
 *
 * Return 0 for sucess, -1 otherwise;
 */
int
door_insert(struct file *fp, door_desc_t *dp)
{
	struct vnode *vp;
	int	fd;
	door_attr_t attributes = DOOR_DESCRIPTOR;

	if ((fd = ufalloc(0)) == -1)
		return (-1);
	setf(fd, fp);
	dp->d_data.d_desc.d_descriptor = fd;

	/* Fill in the attributes */
	if (VOP_REALVP(fp->f_vnode, &vp))
		vp = fp->f_vnode;
	if (vp && vp->v_type == VDOOR) {
		if (VTOD(vp)->door_target == curproc)
			attributes |= DOOR_LOCAL;
		attributes |= VTOD(vp)->door_flags & DOOR_ATTR_MASK;
		dp->d_data.d_desc.d_id = VTOD(vp)->door_index;
	}
	dp->d_attributes = attributes;
	return (0);
}

/*
 * Return an available thread for this server.
 */
static kthread_t *
door_get_server(door_node_t *dp, int dontblock)
{
	kthread_t *server_t;
	kthread_t **tptr;

	ASSERT(MUTEX_HELD(&door_knob));

	if (dp->door_flags & DOOR_PRIVATE) {
		tptr = &dp->door_servers;
	} else {
		tptr = &dp->door_target->p_server_threads;
	}

	while ((server_t = *tptr) == NULL) {
		if (dontblock)
			return (NULL);	/* Can't wait here */
		if (!cv_wait_sig_swap(&door_cv, &door_knob))
			return (NULL);	/* Got a signal */
		if (DOOR_INVALID(dp))
			return (NULL);	/* Target is invalid now */
	}
	ASSERT(server_t != NULL);

	thread_lock(server_t);
	if (server_t->t_state == TS_SLEEP &&
	    SOBJ_TYPE(server_t->t_sobj_ops) == SOBJ_SHUTTLE) {
		/*
		 * Mark the thread as ONPROC and take it off the list
		 * of available server threads. We are committed to
		 * resuming this thread now.
		 */
		disp_lock_t *tlp = server_t->t_lockp;
		cpu_t *cp = CPU;

		*tptr = server_t->t_door->d_servers;
		server_t->t_door->d_servers = NULL;
		/*
		 * Setting t_disp_queue prevents erroneous preemptions
		 * if this thread is still in execution on another processor
		 */
		server_t->t_disp_queue = &cp->cpu_disp;
		THREAD_ONPROC(server_t, cp);
		/*
		 * Make sure we end up on the right CPU if we
		 * are dealing with bound CPU's.  Same for
		 * processor partitions.
		 */
		if (server_t->t_bound_cpu != NULL ||
		    server_t->t_cpupart != cp->cpu_part) {
			aston(server_t);
			cp->cpu_runrun = 1;
		}
		disp_lock_exit(tlp);
		return (server_t);
	} else {
		/* Not kosher to resume this thread */
		thread_unlock(server_t);
		return (NULL);
	}
}

/*
 * Put a server thread back in the pool.
 */
static void
door_release_server(door_node_t *dp, kthread_t *t)
{
	door_data_t *ct = t->t_door;

	ASSERT(MUTEX_HELD(&door_knob));
	ct->d_error = 0;
	ct->d_active = NULL;
	ct->d_caller = NULL;
	if (dp && (dp->door_flags & DOOR_PRIVATE)) {
		ASSERT(dp->door_target == ttoproc(t));
		ct->d_servers = dp->door_servers;
		dp->door_servers = t;
	} else {
		proc_t	*p = ttoproc(t);

		ct->d_servers = p->p_server_threads;
		p->p_server_threads = t;
	}

	/* Wakeup any blocked door calls */
	cv_broadcast(&door_cv);
}

/*
 * Remove a server thread from the pool if present.
 */
static void
door_server_exit(proc_t *p, kthread_t *t)
{
	kthread_t **next;
	door_data_t *ct = t->t_door;

	ASSERT(MUTEX_HELD(&door_knob));
	if (ct->d_pool != NULL) {
		ASSERT(ct->d_pool->door_flags & DOOR_PRIVATE);
		next = &ct->d_pool->door_servers;
	} else {
		next = &p->p_server_threads;
	}

	while (*next != NULL) {
		if (*next == t) {
			*next = t->t_door->d_servers;
			return;
		}
		next = &((*next)->t_door->d_servers);
	}
}

/*
 * Lookup the door descriptor. Caller must call releasef when finished
 * with associated door.
 */
static door_node_t *
door_lookup(int did, file_t **fpp)
{
	vnode_t	*vp;
	file_t *fp;

	if ((fp = getf(did)) == NULL)
		return (NULL);
	/*
	 * Use the underlying vnode (we may be namefs mounted)
	 */
	if (VOP_REALVP(fp->f_vnode, &vp))
		vp = fp->f_vnode;

	if (vp == NULL || vp->v_type != VDOOR) {
		releasef(did);
		return (NULL);
	}

	if (fpp)
		*fpp = fp;

	return (VTOD(vp));
}

/*
 * The current thread is exiting, so clean up any pending
 * invocation details
 */
void
door_slam(void)
{
	door_node_t *dp;
	door_data_t *ct;
	/*
	 * If we are an active door server, notify our
	 * client that we are exiting and revoke our door.
	 */
	if ((ct = curthread->t_door) == NULL)
		return;
	mutex_enter(&door_knob);
	while (ct->d_flag & DOOR_HOLD) {
		ct->d_flag |= DOOR_WAITING;
		cv_wait(&ct->d_cv, &door_knob);
	}
	curthread->t_door = NULL;
	if ((dp = ct->d_active) != NULL) {
		kthread_id_t t = ct->d_caller;

		/* Revoke our door if the process is exiting */
		if (dp->door_target == curproc &&
		    (curproc->p_flag & EXITLWPS)) {
			door_list_delete(dp);
			dp->door_target = NULL;
			dp->door_flags |= DOOR_REVOKED;
			cv_broadcast(&door_cv);
		}

		if (t != NULL) {
			/*
			 * Let the caller know we are gone
			 */
			t->t_door->d_error = DOOR_EXIT;
			thread_lock(t);
			if (t->t_state == TS_SLEEP &&
			    SOBJ_TYPE(t->t_sobj_ops) == SOBJ_SHUTTLE)
				setrun_locked(t);
			thread_unlock(t);
		}
	}
	mutex_exit(&door_knob);
	if (ct->d_pool)
		VN_RELE(DTOV(ct->d_pool));	/* Implicit door_unbind */
	kmem_free(ct, sizeof (door_data_t));
}


/*
 * The process is exiting, and all doors it created need to be revoked.
 */
void
door_exit(void)
{
	door_node_t *dp;
	proc_t *p = ttoproc(curthread);

	ASSERT(p->p_lwpcnt == 1);
	/*
	 * Walk the list of active doors created by this process and
	 * revoke them all.
	 */
	mutex_enter(&door_knob);
	for (dp = p->p_door_list; dp != NULL; dp = dp->door_list) {
		ASSERT(dp->door_target == p);
		dp->door_target = NULL;
		dp->door_flags |= DOOR_REVOKED;
	}
	/* Clear the list */
	p->p_door_list = NULL;
	cv_broadcast(&door_cv);
	mutex_exit(&door_knob);
}


/*
 * The process is forking, and we need to flag threads that are bound
 * to a door in the child.  This will make the child threads return
 * an error to door_return unless they call door_unbind first.
 */
void
door_fork(kthread_t *parent, kthread_t *child)
{
	door_data_t *pt = parent->t_door;
	door_data_t *ct;

	if (pt && (pt->d_pool || (pt->d_flag & DOOR_INVBOUND))) {
		/* parent thread is bound to a door */
		ct = child->t_door =
		    kmem_zalloc(sizeof (door_data_t), KM_SLEEP);
		ct->d_flag = DOOR_INVBOUND;
	}
}

/*
 * Deliver queued unrefs to appropriate door server.
 */
static void
door_unref(void)
{
	door_node_t	*dp;
	static door_arg_t unref_args = { DOOR_UNREF_DATA, 0, 0, 0, 0, 0 };

	if (curthread->t_door == NULL) {
		curthread->t_door = kmem_zalloc(sizeof (door_data_t),
							KM_SLEEP);
	}
	for (;;) {
		mutex_enter(&door_knob);

		/* Grab a queued request */
		while ((dp = curproc->p_unref_list) == NULL) {
			if (!cv_wait_sig(&curproc->p_server_cv, &door_knob)) {
				curproc->p_unref_thread = 0;
				mutex_exit(&door_knob);
				mutex_enter(&curproc->p_lock);
				lwp_exit();
				/* NOTREACHED */
			}
		}
		curproc->p_unref_list = dp->door_ulist;
		dp->door_ulist = NULL;
		dp->door_flags |= DOOR_UNREF_ACTIVE;
		mutex_exit(&door_knob);

		(void) door_upcall(DTOV(dp), &unref_args);

		mutex_enter(&door_knob);
		ASSERT(dp->door_flags & DOOR_UNREF_ACTIVE);
		dp->door_flags &= ~DOOR_UNREF_ACTIVE;
		mutex_exit(&door_knob);
		VN_RELE(DTOV(dp));
	}
}


/*
 * Deliver queued unrefs to kernel door server.
 */
/* ARGSUSED */
static void
door_unref_kernel(caddr_t arg)
{
	door_node_t	*dp;
	static door_arg_t unref_args = { DOOR_UNREF_DATA, 0, 0, 0, 0, 0 };

	if (curthread->t_door == NULL) {
		curthread->t_door = kmem_zalloc(sizeof (door_data_t),
							KM_SLEEP);
	}
	for (;;) {
		mutex_enter(&door_knob);
		/* Grab a queued request */
		while ((dp = curproc->p_unref_list) == NULL)
			cv_wait(&curproc->p_server_cv, &door_knob);
		curproc->p_unref_list = dp->door_ulist;
		dp->door_ulist = NULL;
		dp->door_flags |= DOOR_UNREF_ACTIVE;
		mutex_exit(&door_knob);

		(*(dp->door_pc))(dp->door_data, &unref_args, NULL, NULL, NULL);

		mutex_enter(&door_knob);
		ASSERT(dp->door_flags & DOOR_UNREF_ACTIVE);
		dp->door_flags &= ~DOOR_UNREF_ACTIVE;
		mutex_exit(&door_knob);
		VN_RELE(DTOV(dp));
	}
}


/*
 * Queue an unref invocation for processing for the current process
 * The door may or may not be revoked at this point.
 */
void
door_deliver_unref(door_node_t *d)
{
	struct proc *server = d->door_target;

	ASSERT(MUTEX_HELD(&door_knob));
	ASSERT(d->door_active == 0);

	if (server == NULL)
		return;
	/*
	 * Create a lwp to deliver unref calls if one isn't already running.
	 *
	 * A separate thread is used to deliver unrefs since the current
	 * thread may be holding resources (e.g. locks) in user land that
	 * may be needed by the unref processing. This would cause a
	 * deadlock.
	 */
	if (!server->p_unref_thread) {
		if (server == &p0) {
			mutex_exit(&door_knob);
			if (thread_create(NULL, 0, door_unref_kernel, NULL, 0,
			    &p0, TS_RUN, 60) != NULL)
				server->p_unref_thread = 1;
			mutex_enter(&door_knob);
		} else {
			/*
			 * Uses the attributes of the first thread in the
			 * server.
			 */
			kthread_t	*first;
			int		pri;
			int		cid;
			k_sigset_t	mask;

			if ((first = server->p_tlist) == NULL)
				return;
			pri = first->t_pri;
			cid = first->t_cid;
			mask = first->t_hold;
			mutex_exit(&door_knob);

			if (lwp_create(door_unref, 0, 0, server, TS_RUN,
			    pri, mask, cid) != NULL)
				server->p_unref_thread = 1;
			mutex_enter(&door_knob);
		}
	}
	if (d->door_flags & DOOR_UNREF_MULTI) {
		/* multiple unrefs */
		d->door_flags &= ~DOOR_DELAY;
	} else {
		/* Only 1 unref per door */
		d->door_flags &= ~(DOOR_UNREF|DOOR_DELAY);
	}
	mutex_exit(&door_knob);

	/*
	 * Need to bump the vnode count before putting the door on the
	 * list so it doesn't get prematurely released by door_unref.
	 */
	VN_HOLD(DTOV(d));

	mutex_enter(&door_knob);
	/* is this door already on the unref list? */
	if (d->door_flags & DOOR_UNREF_MULTI) {
		door_node_t *dp;
		for (dp = server->p_unref_list; dp != NULL;
		    dp = dp->door_ulist) {
			if (d == dp) {
				/* already there, don't need to add another */
				mutex_exit(&door_knob);
				VN_RELE(DTOV(d));
				mutex_enter(&door_knob);
				return;
			}
		}
	}
	ASSERT(d->door_ulist == NULL);
	d->door_ulist = server->p_unref_list;
	server->p_unref_list = d;
	cv_broadcast(&server->p_server_cv);
}

/*
 * The callers buffer isn't big enough for all of the data/fd's. Allocate
 * space in the callers address space for the results and copy the data
 * there.
 */
static int
door_overflow(
	kthread_t	*caller,
	caddr_t 	data_ptr,	/* data location */
	size_t		data_size,	/* data size */
	door_desc_t	*desc_ptr,	/* descriptor location */
	uint_t		desc_num)	/* descriptor size */
{
	struct as *as = ttoproc(caller)->p_as;
	door_data_t *ct = caller->t_door;
	caddr_t	addr;			/* Resulting address in target */
	size_t	rlen;			/* Rounded len */
	size_t	len;
	uint_t	i;
	size_t	ds = desc_num * sizeof (door_desc_t);

	ASSERT((ct->d_flag & DOOR_HOLD) || ct->d_kernel);
	/*
	 * Allocate space for this stuff in the callers address space
	 */
	rlen = roundup(data_size + ds, PAGESIZE);
	as_rangelock(as);
	map_addr_proc(&addr, rlen, 0, 1, as->a_userlimit, ttoproc(caller));
	if (addr == NULL) {
		/* No virtual memory available */
		as_rangeunlock(as);
		return (EOVERFLOW);
	}
	if (as_map(as, addr, rlen, segvn_create, zfod_argsp) != 0) {
		as_rangeunlock(as);
		return (EOVERFLOW);
	}
	as_rangeunlock(as);

	if (ct->d_kernel)
		goto out;

	if (data_size != 0) {
		caddr_t	src = data_ptr;
		caddr_t saddr = addr;

		/* Copy any data */
		len = data_size;
		while (len != 0) {
			int	amount;
			int	error;

			amount = len > PAGESIZE ? PAGESIZE : len;
			if ((error = door_copy(as, src, saddr, amount)) != 0) {
				(void) as_unmap(as, addr, rlen);
				return (error);
			}
			saddr += amount;
			src += amount;
			len -= amount;
		}
	}
	/* Copy any fd's */
	if (desc_num != 0) {
		door_desc_t	*didpp, *start;
		struct file	**fpp;
		int		fpp_size;

		/* Check if we would overflow client */
		if (desc_num > P_CURLIMIT(ttoproc(caller), RLIMIT_NOFILE)) {
			(void) as_unmap(as, addr, rlen);
			return (EMFILE);
		}
		start = didpp = kmem_alloc(ds, KM_SLEEP);
		if (copyin_desc(desc_ptr, didpp, ds)) {
			kmem_free(start, ds);
			(void) as_unmap(as, addr, rlen);
			return (EFAULT);
		}

		fpp_size = desc_num * sizeof (struct file *);
		if (fpp_size > ct->d_fpp_size) {
			/* make more space */
			if (ct->d_fpp_size)
				kmem_free(ct->d_fpp, ct->d_fpp_size);
			ct->d_fpp_size = fpp_size;
			ct->d_fpp = kmem_alloc(ct->d_fpp_size, KM_SLEEP);
		}
		fpp = ct->d_fpp;

		for (i = 0; i < desc_num; i++) {
			struct file *fp;
			int fd = didpp->d_data.d_desc.d_descriptor;

			if (!(didpp->d_attributes & DOOR_DESCRIPTOR) ||
			    (fp = getf(fd)) == NULL) {
				/* close translated references */
				door_fp_close(ct->d_fpp, fpp - ct->d_fpp);
				/* close untranslated references */
				door_fd_rele(didpp, desc_num - i, 0);
				kmem_free(start, ds);
				(void) as_unmap(as, addr, rlen);
				return (EINVAL);
			}
			mutex_enter(&fp->f_tlock);
			fp->f_count++;
			mutex_exit(&fp->f_tlock);

			*fpp = fp;
			releasef(fd);

			if (didpp->d_attributes & DOOR_RELEASE) {
				/* release passed reference */
				(void) closeandsetf(fd, NULL);
			}

			fpp++; didpp++;
		}
		kmem_free(start, ds);
	}

out:
	ct->d_overflow = 1;
	ct->d_args.rbuf = addr;
	ct->d_args.rsize = rlen;
	return (0);
}

/*
 * Transfer arguments from the client to the server.
 */
static int
door_args(kthread_t *server)
{
	door_data_t *st = server->t_door;
	door_data_t *ct = curthread->t_door;
	uint_t	ndid;
	size_t	dsize;

	ASSERT(st->d_flag & DOOR_HOLD);

	ndid = ct->d_args.desc_num;
	if (ndid > door_max_desc)
		return (E2BIG);
	dsize = ndid * sizeof (door_desc_t);
	if (ct->d_args.data_size != 0) {
		if (ct->d_args.data_size <= door_max_arg) {
			/*
			 * Use a 2 copy method for small amounts of data
			 *
			 * Allocate a little more than we need for the
			 * args, in the hope that the results will fit
			 * without having to reallocate a buffer
			 */
			ASSERT(ct->d_buf == NULL);
			ct->d_bufsize = roundup(ct->d_args.data_size,
						DOOR_ROUND);
			ct->d_buf = kmem_alloc(ct->d_bufsize, KM_SLEEP);
			if (copyin(ct->d_args.data_ptr,
					ct->d_buf, ct->d_args.data_size) != 0) {
				kmem_free(ct->d_buf, ct->d_bufsize);
				ct->d_buf = NULL;
				ct->d_bufsize = 0;
				return (EFAULT);
			}
		} else {
			struct as 	*as;
			caddr_t		src;
			caddr_t		dest;
			size_t		len = ct->d_args.data_size;

			/*
			 * Use a 1 copy method
			 */
			as = ttoproc(server)->p_as;
			src = ct->d_args.data_ptr;
#if defined(_SYSCALL32_IMPL)
			if (lwp_getdatamodel(ttolwp(server)) == \
			    DATAMODEL_NATIVE) {
				dest = (caddr_t)door_arg_addr(st->d_sp,
				    len, dsize);
			} else {
				dest = (caddr_t)door_arg_addr32(st->d_sp,
				    len, dsize);
			}
#else
			dest = (caddr_t)door_arg_addr(st->d_sp, len, dsize);
#endif

			/* Copy data directly into server */
			while (len != 0) {
				uint_t	amount;
				uint_t	max;
				uint_t	off;
				int	error;

				off = (uint_t)dest & PAGEOFFSET;
				if (off)
					max = PAGESIZE - off;
				else
					max = PAGESIZE;
				amount = len > max ? max : len;
				error = door_copy(as, src, dest, amount);
				if (error != 0)
					return (error);
				dest += amount;
				src += amount;
				len -= amount;
			}
		}
	}
	/*
	 * Copyin the door args and translate them into files
	 */
	if (ndid != 0) {
		door_desc_t	*didpp;
		door_desc_t	*start;
		struct file	**fpp;

		start = didpp = kmem_alloc(dsize, KM_SLEEP);

		if (copyin_desc(ct->d_args.desc_ptr, didpp, dsize)) {
			kmem_free(start, dsize);
			return (EFAULT);
		}
		ct->d_fpp_size = ndid * sizeof (struct file *);
		ct->d_fpp = kmem_alloc(ct->d_fpp_size, KM_SLEEP);
		fpp = ct->d_fpp;
		while (ndid--) {
			struct file *fp;
			int fd = didpp->d_data.d_desc.d_descriptor;

			/* We only understand file descriptors as passed objs */
			if (!(didpp->d_attributes & DOOR_DESCRIPTOR) ||
			    (fp = getf(fd)) == NULL) {
				/* close translated references */
				door_fp_close(ct->d_fpp, fpp - ct->d_fpp);
				/* close untranslated references */
				door_fd_rele(didpp, ndid + 1, 0);
				kmem_free(start, dsize);
				kmem_free(ct->d_fpp, ct->d_fpp_size);
				ct->d_fpp = NULL;
				ct->d_fpp_size = 0;
				return (EINVAL);
			}
			/* Hold the fp */
			mutex_enter(&fp->f_tlock);
			fp->f_count++;
			mutex_exit(&fp->f_tlock);

			*fpp = fp;
			releasef(fd);

			if (didpp->d_attributes & DOOR_RELEASE) {
				/* release passed reference */
				(void) closeandsetf(fd, NULL);
			}

			fpp++; didpp++;
		}
		kmem_free(start, dsize);
	}
	return (0);
}

/*
 * Transfer arguments from a user client to a kernel server.  This copies in
 * descriptors and translates them into door handles.  It doesn't touch the
 * other data, letting the kernel server deal with that (to avoid needing
 * to copy the data twice).
 */
static int
door_translate_in(void)
{
	door_data_t *ct = curthread->t_door;
	uint_t	ndid;

	ndid = ct->d_args.desc_num;
	if (ndid > door_max_desc)
		return (E2BIG);
	/*
	 * Copyin the door args and translate them into door handles.
	 */
	if (ndid != 0) {
		door_desc_t	*didpp;
		door_desc_t	*start;
		size_t		dsize = ndid * sizeof (door_desc_t);
		struct file	*fp;

		start = didpp = kmem_alloc(dsize, KM_SLEEP);

		if (copyin_desc(ct->d_args.desc_ptr, didpp, dsize)) {
			kmem_free(start, dsize);
			return (EFAULT);
		}
		while (ndid--) {
			vnode_t	*vp;
			int fd = didpp->d_data.d_desc.d_descriptor;

			/*
			 * We only understand file descriptors as passed objs
			 */
			if ((didpp->d_attributes & DOOR_DESCRIPTOR) &&
			    (fp = getf(fd)) != NULL) {
				didpp->d_data.d_handle = FTODH(fp);
				/* Hold the door */
				door_ki_hold(didpp->d_data.d_handle);

				releasef(fd);

				if (didpp->d_attributes & DOOR_RELEASE) {
					/* release passed reference */
					(void) closeandsetf(fd, NULL);
				}

				if (VOP_REALVP(fp->f_vnode, &vp))
					vp = fp->f_vnode;

				/* Set attributes */
				didpp->d_attributes = DOOR_HANDLE |
				    (VTOD(vp)->door_flags & DOOR_ATTR_MASK);
			} else {
				/* close translated references */
				door_fd_close(start, didpp - start);
				/* close untranslated references */
				door_fd_rele(didpp, ndid + 1, 0);
				kmem_free(start, dsize);
				return (EINVAL);
			}
			didpp++;
		}
		ct->d_args.desc_ptr = start;
	}
	return (0);
}

/*
 * Translate door arguments from kernel to user.  This copies the passed
 * door handles.  It doesn't touch other data.  It is used by door_upcall,
 * and for data returned by a door_call to a kernel server.
 */
static int
door_translate_out(void)
{
	door_data_t *ct = curthread->t_door;
	uint_t	ndid;

	ndid = ct->d_args.desc_num;
	if (ndid > door_max_desc)
		return (E2BIG);
	/*
	 * Translate the door args into files
	 */
	if (ndid != 0) {
		door_desc_t	*didpp = ct->d_args.desc_ptr;
		struct file	**fpp;

		ct->d_fpp_size = ndid * sizeof (struct file *);
		fpp = ct->d_fpp = kmem_alloc(ct->d_fpp_size, KM_SLEEP);
		while (ndid--) {
			struct file *fp = NULL;
			int fd = -1;

			/*
			 * We understand file descriptors and door
			 * handles as passed objs.
			 */
			if (didpp->d_attributes & DOOR_DESCRIPTOR) {
				fd = didpp->d_data.d_desc.d_descriptor;
				fp = getf(fd);
			} else if (didpp->d_attributes & DOOR_HANDLE)
				fp = DHTOF(didpp->d_data.d_handle);
			if (fp != NULL) {
				/* Hold the fp */
				mutex_enter(&fp->f_tlock);
				fp->f_count++;
				mutex_exit(&fp->f_tlock);

				*fpp = fp;
				if (didpp->d_attributes & DOOR_DESCRIPTOR)
					releasef(fd);
				if (didpp->d_attributes & DOOR_RELEASE) {
					/* release passed reference */
					if (fd >= 0)
						(void) closeandsetf(fd, NULL);
					else
						(void) closef(fp);
				}
			} else {
				/* close translated references */
				door_fp_close(ct->d_fpp, fpp - ct->d_fpp);
				/* close untranslated references */
				door_fd_rele(didpp, ndid + 1, 1);
				kmem_free(ct->d_fpp, ct->d_fpp_size);
				ct->d_fpp = NULL;
				ct->d_fpp_size = 0;
				return (EINVAL);
			}
			fpp++; didpp++;
		}
	}
	return (0);
}

/*
 * Move the results from the server to the client
 */
static int
door_results(kthread_t *caller, caddr_t data_ptr, size_t data_size,
		door_desc_t *desc_ptr, uint_t desc_num)
{
	door_data_t	*ct = caller->t_door;
	size_t		dsize;
	size_t		rlen;
	size_t		result_size;

	ASSERT(ct->d_flag & DOOR_HOLD);

	if (ct->d_noresults)
		return (E2BIG);		/* No results expected */

	if (desc_num > door_max_desc)
		return (E2BIG);		/* Too many descriptors */

	dsize = desc_num * sizeof (door_desc_t);
	/*
	 * Check if the results are bigger than the clients buffer
	 */
	if (dsize)
		rlen = roundup(data_size, sizeof (door_desc_t));
	else
		rlen = data_size;
	if ((result_size = rlen + dsize) == 0)
		return (0);

	if (ct->d_upcall) {
		/*
		 * Handle upcalls
		 */
		if (ct->d_args.rbuf == NULL || ct->d_args.rsize < result_size) {
			/*
			 * If there's no return buffer or the buffer is too
			 * small, allocate a new one.  The old buffer (if it
			 * exists) will be freed by the upcall client.
			 */
			if (result_size > door_max_upcall_reply)
				return (E2BIG);
			ct->d_args.rsize = result_size;
			ct->d_args.rbuf = kmem_alloc(result_size, KM_SLEEP);
		}
		ct->d_args.data_ptr = ct->d_args.rbuf;
		if (data_size != 0 &&
		    copyin(data_ptr, ct->d_args.data_ptr, data_size) != 0)
			return (EFAULT);
	} else if (result_size > ct->d_args.rsize) {
		return (door_overflow(caller, data_ptr, data_size,
					desc_ptr, desc_num));
	} else if (data_size != 0) {
		if (data_size <= door_max_arg) {
			/*
			 * Use a 2 copy method for small amounts of data
			 */
			if (ct->d_buf == NULL) {
				ct->d_bufsize = data_size;
				ct->d_buf = kmem_alloc(ct->d_bufsize, KM_SLEEP);
			} else if (ct->d_bufsize < data_size) {
				kmem_free(ct->d_buf, ct->d_bufsize);
				ct->d_bufsize = data_size;
				ct->d_buf = kmem_alloc(ct->d_bufsize, KM_SLEEP);
			}
			if (copyin(data_ptr, ct->d_buf, data_size) != 0)
				return (EFAULT);
		} else {
			struct as *as = ttoproc(caller)->p_as;
			caddr_t	dest = ct->d_args.rbuf;
			caddr_t	src = data_ptr;
			size_t	len = data_size;

			/* Copy data directly into client */
			while (len != 0) {
				uint_t	amount;
				uint_t	max;
				uint_t	off;
				int	error;

				off = (uint_t)dest & PAGEOFFSET;
				if (off)
					max = PAGESIZE - off;
				else
					max = PAGESIZE;
				amount = len > max ? max : len;
				error = door_copy(as, src, dest, amount);
				if (error != 0)
					return (error);
				dest += amount;
				src += amount;
				len -= amount;
			}
		}
	}

	/*
	 * Copyin the returned door ids and translate them into door_node_t
	 */
	if (desc_num != 0) {
		door_desc_t *start;
		door_desc_t *didpp;
		struct file **fpp;
		size_t	fpp_size;
		uint_t	i;

		start = didpp = kmem_alloc(dsize, KM_SLEEP);
		if (copyin_desc(desc_ptr, didpp, dsize)) {
			kmem_free(start, dsize);
			return (EFAULT);
		}
		fpp_size = desc_num * sizeof (struct file *);
		if (fpp_size > ct->d_fpp_size) {
			/* make more space */
			if (ct->d_fpp_size)
				kmem_free(ct->d_fpp, ct->d_fpp_size);
			ct->d_fpp_size = fpp_size;
			ct->d_fpp = kmem_alloc(fpp_size, KM_SLEEP);
		}
		fpp = ct->d_fpp;

		for (i = 0; i < desc_num; i++) {
			struct file *fp;
			int fd = didpp->d_data.d_desc.d_descriptor;

			/* Only understand file descriptor results */
			if (!(didpp->d_attributes & DOOR_DESCRIPTOR) ||
			    (fp = getf(fd)) == NULL) {
				/* close translated references */
				door_fp_close(ct->d_fpp, fpp - ct->d_fpp);
				/* close untranslated references */
				door_fd_rele(didpp, desc_num - i, 0);
				kmem_free(start, dsize);
				return (EINVAL);
			}

			mutex_enter(&fp->f_tlock);
			fp->f_count++;
			mutex_exit(&fp->f_tlock);

			*fpp = fp;
			releasef(fd);

			if (didpp->d_attributes & DOOR_RELEASE) {
				/* release passed reference */
				(void) closeandsetf(fd, NULL);
			}

			fpp++; didpp++;
		}
		kmem_free(start, dsize);
	}
	return (0);
}

/*
 * Close all the descriptors.
 */
void
door_fd_close(door_desc_t *d, uint_t n)
{
	uint_t	i;

	for (i = 0; i < n; i++) {
		if (d->d_attributes & DOOR_DESCRIPTOR) {
			(void) closeandsetf(
			    d->d_data.d_desc.d_descriptor, NULL);
		} else if (d->d_attributes & DOOR_HANDLE) {
			door_ki_rele(d->d_data.d_handle);
		}
		d++;
	}
}

/*
 * Close descriptors that have the DOOR_RELEASE attribute set.
 */
void
door_fd_rele(door_desc_t *d, uint_t n, int kernel)
{
	uint_t	i;

	for (i = 0; i < n; i++) {
		if (d->d_attributes & DOOR_RELEASE) {
			if (d->d_attributes & DOOR_DESCRIPTOR) {
				(void) closeandsetf(
				    d->d_data.d_desc.d_descriptor, NULL);
			} else if (kernel && (d->d_attributes & DOOR_HANDLE)) {
				door_ki_rele(d->d_data.d_handle);
			}
		}
		d++;
	}
}

/*
 * Decrement ref count on all the files passed
 */
void
door_fp_close(struct file **fp, uint_t n)
{
	uint_t	i;

	for (i = 0; i < n; i++)
		(void) closef(fp[i]);
}

/*
 * Copy data from 'src' in current address space to 'dest' in 'as' for 'len'
 * bytes.
 *
 * Performs this using 1 mapin and 1 copy operation.
 *
 * We really should do more than 1 page at a time to improve
 * performance, but for now this is treated as an anomalous condition.
 */
static int
door_copy(struct as *as, caddr_t src, caddr_t dest, uint_t len)
{
	caddr_t	kaddr;
	caddr_t	rdest;
	uint_t 	off;
	page_t	**pplist;
	page_t	*pp = NULL;
	int	error = 0;

	ASSERT(len <= PAGESIZE);
	off = (uintptr_t)dest & PAGEOFFSET;	/* offset within the page */
	rdest = (caddr_t)((uintptr_t)dest & PAGEMASK);	/* Page boundary */
	ASSERT(off + len <= PAGESIZE);

	/*
	 * Lock down destination page.
	 */
	if (as_pagelock(as, &pplist, rdest, PAGESIZE, S_WRITE))
		return (E2BIG);
	/*
	 * Check if we have a shadow page list from as_pagelock. If not,
	 * we took the slow path and have to find our page struct the hard
	 * way.
	 */
	if (pplist == NULL) {
		pfn_t	pfnum;

		/* MMU mapping is already locked down */
		AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
		pfnum = hat_getpfnum(as->a_hat, rdest);
		AS_LOCK_EXIT(as, &as->a_lock);

		/*
		 * TODO: The pfn step should not be necessary - need
		 * a hat_getpp() function.
		 */
		if (pf_is_memory(pfnum)) {
			pp = page_numtopp_nolock(pfnum);
			ASSERT(pp == NULL || PAGE_LOCKED(pp));
		} else
			pp = NULL;
		if (pp == NULL) {
			as_pageunlock(as, pplist, rdest, PAGESIZE, S_WRITE);
			return (E2BIG);
		}
	} else {
		pp = *pplist;
	}
	/*
	 * Map destination page into kernel address
	 */
	kaddr = (caddr_t)ppmapin(pp, PROT_READ | PROT_WRITE, (caddr_t)-1);

	/*
	 * Copy from src to dest
	 */
	if (copyin(src, kaddr + off, len) != 0)
		error = EFAULT;
	/*
	 * Unmap destination page from kernel
	 */
	ppmapout(kaddr);
	/*
	 * Unlock destination page
	 */
	as_pageunlock(as, pplist, rdest, PAGESIZE, S_WRITE);
	return (error);
}

/*
 * General kernel upcall using doors
 *	Returns 0 on success, errno for failures.
 *	Caller must have a hold on the door based vnode, and on any
 *	references passed in desc_ptr.  The references are released
 *	in the event of an error, and passed without duplication
 *	otherwise.
 */
int
door_upcall(vnode_t *vp, door_arg_t *param)
{
	/* Locals */
	door_node_t	*dp;
	kthread_t	*server_thread;
	int		error = 0;
	klwp_id_t	lwp;
	door_data_t	*ct;		/* curthread door_data */
	door_data_t	*st;		/* server thread door_data */

	if (vp->v_type != VDOOR) {
		if (param->desc_num)
			door_fd_rele(param->desc_ptr, param->desc_num, 1);
		return (EINVAL);
	}

	lwp = ttolwp(curthread);
	if ((ct = curthread->t_door) == NULL) {
		ct = curthread->t_door = kmem_zalloc(sizeof (door_data_t),
							KM_SLEEP);
	}
	dp = VTOD(vp);	/* Convert to a door_node_t */

	mutex_enter(&door_knob);
	if (DOOR_INVALID(dp)) {
		mutex_exit(&door_knob);
		if (param->desc_num)
			door_fd_rele(param->desc_ptr, param->desc_num, 1);
		error = EBADF;
		goto out;
	}

	if (dp->door_target == &p0) {
		/* Can't do an upcall to a kernel server */
		mutex_exit(&door_knob);
		if (param->desc_num)
			door_fd_rele(param->desc_ptr, param->desc_num, 1);
		error = EINVAL;
		goto out;
	}

	/*
	 * Get a server thread from the target domain
	 */
	if ((server_thread = door_get_server(dp, 0)) == NULL) {
		mutex_exit(&door_knob);
		error = EAGAIN;
		if (param->desc_num)
			door_fd_rele(param->desc_ptr, param->desc_num, 1);
		goto out;
	}

	st = server_thread->t_door;
	ct->d_buf = param->data_ptr;
	ct->d_bufsize = param->data_size;
	ct->d_args = *param;	/* structure assignment */

	if (ct->d_args.desc_num) {
		/*
		 * Move data from client to server
		 */
		st->d_flag = DOOR_HOLD;
		mutex_exit(&door_knob);
		error = door_translate_out();
		mutex_enter(&door_knob);
		if (error) {
			/*
			 * We're not going to resume this thread after all
			 */
			st->d_flag = 0;
			door_release_server(dp, server_thread);
			shuttle_sleep(server_thread);
			mutex_exit(&door_knob);
			goto out;
		}
		if (st->d_flag & DOOR_WAITING)
			cv_signal(&st->d_cv);
		st->d_flag = 0;
	}

	ct->d_upcall = 1;
	if (param->rsize == 0)
		ct->d_noresults = 1;
	else
		ct->d_noresults = 0;

	dp->door_active++;

	ct->d_error = DOOR_WAIT;
	st->d_caller = curthread;
	st->d_active = dp;

	/* Feats don`t fail me now... */
	if (schedctl_check(curthread, SC_BLOCK)) {
		(void) schedctl_block(NULL);
		shuttle_resume(server_thread, &door_knob);
		schedctl_unblock();
	} else
		shuttle_resume(server_thread, &door_knob);

	mutex_enter(&door_knob);
shuttle_return:
	if ((error = ct->d_error) < 0) {
		/*
		 * Premature wakeup. Find out why (stop, fork, signal, exit ...)
		 */
		mutex_exit(&door_knob);		/* May block in ISSIG */
		if (lwp && (ISSIG(curthread, FORREAL) ||
		    lwp->lwp_sysabort || MUSTRETURN(curproc, curthread))) {
			/* Signal, fork, ... */
			lwp->lwp_sysabort = 0;
			mutex_enter(&door_knob);
			error = EINTR;
			/*
			 * If the server hasn't exited,
			 * let it know we are not interested anymore.
			 */
			if (ct->d_error != DOOR_EXIT &&
			    st->d_caller == curthread) {
				proc_t	*p = ttoproc(server_thread);

				st->d_active = NULL;
				st->d_caller = NULL;
				st->d_flag = DOOR_HOLD;
				mutex_exit(&door_knob);

				mutex_enter(&p->p_lock);
				sigtoproc(p, server_thread, SIGCANCEL);
				mutex_exit(&p->p_lock);

				mutex_enter(&door_knob);
				st->d_flag = 0;
				cv_signal(&st->d_cv);
			}
		} else {
			/*
			 * Return from stop(), server exit...
			 *
			 * Note that the server could have done a
			 * door_return while the client was in stop state
			 * (ISSIG), in which case the error condition
			 * is updated by the server.
			 */
			mutex_enter(&door_knob);
			if (ct->d_error == DOOR_WAIT) {
				/* Still waiting for a reply */
				if (schedctl_check(curthread, SC_BLOCK)) {
					(void) schedctl_block(NULL);
					shuttle_swtch(&door_knob);
					schedctl_unblock();
				} else
					shuttle_swtch(&door_knob);
				mutex_enter(&door_knob);
				if (lwp)
					lwp->lwp_asleep = 0;
				goto	shuttle_return;
			} else if (ct->d_error == DOOR_EXIT) {
				/* Server exit */
				error = EINTR;
			} else {
				/* Server did a door_return during ISSIG */
				error = ct->d_error;
			}
		}
		/*
		 * Can't exit if the client is currently copying
		 * results for me
		 */
		while (ct->d_flag & DOOR_HOLD) {
			ct->d_flag |= DOOR_WAITING;
			cv_wait(&ct->d_cv, &door_knob);
		}
	}
	if (lwp) {
		lwp->lwp_asleep = 0;		/* /proc */
		lwp->lwp_sysabort = 0;		/* /proc */
	}
	if (--dp->door_active == 0 && (dp->door_flags & DOOR_DELAY))
		door_deliver_unref(dp);
	mutex_exit(&door_knob);

	/*
	 * Translate returned doors (if any)
	 */
	if (error || ct->d_noresults)
		goto out;

	if (ct->d_args.desc_num) {
		struct file 	**fpp;
		door_desc_t	*didpp;
		vnode_t		*vp;
		uint_t		n = ct->d_args.desc_num;

		didpp = ct->d_args.desc_ptr = (door_desc_t *)(ct->d_args.rbuf +
		    roundup(ct->d_args.data_size, sizeof (door_desc_t)));
		fpp = ct->d_fpp;

		while (n--) {
			struct file *fp;

			fp = *fpp;
			if (VOP_REALVP(fp->f_vnode, &vp))
				vp = fp->f_vnode;

			didpp->d_attributes = DOOR_HANDLE |
			    (VTOD(vp)->door_flags & DOOR_ATTR_MASK);
			didpp->d_data.d_handle = FTODH(fp);

			fpp++; didpp++;
		}
	}

	/* on return data is in rbuf */
	*param = ct->d_args;		/* structure assignment */

out:
	if (ct->d_fpp) {
		kmem_free(ct->d_fpp, ct->d_fpp_size);
		ct->d_fpp = NULL;
		ct->d_fpp_size = 0;
	}

	ct->d_upcall = 0;
	ct->d_noresults = 0;
	ct->d_buf = NULL;
	ct->d_bufsize = 0;
	return (error);
}

kthread_t *
door_get_activation(vnode_t *vp)
{
	door_node_t	*dp = VTOD(vp);
	kthread_t	*server_thread;
	door_data_t	*st;		/* server thread door_data */

	ASSERT(vp->v_type == VDOOR);

	/*
	 * Can't block on door_knob because of the risk of deadlock,
	 * so just give up if we can't get it right away.
	 */
	if (mutex_tryenter(&door_knob) == 0)
		return (NULL);

	if (DOOR_INVALID(dp)) {
		mutex_exit(&door_knob);
		return (NULL);
	}
	/*
	 * Get a server thread from the target domain
	 */
	if ((server_thread = door_get_server(dp, 1)) == NULL) {
		mutex_exit(&door_knob);
		return (NULL);
	}

	st = server_thread->t_door;
	st->d_caller = NULL;
	st->d_active = dp;
	mutex_exit(&door_knob);
	return (server_thread);
}

void
door_release_activation(vnode_t *vp, kthread_t *server_thread)
{
	door_node_t	*dp;

	ASSERT(vp->v_type == VDOOR);
	dp = VTOD(vp);	/* Convert to a door_node_t */
	door_release_server(dp, server_thread);
}

/*
 * Add a door to the per-process list of active doors for which the
 * process is a server.
 */
static void
door_list_insert(door_node_t *dp)
{
	proc_t *p = dp->door_target;

	ASSERT(MUTEX_HELD(&door_knob));
	dp->door_list = p->p_door_list;
	p->p_door_list = dp;
}

/*
 * Remove a door from the per-process list of active doors.
 */
void
door_list_delete(door_node_t *dp)
{
	door_node_t **pp;

	ASSERT(MUTEX_HELD(&door_knob));
	/*
	 * Find the door in the list.  If the door belongs to another process,
	 * it's OK to use p_door_list since that process can't exit until all
	 * doors have been taken off the list (see door_exit).
	 */
	pp = &(dp->door_target->p_door_list);
	while (*pp != dp)
		pp = &((*pp)->door_list);

	/* found it, take it off the list */
	*pp = dp->door_list;
}


/*
 * External kernel interfaces for doors.  These functions are available
 * outside the doorfs module for use in creating and using doors from
 * within the kernel.
 */

/*
 * door_ki_upcall invokes a user-level door server from the kernel.
 */
int
door_ki_upcall(door_handle_t dh, door_arg_t *param)
{
	file_t *fp = DHTOF(dh);
	return (door_upcall(fp->f_vnode, param));
}

/*
 * Function call to create a "kernel" door server.  A kernel door
 * server provides a way for a user-level process to invoke a function
 * in the kernel through a door_call.  From the caller's point of
 * view, a kernel door server looks the same as a user-level one
 * (except the server pid is 0).  Unlike normal door calls, the
 * kernel door function is invoked via a normal function call in the
 * same thread and context as the caller.
 */
int
door_ki_create(void (*pc_cookie)(), void *data_cookie, uint_t attributes,
    door_handle_t *dhp)
{
	int err;
	file_t *fp;

	/* no DOOR_PRIVATE */
	if ((attributes & ~(DOOR_UNREF | DOOR_UNREF_MULTI)) ||
	    attributes == (DOOR_UNREF | DOOR_UNREF_MULTI))
		return (EINVAL);

	err = door_create_common(pc_cookie, data_cookie, attributes,
	    &p0, NULL, &fp);
	if (err == 0)
		*dhp = FTODH(fp);
	return (err);
}

void
door_ki_hold(door_handle_t dh)
{
	file_t *fp = DHTOF(dh);

	mutex_enter(&fp->f_tlock);
	fp->f_count++;
	mutex_exit(&fp->f_tlock);
}

void
door_ki_rele(door_handle_t dh)
{
	file_t *fp = DHTOF(dh);

	(void) closef(fp);
}

int
door_ki_open(char *pathname, door_handle_t *dhp)
{
	file_t *fp;
	vnode_t *vp, *realvp;
	int err;

	if ((err = lookupname(pathname, UIO_SYSSPACE, FOLLOW, NULL, &vp)) != 0)
		return (err);
	if (VOP_REALVP(vp, &realvp))
		realvp = vp;
	if (realvp->v_type != VDOOR) {
		VN_RELE(vp);
		return (EINVAL);
	}
	if ((err = falloc(realvp, FREAD | FWRITE, &fp, NULL)) != 0) {
		VN_RELE(vp);
		return (err);
	}
	/* falloc returns with f_tlock held on success */
	mutex_exit(&fp->f_tlock);
	/* release namefs node */
	if (vp != realvp) {
		VN_HOLD(realvp);
		VN_RELE(vp);
	}
	*dhp = FTODH(fp);
	return (0);
}

int
door_ki_info(door_handle_t dh, struct door_info *dip)
{
	file_t *fp = DHTOF(dh);
	vnode_t *vp;

	if (VOP_REALVP(fp->f_vnode, &vp))
		vp = fp->f_vnode;
	if (vp->v_type != VDOOR)
		return (EINVAL);
	door_info_common(VTOD(vp), dip, fp);
	return (0);
}
