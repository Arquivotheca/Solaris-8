/*
 * Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
 *
 */

#ident	"@(#)str_to_mode.c	1.2	92/07/14 SMI"

#include <stdio.h>
#include <string.h>

/* Takes a (10 char) permission string (as returned by ls -l) and prints
 * the equivalent octal number.
 * E.g. -rwsr-xr-- => 04754
 */

main(int argc, char **argv)
{
	char *perm;
	int result = 0;

	if ((argc != 2) || (strlen(argv[1]) != 10)) {
		printf("-1\n");
		exit(1);
	}

	perm = argv[1];

	/* user bits */
	if (perm[1] == 'r')
		result = result | 00400;
	if (perm[2] == 'w')
		result = result | 00200;
	if (perm[3] == 'x')
		result = result | 00100;
	else if (perm[3] == 's')
		result = result | 04100;
	else if (perm[3] == 'S')
		result = result | 04000;

	/* group bits */
	if (perm[4] == 'r')
		result = result | 00040;
	if (perm[5] == 'w')
		result = result | 00020;
	if (perm[6] == 'x')
		result = result | 00010;
	else if (perm[6] == 's')
		result = result | 02010;
	else if (perm[6] == 'S')
		result = result | 02000;

	/* world bits */
	if (perm[7] == 'r')
		result = result | 00004;
	if (perm[8] == 'w')
		result = result | 00002;
	if (perm[9] == 'x')
		result = result | 00001;
	else if (perm[9] == 't')
		result = result | 01001;
	else if (perm[9] == 'T')
		result = result | 01000;

	printf("%05o\n", result);
}
