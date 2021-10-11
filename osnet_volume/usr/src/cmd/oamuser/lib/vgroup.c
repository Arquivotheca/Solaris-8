/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)vgroup.c	1.6	97/05/09 SMI"	/* SVr4.0 1.2 */

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<users.h>
#include	<userdefs.h>

extern int valid_gid();

/*
 *	validate a group name or number and return the appropriate
 *	group structure for it.
 */
int
valid_group(char *group, struct group **gptr, int *warning)
{
	gid_t gid;
	char *ptr;

	*warning = 0;
	if (isdigit(*group)) {
		gid = (gid_t) strtol(group, &ptr, (int) 10);
		if (! *ptr)
		return (valid_gid(gid, gptr));
	}
	for (ptr = group; *ptr != NULL; ptr++) {
		if (!isprint(*ptr) || (*ptr == ':') || (*ptr == '\n'))
			return (INVALID);
	}

	/* length checking and other warnings are done in valid_gname() */
	return (valid_gname(group, gptr, warning));
}
