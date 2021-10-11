/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cuserid.c	1.12	99/04/06 SMI"	/* SVr4.0 1.14	*/

/*LINTLIBRARY*/

#pragma weak cuserid = _cuserid

#include "synonyms.h"
#include <stdio.h>
#include <pwd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static char res[L_cuserid];

char *
cuserid(char *s)
{
	struct passwd *pw;
	struct passwd pwd;
	char buffer[BUFSIZ];
	char utname[L_cuserid];
	char *p;

	if (s == NULL)
		s = res;
	p = getlogin_r(utname, L_cuserid);
	s[L_cuserid - 1] = '\0';
	if (p != NULL)
		return (strncpy(s, p, L_cuserid - 1));
	pw = getpwuid_r(getuid(), &pwd, buffer, BUFSIZ);
	if (pw != NULL)
		return (strncpy(s, pw->pw_name, L_cuserid - 1));
	*s = '\0';
	return (NULL);
}
