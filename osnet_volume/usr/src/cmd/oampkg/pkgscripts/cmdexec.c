/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)cmdexec.c	1.13	93/05/28 SMI"	/* SVr4.0  1.1.1.1	*/
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <locale.h>
#include <libintl.h>
#include <stdlib.h>
#include <unistd.h>
#include <pkglib.h>

#define	COMMAND '!'
#define	LSIZE 256

#define	ERR_NOTROOT	"You must be \"root\" for %s to execute properly."

static void	usage(void);
static int	docmd(char *cmd, char *file, char *input);

main(int argc, char *argv[])
{
	FILE	*fpout, *fp;
	char	line[LSIZE],
		*pt,
		*keyword, 	/* keyword = install || remove */
		*input, 	/* sed input file */
		*cmd,
		*srcfile, 	/* sed data file */
		*destfile; 	/* target file to be updated */
	int	flag;
	char	*prog;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	prog = set_prog_name(argv[0]);

	if (getuid()) {
		progerr(gettext(ERR_NOTROOT), prog);
		exit(1);
	}

	if (argc != 5)
		usage();

	cmd = argv[1];
	keyword = argv[2];
	srcfile = argv[3];
	destfile = argv[4];

	srcfile = argv[3];
	if ((fp = fopen(srcfile, "r")) == NULL) {
		progerr(gettext("unable to open %s"), srcfile);
		exit(1);
	}

	input = tempnam(NULL, "sedinp");
	if ((fpout = fopen(input, "w")) == NULL) {
		progerr(gettext("unable to open %s"), input);
		exit(2);
	}

	flag = (-1);
	while (fgets(line, LSIZE, fp)) {
		for (pt = line; isspace(*pt); /* void */)
			++pt;
		if (*pt == '#')
			continue;
		if (*pt == COMMAND) {
			if (flag > 0)
				break; /* no more lines to read */
			pt = strtok(pt+1, " \t\n");
			if (!pt) {
				progerr(gettext("null token after '!'"));
				exit(1);
			}
			flag = (strcmp(pt, keyword) ? 0 : 1);
		} else if (flag == 1) { /* bug # 1083359 */
			(void) fputs(line, fpout);
		}
	}
	(void) fclose(fpout);
	if (flag > 0) {
		if (docmd(cmd, destfile, input)) {
			progerr(gettext("command failed <%s>"), cmd);
			exit(1);
		}
	}
	(void) unlink(input);
	exit(0);
	/*NOTREACHED*/
#ifdef lint
	return (0);
#endif	/* lint */
}

static int
docmd(char *cmd, char *file, char *input)
{
	char *tempout;
	char command[256];

	tempout = tempnam(NULL, "temp1");
	if (!tempout)
		return (-1);

	(void) sprintf(command, "%s -f %s <%s >%s", cmd, input, file, tempout);
	if (system(command))
		return (-1);

	(void) sprintf(command, "cp %s %s", tempout, file);
	if (system(command))
		return (-1);

	(void) unlink(tempout);
	free(tempout);
	return (0);
}

static void
usage(void)
{
	(void) fprintf(stderr, gettext("usage: %s cmd keyword src dest\n"),
	    get_prog_name());
	exit(2);
}
