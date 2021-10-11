/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)copystream.c	1.6	99/03/09 SMI"	/* SVr4.0 1.2   */
/*LINTLIBRARY*/

/*
    NAME
	copystream - copy one FILE stream to another

    SYNOPSIS
	int copystream(FILE *infp, FILE *outfp)

    DESCRIPTION
	copystream() copies one stream to another. The stream
	infp must be opened for reading and the stream outfp
	must be opened for writing.

	It returns true if the stream is successively copied;
	false if any writes fail.
*/

#include "synonyms.h"
#include "libmail.h"
#include <sys/types.h>

int
copystream(FILE *infp, FILE *outfp)
{
	char buffer[BUFSIZ];
	size_t nread;

	while ((nread = fread(buffer, sizeof (char), sizeof (buffer),
	    infp)) > 0)
		if (fwrite(buffer, sizeof (char), nread, outfp) != nread)
			return (0);
	return (1);
}
