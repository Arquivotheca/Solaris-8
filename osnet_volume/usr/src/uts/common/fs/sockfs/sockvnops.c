/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1989, 1996-1997 by Sun Microsystems, Inc.
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sockvnops.c	1.31	99/05/07 SMI"

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitmap.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/open.h>
#include <sys/swap.h>
#include <sys/sysmacros.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/suntpi.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/filio.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/session.h>
#include <sys/vmsystm.h>
#include <sys/vtrace.h>

#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <sys/un.h>

#define	_SUN_TPI_VERSION	2
#include <sys/tihdr.h>

#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg_dev.h>
#include <vm/seg_vn.h>

#include <fs/fs_subr.h>

#include <sys/esunddi.h>
#include <sys/autoconf.h>

static int sock_close(struct vnode *, int, int, offset_t, struct cred *);
static int sock_read(struct vnode *, struct uio *, int, struct cred *);
static int sock_write(struct vnode *, struct uio *, int, struct cred *);
static int sock_ioctl(struct vnode *, int, intptr_t, int, struct cred *, int *);
static int sock_setfl(vnode_t *vp, int, int, cred_t *);
static int sock_getattr(struct vnode *, struct vattr *, int, struct cred *);
static int sock_setattr(struct vnode *, struct vattr *, int, struct cred *);
static int sock_access(struct vnode *, int, int, struct cred *);
static int sock_fsync(struct vnode *, int, struct cred *);
static void sock_inactive(struct vnode *, struct cred *);
static int sock_fid(struct vnode *, struct fid *);
static int sock_seek(struct vnode *, offset_t, offset_t *);
static int sock_poll(struct vnode *, short, int, short *, struct pollhead **);

static const char sockmod_name[] = "sockmod";

static struct vnodeops sock_vnodeops = {
	sock_open,
	sock_close,
	sock_read,
	sock_write,
	sock_ioctl,
	sock_setfl,
	sock_getattr,
	sock_setattr,
	sock_access,
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
	sock_fsync,
	sock_inactive,
	sock_fid,
	fs_rwlock,
	fs_rwunlock,
	sock_seek,
	fs_cmp,
	fs_frlock,
	fs_nosys,	/* space */
	fs_nosys,	/* realvp */
	fs_nosys,	/* getpage */
	fs_nosys,	/* putpage */
	fs_nosys_map,	/* mmap */
	fs_nosys_addmap,	/* addmap */
	fs_nosys,	/* delmap */
	sock_poll,
	fs_nosys,	/* dump */
	fs_pathconf,
	fs_nosys,	/* pageio */
	fs_nosys,	/* dumpctl */
	fs_nodispose,
	fs_nosys,	/* setsecattr */
	fs_fab_acl,	/* getsecattr */
	fs_shrlock	/* shrlock */
};

/*
 * Return address of sock_vnodeops
 */
struct vnodeops *
sock_getvnodeops(void)
{
	return (&sock_vnodeops);
}

/*
 * Open routine used by socket() call. Note that vn_open checks for
 * VSOCK and fails the open (and VOP_OPEN is fs_nosys). The VSOCK check is
 * needed since VSOCK type vnodes exist in various underlying filesystems as
 * a result of an AF_UNIX bind to a pathname.
 *
 * Sockets assume that the driver will clone (either itself
 * or by using the clone driver) i.e. a socket() call will always
 * result in a new vnode being created. This routine single-threads
 * open/closes for a given vnode which is probably not needed.
 */
int
sock_open(struct vnode **vpp, int flag, struct cred *cr)
{
	major_t maj;
	dev_t dev;
	dev_t newdev;
	struct vnode *vp = *vpp;
	struct sonode *so;
	int error = 0;
	struct dev_ops *ops = NULL;
	struct stdata *stp;
	int sflag;

	dprint(1, ("sock_open()\n"));
	flag &= ~FCREAT;		/* paranoia */

	so = VTOSO(vp);

	mutex_enter(&so->so_lock);
	so->so_count++;			/* one more open reference */
	ASSERT(so->so_count != 0);	/* wraparound */
	mutex_exit(&so->so_lock);

	dev = vp->v_rdev;
	newdev = dev;

	/*
	 * Autoload, install and hold the driver.
	 */
	if (((maj = getmajor(dev)) >= devcnt) ||
	    ((ops = ddi_hold_installed_driver(maj)) == NULL) ||
	    (ops->devo_cb_ops == NULL)) {
		error = ENXIO;
		goto done;
	}

	ASSERT(vp->v_type == VSOCK);
	if (!STREAMSTAB(maj)) {
		error = ENXIO;
		goto done;
	}

	mutex_enter(&so->so_lock);
	(void) so_lock_single(so, SOLOCKED, 0);
	mutex_exit(&so->so_lock);
	if (so->so_flag & SOCLONE) {
		dprintso(so, 1, ("sock_open: clone\n"));
		sflag = CLONEOPEN;
	} else
		sflag = 0;

	error = stropen(vp, &newdev, flag, sflag, cr);

	stp = vp->v_stream;
	if (error == 0) {
		/*
		 * Minor number might have changed.
		 * The major number never changes since the clonemaj is
		 * detected in makesockvp.
		 */
		ASSERT(getmajor(newdev) == getmajor(dev));
		mutex_enter(&so->so_lock);
		so->so_dev = newdev;
		vp->v_rdev = newdev;
		so_unlock_single(so, SOLOCKED);
		mutex_exit(&so->so_lock);

		if (stp->sd_flag & STRISTTY) {
			/*
			 * this is a post SVR4 tty driver - a socket can not
			 * be a controlling terminal. Fail the open.
			 */
			(void) sock_close(vp, flag, 1, (offset_t)0, cr);
			return (ENOTTY);	/* XXX */
		}

		ASSERT(stp->sd_wrq != NULL);
		so->so_provinfo = tpi_findprov(stp->sd_wrq);
	} else {
		/*
		 * While the same socket can not be reopened (unlike specfs)
		 * the stream head sets STREOPENFAIL when the autopush fails.
		 */
		if ((stp != NULL) &&
		    (stp->sd_flag & STREOPENFAIL)) {
			/*
			 * Open failed part way through.
			 */
			mutex_enter(&stp->sd_lock);
			stp->sd_flag &= ~STREOPENFAIL;
			mutex_exit(&stp->sd_lock);

			mutex_enter(&so->so_lock);
			so_unlock_single(so, SOLOCKED);
			mutex_exit(&so->so_lock);
			(void) sock_close(vp, flag, 1,
			    (offset_t)0, cr);
			return (error);
			/*NOTREACHED*/
		}
		ASSERT(stp == NULL);
		mutex_enter(&so->so_lock);
		so_unlock_single(so, SOLOCKED);
		mutex_exit(&so->so_lock);
	}
done:
	if (error != 0) {
		mutex_enter(&so->so_lock);
		ASSERT(so->so_count > 0);
		so->so_count--;		/* one less open reference */
		mutex_exit(&so->so_lock);
		if (ops)
			ddi_rele_driver(maj);
	}
	TRACE_4(TR_FAC_SOCKFS, TR_SOCKFS_OPEN,
		"sockfs open:maj %d vp %p so %p error %d", maj,
		*vpp, so, error);
	return (error);
}

/*ARGSUSED2*/
static int
sock_close(
	struct vnode	*vp,
	int		flag,
	int		count,
	offset_t	offset,
	struct cred	*cr)
{
	struct sonode *so;
	dev_t dev;
	int error = 0;

	so = VTOSO(vp);

	dprintso(so, 1, ("sock_close(%p, %x, %d) %s\n",
			vp, flag, count, pr_state(so->so_state, so->so_mode)));

	cleanlocks(vp, ttoproc(curthread)->p_pid, 0);
	cleanshares(vp, ttoproc(curthread)->p_pid);
	if (vp->v_stream)
		strclean(vp);
	if (count > 1)
		return (0);

	dev = so->so_dev;

	ASSERT(vp->v_type == VSOCK);
	ASSERT(STREAMSTAB(getmajor(dev)));

	mutex_enter(&so->so_lock);
	(void) so_lock_single(so, SOLOCKED, 0);
	ASSERT(so->so_count > 0);
	so->so_count--;			/* one fewer open reference */

	/*
	 * Only call the close routine when the last open reference through
	 * any [s, v]node goes away.
	 */
	if (so->so_count == 0 && vp->v_stream != NULL) {
		vnode_t *ux_vp;

		if (so->so_family == AF_UNIX) {
			/* Could avoid this when CANTSENDMORE for !dgram */
			so_unix_close(so);
		}

		mutex_exit(&so->so_lock);
		/*
		 * Disassemble the linkage from the AF_UNIX underlying file
		 * system vnode to this socket (by atomically clearing
		 * v_stream in vn_rele_stream) before strclose clears sd_vnode
		 * and frees the stream head.
		 */
		if ((ux_vp = so->so_ux_bound_vp) != NULL) {
			ASSERT(ux_vp->v_stream);
			so->so_ux_bound_vp = NULL;
			vn_rele_stream(ux_vp);
		}
		error = strclose(vp, flag, cr);
		vp->v_stream = NULL;
		mutex_enter(&so->so_lock);
	}

	so_unlock_single(so, SOLOCKED);
	mutex_exit(&so->so_lock);

	/*
	 * Decrement the device driver's reference count for every close.
	 */
	ddi_rele_driver(getmajor(dev));
	return (error);
}

/*ARGSUSED2*/
static int
sock_read(
	struct vnode	*vp,
	struct uio	*uiop,
	int		ioflag,
	struct cred	*cr)
{
	struct sonode *so = VTOSO(vp);
	struct nmsghdr lmsg;

	dprintso(so, 1, ("sock_read(%p) %s\n",
			so, pr_state(so->so_state, so->so_mode)));

	ASSERT(vp->v_type == VSOCK);
	so_update_attrs(so, SOACC);
	if (so->so_version == SOV_STREAM) {
		/* The imaginary "sockmod" has been popped - act as a stream */
		return (strread(vp, uiop, cr));
	}
	lmsg.msg_namelen = 0;
	lmsg.msg_controllen = 0;
	lmsg.msg_flags = 0;
	return (sorecvmsg(so, &lmsg, uiop));
}

/* ARGSUSED2 */
static int
sock_write(
	struct vnode		*vp,
	struct uio		*uiop,
	int			ioflag,
	struct cred		*cr)
{
	struct sonode *so = VTOSO(vp);
	int so_state;
	int so_mode;
	int error;

	dprintso(so, 1, ("sock_write(%p) %s\n",
			so, pr_state(so->so_state, so->so_mode)));

	ASSERT(vp->v_type == VSOCK);
	if (so->so_version == SOV_STREAM) {
		/* The imaginary "sockmod" has been popped - act as a stream */
		so_update_attrs(so, SOMOD);
		return (strwrite(vp, uiop, cr));
	}
	/* State checks */
	so_state = so->so_state;
	so_mode = so->so_mode;
	if (so_state & SS_CANTSENDMORE) {
		psignal(ttoproc(curthread), SIGPIPE);
		return (EPIPE);
	}

	if (so->so_error != 0) {
		mutex_enter(&so->so_lock);
		error = sogeterr(so, 0);
		if (error != 0) {
			mutex_exit(&so->so_lock);
			return (error);
		}
		mutex_exit(&so->so_lock);
	}

	if ((so_state & (SS_ISCONNECTED|SS_ISBOUND)) !=
	    (SS_ISCONNECTED|SS_ISBOUND)) {
		if (so_mode & SM_CONNREQUIRED)
			return (ENOTCONN);
		else
			return (EDESTADDRREQ);
	}

	if (!(so_mode & SM_CONNREQUIRED)) {
		/*
		 * Note that this code does not prevent so_faddr_sa
		 * from changing while it is being used. Thus
		 * if an "unconnect"+connect occurs concurrently with
		 * this write the datagram might be delivered to a
		 * garbaled address.
		 */
		so_update_attrs(so, SOMOD);
		return (sosend_dgram(so, so->so_faddr_sa,
		    (t_uscalar_t)so->so_faddr_len, uiop, 0));
	}
	so_update_attrs(so, SOMOD);

	if (so_mode & SM_BYTESTREAM) {
		/* Send M_DATA messages */
		return (strwrite(vp, uiop, cr));
	} else {
		/* Send T_DATA_REQ messages without MORE_flag set */
		return (sosend_svc(so, uiop, T_DATA_REQ, 0, 0));
	}
}

static int
so_copyin(const void *from, void *to, size_t size, int fromkernel)
{
	if (fromkernel) {
		bcopy(from, to, size);
		return (0);
	}
	return (xcopyin(from, to, size));
}

static int
so_copyout(const void *from, void *to, size_t size, int tokernel)
{
	if (tokernel) {
		bcopy(from, to, size);
		return (0);
	}
	return (xcopyout(from, to, size));
}

static int
sock_ioctl(struct vnode *vp, int cmd, intptr_t arg, int mode, struct cred *cr,
    int32_t *rvalp)
{
	struct sonode *so = VTOSO(vp);
	int error = 0;

	ASSERT(vp->v_type == VSOCK);
	dprintso(so, 0, ("sock_ioctl: cmd 0x%x, arg 0x%lx, state %s\n",
		cmd, arg, pr_state(so->so_state, so->so_mode)));

	if (so->so_version == SOV_STREAM) {
		/*
		 * The imaginary "sockmod" has been popped - act as a stream.
		 * If this is a push of "sockmod" then change back to being
		 * a socket.
		 */
		if (cmd == I_PUSH) {
			char mname[FMNAMESZ + 1];

			error = ((mode & (int)FKIOCTL) ? copystr : copyinstr)(
			    (void *)arg, mname, FMNAMESZ + 1, NULL);

			/* Single-thread to avoid concurrent I_PUSH */
			mutex_enter(&so->so_lock);
			(void) so_lock_single(so, SOLOCKED, 0);
			if (error == 0 && strcmp(mname, sockmod_name) == 0) {
				dprintso(so, 0,
				    ("sock_ioctl: going to socket version\n"));
				so_stream2sock(so);
				so_unlock_single(so, SOLOCKED);
				mutex_exit(&so->so_lock);
				return (0);
			}
			so_unlock_single(so, SOLOCKED);
			mutex_exit(&so->so_lock);
			error = 0;
		}
		return (strioctl(vp, cmd, arg, mode, U_TO_K, cr, rvalp));
	}
	/* handle socket specific ioctls */
	switch (cmd) {
	case FIONBIO: {
		int32_t value;

		if (so_copyin((void *)arg, &value, sizeof (int32_t),
		    (mode & (int)FKIOCTL)))
			return (EFAULT);

		mutex_enter(&so->so_lock);
		if (value) {
			so->so_state |= SS_NDELAY;
		} else {
			so->so_state &= ~SS_NDELAY;
		}
		mutex_exit(&so->so_lock);
		return (0);
	}

	case FIOASYNC: {
		struct strsigset ss;
		int32_t value;

		if (so_copyin((void *)arg, &value, sizeof (int32_t),
		    (mode & (int)FKIOCTL)))
			return (EFAULT);

		mutex_enter(&so->so_lock);
		if (value) {
			if (so->so_state & SS_ASYNC) {
				mutex_exit(&so->so_lock);
				return (0);
			}

			/* Turn on SIGIO */
			ss.ss_events = S_RDNORM | S_OUTPUT;
		} else {
			if ((so->so_state & SS_ASYNC) == 0) {
				mutex_exit(&so->so_lock);
				return (0);
			}

			/* Turn off SIGIO */
			ss.ss_events = 0;
		}
		if (so->so_pgrp != 0) {
			mutex_exit(&so->so_lock);

			ss.ss_pid = so->so_pgrp;
			ss.ss_events |= S_RDBAND | S_BANDURG;
			error = strioctl(vp, I_ESETSIG, (intptr_t)&ss, mode,
					K_TO_K, cr, rvalp);
			if (error)
				return (error);
			mutex_enter(&so->so_lock);
		}
		if (value)
			so->so_state |= SS_ASYNC;
		else
			so->so_state &= ~SS_ASYNC;
		mutex_exit(&so->so_lock);
		return (0);
	}

	case SIOCSPGRP:
	case FIOSETOWN: {
		struct strsigset ss;

		if (so_copyin((void *)arg, &ss.ss_pid, sizeof (pid_t),
		    (mode & (int)FKIOCTL)))
			return (EFAULT);

		mutex_enter(&so->so_lock);
		dprintso(so, 1, ("setown: new %d old %d\n",
				ss.ss_pid, so->so_pgrp));
		if (ss.ss_pid == so->so_pgrp) {
			/* No change */
			mutex_exit(&so->so_lock);
			return (0);
		}

		if (ss.ss_pid != 0) {
			/*
			 * Change socket process (group).
			 *
			 * strioctl will perform permission check and
			 * also keep a PID_HOLD to prevent the pid
			 * from being reused.
			 */
			ss.ss_events = S_RDBAND | S_BANDURG;
			if (so->so_state & SS_ASYNC)
				ss.ss_events |= S_RDNORM | S_OUTPUT;

			mutex_exit(&so->so_lock);

			dprintso(so, 1, ("setown: adding %d ev 0x%x\n",
				ss.ss_pid, ss.ss_events));
			error = strioctl(vp, I_ESETSIG, (intptr_t)&ss, mode,
					K_TO_K, cr, rvalp);
			if (error) {
				eprintsoline(so, error);
				return (error);
			}
			/* Remove the previously registered process/group */
			if (so->so_pgrp != 0) {
				struct strsigset oss;

				oss.ss_events = 0;
				oss.ss_pid = so->so_pgrp;

				dprintso(so, 1,
					("setown: removing %d ev 0x%x\n",
					oss.ss_pid, oss.ss_events));
				error = strioctl(vp, I_ESETSIG, (intptr_t)&oss,
						mode, K_TO_K, cr, rvalp);
				if (error) {
					eprintsoline(so, error);
					error = 0;
				}
			}
			mutex_enter(&so->so_lock);
			so->so_pgrp = ss.ss_pid;
			mutex_exit(&so->so_lock);
		} else {
			/* Remove registration */
			ss.ss_events = 0;
			ss.ss_pid = so->so_pgrp;
			ASSERT(ss.ss_pid != 0);
			mutex_exit(&so->so_lock);
			dprintso(so, 1, ("setown: removing %d ev 0x%x\n",
				ss.ss_pid, ss.ss_events));
			error = strioctl(vp, I_ESETSIG, (intptr_t)&ss, mode,
					K_TO_K, cr, rvalp);
			if (error) {
				eprintsoline(so, error);
				return (error);
			}
			mutex_enter(&so->so_lock);
			so->so_pgrp = 0;
			mutex_exit(&so->so_lock);
		}
		return (0);
	}
	case SIOCGPGRP:
	case FIOGETOWN:
		if (so_copyout(&so->so_pgrp, (void *)arg,
		    sizeof (pid_t), (mode & (int)FKIOCTL)))
			return (EFAULT);
		return (0);

	case SIOCATMARK: {
		int retval;
		uint_t so_state;

		/*
		 * strwaitmark has a finite timeout after which it
		 * returns -1 if the mark state is undetermined.
		 * In order to avoid any race between the mark state
		 * in sockfs and the mark state in the stream head this
		 * routine loops until the mark state can be determined
		 * (or the urgent data indication has been removed by some
		 * other thread).
		 */
		do {
			mutex_enter(&so->so_lock);
			so_state = so->so_state;
			mutex_exit(&so->so_lock);
			if (so_state & SS_RCVATMARK) {
				retval = 1;
			} else if (!(so_state & SS_OOBPEND)) {
				/*
				 * No SIGURG has been generated -- there is no
				 * pending or present urgent data. Thus can't
				 * possibly be at the mark.
				 */
				retval = 0;
			} else {
				/*
				 * Have the stream head wait until there is
				 * either some messages on the read queue, or
				 * STRATMARK or STRNOTATMARK gets set. The
				 * STRNOTATMARK flag is used so that the
				 * transport can send up a MSGNOTMARKNEXT
				 * M_DATA to indicate that it is not
				 * at the mark and additional data is not about
				 * to be send upstream.
				 *
				 * If the mark state is undetermined this will
				 * return -1 and we will loop rechecking the
				 * socket state.
				 */
				retval = strwaitmark(vp);
			}
		} while (retval == -1);

		if (so_copyout(&retval, (void *)arg, sizeof (int),
		    (mode & (int)FKIOCTL)))
			return (EFAULT);
		return (0);
	}
	case I_LIST: {
		struct str_list kstrlist;
		STRUCT_DECL(str_list, u_strlist);
		struct str_mlist *freebuf;
		ssize_t freebufsize;
		struct str_mlist *mlist_ptr;
		int	num_modules, space;

		int i;

		STRUCT_INIT(u_strlist, mode);
		if (so->so_version == SOV_SOCKBSD)
			return (EOPNOTSUPP);

		if (arg == NULL) {
			error = strioctl(vp, cmd, arg, mode, U_TO_K, cr,
					rvalp);
			if (error)
				return (error);
			/* Add one for sockmod */
			(*rvalp)++;
			return (0);
		}

		/*
		 * Copyin structure and allocate local list of str_mlist.
		 * Then ask the stream head for the list of modules.
		 * Finally copyout list of modules and insert "sockmod"
		 * in the appropriate place.
		 */
		error = so_copyin((void *)arg, STRUCT_BUF(u_strlist),
		    STRUCT_SIZE(u_strlist), (mode & (int)FKIOCTL));

		if (error)
			return (error);

		num_modules = STRUCT_FGET(u_strlist, sl_nmods);

		if (num_modules <= 0)
			return (EINVAL);

		if (num_modules == 1)
			return (ENOSPC);

		kstrlist.sl_nmods = num_modules - 1;
		freebufsize = kstrlist.sl_nmods * sizeof (struct str_mlist);
		kstrlist.sl_modlist = freebuf =
		    kmem_zalloc(freebufsize, KM_SLEEP);

		dprintso(so, 0, ("I_LIST: sl_nmods %d, freebufsize %ld\n",
				num_modules, freebufsize));

		error = strioctl(vp, cmd, (intptr_t)&kstrlist, mode,
		    K_TO_K, cr, rvalp);
		if (error)
			goto done;

		space = num_modules;
		num_modules = 0;

		dprintso(so, 0, ("I_LIST: pushcnt %d, sl_nmods %d\n",
				so->so_pushcnt, kstrlist.sl_nmods));
		/* Copyout so_pushcnt elements from kstrlist */
		for (i = 0; i < so->so_pushcnt; i++) {
			mlist_ptr = STRUCT_FGETP(u_strlist, sl_modlist);
			error = so_copyout(kstrlist.sl_modlist->l_name,
			    mlist_ptr, strlen(kstrlist.sl_modlist->l_name) + 1,
			    (mode & (int)FKIOCTL));
			if (error)
				goto done;
			num_modules++;
			kstrlist.sl_modlist++;
			mlist_ptr = (struct str_mlist *)((uintptr_t)mlist_ptr +
			    sizeof (struct str_mlist));
			STRUCT_FSETP(u_strlist, sl_modlist, mlist_ptr);
			kstrlist.sl_nmods--;
			space--;
		}
		ASSERT(space > 0);
		/* Copyout "sockmod" */
		mlist_ptr = STRUCT_FGETP(u_strlist, sl_modlist);
		error = so_copyout(sockmod_name, mlist_ptr,
		    sizeof (sockmod_name), (mode & (int)FKIOCTL));
		if (error)
			goto done;
		num_modules++;
		mlist_ptr = (struct str_mlist *)((uintptr_t)mlist_ptr +
		    sizeof (struct str_mlist));
		STRUCT_FSETP(u_strlist, sl_modlist, mlist_ptr);
		space--;

		/* Copyout remaining modules */
		while (kstrlist.sl_nmods > 0) {
			mlist_ptr = STRUCT_FGETP(u_strlist, sl_modlist);
			error = so_copyout(kstrlist.sl_modlist->l_name,
			    mlist_ptr, strlen(kstrlist.sl_modlist->l_name) + 1,
			    (mode & (int)FKIOCTL));
			if (error)
				goto done;
			num_modules++;
			kstrlist.sl_modlist++;
			mlist_ptr = (struct str_mlist *)((uintptr_t)mlist_ptr +
			    sizeof (struct str_mlist));
			STRUCT_FSETP(u_strlist, sl_modlist,
			    mlist_ptr);
			kstrlist.sl_nmods--;
			space--;
		}
		ASSERT(space >= 0);
		error = so_copyout(&num_modules, (void *)arg,
		    sizeof (int32_t), (mode & (int)FKIOCTL));
		if (error)
			goto done;
		*rvalp = num_modules;
	done:
		kmem_free(freebuf, freebufsize);
		return (error);
	}
	case I_LOOK:
		if (so->so_version == SOV_SOCKBSD)
			return (EOPNOTSUPP);

		if (so->so_pushcnt == 0) {
			/* return sockmod */
			error = so_copyout(sockmod_name, (void *)arg,
			    sizeof (sockmod_name), (mode & (int)FKIOCTL));
		} else {
			error = strioctl(vp, cmd, arg, mode, U_TO_K, cr,
			    rvalp);
		}
		return (error);

	case I_FIND:
		if (so->so_version == SOV_SOCKBSD)
			return (EOPNOTSUPP);

		error = strioctl(vp, cmd, arg, mode, U_TO_K, cr, rvalp);
		if (error && error != EINVAL)
			return (error);

		/* if not found and string was sockmod return 1 */
		if (*rvalp == 0 || error == EINVAL) {
			char mname[FMNAMESZ + 1];

			error = ((mode & (int)FKIOCTL) ? copystr : copyinstr)(
			    (void *)arg, mname, FMNAMESZ + 1, NULL);
			if (error == ENAMETOOLONG)
				error = EINVAL;

			if (error == 0 && strcmp(mname, sockmod_name) == 0) {
				*rvalp = 1;
				error = 0;
			}
		}
		return (error);

	case I_FDINSERT:
	case I_SENDFD:
	case I_RECVFD:
	case I_ATMARK:
		/*
		 * These ioctls do not apply to sockets. I_FDINSERT can be
		 * used to send M_PROTO messages without modifying the socket
		 * state. I_SENDFD/RECVFD should not be used for socket file
		 * descriptor passing since they assume a twisted stream.
		 * SIOCATMARK must be used instead of I_ATMARK.
		 */
#ifdef DEBUG
		cmn_err(CE_WARN,
			"Unsupported STREAMS ioctl 0x%x on socket. "
			"Pid = %d\n",
			(int)cmd, (int)curproc->p_pid);
#endif /* DEBUG */
		return (EOPNOTSUPP);

	default:
		/*
		 * Do the higher-order bits of the ioctl cmd indicate
		 * that it is an I_* streams ioctl?
		 */
		if ((cmd & 0xffffff00U) == STR &&
		    so->so_version == SOV_SOCKBSD) {
#ifdef DEBUG
			cmn_err(CE_WARN,
				"Unsupported STREAMS ioctl 0x%x on socket. "
				"Pid = %d\n",
				(int)cmd, (int)curproc->p_pid);
#endif /* DEBUG */
			return (EOPNOTSUPP);
		}
		if (cmd == I_POP) {
			mutex_enter(&so->so_lock);
			(void) so_lock_single(so, SOLOCKED, 0);
			if (so->so_pushcnt == 0) {
				/* Emulate sockmod being popped */
				dprintso(so, 0,
				    ("sock_ioctl: going to STREAMS version\n"));
				so_sock2stream(so);
				so_unlock_single(so, SOLOCKED);
				mutex_exit(&so->so_lock);
				return (0);
			}
			so_unlock_single(so, SOLOCKED);
			mutex_exit(&so->so_lock);
		}
		error = strioctl(vp, cmd, arg, mode, U_TO_K, cr, rvalp);
		if (error == 0) {
			/*
			 * Track the number of modules pushed above the
			 * imaginary sockmod.
			 */
			mutex_enter(&so->so_lock);
			if (cmd == I_PUSH)
				so->so_pushcnt++;
			else if (cmd == I_POP)
				so->so_pushcnt--;
			mutex_exit(&so->so_lock);
		}
		return (error);
	}
}

/*
 * Allow any flags. Record FNDELAY and FNONBLOCK so that they can be inherited
 * from listener to acceptor.
 */
/* ARGSUSED */
static int
sock_setfl(vnode_t *vp, int oflags, int nflags, cred_t *cr)
{
	struct sonode *so;

	so = VTOSO(vp);

	dprintso(so, 0, ("sock_setfl: oflags 0x%x, nflags 0x%x, state %s\n",
		oflags, nflags, pr_state(so->so_state, so->so_mode)));
	mutex_enter(&so->so_lock);
	if (nflags & FNDELAY)
		so->so_state |= SS_NDELAY;
	else
		so->so_state &= ~SS_NDELAY;
	if (nflags & FNONBLOCK)
		so->so_state |= SS_NONBLOCK;
	else
		so->so_state &= ~SS_NONBLOCK;
	mutex_exit(&so->so_lock);
	return (0);
}

/*
 * Get the made up attributes for the vnode.
 * 4.3BSD returns the current time for all the timestamps.
 * 4.4BSD returns 0 for all the timestamps.
 * Here we use the access and modified times recorded in the sonode.
 *
 * Just like in BSD there is not effect on the underlying file system node
 * bound to an AF_UNIX pathname.
 *
 * When sockmod has been popped this will act just like a stream. Since
 * a socket is always a clone there is no need to inspect the attributes
 * of the "realvp".
 */
/* ARGSUSED */
static int
sock_getattr(
	struct vnode	*vp,
	struct vattr	*vap,
	int		flags,
	struct cred	*cr)
{
	dev_t	fsid;
	struct sonode *so;
	static int	sonode_shift	= 0;

	/*
	 * Calculate the amount of bitshift to a sonode pointer which will
	 * still keep it unique.  See below.
	 */
	if (sonode_shift == 0)
		sonode_shift = highbit(sizeof (struct sonode));
	ASSERT(sonode_shift > 0);

	so = VTOSO(vp);
	fsid = so->so_fsid;

	if (so->so_version == SOV_STREAM) {
		/*
		 * The imaginary "sockmod" has been popped - act
		 * as a stream
		 */
		vap->va_type = VCHR;
		vap->va_mode = 0;
	} else {
		vap->va_type = vp->v_type;
		vap->va_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|
				S_IROTH|S_IWOTH;
	}
	vap->va_uid = vap->va_gid = 0;
	vap->va_fsid = fsid;
	/*
	 * If the va_nodeid is > MAX_USHORT, then i386 stats might fail.
	 * So we shift down the sonode pointer to try and get the most
	 * uniqueness into 16-bits.
	 */
	vap->va_nodeid = ((ino_t)so >> sonode_shift) & 0xFFFF;
	vap->va_nlink = 0;
	vap->va_size = 0;

	/*
	 * We need to zero out the va_rdev to avoid some fstats getting
	 * EOVERFLOW.  This also mimics SunOS 4.x and BSD behaviour.
	 */
	vap->va_rdev = (dev_t)0;
	vap->va_blksize = MAXBSIZE;
	vap->va_nblocks = btod(vap->va_size);

	mutex_enter(&so->so_lock);
	vap->va_atime.tv_sec = so->so_atime;
	vap->va_mtime.tv_sec = so->so_mtime;
	vap->va_ctime.tv_sec = so->so_ctime;
	mutex_exit(&so->so_lock);

	vap->va_atime.tv_nsec = 0;
	vap->va_mtime.tv_nsec = 0;
	vap->va_ctime.tv_nsec = 0;
	vap->va_vcode = 0;

	return (0);
}

/*
 * Set attributes.
 * Just like in BSD there is not effect on the underlying file system node
 * bound to an AF_UNIX pathname.
 *
 * When sockmod has been popped this will act just like a stream. Since
 * a socket is always a clone there is no need to modify the attributes
 * of the "realvp".
 */
/* ARGSUSED */
static int
sock_setattr(
	struct vnode	*vp,
	struct vattr	*vap,
	int		flags,
	struct cred	*cr)
{
	struct sonode *so = VTOSO(vp);
	int error;

	error = 0;	/* no real vnode to update */
	if (error == 0) {
		/*
		 * If times were changed, update sonode.
		 */
		mutex_enter(&so->so_lock);
		if (vap->va_mask & AT_ATIME)
			so->so_atime = vap->va_atime.tv_sec;
		if (vap->va_mask & AT_MTIME) {
			so->so_mtime = vap->va_mtime.tv_sec;
			so->so_ctime = hrestime.tv_sec;
		}
		mutex_exit(&so->so_lock);
	}
	return (error);
}

static int
sock_access(struct vnode *vp, int mode, int flags, struct cred *cr)
{
	struct vnode *accessvp;
	struct sonode *so = VTOSO(vp);

	if ((accessvp = so->so_accessvp) != NULL)
		return (VOP_ACCESS(accessvp, mode, flags, cr));
	else
		return (0);	/* Allow all access. */
}

/*
 * 4.3BSD and 4.4BSD fail a fsync on a socket with EINVAL.
 * This code does the same to be compatible and also to not give an
 * application the impression that the data has actually been "synced"
 * to the other end of the connection.
 */
/* ARGSUSED */
static int
sock_fsync(struct vnode *vp, int syncflag, struct cred *cr)
{
	return (EINVAL);
}

/* ARGSUSED */
static void
sock_inactive(struct vnode *vp, struct cred *cr)
{
	struct sonode *so = VTOSO(vp);

	mutex_enter(&vp->v_lock);
	/*
	 * If no one has reclaimed the vnode, remove from the
	 * cache now.
	 */
	if (vp->v_count < 1)
		cmn_err(CE_PANIC, "sock_inactive: Bad v_count");

	/*
	 * Drop the temporary hold by vn_rele now
	 */
	if (--vp->v_count != 0) {
		mutex_exit(&vp->v_lock);
		return;
	}
	mutex_exit(&vp->v_lock);

	/* We are the sole owner of so now */

	ASSERT(vp->v_pages == NULL);
	sockfree(so);
}

/* ARGSUSED */
static int
sock_fid(struct vnode *vp, struct fid *fidp)
{
	return (EINVAL);
}

/*
 * Sockets are not seekable.
 * (and there is a bug to fix STREAMS to make them fail this as well).
 */
/*ARGSUSED*/
static int
sock_seek(struct vnode *vp, offset_t ooff, offset_t *noffp)
{
	return (ESPIPE);
}

/*
 * Wrapper around the streams poll routine that implements socket poll
 * semantics.
 * The sockfs never calls pollwakeup itself - the stream head take care
 * of all pollwakeups. Since sockfs never holds so_lock when calling the
 * stream head there can never be a deadlock due to holding so_lock across
 * pollwakeup and acquiring so_lock in this routine.
 *
 * However, since the performance of VOP_POLL is critical we avoid
 * acquiring so_lock here. This is based on two assumptions:
 *  - The poll implementation holds locks to serialize the VOP_POLL call
 *    and a pollwakeup for the same pollhead. This ensures that should
 *    e.g. so_state change during a sock_poll call the pollwakeup
 *    (which strsock_* and strrput conspire to issue) is issued after
 *    the state change. Thus the pollwakeup will block until VOP_POLL has
 *    returned and then wake up poll and have it call VOP_POLL again.
 *  - The reading of so_state without holding so_lock does not result in
 *    stale data that is older than the latest state change that has dropped
 *    so_lock. This is ensured by the mutex_exit issuing the appropriate
 *    memory barrier to force the data into the coherency domain.
 */
static int
sock_poll(
	struct vnode	*vp,
	short		events,
	int		anyyet,
	short		*reventsp,
	struct pollhead **phpp)
{
	short origevents = events;
	struct sonode *so = VTOSO(vp);
	int error;
	int so_state = so->so_state;	/* snapshot */

	dprintso(so, 0, ("sock_poll(%p): state %s err %d\n",
			vp, pr_state(so_state, so->so_mode), so->so_error));

	ASSERT(vp->v_type == VSOCK);
	ASSERT(vp->v_stream != NULL);

	if (so->so_version == SOV_STREAM) {
		/* The imaginary "sockmod" has been popped - act as a stream */
		return (strpoll(vp->v_stream, events, anyyet,
			reventsp, phpp));
	}

	if (!(so_state & SS_ISCONNECTED) &&
	    (so->so_mode & SM_CONNREQUIRED)) {
		/* Not connected yet - turn off write side events */
		events &= ~(POLLOUT|POLLWRBAND);
	}
	/*
	 * Check for errors without calling strpoll if the caller wants them.
	 * In sockets the errors are represented as input/output events
	 * and there is no need to ask the stream head for this information.
	 */
	if (so->so_error != 0 &&
	    ((POLLIN|POLLRDNORM|POLLOUT) & origevents)  != 0) {
		*reventsp = (POLLIN|POLLRDNORM|POLLOUT) & origevents;
		return (0);
	}
	/*
	 * Ignore M_PROTO only messages such as the T_EXDATA_IND messages.
	 * These message with only an M_PROTO/M_PCPROTO part and no M_DATA
	 * will not trigger a POLLIN event with POLLRDDATA set.
	 * The handling of urgent data (causing POLLRDBAND) is done by
	 * inspecting SS_OOBPEND below.
	 */
	events |= POLLRDDATA;

	/*
	 * After shutdown(output) a stream head write error is set.
	 * However, we should not return output events.
	 */
	events |= POLLNOERR;
	error = strpoll(vp->v_stream, events, anyyet,
			reventsp, phpp);
	if (error)
		return (error);

	ASSERT(!(*reventsp & POLLERR));

	if (so_state & (SS_HASCONNIND|SS_OOBPEND)) {
		if (so_state & SS_HASCONNIND)
			*reventsp |= (POLLIN|POLLRDNORM) & events;
		if (so_state & SS_OOBPEND)
			*reventsp |= POLLRDBAND & events;
	}
	return (0);
}

/*
 * Wrapper for getmsg. If the socket has been converted to a stream
 * pass the request to the stream head.
 */
int
sock_getmsg(
	struct vnode *vp,
	struct strbuf *mctl,
	struct strbuf *mdata,
	uchar_t *prip,
	int *flagsp,
	int fmode,
	rval_t *rvp
)
{
	struct sonode *so;

	ASSERT(vp->v_type == VSOCK);
	/*
	 * Use the stream head to find the real socket vnode.
	 * This is needed when namefs sits above sockfs.
	 */
	ASSERT(vp->v_stream);
	ASSERT(vp->v_stream->sd_vnode);
	vp = vp->v_stream->sd_vnode;
	ASSERT(vp->v_op == &sock_vnodeops);
	so = VTOSO(vp);

	dprintso(so, 1, ("sock_getmsg(%p) %s\n",
		so, pr_state(so->so_state, so->so_mode)));

	if (so->so_version == SOV_STREAM) {
		/* The imaginary "sockmod" has been popped - act as a stream */
		return (strgetmsg(vp, mctl, mdata, prip, flagsp, fmode, rvp));
	}
	eprintsoline(so, ENOSTR);
	return (ENOSTR);
}

/*
 * Wrapper for putmsg. If the socket has been converted to a stream
 * pass the request to the stream head.
 *
 * Note that a while a regular socket (SOV_SOCKSTREAM) does support the
 * streams ioctl set it does not support putmsg and getmsg.
 * Allowing putmsg would prevent sockfs from tracking the state of
 * the socket/transport and would also invalidate the locking in sockfs.
 */
int
sock_putmsg(
	struct vnode *vp,
	struct strbuf *mctl,
	struct strbuf *mdata,
	uchar_t pri,
	int flag,
	int fmode
)
{
	struct sonode *so;

	ASSERT(vp->v_type == VSOCK);
	/*
	 * Use the stream head to find the real socket vnode.
	 * This is needed when namefs sits above sockfs.
	 */
	ASSERT(vp->v_stream);
	ASSERT(vp->v_stream->sd_vnode);
	vp = vp->v_stream->sd_vnode;
	ASSERT(vp->v_op == &sock_vnodeops);
	so = VTOSO(vp);

	dprintso(so, 1, ("sock_putmsg(%p) %s\n",
		so, pr_state(so->so_state, so->so_mode)));

	if (so->so_version == SOV_STREAM) {
		/* The imaginary "sockmod" has been popped - act as a stream */
		return (strputmsg(vp, mctl, mdata, pri, flag, fmode));
	}
	eprintsoline(so, ENOSTR);
	return (ENOSTR);

}
