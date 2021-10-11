/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)conv_fix.c	1.10	99/07/12 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <sys/file.h>

extern char *optarg;

/*
 * FUNCTION:
 *	static char *_file_getline(FILE *fp)
 * INPUT:
 *	FILE *fp - file pointer to read from
 * OUTPUT:
 *	char *(return) - an entry from the stream
 * DESCRIPTION:
 *	This routine will read in a line at a time.  If the line ends in a
 *	newline, it returns.  If the line ends in a backslash newline, it
 *	continues reading more.  It will ignore lines that start in # or
 *	blank lines.
 */
static char *
_file_getline(FILE *fp)
{
	char entry[BUFSIZ], *tmp;
	int size;

	size = sizeof (entry);
	tmp  = entry;

	/* find an entry */
	while (fgets(tmp, size, fp)) {
		if ((tmp == entry) && ((*tmp == '#') || (*tmp == '\n'))) {
			continue;
		} else {
			if ((*tmp == '#') || (*tmp == '\n')) {
				*tmp = NULL;
				break;
			}

			size -= strlen(tmp);
			tmp += strlen(tmp);

			if (*(tmp-2) != '\\')
				break;

			size -= 2;
			tmp -= 2;
		}
	}

	if (tmp == entry)
		return (NULL);
	else
		return (strdup(entry));
}

main(int ac, char *av[])
{
	int   c;
	char  file[80], ofile[80];
	char *cp;
	FILE *fp, *fp2;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(ac, av, "f:o:")) != EOF)

		switch (c) {
		case 'f':
			(void) strncpy(file, optarg, 79);
			break;
		case 'o':
			(void) strncpy(ofile, optarg, 79);
			break;
		default:
			(void) fprintf(stderr,
				gettext("Usage: %s [-f file] [-o output file]\n"),
				av[0]);
			return (1);
		}

	if ((fp = fopen(file, "r")) != NULL) {
		if ((fp2 = fopen(ofile, "a")) != NULL) {
			while ((cp = _file_getline(fp)) != NULL) {
				(void) fprintf(fp2, "%s", cp);
			}
			return (0);
		} else {
			(void) fprintf(stderr,
			    gettext("Error trying to open file.\n"));
			return (1);
		}
	} else {
		(void) fprintf(stderr,
		    gettext("Error trying to open file.\n"));
		return (1);
	}
}
