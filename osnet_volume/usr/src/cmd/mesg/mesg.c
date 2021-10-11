/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)mesg.c	1.6	94/10/11 SMI"	/* SVr4.0 1.5	*/

/*
 * mesg -- set current tty to accept or
 *	forbid write permission.
 *
 *	mesg [-y | -n | y | n]
 *		y allow messages
 *		n forbid messages
 *	return codes
 *		0 if messages are ON or turned ON
 *		1 if messages are OFF or turned OFF
 *		2 if an error occurs
 */

#include <stdio.h>
#include <locale.h>
#include <libintl.h>
#include <sys/types.h>
#include <sys/stat.h>

struct stat sbuf;

char *tty;
char *ttyname();

main(argc, argv)
char *argv[];
{
	int i, c, r = 0;
	int action = 0;
	extern int optind;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	/*
	 * Check stdin, stdout and stderr, in order, for a tty
	 */
	for (i = 0; i <= 2; i++) {
		if ((tty = ttyname(i)) != NULL)
			break;
	}

	if (stat(tty, &sbuf) < 0)
		error("cannot stat");

	if (argc < 2) {
		if (sbuf.st_mode & (S_IWGRP | S_IWOTH))
			printf("is y\n");
		else  {
			r = 1;
			printf("is n\n");
		}
		exit(r);
	}

	while ((c = getopt(argc, argv, "yn")) != EOF) {
		switch (c) {
		case 'y':
			if (action > 0)
				usage();

			newmode(S_IRUSR | S_IWUSR | S_IWGRP);
			action++;
			break;

		case 'n':
			if (action > 0)
				usage();

			newmode(S_IRUSR | S_IWUSR);
			r = 1;
			action++;
			break;

		case '?':
			usage();
			break;
		}
	}

	/*
	 * Required for POSIX.2
	 */
	if (argc > optind) {
		if (action > 0)
			usage();

		switch (*argv[optind]) {
		case 'y':
			newmode(S_IRUSR | S_IWUSR | S_IWGRP);
			break;

		case 'n':
			newmode(S_IRUSR | S_IWUSR);
			r = 1;
			break;

		default:
			usage();
			break;
		}
	}

	exit(r);
}

error(s)
char *s;
{
	fprintf(stderr, "mesg: ");
	fprintf(stderr, "%s\n", s);
	exit(2);
}

newmode(m)
mode_t m;
{
	if (chmod(tty, m) < 0)
		error("cannot change mode");
}

usage()
{
	fprintf(stderr, gettext("usage: mesg [-y | -n | y | n]\n"));
	exit(2);
}
