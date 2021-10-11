/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)csplit.c	1.18	98/03/19 SMI"

/*
 * csplit - Context or line file splitter
 * Compile: cc -O -s -o csplit csplit.c
 */

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <regexpr.h>
#include <signal.h>
#include <locale.h>
#include <libintl.h>

#define	LAST	0LL
#define	ERR	-1
#define	FALSE	0
#define	TRUE	1
#define	EXPMODE	2
#define	LINMODE	3
#define	LINSIZ	LINE_MAX	/* POSIX.2 - read lines LINE_MAX long */

	/* Globals */

char *strrchr();
char linbuf[LINSIZ];		/* Input line buffer */
char *expbuf;
char tmpbuf[BUFSIZ];		/* Temporary buffer for stdin */
char file[8192] = "xx";		/* File name buffer */
char *targ;			/* Arg ptr for error messages */
char *sptr;
FILE *infile, *outfile;		/* I/O file streams */
int silent, keep, create;	/* Flags: -s(ilent), -k(eep), (create) */
int errflg;
int fiwidth = 2;		/* file index width (output file names) */
extern int optind;
extern char *optarg;
off_t offset;			/* Regular expression offset value */
off_t curline;			/* Current line in input file */

/*
 * These defines are needed for regexp handling(see regexp(7))
 */
#define	PERROR(x)	fatal("%s: Illegal Regular Expression\n", targ);

char *getline();

main(argc, argv)
int argc;
char **argv;
{
	FILE *getfile();
	int ch, mode;
	void sig();
	char *ptr;
	off_t findline();

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((ch = getopt(argc, argv, "skf:n:")) != EOF) {
		switch (ch) {
			case 'f':
				strcpy(file, optarg);
				if ((ptr = strrchr(optarg, '/')) == NULL)
					ptr = optarg;
				else
					ptr++;

				break;
			case 'n':		/* POSIX.2 */
				for (ptr = optarg; *ptr != NULL; ptr++)
					if (!isdigit((int)*ptr))
						fatal("-n num\n", NULL);
				fiwidth = atoi(optarg);
				break;
			case 'k':
				keep++;
				break;
			case 's':
				silent++;
				break;
			case '?':
				errflg++;
		}
	}

	argv = &argv[optind];
	argc -= optind;
	if (argc <= 1 || errflg)
		usage();

	if (strcmp(*argv, "-") == 0) {
		infile = tmpfile();

		while (fread(tmpbuf, 1, BUFSIZ, stdin) != 0) {
			if (fwrite(tmpbuf, 1, BUFSIZ, infile) == 0)
				if (errno == ENOSPC) {
					fprintf(stderr, "csplit: ");
					fprintf(stderr, gettext(
						"No space left on device\n"));
					exit(1);
				} else {
					fprintf(stderr, "csplit: ");
					fprintf(stderr, gettext(
						"Bad write to temporary "
							"file\n"));
					exit(1);
				}

	/* clear the buffer to get correct size when writing buffer */

			memset(tmpbuf, '\0', sizeof (tmpbuf));
		}
		rewind(infile);
	} else if ((infile = fopen(*argv, "r")) == NULL)
		fatal("Cannot open %s\n", *argv);
	++argv;
	curline = (off_t)1;
	signal(SIGINT, sig);

	/*
	 * The following for loop handles the different argument types.
	 * A switch is performed on the first character of the argument
	 * and each case calls the appropriate argument handling routine.
	 */

	for (; *argv; ++argv) {
		targ = *argv;
		switch (**argv) {
		case '/':
			mode = EXPMODE;
			create = TRUE;
			re_arg(*argv);
			break;
		case '%':
			mode = EXPMODE;
			create = FALSE;
			re_arg(*argv);
			break;
		case '{':
			num_arg(*argv, mode);
			mode = FALSE;
			break;
		default:
			mode = LINMODE;
			create = TRUE;
			line_arg(*argv);
			break;
		}
	}
	create = TRUE;
	to_line(LAST);
	exit(0);
}

/*
 * Atoll takes an ascii argument(str) and converts it to a long long(plc)
 * It returns ERR if an illegal character.  The reason that atoll
 * does not return an answer(long long) is that any value for the long
 * long is legal, and this version of atoll detects error strings.
 */

atoll(str, plc)
register char *str;
long long *plc;
{
	register int f;
	*plc = 0;
	f = 0;
	for (; ; str++) {
		switch (*str) {
		case ' ':
		case '\t':
			continue;
		case '-':
			f++;
		case '+':
			str++;
		}
		break;
	}
	for (; *str != NULL; str++)
		if (*str >= '0' && *str <= '9')
			*plc = *plc * 10 + *str - '0';
		else
			return (ERR);
	if (f)
		*plc = -(*plc);
	return (TRUE);	/* not error */
}

/*
 * Closefile prints the byte count of the file created,(via fseeko
 * and ftello), if the create flag is on and the silent flag is not on.
 * If the create flag is on closefile then closes the file(fclose).
 */

closefile()
{
	if (!silent && create) {
		fseeko(outfile, (off_t)0, SEEK_END);
		fprintf(stdout, "%lld\n", (off_t)ftello(outfile));
	}
	if (create)
		fclose(outfile);
}

/*
 * Fatal handles error messages and cleanup.
 * Because "arg" can be the global file, and the cleanup processing
 * uses the global file, the error message is printed first.  If the
 * "keep" flag is not set, fatal unlinks all created files.  If the
 * "keep" flag is set, fatal closes the current file(if there is one).
 * Fatal exits with a value of 1.
 */

fatal(string, arg)
char *string, *arg;
{
	register char *fls;
	register int num;
	char *format;
	char workstr[32];	/* must hold number of digits in fiwidth */

	fprintf(stderr, "csplit: ");

	/* gettext dynamically replaces string */

	fprintf(stderr, gettext(string), arg);
	if (!keep) {
		if (outfile) {
			fclose(outfile);
			for (fls = file; *fls != NULL; fls++);
			fls -= fiwidth;
			/*
			 * Allocate a format string of form %.0<n>d
			 * where <n> is fiwidth
			 */
			sprintf(workstr, "%d", fiwidth);
			format = (char *)malloc(3 + strlen(workstr) + 1 + 1);
			sprintf(format, "%%.0%dd", fiwidth);
			for (num = atoi(fls); num >= 0; num--) {
				sprintf(fls, format, num);
				unlink(file);
			}
			free(format);
		}
	} else
		if (outfile)
			closefile();
	exit(1);
}

/*
 * Findline returns the line number referenced by the current argument.
 * Its arguments are a pointer to the compiled regular expression(expr),
 * and an offset(oset).  The variable lncnt is used to count the number
 * of lines searched.  First the current stream location is saved via
 * ftello(), and getline is called so that R.E. searching starts at the
 * line after the previously referenced line.  The while loop checks
 * that there are more lines(error if none), bumps the line count, and
 * checks for the R.E. on each line.  If the R.E. matches on one of the
 * lines the old stream location is restored, and the line number
 * referenced by the R.E. and the offset is returned.
 */

off_t
findline(expr, oset)
register char *expr;
off_t oset;
{
	static int benhere = 0;
	off_t lncnt = 0, saveloc;

	saveloc = ftello(infile);
	if (curline != (off_t)1 || benhere)	/* If first line, first time, */
		getline(FALSE);			/* then don't skip */
	else
		lncnt--;
	benhere = 1;
	while (getline(FALSE) != NULL) {
		lncnt++;
		if ((sptr = strrchr(linbuf, '\n')) != NULL)
			*sptr = '\0';
		if (step(linbuf, expr)) {
			fseeko(infile, (off_t)saveloc, SEEK_SET);
			return (curline+lncnt+oset);
		}
	}
	fseeko(infile, (off_t)saveloc, SEEK_SET);
	return (curline+lncnt+oset+2);
}

/*
 * Flush uses fputs to put lines on the output file stream(outfile)
 * Since fputs does its own buffering, flush doesn't need to.
 * Flush does nothing if the create flag is not set.
 */

flush()
{
	if (create)
		fputs(linbuf, outfile);
}

/*
 * Getfile does nothing if the create flag is not set.  If the create
 * flag is set, getfile positions the file pointer(fptr) at the end of
 * the file name prefix on the first call(fptr=0).  The file counter is
 * stored in the file name and incremented.  If the subsequent fopen
 * fails, the file name is copied to tfile for the error message, the
 * previous file name is restored for cleanup, and fatal is called.  If
 * the fopen succeeds, the stream(opfil) is returned.
 */

FILE *
getfile()
{
	static char *fptr;
	static int ctr;
	FILE *opfil;
	char tfile[15];
	char *delim;
	char savedelim;

	if (create) {
		if (fptr == 0)
			for (fptr = file; *fptr != NULL; fptr++);
		sprintf(fptr, "%.*d", fiwidth, ctr++);

		/* check for suffix length overflow */
		if (strlen(fptr) > fiwidth) {
			fatal("Suffix longer than %d chars; increase -n\n",
			    fiwidth);
		}

		/* check for filename length overflow */

		delim = strrchr(file, '/');
		if (delim == (char *)NULL) {
			if (strlen(file) > pathconf(".", _PC_NAME_MAX)) {
				fatal("Name too long: %s\n", file);
			}
		} else {
			/* truncate file at pathname delim to do pathconf */
			savedelim = *delim;
			*delim = '\0';
			/*
			 * file: pppppppp\0fffff\0
			 *       ^ file
			 *               ^delim
			 */
			if (strlen(delim + 1) > pathconf(file, _PC_NAME_MAX)) {
				fatal("Name too long: %s\n", delim + 1);
			}
			*delim = savedelim;
		}

		if ((opfil = fopen(file, "w")) == NULL) {
			strcpy(tfile, file);
			sprintf(fptr, "%.*d", fiwidth, (ctr-2));
			fatal("Cannot create %s\n", tfile);
		}
		return (opfil);
	}
	return (NULL);
}

/*
 * Getline gets a line via fgets from the input stream "infile".
 * The line is put into linbuf and may not be larger than LINSIZ.
 * If getline is called with a non-zero value, the current line
 * is bumped, otherwise it is not(for R.E. searching).
 */

char *
getline(bumpcur)
int bumpcur;
{
	char *ret;
	if (bumpcur)
		curline++;
	ret = fgets(linbuf, LINSIZ, infile);
	return (ret);
}

/*
 * Line_arg handles line number arguments.
 * line_arg takes as its argument a pointer to a character string
 * (assumed to be a line number).  If that character string can be
 * converted to a number(long long), to_line is called with that number,
 * otherwise error.
 */

line_arg(line)
char *line;
{
	long long to;

	if (atoll(line, &to) == ERR)
		fatal("%s: bad line number\n", line);
	to_line(to);
}

/*
 * Num_arg handles repeat arguments.
 * Num_arg copies the numeric argument to "rep" (error if number is
 * larger than 20 characters or } is left off).  Num_arg then converts
 * the number and checks for validity.  Next num_arg checks the mode
 * of the previous argument, and applys the argument the correct number
 * of times. If the mode is not set properly its an error.
 */

num_arg(arg, md)
register char *arg;
int md;
{
	off_t repeat, toline;
	char rep[21];
	register char *ptr;
	int		len;

	ptr = rep;
	for (++arg; *arg != '}'; arg += len) {
		if (*arg == NULL)
			fatal("%s: missing '}'\n", targ);
		if ((len = mblen(arg, MB_LEN_MAX)) <= 0)
			len = 1;
		if ((ptr + len) >= &rep[20])
			fatal("%s: Repeat count too large\n", targ);
		memcpy(ptr, arg, len);
		ptr += len;
	}
	*ptr = NULL;
	if ((atoll(rep, &repeat) == ERR) || repeat < 0L)
		fatal("Illegal repeat count: %s\n", targ);
	if (md == LINMODE) {
		toline = offset = curline;
		for (; repeat > 0LL; repeat--) {
			toline += offset;
			to_line(toline);
		}
	} else	if (md == EXPMODE)
			for (; repeat > 0LL; repeat--)
				to_line(findline(expbuf, offset));
		else
			fatal("No operation for %s\n", targ);
}

/*
 * Re_arg handles regular expression arguments.
 * Re_arg takes a csplit regular expression argument.  It checks for
 * delimiter balance, computes any offset, and compiles the regular
 * expression.  Findline is called with the compiled expression and
 * offset, and returns the corresponding line number, which is used
 * as input to the to_line function.
 */

re_arg(string)
char *string;
{
	register char *ptr;
	register char ch;
	int		len;

	ch = *string;
	ptr = string;
	ptr++;
	while (*ptr != ch) {
		if (*ptr == '\\')
			++ptr;

		if (*ptr == NULL)
			fatal("%s: missing delimiter\n", targ);

		if ((len = mblen(ptr, MB_LEN_MAX)) <= 0)
			len = 1;
		ptr += len;
	}

	/*
	 * The line below was added because compile no longer supports
	 * the fourth argument being passed.  The fourth argument used
	 * to be '/' or '%'.
	 */

	*ptr = NULL;
	if (atoll(++ptr, &offset) == ERR)
		fatal("%s: illegal offset\n", string);

	/*
	 * The line below was added because INIT which did this for us
	 * was removed from compile in regexp.h
	 */

	string++;
	expbuf = compile(string, (char *)0, (char *)0);
	if (regerrno)
		PERROR(regerrno);
	to_line(findline(expbuf, offset));
}

/*
 * Sig handles breaks.  When a break occurs the signal is reset,
 * and fatal is called to clean up and print the argument which
 * was being processed at the time the interrupt occured.
 */

void
sig(s)
int	s;
{
	signal(SIGINT, sig);
	fatal("Interrupt - program aborted at arg '%s'\n", targ);
}

/*
 * To_line creates split files.
 * To_line gets as its argument the line which the current argument
 * referenced.  To_line calls getfile for a new output stream, which
 * does nothing if create is False.  If to_line's argument is not LAST
 * it checks that the current line is not greater than its argument.
 * While the current line is less than the desired line to_line gets
 * lines and flushes(error if EOF is reached).
 * If to_line's argument is LAST, it checks for more lines, and gets
 * and flushes lines till the end of file.
 * Finally, to_line calls closefile to close the output stream.
 */

to_line(ln)
off_t ln;
{
	outfile = getfile();
	if (ln != LAST) {
		if (curline > ln)
			fatal("%s - out of range\n", targ);
		while (curline < ln) {
			if (getline(TRUE) == NULL)
				fatal("%s - out of range\n", targ);
			flush();
		}
	} else		/* last file */
		if (getline(TRUE) != NULL) {
			flush();
			while (TRUE) {
				if (getline(TRUE) == NULL)
					break;
				flush();
			}
		} else
			fatal("%s - out of range\n", targ);
	closefile();
}

usage()
{
	fprintf(stderr, gettext(
		"usage: csplit [-ks] [-f prefix] [-n number] "
			"file arg1 ...argn\n"));
	exit(1);
}
