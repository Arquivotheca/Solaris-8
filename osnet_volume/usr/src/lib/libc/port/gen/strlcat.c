/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)strlcat.c	1.3	99/08/25 SMI"	/* SVr4.0 1.9	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <string.h>
#include <sys/types.h>

/*
 * Appends src to the dstsize buffer at dst. The append will never
 * overflow the destination buffer and the buffer will always be null
 * terminated.
 */

size_t
strlcat(char *dst, const char *src, size_t dstsize)
{
	size_t l1 = strlen(dst);
	size_t l2 = strlen(src);
	size_t copied;

	if (dstsize == 0 || l1 >= dstsize - 1)
		return (l1 + l2);

	copied = l1 + l2 >= dstsize ? dstsize - l1 - 1 : l2;
	memcpy(dst + l1, src, copied);
	dst[l1+copied] = '\0';
	return (l1 + l2);
}
