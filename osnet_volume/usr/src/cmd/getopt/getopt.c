/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)getopt.c	1.7	98/02/10 SMI"	/* SVr4.0 1.7	*/

#include	<stdio.h>
#include	<locale.h>
#include	<stdlib.h>
#include	<string.h>

#define	BLOCKLEN	5120

#define	ALLOC_BUFMEM(buf, size, incr) \
	{ \
		if ((strlen(buf)+incr) >= size) { \
			size *= 2; \
			if ((buf = (char *)realloc((void *)buf, size)) \
			    == NULL) { \
				(void) fputs(gettext("getopt: Out of memory\n"), stderr); \
				exit(2); \
			} \
		} \
	}

extern void exit();
extern char *strcat();
extern char *strchr();

void
main(int argc, char **argv)
{
	extern	int optind;
	extern	char *optarg;
	register int	c;
	int	errflg = 0;
	char	tmpstr[4];
	char	*outstr;
	char	*goarg;
	size_t	bufsize;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	if (argc < 2) {
		(void) fputs(gettext("usage: getopt legal-args $*\n"), stderr);
		exit(2);
	}

	goarg = argv[1];
	argv[1] = argv[0];
	argv++;
	argc--;

	bufsize = BLOCKLEN;
	if ((outstr = (char *)malloc(bufsize)) == NULL) {
		(void) fputs(gettext("getopt: Out of memory\n"), stderr);
		exit(2);
	}
	outstr[0] = '\0';

	while ((c = getopt(argc, argv, goarg)) != EOF) {
		if (c == '?') {
			errflg++;
			continue;
		}

		tmpstr[0] = '-';
		tmpstr[1] = c;
		tmpstr[2] = ' ';
		tmpstr[3] = '\0';

		/* If the buffer is full, double its size */
		ALLOC_BUFMEM(outstr, bufsize, strlen(tmpstr));

		(void) strcat(outstr, tmpstr);

		if (*(strchr(goarg, c)+1) == ':') {
			ALLOC_BUFMEM(outstr, bufsize, strlen(optarg)+1)
			(void) strcat(outstr, optarg);
			(void) strcat(outstr, " ");
		}
	}

	if (errflg) {
		exit(2);
	}

	ALLOC_BUFMEM(outstr, bufsize, 3)
	(void) strcat(outstr, "-- ");
	while (optind < argc) {
		ALLOC_BUFMEM(outstr, bufsize, strlen(argv[optind])+1)
		(void) strcat(outstr, argv[optind++]);
		(void) strcat(outstr, " ");
	}

	(void) printf("%s\n", outstr);
	exit(0);
}
