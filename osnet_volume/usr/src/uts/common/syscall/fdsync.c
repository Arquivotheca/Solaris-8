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

#ident	"@(#)fdsync.c	1.4	98/03/01 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mode.h>
#include <sys/debug.h>

/*
 * Flush output pending for file.
 */
int
fdsync(int fd, int flag)
{
	file_t *fp;
	register int error;
	int syncflag;

	if ((fp = getf(fd)) != NULL) {
		/*
		 * This flag will determine the file sync
		 * or data sync.
		 * FSYNC : file sync
		 * FDSYNC : data sync
		 */
		syncflag = flag & (FSYNC|FDSYNC);

		if (error = VOP_FSYNC(fp->f_vnode, syncflag, fp->f_cred))
			(void) set_errno(error);
		releasef(fd);
	} else
		error = set_errno(EBADF);
	return (error);
}
