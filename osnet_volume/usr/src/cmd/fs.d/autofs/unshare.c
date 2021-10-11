/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)unshare.c	1.1	99/05/24 SMI"

/*
 * autofs unshare - dummy utility to accomodate autofs inclusion in 
 * /etc/dfs/fstypes
 */
#include <stdio.h>
#include <libintl.h>

int
main(int argc, char **argv)
{
	fprintf(stderr, gettext("autofs unshare is not supported.\n"));
	return(1);
}
