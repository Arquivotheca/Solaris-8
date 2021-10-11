/*
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

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
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *			All rights reserved.
 *
 */

#pragma ident	"@(#)stat.c	1.25	99/06/04 SMI"

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/pathname.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mode.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/ioreq.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>

/*
 * Get file attribute information through a file name or a file descriptor.
 */

#if defined(i386) || defined(__i386) || \
	(defined(__ia64) && defined(_SYSCALL32_IMPL))
static	int	o_cstat(vnode_t *, struct o_stat *, int, struct cred *);
#endif

#if defined(_SYSCALL32_IMPL)
static int cstat32(vnode_t *, struct stat32 *, int, struct cred *);
#endif

static	int	cstat(vnode_t *, struct stat *, int, struct cred *);


int
stat(char *fname, struct stat *sb)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
#if defined(i386) || defined(__i386)
	error = o_cstat(vp, (struct o_stat *)sb, ATTR_REAL, CRED());
#else
	error = cstat(vp, sb, ATTR_REAL, CRED());
#endif
	VN_RELE(vp);
	return (error);
}

int
lstat(char *fname, struct stat *sb)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
#if defined(i386) || defined(__i386)
	error = o_cstat(vp, (struct o_stat *)sb, 0, CRED());
#else
	error = cstat(vp, sb, 0, CRED());
#endif
	VN_RELE(vp);
	return (error);
}

int
fstat(int fdes, struct stat *sb)
{
	file_t *fp;
	int error;

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
#if defined(i386) || defined(__i386)
	error = o_cstat(fp->f_vnode, (struct o_stat *)sb, 0, fp->f_cred);
#else
	error = cstat(fp->f_vnode, sb, 0, fp->f_cred);
#endif
	releasef(fdes);
	return (error);
}

/*
 * Common code for stat(), lstat(), and fstat().
 */
static int
cstat(vnode_t *vp, struct stat *ubp, int flag, struct cred *cr)
{
	struct stat sb;
	struct vattr vattr;
	int error;
	struct vfssw *vswp;

	vattr.va_mask = AT_STAT | AT_NBLOCKS | AT_BLKSIZE | AT_SIZE;
	if (error = VOP_GETATTR(vp, &vattr, flag, cr))
		return (set_errno(error));

#ifdef	_ILP32
	/*
	 * XXX
	 * This is for compatibility for 32-bit programs which do stat()
	 * on a block device bigger than 2 gigabytes and can't deal with
	 * stat() failing and returning EOVERFLOW.
	 * XXX
	 */
	if ((vp->v_type == VBLK) && (vattr.va_size > INT_MAX)) {
		vattr.va_size = INT_MAX;
		vattr.va_nblocks = btod(vattr.va_size);
	}
#endif	/* _ILP32 */

	if (vattr.va_size > MAXOFF_T || vattr.va_nblocks > LONG_MAX ||
	    vattr.va_nodeid > ULONG_MAX) {
		return (set_errno(EOVERFLOW));
	}

	bzero(&sb, sizeof (sb));
	sb.st_mode = VTTOIF(vattr.va_type) | vattr.va_mode;
	sb.st_uid = vattr.va_uid;
	sb.st_gid = vattr.va_gid;
	sb.st_dev = vattr.va_fsid;
	sb.st_nlink = vattr.va_nlink;
	sb.st_atim = vattr.va_atime;
	sb.st_mtim = vattr.va_mtime;
	sb.st_ctim = vattr.va_ctime;
	sb.st_rdev = vattr.va_rdev;
	sb.st_blksize = vattr.va_blksize;
	sb.st_ino = (ino_t)vattr.va_nodeid;
	sb.st_size = (off_t)vattr.va_size;
	sb.st_blocks = (blkcnt_t)vattr.va_nblocks;
	if (vp->v_vfsp != NULL) {
		vswp = &vfssw[vp->v_vfsp->vfs_fstype];
		if (vswp->vsw_name && *vswp->vsw_name)
			(void) strcpy(sb.st_fstype, vswp->vsw_name);
	}
	if (copyout(&sb, ubp, sizeof (sb)))
		return (set_errno(EFAULT));
	return (0);
}

#if defined(i386) || defined(__i386) || \
	(defined(__ia64) && defined(_SYSCALL32_IMPL))

static int
o_cstat(vnode_t *vp, struct o_stat *ubp, int flag, struct cred *cr)
{
	struct o_stat sb;
	struct vattr vattr;
	int error;

	vattr.va_mask = AT_STAT;
	if (error = VOP_GETATTR(vp, &vattr, flag, cr))
		return (set_errno(error));
	sb.st_mode = (o_mode_t)(VTTOIF(vattr.va_type) | vattr.va_mode);

	if ((vp->v_type == VBLK) && (vattr.va_size > INT_MAX))
		vattr.va_size = INT_MAX;
	/*
	 * Check for large values.
	 */
	if ((uint_t)vattr.va_uid > (uint_t)USHRT_MAX ||
	    (uint_t)vattr.va_gid > (uint_t)USHRT_MAX ||
	    vattr.va_nodeid > USHRT_MAX || vattr.va_nlink > SHRT_MAX ||
	    vattr.va_size > MAXOFF32_T)
		return (set_errno(EOVERFLOW));

	sb.st_uid = (o_uid_t)vattr.va_uid;
	sb.st_gid = (o_gid_t)vattr.va_gid;
	/*
	 * Need to convert expanded dev to old dev format.
	 */
	if (vattr.va_fsid & 0x8000)
		sb.st_dev = (o_dev_t)vattr.va_fsid;
	else
		sb.st_dev = (o_dev_t)cmpdev(vattr.va_fsid);
	sb.st_ino = (o_ino_t)vattr.va_nodeid;
	sb.st_nlink = (o_nlink_t)vattr.va_nlink;
	sb.st_size = (off32_t)vattr.va_size;
	sb.st_atime = (time32_t)vattr.va_atime.tv_sec;
	sb.st_mtime = (time32_t)vattr.va_mtime.tv_sec;
	sb.st_ctime = (time32_t)vattr.va_ctime.tv_sec;
	sb.st_rdev = (o_dev_t)cmpdev(vattr.va_rdev);

	if (copyout(&sb, ubp, sizeof (sb)))
		return (set_errno(EFAULT));
	return (0);
}

/*
 * For i386, USL SVR4 Intel compatibility is needed.  xstat() is
 * used for distinguishing old and new uses of stat.
 */

#if defined(__ia64)
#define	STRUCT_STAT	struct stat32
#define	XSTAT		xstat32
#define	LXSTAT		lxstat32
#define	FXSTAT		fxstat32
#define	CSTAT		cstat32
#else
#define	STRUCT_STAT	struct stat
#define	XSTAT		xstat
#define	LXSTAT		lxstat
#define	FXSTAT		fxstat
#define	CSTAT		cstat
#endif

int
XSTAT(int version, char *fname, STRUCT_STAT *sb)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));

	/*
	 * Check version.
	 */
	switch (version) {

	case _STAT_VER:
		/* SVR4 stat */
		error = CSTAT(vp, sb, ATTR_REAL, CRED());
		break;

	case _R3_STAT_VER:
		/* SVR3 stat */
		error = o_cstat(vp, (struct o_stat *)sb, ATTR_REAL, CRED());
		break;

	default:
		error = set_errno(EINVAL);
	}

	VN_RELE(vp);
	return (error);
}

/* ARGSUSED */
int
LXSTAT(int version, char *fname, STRUCT_STAT *sb)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp))
		return (set_errno(error));

	/*
	 * Check version.
	 */
	switch (version) {

	case _STAT_VER:
		/* SVR4 stat */
		error = CSTAT(vp, sb, 0, CRED());
		break;

	case _R3_STAT_VER:
		/* SVR3 stat */
		error = o_cstat(vp, (struct o_stat *)sb, 0, CRED());
		break;

	default:
		error = set_errno(EINVAL);
	}

	VN_RELE(vp);
	return (error);
}

int
FXSTAT(int version, int fdes, STRUCT_STAT *sb)
{
	file_t *fp;
	int error;

	/*
	 * Check version number.
	 */
	switch (version) {
	case _STAT_VER:
		break;
	default:
		return (set_errno(EINVAL));
	}

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));

	switch (version) {
	case _STAT_VER:
		/* SVR4 stat */
		error = CSTAT(fp->f_vnode, sb, 0, fp->f_cred);
		break;

	case _R3_STAT_VER:
		/* SVR3 stat */
		error = o_cstat(fp->f_vnode, (struct o_stat *)sb,
			0, fp->f_cred);
		break;

	default:
		error = set_errno(EINVAL);
	}

	releasef(fdes);
	return (error);
}

#else	/* i386 || (__ia64 && _SYSCALL32_IMPL) */

#if defined(_ILP32)

/*ARGSUSED*/
int
xstat(int version, char *fname, struct stat *sb)
{
	return (stat(fname, sb));
}

/*ARGSUSED*/
int
lxstat(int version, char *fname, struct stat *sb)
{
	return (lstat(fname, sb));
}

/*ARGSUSED*/
int
fxstat(int version, int fdes, struct stat *sb)
{
	return (fstat(fdes, sb));
}

#elif defined(_SYSCALL32_IMPL)

extern int stat32(char *, struct stat32 *);
extern int lstat32(char *, struct stat32 *);
extern int fstat32(int, struct stat32 *);

/*ARGSUSED*/
int
xstat32(int version, char *fname, struct stat32 *sb)
{
	return (stat32(fname, sb));
}

/*ARGSUSED*/
int
lxstat32(int version, char *fname, struct stat32 *sb)
{
	return (lstat32(fname, sb));
}

/*ARGSUSED*/
int
fxstat32(int version, int fdes, struct stat32 *sb)
{
	return (fstat32(fdes, sb));
}

#endif

#endif /* i386 */

#if defined(_SYSCALL32_IMPL)

int
stat32(char *fname, struct stat32 *sb)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
#if defined(__ia64)
	error = o_cstat(vp, (struct o_stat *)sb, ATTR_REAL, CRED());
#else
	error = cstat32(vp, sb, ATTR_REAL, CRED());
#endif
	VN_RELE(vp);
	return (error);
}

int
lstat32(char *fname, struct stat32 *sb)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
#if defined(__ia64)
	error = o_cstat(vp, (struct o_stat *)sb, 0, CRED());
#else
	error = cstat32(vp, sb, 0, CRED());
#endif
	VN_RELE(vp);
	return (error);
}

int
fstat32(int fdes, struct stat32 *sb)
{
	file_t *fp;
	int error;

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
#if defined(__ia64)
	error = o_cstat(fp->f_vnode, (struct o_stat *)sb, 0, fp->f_cred);
#else
	error = cstat32(fp->f_vnode, sb, 0, fp->f_cred);
#endif
	releasef(fdes);
	return (error);
}

/*
 * Common code for stat32(), lstat32(), and fstat32().
 * (64-bit kernel, 32-bit applications, 32-bit files)
 */
static int
cstat32(vnode_t *vp, struct stat32 *ubp, int flag, struct cred *cr)
{
	struct stat32 sb;
	struct vattr vattr;
	int error;
	struct vfssw *vswp;
	dev32_t st_dev, st_rdev;

	vattr.va_mask = AT_STAT | AT_NBLOCKS | AT_BLKSIZE | AT_SIZE;
	if (error = VOP_GETATTR(vp, &vattr, flag, cr))
		return (set_errno(error));

	if ((vp->v_type == VBLK) && (vattr.va_size > INT_MAX)) {
		vattr.va_size = INT_MAX;
		vattr.va_nblocks = btod(vattr.va_size);
	}

	if (!cmpldev(&st_dev, vattr.va_fsid) ||
	    !cmpldev(&st_rdev, vattr.va_rdev) ||
	    vattr.va_size > MAXOFF32_T ||
	    vattr.va_nblocks > INT32_MAX ||
	    vattr.va_nodeid > UINT32_MAX ||
	    TIMESPEC_OVERFLOW(&(vattr.va_atime)) ||
	    TIMESPEC_OVERFLOW(&(vattr.va_mtime)) ||
	    TIMESPEC_OVERFLOW(&(vattr.va_ctime)))
		return (set_errno(EOVERFLOW));

	bzero(&sb, sizeof (sb));
	sb.st_dev = st_dev;
	sb.st_ino = (ino32_t)vattr.va_nodeid;
	sb.st_mode = VTTOIF(vattr.va_type) | vattr.va_mode;
	sb.st_nlink = vattr.va_nlink;
	sb.st_uid = vattr.va_uid;
	sb.st_gid = vattr.va_gid;
	sb.st_rdev = st_rdev;
	sb.st_size = (off32_t)vattr.va_size;
	TIMESPEC_TO_TIMESPEC32(&(sb.st_atim), &(vattr.va_atime));
	TIMESPEC_TO_TIMESPEC32(&(sb.st_mtim), &(vattr.va_mtime));
	TIMESPEC_TO_TIMESPEC32(&(sb.st_ctim), &(vattr.va_ctime));
	sb.st_blksize = vattr.va_blksize;
	sb.st_blocks = (blkcnt32_t)vattr.va_nblocks;
	if (vp->v_vfsp != NULL) {
		vswp = &vfssw[vp->v_vfsp->vfs_fstype];
		if (vswp->vsw_name && *vswp->vsw_name)
			(void) strcpy(sb.st_fstype, vswp->vsw_name);
	}
	if (copyout(&sb, ubp, sizeof (sb)))
		return (set_errno(EFAULT));
	return (0);
}
#endif	/* _SYSCALL32_IMPL */

#if defined(_ILP32)

static int cstat64(vnode_t *vp, struct stat64 *ubp, int flag, struct cred *cr);

int
stat64(char *fname, struct stat64 *sb)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
	error = cstat64(vp, sb, ATTR_REAL, CRED());
	VN_RELE(vp);
	return (error);
}

int
lstat64(char *fname, struct stat64 *sb)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
	error = cstat64(vp, sb, 0, CRED());
	VN_RELE(vp);
	return (error);
}

int
fstat64(int fdes, struct stat64 *sb)
{
	file_t *fp;
	int error;

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	error = cstat64(fp->f_vnode, sb, 0, fp->f_cred);
	releasef(fdes);
	return (error);
}

/*
 * Common code for stat64(), lstat64(), and fstat64().
 * (32-bit kernel, 32-bit applications, 64-bit files)
 */
static int
cstat64(vnode_t *vp, struct stat64 *ubp, int flag, struct cred *cr)
{
	struct stat64 lsb;
	struct vattr vattr;
	int error;
	struct vfssw *vswp;

	vattr.va_mask = AT_STAT | AT_NBLOCKS | AT_BLKSIZE | AT_SIZE;
	if (error = VOP_GETATTR(vp, &vattr, flag, cr))
		return (set_errno(error));

	bzero(&lsb, sizeof (lsb));
	lsb.st_mode = VTTOIF(vattr.va_type) | vattr.va_mode;
	lsb.st_uid = vattr.va_uid;
	lsb.st_gid = vattr.va_gid;
	lsb.st_dev = vattr.va_fsid;
	lsb.st_ino = vattr.va_nodeid;
	lsb.st_nlink = vattr.va_nlink;
	lsb.st_atim = vattr.va_atime;
	lsb.st_mtim = vattr.va_mtime;
	lsb.st_ctim = vattr.va_ctime;
	lsb.st_rdev = vattr.va_rdev;
	lsb.st_blksize = vattr.va_blksize;
	lsb.st_size = vattr.va_size;
	lsb.st_blocks = vattr.va_nblocks;
	if (vp->v_vfsp != NULL) {
		vswp = &vfssw[vp->v_vfsp->vfs_fstype];
		if (vswp->vsw_name && *vswp->vsw_name)
			(void) strcpy(lsb.st_fstype, vswp->vsw_name);
	}
	if (copyout(&lsb, ubp, sizeof (lsb)))
		return (set_errno(EFAULT));
	return (0);
}

#elif defined(_SYSCALL32_IMPL)

/*
 * We'd really like to call the "native" stat calls for these ones,
 * but the problem is that the 64-bit ABI defines the 'stat64' structure
 * differently from the way the 32-bit ABI defines it.
 */

static int cstat64_32(vnode_t *vp, struct stat64_32 *ubp, int flag, cred_t *cr);

int
stat64_32(char *fname, struct stat64_32 *sb)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
	error = cstat64_32(vp, sb, ATTR_REAL, CRED());
	VN_RELE(vp);
	return (error);
}

int
lstat64_32(char *fname, struct stat64_32 *sb)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
	error = cstat64_32(vp, sb, 0, CRED());
	VN_RELE(vp);
	return (error);
}

int
fstat64_32(int fdes, struct stat64_32 *sb)
{
	file_t *fp;
	int error;

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	error = cstat64_32(fp->f_vnode, sb, 0, fp->f_cred);
	releasef(fdes);
	return (error);
}

/*
 * Common code for stat64_32(), lstat64_32(), and fstat64_32().
 * (64-bit kernel, 32-bit applications, 64-bit files)
 */
static int
cstat64_32(vnode_t *vp, struct stat64_32 *ubp, int flag, cred_t *cr)
{
	struct stat64_32 lsb;
	struct vattr vattr;
	int error;
	struct vfssw *vswp;
	dev32_t st_dev, st_rdev;

	vattr.va_mask = AT_STAT | AT_NBLOCKS | AT_BLKSIZE | AT_SIZE;
	if (error = VOP_GETATTR(vp, &vattr, flag, cr))
		return (set_errno(error));

	if (!cmpldev(&st_dev, vattr.va_fsid) ||
	    !cmpldev(&st_rdev, vattr.va_rdev) ||
	    TIMESPEC_OVERFLOW(&(vattr.va_atime)) ||
	    TIMESPEC_OVERFLOW(&(vattr.va_mtime)) ||
	    TIMESPEC_OVERFLOW(&(vattr.va_ctime)))
		return (set_errno(EOVERFLOW));

	bzero(&lsb, sizeof (lsb));
	lsb.st_dev = st_dev;
	lsb.st_ino = vattr.va_nodeid;
	lsb.st_mode = VTTOIF(vattr.va_type) | vattr.va_mode;
	lsb.st_nlink = vattr.va_nlink;
	lsb.st_uid = vattr.va_uid;
	lsb.st_gid = vattr.va_gid;
	lsb.st_rdev = st_rdev;
	lsb.st_size = vattr.va_size;
	TIMESPEC_TO_TIMESPEC32(&(lsb.st_atim), &(vattr.va_atime));
	TIMESPEC_TO_TIMESPEC32(&(lsb.st_mtim), &(vattr.va_mtime));
	TIMESPEC_TO_TIMESPEC32(&(lsb.st_ctim), &(vattr.va_ctime));
	lsb.st_blksize = vattr.va_blksize;
	lsb.st_blocks = vattr.va_nblocks;
	if (vp->v_vfsp != NULL) {
		vswp = &vfssw[vp->v_vfsp->vfs_fstype];
		if (vswp->vsw_name && *vswp->vsw_name)
			(void) strcpy(lsb.st_fstype, vswp->vsw_name);
	}
	if (copyout(&lsb, ubp, sizeof (lsb)))
		return (set_errno(EFAULT));
	return (0);
}

#endif /* _SYSCALL32_IMPL */
