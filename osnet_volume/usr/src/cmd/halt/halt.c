/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)halt.c	1.19	99/03/23 SMI"

/*

		PROPRIETARY NOTICE(Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice

Notice of copyright on this source code product does not indicate
publication.

	(c) 1986, 1987, 1988, 1989  Sun Microsystems, Inc
	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
		All rights reserved.
*/

/*
 * Common code for halt(1M), poweroff(1M), and reboot(1M).  We use
 * argv[0] to determine which behavior to exhibit.
 */

#include <sys/types.h>
#include <sys/uadmin.h>
#include <libgen.h>
#include <locale.h>
#include <libintl.h>
#include <syslog.h>
#include <signal.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <utmpx.h>
#include <pwd.h>

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

extern int audit_halt_setup(int, char **);
extern int audit_halt_success(void);
extern int audit_halt_fail(void);

extern int audit_reboot_setup(void);
extern int audit_reboot_success(void);
extern int audit_reboot_fail(void);

int
main(int argc, char *argv[])
{
	char *cmdname = basename(argv[0]);
	char *ttyn = ttyname(STDERR_FILENO);

	int qflag = 0, needlog = 1, nosync = 0;
	uintptr_t mdep = NULL;
	int cmd, fcn, c, aval;
	char *usage;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	if (strcmp(cmdname, "halt") == 0) {
		(void) audit_halt_setup(argc, argv);
		usage = gettext("usage: %s [ -dlnqy ]\n");
		cmd = A_SHUTDOWN;
		fcn = AD_HALT;
	} else if (strcmp(cmdname, "poweroff") == 0) {
		(void) audit_halt_setup(argc, argv);
		usage = gettext("usage: %s [ -dlnqy ]\n");
		cmd = A_SHUTDOWN;
		fcn = AD_POWEROFF;
	} else if (strcmp(cmdname, "reboot") == 0) {
		(void) audit_reboot_setup();
		usage = gettext("usage: %s [ -dlnq ] [ boot args ]\n");
		cmd = A_SHUTDOWN;
		fcn = AD_BOOT;
	} else {
		(void) fprintf(stderr,
		    gettext("%s: not installed properly\n"), cmdname);
		return (1);
	}

	while ((c = getopt(argc, argv, "dlnqy")) != EOF) {
		switch (c) {
		case 'd':
			cmd = A_DUMP;
			break;
		case 'l':
			needlog = 0;
			break;
		case 'n':
			nosync = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'y':
			ttyn = NULL;
			break;
		default:
			/*
			 * TRANSLATION_NOTE
			 * Don't translate the words "halt" or "reboot"
			 */
			(void) fprintf(stderr, usage, cmdname);
			return (1);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 0) {
		if (fcn != AD_BOOT || argc != 1) {
			(void) fprintf(stderr, usage, cmdname);
			return (1);
		} else
			mdep = (uintptr_t)argv[0];
	}

	if (geteuid() != 0) {
		(void) fprintf(stderr,
		    gettext("%s: permission denied\n"), cmdname);
		goto fail;
	}

	if (fcn != AD_BOOT && ttyn != NULL &&
	    strncmp(ttyn, "/dev/term/", strlen("/dev/term/")) == 0) {
		/*
		 * TRANSLATION_NOTE
		 * Don't translate ``halt -y''
		 */
		(void) fprintf(stderr,
		    gettext("%s: dangerous on a dialup;"), cmdname);
		(void) fprintf(stderr,
		    gettext("use ``%s -y'' if you are really sure\n"), cmdname);
		goto fail;
	}

	if (needlog) {
		char *user = getlogin();
		struct passwd *pw;

		openlog(cmdname, 0, LOG_AUTH);
		if (user == NULL && (pw = getpwuid(getuid())) != NULL)
			user = pw->pw_name;
		if (user == NULL)
			user = "root";
		syslog(LOG_CRIT, "%sed by %s", cmdname, user);
	}

	/*
	 * We must assume success and log it before auditd is terminated.
	 */
	if (fcn == AD_BOOT)
		aval = audit_reboot_success();
	else
		aval = audit_halt_success();

	if (aval == -1) {
		(void) fprintf(stderr,
		    gettext("%s: can't turn off auditd\n"), cmdname);
		if (needlog)
			(void) sleep(5); /* Give syslogd time to record this */
	}

	(void) signal(SIGHUP, SIG_IGN);	/* for remote connections */

	/*
	 * If we're not forcing a crash dump, idle the init process.
	 */
	if (cmd != A_DUMP && kill(1, SIGTSTP) == -1) {
		/*
		 * TRANSLATION_NOTE
		 * Don't translate the word "init"
		 */
		(void) fprintf(stderr,
		    gettext("%s: can't idle init\n"), cmdname);
		goto fail;
	}

	/*
	 * Make sure we don't get stopped by a jobcontrol shell
	 * once we start killing everybody.
	 */
	(void) signal(SIGTSTP, SIG_IGN);
	(void) signal(SIGTTIN, SIG_IGN);
	(void) signal(SIGTTOU, SIG_IGN);
	(void) signal(SIGTERM, SIG_IGN);

	/*
	 * If we're not forcing a crash dump, give everyone 5 seconds to
	 * handle a SIGTERM and clean up properly.
	 */
	if (cmd != A_DUMP) {
		(void) kill(-1, SIGTERM);
		(void) sleep(5);
	}

	if (!qflag && !nosync) {
		struct utmpx wtmpx;

		bzero(&wtmpx, sizeof (struct utmpx));
		(void) strcpy(wtmpx.ut_line, "~");
		(void) time(&wtmpx.ut_tv.tv_sec);

		if (cmd == A_DUMP)
			(void) strcpy(wtmpx.ut_name, "crash dump");
		else
			(void) strcpy(wtmpx.ut_name, "shutdown");

		(void) updwtmpx(WTMPX_FILE, &wtmpx);
		sync();
	}

	if (cmd == A_DUMP && nosync != 0)
		(void) uadmin(A_DUMP, AD_NOSYNC, NULL);

	(void) uadmin(cmd, fcn, mdep);
	perror(cmdname);
	(void) kill(1, SIGHUP);		/* tell init to restate current level */

fail:
	if (fcn == AD_BOOT)
		(void) audit_reboot_fail();
	else
		(void) audit_halt_fail();

	return (1);
}
