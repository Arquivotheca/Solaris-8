/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)gettxt.c	1.6	92/07/14 SMI"	/* SVr4.0 1.4	*/

#include <stdio.h>
#include <locale.h>

extern	char	*gettxt();
extern char	*strccpy();
extern	char	*malloc();

main(argc, argv)
int	argc;
char	*argv[];
{
	char	*dfltp;
	char	*locp;

	locp = setlocale(LC_ALL, "");
	if (locp == (char *)NULL) {
		(void)setlocale(LC_CTYPE, "");
		(void)setlocale(LC_MESSAGES, "");
	}
#if !defined(TEXT_DOMAIN)
#define TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);
	if (argc != 2 && argc != 3) {
		fprintf(stderr, gettext("Incorrect usage.\n"));
		fprintf(stderr, 
		gettext("usage: gettxt msgid [ dflt_msg ] \n"));
		exit(1);
	}


	if (argc == 2) {
		fputs(gettxt(argv[1], ""), stdout);
		exit(0);
	}

	if ((dfltp = malloc(strlen(argv[2] + 1))) == (char *)NULL) {
		fprintf(stderr, gettext("malloc failed\n"));
		exit(1);
	}

	strccpy(dfltp, argv[2]);

	fputs(gettxt(argv[1], dfltp), stdout);

	(void)free(dfltp);

	exit(0);
}
