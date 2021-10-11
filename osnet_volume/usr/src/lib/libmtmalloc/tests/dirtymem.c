/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dirtymem.c	1.1	98/04/14 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "mtmalloc.h"

/*
 * This file tests for reference after free
 *
 * cc -O -o dirtymem dirtymem.c -lmtmalloc -I../common
 */

struct a_struct {
	int a;
	char *b;
	double c;
} struct_o_death;


main(int argc, char ** argv)
{
	struct a_struct *foo, *leak;
	int ncpus = sysconf(_SC_NPROCESSORS_CONF);

	mallocctl(MTDEBUGPATTERN, 1);
	foo = (struct a_struct *)malloc(sizeof (struct_o_death));

	free(foo);
	foo->a = 4;

	/*
	 * We have to make sure we allocate from the same pool
	 * as the last time. Turn the rotor with malloc until
	 * we get back to where we started.
	 */
	while (ncpus-- > 1)
		leak = malloc(sizeof (struct_o_death));

	fprintf(stderr, "malloc struct again\n");
	fprintf(stderr, "we should dump core\n");
	foo = malloc(sizeof (struct_o_death));

	exit(0);
}
