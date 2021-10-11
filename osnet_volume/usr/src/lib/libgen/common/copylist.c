/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)copylist.c	1.12	97/06/17 SMI"	/* SVr4.0 1.1.3.2 */

/*LINTLIBRARY*/

/*
 * copylist copies a file into a block of memory, replacing newlines
 * with null characters, and returns a pointer to the copy.
 */

#include <sys/feature_tests.h>
#ifndef _LP64
#pragma weak copylist64 = _copylist64
#endif
#pragma weak copylist = _copylist

#include "synonyms.h"
#include <sys/types.h>
#include <libgen.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>

static char *
common_copylist(const char *filenm, off64_t size)
{
	FILE	*strm;
	int	c;
	char	*ptr, *p;

	if (size > SSIZE_MAX) {
		errno = EOVERFLOW;
		return (NULL);
	}

	/* get block of memory */
	if ((ptr = malloc(size)) == NULL) {
		return (NULL);
	}

	/* copy contents of file into memory block, replacing newlines */
	/* with null characters */
	if ((strm = fopen(filenm, "r")) == NULL) {
		return (NULL);
	}
	for (p = ptr; p < ptr + size && (c = getc(strm)) != EOF; p++) {
		if (c == '\n')
			*p = '\0';
		else
			*p = (char)c;
	}
	(void) fclose(strm);

	return (ptr);
}


#ifndef _LP64
char *
_copylist64(const char *filenm, off64_t *szptr)
{
	struct	stat64	stbuf;

	/* get size of file */
	if (stat64(filenm, &stbuf) == -1) {
		return (NULL);
	}
	*szptr = stbuf.st_size;

	return (common_copylist(filenm, stbuf.st_size));
}
#endif


char *
_copylist(const char *filenm, off_t *szptr)
{
	struct	stat64	stbuf;

	/* get size of file */
	if (stat64(filenm, &stbuf) == -1) {
		return (NULL);
	}

	if (stbuf.st_size > LONG_MAX) {
		errno = EOVERFLOW;
		return (NULL);
	}

	*szptr = (off_t)stbuf.st_size;

	return (common_copylist(filenm, stbuf.st_size));
}
