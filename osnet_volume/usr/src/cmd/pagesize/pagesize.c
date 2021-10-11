/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pagesize.c	1.3	95/03/02 SMI"

#include <stdio.h>
#include <unistd.h>

main()
{
	(void) printf("%d\n", getpagesize());
	exit(0);
}
