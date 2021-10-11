/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)skipspace.c	1.7	99/03/09 SMI"	/*	SVr4.0 1.5	*/
/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include "libmail.h"
#include <ctype.h>

/*
 * Return pointer to first non-blank character in p
 */
char *
skipspace(char *p)
{
	while (*p && isspace(*p)) {
		p++;
	}
	return (p);
}
