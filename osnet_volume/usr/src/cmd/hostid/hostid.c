/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hostid.c	1.5	95/03/02 SMI"

#include <stdio.h>
#include <unistd.h>

main()
{
	long hostval;

	if ((hostval = gethostid()) == -1) {
		(void) fprintf(stderr, "bad hostid format\n");
		exit(1);
	}
	(void) printf("%08x\n", (unsigned long)hostval);
	exit(0);
}
