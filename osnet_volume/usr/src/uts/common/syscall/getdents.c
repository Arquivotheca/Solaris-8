/*
 * Copyright (c) 1986-1998 by Sun Microsystems, Inc.
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

#pragma	ident	"@(#)getdents.c	1.14	98/06/30 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/dirent.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mode.h>
#include <sys/uio.h>
#include <sys/ioreq.h>
#include <sys/filio.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>

#if defined(_SYSCALL32_IMPL) || defined(_ILP32)

/*
 * Get directory entries in a file system-independent format.
 *
 * The 32-bit version of this function now allocates a buffer to grab the
 * directory entries in dirent64 formats from VOP_READDIR routines.
 * The dirent64 structures are converted to dirent32 structures and
 * copied to the user space.
 *
 * Both 32-bit and 64-bit versions of libc use getdents64() and therefore
 * we don't expect any major performance impact due to the extra kmem_alloc's
 * and copying done in this routine.
 */

#define	MAXGETDENTS_SIZE	(64 * 1024)

/*
 * Native 32-bit system call for non-large-file applications.
 */
int
getdents32(int fd, void *buf, size_t count)
{
	vnode_t *vp;
	file_t *fp;
	struct uio auio;
	struct iovec aiov;
	register int error;
	int sink;
	char *newbuf;
	char *obuf;
	int bufsize;
	int osize, nsize;
	struct dirent64 *dp;
	struct dirent32 *op;

	if (count < sizeof (struct dirent32))
		return (set_errno(EINVAL));

	if ((fp = getf(fd)) == NULL)
		return (set_errno(EBADF));
	vp = fp->f_vnode;
	if (vp->v_type != VDIR) {
		releasef(fd);
		return (set_errno(ENOTDIR));
	}

	/*
	 * Don't let the user overcommit kernel resources.
	 */
	if (count > MAXGETDENTS_SIZE)
		count = MAXGETDENTS_SIZE;

	bufsize = count;
	newbuf = kmem_zalloc(bufsize, KM_SLEEP);
	obuf = kmem_zalloc(bufsize, KM_SLEEP);

	aiov.iov_base = newbuf;
	aiov.iov_len = count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_loffset = fp->f_offset;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_resid = count;
	auio.uio_fmode = 0;
	VOP_RWLOCK(vp, 0);
	error = VOP_READDIR(vp, &auio, fp->f_cred, &sink);
	VOP_RWUNLOCK(vp, 0);
	if (error)
		goto out;
	count = count - auio.uio_resid;
	fp->f_offset = auio.uio_loffset;

	dp = (struct dirent64 *)newbuf;
	op = (struct dirent32 *)obuf;
	osize = 0;
	nsize = 0;

	while (nsize < count) {
		/*
		 * This check ensures that the 64 bit d_ino and d_off
		 * fields will fit into their 32 bit equivalents.
		 *
		 * Although d_off is a signed value, the check is done
		 * against the full 32 bits because certain file systems,
		 * NFS for one, allow directory cookies to use the full
		 * 32 bits.  We use uint64_t because there is no exact
		 * unsigned analog to the off64_t type of dp->d_off.
		 */
		if (dp->d_ino > (ino64_t)UINT32_MAX ||
		    dp->d_off > (uint64_t)UINT32_MAX) {
			error = EOVERFLOW;
			goto out;
		}
		op->d_ino = (ino32_t)dp->d_ino;
		op->d_off = (off32_t)dp->d_off;
		(void) strcpy(op->d_name, dp->d_name);
		op->d_reclen = (uint16_t)DIRENT32_RECLEN(strlen(op->d_name));
		nsize += (u_int)dp->d_reclen;
		osize += (u_int)op->d_reclen;
		dp = (struct dirent64 *)((char *)dp + (u_int)dp->d_reclen);
		op = (struct dirent32 *)((char *)op + (u_int)op->d_reclen);
	}

	ASSERT(osize <= count);
	ASSERT((char *)op <= (char *)obuf + bufsize);
	ASSERT((char *)dp <= (char *)newbuf + bufsize);

	if ((error = copyout(obuf, buf, osize)) < 0)
		error = EFAULT;
out:
	kmem_free(newbuf, bufsize);
	kmem_free(obuf, bufsize);

	if (error) {
		releasef(fd);
		return (set_errno(error));
	}

	releasef(fd);
	return (osize);
}

#endif	/* _SYSCALL32 || _ILP32 */

int
getdents64(int fd, void *buf, size_t count)
{
	vnode_t *vp;
	file_t *fp;
	struct uio auio;
	struct iovec aiov;
	register int error;
	int sink;

	if (count < sizeof (struct dirent64))
		return (set_errno(EINVAL));

	/*
	 * Don't let the user overcommit kernel resources.
	 */
	if (count > MAXGETDENTS_SIZE)
		count = MAXGETDENTS_SIZE;

	if ((fp = getf(fd)) == NULL)
		return (set_errno(EBADF));
	vp = fp->f_vnode;
	if (vp->v_type != VDIR) {
		releasef(fd);
		return (set_errno(ENOTDIR));
	}
	aiov.iov_base = buf;
	aiov.iov_len = count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_loffset = fp->f_offset;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_resid = count;
	auio.uio_fmode = 0;
	VOP_RWLOCK(vp, 0);
	error = VOP_READDIR(vp, &auio, fp->f_cred, &sink);
	VOP_RWUNLOCK(vp, 0);
	if (error) {
		releasef(fd);
		return (set_errno(error));
	}
	count = count - auio.uio_resid;
	fp->f_offset = auio.uio_loffset;
	releasef(fd);
	return (count);
}
