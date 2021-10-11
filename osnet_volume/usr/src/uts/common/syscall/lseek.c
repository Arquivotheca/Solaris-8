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

#pragma	ident	"@(#)lseek.c	1.14	98/03/01 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>

/*
 * Seek on a file
 */

#if defined(_SYSCALL32_IMPL) || defined(_ILP32)
/*
 * Workhorse for the 32-bit seek variants: lseek32 and llseek32
 *
 * 'max' represents the maximum possible representation of offset
 * in the data type corresponding to lseek and llseek. It is
 * MAXOFF32_T for off32_t and MAXOFFSET_T for off64_t.
 * We return EOVERFLOW if we cannot represent the resulting offset
 * in the data type.
 * We provide support for character devices to be seeked beyond MAXOFF32_T
 * by lseek. To maintain compatibility in such cases lseek passes
 * the arguments carefully to lseek_common when file is not regular.
 * (/dev/kmem is a good example of a > 2Gbyte seek!)
 */
static int
lseek32_common(file_t *fp, int sbase, offset_t off, offset_t max,
    offset_t *retoff)
{
	vnode_t *vp;
	struct vattr vattr;
	int error;
	u_offset_t noff;
	offset_t curoff, newoff;
	int reg;

	vp = fp->f_vnode;
	reg = (vp->v_type == VREG);

	curoff = fp->f_offset;

	switch (sbase) {
	case 0: /* SEEK_SET */
		noff = (u_offset_t)off;
		if (reg && noff > max) {
			error = EINVAL;
			goto out;
		}
		break;

	case 1: /* SEEK_CUR */
		if (reg && off > (max - curoff)) {
			error = EOVERFLOW;
			goto out;
		}
		noff = (u_offset_t)(off + curoff);
		if (reg && noff > max) {
			error = EINVAL;
			goto out;
		}
		break;

	case 2: /* SEEK_END */
		vattr.va_mask = AT_SIZE;
		if (error = VOP_GETATTR(vp, &vattr, 0, fp->f_cred)) {
			goto out;
		}
		if (reg && (off  > (max - (offset_t)vattr.va_size))) {
			error = EOVERFLOW;
			goto out;
		}
		noff = (u_offset_t)(off + (offset_t)vattr.va_size);
		if (reg && noff > max) {
			error = EINVAL;
			goto out;
		}
		break;

	default:
		error = EINVAL;
		goto out;
	}

	ASSERT((reg && noff <= max) || !reg);
	newoff = (offset_t)noff;
	if ((error = VOP_SEEK(vp, curoff, &newoff)) == 0) {
		fp->f_offset = newoff;
		(*retoff) = newoff;
		return (0);
	}
out:
	return (error);
}

off32_t
lseek32(int32_t fdes, off32_t off, int32_t sbase)
{
	file_t *fp;
	int error;
	offset_t retoff;

	if ((fp = getf(fdes)) == NULL)
		return ((off32_t)set_errno(EBADF));

	/*
	 * lseek32 returns EOVERFLOW if we cannot represent the resulting
	 * offset from seek in a 32-bit off_t.
	 * The following routines are sensitive to sign extensions and
	 * calculations and if ever you change this make sure it works for
	 * special files.
	 *
	 * When VREG is not set we do the check for sbase != SEEK_SET
	 * to send the unsigned value to lseek_common and not the sign
	 * extended value. (The maximum representable value is not
	 * checked by lseek_common for special files.)
	 */
	if (fp->f_vnode->v_type == VREG || sbase != 0)
		error = lseek32_common(fp, sbase, (offset_t)off,
		    (offset_t)MAXOFF32_T, &retoff);
	else if (sbase == 0)
		error = lseek32_common(fp, sbase, (offset_t)(u_int)off,
		    (offset_t)(u_int)UINT_MAX, &retoff);

	releasef(fdes);
	if (!error)
		return ((off32_t)retoff);
	return ((off32_t)set_errno(error));
}

/*
 * 64-bit seeks from 32-bit applications
 */
offset_t
llseek32(int32_t fdes, uint32_t off1, uint32_t off2, int sbase)
{
	file_t *fp;
	int error;
	offset_t retoff;
#if defined(_LITTLE_ENDIAN)
	offset_t off = ((u_offset_t)off2 << 32) | (u_offset_t)off1;
#else
	offset_t off = ((u_offset_t)off1 << 32) | (u_offset_t)off2;
#endif

	if ((fp = getf(fdes)) == NULL)
		error = EBADF;
	else {
		error = lseek32_common(fp, sbase, off, MAXOFFSET_T, &retoff);
		releasef(fdes);
	}

	return (error ? (offset_t)set_errno(error) : retoff);
}
#endif	/* _SYSCALL32_IMPL || _ILP32 */

#ifdef _LP64
/*
 * Seek on a file.
 *
 * Life is almost simple again (at least until we do 128-bit files ;-)
 * This is both 'lseek' and 'llseek' to a 64-bit application.
 *
 * XX64	Need to check seeks into top part of 64-bit address space
 *	via /dev/kmem
 */
off_t
lseek64(int fdes, off_t off, int sbase)
{
	file_t *fp;
	vnode_t *vp;
	struct vattr vattr;
	int error;
	off_t old_off;
	offset_t new_off;

	if ((fp = getf(fdes)) == NULL)
		return ((off_t)set_errno(EBADF));

	vp = fp->f_vnode;
	new_off = off;

	switch (sbase) {
	case 1:	/* SEEK_CUR */
		new_off += fp->f_offset;
		break;

	case 2:	/* SEEK_END */
		vattr.va_mask = AT_SIZE;
		if ((error = VOP_GETATTR(vp, &vattr, 0, fp->f_cred)) != 0)
			goto lseek64error;
		new_off += vattr.va_size;
		break;

	case 0:	/* SEEK_SET */
		break;

	default:
		error = EINVAL;
		goto lseek64error;
	}

	old_off = fp->f_offset;
	if ((error = VOP_SEEK(vp, old_off, &new_off)) == 0) {
		fp->f_offset = new_off;
		releasef(fdes);
		return (new_off);
	}

lseek64error:
	releasef(fdes);
	return ((off_t)set_errno(error));
}
#endif	/* _LP64 */
