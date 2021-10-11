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

#pragma ident	"@(#)gethostname.c	1.3	97/06/16 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>
#include <unistd.h>
#include <errno.h>

int
gethostname(char *name, int namelen)
{
	int	error;

	error = sysinfo(SI_HOSTNAME, name, namelen);
	/*
	 * error > 0 ==> number of bytes to hold name
	 * and is discarded since gethostname only
	 * cares if it succeeded or failed
	 */
	return (error == -1 ? -1 : 0);
}

int
sethostname(char *name, int namelen)
{
	int	error;

	/*
	 * Check if superuser
	 */
	if (getuid()) {
		errno = EPERM;
		return (-1);
	}
	error = sysinfo(SI_SET_HOSTNAME, name, namelen);
	return (error == -1 ? -1 : 0);
}