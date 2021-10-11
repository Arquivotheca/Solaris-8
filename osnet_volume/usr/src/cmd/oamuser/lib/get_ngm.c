/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)get_ngm.c	1.4	96/09/05 SMI"	/* SVr4.0 1.2 */

#include <sys/param.h>
#include <unistd.h>

/*
 * read the value of NGROUPS_MAX from the kernel
 */
int
get_ngm(void)
{
	static int ngm = -1;

	if (ngm == -1 &&
	    (ngm = (int)sysconf(_SC_NGROUPS_MAX)) == -1)
		ngm = NGROUPS_UMAX;

	return (ngm);
}
