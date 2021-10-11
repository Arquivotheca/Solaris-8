/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)presvr4.c	1.14	94/08/02 SMI"	/* SVr4.0  1.9.1.1	*/

/* 5-20-92	added newroot functions */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#ifdef SUNOS41
#include <sys/types.h>
#endif
#include <sys/stat.h>	/* chmod()? definition */
#include <sys/utsname.h>
#include <pkginfo.h>
#include <pkgstrct.h>
#include <pkgdev.h>
#include <pkglocs.h>
#include <locale.h>
#include <libintl.h>
#include "install.h"
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"

extern struct admin adm;
extern struct pkgdev pkgdev;
extern char	*respfile, *tmpdir;
extern int	warnflag, nointeract;
extern void	quit(int exitval);
extern void	(*func)();

int	intfchg = 0;

#define	MSG_SUCCEED	"\n## Pre-SVR4 package reports successful installation."

#define	MSG_FAIL	"\n## Pre-SVR4 package reports failed installation."

#define	MSG_MAIL	"An attempt to install the <%s> pre-SVR4 package " \
			"on <%s> completed with exit status <%d>."

#define	ERR_NOCOPY	"unable to create copy of UNINSTALL script in <%s>"

#define	ERR_NOINT	"-n option cannot be used when removing pre-SVR4 " \
			"packages"
#define	ERR_RESTFILE	"unable to restore permissions for <%s>"

#define	ERR_RESPFILE	"response file is invalid for pre-SVR4 package"

#define	ERR_PRESVR4	"The current volume is in old (pre-SVR4) format; " \
			"the PKGASK command is not able to understand this " \
			"format"

#define	INFO_DIRVER	"   %d directories successfully verified."

int
presvr4(char **ppkg)
{
	int	retcode;
	char	*tmpcmd, path[PATH_MAX];
	void	(*tmpfunc)();

	echo(gettext("*** Installing Pre-SVR4 Package ***"));
	if (nointeract) {
		progerr(gettext(ERR_NOINT));
		quit(1);
	}

	if (respfile) {
		progerr(gettext(ERR_RESPFILE));
		quit(1);
	}

	/*
	 * if we were looking for a particular package, verify
	 * the first media has a /usr/options file on it
	 * which matches
	 */
	psvr4pkg(ppkg);

	/*
	 * check to see if we can guess (via Rlist) what
	 * pathnames this package is likely to install;
	 * if we can, check these against the 'contents'
	 * file and warn the administrator that these
	 * pathnames might be modified in some manner
	 */
	psvr4cnflct();

	if (chdir(tmpdir)) {
		progerr(gettext("unable to change directory to <%s>"), tmpdir);
		quit(99);
	}

	(void) sprintf(path, "%s/install/INSTALL", pkgdev.dirname);
	tmpcmd = tempnam(tmpdir, "INSTALL");
	if (!tmpcmd || copyf(path, tmpcmd, 0L) || chmod(tmpcmd, 0500)) {
		progerr(gettext(ERR_NOCOPY), tmpdir);
		quit(99);
	}

	echo(gettext("## Executing INSTALL script provided by package"));
	tmpfunc = signal(SIGINT, func);
	retcode = pkgexecl(NULL, NULL, NULL, NULL, SHELL, "-c", tmpcmd,
	    pkgdev.bdevice, pkgdev.dirname, NULL);
	tmpfunc = signal(SIGINT, SIG_IGN);
	echo(retcode ? gettext(MSG_FAIL) : gettext(MSG_SUCCEED));

	(void) unlink(tmpcmd);
	(void) chdir("/");
	(void) pkgumount(&pkgdev);

	psvr4mail(adm.mail, gettext(MSG_MAIL), retcode, *ppkg ? *ppkg :
	    gettext("(unknown)"));
	(void) signal(SIGINT, tmpfunc);

	intfchg++;
	return (retcode);
}

void
intf_reloc(void)
{
	char	path[PATH_MAX];

	(void) sprintf(path, "%s/intf_reloc", PKGBIN);
	(void) pkgexecl(NULL, NULL, NULL, NULL, SHELL, "-c", path, NULL);
}
