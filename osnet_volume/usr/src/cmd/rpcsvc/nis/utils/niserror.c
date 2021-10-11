/*
 *	niserror.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)niserror.c	1.4	92/07/14 SMI"

/*
 *	niserror.c
 *
 *	This module prints the error message associated with an NIS+
 * error code.
 */

#include <stdio.h>
#include <ctype.h>
#include <rpcsvc/nis.h>

usage()
{
	fprintf(stderr, "usage: niserror error-num\n");
	exit(1);
}

main(argc, argv)
	int	argc;
	char	*argv[];
{
	nis_error	err;

	if (argc != 2)
		usage();

	if (! isdigit(*argv[1]))
		usage();

	err = (nis_error) atoi(argv[1]);
	printf("%s\n", nis_sperrno(err));
	exit(0);
}
