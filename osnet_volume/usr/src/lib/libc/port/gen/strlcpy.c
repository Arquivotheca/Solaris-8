/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)strlcpy.c	1.3	99/08/25 SMI"	/* SVr4.0 1.9	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <string.h>
#include <sys/types.h>

/*
 * Copies src to the dstsize buffer at dst. The copy will never
 * overflow the destination buffer and the buffer will always be null
 * terminated.
 */

size_t
strlcpy(char *dst, const char *src, size_t len)
{
	size_t slen = strlen(src);
	size_t copied;

	if (len == 0)
		return (slen);

	if (slen >= len)
		copied = len - 1;
	else
		copied = slen;
	memcpy(dst, src, copied);
	dst[copied] = '\0';
	return (slen);
}
