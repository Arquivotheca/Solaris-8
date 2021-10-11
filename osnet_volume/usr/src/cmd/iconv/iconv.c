/*
 * Copyright (c) 1993, 1994, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)iconv.c	1.17	97/07/31 SMI"

/*
 * iconv.c	code set conversion
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <stdlib.h>
#include <libintl.h>
#include <sys/errno.h>
#include <dirent.h>
#include <string.h>
#include "usl_iconv.h"
#include <iconv.h>
#include <locale.h>

#ifndef _LP64
#define	DIR_DATABASE	"/usr/lib/iconv"    /* default database */
#else
#define	DIR_DATABASE	"/usr/lib/iconv/sparcv9"    /* default database */
#endif
#define	FILE_DATABASE	"iconv_data"	    /* default database */
#define	MAXLINE		1282		    /* max chars in database line */
#define	MINFLDS		4		    /* min fields in database */
#define	FLDSZ		257		    /* max field size in database */

/*
 * For state dependent encodings, change the state of
 * the conversion to initial shift state.
 */
#define	INIT_SHIFT_STATE(cd, fptr, ileft, tptr, oleft) \
	{ \
	fptr = (const char *) NULL; \
	ileft = 0; \
	tptr = to; \
	oleft = BUFSIZ; \
	iconv(cd, &fptr, &ileft, &tptr, &oleft); \
	fwrite(to, 1, BUFSIZ - oleft, stdout); \
	}

extern int errno;
extern int optind;
extern int opterr;
extern char *optarg;

extern struct kbd_tab *gettab(char *, char *, char *, char *, int);
extern void	process(struct kbd_tab *, int, int);

static void	usage_iconv(void);
static int	use_iconv_func(iconv_t, FILE *, char *);
static void	use_table(char *, char *, int);
int	search_dbase(char *, char *, char *, char *, char *, char *, char *);

char 	cmd[MAXPATHLEN];			    /* program name */

void
main(int argc, char **argv)
{
	int 	c;
	FILE 	*fp;
	char		*fcode;
	char 		*tcode;
	iconv_t 	cd;
	int		errors = 0;
	int		use_iconv_func_flag = 0;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);
	fcode = (char *)NULL;
	tcode = (char *)NULL;
	c = 0;
	while ((c = getopt(argc, argv, "f:t:")) != EOF) {
		switch (c) {
			case 'f':
				fcode = optarg;
				break;

			case 't':
				tcode = optarg;
				break;

			default:
				usage_iconv();
		}
	}

	/* required arguments */
	if (!fcode || !tcode)
		usage_iconv();

	/*
	 * If the loadable conversion module is unavailable,
	 * or if there is an error using it,
	 * use the original table driven code.
	 * Otherwise, use the loadable conversion module.
	 */
	if ((cd = iconv_open(tcode, fcode)) != (iconv_t) -1) {
		use_iconv_func_flag = 1;
	}

	do {
		if (optind == argc) {
			fp = stdin;
		} else {
			/*
			 * there is an input file
			 */
			if ((fp = fopen(argv[optind], "r")) == NULL) {
				fprintf(stderr, gettext("Can't open %s\n"),
					argv[optind]);
				errors++;
				continue;
			}
		}
		if (use_iconv_func_flag == 1) {
			strcpy(cmd, argv[0]);
			if (use_iconv_func(cd, fp, cmd) != 0) {
				fprintf(stderr, gettext("Conversion error "
					"detected\n"), argv[optind]);
				errors++;
			}
		} else {
			use_table(tcode, fcode, fileno(fp));
		}

		fclose(fp);

	} while (++optind < argc);

	if (use_iconv_func_flag == 1) {
		iconv_close(cd);
	}
	exit(errors);
}

/*
 * This function uses the iconv library routines iconv_open,
 * iconv_close and iconv.  These routines use the loadable conversion
 * modules.
*/
static int
use_iconv_func(iconv_t cd, FILE *fp, char *cmd)
{
	size_t  	ileft, oleft;
	char 		from[BUFSIZ];
	char 		to[BUFSIZ];
	const char	*fptr;
	char		*tptr;
	size_t		num;

	ileft = 0;
	while ((ileft +=
		(num = fread(from + ileft, 1, BUFSIZ - ileft, fp))) > 0) {
		if (num == 0) {
			INIT_SHIFT_STATE(cd, fptr, ileft, tptr, oleft)
			return (1);
		}

		fptr = from;
		while (1) {
			tptr = to;
			oleft = BUFSIZ;

			if (iconv(cd, &fptr, &ileft, &tptr, &oleft) !=
					(size_t) -1) {
				fwrite(to, 1, BUFSIZ - oleft, stdout);
				break;
			}

			if (errno == EINVAL) {
				fwrite(to, 1, BUFSIZ - oleft, stdout);
				memcpy(from, fptr, ileft);
				break;
			} else if (errno == E2BIG) {
				fwrite(to, 1, BUFSIZ - oleft, stdout);
				continue;
			} else {		/* may be EILSEQ */
				fwrite(to, 1, BUFSIZ - oleft, stdout);
				fprintf(stderr, gettext("%s: conversion "
					"error\n"), cmd);

				INIT_SHIFT_STATE(cd, fptr, ileft, tptr, oleft)
				return (1);
			}
		}
	}

	INIT_SHIFT_STATE(cd, fptr, ileft, tptr, oleft)
	return (0);
}

static void
usage_iconv(void)
{
	fprintf(stderr, gettext("Usage: iconv -f fromcode -t tocode "
		"[file...]\n"));
	exit(1);
}

/*
 * This function uses the table driven code ported from the existing
 * iconv command.
*/
static void
use_table(char *tcode, char *fcode, int fd)
{
	char 	*d_data_base = DIR_DATABASE;
	char 	*f_data_base = FILE_DATABASE;
	char 	table[FLDSZ];
	char 	file[FLDSZ];
	struct 	kbd_tab *t;

	if (search_dbase(file, table, d_data_base, f_data_base, (char *) 0,
			fcode, tcode)) {
		/*
		 * got it so set up tables
		 */
		t = gettab(file, table, d_data_base, f_data_base, 0);
		if (!t) {
			fprintf(stderr, gettext("Cannot access conversion "
				"table %s %d\n"), table, errno);
			exit(1);
		}
		process(t, fd, 0);
	} else {
		fprintf(stderr, gettext("Not supported %s to %s\n"), fcode,
			tcode);
		exit(1);
	}
}


int
search_dbase(char *o_file, char *o_table, char *d_data_base,
	char *f_data_base, char *this_table, char *fcode, char *tcode)
{
	int fields;
	int row;
	char buff[MAXLINE];
	FILE *dbfp;
	char from[FLDSZ];
	char to[FLDSZ];
	char data_base[MAXNAMLEN];

	fields = 0;

	from[FLDSZ-1] = '\0';
	to[FLDSZ-1] = '\0';
	o_table[FLDSZ-1] = '\0';
	o_file[FLDSZ-1] =  '\0';
	buff[MAXLINE-2] = '\0';

	sprintf(data_base, "%s/%s", d_data_base, f_data_base);

	/* open database for reading */
	if ((dbfp = fopen(data_base, "r")) == NULL) {
		fprintf(stderr, "Cannot access data base %s (%d)\n",
			data_base, errno);
			exit(1);
	}

	/* start the search */
	for (row = 1; fgets(buff, MAXLINE, dbfp) != NULL; row++) {

		if (buff[MAXLINE-2] != NULL) {
			fprintf(stderr, gettext("Database Error : row %d has "
				"more than %d characters\n"), row, MAXLINE-2);
			exit(1);
		}

		fields = sscanf(buff, "%s %s %s %s", from, to, o_table, o_file);
		if (fields < MINFLDS) {
			fprintf(stderr, gettext("Database Error : row %d "
				"cannot retrieve required %d fields\n"),
				row, MINFLDS);
			exit(1);
		}

		if ((from[FLDSZ-1] != NULL) || (to[FLDSZ-1] != NULL) ||
		    (o_table[FLDSZ-1] != NULL) || (o_file[FLDSZ-1] != NULL)) {
			fprintf(stderr, gettext("Database Error : row %d has "
				"a field with more than %d characters\n"),
				row, FLDSZ-1);
			exit(1);
		}

		if (this_table) {
			if (strncmp(this_table, o_table, KBDNL) == 0) {
				fclose(dbfp);
				return (1);
			}
		} else if (strcmp(fcode, from) == 0 && strcmp(tcode, to) == 0) {
			fclose(dbfp);
			return (1);
		}
	}

	fclose(dbfp);
	return (0);
}
