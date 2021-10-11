/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)wideload.c	1.1	98/07/19 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>

/*
 * This file tests for large (2GB and greater) allocations.
 *
 * cc -O -o wideload wideload.c -lmtmalloc
 */

main(int argc, char ** argv)
{
	char * foo;
	char * bar;
	size_t request = LONG_MAX;

	foo = malloc(0); /* prime the pump */

	foo = (char *)sbrk(0);
	printf ("Before big malloc brk is %p request %d\n", foo, request);
	foo = malloc(request + 100);
	bar = (char *)sbrk(0);
	printf ("After big malloc brk is %p\n", bar);
}
