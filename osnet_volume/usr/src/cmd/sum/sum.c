/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)sum.c	1.12	96/04/18 SMI"	/* SVr4.0 1.6	*/
/*
 * Sum bytes in file mod 2^16
 */


#define	WDMSK 0177777L
#define	BUFSIZE 512
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

static void usage(void);

struct part {
	short unsigned hi, lo;
};

main(argc, argv)
int argc;
char **argv;
{
	int			ca;
	int			i		= 0;
	int			alg		= 0;
	int			errflg		= 0;
	register		c;
	register		FILE *f;
	register		long long nbytes;
	register unsigned	sum;
	unsigned int		lsavhi;
	unsigned int		lsavlo;

	union hilo { /* this only works right in case short is 1/2 of long */
		struct part hl;
		long	lg;
	} tempa, suma;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "r")) != EOF)
		switch (c) {
		case 'r':
			alg = 1;
			break;
		case '?':
			usage();
		}

	argc -= optind;
	argv  = &argv[optind];

	do {
		if (i < argc) {
			if ((f = fopen(argv[i], "r")) == NULL) {
				(void) fprintf(stderr, "sum: %s ", argv[i]);
				perror("");
				errflg += 10;
				continue;
			}
		} else
			f = stdin;
		sum = 0;
		suma.lg = 0;
		nbytes = 0;
		if (alg == 1) {
			while ((c = getc(f)) != EOF) {
				nbytes++;
				if (sum & 01)
					sum = (sum >> 1) + 0x8000;
				else
					sum >>= 1;
				sum += c;
				sum &= 0xFFFF;
			}
		} else {
			while ((ca = getc(f)) != EOF) {
				nbytes++;
				suma.lg += ca & WDMSK;
			}
		}
		if (ferror(f)) {
			errflg++;
			(void) fprintf(stderr,
			gettext("sum: read error on %s\n"),
			    (argc > 0) ? argv[i] : "-");
		}
		if (alg == 1)
			(void) printf("%.5u %6lld", sum,
			    (nbytes+BUFSIZE-1)/BUFSIZE);
		else {
			tempa.lg = (suma.hl.lo & WDMSK) + (suma.hl.hi & WDMSK);
			lsavhi = (unsigned) tempa.hl.hi;
			lsavlo = (unsigned) tempa.hl.lo;
			(void) printf("%u %lld", (unsigned)(lsavhi + lsavlo),
			    (nbytes+BUFSIZE-1)/BUFSIZE);
		}
		if (argc > 0)
			(void) printf(" %s",
			    (argv[i] == (char *)0) ? "" : argv[i]);
		(void) printf("\n");
		(void) fclose(f);
	} while (++i < argc);
	return (errflg);
}

static void
usage(void)
{
	fprintf(stderr, gettext("usage: sum [-r] [file...]\n"));
	exit(2);
}
