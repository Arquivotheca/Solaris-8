/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)abspath.c	1.5	99/03/09 SMI" 	/* SVr4.0 1.3	*/
/*LINTLIBRARY*/
/*
    NAME
	abspath - expand a path relative to some `.'

    SYNOPSIS
	string *abspath(char *path, char *dot, string *to)

    DESCRIPTION
	If "path" is relative, ie: does not start with `.', the
	the value of "dot" will be prepended and the result
	returned in "to". Otherwise, the value of "path" is
	returned in "to".
*/
#include "synonyms.h"
#include "libmail.h"
#include <sys/types.h>

string *
abspath(char *path, char *dot, string *to)
{
	if (*path == '/') {
		to = s_append(to, path);
	} else {
		to = s_append(to, dot);
		to = s_append(to, path);
	}
	return (to);
}
