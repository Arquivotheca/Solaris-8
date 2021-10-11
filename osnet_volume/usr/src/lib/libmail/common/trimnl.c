/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)trimnl.c	1.4	99/03/09 SMI" 	/* SVr4.0 1.3	*/
/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include "libmail.h"
#include <string.h>

/*
 * Trim trailing newlines from string.
 */
void
trimnl(char *s)
{
	char	*p;

	p = s + strlen(s) - 1;
	while ((*p == '\n') && (p >= s)) {
		*p-- = '\0';
	}
}
