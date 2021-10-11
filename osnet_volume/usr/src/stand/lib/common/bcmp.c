/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)bcmp.c	1.10	97/03/10 SMI"

#include <sys/salib.h>

int
bcmp(const void *s1, const void *s2, size_t len)
{
	const char	*cs1	= s1;
	const char	*cs2	= s2;

	for (; len != 0; --len)
		if (*cs1++ != *cs2++)
			return (1);
	return (0);
}
