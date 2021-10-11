/*
 * Copyright (c) 1991, Sun Microsytems, Inc.
 */

#pragma ident	"@(#)malloc_debug.c	1.2	97/08/04 SMI"

/*LINTLIBRARY*/
#include <sys/types.h>


/*
 * malloc_debug(level) - empty routine
 */

/*ARGSUSED*/
int
malloc_debug(int level)
{
	return (1);
}


/*
 * malloc_verify() - empty routine
 */

int
malloc_verify(void)
{
	return (1);
}


/*
 * mallocmap() - empty routine
 */

void
mallocmap(void)
{
	;
}
