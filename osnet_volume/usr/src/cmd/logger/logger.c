/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)logger.c	1.12	97/03/31 SMI"	/* SVr4.0 1.1	*/

/*
 *
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <limits.h>
#include <pwd.h>

#define	LOG_MARK	(LOG_NFACILITIES << 3)	/* mark "facility" */
#define	SYSLOG_LINE_LEN	1024	/* from man page syslog(3) */
#define	MIN(x, y)	((x) < (y) ? (x) : (y))
#define	LINE_LEN	MIN(SYSLOG_LINE_LEN, LINE_MAX)

struct code {
	char	*c_name;
	int	c_val;
};

static struct code	PriNames[] = {
	"panic",	LOG_EMERG,
	"emerg",	LOG_EMERG,
	"alert",	LOG_ALERT,
	"crit",		LOG_CRIT,
	"err",		LOG_ERR,
	"error",	LOG_ERR,
	"warn",		LOG_WARNING,
	"warning", 	LOG_WARNING,
	"notice",	LOG_NOTICE,
	"info",		LOG_INFO,
	"debug",	LOG_DEBUG,
	NULL,		-1
};

static struct code	FacNames[] = {
	"kern",		LOG_KERN,
	"user",		LOG_USER,
	"mail",		LOG_MAIL,
	"daemon",	LOG_DAEMON,
	"auth",		LOG_AUTH,
	"security",	LOG_AUTH,
	"mark",		LOG_MARK,
	"syslog",	LOG_SYSLOG,
	"lpr",		LOG_LPR,
	"news",		LOG_NEWS,
	"uucp",		LOG_UUCP,
	"cron",		LOG_CRON,
	"local0",	LOG_LOCAL0,
	"local1",	LOG_LOCAL1,
	"local2",	LOG_LOCAL2,
	"local3",	LOG_LOCAL3,
	"local4",	LOG_LOCAL4,
	"local5",	LOG_LOCAL5,
	"local6",	LOG_LOCAL6,
	"local7",	LOG_LOCAL7,
	NULL,		-1
};

static int	pencode(register char *);
static int	decode(char *, struct code *);
static void	bailout(char *, char *);
static void	usage(void);

/*
**  LOGGER -- read and log utility
**
**	This routine reads from an input and arranges to write the
**	result on the system log, along with a useful tag.
*/

main(argc, argv)
int argc;
char **argv;
{
	char tmp[23];
	char buf[LINE_MAX];
	char *tag;
	char *infile = NULL;
	int pri = LOG_NOTICE;
	int logflags = 0;
	int opt;
	int pid_len = 0;
	struct passwd *pw;
	uid_t u;
	char fmt_uid[16];

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);
	/* initialize */

	if ((tag = getlogin()) == NULL) {
		u = getuid();
		if ((pw = getpwuid(u)) == NULL) {
			(void) sprintf(fmt_uid, "%ld", u);
			tag = fmt_uid;
		} else
			tag = pw->pw_name;
	}
	while ((opt = getopt(argc, argv, "it:p:f:")) != EOF)
		switch (opt) {

		    case 't':		/* tag */
			tag = optarg;
			break;

		    case 'p':		/* priority */
			pri = pencode(optarg);
			break;

		    case 'i':		/* log process id also */
			logflags |= LOG_PID;
			pid_len = sprintf(tmp, "%ld", (long)getpid());
			pid_len = (pid_len <= 0) ? 0 : pid_len +2;
			break;

		    case 'f':		/* file to log */
			if (strcmp(optarg, "-") == 0)
				break;
			infile = optarg;
			if (freopen(infile, "r", stdin) == NULL)
			{
				(void) fprintf(stderr, gettext("logger: "));
				perror(infile);
				exit(1);
			}
			break;

		    default:
			usage();
		}

		argc -= optind;
		argv = &argv[optind];

	/* setup for logging */
	openlog(tag, logflags, 0);
	(void) fclose(stdout);

	/* log input line if appropriate */
	if (argc > 0)
	{
		int input_len = strlen(tag) + pid_len;

		buf[0] = '\0';
		while (argc-- > 0)
		{
			int	x;
			int	y;

			(void) strcat(buf, " ");
			input_len += strlen(*argv) +1;
			y = strlen(*argv);
			x = MIN(LINE_LEN - input_len, y);
			(void) strncat(buf, *argv++, x);
		}
		syslog(pri, "%s", buf + 1);
		exit(0);
	}

	/* main loop */
	while (fgets(buf, sizeof (buf), stdin) != NULL)
		syslog(pri, "%s", buf);

	return (0);
}

/*
 *  Decode a symbolic name to a numeric value
 */


static int
pencode(s)
register char *s;
{
	register char *p;
	int lev;
	int fac;
	char buf[100];

	for (p = buf; *s && *s != '.'; )
		*p++ = *s++;
	*p = '\0';
	if (*s++) {
		fac = decode(buf, FacNames);
		if (fac < 0)
			bailout("unknown facility name: ", buf);
		for (p = buf; *p++ = *s++; )
			continue;
	} else
		fac = 0;
	lev = decode(buf, PriNames);
	if (lev < 0)
		bailout("unknown priority name: ", buf);

	return ((lev & LOG_PRIMASK) | (fac & LOG_FACMASK));
}


static int
decode(name, codetab)
char *name;
struct code *codetab;
{
	register struct code *c;
	register char *p;
	char buf[40];

	if (isdigit(*name))
		return (atoi(name));

	(void) strcpy(buf, name);
	for (p = buf; *p; p++)
		if (isupper(*p))
			*p = tolower(*p);
	for (c = codetab; c->c_name; c++)
		if (strcmp(buf, c->c_name) == 0)
			return (c->c_val);

	return (-1);
}


static void
bailout(a, b)
char *a, *b;
{
	(void) fprintf(stderr, gettext("logger: %s%s\n"), a, b);
	exit(1);
}


static void
usage(void)
{
	(void) fprintf(stderr, gettext(
	    "Usage:\tlogger string\n"
	    "\tlogger [-i] [-f filename] [-p priority] [-t tag] "
		"[message] ...\n"));
	exit(1);
}
