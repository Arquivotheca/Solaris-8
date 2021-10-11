/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)main.c 1.17	99/10/25 SMI"

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986-1990,1995-1997,1999  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */


/*
 * FTP User Program -- Command Interface.
 */
#define	EXTERN
#include "ftp_var.h"

static void timeout_sig(int sig);
static void cmdscanner(int top);
static void intr(int sig);
static char *slurpstring(void);
extern	int use_eprt;

int
main(int argc, char *argv[])
{
	register char *cp;
	int top;
	struct passwd *pw = NULL;
	char homedir[MAXPATHLEN];

	(void) setlocale(LC_ALL, "");

	buf = (char *)memalign(getpagesize(), FTPBUFSIZ);
	if (buf == NULL) {
		fprintf(stderr, "ftp: memory allocation failed\n");
		return (1);
	}

	sp = getservbyname("ftp", "tcp");
	if (sp == NULL) {
		fprintf(stderr, "ftp: ftp/tcp: unknown service\n");
		return (1);
	}
	timeoutms = timeout = 0;
	doglob = 1;
	interactive = 1;
	autologin = 1;
	sendport = -1;	/* tri-state variable. start out in "automatic" mode. */
	argc--, argv++;
	while (argc > 0 && **argv == '-') {
		for (cp = *argv + 1; *cp; cp++)
			switch (*cp) {

			case 'd':
				options |= SO_DEBUG;
				debug++;
				break;

			/* undocumented option: allows testing of EPRT */
			case 'E':
				use_eprt = 1;
				break;

			case 'v':
				verbose++;
				break;

			case 't':
				trace++;
				break;

			case 'i':
				interactive = 0;
				break;

			case 'n':
				autologin = 0;
				break;

			case 'g':
				doglob = 0;
				break;

			case 'T':
				cp++;
				if (!*cp) {
					if (argc <= 1) {
						printf("ftp: missing argument "
						    "for -T\n");
						cp--;
						break;
					}
					cp = *++argv;
					argc--;
				}
				if (!isdigit(*cp)) {
					printf("ftp: bad timeout: \"%s\"\n",
					    cp);
					break;
				}
				timeout = atoi(cp);
				timeoutms = timeout * MILLISEC;
				cp += strlen(cp) - 1;
				break;
			default:
				fprintf(stdout,
				    "ftp: %c: unknown option\n", *cp);
				return (1);
			}
		argc--, argv++;
	}
	fromatty = isatty(fileno(stdin));
	/*
	 * Set up defaults for FTP.
	 */
	(void) strcpy(typename, "ascii"), type = TYPE_A;
	(void) strcpy(formname, "non-print"), form = FORM_N;
	(void) strcpy(modename, "stream"), mode = MODE_S;
	(void) strcpy(structname, "file"), stru = STRU_F;
	(void) strcpy(bytename, "8"), bytesize = 8;
	if (fromatty)
		verbose++;
	cpend = 0;	/* no pending replies */
	proxy = 0;	/* proxy not active */
	crflag = 1;	/* strip c.r. on ascii gets */
	/*
	 * Set up the home directory in case we're globbing.
	 */
	cp = getlogin();
	if (cp != NULL) {
		pw = getpwnam(cp);
	}
	if (pw == NULL)
		pw = getpwuid(getuid());
	if (pw != NULL) {
		home = homedir;
		(void) strcpy(home, pw->pw_dir);
	}
	if (setjmp(timeralarm)) {
		(void) fflush(stdout);
		printf("Connection timeout\n");
		exit(1);
	}
	(void) signal(SIGALRM, timeout_sig);
	reset_timer();
	if (argc > 0) {
		if (setjmp(toplevel))
			return (0);
		(void) signal(SIGINT, intr);
		(void) signal(SIGPIPE, lostpeer);
		setpeer(argc + 1, argv - 1);
	}
	top = setjmp(toplevel) == 0;
	if (top) {
		(void) signal(SIGINT, intr);
		(void) signal(SIGPIPE, lostpeer);
	}

	for (;;) {
		cmdscanner(top);
		top = 1;
	}
}

void
reset_timer()
{
	/* The test is just to reduce syscalls if timeouts aren't used */
	if (timeout)
		alarm(timeout);
}

void
stop_timer()
{
	if (timeout)
		alarm(0);
}

/*ARGSUSED*/
static void
timeout_sig(int sig)
{
	longjmp(timeralarm, 1);
}

/*ARGSUSED*/
static void
intr(int sig)
{
	longjmp(toplevel, 1);
}

/*ARGSUSED*/
void
lostpeer(int sig)
{
	extern FILE *ctrl_out;
	extern int data;

	if (connected) {
		if (ctrl_out != NULL) {
			(void) shutdown(fileno(ctrl_out), 1+1);
			(void) fclose(ctrl_out);
			ctrl_out = NULL;
		}
		if (data >= 0) {
			(void) shutdown(data, 1+1);
			(void) close(data);
			data = -1;
		}
		connected = 0;
	}
	pswitch(1);
	if (connected) {
		if (ctrl_out != NULL) {
			(void) shutdown(fileno(ctrl_out), 1+1);
			(void) fclose(ctrl_out);
			ctrl_out = NULL;
		}
		connected = 0;
	}
	proxflag = 0;
	pswitch(0);
}

/*
 * Command parser.
 */
static void
cmdscanner(int top)
{
	register struct cmd *c;

	if (!top)
		(void) putchar('\n');
	for (;;) {
		stop_timer();
		if (fromatty) {
			printf("ftp> ");
			(void) fflush(stdout);
		}
		if (fgets(line, sizeof (line), stdin) == 0) {
			if (feof(stdin) || ferror(stdin))
				quit(0, NULL);
			break;
		}
		if (line[0] == 0)
			break;
		/* If not all, just discard rest of line */
		if (line[strlen(line)-1] != '\n') {
			while (fgetc(stdin) != '\n' && !feof(stdin) &&
			    !ferror(stdin))
				;
			printf("Line too long\n");
			continue;
		} else
			line[strlen(line)-1] = 0;

		makeargv();
		if (margc == 0) {
			continue;
		}
		c = getcmd(margv[0]);
		if (c == (struct cmd *)-1) {
			printf("?Ambiguous command\n");
			continue;
		}
		if (c == 0) {
			printf("?Invalid command\n");
			continue;
		}
		if (c->c_conn && !connected) {
			printf("Not connected.\n");
			continue;
		}
		reset_timer();
		(*c->c_handler)(margc, margv);
#ifndef CTRL
#define	CTRL(c) ((c)&037)
#endif
		stop_timer();
		if (bell && c->c_bell)
			(void) putchar(CTRL('g'));
		if (c->c_handler != help)
			break;
	}
	(void) signal(SIGINT, intr);
	(void) signal(SIGPIPE, lostpeer);
}

struct cmd *
getcmd(char *name)
{
	register char *p, *q;
	register struct cmd *c, *found;
	register int nmatches, longest;
	extern struct cmd cmdtab[];

	longest = 0;
	nmatches = 0;
	found = 0;
	for (c = cmdtab; (p = c->c_name) != NULL; c++) {
		for (q = name; *q == *p++; q++)
			if (*q == 0)		/* exact match? */
				return (c);
		if (!*q) {			/* the name was a prefix */
			if (q - name > longest) {
				longest = q - name;
				nmatches = 1;
				found = c;
			} else if (q - name == longest)
				nmatches++;
		}
	}
	if (nmatches > 1)
		return ((struct cmd *)-1);
	return (found);
}

/*
 * Slice a string up into argc/argv.
 */

static int slrflag;

void
makeargv(void)
{
	char **argp;

	margc = 0;
	argp = margv;
	stringbase = line;		/* scan from first of buffer */
	argbase = argbuf;		/* store from first of buffer */
	slrflag = 0;
	while (*argp++ = slurpstring()) {
		margc++;
		if (margc == sizeof (margv) / sizeof (margv[0])) {
			if (slurpstring())
				printf("Ignoring additional arguments\n");
			break;
		}
	}
}

/*
 * Parse string into argbuf;
 * implemented with FSM to
 * handle quoting and strings
 */
static char *
slurpstring(void)
{
	int got_one = 0;
	register char *sb = stringbase;
	register char *ap = argbase;
	char *tmp = argbase;		/* will return this if token found */
	int	len;

	if (*sb == '!' || *sb == '$') {	/* recognize ! as a token for shell */
		switch (slrflag) {	/* and $ as token for macro invoke */
			case 0:
				slrflag++;
				stringbase++;
				return ((*sb == '!') ? "!" : "$");
			case 1:
				slrflag++;
				altarg = stringbase;
				break;
			default:
				break;
		}
	}

S0:
	switch (*sb) {

	case '\0':
		goto OUT;

	case ' ':
	case '\t':
		sb++; goto S0;

	default:
		switch (slrflag) {
			case 0:
				slrflag++;
				break;
			case 1:
				slrflag++;
				altarg = sb;
				break;
			default:
				break;
		}
		goto S1;
	}

S1:
	switch (*sb) {

	case ' ':
	case '\t':
	case '\0':
		goto OUT;	/* end of token */

	case '\\':
		sb++; goto S2;	/* slurp next character */

	case '"':
		sb++; goto S3;	/* slurp quoted string */

	default:
		if ((len = mblen(sb, MB_CUR_MAX)) <= 0)
			len = 1;
		memcpy(ap, sb, len);
		ap += len;
		sb += len;
		got_one = 1;
		goto S1;
	}

S2:
	switch (*sb) {

	case '\0':
		goto OUT;

	default:
		if ((len = mblen(sb, MB_CUR_MAX)) <= 0)
			len = 1;
		memcpy(ap, sb, len);
		ap += len;
		sb += len;
		got_one = 1;
		goto S1;
	}

S3:
	switch (*sb) {

	case '\0':
		goto OUT;

	case '"':
		sb++; goto S1;

	default:
		if ((len = mblen(sb, MB_CUR_MAX)) <= 0)
			len = 1;
		memcpy(ap, sb, len);
		ap += len;
		sb += len;
		got_one = 1;
		goto S3;
	}

OUT:
	if (got_one)
		*ap++ = '\0';
	argbase = ap;			/* update storage pointer */
	stringbase = sb;		/* update scan pointer */
	if (got_one) {
		return (tmp);
	}
	switch (slrflag) {
		case 0:
			slrflag++;
			break;
		case 1:
			slrflag++;
			altarg = (char *)0;
			break;
		default:
			break;
	}
	return ((char *)0);
}

#define	HELPINDENT (sizeof ("directory"))

/*
 * Help command.
 * Call each command handler with argc == 0 and argv[0] == name.
 */
void
help(int argc, char *argv[])
{
	register struct cmd *c;
	extern struct cmd cmdtab[];

	if (argc == 1) {
		register int i, j, w, k;
		int columns, width = 0, lines;
		extern int NCMDS;

		printf("Commands may be abbreviated.  Commands are:\n\n");
		for (c = cmdtab; c < &cmdtab[NCMDS]; c++) {
			int len = strlen(c->c_name);

			if (len > width)
				width = len;
		}
		width = (width + 8) &~ 7;
		columns = 80 / width;
		if (columns == 0)
			columns = 1;
		lines = (NCMDS + columns - 1) / columns;
		for (i = 0; i < lines; i++) {
			for (j = 0; j < columns; j++) {
				c = cmdtab + j * lines + i;
				if (c->c_name && (!proxy || c->c_proxy)) {
					printf("%s", c->c_name);
				} else if (c->c_name) {
					for (k = 0; k < strlen(c->c_name);
					    k++) {
						(void) putchar(' ');
					}
				}
				if (c + lines >= &cmdtab[NCMDS]) {
					printf("\n");
					break;
				}
				w = strlen(c->c_name);
				while (w < width) {
					w = (w + 8) &~ 7;
					(void) putchar('\t');
				}
			}
		}
		return;
	}
	while (--argc > 0) {
		register char *arg;
		arg = *++argv;
		c = getcmd(arg);
		if (c == (struct cmd *)-1)
			printf("?Ambiguous help command %s\n", arg);
		else if (c == (struct cmd *)0)
			printf("?Invalid help command %s\n", arg);
		else
			printf("%-*s\t%s\n", HELPINDENT,
				c->c_name, c->c_help);
	}
}

/*
 * Call routine with argc, argv set from args (terminated by 0).
 */
void
call(void (*routine)(int argc, char *argv[]), ...)
{
	va_list ap;
	char *argv[10];
	register int argc = 0;

	va_start(ap, routine);
	while ((argv[argc] = va_arg(ap, char *)) != (char *)0)
		argc++;
	va_end(ap);
	(*routine)(argc, argv);
}
