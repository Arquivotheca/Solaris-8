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
 * 	(c) 1986, 1987, 1988, 1989  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *			All rights reserved.
 *
 */

#pragma	ident	"@(#)utime.c	1.4	97/08/12 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/time.h>
#include <sys/debug.h>
#include <sys/model.h>

extern int	namesetattr(char *, enum symfollow, vattr_t *, int);
extern int	fdsetattr(int, vattr_t *);

/*
 * Set access/modify times on named file.
 */
int
utime(char *fname, time_t *tptr)
{
	time_t tv[2];
	struct vattr vattr;
	int flags = 0;

	if (tptr != NULL) {
		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (copyin(tptr, tv, sizeof (tv)))
				return (set_errno(EFAULT));
		} else {
			time32_t tv32[2];

			if (copyin(tptr, &tv32, sizeof (tv32)))
				return (set_errno(EFAULT));

			tv[0] = (time_t)tv32[0];
			tv[1] = (time_t)tv32[1];
		}

		flags |= ATTR_UTIME;
	} else {
		tv[0] = hrestime.tv_sec;
		tv[1] = tv[0];
	}
	vattr.va_atime.tv_sec = tv[0];
	vattr.va_atime.tv_nsec = 0;
	vattr.va_mtime.tv_sec = tv[1];
	vattr.va_mtime.tv_nsec = 0;
	vattr.va_mask = AT_ATIME|AT_MTIME;
	return (namesetattr(fname, FOLLOW, &vattr, flags));
}

/*
 * SunOS4.1 Buyback:
 * Set access/modify time on named file, with hi res timer
 */
int
utimes(char *fname, struct timeval *tvptr)
{
	struct timeval tv[2];
	struct vattr vattr;
	int flags = 0;

	if (tvptr != NULL) {
		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (copyin(tvptr, tv, sizeof (tv)))
				return (set_errno(EFAULT));
		} else {
			struct timeval32 tv32[2];

			if (copyin(tvptr, tv32, sizeof (tv32)))
				return (set_errno(EFAULT));

			TIMEVAL32_TO_TIMEVAL(&tv[0], &tv32[0]);
			TIMEVAL32_TO_TIMEVAL(&tv[1], &tv32[1]);
		}

		if (tv[0].tv_usec < 0 || tv[0].tv_usec >= 1000000 ||
		    tv[1].tv_usec < 0 || tv[1].tv_usec >= 1000000)
			return (set_errno(EINVAL));

		vattr.va_atime.tv_sec = tv[0].tv_sec;
		vattr.va_atime.tv_nsec = tv[0].tv_usec * 1000;
		vattr.va_mtime.tv_sec = tv[1].tv_sec;
		vattr.va_mtime.tv_nsec = tv[1].tv_usec * 1000;
		flags |= ATTR_UTIME;
	} else {
		vattr.va_atime = hrestime;
		vattr.va_mtime = hrestime;
	}
	vattr.va_mask = AT_ATIME | AT_MTIME;
	return (namesetattr(fname, FOLLOW, &vattr, flags));
}
