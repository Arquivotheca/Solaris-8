/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fs_subr.c	1.48	99/09/16 SMI"

/*
 * Generic vnode operations.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/unistd.h>
#include <sys/cred.h>
#include <sys/poll.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/stream.h>
#include <fs/fs_subr.h>
#include <sys/acl.h>
#include <sys/share.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/file.h>

/*
 * The associated operation is not supported by the file system.
 */
int
fs_nosys()
{
	return (ENOSYS);
}

/*
 * Free the file system specific resources. For the file systems that
 * do not support the forced unmount, it will be a nop function.
 */

/*ARGSUSED*/
void
fs_freevfs(vfs_t *vfsp)
{
}

/* ARGSUSED */
int
fs_nosys_map(struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t *addrp,
	size_t len,
	u_char prot,
	u_char maxprot,
	u_int flags,
	struct cred *cr)
{
	return (ENOSYS);
}

/* ARGSUSED */
int
fs_nosys_addmap(struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,
	u_char prot,
	u_char maxprot,
	u_int flags,
	struct cred *cr)
{
	return (ENOSYS);
}

/* ARGSUSED */
int
fs_nosys_poll(vnode_t *vp,
	register short events,
	int anyyet,
	register short *reventsp,
	struct pollhead **phpp)
{
	return (ENOSYS);
}


/*
 * The file system has nothing to sync to disk.  However, the
 * VFS_SYNC operation must not fail.
 */
/* ARGSUSED */
int
fs_sync(struct vfs *vfspp, short flag, cred_t *cr)
{
	return (0);
}

/*
 * Read/write lock/unlock.  Does nothing.
 */
/* ARGSUSED */
void
fs_rwlock(vnode_t *vp, int write_lock)
{
}

/* ARGSUSED */
void
fs_rwunlock(vnode_t *vp, int write_lock)
{
}

/*
 * Compare two vnodes.
 */
int
fs_cmp(vnode_t *vp1, vnode_t *vp2)
{
	return (vp1 == vp2);
}

/*
 * File and record locking.
 */
/* ARGSUSED */
int
fs_frlock(register vnode_t *vp, int cmd, struct flock64 *bfp, int flag,
	offset_t offset, cred_t *cr)
{
	int frcmd;
	int nlmid;

	switch (cmd) {

	case F_GETLK:
	case F_O_GETLK:
		if (flag & F_REMOTELOCK) {
			frcmd = RCMDLCK;
			break;
		}
		if (flag & F_PXFSLOCK) {
			frcmd = PCMDLCK;
			break;
		}
		bfp->l_pid = ttoproc(curthread)->p_pid;
		bfp->l_sysid = 0;
		frcmd = 0;
		break;

	case F_SETLK:
		if (flag & F_REMOTELOCK) {
			frcmd = SETFLCK|RCMDLCK;
			break;
		}
		if (flag & F_PXFSLOCK) {
			frcmd = SETFLCK|PCMDLCK;
			break;
		}
		bfp->l_pid = ttoproc(curthread)->p_pid;
		bfp->l_sysid = 0;
		frcmd = SETFLCK;
		break;

	case F_SETLKW:
		if (flag & F_REMOTELOCK) {
			frcmd = SETFLCK|SLPFLCK|RCMDLCK;
			break;
		}
		if (flag & F_PXFSLOCK) {
			frcmd = SETFLCK|SLPFLCK|PCMDLCK;
			break;
		}
		bfp->l_pid = ttoproc(curthread)->p_pid;
		bfp->l_sysid = 0;
		frcmd = SETFLCK|SLPFLCK;
		break;

	case F_HASREMOTELOCKS:
		nlmid = GETNLMID(bfp->l_sysid);
		if (nlmid != 0) {	/* booted as a cluster */
			l_has_rmt(bfp) =
				cl_flk_has_remote_locks_for_nlmid(vp, nlmid);
		} else {		/* not booted as a cluster */
			l_has_rmt(bfp) = flk_has_remote_locks(vp);
		}

		return (0);

	default:
		return (EINVAL);
	}

	return (reclock(vp, bfp, frcmd, flag, offset));
}

/*
 * Allow any flags.
 */
/* ARGSUSED */
int
fs_setfl(vnode_t *vp, int oflags, int nflags, cred_t *cr)
{
	return (0);
}

/*
 * Return the answer requested to poll() for non-device files.
 * Only POLLIN, POLLRDNORM, and POLLOUT are recognized.
 */
struct pollhead fs_pollhd;

/* ARGSUSED */
int
fs_poll(vnode_t *vp,
	register short events,
	int anyyet,
	register short *reventsp,
	struct pollhead **phpp)
{
	*reventsp = 0;
	if (events & POLLIN)
		*reventsp |= POLLIN;
	if (events & POLLRDNORM)
		*reventsp |= POLLRDNORM;
	if (events & POLLRDBAND)
		*reventsp |= POLLRDBAND;
	if (events & POLLOUT)
		*reventsp |= POLLOUT;
	if (events & POLLWRBAND)
		*reventsp |= POLLWRBAND;
	*phpp = !anyyet && !*reventsp ? &fs_pollhd : (struct pollhead *)NULL;
	return (0);
}

/*
 * vcp is an in/out parameter.  Updates *vcp with a version code suitable
 * for the va_vcode attribute, possibly the value passed in.
 *
 * The va_vcode attribute is intended to support cache coherency
 * and IO atomicity for file servers that provide traditional
 * UNIX file system semantics.  The vnode of the file object
 * whose va_vcode is being updated must be held locked when
 * this function is evaluated.
 *
 * Returns 0 for success, a nonzero errno for failure.
 */
int
fs_vcode(vnode_t *vp, u_int *vcp)
{
	static u_int	vcode;
	int		error = 0;

	if (vp->v_type == VREG && *vcp == 0) {
		if (vcode == (u_int)~0) {
			cmn_err(CE_WARN, "fs_vcode: vcode overflow");
			error = ENOMEM;
		} else
			*vcp = ++vcode;
	}
	return (error);
}

/*
 * POSIX pathconf() support.
 */
/* ARGSUSED */
int
fs_pathconf(vnode_t *vp, int cmd, u_long *valp, cred_t *cr)
{
	register u_long val;
	register int error = 0;
	struct statvfs64 vfsbuf;

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
		bzero(&vfsbuf, sizeof (vfsbuf));
		if (error = VFS_STATVFS(vp->v_vfsp, &vfsbuf))
			break;
		val = vfsbuf.f_namemax;
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
			val = (u_long)-1;
		break;

	case _PC_VDISABLE:
		val = _POSIX_VDISABLE;
		break;

	case _PC_CHOWN_RESTRICTED:
		if (rstchown)
			val = rstchown; /* chown restricted enabled */
		else
			val = (u_long)-1;
		break;

	case _PC_FILESIZEBITS:

		/*
		 * If ever we come here it means that underlying file system
		 * does not recognise the command and therefore this
		 * configurable limit cannot be determined. We return -1
		 * and don't change errno.
		 */

		val = (u_long)-1;    /* large file support */
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
 * Dispose of a page.
 */
/* ARGSUSED */
void
fs_dispose(struct vnode *vp, page_t *pp, int fl, int dn, struct cred *cr)
{

	ASSERT(fl == B_FREE || fl == B_INVAL);

	if (fl == B_FREE)
		page_free(pp, dn);
	else
		page_destroy(pp, dn);
}

/* ARGSUSED */
void
fs_nodispose(struct vnode *vp, page_t *pp, int fl, int dn, struct cred *cr)
{
	cmn_err(CE_PANIC, "fs_nodispose invoked");
}

/*
 * fabricate acls for file systems that do not support acls.
 */
/* ARGSUSED */
int
fs_fab_acl(vp, vsecattr, flag, cr)
vnode_t		*vp;
vsecattr_t	*vsecattr;
int		flag;
cred_t		*cr;
{
	aclent_t	*aclentp;
	struct vattr	vattr;
	int		error;

	vsecattr->vsa_aclcnt	= 0;
	vsecattr->vsa_aclentp	= NULL;
	vsecattr->vsa_dfaclcnt	= 0;	/* Default ACLs are not fabricated */
	vsecattr->vsa_dfaclentp	= NULL;

	if (vsecattr->vsa_mask & (VSA_ACLCNT | VSA_ACL))
		vsecattr->vsa_aclcnt	= 4; /* USER, GROUP, OTHER, and CLASS */

	if (vsecattr->vsa_mask & VSA_ACL) {
		vsecattr->vsa_aclentp = kmem_zalloc(4 * sizeof (aclent_t),
		    KM_SLEEP);
		vattr.va_mask = AT_MODE;
		if (error = VOP_GETATTR(vp, &vattr, 0, CRED()))
			return (error);
		aclentp = vsecattr->vsa_aclentp;

		aclentp->a_type = USER_OBJ;	/* Owner */
		aclentp->a_perm = ((ushort_t)(vattr.va_mode & 0700)) >> 6;
		aclentp->a_id = vattr.va_uid;   /* Really undefined */
		aclentp++;

		aclentp->a_type = GROUP_OBJ;    /* Group */
		aclentp->a_perm = ((ushort_t)(vattr.va_mode & 0070)) >> 3;
		aclentp->a_id = vattr.va_gid;   /* Really undefined */
		aclentp++;

		aclentp->a_type = OTHER_OBJ;    /* Other */
		aclentp->a_perm = vattr.va_mode & 0007;
		aclentp->a_id = -1;		/* Really undefined */
		aclentp++;

		aclentp->a_type = CLASS_OBJ;    /* Class */
		aclentp->a_perm = (ushort_t)(0777);
		aclentp->a_id = -1;		/* Really undefined */
	}

	return (0);
}

/*
 * Common code for implementing DOS share reservations
 */
int
fs_shrlock(struct vnode *vp, int cmd, struct shrlock *shr, int flag)
{
	int error;

	/*
	 * Check access permissions
	 */
	if ((cmd & F_SHARE) &&
		((shr->s_access & F_RDACC && (flag & FREAD) == 0) ||
		(shr->s_access == F_WRACC && (flag & FWRITE) == 0)))
			return (EBADF);

	switch (cmd) {

	case F_SHARE:
		error = add_share(vp, shr);
		break;

	case F_UNSHARE:
		error = del_share(vp, shr);
		break;

	case F_HASREMOTELOCKS:
		/*
		 * We are overloading this command to refer to remote
		 * shares as well as remote locks, despite its name.
		 */
		shr->s_access = shr_has_remote_shares(vp, shr->s_sysid);
		error = 0;
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}
