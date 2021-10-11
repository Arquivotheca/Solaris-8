/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989 Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#pragma ident	"@(#)flock.c	1.3	97/06/16 SMI"

/*LINTLIBRARY*/

#include <sys/types.h>
#include <sys/file.h>
#include <fcntl.h>
#include <errno.h>

int
flock(int fd, int operation)
{
	struct flock	fl;
	int	cmd;
	int	ret;

	/* initialize the flock struct to set lock on entire file */
	fl.l_whence = 0;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_type = 0;

	/* In non-blocking lock, use F_SETLK for cmd, F_SETLKW otherwise */
	if (operation & LOCK_NB) {
		cmd = F_SETLK;
		operation &= ~LOCK_NB;	/* turn off this bit */
	} else
		cmd = F_SETLKW;

	switch (operation) {
	case LOCK_UN:
		fl.l_type |= F_UNLCK;
		break;
	case LOCK_SH:
		fl.l_type |= F_RDLCK;
		break;
	case LOCK_EX:
		fl.l_type |= F_WRLCK;
		break;
	default:
		errno = EINVAL;
		return (-1);
	}

	ret = fcntl(fd, cmd, &fl);

	if (ret == -1 && errno == EACCES)
		errno = EWOULDBLOCK;

	return (ret);
}
