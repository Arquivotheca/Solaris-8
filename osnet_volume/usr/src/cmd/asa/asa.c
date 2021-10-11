/*
 * Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)asa.c	1.2	97/01/28 SMI"


#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <nl_types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define	FF '\f'
#define	NL '\n'
#define	NUL '\0'

static	void usage(void);
static	void disp_file(int need_a_newline, FILE *f, char *filename);
static	FILE *get_next_file(int, char *, int, char *[]);
static	void finish(int need_a_newline, int status);

void
main(int argc, char *argv[])
{
	int c;
	int i;
	int form_feeds		= 0;
	int need_a_newline	= 0;
	char *filename		= NULL;
	register FILE *f;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"  /* Use this only if it were not */
#endif

	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "f")) != EOF) {
		switch (c) {
		case 'f':
			form_feeds = 1;
			break;

		case '?':
			usage();
		}
	}

	i = optind;
	if (argc <= i) {
		filename = NULL;
		f = stdin;
	} else {
		f = get_next_file(need_a_newline, filename, i, argv);
		++i;
	}

	need_a_newline = 0;
	for (;;) {
		/* interpret the first character in the line */

		c = getc(f);
		switch (c) {
		case EOF:
			disp_file(need_a_newline, f, filename);

			if (i >= argc)
				finish(need_a_newline, 0);

			f = get_next_file(need_a_newline, filename, i, argv);
			++i;

			if (need_a_newline) {
				(void) putchar(NL);
				need_a_newline = 0;
			}

			if (form_feeds)
				(void) putchar(FF);

			continue;

		case NL:
			if (need_a_newline)
				(void) putchar(NL);
			need_a_newline = 1;
			continue;

		case '+':
			if (need_a_newline)
				(void) putchar('\r');
			break;

		case '0':
			if (need_a_newline)
				(void) putchar(NL);
			(void) putchar(NL);
			break;

		case '1':
			if (need_a_newline)
				(void) putchar(NL);
			(void) putchar(FF);
			break;

		case ' ':
		default:
			if (need_a_newline)
				(void) putchar(NL);
			break;
		}

		need_a_newline = 0;

		for (;;) {
			c = getc(f);
			if (c == NL) {
				need_a_newline = 1;
				break;
			} else if (c == EOF) {
				disp_file(need_a_newline, f, filename);

				if (i >= argc)
					finish(need_a_newline, 0);

				f = get_next_file(need_a_newline, filename, i,
							argv);
				++i;

				if (form_feeds) {
					(void) putchar(NL);
					(void) putchar(FF);
					need_a_newline = 0;
					break;
				}
			} else {
				(void) putchar(c);
			}
		}
	}
}

static void
usage(void)
{
	(void) fprintf(stderr, gettext("usage: asa [-f] [-|file...]\n"));
	exit(1);
}


static	void
disp_file(int need_a_newline, FILE *f, char *filename)
{

	if (ferror(f)) {
		if (filename) {
			(void) fprintf(stderr, gettext(
			    "asa: read error on file %s\n"), filename);
		} else {
			(void) fprintf(stderr, gettext(
			    "asa: read error on standard input\n"));
		}
		perror("");
		finish(need_a_newline, 1);
	}

	(void) fclose(f);

}

static	FILE *
get_next_file(int need_a_newline, char *filename, int i, char *argv[])
{
	FILE	*f;
	if (strcmp(argv[i], "-") == 0) {
		filename = NULL;
		f = stdin;
	} else {
		filename = argv[i];
		f = fopen(filename, "r");
		if (f == NULL) {
			(void) fprintf(stderr,
				gettext("asa: cannot open %s:"), filename);
			perror("");
			finish(need_a_newline, 1);
		}
	}
	return (f);
}

static	void
finish(int need_a_newline, int status)
{
	if (need_a_newline)
		(void) putchar(NL);
	exit(status);
}
