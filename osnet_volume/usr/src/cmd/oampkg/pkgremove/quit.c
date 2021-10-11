/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)quit.c	1.12	96/04/05 SMI"	/* SVr4.0 1.5.1.1 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <locale.h>
#include <libintl.h>
#include "install.h"
#include "libinst.h"
#include <pkglib.h>

#define	MAILCMD	"/usr/bin/mail"

#define	MSG_NOCHANGE	"No changes were made to the system."

#define	WRN_NOMAIL	"WARNING: unable to send e-mail notification"
#define	WRN_FLMAIL	"WARNING: e-mail notification may have failed"

/* lockinst.c */
extern void	unlockinst(void);

/* mntinfo.c */
extern int	unmount_client(void);

extern struct admin adm;
extern int	started, failflag, warnflag, dreboot, ireboot;
extern char	*msgtext, *pkginst;

void		trap(int signo), quit(int retcode);

static void	mailmsg(int retcode), quitmsg(int retcode);

void
trap(int signo)
{
	if ((signo == SIGINT) || (signo == SIGHUP))
		quit(3);
	else
		quit(1);
}

void
quit(int retcode)
{
	(void) signal(SIGINT, SIG_IGN);

	if (retcode != 99) {
		if ((retcode % 10) == 0) {
			if (failflag)
				retcode += 1;
			else if (warnflag)
				retcode += 2;
		}

		if (ireboot)
			retcode = (retcode % 10) + 20;
		if (dreboot)
			retcode = (retcode % 10) + 10;
	}

	/*
	 * In the event that this quit() was called prior to completion of
	 * the task, do an unlockinst() just in case.
	 */
	unlockinst();

	/* unmount the mounts that are our responsibility. */
	(void) unmount_client();

	/* send mail to appropriate user list */
	mailmsg(retcode);

	/* display message about this installation */
	quitmsg(retcode);
	exit(retcode);
	/*NOTREACHED*/
}

static void
quitmsg(int retcode)
{
	(void) putc('\n', stderr);
	ptext(stderr, qreason(3, retcode, 0), pkginst);

	if (retcode && !started)
		ptext(stderr, gettext(MSG_NOCHANGE));
}

static void
mailmsg(int retcode)
{
	struct utsname utsbuf;
	FILE	*pp;
	char	*cmd;

	if (!started || (adm.mail == NULL))
		return;

	cmd = calloc(strlen(adm.mail) + sizeof (MAILCMD) + 2, sizeof (char));
	if (cmd == NULL) {
		logerr(gettext(WRN_NOMAIL));
		return;
	}

	(void) sprintf(cmd, "%s %s", MAILCMD, adm.mail);
	if ((pp = popen(cmd, "w")) == NULL) {
		logerr(gettext(WRN_NOMAIL));
		return;
	}

	if (msgtext)
		ptext(pp, gettext(msgtext));

	(void) strcpy(utsbuf.nodename, gettext("(unknown)"));
	(void) uname(&utsbuf);
	ptext(pp, qreason(4, retcode, 0), pkginst, utsbuf.nodename);

	if (pclose(pp))
		logerr(gettext(WRN_FLMAIL));
}
