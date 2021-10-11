/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)align.c	1.1	98/04/14 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

/*
 * This file tests for 8 bytes alignment on all allocations.
 *
 * cc -O -o align align.c -lmtmalloc
 */

#define	N 100	/* big enough to hold results */

main(int argc, char ** argv)
{
	int i = 0;
	char *bar[N];

	while (i < 20) {
		bar[i] = malloc(1<<i);
		if ((uintptr_t)bar[i] & 7) {
			fprintf(stderr, "Address %p is not 8 byte aligned\n",
				bar[i]);
			fprintf(stderr, "Allocation size %d\n", 1<<i);
		}
		i++;
	}

	i = 0;
	while (i < 20) {
		free(bar[i]);
		i++;
	}
}
