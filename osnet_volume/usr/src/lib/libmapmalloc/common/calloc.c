/*
 * Copyright (c) 1991-1997, Sun Microsytems, Inc.
 */

#pragma ident	"@(#)calloc.c	1.2	97/08/04 SMI"	/* SunOS 1.10 */
/*LINTLIBRARY*/
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
/*
 * calloc - allocate and clear memory block
 */

void *
calloc(size_t num, size_t size)
{
	void *mp;

	num *= size;
	mp = malloc(num);
	if (mp == NULL)
		return (NULL);
	(void) memset(mp, 0, num);
	return (mp);
}

/*ARGSUSED*/
void
cfree(void *p, unsigned num, unsigned size)
{
	free(p);
}
