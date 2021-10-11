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
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)statvfs.c	1.15	98/03/17 SMI"

/*
 * Get file system statistics (statvfs and fstatvfs).
 */

#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/fstyp.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/statvfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pathname.h>

#include <vm/page.h>

#define	STATVFSCOPY(dst, src)					\
	(dst)->f_bsize	= (src)->f_bsize;			\
	(dst)->f_frsize	= (src)->f_frsize;			\
	(dst)->f_blocks	= (src)->f_blocks;			\
	(dst)->f_bfree	= (src)->f_bfree;			\
	(dst)->f_bavail	= (src)->f_bavail;			\
	(dst)->f_files	= (src)->f_files;			\
	(dst)->f_ffree	= (src)->f_ffree;			\
	(dst)->f_favail	= (src)->f_favail;			\
	(dst)->f_fsid	= (src)->f_fsid;			\
	bcopy((src)->f_basetype, (dst)->f_basetype,		\
		sizeof ((dst)->f_basetype));			\
	(dst)->f_flag	= (src)->f_flag;			\
	(dst)->f_namemax = (src)->f_namemax;			\
	bcopy((src)->f_fstr, (dst)->f_fstr,			\
		sizeof ((dst)->f_fstr))

/*
 * Common routines for statvfs and fstatvfs.
 */

static int
cstatvfs32(struct vfs *vfsp, struct statvfs32 *ubp)
{
	struct statvfs64 ds64;
	struct statvfs32 ds32;
	int error;

#if !defined(lint)
	ASSERT32(sizeof (struct statvfs) == sizeof (struct statvfs32));
	ASSERT32(sizeof (struct statvfs64) == sizeof (struct statvfs64_32));
#endif

	bzero(&ds64, sizeof (ds64));
	if ((error = VFS_STATVFS(vfsp, &ds64)) != 0)
		return (set_errno(error));

	/*
	 * VFS_STATVFS can return data that is incompatible with the space
	 * available the 32-bit statvfs structure.  Check here to see if
	 * it will fit into the 32-bit structure, if not, return EOVERFLOW.
	 *
	 * The check for -1 is because some file systems return -1 in the
	 * fields that are irrelevant or nonessential, and we do not want
	 * to return EOVERFLOW for them.  For example: df is expected to
	 * show -1 in the output for some of these fields on NFS mounted
	 * filesystems.
	 */
	if (ds64.f_files == (fsfilcnt64_t)-1)
		ds64.f_files = UINT32_MAX;
	if (ds64.f_ffree == (fsfilcnt64_t)-1)
		ds64.f_ffree = UINT32_MAX;
	if (ds64.f_favail == (fsfilcnt64_t)-1)
		ds64.f_favail = UINT32_MAX;
	if (ds64.f_bavail == (fsblkcnt64_t)-1)
		ds64.f_bavail = UINT32_MAX;
	if (ds64.f_bfree == (fsblkcnt64_t)-1)
		ds64.f_bfree = UINT32_MAX;

	if (ds64.f_blocks > UINT32_MAX || ds64.f_bfree > UINT32_MAX ||
	    ds64.f_bavail > UINT32_MAX || ds64.f_files > UINT32_MAX ||
	    ds64.f_ffree > UINT32_MAX || ds64.f_favail > UINT32_MAX)
		return (set_errno(EOVERFLOW));
#ifdef _LP64
	/*
	 * On the 64-bit kernel, even these fields grow to 64-bit
	 * quantities in the statvfs64 structure.
	 */
	if (ds64.f_namemax == (ulong_t)-1l)
		ds64.f_namemax = UINT32_MAX;

	if (ds64.f_bsize > UINT32_MAX || ds64.f_frsize > UINT32_MAX ||
	    ds64.f_fsid > UINT32_MAX || ds64.f_flag > UINT32_MAX ||
	    ds64.f_namemax > UINT32_MAX)
		return (set_errno(EOVERFLOW));
#endif

	bzero(&ds32, sizeof (ds32));
	STATVFSCOPY(&ds32, &ds64);
	if (copyout(&ds32, ubp, sizeof (ds32)) != 0)
		return (set_errno(EFAULT));
	return (0);
}

static int
cstatvfs64(struct vfs *vfsp, struct statvfs64 *ubp)
{
	struct statvfs64 ds64;
	int error;

#if !defined(lint)
	ASSERT64(sizeof (struct statvfs) == sizeof (struct statvfs64));
#endif
	bzero(&ds64, sizeof (ds64));
	if ((error = VFS_STATVFS(vfsp, &ds64)) != 0)
		return (set_errno(error));
	if (copyout(&ds64, ubp, sizeof (ds64)) != 0)
		return (set_errno(EFAULT));
	return (0);
}

/*
 * Native system calls
 */
int
statvfs(char *fname, struct statvfs *sbp)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
#ifdef _LP64
	error = cstatvfs64(vp->v_vfsp, (struct statvfs64 *)sbp);
#else
	error = cstatvfs32(vp->v_vfsp, (struct statvfs32 *)sbp);
#endif
	VN_RELE(vp);
	return (error);
}

int
fstatvfs(int fdes, struct statvfs *sbp)
{
	struct file *fp;
	int error;

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
#ifdef _LP64
	error = cstatvfs64(fp->f_vnode->v_vfsp, (struct statvfs64 *)sbp);
#else
	error = cstatvfs32(fp->f_vnode->v_vfsp, (struct statvfs32 *)sbp);
#endif
	releasef(fdes);
	return (error);
}

#if defined(_ILP32)

/*
 * Large File system calls.
 *
 * (We deliberately don't have special "large file" system calls in the
 * 64-bit kernel -- we just use the native versions, since they're just
 * as functional.)
 */
int
statvfs64(char *fname, struct statvfs64 *sbp)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
	error = cstatvfs64(vp->v_vfsp, sbp);
	VN_RELE(vp);
	return (error);
}

int
fstatvfs64(int fdes, struct statvfs64 *sbp)
{
	struct file *fp;
	int error;

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	error = cstatvfs64(fp->f_vnode->v_vfsp, sbp);
	releasef(fdes);
	return (error);
}

#endif	/* _ILP32 */

#ifdef _SYSCALL32_IMPL

static int
cstatvfs64_32(struct vfs *vfsp, struct statvfs64_32 *ubp)
{
	struct statvfs64 ds64;
	struct statvfs64_32 ds64_32;
	int error;

	bzero(&ds64, sizeof (ds64));
	if ((error = VFS_STATVFS(vfsp, &ds64)) != 0)
		return (set_errno(error));

	/*
	 * On the 64-bit kernel, even these fields grow to 64-bit
	 * quantities in the statvfs64 structure.
	 */
	if (ds64.f_namemax == (ulong_t)-1l)
		ds64.f_namemax = UINT32_MAX;

	if (ds64.f_bsize > UINT32_MAX || ds64.f_frsize > UINT32_MAX ||
	    ds64.f_fsid > UINT32_MAX || ds64.f_flag > UINT32_MAX ||
	    ds64.f_namemax > UINT32_MAX)
		return (set_errno(EOVERFLOW));

	STATVFSCOPY(&ds64_32, &ds64);
	if (copyout(&ds64_32, ubp, sizeof (ds64_32)) != 0)
		return (set_errno(EFAULT));
	return (0);
}

/*
 * ILP32 "small file" system calls on LP64 kernel
 */
int
statvfs32(char *fname, struct statvfs32 *sbp)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
	error = cstatvfs32(vp->v_vfsp, sbp);
	VN_RELE(vp);
	return (error);
}

int
fstatvfs32(int fdes, struct statvfs32 *sbp)
{
	struct file *fp;
	int error;

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	error = cstatvfs32(fp->f_vnode->v_vfsp, sbp);
	releasef(fdes);
	return (error);
}

/*
 * ILP32 Large File system calls on LP64 kernel
 */
int
statvfs64_32(char *fname, struct statvfs64_32 *sbp)
{
	vnode_t *vp;
	int error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
	error = cstatvfs64_32(vp->v_vfsp, sbp);
	VN_RELE(vp);
	return (error);
}

int
fstatvfs64_32(int fdes, struct statvfs64_32 *sbp)
{
	struct file *fp;
	int error;

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	error = cstatvfs64_32(fp->f_vnode->v_vfsp, sbp);
	releasef(fdes);
	return (error);
}

#endif	/* _SYSCALL32_IMPL */
