/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)line.c	1.7	94/10/06 SMI"	/* SVr4.0 1.3	*/

/*
 *	This program reads a single line from the standard input
 *	and writes it on the standard output. It is probably most useful
 *	in conjunction with the shell.
 */

#include <limits.h>

#define	LSIZE	LINE_MAX		/* POSIX.2 */

int EOF;
char nl = '\n';

main()
{
	register char c;
	char line[LSIZE];
	register char *linep, *linend;

	EOF = 0;
	linep = line;
	linend = line + LSIZE;

	while ((c = readc()) != nl) {
		if (linep == linend) {
			write(1, line, LSIZE);
			linep = line;
		}
		*linep++ = c;
	}

	write(1, line, linep-line);
	write(1, &nl, 1);
	if (EOF == 1)
		exit(1);
	exit(0);
}

readc()
{
	char c;

	if (read(0, &c, 1) != 1) {
		EOF = 1;
		return (nl);
	}
	else
		return (c);
}
