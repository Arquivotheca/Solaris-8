/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wc.c	1.17	97/12/06 SMI"	/* SVr4.0 1.5.1.3	*/
/*
**	wc -- word and line count
*/

#include	<stdio.h>
#include	<limits.h>
#include	<locale.h>
#include	<wctype.h>
#include	<stdlib.h>
#include	<euc.h>

#undef BUFSIZ
#define	BUFSIZ	4096
unsigned char	b[BUFSIZ];

FILE *fptr = stdin;
unsigned long long 	wordct;
unsigned long long	twordct;
unsigned long long	linect;
unsigned long long	tlinect;
unsigned long long	charct;
unsigned long long	tcharct;
unsigned long long	real_charct;
unsigned long long	real_tcharct;

int cflag = 0, mflag = 0, lflag = 0, wflag = 0;


main(argc, argv)
char **argv;
{
	register unsigned char *p1, *p2;
	register unsigned int c;
	int	flag;
	int	i, token;
	int	status = 0;
	wchar_t wc;
	int	len, n;


	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);


	while ((flag = getopt(argc, argv, "cCmlw")) != EOF) {
		switch (flag) {
		case 'c':
			if (mflag)
				usage();

			cflag++;
			break;

		case 'C':
		case 'm':		/* POSIX.2 */
			if (cflag)
				usage();
			mflag++;
			break;

		case 'l':
			lflag++;
			break;

		case 'w':
			wflag++;
			break;

		default:
			usage();
			break;
		}
	}

	argc -= optind;
	argv = &argv[optind];

	/*
	 * If no flags set, use defaults
	 */
	if (cflag == 0 && mflag == 0 && lflag == 0 && wflag == 0) {
		cflag = 1;
		lflag = 1;
		wflag = 1;
	}

	i = 0;
	do {
		if (argc > 0 && (fptr = fopen(argv[i], "r")) == NULL) {
			fprintf(stderr, gettext(
				"wc: cannot open %s\n"), argv[i]);
			status = 2;
			continue;
		}

		p1 = p2 = b;
		linect = 0;
		wordct = 0;
		charct = 0;
		real_charct = 0;
		token = 0;
		for (;;) {
			if (p1 >= p2) {
				p1 = b;
				c = fread(p1, 1, BUFSIZ, fptr);
				if ((int)c <= 0)
					break;
				charct += c;
				p2 = p1+c;
			}
			c = *p1++;
			real_charct++;
			if (ISASCII(c)) {
				if (isspace(c)) {
					if (c == '\n')
						linect++;
					token = 0;
					continue;
				}

				if (!token) {
					wordct++;
					token++;
				}
			} else {
				p1--;
				if ((len = (p2 - p1)) <
						(unsigned int)MB_CUR_MAX) {
					for (n = 0; n < len; n++)
						b[n] = *p1++;
					p1 = b;
					p2 = p1 + n;
					c = fread(p2, 1, BUFSIZ - n, fptr);
					if ((int)c > 0) {
						charct += c;
						p2 += c;
					}
				}

				if ((len = (p2 - p1)) >
						(unsigned int)MB_CUR_MAX)
					len = (unsigned int)MB_CUR_MAX;
				if ((len = mbtowc(&wc, (char *)p1, len)) > 0) {
					p1 += len;
					if (iswspace(wc)) {
						token = 0;
						continue;
					}
				} else
					p1++;
				if (!token) {
					wordct++;
					token++;
				}
			}

		}
		/* print lines, words, chars */
printwc:
		wcp(charct, wordct, linect, real_charct);
		if (argc > 0) {
			printf(" %s\n", argv[i]);
		}
		else
			printf("\n");
		fclose(fptr);
		tlinect += linect;
		twordct += wordct;
		tcharct += charct;
		real_tcharct += real_charct;
	} while (++i < argc);

	if (argc > 1) {
		wcp(tcharct, twordct, tlinect, real_tcharct);
		printf(" total\n");
	}
	exit(status);
}

wcp(charct, wordct, linect, real_charct)
unsigned long long charct;
unsigned long long wordct;
unsigned long long linect;
unsigned long long real_charct;
{
	if (lflag)
		printf((linect < 10000000) ? " %7llu" :
			" %llu", linect);

	if (wflag)
		printf((wordct < 10000000) ? " %7llu" :
			" %llu", wordct);

	if (cflag)
		printf((charct < 10000000) ? " %7llu" :
			" %llu", charct);
	else if (mflag)
		printf((real_charct < 10000000) ? " %7llu" :
			" %llu", real_charct);
}

usage()
{
	fprintf(stderr, gettext(
		"usage: wc [-c | -m | -C] [-lw] [file ...]\n"));
	exit(2);
}
