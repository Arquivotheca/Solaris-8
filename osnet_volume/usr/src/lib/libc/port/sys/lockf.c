/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lockf.c 1.15	98/02/27	 SMI"	/* SVr4.0 1.16	*/

/*LINTLIBRARY*/

#include <sys/feature_tests.h>

#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#pragma weak lockf64 = _lockf64
#pragma weak    _libc_lockf64 = _lockf64
#define	lockf64		_lockf64
#else
#pragma weak lockf = _lockf
#pragma weak    _libc_lockf = _lockf
#define	lockf		_lockf
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

int
lockf(int fildes, int function, off_t size)
{
	struct flock l;
	int rv;

	l.l_whence = 1;
	if (size < 0) {
		l.l_start = size;
		l.l_len = -size;
	} else {
		l.l_start = (off_t)0;
		l.l_len = size;
	}
	switch (function) {
	case F_ULOCK:
		l.l_type = F_UNLCK;
		rv = fcntl(fildes, F_SETLK, &l);
		break;
	case F_LOCK:
		l.l_type = F_WRLCK;
		rv = fcntl(fildes, F_SETLKW, &l);
		break;
	case F_TLOCK:
		l.l_type = F_WRLCK;
		rv = fcntl(fildes, F_SETLK, &l);
		break;
	case F_TEST:
		l.l_type = F_WRLCK;
		rv = fcntl(fildes, F_GETLK, &l);
		if (rv != -1) {
			if (l.l_type == F_UNLCK)
				return (0);
			else {
				errno = EAGAIN;
				return (-1);
			}
		}
		break;
	default:
		errno = EINVAL;
		return (-1);
	}
	if (rv < 0) {
		switch (errno) {
		case EMFILE:
		case ENOSPC:
		case ENOLCK:
			/*
			 * A deadlock error is given if we run out of resources,
			 * in compliance with /usr/group standards.
			 */
			errno = EDEADLK;
			break;
		default:
			break;
		}
	}
	return (rv);
}
