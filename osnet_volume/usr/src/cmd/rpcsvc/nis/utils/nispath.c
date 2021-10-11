/*
 *	nispath.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)nispath.c	1.4	98/05/18 SMI"

/*
 *	nispath.c
 *
 * This little utility will print out the search path for a given
 * NIS+ name.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <rpcsvc/nis.h>

void
usage(cmd)
	char	*cmd;
{
	fprintf(stderr, "usage : %s [-v] name\n", cmd);
	exit(1);
}

main(argc, argv)
	int	argc;
	char	*argv[];
{
	nis_name	*result;
	int		i = 0;
	char		*name;
	int		verbose = 0;

	if ((argc == 1) || (argc > 3))
		usage(argv[0]);

	if (argc == 3) {
		if (strcmp(argv[1], "-v") == 0)
			verbose = 1;
		else
			usage(argv[0]);
		name = argv[2];
	} else {
		if (strcmp(argv[1], "-v") == 0)
			usage(argv[0]);
		name = argv[1];
	}

	result = nis_getnames(name);
	if (verbose) {
		printf("For NIS+ Name : \"%s\"\n", name);
		printf("Search Path   :\n");
	}
	if (! result) {
		if (verbose)
			printf("\t**NONE**\n");
		exit(1);
	} else
		while (result[i]) {
			if (verbose)
				printf("\t");
			printf("\"%s\"\n", result[i++]);
		}
	exit(0);
}
