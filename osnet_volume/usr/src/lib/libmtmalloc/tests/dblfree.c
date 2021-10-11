/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dblfree.c	1.2	98/04/15 SMI"

#include <stdio.h>
#include "mtmalloc.h"

/*
 * test double free. The first double free should be fine.
 * the next double free should result in a core dump.
 *
 * cc -O -o dblfree dblfree.c -I../common -lmtmalloc
 */

main(int argc, char ** argv)
{
	char *foo;

	foo = malloc(10);
	free(foo);

	mallocctl(MTDOUBLEFREE, 1);

	printf("Double free coming up\n");
	printf("This should NOT dump core.\n");
	free(foo);

	foo = malloc(10);
	free(foo);

	mallocctl(MTDOUBLEFREE, 0);

	printf("Double free coming up\n");
	printf("This should dump core.\n");

	free(foo);
}
