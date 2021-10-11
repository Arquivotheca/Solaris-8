/*
 * Copyright (c) 1995-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)setbuffer.c	1.3	97/06/21 SMI"

/*LINTLIBRARY*/

/*
 * Compatibility wrappers for setbuffer and setlinebuf
 */

#include "synonyms.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Associate a buffer with an "unused" stream.
 * If the buffer is NULL, then make the stream completely unbuffered.
 */
void
setbuffer(FILE *iop, char *abuf, size_t asize)
{
	if (abuf == NULL)
		(void) setvbuf(iop, NULL, _IONBF, 0);
	else
		(void) setvbuf(iop, abuf, _IOFBF, asize);
}

/*
 * Convert a block buffered or line buffered stream to be line buffered
 * Allowed while the stream is still active; relies on the implementation
 * not the interface!
 */

int
setlinebuf(FILE *iop)
{
	(void) fflush(iop);
	(void) setvbuf(iop, NULL, _IOLBF, 128);
	return (0);
}
