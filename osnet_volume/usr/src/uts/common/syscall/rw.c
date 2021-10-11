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

#pragma ident	"@(#)rw.c	1.26	98/06/30 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/cpuvar.h>
#include <sys/uio.h>
#include <sys/ioreq.h>
#include <sys/debug.h>

/*
 * read, write, pread, pwrite, readv, and writev syscalls.
 *
 * 64-bit open:	all open's are large file opens.
 * Large Files: the behaviour of read depends on whether the fd
 *		corresponds to large open or not.
 * 32-bit open:	FOFFMAX flag not set.
 *		read until MAXOFF32_T - 1 and read at MAXOFF32_T returns
 *		EOVERFLOW if count is non-zero and if size of file
 *		is > MAXOFF32_T. If size of file is <= MAXOFF32_T read
 *		at >= MAXOFF32_T returns EOF.
 */

/*
 * Native system call
 */
ssize_t
read(int fdes, void *cbuf, size_t count)
{
	struct uio auio;
	struct iovec aiov;
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag;
	ssize_t cnt, bcount;
	int error = 0;
	u_offset_t fileoff;

	if ((cnt = (ssize_t)count) < 0)
		return (set_errno(EINVAL));
	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & FREAD) == 0) {
		error = EBADF;
		goto out;
	}
	vp = fp->f_vnode;

	if (vp->v_type == VREG && cnt == 0) {
		goto out;
	}

	rwflag = 0;
	aiov.iov_base = cbuf;
	aiov.iov_len = cnt;
	VOP_RWLOCK(vp, rwflag);

	/*
	 * We do the following checks inside VOP_RWLOCK so as to
	 * prevent file size from changing while these checks are
	 * being done. Also, we load fp's offset to the local
	 * variable fileoff because we can have a parallel lseek
	 * going on (f_offset is not protected by any lock) which
	 * could change f_offset. We need to see the value only
	 * once here and take a decision. Seeing it more than once
	 * can lead to incorrect functionality.
	 */

	fileoff = (u_offset_t)fp->f_offset;
	if (fileoff >= OFFSET_MAX(fp) && (vp->v_type == VREG)) {
		struct vattr va;
		va.va_mask = AT_SIZE;
		if ((error = VOP_GETATTR(vp, &va, 0, fp->f_cred)))  {
			VOP_RWUNLOCK(vp, rwflag);
			goto out;
		}
		if (fileoff >= va.va_size) {
			cnt = 0;
			VOP_RWUNLOCK(vp, rwflag);
			goto out;
		} else {
			error = EOVERFLOW;
			VOP_RWUNLOCK(vp, rwflag);
			goto out;
		}
	}
	if ((vp->v_type == VREG) &&
	    (fileoff + cnt > OFFSET_MAX(fp))) {
		cnt = (ssize_t)(OFFSET_MAX(fp) - fileoff);
	}
	auio.uio_loffset = fileoff;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = bcount = cnt;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = P_CURLIMIT(curproc, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	/* If read sync is not asked for, filter sync flags */
	if ((ioflag & FRSYNC) == 0)
		ioflag &= ~(FSYNC|FDSYNC);
	error = VOP_READ(vp, &auio, ioflag, fp->f_cred);
	cnt -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.sysread, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.readch, (u_long)cnt);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (u_long)cnt;

	if (vp->v_type == VFIFO)	/* Backward compatibility */
		fp->f_offset = cnt;
	else if (((fp->f_flag & FAPPEND) == 0) ||
		(vp->v_type != VREG) || (bcount != 0))	/* POSIX */
		fp->f_offset = auio.uio_loffset;
	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && cnt != 0)
		error = 0;
out:
	releasef(fdes);
	if (error)
		return (set_errno(error));
	return (cnt);
}

/*
 * Native system call
 */
ssize_t
write(int fdes, void *cbuf, size_t count)
{
	struct uio auio;
	struct iovec aiov;
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag;
	ssize_t cnt, bcount;
	int error = 0;
	u_offset_t fileoff;

	if ((cnt = (ssize_t)count) < 0)
		return (set_errno(EINVAL));
	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & FWRITE) == 0) {
		error = EBADF;
		goto out;
	}
	vp = fp->f_vnode;

	if (vp->v_type == VREG && cnt == 0) {
		goto out;
	}

	rwflag = 1;
	aiov.iov_base = cbuf;
	aiov.iov_len = cnt;

	VOP_RWLOCK(vp, rwflag);

	fileoff = fp->f_offset;
	if (vp->v_type == VREG) {

		/*
		 * We raise psignal if write for >0 bytes causes
		 * it to exceed the ulimit.
		 */
		if (fileoff >= P_CURLIMIT(curproc, RLIMIT_FSIZE)) {
			VOP_RWUNLOCK(vp, rwflag);
			psignal(ttoproc(curthread), SIGXFSZ);
			error = EFBIG;
			goto out;
		}
		/*
		 * We return EFBIG if write is done at an offset
		 * greater than the offset maximum for this file structure.
		 */

		if (fileoff >= OFFSET_MAX(fp)) {
			VOP_RWUNLOCK(vp, rwflag);
			error = EFBIG;
			goto out;
		}
		/*
		 * Limit the bytes to be written  upto offset maximum for
		 * this open file structure.
		 */
		if (fileoff + cnt > OFFSET_MAX(fp))
			cnt = (ssize_t)(OFFSET_MAX(fp) - fileoff);
	}
	auio.uio_loffset = fileoff;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = bcount = cnt;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = P_CURLIMIT(curproc, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	error = VOP_WRITE(vp, &auio, ioflag, fp->f_cred);
	cnt -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.syswrite, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.writech, (u_long)cnt);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (u_long)cnt;

	if (vp->v_type == VFIFO)	/* Backward compatibility */
		fp->f_offset = cnt;
	else if (((fp->f_flag & FAPPEND) == 0) ||
		(vp->v_type != VREG) || (bcount != 0))	/* POSIX */
		fp->f_offset = auio.uio_loffset;
	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && cnt != 0)
		error = 0;
out:
	releasef(fdes);
	if (error)
		return (set_errno(error));
	return (cnt);
}

ssize_t
pread(int fdes, void *cbuf, size_t count, off_t offset)
{
	struct uio auio;
	struct iovec aiov;
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag;
	ssize_t bcount;
	int error = 0;
	u_offset_t fileoff = (u_offset_t)(ulong_t)offset;
#ifdef _SYSCALL32_IMPL
	u_offset_t maxoff = get_udatamodel() == DATAMODEL_ILP32 ?
		MAXOFF32_T : MAXOFFSET_T;
#else
	const u_offset_t maxoff = MAXOFF32_T;
#endif

	if ((bcount = (ssize_t)count) < 0)
		return (set_errno(EINVAL));

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & (FREAD)) == 0) {
		error = EBADF;
		goto out;
	}

	rwflag = 0;
	vp = fp->f_vnode;

	if (vp->v_type == VREG) {

		if (bcount == 0)
			goto out;

		/*
		 * Return EINVAL if an invalid offset comes to pread.
		 * Negative offset from user will cause this error.
		 */

		if (fileoff > maxoff) {
			error = EINVAL;
			goto out;
		}
		/*
		 * Limit offset such that we don't read or write
		 * a file beyond the maximum offset representable in
		 * an off_t structure.
		 */
		if (fileoff + bcount > maxoff)
			bcount = (ssize_t)((offset_t)maxoff - fileoff);
	} else if (vp->v_type == VFIFO) {
		error = ESPIPE;
		goto out;
	}

	aiov.iov_base = cbuf;
	aiov.iov_len = bcount;
	VOP_RWLOCK(vp, rwflag);
	if (vp->v_type == VREG && fileoff == (u_offset_t)maxoff) {
		struct vattr va;
		va.va_mask = AT_SIZE;
		if ((error = VOP_GETATTR(vp, &va, 0, fp->f_cred))) {
			VOP_RWUNLOCK(vp, rwflag);
			goto out;
		}
		VOP_RWUNLOCK(vp, rwflag);

		/*
		 * We have to return EOF if fileoff is >= file size.
		 */
		if (fileoff >= va.va_size) {
			bcount = 0;
			goto out;
		}

		/*
		 * File is greater than or equal to maxoff and therefore
		 * we return EOVERFLOW.
		 */
		error = EOVERFLOW;
		goto out;
	}
	auio.uio_loffset = fileoff;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = bcount;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = P_CURLIMIT(curproc, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	/* If read sync is not asked for, filter sync flags */
	if ((ioflag & FRSYNC) == 0)
		ioflag &= ~(FSYNC|FDSYNC);
	error = VOP_READ(vp, &auio, ioflag, fp->f_cred);
	bcount -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.sysread, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.readch, (u_long)bcount);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (u_long)bcount;
	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && bcount != 0)
		error = 0;
out:
	releasef(fdes);
	if (error)
		return (set_errno(error));
	return (bcount);
}

ssize_t
pwrite(int fdes, void *cbuf, size_t count, off_t offset)
{
	struct uio auio;
	struct iovec aiov;
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag;
	ssize_t bcount;
	int error = 0;
	u_offset_t fileoff = (u_offset_t)(ulong_t)offset;
#ifdef _SYSCALL32_IMPL
	u_offset_t maxoff = get_udatamodel() == DATAMODEL_ILP32 ?
		MAXOFF32_T : MAXOFFSET_T;
#else
	const u_offset_t maxoff = MAXOFF32_T;
#endif

	if ((bcount = (ssize_t)count) < 0)
		return (set_errno(EINVAL));
	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & (FWRITE)) == 0) {
		error = EBADF;
		goto out;
	}

	rwflag = 1;
	vp = fp->f_vnode;

	if (vp->v_type == VREG) {

		if (bcount == 0)
			goto out;

		/*
		 * return EINVAL for offsets that cannot be
		 * represented in an off_t.
		 */
		if (fileoff > maxoff) {
			error = EINVAL;
			goto out;
		}
		/*
		 * Raise signal if we are trying to write above
		 * the resource limit.
		 */
		if (fileoff >= P_CURLIMIT(curproc, RLIMIT_FSIZE)) {
			psignal(ttoproc(curthread), SIGXFSZ);
			error = EFBIG;
			goto out;
		}
		/*
		 * Don't allow pwrite to cause file sizes to exceed
		 * maxoff.
		 */
		if (fileoff == maxoff) {
			error = EFBIG;
			goto out;
		}
		if (fileoff + count > maxoff)
			bcount = (ssize_t)((u_offset_t)maxoff - fileoff);
	} else if (vp->v_type == VFIFO) {
		error = ESPIPE;
		goto out;
	}

	aiov.iov_base = cbuf;
	aiov.iov_len = bcount;
	VOP_RWLOCK(vp, rwflag);
	auio.uio_loffset = fileoff;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = bcount;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = P_CURLIMIT(curproc, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	error = VOP_WRITE(vp, &auio, ioflag, fp->f_cred);
	bcount -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.syswrite, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.writech, (u_long)bcount);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (u_long)bcount;
	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && bcount != 0)
		error = 0;
out:
	releasef(fdes);
	if (error)
		return (set_errno(error));
	return (bcount);
}

/*
 * XXX -- The SVID refers to IOV_MAX, but doesn't define it.  Grrrr....
 * XXX -- However, SVVS expects readv() and writev() to fail if
 * XXX -- iovcnt > 16 (yes, it's hard-coded in the SVVS source),
 * XXX -- so I guess that's the "interface".
 */
#define	DEF_IOV_MAX	16

ssize_t
readv(int fdes, struct iovec *iovp, int iovcnt)
{
	struct uio auio;
	struct iovec aiov[DEF_IOV_MAX];
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag;
	ssize_t count, bcount;
	int error = 0;
	int i;
	u_offset_t fileoff;

	if (iovcnt <= 0 || iovcnt > DEF_IOV_MAX)
		return (set_errno(EINVAL));

#ifdef _SYSCALL32_IMPL
	/*
	 * 32-bit callers need to have their iovec expanded,
	 * while ensuring that they can't move more than 2Gbytes
	 * of data in a single call.
	 */
	if (get_udatamodel() == DATAMODEL_ILP32) {
		struct iovec32 aiov32[DEF_IOV_MAX];
		ssize32_t count32;

		if (copyin(iovp, aiov32, iovcnt * sizeof (struct iovec32)))
			return (set_errno(EFAULT));

		count32 = 0;
		for (i = 0; i < iovcnt; i++) {
			ssize32_t iovlen32 = aiov32[i].iov_len;
			count32 += iovlen32;
			if (iovlen32 < 0 || count32 < 0)
				return (set_errno(EINVAL));
			aiov[i].iov_len = iovlen32;
			aiov[i].iov_base = (caddr_t)aiov32[i].iov_base;
		}
	} else
#endif
	if (copyin(iovp, aiov, iovcnt * sizeof (struct iovec)))
		return (set_errno(EFAULT));

	count = 0;
	for (i = 0; i < iovcnt; i++) {
		ssize_t iovlen = aiov[i].iov_len;
		count += iovlen;
		if (iovlen < 0 || count < 0)
			return (set_errno(EINVAL));
	}
	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & FREAD) == 0) {
		error = EBADF;
		goto out;
	}
	vp = fp->f_vnode;
	if (vp->v_type == VREG && count == 0) {
		goto out;
	}

	rwflag = 0;
	VOP_RWLOCK(vp, rwflag);
	fileoff = fp->f_offset;

	/*
	 * Behaviour is same as read. Please see comments in read.
	 */

	if ((vp->v_type == VREG) && (fileoff >= OFFSET_MAX(fp))) {
		struct vattr va;
		va.va_mask = AT_SIZE;
		if ((error = VOP_GETATTR(vp, &va, 0, fp->f_cred)))  {
			VOP_RWUNLOCK(vp, rwflag);
			goto out;
		}
		if (fileoff >= va.va_size) {
			VOP_RWUNLOCK(vp, rwflag);
			count = 0;
			goto out;
		} else {
			VOP_RWUNLOCK(vp, rwflag);
			error = EOVERFLOW;
			goto out;
		}
	}
	if ((vp->v_type == VREG) && (fileoff + count > OFFSET_MAX(fp))) {
		count = (ssize_t)(OFFSET_MAX(fp) - fileoff);
	}
	auio.uio_loffset = fileoff;
	auio.uio_iov = aiov;
	auio.uio_iovcnt = iovcnt;
	auio.uio_resid = bcount = count;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = P_CURLIMIT(curproc, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	/* If read sync is not asked for, filter sync flags */
	if ((ioflag & FRSYNC) == 0)
		ioflag &= ~(FSYNC|FDSYNC);
	error = VOP_READ(vp, &auio, ioflag, fp->f_cred);
	count -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.sysread, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.readch, (u_long)count);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (u_long)count;

	if (vp->v_type == VFIFO)	/* Backward compatibility */
		fp->f_offset = count;
	else if (((fp->f_flag & FAPPEND) == 0) ||
		(vp->v_type != VREG) || (bcount != 0))	/* POSIX */
		fp->f_offset = auio.uio_loffset;

	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && count != 0)
		error = 0;
out:
	releasef(fdes);
	if (error)
		return (set_errno(error));
	return (count);
}

ssize_t
writev(int fdes, struct iovec *iovp, int iovcnt)
{
	struct uio auio;
	struct iovec aiov[DEF_IOV_MAX];
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag;
	ssize_t count, bcount;
	int error = 0;
	int i;
	u_offset_t fileoff;

	if (iovcnt <= 0 || iovcnt > DEF_IOV_MAX)
		return (set_errno(EINVAL));

#ifdef _SYSCALL32_IMPL
	/*
	 * 32-bit callers need to have their iovec expanded,
	 * while ensuring that they can't move more than 2Gbytes
	 * of data in a single call.
	 */
	if (get_udatamodel() == DATAMODEL_ILP32) {
		struct iovec32 aiov32[DEF_IOV_MAX];
		ssize32_t count32;

		if (copyin(iovp, aiov32, iovcnt * sizeof (struct iovec32)))
			return (set_errno(EFAULT));

		count32 = 0;
		for (i = 0; i < iovcnt; i++) {
			ssize32_t iovlen = aiov32[i].iov_len;
			count32 += iovlen;
			if (iovlen < 0 || count32 < 0)
				return (set_errno(EINVAL));
			aiov[i].iov_len = iovlen;
			aiov[i].iov_base = (caddr_t)aiov32[i].iov_base;
		}
	} else
#endif
	if (copyin(iovp, aiov, iovcnt * sizeof (struct iovec)))
		return (set_errno(EFAULT));

	count = 0;
	for (i = 0; i < iovcnt; i++) {
		ssize_t iovlen = aiov[i].iov_len;
		count += iovlen;
		if (iovlen < 0 || count < 0)
			return (set_errno(EINVAL));
	}
	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & FWRITE) == 0) {
		error = EBADF;
		goto out;
	}
	vp = fp->f_vnode;
	if (vp->v_type == VREG && count == 0) {
		goto out;
	}

	rwflag = 1;

	VOP_RWLOCK(vp, rwflag);

	fileoff = fp->f_offset;

	/*
	 * Behaviour is same as write. Please see comments for write.
	 */

	if (vp->v_type == VREG) {
		if (fileoff >= P_CURLIMIT(curproc, RLIMIT_FSIZE)) {
			psignal(ttoproc(curthread), SIGXFSZ);
			VOP_RWUNLOCK(vp, rwflag);
			error = EFBIG;
			goto out;
		}
		if (fileoff >= OFFSET_MAX(fp)) {
			VOP_RWUNLOCK(vp, rwflag);
			error = EFBIG;
			goto out;
		}
		if (fileoff + count > OFFSET_MAX(fp))
			count = (ssize_t)(OFFSET_MAX(fp) - fileoff);
	}
	auio.uio_loffset = fileoff;
	auio.uio_iov = aiov;
	auio.uio_iovcnt = iovcnt;
	auio.uio_resid = bcount = count;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = P_CURLIMIT(curproc, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	error = VOP_WRITE(vp, &auio, ioflag, fp->f_cred);
	count -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.syswrite, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.writech, (u_long)count);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (u_long)count;

	if (vp->v_type == VFIFO)	/* Backward compatibility */
		fp->f_offset = count;
	else if (((fp->f_flag & FAPPEND) == 0) ||
		(vp->v_type != VREG) || (bcount != 0))	/* POSIX */
		fp->f_offset = auio.uio_loffset;
	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && count != 0)
		error = 0;
out:
	releasef(fdes);
	if (error)
		return (set_errno(error));
	return (count);
}

#if defined(_SYSCALL32_IMPL) || defined(_ILP32)

/*
 * This syscall supplies 64-bit file offsets to 32-bit applications only.
 */
ssize32_t
pread64(int fdes, void *cbuf, size32_t count, uint32_t offset_1,
    uint32_t offset_2)
{
	struct uio auio;
	struct iovec aiov;
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag;
	ssize_t bcount;
	int error = 0;
	u_offset_t fileoff;

#if defined(_LITTLE_ENDIAN)
	fileoff = ((u_offset_t)offset_2 << 32) | (u_offset_t)offset_1;
#else
	fileoff = ((u_offset_t)offset_1 << 32) | (u_offset_t)offset_2;
#endif

	if ((bcount = (ssize_t)count) < 0 || bcount > INT32_MAX)
		return (set_errno(EINVAL));

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & (FREAD)) == 0) {
		error = EBADF;
		goto out;
	}

	rwflag = 0;
	vp = fp->f_vnode;

	if (vp->v_type == VREG) {

		if (bcount == 0)
			goto out;

		/*
		 * Same as pread. See comments in pread.
		 */

		if (fileoff > MAXOFFSET_T) {
			error = EINVAL;
			goto out;
		}
		if (fileoff + bcount > MAXOFFSET_T)
			bcount = (ssize_t)(MAXOFFSET_T - fileoff);
	} else if (vp->v_type == VFIFO) {
		error = ESPIPE;
		goto out;
	}

	aiov.iov_base = cbuf;
	aiov.iov_len = bcount;
	VOP_RWLOCK(vp, rwflag);
	auio.uio_loffset = fileoff;

	/*
	 * Note: File size can never be greater than MAXOFFSET_T.
	 * If ever we start supporting 128 bit files the code
	 * similar to the one in pread at this place should be here.
	 * Here we avoid the unnecessary VOP_GETATTR() when we
	 * know that fileoff == MAXOFFSET_T implies that it is always
	 * greater than or equal to file size.
	 */
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = bcount;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = P_CURLIMIT(curproc, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	/* If read sync is not asked for, filter sync flags */
	if ((ioflag & FRSYNC) == 0)
		ioflag &= ~(FSYNC|FDSYNC);
	error = VOP_READ(vp, &auio, ioflag, fp->f_cred);
	bcount -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.sysread, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.readch, (u_long)bcount);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (u_long)bcount;
	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && bcount != 0)
		error = 0;
out:
	releasef(fdes);
	if (error)
		return (set_errno(error));
	return (bcount);
}

/*
 * This syscall supplies 64-bit file offsets to 32-bit applications only.
 */
ssize32_t
pwrite64(int fdes, void *cbuf, size32_t count, uint32_t offset_1,
    uint32_t offset_2)
{
	struct uio auio;
	struct iovec aiov;
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag;
	ssize_t bcount;
	int error = 0;
	u_offset_t fileoff;

#if defined(_LITTLE_ENDIAN)
	fileoff = ((u_offset_t)offset_2 << 32) | (u_offset_t)offset_1;
#else
	fileoff = ((u_offset_t)offset_1 << 32) | (u_offset_t)offset_2;
#endif

	if ((bcount = (ssize_t)count) < 0 || bcount > INT32_MAX)
		return (set_errno(EINVAL));
	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & (FWRITE)) == 0) {
		error = EBADF;
		goto out;
	}

	rwflag = 1;
	vp = fp->f_vnode;

	if (vp->v_type == VREG) {

		if (bcount == 0)
			goto out;

		/*
		 * See comments in pwrite.
		 */
		if (fileoff > MAXOFFSET_T) {
			error = EINVAL;
			goto out;
		}
		if (fileoff >= P_CURLIMIT(curproc, RLIMIT_FSIZE)) {
			psignal(ttoproc(curthread), SIGXFSZ);
			error = EFBIG;
			goto out;
		}
		if (fileoff == MAXOFFSET_T) {
			error = EFBIG;
			goto out;
		}
		if (fileoff + bcount > MAXOFFSET_T)
			bcount = (ssize_t)((u_offset_t)MAXOFFSET_T - fileoff);
	} else if (vp->v_type == VFIFO) {
		error = ESPIPE;
		goto out;
	}

	aiov.iov_base = cbuf;
	aiov.iov_len = bcount;
	VOP_RWLOCK(vp, rwflag);
	auio.uio_loffset = fileoff;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = bcount;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = P_CURLIMIT(curproc, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	error = VOP_WRITE(vp, &auio, ioflag, fp->f_cred);
	bcount -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.syswrite, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.writech, (u_long)bcount);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (u_long)bcount;
	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && bcount != 0)
		error = 0;
out:
	releasef(fdes);
	if (error)
		return (set_errno(error));
	return (bcount);
}

#endif	/* _SYSCALL32_IMPL || _ILP32 */

#ifdef _SYSCALL32_IMPL

ssize32_t
read32(int32_t fdes, caddr32_t cbuf, size32_t count)
{
	if (count > INT32_MAX)
		return (set_errno(EINVAL));
	return ((ssize32_t)read(fdes, (void *)cbuf, (size_t)count));
}

ssize32_t
write32(int32_t fdes, caddr32_t cbuf, size32_t count)
{
	if (count > INT32_MAX)
		return (set_errno(EINVAL));
	return ((ssize32_t)write(fdes, (void *)cbuf, (size_t)count));
}

ssize32_t
pread32(int32_t fdes, caddr32_t cbuf, size32_t count, off32_t offset)
{
	if (count > INT32_MAX)
		return (set_errno(EINVAL));
	return ((ssize32_t)pread(fdes,
	    (void *)cbuf, (size_t)count, (off_t)(uint32_t)offset));
}

ssize32_t
pwrite32(int32_t fdes, caddr32_t cbuf, size32_t count, off32_t offset)
{
	if (count > INT32_MAX)
		return (set_errno(EINVAL));
	return ((ssize32_t)pwrite(fdes,
	    (void *)cbuf, (size_t)count, (off_t)(uint32_t)offset));
}

ssize32_t
readv32(int32_t fdes, caddr32_t iovp, int32_t iovcnt)
{
	return ((ssize32_t)readv(fdes, (void *)iovp, iovcnt));
}

ssize32_t
writev32(int32_t fdes, caddr32_t iovp, int32_t iovcnt)
{
	return ((ssize32_t)writev(fdes, (void *)iovp, iovcnt));
}

#endif	/* _SYSCALL32_IMPL */
