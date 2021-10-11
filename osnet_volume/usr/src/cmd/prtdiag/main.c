/*
 * Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)main.c	1.19	99/10/19 SMI"	/* SVr4.0 1.7 */

#include	<stdio.h>
#include	<locale.h>
#include	<stdlib.h>
#include	<libintl.h>
#include 	<string.h>
#include	<unistd.h>
#include 	<sys/openpromio.h>

/*
 * function prototypes
 */
extern int	do_prominfo(int syserrlog, char *progname,
		    int logging, int print_flag);
static char	*setprogname(char *name);

void
main(int argc, char *argv[])
{
	int	c;
	int	syserrlog = 0;
	char	*usage = "%s [ -v ] [ -l ]\n";
	char	*progname;
	int	print_flag = 1;
	int	logging = 0;

	/* set up for internationalization */
	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	progname = setprogname(argv[0]);
	while ((c = getopt(argc, argv, "vl")) != -1)  {
		switch (c)  {
		case 'v':
			++syserrlog;
			break;

		case 'l':
			logging = 1;
			break;

		default:
			(void) fprintf(stderr, usage, progname);
			exit(1);
			/*NOTREACHED*/
		}
	}

	/*
	 * for sun4d do_prominfo() is part of the prtdiag cmd itself
	 * for sun4u do_prominfo() is in libprtdiag
	 */
	exit(do_prominfo(syserrlog, progname, logging, print_flag));
}

static char *
setprogname(char *name)
{
	char	*p;

	if (p = strrchr(name, '/'))
		return (p + 1);
	else
		return (name);
}
