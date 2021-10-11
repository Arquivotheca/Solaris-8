/*
 * Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
 *	  All Rights Reserved
 */

/*
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 */

#ident	"@(#)env.c	1.10	94/10/05 SMI"	/* SVr4.0 1.5	*/

/*
 *	env [ - ] [ name=value ]... [command arg...]
 *	set environment, then execute command (or print environment)
 *	- says start fresh, otherwise merge with inherited environment
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <locale.h>
#include <string.h>
#include <unistd.h>


static	void	Usage();
static	char	*nullp = NULL;
extern	char	**environ;


int
main(int argc, char **argv)
{
	char	**p;
	int	opt;
	int	i;


	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	/* check for non-standard "-" option */
	if ((argc > 1) && (strcmp(argv[1], "-")) == 0) {
		environ = &nullp;
		for (i = 1; i < argc; i++)
			argv[i] = argv[i+1];
		argc--;
	}

	/* get options */
	while ((opt = getopt(argc, argv, "i")) != EOF) {
		switch (opt) {
		case 'i':
			environ = &nullp;
			break;

		default:
			Usage();
		}
	}

	/* get environment strings */
	while (argv[optind] != NULL && strchr(argv[optind], '=') != NULL) {
		if (putenv(argv[optind])) {
			(void) perror(argv[optind]);
			exit(1);
		}
		optind++;
	}

	/* if no utility, output environment strings */
	if (argv[optind] == NULL) {
		p = environ;
		while (*p != NULL)
			(void) puts(*p++);
	} else {
		(void) execvp(argv[optind],  &argv[optind]);
		(void) perror(argv[0]);
		exit(((errno == ENOENT) || (errno == ENOTDIR)) ? 127 : 126);
	}
	return (0);
}


static	void
Usage()
{
	(void) fprintf(stderr, gettext(
	    "Usage: env [-i] [name=value ...] [utility [argument ...]]\n"
	    "       env [-] [name=value ...] [utility [argument ...]]\n"));
	exit(1);
}
