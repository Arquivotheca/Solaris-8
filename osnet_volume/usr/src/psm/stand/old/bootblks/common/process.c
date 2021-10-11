#ident	"@(#)process.c	1.3	94/09/28 SMI"

/*
 * Copyright (c) 1991 Sun Microsystems, Inc.
 *
 * Produce a big hunk o' data from an Fcode input file.
 * Usage: process <infile.fcode >outfile
 */

#include <stdio.h>

main()
{
	int c, count = 0;

	(void) printf("const unsigned char forthblock[] = {\n");
	while ((c = getchar()) != EOF)
		(void) printf("0x%02x,%c", c & 0xff,
		    (count = ++count % 8) ? ' ' : '\n');
	(void) printf("\n};\n");
	return (0);
}
