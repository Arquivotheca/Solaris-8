/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)share.c	1.1	99/05/24 SMI"

/*
 * cachefs share - dummy utility to accomodate cachefs inclusion in 
 * /etc/dfs/fstypes
 */
#include <stdio.h>
#include <libintl.h>

int
main(int argc, char **argv)
{
	fprintf(stderr, gettext("cachefs share is not supported.\n"));
	return(1);
}
