/*
 * Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
 *
 */

#ident	"@(#)realpath.c	1.2	92/07/14 SMI"

#include <sys/param.h>

main(int argc, 	char **argv)
{
	char *realpath();
	char *path;
	char resolved_path[MAXPATHLEN];

	if (realpath(argv[1], resolved_path) == NULL) {
		printf("\n");
	} else 
		printf("%s\n", resolved_path);
}
