/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strmove.c	1.5	99/03/09 SMI" 	/* SVr4.0 1.1	*/
/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include "libmail.h"

/*
    NAME
	strmove - copy a string, permitting overlaps

    SYNOPSIS
	void strmove(char *to, char *from)

    DESCRIPTION
	strmove() acts exactly like strcpy() with the additional
	guarantee that it will work with overlapping strings.
	It does it left-to-right, a byte at a time.
*/

void
strmove(char *to, char *from)
{
	while ((*to++ = *from++) != 0)
		;
}
