/*
 * Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
 *
 */

#ident	"@(#)is_writable.c	1.2	92/07/14 SMI"

#include <sys/types.h>
#include <sys/stat.h>

/* Checks group (-g) or world (default) writeability.
 * Returns as exit code: 0 = writable
 *                       1 = not writable
 */

main(int argc, char **argv)
{
	int group = 0, xmode = 0;
	struct stat statb;

	if (argc < 2) {
		printf("Usage: %s [-g] file\n",argv[0]);
		exit(0);
	}

	if (argc > 2) {
		if (!strcmp(argv[1], "-g")) {
			group = 1;
			argc--;
			argv++;
		}
	}

	if (stat(*++argv,&statb) < 0) {
		exit(2);
	}

	if (group)
		xmode = statb.st_mode & S_IWGRP;
	else 
		xmode = statb.st_mode & S_IWOTH;

	exit(!xmode);
}
