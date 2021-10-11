/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)check.c	1.12	98/10/22 SMI"	/* SVr4.0  1.7.1.1	*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifndef SUNOS41
#include <utmpx.h>
#endif
#include <dirent.h>
#include <sys/types.h>
#include <pkgstrct.h>
#include <locale.h>
#include <libintl.h>
#include "install.h"
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"

extern struct admin adm;
/* extern struct cfent **eptlist; */
extern int	nointeract;
extern char	pkgloc[], *pkginst, *msgtext;

extern void	quit(int exitval);

#define	ERR_RUNSTATE	"unable to determine current run-state"

#define	MSG_DEPEND	"Dependency checking failed."
#define	HLP_DEPEND	"Other packages currently installed on the system " \
			"have indicated a dependency on the package being " \
			"removed.  If removal of this package occurs, it " \
			"may render other packages inoperative.  If you " \
			"wish to disregard this dependency, answer 'y' to " \
			"continue the package removal process."

#define	MSG_RUNLEVEL	"\\nThe current run-level of this machine is <%s>, " \
			"which is not a run-level suggested for removal of " \
			"this package.  Suggested run-levels (in order of " \
			"preference) include:"
#define	HLP_RUNLEVEL	"If this package is not removed in a run-level " \
			"which has been suggested, it is possible that the " \
			"package may not remove properly.  If you wish to " \
			"follow the run-level suggestions, answer 'n' to " \
			"stop the package removal process."
#define	MSG_STATECHG	"\\nTo change states, execute\\n\\tshutdown -y -i%s " \
			"-g0\\nafter exiting the package removal process. " \
			"Please note that after changing states you " \
			"may have to mount appropriate filesystem(s) " \
			"in order to remove this package."

#define	MSG_PRIV	"\\nThis package contains scripts which will be " \
			"executed with super-user permission during the " \
			"process of removing this package."
#define	HLP_PRIV	"During the removal of this package, certain " \
			"scripts provided with the package will execute " \
			"with super-user permission.  These scripts may " \
			"modify or otherwise change your system without your " \
			"knowledge.  If you are certain of the origin of the " \
			"package being removed and trust its worthiness, " \
			"answer 'y' to continue the package removal process."
#define	ASK_CONTINUE	"Do you want to continue with the removal of this " \
			"package"

#ifndef SUNOS41
void
rckrunlevel(void)
{
	struct utmpx utmpx;
	struct utmpx *putmpx;
	char	ans[MAX_INPUT], *pt, *rstates, *pstate;
	int	n;
	char	*uxstate;

	if (ADM(runlevel, "nocheck"))
		return;

	pt = getenv("RSTATES");
	if (pt == NULL)
		return;

	utmpx.ut_type = RUN_LVL;
	putmpx = getutxid(&utmpx);
	if (putmpx == NULL) {
		progerr(gettext(ERR_RUNSTATE));
		quit(99);
	}
	uxstate = strtok(&putmpx->ut_line[10], " \t\n");

	rstates = qstrdup(pt);
	if ((pt = strtok(pt, " \t\n, ")) == NULL)
		return; /* no list is no list */
	pstate = pt;
	do {
		if (strcmp(pt, uxstate) == NULL) {
			free(rstates);
			return;
		}
	} while (pt = strtok(NULL, " \t\n, "));

	msgtext = gettext(MSG_RUNLEVEL);
	ptext(stderr, msgtext, uxstate);
	pt = strtok(rstates, " \t\n, ");
	do {
		ptext(stderr, "\\t%s", pt);
	} while (pt = strtok(NULL, " \t\n, "));
	free(rstates);
	if (ADM(runlevel, "quit"))
		quit(4);
	if (nointeract)
		quit(5);
	msgtext = NULL;

	if (n = ckyorn(ans, NULL, NULL, gettext(HLP_RUNLEVEL),
	    gettext(ASK_CONTINUE)))
		quit(n);

	if (strchr("yY", *ans) != NULL)
		return;
	else {
		ptext(stderr, gettext(MSG_STATECHG), pstate);
		quit(3);
	}
}
#endif

void
rckdepend(void)
{
	int	n;
	char	ans[MAX_INPUT];

	if (ADM(rdepend, "nocheck"))
		return;

	echo(gettext("## Verifying package dependencies."));
	if (dockdeps(pkginst, 1)) {
		msgtext = gettext(MSG_DEPEND);
		echo(msgtext);
		if (ADM(rdepend, "quit"))
			quit(4);
		if (nointeract)
			quit(5);
		msgtext = NULL;

		if (n = ckyorn(ans, NULL, NULL, gettext(HLP_DEPEND),
		    gettext(ASK_CONTINUE)))
			quit(n);
		if (strchr("yY", *ans) == NULL)
			quit(3);
	}
}

void
rckpriv(void)
{
	struct dirent *dp;
	DIR	*dirfp;
	int	n, found;
	char	ans[MAX_INPUT], path[PATH_MAX];

	if (ADM(action, "nocheck"))
		return;

	(void) sprintf(path, "%s/install", pkgloc);
	if ((dirfp = opendir(path)) == NULL)
		return;

	found = 0;
	while ((dp = readdir(dirfp)) != NULL) {
		if ((strcmp(dp->d_name, "preremove") == NULL) ||
		    (strcmp(dp->d_name, "postremove") == NULL) ||
		    (strncmp(dp->d_name, "r.", 2) == NULL)) {
			found++;
			break;
		}
	}
	(void) closedir(dirfp);

	if (found) {
		ptext(stderr, gettext(MSG_PRIV));
		msgtext = gettext("Package scripts were found.");
		if (ADM(action, "quit"))
			quit(4);
		if (nointeract)
			quit(5);
		msgtext = NULL;

		if (n = ckyorn(ans, NULL, NULL, gettext(HLP_PRIV),
		    gettext(ASK_CONTINUE)))
			quit(n);
		if (strchr("yY", *ans) == NULL)
			quit(3);
	}
}
