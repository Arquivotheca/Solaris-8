/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)errmsg.c	1.4	97/05/09 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

#include	<stdio.h>
#include	<varargs.h>
#include	"users.h"

extern	char	*errmsgs[];
extern	int	lasterrmsg;
extern	char	*cmdname;

/*
	synopsis: errmsg(msgid, (arg1, ..., argN))
*/

/*VARARGS*/
void
errmsg(va_alist)
va_dcl
{
	va_list	args;
	int	msgid;

	va_start(args);

	msgid = va_arg(args, int);

	if (msgid >= 0 && msgid < lasterrmsg) {
		(void) fprintf(stderr, "UX: %s: ", cmdname);
		(void) vfprintf(stderr, errmsgs[ msgid ], args);
	}

	va_end(args);
}

void
warningmsg(int what, char *name)
{
	if ((what & WARN_NAME_TOO_LONG) != 0) {
		(void) fprintf(stderr, "UX: %s: ", cmdname);
		(void) fprintf(stderr, "%s name too long.\n", name);
	}
	if ((what & WARN_BAD_GROUP_NAME) != 0) {
		(void) fprintf(stderr, "UX: %s: ", cmdname);
		(void) fprintf(stderr, "%s name should be all lower case"
			" or numeric.\n", name);
	}
	if ((what & WARN_BAD_LOGNAME_CHAR) != 0) {
		(void) fprintf(stderr, "UX: %s: ", cmdname);
		(void) fprintf(stderr, "%s name should be all alphanumeric,"
			" '-', '_', or '.'\n", name);
	}
	if ((what & WARN_BAD_LOGNAME_FIRST) != 0) {
		(void) fprintf(stderr, "UX: %s: ", cmdname);
		(void) fprintf(stderr, "%s name first character"
			" should be alphabetic.\n", name);
	}
	if ((what & WARN_NO_LOWERCHAR) != 0) {
		(void) fprintf(stderr, "UX: %s: ", cmdname);
		(void) fprintf(stderr, "%s name should have at least one "
			"lower case character.\n", name);
	}
}
