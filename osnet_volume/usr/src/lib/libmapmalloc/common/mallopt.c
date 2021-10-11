/*
 * Copyright (c) 1991-1997, Sun Microsytems, Inc.
 */

#pragma ident	"@(#)mallopt.c	1.2	97/08/04 SMI"

/*LINTLIBRARY*/
#include <sys/types.h>
#include <malloc.h>

struct  mallinfo __mallinfo;

/*
 * mallopt -- Do nothing
 */
/*ARGSUSED*/
mallopt(int cmd, int value)
{
	return (0);
}


/*
 * mallinfo -- Do nothing
 */
struct mallinfo
mallinfo(void)
{
	return (__mallinfo);
}
