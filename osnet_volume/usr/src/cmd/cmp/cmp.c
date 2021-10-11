/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)cmp.c	1.13	96/04/18 SMI"	/* SVr4.0 1.4	*/
/*
**	compare two files
*/

#include	<stdio.h>
#include	<ctype.h>
#include 	<locale.h>
#include	<sys/types.h>

FILE	*file1, *file2;

char	*arg;

int	eflg;
int	lflg = 1;

off_t	line = 1;
off_t	chr = 0;
off_t	skip1;
off_t	skip2;

off_t 	otoi();

main(argc, argv)
char **argv;
{
	extern char	*optarg;	/* getopt externals */
	extern int	optind;
	int		c;
	register	c1, c2;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "ls")) != EOF)
		switch (c) {
			case 'l':
				lflg++;
				break;
			case 's':
				lflg--;
				break;
			case '?':
			default:
				narg();
		}
	argv += optind;
	argc -= optind;
	if (argc < 2 || argc > 4)
		narg();

	arg = argv[0];
	if (arg[0] == '-' && arg[1] == 0)
		file1 = stdin;
	else if ((file1 = fopen(arg, "r")) == NULL)
		barg();

	arg = argv[1];
	if (arg[0] == '-' && arg[1] == 0)
		file2 = stdin;
	else if ((file2 = fopen(arg, "r")) == NULL)
		barg();

	if (file1 == stdin && file2 == stdin)
		narg();

	if (argc > 2)
		skip1 = otoi(argv[2]);
	if (argc > 3)
		skip2 = otoi(argv[3]);
	while (skip1) {
		if ((c1 = getc(file1)) == EOF) {
			arg = argv[0];
			earg();
		}
		skip1--;
	}
	while (skip2) {
		if ((c2 = getc(file2)) == EOF) {
			arg = argv[1];
			earg();
		}
		skip2--;
	}

	while (1) {
		chr++;
		c1 = getc(file1);
		c2 = getc(file2);
		if (c1 == c2) {
			if (c1 == '\n')
				line++;
			if (c1 == EOF) {
				if (eflg)
					exit(1);
				exit(0);
			}
			continue;
		}
		if (lflg == 0)
			exit(1);
		if (c1 == EOF) {
			arg = argv[0];
			earg();
		}
		if (c2 == EOF)
			earg();
		if (lflg == 1) {
			printf(gettext("%s %s differ: char %lld, line %lld\n"),
				argv[0], arg, chr, line);
			exit(1);
		}
		eflg = 1;
		printf("%6lld %3o %3o\n", chr, c1, c2);
	}
}

off_t
otoi(s)
char *s;
{
	off_t v;
	int base;

	v = 0;
	base = 10;
	if (*s == '0')
		base = 8;
	while (isdigit(*s))
		v = v*base + *s++ - '0';
	return (v);
}

narg()
{
	fprintf(stderr,
		gettext("usage: cmp [-l] [-s] file1 file2 [skip1] [skip2]\n"));
	exit(2);
}

barg()
{
	if (lflg)
		fprintf(stderr, gettext("cmp: cannot open %s\n"), arg);
	exit(2);
}

earg()
{
	fprintf(stderr, gettext("cmp: EOF on %s\n"), arg);
	exit(1);
}
