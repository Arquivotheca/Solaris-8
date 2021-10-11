/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)bcopy.c	1.4	97/03/10 SMI"

#include <sys/salib.h>

void
bcopy(const void *src, void *dest, size_t count)
{
	const char	*csrc	= src;
	char		*cdest	= dest;

	if (count == 0)
		return;

	if (csrc < cdest && (csrc + count) > cdest) {
		/* overlap copy */
		while (count != 0)
			--count, *(cdest + count) = *(csrc + count);



	} else {
		while (count != 0)
			--count, *cdest++ = *csrc++;

	}
}
