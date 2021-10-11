/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)cut.c	1.20	99/05/19 SMI"	/* SVr4.0 1.11	*/
#
/* cut : cut and paste columns of a table (projection of a relation) */
/* Release 1.5; handles single backspaces as produced by nroff    */

#include <stdio.h>	/* make: cc cut.c */
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <wchar.h>
#include <stdlib.h>

#define	MAX_RANGES	MAX_INPUT	/* maximum number of ranges */
					/* as input args */

int strcmp(), atoi();

void	bfunc();
void	bnfunc();
void	cfunc();
void	ffunc();
void	process_list(char *list);
void	diag(const char *s);
void	usage(void);

wchar_t wdel = '\t';
int	dellen;
int	supflag = 0;
int	rstart[MAX_RANGES];
int	rend[MAX_RANGES];
int	nranges = 0;
FILE	*inptr;
char	dummy[MB_LEN_MAX];

wchar_t	*linebuf = NULL;
int	bufsiz;

main(argc, argv)
int	argc;
char	**argv;
{
	int	c;
	char	*p, *list;
	int	status = 0;
	int	bflag, nflag, cflag, fflag, dflag, filenr;
	void	(*funcp)();

	bflag = nflag = cflag = fflag = dflag = 0;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"	/* Use this only if it weren't. */
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "b:c:d:f:ns")) != EOF)
		switch (c) {
			case 'b':
				if (fflag || cflag)
					usage();
				bflag++;
				list = optarg;
				break;

			case 'c':
				if (fflag || bflag)
					usage();
				cflag++;
				list = optarg;
				break;

			case 'd':
				dellen = mbtowc(&wdel, optarg, MB_CUR_MAX);
				if (dellen == -1) {
					diag("no delimiter specified");
				} else if (dellen != strlen(optarg))
					diag("invalid delimiter");
				dflag++;
				break;

			case 'f':
				if (bflag || cflag)
					usage();
				fflag++;
				list = optarg;
				break;

			case 'n':
				nflag++;
				break;

			case 's':
				supflag++;
				break;

			case '?':
				usage();
		}

	argv = &argv[optind];
	argc -= optind;

	/* you must use one and only one option -b, -c, or -f */
	if (!(cflag || fflag || bflag))
		usage();

	/*
	 * Make sure combination of options is correct
	 */
	if (nflag) {
		if (cflag || fflag) {
			fprintf(stderr, gettext(
				"cut: -n may only be used with -b\n"));
			usage();
		}
	}

	if (dflag || supflag) {
		if (bflag || cflag) {
			if (dflag)
				fprintf(stderr, gettext(
					"cut: -d may only be used with -f\n"));
			if (supflag)
				fprintf(stderr, gettext(
					"cut: -s may only be used with -f\n"));
			usage();
		}
	}

	process_list(list);

	if (cflag) {
		funcp = cfunc;
	} else if (bflag) {
		if (nflag)
			funcp = bnfunc;
		else
			funcp = bfunc;
	} else { /* fflag */
		funcp = ffunc;
	}

	if (nranges == 0)
		diag("no list specified");

	filenr = 0;
	do {	/* for all input files */
		if (argc == 0 || strcmp(argv[filenr], "-") == 0)
			inptr = stdin;
		else
			if ((inptr = fopen(argv[filenr], "r")) == NULL) {
				fprintf(stderr, gettext(
					"cut: cannot open %s\n"),
						argv[filenr]);
				status = 1;
				continue;
			}
		(*funcp)();
		fclose(inptr);
	} while (++filenr < argc);
	exit(status);
}

/* parse range list argument and set-up rstart/rend array */
void
process_list(list)
char *list;
{
	int inrange = 0;
	int start = 0;
	int num = 0;
	char *rlist = list;
	char *p;
	int i, j;
	int tmp;

	/* first, parse list of ranges */
	do {
		p = rlist;
		switch (*p) {
			case '-':
				if (inrange)
					diag("invalid range specifier");

				inrange = 1;
				if (num == 0)
					start = 1;
				else {
					start = num;
					num = 0;
				}
				break;

			case '\0':
			case ',':
			case ' ':
			case '\t':
				/*
				 * this is temporary - it will change
				 * when the isblank() routine becomes
				 * available.
				 */
				if (nranges == MAX_RANGES)
					diag("too many ranges specified");

				if (inrange) {
					if (num == 0)
						num = INT_MAX;
					if (num < start)
						diag("ranges must be "
						    "increasing");
					rstart[nranges] = start;
					rend[nranges] = num;
					nranges++;
				} else {
					rstart[nranges] = num;
					rend[nranges] = num;
					nranges++;
				}

				num = 0;
				start = 0;
				inrange = 0;

				if (*p == '\0')
					continue;
				break;

			default:
				if (!isdigit(*p))
					diag("invalid character in range");
				num = atoi(p);
				while (isdigit(*rlist))
					rlist++;
				continue;
		}
		rlist++;
	} while (*p != '\0');

	/* then, consolidate ranges where possible */
	for (i = 0; i < (nranges - 1); i++) {
		for (j = i + 1; j < nranges; j++) {
			if (rstart[i] != 0 && rend[i] != 0 &&
			    (!(rend[i] < rstart[j] || rstart[i] > rend[j]))) {
				if (rstart[i] < rstart[j])
					rstart[j] = rstart[i];
				if (rend[i] > rend[j])
					rend[j] = rend[i];
				rstart[i] = 0;
				rend[i] = 0;
				break;
			}
		}
	}

	/* then, weed out the zero'ed/consolidated entries */
	for (i = 0; i < nranges; ) {
		if (rstart[i] == 0 && rend[i] == 0) {
			for (j = i; j < (nranges - 1); j++) {
				rstart[j] = rstart[j+1];
				rend[j] = rend[j+1];
			}
			nranges--;
		} else if (rstart[i] == 0 || rend[i] == 0) {
			diag("Internal error processing input");
		} else {
			i++;
		}
	}

	/* finally, sort the remaining entries */
	for (i = 0; i < (nranges - 1); i++) {
		for (j = i+1; j < nranges; j++) {
			if (rstart[i] > rend[j]) {
				tmp = rstart[i];
				rstart[i] = rstart[j];
				rstart[j] = tmp;

				tmp = rend[i];
				rend[i] = rend[j];
				rend[j] = tmp;
			}
		}
	}

#ifdef DEBUG
	/* dump ranges */
	for (i = 0; i < nranges; i++) {
		printf("Range %d - start: %d end: %d\n", i, rstart[i],
		    rend[i]);
	}
#endif
}

/* called when -c is used */
/* print out those characters selected */

void
cfunc()
{
	wint_t	c;		/* current character */
	int	pos = 0;	/* current position within line */
	int	inrange = 0;	/* is 'pos' within a range */
	int	rndx = 0;	/* current index into range table */

	while ((c = fgetwc(inptr)) != EOF) {
		if (c == '\n') {
			putchar('\n');

			/* reset per-line variables */
			pos = 0;
			inrange = 0;
			rndx = 0;
		} else {
			pos++;

			/*
			 * check if current character is within range and,
			 * if so, print it.
			 */
			if (!inrange)
				if (pos == rstart[rndx])
					inrange = 1;

			if (inrange) {
				putwchar(c);
				if (pos == rend[rndx]) {
					inrange = 0;
					rndx++;
					/*
					 * optimization -
					 * check for last range index
					 * and eat chars until newline
					 * if so.
					 */
				}
			}
		}
	}
}

void
bfunc() /* called when -b is used but -n is not */
{
	int	c;		/* current character */
	int	pos = 0;	/* current position within line */
	int	inrange = 0;	/* is 'pos' within a range */
	int	rndx = 0;	/* current index into range table */

	while ((c = getc(inptr)) != EOF) {
		if (c == L'\n') {
			putchar('\n');

			/* reset per-line variables */
			pos = 0;
			inrange = 0;
			rndx = 0;
		} else {
			pos++;

			/*
			 * check if current character is within range and,
			 * if so, print it.
			 */
			if (!inrange)
				if (pos == rstart[rndx])
					inrange = 1;

			if (inrange) {
				putchar(c);
				if (pos == rend[rndx]) {
					inrange = 0;
					rndx++;
					/*
					 * optimization -
					 * check for last range index
					 * and eat chars until newline
					 * if so.
					 */
				}
			}
		}
	}
}


void
bnfunc() /* called when -b -n is used */
{
	wint_t	c;		/* current character */
	int	pos = 0;	/* current position within line */
	int	inrange = 0;	/* is 'pos' within a range */
	int	rndx = 0;	/* current index into range table */
	int	wlen;		/* byte length of current wide char */

	while ((c = fgetwc(inptr)) != EOF) {
		if (c == '\n') {
			putchar('\n');

			/* reset per-line variables */
			pos = 0;
			inrange = 0;
			rndx = 0;
		} else {
			if (rndx >= nranges) continue;

			if ((wlen = wctomb(dummy, c)) < 0)
				diag("invalid multibyte character");
			pos += wlen;

			/*
			 * when trying to figure this out, remember that
			 * pos is actually pointing to the start byte of
			 * the next char.
			 */

			/*
			 * if char starts after beginning of range,
			 * for the moment, consider it in range.
			 */
			if (!inrange)
				if (pos >= rstart[rndx])
					inrange = 1;

			/*
			 * If tail of the multibyte is out of the range.
			 * do not print the character.
			 * (See XCU4)
			 */

			if (inrange)
				if (pos > rend[rndx]) {
					inrange = 0;
					rndx++;
				}

			/*
			 * if we still think the character is in range
			 * after the above, we print it.
			 */
			if (inrange)
				putwchar(c);

		}
	}
}

wchar_t *
read_line(FILE *fp)
{
	wint_t	c;
	wchar_t	*cp;
	int charcnt;

	/* alloc the line buffer if it isn't already there */
	if (linebuf == NULL) {
		bufsiz = BUFSIZ - 1;
		if ((linebuf = (wchar_t *)malloc((bufsiz + 1) *
		    sizeof (wchar_t))) == NULL)
			diag("unable to allocate enough memory");
	}

	cp = linebuf;
	charcnt = 0;
	while ((c = fgetwc(fp)) != EOF) {
		if (c == '\n') {
			*cp = NULL;
			return (linebuf);
		} else {
			charcnt++;
			if (charcnt == bufsiz) {
				/*
				 * there is no line length limitation so we
				 * have to be ready to expand the line buffer.
				 */
				bufsiz += BUFSIZ;
				if ((linebuf = (wchar_t *)realloc(linebuf,
				    (bufsiz + 1) * sizeof (wchar_t))) == NULL)
					diag("unable to allocate "
					    "enough memory");

				cp = linebuf + charcnt - 1;
			}
			*cp++ = c;
		}
	}

	if (cp != linebuf) {
		*cp = NULL;
		return (linebuf);
	} else
		return (NULL);
}

void
ffunc()  /* called when -f is used */
{
	int	fpos;		/* current field position within line */
	int	inrange;	/* is 'pos' within a range */
	int	rndx;		/* current index into range table */
	int	need_del;	/* need to put a delimiter char in output */
	wchar_t	*linep;		/* pointer to line buffer */
	wchar_t	*cp, *ncp;	/* working pointers into linebuf */

	while ((linep = read_line(inptr)) != NULL) {

		/* first, prune out line with no delimiters */
		if (wcschr(linep, wdel) == NULL) {
			if (!supflag)
				printf("%ws\n", linep);
			continue;
		}

		/* init per-line variable */
		fpos = 1;
		inrange = 0;
		rndx = 0;
		need_del = 0;

		for (ncp = cp = linep; ncp != NULL; fpos++) {
			/* why continue processing if no more ranges? */
			if (rndx >= nranges)
				break;

			/* find the next field delimiter */
			ncp = wcschr(cp, wdel);

			if (!inrange)
				if (fpos == rstart[rndx])
					inrange = 1;

			if (inrange) {
				if (need_del)
					putwchar(wdel);

				if (ncp == NULL) {
					/*
					 * if there are no more delimiters
					 * and we are in the range, print
					 * out the rest of the line.
					 */
					printf("%ws", cp);
					break;
				}
				else
					while (cp != ncp)
						putwchar(*cp++);
				need_del = 1;

				if (fpos == rend[rndx]) {
					inrange = 0;
					rndx++;
				}
			}

			if (ncp != NULL)
				cp = ncp + 1;
		}
		putchar('\n');
	}
}


void
diag(const char *s)
{
	fprintf(stderr, "cut: ");
	fprintf(stderr, gettext(s));
	fprintf(stderr, "\n");
	exit(2);
}


void
usage(void)
{
	fprintf(stderr, gettext(
	    "usage: cut -b list [-n] [filename ...]\n"
	    "       cut -c list [filename ...]\n"
	    "       cut -f list [-d delim] [-s] [filename]\n"));
	exit(2);
}
