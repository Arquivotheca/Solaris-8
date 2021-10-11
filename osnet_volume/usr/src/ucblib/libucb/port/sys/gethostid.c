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

#pragma ident	"@(#)gethostid.c	1.4	97/06/16 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>
#include <unistd.h>
#include <stdlib.h>

#define	HOSTIDLEN	40

long
gethostid(void)
{
	char	name[HOSTIDLEN], *end;
	unsigned long	hostid;
	int	error;

	error = sysinfo(SI_HW_SERIAL, name, HOSTIDLEN);
	/*
	 * error > 0 ==> number of bytes to hold name
	 * and is discarded since gethostid only
	 * cares if it succeeded or failed
	 */
	if (error == -1)
		return (-1);
	else {
		hostid = strtoul(name, &end, 10);
		if (hostid == 0 && end == name) {
			return (-1);
		}
		return ((long) hostid);
	}
}
