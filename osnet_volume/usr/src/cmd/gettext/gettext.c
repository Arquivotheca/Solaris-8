/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)gettext.c	1.4	94/12/13 SMI"       

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

/*
 * TEXTDOMAIN should be defined in Makefile
 * in case it isn't, define it here
 */
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif

static char *
expand_metas(char *in)	/* walk thru string interpreting \n etc. */
{
	register char *out, *cp;

	for (cp = out = in; *in != NULL; out++, in++) {
		if (*in == '\\') {
			switch (*++in) {
			case 'b' :
				*out = '\b';
				break;
			case 'f' :
				*out = '\f';
				break;
			case 'n' :
				*out = '\n';
				break;
			case 'r' :
				*out = '\r';
				break;
			case 't' :
				*out = '\t';
				break;
			case 'v' :
				*out = '\v';
				break;
			default:
				*out = *in;
				break;
			}
		} else
			*out = *in;
	}
	*out = NULL;
	return (cp);
}



void
main(int argc, char *argv[])	/* shell script equivalent of gettext(3) */
{
	char *domain, *domainpath, *msgid;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);
	if (argc == 1) {
		(void) fprintf(stderr,
		    gettext("Usage: gettext [domain] \"msgid\"\n"));
		exit(-1);
	}
	argv++;
	if (argc == 3)	/* arg1 overrides env */
		domain = *argv++;
	else if ((domain = getenv("TEXTDOMAIN")) == NULL) {
		/*
		 * Just print the argument given.
		 */
		(void) printf("%s", expand_metas(*argv));
		exit(1);
	}
	if ((domainpath = getenv("TEXTDOMAINDIR")) != NULL)
		(void) bindtextdomain(domain, domainpath);
	msgid = expand_metas(*argv);
	(void) printf("%s", dgettext(domain, msgid));
	exit(*domain == NULL);
}
