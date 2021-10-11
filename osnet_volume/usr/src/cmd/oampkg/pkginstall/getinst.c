/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)getinst.c	1.14	96/03/06 SMI"	/* SVr4.0 1.8 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <valtools.h>
#include <locale.h>
#include <libintl.h>
#include <pkginfo.h>
#include "install.h"
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"
#include "pkginstall.h"

extern struct admin adm;
extern char	*pkgarch, *pkgvers, *msgtext, *pkgabrv;
extern int	opresvr4, update, maxinst, nointeract;

#define	MSG_UNIQ1	"\\nCurrent administration requires that a unique " \
			"instance of the <%s> package be created.  However, " \
			"the maximum number of instances of the package " \
			"which may be supported at one time on the same " \
			"system has already been met."

#define	MSG_NOINTERACT	"\\nUnable to determine whether to overwrite an " \
			"existing package instance, or add a new instance."

#define	MSG_NEWONLY	"\\nA version of the <%s> package is already " \
			"installed on this machine.  Current administration " \
			"does not allow new instances of an existing package " \
			"to be created, nor existing instances to be " \
			"overwritten."

#define	MSG_SAME	"\\nThis appears to be an attempt to install the " \
			"same architecture and version of a package which " \
			"is already installed.  This installation will " \
			"attempt to overwrite this package.\\n"

#define	MSG_OVERWRITE	"\\nCurrent administration does not allow new " \
			"instances of an existing package to be created.  " \
			"However, the installation service was unable to " \
			"determine which package instance to overwrite."

static char	newinst[15];
static char	*nextinst(void);
static char	*prompt(struct pkginfo *info, int npkgs);
static int	same_pkg;	/* same PKG, ARCH and VERSION */

/*
 * This returns the correct package instance based on how many packages are
 * already installed. If there are none (npkgs == 0), it just returns the
 * package abbreviation. Otherwise, it interacts with the user (or reads the
 * admin file) to determine if we should overwrite an instance which is
 * already installed, or possibly install a new instance of this package
 */
char *
getinst(struct pkginfo *info, int npkgs)
{
	int	i, samearch, nsamearch;
	char	*inst, *sameinst;

	same_pkg = 0;

	/*
	 * If this is the first instance of the package, it's called the by
	 * the package abbreviation.
	 */
	if (npkgs == 0)
		return (pkgabrv);

	if (ADM(instance, "newonly") || ADM(instance, "quit")) {
		msgtext = gettext(MSG_NEWONLY);
		ptext(stderr, msgtext, pkgabrv);
		quit(4);
	}

	samearch = nsamearch = 0;
	sameinst  = NULL;
	for (i = 0; i < npkgs; i++) {
		if (strcmp(info[i].arch, pkgarch) == NULL) {
			samearch = i;
			nsamearch++;
			if (strcmp(info[i].version, pkgvers) == NULL)
				sameinst = info[i].pkginst;
		}
	}

	if (sameinst) {
		ptext(stderr, gettext(MSG_SAME));
		inst = sameinst; /* can't be overwriting a pre-svr4 package */
		same_pkg++;
		update++;
	} else if (ADM(instance, "overwrite")) {
		if (npkgs == 1)
			samearch = 0; /* use only package we know about */
		else if (nsamearch != 1) {
			/*
			 * more than one instance of
			 * the same ARCH is already
			 * installed on this machine
			 */
			msgtext = gettext(MSG_OVERWRITE);
			ptext(stderr, msgtext);
			quit(4);
		}
		inst = info[samearch].pkginst;
		if (info[samearch].status == PI_PRESVR4)
			opresvr4++; /* overwriting a pre-svr4 package */
		update++;
	} else if (ADM(instance, "unique")) {
		if (maxinst <= npkgs) {
			/* too many instances */
			msgtext = gettext(MSG_UNIQ1);
			ptext(stderr, msgtext, pkgabrv);
			quit(4);
		}
		inst = nextinst();
	} else if (nointeract) {
		msgtext = gettext(MSG_NOINTERACT);
		ptext(stderr, msgtext);
		quit(5);
	} else {
		inst = prompt(info, npkgs);
		if (strcmp(inst, "new") == NULL)
			inst = nextinst();
		else {
			update++;
			/* see if this instance is presvr4 */
			for (i = 0; i < npkgs; i++) {
				if (strcmp(inst, info[i].pkginst) == NULL) {
					if (info[i].status == PI_PRESVR4)
						opresvr4++;
					break;
				}
			}
		}
	}
	return (inst);
}

/*
 * This informs the caller whether the package in question is the same
 * version and architecture as an installed package of the same name.
 */
int
is_samepkg(void) {
	return (same_pkg);
}

static char *
nextinst(void)
{
	struct pkginfo info;
	int	n;

	n = 2; /* requirements say start at 2 */

	info.pkginst = NULL;
	(void) strcpy(newinst, pkgabrv);
	while (pkginfo(&info, newinst, NULL, NULL) == 0)
		(void) sprintf(newinst, "%s.%d", pkgabrv, n++);
	return (newinst);
}

#define	PROMPT0	"Do you want to overwrite this installed instance"

#define	PROMPT1	"Do you want to create a new instance of this package"

#define	HELP1	"The package you are attempting to install already exists " \
		"on this machine.  You may choose to create a new instance " \
		"of this package by answering 'y' to this prompt.  If you " \
		"answer 'n' you will be asked to choose one of the instances " \
		"which is already to be overwritten."

#define	HEADER	"The following instance(s) of the <%s> package are already " \
		"installed on this machine:"

#define	PROMPT2	"Enter the identifier for the instance that you want to " \
		"overwrite"

#define	HELP2	"The package you are attempting to install already exists on " \
		"this machine.  You may choose to overwrite one of the " \
		"versions which is already installed by selecting the " \
		"appropriate entry from the menu."

static char *
prompt(struct pkginfo *info, int npkgs)
{
	CKMENU	*menup;
	int	i, n;
	char	ans[MAX_INPUT], *inst;
	char	header[256];
	char	temp[256];

	if (maxinst > npkgs) {
		/*
		 * the user may choose to install a completely new
		 * instance of this package
		 */
		if (n = ckyorn(ans, NULL, NULL, gettext(HELP1),
		    gettext(PROMPT1)))
			quit(n);
		if (strchr("yY", *ans) != NULL)
			return ("new");
	}

	(void) sprintf(header, gettext(HEADER), pkgabrv);
	menup = allocmenu(header, CKALPHA);

	for (i = 0; i < npkgs; i++) {
		(void) sprintf(temp, "%s %s\n(%s) %s", info[i].pkginst,
			info[i].name, info[i].arch, info[i].version);
		if (setitem(menup, temp)) {
			progerr(gettext("no memory"));
			quit(99);
		}
	}

	if (npkgs == 1) {
		printmenu(menup);
		if (n = ckyorn(ans, NULL, NULL, NULL, gettext(PROMPT0)))
			quit(n);
		if (strchr("yY", *ans) == NULL)
			quit(3);
		(void) strcpy(newinst, info[0].pkginst);
	} else {
		if (n = ckitem(menup, &inst, 1, NULL, NULL, gettext(HELP2),
		    gettext(PROMPT2)))
			quit(n);
		(void) strcpy(newinst, inst);
	}
	(void) setitem(menup, 0); /* clear resource usage */
	free(menup); /* clear resource usage */

	return (newinst);
}
