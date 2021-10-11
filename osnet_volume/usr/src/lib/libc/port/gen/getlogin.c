/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 *	Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)getlogin.c	1.22	98/11/17 SMI"	/* SVr4.0 1.16  */

/*LINTLIBRARY*/

#pragma weak getlogin = _getlogin

#ifdef _REENTRANT
#pragma weak getlogin_r = _getlogin_r
#endif /* _REENTRANT */

#include "synonyms.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "utmpx.h"
#include <unistd.h>
#include <errno.h>
#include <thread.h>
#include <synch.h>
#include <mtlib.h>

/*
 * XXX - _POSIX_LOGIN_NAME_MAX limits the length of a login name.  The utmpx
 * interface provides for a 32 character login name, but for the sake of
 * compatibility, we are still using the old utmp-imposed limit.
 */

/*
 * POSIX.1c Draft-6 version of the function getlogin_r.
 * It was implemented by Solaris 2.3.
 */
char *
_getlogin_r(char *answer, int namelen)
{
	int		uf;
	off64_t		me;
	struct futmpx	ubuf;

	if (namelen < _POSIX_LOGIN_NAME_MAX) {
		errno = ERANGE;
		return (NULL);
	}

	if ((me = (off64_t)ttyslot()) < 0)
		return (NULL);
	if ((uf = open64(UTMPX_FILE, 0)) < 0)
		return (NULL);
	(void) lseek64(uf, me * sizeof (ubuf), SEEK_SET);
	if (read(uf, &ubuf, sizeof (ubuf)) != sizeof (ubuf)) {
		(void) close(uf);
		return (NULL);
	}
	(void) close(uf);
	if (ubuf.ut_user[0] == '\0')
		return (NULL);
	(void) strncpy(&answer[0], &ubuf.ut_user[0],
	    _POSIX_LOGIN_NAME_MAX - 1);
	answer[_POSIX_LOGIN_NAME_MAX - 1] = '\0';
	return (&answer[0]);
}

/*
 * POSIX.1c standard version of the function getlogin_r.
 * User gets it via static getlogin_r from the header file.
 */
int
__posix_getlogin_r(char *name, int namelen)
{
	int nerrno = 0;
	int oerrno = errno;

	errno = 0;
	if (_getlogin_r(name, namelen) == NULL) {
		if (errno == 0)
			nerrno = EINVAL;
		else
			nerrno = errno;
	}
	errno = oerrno;
	return (nerrno);
}

char *
getlogin(void)
{
	struct utmp ubuf;
	static char *answer = 0;

	if (!answer && ((answer = malloc(_POSIX_LOGIN_NAME_MAX)) == NULL)) {
		errno = ENOMEM;
		return (NULL);
	}
	return (_getlogin_r(answer, _POSIX_LOGIN_NAME_MAX));
}
