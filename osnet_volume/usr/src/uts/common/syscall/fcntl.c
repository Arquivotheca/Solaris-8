/* ONC_PLUS EXTRACT START */
/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
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
 * 	Copyright (c) 1986-1989,1994,1996-1997 by Sun Microsystems, Inc.
 * 	Copyright (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)fcntl.c	1.23	99/08/31 SMI"
/* ONC_PLUS EXTRACT END */

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
/* ONC_PLUS EXTRACT START */
#include <sys/flock.h>
/* ONC_PLUS EXTRACT END */
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mode.h>
#include <sys/proc.h>
#include <sys/filio.h>
#include <sys/share.h>
#include <sys/debug.h>

/* ONC_PLUS EXTRACT START */
static int flock_check(vnode_t *, flock64_t *, offset_t, offset_t);
/*
 * File control.
 */
int
fcntl(int fdes, int cmd, intptr_t arg)
{
	int iarg = (int)arg;
	int retval = 0;
	file_t *fp;
	vnode_t *vp;
	u_offset_t offset;
	int flag;
	struct flock sbf;
	struct flock64 bf;
	struct o_flock obf;
	struct flock64_32 bf64_32;
	struct fshare fsh;
	struct shrlock shr;
	struct shr_locowner shr_own;
	offset_t maxoffset = MAXOFF_T;
	model_t datamodel = DATAMODEL_NATIVE;

#if defined(_ILP32) && !defined(lint) && defined(_SYSCALL32)
	ASSERT(sizeof (struct flock) == sizeof (struct flock32));
	ASSERT(sizeof (struct flock64) == sizeof (struct flock64_32));
#endif
#if defined(_LP64) && !defined(lint) && defined(_SYSCALL32)
	ASSERT(sizeof (struct flock) == sizeof (struct flock64_64));
	ASSERT(sizeof (struct flock64) == sizeof (struct flock64_64));
#endif

	if ((fp = getf(fdes)) == NULL)
		return (set_errno(EBADF));
	vp = fp->f_vnode;
	flag = fp->f_flag;
	offset = fp->f_offset;

#if defined(_SYSCALL32_IMPL)
	if ((datamodel = get_udatamodel()) == DATAMODEL_ILP32)
		maxoffset = MAXOFF32_T;
#endif

	switch (cmd) {
/* ONC_PLUS EXTRACT END */

	case F_DUPFD:
		if (iarg < 0 || iarg >= P_CURLIMIT(curproc, RLIMIT_NOFILE)) {
			retval = set_errno(EINVAL);
			break;
		}
		if ((retval = ufalloc(iarg)) == -1) {
			retval = set_errno(EMFILE);
			break;
		}
		mutex_enter(&fp->f_tlock);
		fp->f_count++;
		mutex_exit(&fp->f_tlock);
		setf(retval, fp);
		break;

	case F_DUP2FD:
		if (iarg < 0 || iarg >= P_CURLIMIT(curproc, RLIMIT_NOFILE))
			retval = set_errno(EBADF);
		else if (fdes == iarg)
			retval = iarg;
		else {
			/*
			 * We can't hold our getf(fdes) across the call to
			 * closeandsetf() because it creates a window for
			 * deadlock: if one thread is doing dup2(a, b) while
			 * another is doing dup2(b, a), each one will block
			 * waiting for the other to call releasef().  The
			 * solution is to increment the file reference count
			 * (which we have to do anyway), then releasef(fdes),
			 * then closeandsetf().  Incrementing f_count ensures
			 * that fp won't disappear after we call releasef().
			 */
			mutex_enter(&fp->f_tlock);
			fp->f_count++;
			mutex_exit(&fp->f_tlock);
			releasef(fdes);
			(void) closeandsetf(iarg, fp);
			return (iarg);
		}
		break;

	case F_GETFD:
		retval = f_getfd(fdes);
		break;

	case F_SETFD:
		f_setfd(fdes, (char)iarg);
		break;

	case F_GETFL:
		mutex_enter(&fp->f_tlock);
		retval = fp->f_flag + FOPEN;
		mutex_exit(&fp->f_tlock);
		break;

	case F_SETFL:
		if ((iarg & (FNONBLOCK|FNDELAY)) == (FNONBLOCK|FNDELAY))
			iarg &= ~FNDELAY;
		if (retval = VOP_SETFL(vp, flag, iarg, fp->f_cred)) {
			retval = set_errno(retval);
		} else {
			iarg &= FMASK;
			mutex_enter(&fp->f_tlock);
			fp->f_flag &= (FREAD|FWRITE);
			fp->f_flag |= (iarg - FOPEN) & ~(FREAD|FWRITE);
			mutex_exit(&fp->f_tlock);
		}
		break;

/* ONC_PLUS EXTRACT START */
	/*
	 * The file system and vnode layers understand and implement
	 * locking with flock64 structures. So here once we pass through
	 * the test for compatibility as defined by LFS API, (for F_SETLK,
	 * F_SETLKW, F_GETLK, F_GETLKW, F_FREESP) we transform
	 * the flock structure to a flock64 structure and send it to the
	 * lower layers. Similarly in case of GETLK the returned flock64
	 * structure is transformed to a flock structure if everything fits
	 * in nicely, otherwise we return EOVERFLOW.
	 */

	case F_GETLK:
	case F_O_GETLK:
	case F_SETLK:
	case F_SETLKW:

		/*
		 * Copy in input fields only.
		 */

		if (cmd == F_O_GETLK) {
			if (datamodel != DATAMODEL_ILP32) {
				retval = set_errno(EINVAL);
				break;
			}

			if (copyin((void *)arg, &obf, sizeof (obf))) {
				retval = set_errno(EFAULT);
				break;
			}
			bf.l_type = obf.l_type;
			bf.l_whence = obf.l_whence;
			bf.l_start = (off64_t)obf.l_start;
			bf.l_len = (off64_t)obf.l_len;
			bf.l_sysid = (int)obf.l_sysid;
			bf.l_pid = obf.l_pid;
		} else if (datamodel == DATAMODEL_NATIVE) {
			if (copyin((void *)arg, &sbf, sizeof (sbf))) {
				retval = set_errno(EFAULT);
				break;
			}
			/*
			 * XXX	In an LP64 kernel with an LP64 application
			 *	there's no need to do a structure copy here
			 *	struct flock == struct flock64. However,
			 *	we did it this way to avoid more conditional
			 *	compilation.
			 */
			bf.l_type = sbf.l_type;
			bf.l_whence = sbf.l_whence;
			bf.l_start = (off64_t)sbf.l_start;
			bf.l_len = (off64_t)sbf.l_len;
			bf.l_sysid = sbf.l_sysid;
			bf.l_pid = sbf.l_pid;
		}
#if defined(_SYSCALL32_IMPL)
		else {
			struct flock32 sbf32;
			if (copyin((void *)arg, &sbf32, sizeof (sbf32))) {
				retval = set_errno(EFAULT);
				break;
			}
			bf.l_type = sbf32.l_type;
			bf.l_whence = sbf32.l_whence;
			bf.l_start = (off64_t)sbf32.l_start;
			bf.l_len = (off64_t)sbf32.l_len;
			bf.l_sysid = sbf32.l_sysid;
			bf.l_pid = sbf32.l_pid;
		}
#endif /* _SYSCALL32_IMPL */

		/*
		 * 64-bit support: check for overflow for 32-bit lock ops
		 */
		if ((retval = flock_check(vp, &bf, offset, maxoffset))) {
			retval = set_errno(retval);
			break;
		}

		/*
		 * Not all of the filesystems understand F_O_GETLK, and
		 * there's no need for them to know.  Map it to F_GETLK.
		 */
		if (retval = VOP_FRLOCK(vp, (cmd == F_O_GETLK) ? F_GETLK : cmd,
			&bf, flag, offset, fp->f_cred)) {
			retval = set_errno(retval);
			break;
		}

		/*
		 * If command is GETLK and no lock is found, only
		 * the type field is changed.
		 */
		if ((cmd == F_O_GETLK || cmd == F_GETLK) &&
		    bf.l_type == F_UNLCK) {
			/* l_type always first entry, always a short */
			if (copyout(&bf.l_type, &((struct flock *)arg)->l_type,
			    sizeof (bf.l_type)))
				retval = set_errno(EFAULT);
			break;
		}

		if (cmd == F_O_GETLK) {
			/*
			 * Return an SVR3 flock structure to the user.
			 */
			obf.l_type = (int16_t)bf.l_type;
			obf.l_whence = (int16_t)bf.l_whence;
			obf.l_start = (int32_t)bf.l_start;
			obf.l_len = (int32_t)bf.l_len;
			if (bf.l_sysid > SHRT_MAX || bf.l_pid > SHRT_MAX) {
				/*
				 * One or both values for the above fields
				 * is too large to store in an SVR3 flock
				 * structure.
				 */
				retval = set_errno(EOVERFLOW);
				break;
			}
			obf.l_sysid = (int16_t)bf.l_sysid;
			obf.l_pid = (int16_t)bf.l_pid;
			if (copyout(&obf, (void *)arg, sizeof (obf)))
				retval = set_errno(EFAULT);
		} else if (cmd == F_GETLK) {
			/*
			 * Copy out SVR4 flock.
			 */
			int i;

			if (bf.l_start > maxoffset || bf.l_len > maxoffset) {
				retval = set_errno(EOVERFLOW);
				break;
			}

			if (datamodel == DATAMODEL_NATIVE) {
				for (i = 0; i < 4; i++)
					sbf.l_pad[i] = 0;
				/*
				 * XXX	In an LP64 kernel with an LP64
				 *	application there's no need to do a
				 *	structure copy here as currently
				 *	struct flock == struct flock64.
				 *	We did it this way to avoid more
				 *	conditional compilation.
				 */
				sbf.l_type = bf.l_type;
				sbf.l_whence = bf.l_whence;
				sbf.l_start = (off_t)bf.l_start;
				sbf.l_len = (off_t)bf.l_len;
				sbf.l_sysid = bf.l_sysid;
				sbf.l_pid = bf.l_pid;
				if (copyout(&sbf, (void *)arg, sizeof (sbf)))
					retval = set_errno(EFAULT);
			}
#if defined(_SYSCALL32_IMPL)
			else {
				struct flock32 sbf32;
				if (bf.l_start > MAXOFF32_T ||
				    bf.l_len > MAXOFF32_T) {
					retval = set_errno(EOVERFLOW);
					break;
				}
				for (i = 0; i < 4; i++)
					sbf32.l_pad[i] = 0;
				sbf32.l_type = (int16_t)bf.l_type;
				sbf32.l_whence = (int16_t)bf.l_whence;
				sbf32.l_start = (off32_t)bf.l_start;
				sbf32.l_len = (off32_t)bf.l_len;
				sbf32.l_sysid = (int32_t)bf.l_sysid;
				sbf32.l_pid = (pid32_t)bf.l_pid;
				if (copyout(&sbf32,
				    (void *)arg, sizeof (sbf32)))
					retval = set_errno(EFAULT);
			}
#endif
		}
		break;
/* ONC_PLUS EXTRACT END */

	case F_CHKFL:
		/*
		 * This is for internal use only, to allow the vnode layer
		 * to validate a flags setting before applying it.  User
		 * programs can't issue it.
		 */
		retval = set_errno(EINVAL);
		break;

	case F_ALLOCSP:
	case F_FREESP:
		if ((flag & FWRITE) == 0) {
			retval = set_errno(EBADF);
			break;
		}
		if (vp->v_type != VREG) {
			retval = set_errno(EINVAL);
			break;
		}

#if defined(_ILP32) || defined(_SYSCALL32_IMPL)
		if (datamodel == DATAMODEL_ILP32) {
			struct flock32 sbf32;
			/*
			 * For compatibility we overlay an SVR3 flock on an SVR4
			 * flock.  This works because the input field offsets
			 * in "struct flock" were preserved.
			 */
			if (copyin((void *)arg, &sbf32, sizeof (sbf32))) {
				retval = set_errno(EFAULT);
				break;
			} else {
				bf.l_type = sbf32.l_type;
				bf.l_whence = sbf32.l_whence;
				bf.l_start = (off64_t)sbf32.l_start;
				bf.l_len = (off64_t)sbf32.l_len;
				bf.l_sysid = sbf32.l_sysid;
				bf.l_pid = sbf32.l_pid;
			}
		}
#endif /* _ILP32 || _SYSCALL32_IMPL */

#if defined(_LP64)
		if (datamodel == DATAMODEL_LP64) {
			if (copyin((void *)arg, &bf, sizeof (bf))) {
				retval = set_errno(EFAULT);
				break;
			}
		}
#endif

		if ((retval = flock_check(vp, &bf, offset, maxoffset))) {
			retval = set_errno(retval);
			break;
		}

		retval = VOP_SPACE(vp, cmd, &bf, flag, offset, fp->f_cred);
		if (retval)
			retval = set_errno(retval);
		break;

#if !defined(_LP64) || defined(_SYSCALL32_IMPL)
/* ONC_PLUS EXTRACT START */
	case F_GETLK64:
	case F_SETLK64:
	case F_SETLKW64:
		/*
		 * Large Files: Here we set cmd as *LK and send it to
		 * lower layers. *LK64 is only for the user land.
		 * Most of the comments described above for F_SETLK
		 * applies here too.
		 * Large File support is only needed for ILP32 apps!
		 */
		if (datamodel != DATAMODEL_ILP32) {
			retval = set_errno(EINVAL);
			break;
		}

		if (cmd == F_GETLK64)
			cmd = F_GETLK;
		else if (cmd == F_SETLK64)
			cmd = F_SETLK;
		else if (cmd == F_SETLKW64)
			cmd = F_SETLKW;

		/*
		 * Note that the size of flock64 is different in the ILP32
		 * and LP64 models, due to the sucking l_pad field.
		 * We do not want to assume that the flock64 structure is
		 * laid out in the same in ILP32 and LP64 environments, so
		 * we will copy in the ILP32 version of flock64 explicitly
		 * and copy it to the native flock64 structure.
		 */

		if (copyin((void *)arg, &bf64_32, sizeof (bf64_32))) {
			retval = set_errno(EFAULT);
			break;
		}
		bf.l_type = (short)bf64_32.l_type;
		bf.l_whence = (short)bf64_32.l_whence;
		bf.l_start = bf64_32.l_start;
		bf.l_len = bf64_32.l_len;
		bf.l_sysid = (int)bf64_32.l_sysid;
		bf.l_pid = (pid_t)bf64_32.l_pid;

		if ((retval = flock_check(vp, &bf, offset, MAXOFFSET_T))) {
			retval = set_errno(retval);
			break;
		}

		if (retval =
		    VOP_FRLOCK(vp, cmd, &bf, flag, offset, fp->f_cred)) {
			retval = set_errno(retval);
			break;
		}

		if ((cmd == F_GETLK) && bf.l_type == F_UNLCK) {
			if (copyout(&bf.l_type, &((struct flock *)arg)->l_type,
			    sizeof (bf.l_type)))
				retval = set_errno(EFAULT);
			break;
		}

		if (cmd == F_GETLK) {
			int i;

			/*
			 * We do not want to assume that the flock64 structure
			 * is laid out in the same in ILP32 and LP64
			 * environments, so we will copy out the ILP32 version
			 * of flock64 explicitly after copying the native
			 * flock64 structure to it.
			 */
			for (i = 0; i < 4; i++)
				bf64_32.l_pad[i] = 0;
			bf64_32.l_type = (int16_t)bf.l_type;
			bf64_32.l_whence = (int16_t)bf.l_whence;
			bf64_32.l_start = bf.l_start;
			bf64_32.l_len = bf.l_len;
			bf64_32.l_sysid = (int32_t)bf.l_sysid;
			bf64_32.l_pid = (pid32_t)bf.l_pid;
			if (copyout(&bf64_32, (void *)arg, sizeof (bf64_32))) {
				retval = set_errno(EFAULT);
			}
		}
		break;
/* ONC_PLUS EXTRACT END */

	case F_FREESP64:
		if (datamodel != DATAMODEL_ILP32) {
			retval = set_errno(EINVAL);
			break;
		}
		cmd = F_FREESP;
		if ((flag & FWRITE) == 0)
			retval = EBADF;
		else if (vp->v_type != VREG)
			retval = EINVAL;
		else if (copyin((void *)arg, &bf64_32, sizeof (bf64_32)))
			retval = EFAULT;
		else {
			/*
			 * Note that the size of flock64 is different in
			 * the ILP32 and LP64 models, due to the l_pad field.
			 * We do not want to assume that the flock64 structure
			 * is laid out the same in ILP32 and LP64
			 * environments, so we will copy in the ILP32
			 * version of flock64 explicitly and copy it to
			 * the native flock64 structure.
			 */
			bf.l_type = (short)bf64_32.l_type;
			bf.l_whence = (short)bf64_32.l_whence;
			bf.l_start = bf64_32.l_start;
			bf.l_len = bf64_32.l_len;
			bf.l_sysid = (int)bf64_32.l_sysid;
			bf.l_pid = (pid_t)bf64_32.l_pid;

			if ((retval = flock_check(vp, &bf, offset,
			    MAXOFFSET_T))) {
				retval = set_errno(retval);
				break;
			}

			if (vp->v_type == VREG && bf.l_len == 0 &&
			    bf.l_start > OFFSET_MAX(fp)) {
				retval = set_errno(EFBIG);
				break;
			}
			retval = VOP_SPACE(vp, cmd, &bf, flag, offset,
			    fp->f_cred);
		}
		if (retval)
			retval = set_errno(retval);
		break;
#endif /*  !_LP64 || _SYSCALL32_IMPL */

/* ONC_PLUS EXTRACT START */
	case F_SHARE:
	case F_UNSHARE:

		/*
		 * Copy in input fields only.
		 */
		if (copyin((void *)arg, &fsh, sizeof (fsh))) {
			retval = set_errno(EFAULT);
			break;
		}

		/*
		 * Local share reservations always have this simple form
		 */
		shr.s_access = fsh.f_access;
		shr.s_deny = fsh.f_deny;
		shr.s_sysid = 0;
		shr.s_pid = ttoproc(curthread)->p_pid;
		shr_own.sl_pid = shr.s_pid;
		shr_own.sl_id = fsh.f_id;
		shr.s_own_len = sizeof (shr_own);
		shr.s_owner = (caddr_t)&shr_own;
		if (retval = VOP_SHRLOCK(vp, cmd, &shr, flag)) {
			retval = set_errno(retval);
			break;
/* ONC_PLUS EXTRACT END */
		}
		break;

	default:
		retval = set_errno(EINVAL);
		break;
	}

	releasef(fdes);

	return (retval);
}

int
dup(int fd)
{
	return (fcntl(fd, F_DUPFD, 0));
}

/* ONC_PLUS EXTRACT START */
int
flock_check(vnode_t *vp, flock64_t *flp, offset_t offset, offset_t max)
{
	struct vattr	vattr;
	int	error;
	u_offset_t start, end;

	/*
	 * Determine the starting point of the request
	 */
	switch (flp->l_whence) {
	case 0:		/* SEEK_SET */
		start = (u_offset_t)flp->l_start;
		if (start > max)
			return (EINVAL);
		break;
	case 1:		/* SEEK_CUR */
		if (flp->l_start > (max - offset))
			return (EOVERFLOW);
		start = (u_offset_t)(flp->l_start + offset);
		if (start > max)
			return (EINVAL);
		break;
	case 2:		/* SEEK_END */
		vattr.va_mask = AT_SIZE;
		if (error = VOP_GETATTR(vp, &vattr, 0, CRED()))
			return (error);
		if (flp->l_start > (max - (offset_t)vattr.va_size))
			return (EOVERFLOW);
		start = (u_offset_t)(flp->l_start + (offset_t)vattr.va_size);
		if (start > max)
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	/*
	 * Determine the range covered by the request.
	 */
	if (flp->l_len == 0)
		end = MAXEND;
	else if ((offset_t)flp->l_len > 0) {
		if (flp->l_len > (max - start + 1))
			return (EOVERFLOW);
		end = (u_offset_t)(start + (flp->l_len - 1));
		ASSERT(end <= max);
	} else {
		/*
		 * Negative length; why do we even allow this ?
		 * Because this allows easy specification of
		 * the last n bytes of the file.
		 */
		end = start;
		start += (u_offset_t)flp->l_len;
		(start)++;
		if (start > max)
			return (EINVAL);
		ASSERT(end <= max);
	}
	ASSERT(start <= max);
	if (flp->l_type == F_UNLCK && flp->l_len > 0 &&
	    end == (offset_t)max) {
		flp->l_len = 0;
	}
	if (start  > end)
		return (EINVAL);
	return (0);
}
/* ONC_PLUS EXTRACT END */
