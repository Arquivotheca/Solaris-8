/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)vgname.c	1.7	97/05/09	SMI"	/* SVr4.0 1.2 */

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	<stdio.h>
#include	<ctype.h>
#include	<userdefs.h>
#include	<users.h>

/*
 * validate string given as group name.
 */
int
valid_gname(char *group, struct group **gptr, int *warning)
{
	struct group *t_gptr;
	char *ptr = group;
	char c;
	int len = 0;
	int badchar = 0;

	*warning = 0;
	if (!group || !*group)
		return (INVALID);

	for (c = *ptr; c != NULL; ptr++, c = *ptr) {
		len++;
		if (!isprint(c) || (c == ':') || (c == '\n'))
			return (INVALID);
		if (!(islower(c) || isdigit(c)))
			badchar++;
	}

	/*
	 * XXX constraints causes some operational/compatibility problem.
	 * This has to be revisited in the future as ARC/standards issue.
	 */
	if (len > MAXGLEN - 1)
		*warning = *warning | WARN_NAME_TOO_LONG;
	if (badchar != 0)
		*warning = *warning | WARN_BAD_GROUP_NAME;

	if ((t_gptr = getgrnam(group)) != NULL) {
		if (gptr) *gptr = t_gptr;
		return (NOTUNIQUE);
	}
	return (UNIQUE);
}
