/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#pragma ident	"@(#)pkgmount.c	1.10	98/12/19 SMI"	/* SVr4.0  1.10.3.1	*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pkgdev.h>
#include <pkginfo.h>
#include <sys/types.h>
#include <devmgmt.h>
#include <sys/mount.h>
#include <pkglib.h>
#include "pkglocale.h"

extern void	quit(int retcode); 	/* Expected to be declared by caller! */
/* libadm.a */
extern int	getvol(char *device, char *label, int options, char *prompt);

#define	CMDSIZ	256
#define	ERR_FSTYP	"unable to determine fstype for <%s>"
#define	ERR_NOTROOT	"You must be \"root\" when using mountable media."
#define	MOUNT		"/sbin/mount"
#define	UMOUNT		"/sbin/umount"
#define	FSTYP		"/usr/sbin/fstyp"

#define	LABEL0	"Insert %%v %d of %d for <%s> package into %%p."
#define	LABEL1	"Insert %%v %d of %d into %%p."
#define	LABEL2	"Insert %%v for <%s> package into %%p."
#define	LABEL3	"Insert %%v into %%p."

int
pkgmount(struct pkgdev *devp, char *pkg, int part, int nparts, int getvolflg)
{
	int	n, flags;
	char	*pt, prompt[64], cmd[CMDSIZ];
	FILE	*pp;

	if (getuid()) {
		progerr(pkg_gt(ERR_NOTROOT));
		return (99);
	}

	if (part && nparts) {
		if (pkg) {
			(void) sprintf(prompt, pkg_gt(LABEL0), part,
			    nparts, pkg);
		} else {
			(void) sprintf(prompt, pkg_gt(LABEL1), part,
			    nparts);
		}
	} else if (pkg)
		(void) sprintf(prompt, pkg_gt(LABEL2), pkg);
	else
		(void) sprintf(prompt, pkg_gt(LABEL3));

	n = 0;
	for (;;) {
		if (!getvolflg && n)
			/*
			 * Return to caller if not prompting
			 * and error was encountered.
			 */
			return (-1);
		if (getvolflg && (n = getvol(devp->bdevice, NULL,
		    (devp->rdonly ? 0 : DM_FORMFS|DM_WLABEL), prompt))) {
			if (n == 3)
				return (3);
			if (n == 2)
				progerr(pkg_gt("unknown device <%s>"),
				    devp->bdevice);
			else
				progerr(
				    pkg_gt("unable to obtain package volume"));
			return (99);
		}

		if (devp->fstyp == NULL) {
			(void) sprintf(cmd, "%s %s", FSTYP, devp->bdevice);
			if ((pp = epopen(cmd, "r")) == NULL) {
				rpterr();
				logerr(pkg_gt(ERR_FSTYP), devp->bdevice);
				n = -1;
				continue;
			}
			cmd[0] = '\0';
			if (fgets(cmd, CMDSIZ, pp) == NULL) {
				logerr(pkg_gt(ERR_FSTYP), devp->bdevice);
				(void) pclose(pp);
				n = -1;
				continue;
			}
			if (epclose(pp)) {
				rpterr();
				logerr(pkg_gt(ERR_FSTYP), devp->bdevice);
				n = -1;
				continue;
			}
			if (pt = strpbrk(cmd, " \t\n"))
				*pt = '\0';
			if (cmd[0] == '\0') {
				logerr(pkg_gt(ERR_FSTYP), devp->bdevice);
				n = -1;
				continue;
			}
			devp->fstyp = strdup(cmd);
		}

		if (devp->rdonly) {
			n = pkgexecl(NULL, NULL, NULL, NULL, MOUNT, "-r", "-F",
			    devp->fstyp, devp->bdevice, devp->mount, NULL);
		} else {
			n = pkgexecl(NULL, NULL, NULL, NULL, MOUNT, "-F",
			    devp->fstyp, devp->bdevice, devp->mount, NULL);
		}
		if (n) {
			progerr(pkg_gt("mount of %s failed"), devp->bdevice);
			continue;
		}
		devp->mntflg++;
		break;
	}
	return (0);
}

int
pkgumount(struct pkgdev *devp)
{
	int	n = 1;
	int	retry = 10;

	if (!devp->mntflg)
		return (0);

	while (n != 0 && retry-- > 0) {
		n = pkgexecl(NULL, NULL, NULL, NULL, UMOUNT, devp->bdevice,
		    NULL);
		if (n != 0) {
			progerr(pkg_gt("retrying umount of %s"),
			    devp->bdevice);
			sleep(5);
		}
	}
	if (n == 0)
		devp->mntflg = 0;
	return (n);
}
