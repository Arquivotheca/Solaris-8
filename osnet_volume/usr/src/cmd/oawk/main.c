/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)main.c	1.9	99/11/16 SMI"

#include <stdio.h>
#include <ctype.h>
#include "awk.def"
#include "awk.h"
#include <wctype.h>
#include <getwidth.h>
#include <langinfo.h>
#include <stdlib.h>

int	dbg	= 0;
int	svargc;
wchar_t **svargv;
eucwidth_t eucwidth;

extern FILE	*yyin;	/* lex input file */
wchar_t *lexprog;	/* points to program argument if it exists */
extern	errorflag;	/* non-zero if any syntax errors; set by yyerror */

wchar_t	radixpoint = L'.';

int filefd, symnum, ansfd;
extern int maxsym, errno;
main(argc, argv) int argc; char **argv; {
	register wchar_t	*p, **wargv;
	register int		i;
	static wchar_t L_dash[] = L"-";
	static wchar_t L_dashd[] = L"-d";
	char	*nl_radix;

	/*
	 * At this point, numbers are still scanned as in
	 * the POSIX locale.
	 * (POSIX.2, volume 2, P867, L4742-4757)
	 */
	(void) setlocale(LC_ALL, "");
	(void) setlocale(LC_NUMERIC, "C");
#ifndef	TEXT_DOMAIN
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

	textdomain(TEXT_DOMAIN);

	getwidth(&eucwidth);
	if (argc == 1) {
		fprintf(stderr,
gettext("awk: Usage: awk [-Fc] [-f source | 'cmds'] [files]\n"));
		exit(2);
	}
	syminit();
	if ((wargv = (wchar_t **)malloc((argc+1) * sizeof (wchar_t *))) == NULL)
		error(FATAL, "Insuffcient memory on argv");
	for (i = 0; i < argc; i++) {
		if ((p = (wchar_t *)malloc((strlen(argv[i]) + 1)
						* sizeof (wchar_t))) == NULL)
			error(FATAL, "Insuffcient memory on argv");
		mbstowcs(p, argv[i], strlen(argv[i]) + 1);
		wargv[i] = p;
	}
	wargv[argc] = NULL;
	while (argc > 1) {
		argc--;
		wargv++;
		/* this nonsense is because gcos argument handling */
		/* folds -F into -f.  accordingly, one checks the next */
		/* character after f to see if it's -f file or -Fx. */
		if (wargv[0][0] == L'-' && wargv[0][1] == L'f' &&
			wargv[0][2] == 0) {
			if (argc <= 1)
				error(FATAL, "no argument for -f");
			yyin = (wscmp(wargv[1], L_dash) == 0)
					? stdin
					: fopen(toeuccode(wargv[1]), "r");
			if (yyin == NULL)
				error(FATAL, "can't open %ws", wargv[1]);
			argc--;
			wargv++;
			break;
		} else if (wargv[0][0] == L'-' && wargv[0][1] == L'F') {
			if (wargv[0][2] == L't')
				**FS = L'\t';
			else
				/* set field sep */
				**FS = wargv[0][2];
			continue;
		} else if (wargv[0][0] != L'-') {
			dprintf("cmds=|%ws|\n", wargv[0], NULL, NULL);
			yyin = NULL;
			lexprog = wargv[0];
			wargv[0] = wargv[-1];   /* need this space */
			break;
		} else if (wscmp(L_dashd, wargv[0]) == 0) {
			dbg = 1;
		}
	}
	if (argc <= 1) {
		wargv[0][0] = L'-';
		wargv[0][1] = 0;
		argc++;
		wargv--;
	}
	svargc = --argc;
	svargv = ++wargv;
	dprintf("svargc=%d svargv[0]=%ws\n", svargc, svargv[0], NULL);
	*FILENAME = *svargv;    /* initial file name */
	yyparse();
	dprintf("errorflag=%d\n", errorflag, NULL, NULL);
	/*
	 * done parsing, so now activate the LC_NUMERIC
	 */
	(void) setlocale(LC_ALL, "");
	nl_radix = nl_langinfo(RADIXCHAR);
	if (nl_radix) {
		radixpoint = (wchar_t)*nl_radix;
	}
	if (errorflag)
		exit(errorflag);
	run(winner);
	exit(errorflag);
}

yywrap()
{
	return (1);
}
