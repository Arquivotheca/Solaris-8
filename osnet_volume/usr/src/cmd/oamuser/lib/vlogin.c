/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)vlogin.c	1.5	97/05/09 SMI"	/* SVr4.0 1.3 */

/*LINTLIBRARY*/

#include	<sys/types.h>
#include	<stdio.h>
#include	<ctype.h>
#include	<userdefs.h>
#include	<users.h>
#include	<limits.h>

/*
 * validate string given as login name.
 */
int
valid_login(char *login, struct passwd **pptr, int *warning)
{
	struct passwd *t_pptr;
	char *ptr = login;
	int bad1char, badc, clower, len;
	char c;

	len = 0; clower = 0; badc = 0; bad1char = 0;
	*warning = 0;
	if (!login || !*login)
		return (INVALID);

	c = *ptr;
	if (!isalpha(c))
		bad1char++;
	for (; c != NULL; ptr++, c = *ptr) {
		len++;
		if (!isprint(c) || (c == ':') || (c == '\n'))
			return (INVALID);
		if (!isalnum(c) && c != '_' && c != '-' && c != '.')
			badc++;
		if (islower(c))
			clower++;
	}

	/*
	 * XXX length checking causes some operational/compatibility problem.
	 * This has to be revisited in the future as ARC/standards issue.
	 */
	if (len > LOGNAME_MAX)
		*warning = *warning | WARN_NAME_TOO_LONG;
	if (clower == 0)
		*warning = *warning | WARN_NO_LOWERCHAR;
	if (badc != 0)
		*warning = *warning | WARN_BAD_LOGNAME_CHAR;
	if (bad1char != 0)
		*warning = *warning | WARN_BAD_LOGNAME_FIRST;

	if ((t_pptr = getpwnam(login)) != NULL) {
		if (pptr) *pptr = t_pptr;
		return (NOTUNIQUE);
	}
	return (UNIQUE);
}
