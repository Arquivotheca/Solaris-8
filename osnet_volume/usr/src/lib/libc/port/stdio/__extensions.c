/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)__extensions.c	1.1	98/01/27 SMI"

/*LINTLIBRARY*/

#include "file64.h"
#include "synonyms.h"
#include <sys/types.h>
#include <stdio.h>
#include <mtlib.h>
#include "stdiom.h"
#include <stdio_ext.h>

/*
 * Returns non-zero if the file is open readonly, or if the last operation
 * on the stream was a read e.g. fread() or fgetc().  Otherwise returns 0.
 */
int
__freading(FILE *stream)
{
	return (stream->_flag & _IOREAD);
}

/*
 * Returns non-zero if the file is open write-only or append-only, or if
 * the last operation on the stream was a write e.g. fwrite() or fputc().
 * Otherwise returns 0.
 */
int
__fwriting(FILE *stream)
{
	return (stream->_flag & _IOWRT);
}

/*
 * Returns non-zero if it is possible to read from a stream.
 */
int
__freadable(FILE *stream)
{
	return (stream->_flag & (_IOREAD|_IORW));
}

/*
 * Returns non-zero if it is possible to write on a stream.
 */
int
__fwritable(FILE *stream)
{
	return (stream->_flag & (_IOWRT|_IORW));
}

/*
 * Returns non-zero if the stream is line buffered.
 */
int
__flbf(FILE *stream)
{
	return (stream->_flag & _IOLBF);
}

/*
 * Discard any pending buffered I/O.
 */
void
__fpurge(FILE *stream)
{
	rmutex_t *lk;

	FLOCKFILE(lk, stream);
	if ((stream->_ptr = stream->_base) != NULL)
		stream->_cnt = 0;
	FUNLOCKFILE(lk);
}

/*
 * Return the amount of output pending on a stream (in bytes).
 */
size_t
__fpending(FILE *stream)
{
	size_t amount;
	rmutex_t *lk;

	FLOCKFILE(lk, stream);
	amount = stream->_ptr - stream->_base;
	FUNLOCKFILE(lk);
	return (amount);
}

/*
 * Returns the buffer size (in bytes) currently in use by the given stream.
 */
size_t
__fbufsize(FILE *stream)
{
	size_t size;
	rmutex_t *lk;

	FLOCKFILE(lk, stream);
	size = _bufend(stream) - stream->_base;
	FUNLOCKFILE(lk);
	return (size);
}
