/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* 	Copyright(c) 1988 by Sun Microsystems, Inc.		*/
/*	All Rights Reserved.					*/


#pragma ident	"@(#)crontab.c	1.27	99/07/30 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <pwd.h>
#include <unistd.h>
#include <locale.h>
#include <nl_types.h>
#include <langinfo.h>
#include <libintl.h>
#include "cron.h"

#define	TMPFILE		"_cron"		/* prefix for tmp file */
#define	CRMODE		0400	/* mode for creating crontabs */

#define	BADCREATE	\
	"can't create your crontab file in the crontab directory."
#define	BADOPEN		"can't open your crontab file."
#define	BADSHELL	\
	"because your login shell isn't /usr/bin/sh, you can't use cron."
#define	WARNSHELL	"warning: commands will be executed using /usr/bin/sh\n"
#define	BADUSAGE	\
	"proper usage is: \n	crontab [file | -e | -l | -r ] [user]"
#define	INVALIDUSER	"you are not a valid user (no entry in /etc/passwd)."
#define	NOTALLOWED	"you are not authorized to use cron.  Sorry."
#define	NOTROOT		\
	"you must be super-user to access another user's crontab file"
#define	EOLN		"unexpected end of line."
#define	UNEXPECT	"unexpected character found in line."
#define	OUTOFBOUND	"number out of bounds."
#define	ERRSFND		"errors detected in input, no crontab file generated."
#define	ED_ERROR	\
	"     The editor indicates that an error occurred while you were\n"\
	"     editing the crontab data - usually a minor typing error.\n\n"
#define	BADREAD		"error reading your crontab file"
#define	ED_PROMPT	\
	"     Edit again, to ensure crontab information is intact (%c/%c)?\n"\
	"     ('%c' will discard edits.)"

extern int	optind, per_errno;
extern char	*xmalloc();

extern int	audit_crontab_modify(char *, char *, int);
extern int	audit_crontab_delete(char *, int);

int		err;
int		cursor;
char		*cf;
char		*tnam;
char		edtemp[5+13+1];
char		line[CTLINESIZE];
static		char	login[UNAMESIZE];
static		char	yeschr;
static		char	nochr;

static int yes(void);
static int next_field(int, int);
static void catch(int);
static void crabort(char *);
static void cerror(char *);
static void copycron(FILE *);

main(argc, argv)
int	argc;
char	**argv;
{
	int	c, r;
	int	rflag	= 0;
	int	lflag	= 0;
	int	eflag	= 0;
	int	errflg	= 0;
	char *pp;
	FILE *fp, *tmpfp;
	struct stat stbuf;
	struct passwd *pwp;
	time_t omodtime;
	char *editor;
	char buf[BUFSIZ];
	uid_t ruid;
	pid_t pid;
	int stat_loc;
	int ret;
	char real_login[UNAMESIZE];
	int tmpfd = -1;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);
	yeschr = *nl_langinfo(YESSTR);
	nochr = *nl_langinfo(NOSTR);

	while ((c = getopt(argc, argv, "elr")) != EOF)
		switch (c) {
			case 'e':
				eflag++;
				break;
			case 'l':
				lflag++;
				break;
			case 'r':
				rflag++;
				break;
			case '?':
				errflg++;
				break;
		}

	if (eflag + lflag + rflag > 1)
		errflg++;

	argc -= optind;
	argv += optind;
	if (errflg || argc > 1)
		crabort(BADUSAGE);

	ruid = getuid();
	if ((pwp = getpwuid(ruid)) == NULL)
		crabort(INVALIDUSER);

	strcpy(real_login, pwp->pw_name);

	if ((eflag || lflag || rflag) && argc == 1) {
		if ((pwp = getpwnam(*argv)) == NULL)
			crabort(INVALIDUSER);

		if (!chkauthattr(CRONADMIN_AUTH, real_login)) {
			if (pwp->pw_uid != ruid)
				crabort(NOTROOT);
			else
				pp = getuser(ruid);
		} else
			pp = *argv++;
	} else {
		pp = getuser(ruid);
	}

	if (pp == NULL) {
		if (per_errno == 2)
			crabort(BADSHELL);
		else
			crabort(INVALIDUSER);
	}

	strcpy(login, pp);
	if (!allowed(login, CRONALLOW, CRONDENY))
		crabort(NOTALLOWED);

	cf = xmalloc(strlen(CRONDIR)+strlen(login)+2);
	strcat(strcat(strcpy(cf, CRONDIR), "/"), login);
	if (rflag) {
		r = unlink(cf);
		sendmsg(DELETE, login, login, CRON);
		audit_crontab_delete(cf, r);
		exit(0);
	}
	if (lflag) {
		if ((fp = fopen(cf, "r")) == NULL)
			crabort(BADOPEN);
		while (fgets(line, CTLINESIZE, fp) != NULL)
			fputs(line, stdout);
		fclose(fp);
		exit(0);
	}
	if (eflag) {
		if ((fp = fopen(cf, "r")) == NULL) {
			if (errno != ENOENT)
				crabort(BADOPEN);
		}
		(void) strcpy(edtemp, "/tmp/crontabXXXXXX");
		tmpfd = mkstemp(edtemp);
		if (fchown(tmpfd, ruid, -1) == -1) {
			(void) close(tmpfd);
			crabort("fchown of temporary file failed");
		}
		(void) close(tmpfd);
		/*
		 * Fork off a child with user's permissions,
		 * to edit the crontab file
		 */
		if ((pid = fork()) == (pid_t)-1)
			crabort("fork failed");
		if (pid == 0) {		/* child process */
			/* give up super-user privileges. */
			setuid(ruid);
			if ((tmpfp = fopen(edtemp, "w")) == NULL)
				crabort("can't create temporary file");
			if (fp != NULL) {
				/*
				 * Copy user's crontab file to temporary file.
				 */
				while (fgets(line, CTLINESIZE, fp) != NULL) {
					fputs(line, tmpfp);
					if (ferror(tmpfp)) {
						fclose(fp);
						fclose(tmpfp);
						crabort("write error on"
						    "temporary file");
					}
				}
				if (ferror(fp)) {
					fclose(fp);
					fclose(tmpfp);
					crabort(BADREAD);
				}
				fclose(fp);
			}
			if (fclose(tmpfp) == EOF)
				crabort("write error on temporary file");
			if (stat(edtemp, &stbuf) < 0)
				crabort("can't stat temporary file");
			omodtime = stbuf.st_mtime;
			editor = getenv("VISUAL");
			if (editor == NULL)
				editor = getenv("EDITOR");
			if (editor == NULL)
				editor = "ed";
			(void) snprintf(buf, sizeof (buf),
				"%s %s", editor, edtemp);
			sleep(1);

			while (1) {
				ret = system(buf);
				/* sanity checks */
				if ((tmpfp = fopen(edtemp, "r")) == NULL)
				    crabort("can't open temporary file");
				if (fstat(fileno(tmpfp), &stbuf) < 0)
				    crabort("can't stat temporary file");
				if (stbuf.st_size == 0)
				    crabort("temporary file empty");
				if (omodtime == stbuf.st_mtime) {
				    (void) unlink(edtemp);
				    fprintf(stderr, gettext(
					"The crontab file was not changed.\n"));
				    exit(1);
				}
				if ((ret) && (errno != EINTR)) {
				/*
				 * Some editors (like 'vi') can return
				 * a non-zero exit status even though
				 * everything is okay. Need to check.
				 */
				fprintf(stderr, gettext(ED_ERROR));
				fflush(stderr);
				if (isatty(fileno(stdin))) {
				    /* Interactive */
					fprintf(stdout, gettext(ED_PROMPT),
					    yeschr, nochr, nochr);
					fflush(stdout);

					if (yes()) {
						/* Edit again */
						continue;
					} else {
						/* Dump changes */
						(void) unlink(edtemp);
						exit(1);
					}
				} else {
				    /* Non-interactive, dump changes */
				    (void) unlink(edtemp);
				    exit(1);
				}
			}
			exit(0);
			} /* while (1) */
		}

		/* fix for 1125555 - ignore common signals while waiting */
		(void) signal(SIGINT, SIG_IGN);
		(void) signal(SIGHUP, SIG_IGN);
		(void) signal(SIGQUIT, SIG_IGN);
		(void) signal(SIGTERM, SIG_IGN);
		wait(&stat_loc);
		if ((stat_loc & 0xFF00) != 0)
			exit(1);

		if ((seteuid(ruid) < 0) ||
		    ((tmpfp = fopen(edtemp, "r")) == NULL)) {
			fprintf(stderr, "crontab: %s: %s\n",
			    edtemp, errmsg(errno));
			(void) unlink(edtemp);
			exit(1);
		} else
			seteuid(0);

		copycron(tmpfp);
		(void) unlink(edtemp);
	} else {
		if (argc == 0)
			copycron(stdin);
		else if (seteuid(getuid()) != 0 || (fp = fopen(argv[0], "r"))
		    == NULL)
			crabort(BADOPEN);
		else {
			seteuid(0);
			copycron(fp);
		}
	}
	sendmsg(ADD, login, login, CRON);
/*
	if (per_errno == 2)
		fprintf(stderr, gettext(WARNSHELL));
*/
	return (0);
}

static void
copycron(fp)
FILE *fp;
{
	FILE *tfp;
	char pid[6], *tnam_end;
	int t;

	sprintf(pid, "%-5d", getpid());
	tnam = xmalloc(strlen(CRONDIR)+strlen(TMPFILE)+7);
	strcat(strcat(strcat(strcpy(tnam, CRONDIR), "/"), TMPFILE), pid);
	/* cut trailing blanks */
	tnam_end = strchr(tnam, ' ');
	if (tnam_end != NULL)
		*tnam_end = 0;
	/* catch SIGINT, SIGHUP, SIGQUIT signals */
	if (signal(SIGINT, catch) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
	if (signal(SIGHUP, catch) == SIG_IGN) signal(SIGHUP, SIG_IGN);
	if (signal(SIGQUIT, catch) == SIG_IGN) signal(SIGQUIT, SIG_IGN);
	if (signal(SIGTERM, catch) == SIG_IGN) signal(SIGTERM, SIG_IGN);
	if ((t = creat(tnam, CRMODE)) == -1) crabort(BADCREATE);
	if ((tfp = fdopen(t, "w")) == NULL) {
		unlink(tnam);
		crabort(BADCREATE);
	}
	err = 0;	/* if errors found, err set to 1 */
	while (fgets(line, CTLINESIZE, fp) != NULL) {
		cursor = 0;
		while (line[cursor] == ' ' || line[cursor] == '\t')
			cursor++;
		/* fix for 1039689 - treat blank line like a comment */
		if (line[cursor] == '#' || line[cursor] == '\n')
			goto cont;
		if (next_field(0, 59)) continue;
		if (next_field(0, 23)) continue;
		if (next_field(1, 31)) continue;
		if (next_field(1, 12)) continue;
		if (next_field(0, 06)) continue;
		if (line[++cursor] == '\0') {
			cerror(EOLN);
			continue;
		}
cont:
		if (fputs(line, tfp) == EOF) {
			unlink(tnam);
			crabort(BADCREATE);
		}
	}
	fclose(fp);
	fclose(tfp);

	/* audit differences between old and new crontabs */
	audit_crontab_modify(cf, tnam, err);

	if (!err) {
		/* make file tfp the new crontab */
		unlink(cf);
		if (link(tnam, cf) == -1) {
			unlink(tnam);
			crabort(BADCREATE);
		}
	} else
		fprintf(stderr, "crontab: %s\n", gettext(ERRSFND));
	unlink(tnam);
}

static int
next_field(lower, upper)
int lower, upper;
{
	int num, num2;

	while ((line[cursor] == ' ') || (line[cursor] == '\t')) cursor++;
	if (line[cursor] == '\0') {
		cerror(EOLN);
		return (1);
	}
	if (line[cursor] == '*') {
		cursor++;
		if ((line[cursor] != ' ') && (line[cursor] != '\t')) {
			cerror(UNEXPECT);
			return (1);
		}
		return (0);
	}
	while (TRUE) {
		if (!isdigit(line[cursor])) {
			cerror(UNEXPECT);
			return (1);
		}
		num = 0;
		do {
			num = num*10 + (line[cursor]-'0');
		} while (isdigit(line[++cursor]));
		if ((num < lower) || (num > upper)) {
			cerror(OUTOFBOUND);
			return (1);
		}
		if (line[cursor] == '-') {
			if (!isdigit(line[++cursor])) {
				cerror(UNEXPECT);
				return (1);
			}
			num2 = 0;
			do {
				num2 = num2*10 + (line[cursor]-'0');
			} while (isdigit(line[++cursor]));
			if ((num2 < lower) || (num2 > upper)) {
				cerror(OUTOFBOUND);
				return (1);
			}
		}
		if ((line[cursor] == ' ') || (line[cursor] == '\t')) break;
		if (line[cursor] == '\0') {
			cerror(EOLN);
			return (1);
		}
		if (line[cursor++] != ',') {
			cerror(UNEXPECT);
			return (1);
		}
	}
	return (0);
}

static void
cerror(msg)
char *msg;
{
	fprintf(stderr, gettext("%scrontab: error on previous line; %s\n"),
	    line, msg);
	err = 1;
}


static void
catch(int x)
{
	unlink(tnam);
	exit(1);
}

static void
crabort(msg)
char *msg;
{
	int sverrno;

	if (strcmp(edtemp, "") != 0) {
		sverrno = errno;
		(void) unlink(edtemp);
		errno = sverrno;
	}
	if (tnam != NULL) {
		sverrno = errno;
		(void) unlink(tnam);
		errno = sverrno;
	}
	fprintf(stderr, "crontab: %s\n", gettext(msg));
	exit(1);
}

static int
yes(void)
{
	int	first_char;
	int	dummy_char;

	first_char = dummy_char = getchar();
	while ((dummy_char != '\n')	&&
	    (dummy_char != '\0')	&&
	    (dummy_char != EOF))
		dummy_char = getchar();
	return (first_char == yeschr);
}
