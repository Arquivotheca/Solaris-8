/*
 * Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
 *
 */

#ident	"@(#)minmode.c	1.2	92/07/14 SMI"

#include <sys/types.h>
#include <sys/stat.h>

#define	USAGE "Usage: minmode pathname mode\n"

/* minmode: takes a pathname and a mode representation in octal, sets
 * the new mode to be stricter than both the current mode and the specified
 * mode.
 * If successful, prints the new mode (exit status = 0);
 * if unsuccessful, prints the usage message (exit status = -1).
 */

main(int argc, char **argv)
{
	struct stat sbuf;
	long mode, perm, sbits;
	long currmode, currperm, currsbits;
	long newmode, newperm, newsbits;
	long strtol();
	void perror();

	if (argc != 3) {
		printf("%s\n", USAGE);
		exit(1);
	}

	mode = strtol(argv[2], 0, 8);
	if (mode == 0) {
		printf("minmode: invalid mode - %s\n", argv[2]);
		printf("%s\n", USAGE);
		exit(1);
	}

	if (stat(argv[1], &sbuf)) {
		printf("minmode: can't stat %s\n", argv[1]);
		perror(0);
		printf("%s\n", USAGE);
		exit(1);
	}
	currmode = ((long) sbuf.st_mode) & 07777;

	perm = mode & 0777;
	sbits = mode & 007000;
	currperm = currmode & 0777;
	currsbits = currmode & 007000;
	newperm = perm & currperm;
	newsbits = sbits | currsbits;
	newmode = newsbits | newperm;
	if (newmode == currmode)
		exit(1);
	printf("%o\n", newmode);
	exit(0);
}
