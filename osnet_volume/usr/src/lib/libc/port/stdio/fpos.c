/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fpos.c	1.15	96/11/19 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

int
fgetpos(FILE *stream, fpos_t *pos)
{
	if ((*pos = (fpos_t)ftello(stream)) == (fpos_t)-1)
		return (-1);
	return (0);
}

int
fsetpos(FILE *stream, const fpos_t *pos)
{
	if (fseeko(stream, (off_t)*pos, SEEK_SET) != 0)
		return (-1);
	return (0);
}
