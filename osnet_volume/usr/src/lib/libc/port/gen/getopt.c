/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 *	Copyright (c) 1992-1996 Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)getopt.c	1.19	98/01/30 SMI"	/* SVr4.0 1.23	*/

/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
#pragma weak getopt = _getopt

#include "synonyms.h"
#include "_libc_gettext.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define	EOF	(-1)
#define	ERR(s, argv0, c)	if (opterr) {\
	char errbuf[256]; \
	(void) snprintf(errbuf, sizeof(errbuf), s, argv0, c); \
	(void) write(2, errbuf, strlen(errbuf)); }
/*
 * The argv0 argument is here for a readability purpose.  The format
 * string s would be easier to understand in this way.
 */

/*
 * If building the regular library, pick up the defintions from this file
 * If building the shared library, pick up definitions from opt_data.c
 */

extern int opterr, optind, optopt;
extern char *optarg;
int _sp = 1;

int
getopt(int argc, char *const *argv, const char *opts)
{
	char c;
	char *cp;

	if (_sp == 1) {
		if (optind >= argc || argv[optind][0] != '-' ||
		    argv[optind] == NULL || argv[optind][1] == '\0')
			return (EOF);
		else if (strcmp(argv[optind], "--") == NULL) {
			optind++;
			return (EOF);
		}
	}
	optopt = c = (unsigned char)argv[optind][_sp];
	if (c == ':' || (cp = strchr(opts, c)) == NULL) {
		if (opts[0] != ':')
			ERR(_libc_gettext("%s: illegal option -- %c\n"), \
			    argv[0], c);
		if (argv[optind][++_sp] == '\0') {
			optind++;
			_sp = 1;
		}
		return ('?');
	}
	if (*(cp + 1) == ':') {
		if (argv[optind][_sp+1] != '\0')
			optarg = &argv[optind++][_sp+1];
		else if (++optind >= argc) {
			if (opts[0] != ':')
				ERR(_libc_gettext( \
					"%s: option requires an argument" \
					    " -- %c\n"), argv[0], c);
			_sp = 1;
			optarg = NULL;
			return (opts[0] == ':' ? ':' : '?');
		} else
			optarg = argv[optind++];
		_sp = 1;
	} else {
		if (argv[optind][++_sp] == '\0') {
			_sp = 1;
			optind++;
		}
		optarg = NULL;
	}
	return (c);
}
