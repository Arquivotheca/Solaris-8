/*
 * Copyright (c) 1991-1997, Sun Microsytems, Inc.
 */

#pragma ident	"@(#)valloc.c	1.2	97/08/04 SMI"

/*LINTLIBRARY*/
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>

/*
 * valloc(size) - do nothing
 */

/*ARGSUSED*/
void *
valloc(size_t size)
{
	return (0);
}


/*
 * memalign(align,nbytes) - do nothing
 */

/*ARGSUSED*/
void *
memalign(size_t align, size_t nbytes)
{
	errno = EINVAL;
	return (NULL);
}
