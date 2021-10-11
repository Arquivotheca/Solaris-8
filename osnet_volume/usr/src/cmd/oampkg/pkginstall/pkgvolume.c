/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)pkgvolume.c	1.7	93/03/17 SMI"	/* SVr4.0 1.5.2.1 */

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mount.h>
#include <pkgdev.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "libadm.h"

extern char	instdir[], pkgbin[];

extern void	quit(int extival);

void
pkgvolume(struct pkgdev *devp, char *pkg, int part, int nparts)
{
	static int	cpart = 0;
	char	path[PATH_MAX];
	int	n;

	if (devp->cdevice)
		return;
	if (cpart == part)
		return;
	cpart = part;

	if (part == 1) {
		if (ckvolseq(instdir, 1, nparts)) {
			progerr(gettext("corrupt directory structure"));
			quit(99);
		}
		cpart = 1;
		return;
	}

	if (devp->mount == NULL) {
		if (ckvolseq(instdir, part, nparts)) {
			progerr(gettext("corrupt directory structure"));
			quit(99);
		}
		return;
	}

	for (;;) {
		(void) chdir("/");
		if (n = pkgumount(devp)) {
			progerr(gettext("attempt to unmount <%s> failed (%d)"),
				devp->bdevice, n);
			quit(99);
		}
		if (n = pkgmount(devp, pkg, part, nparts, 1))
			quit(n);
		(void) sprintf(path, "%s/%s", devp->dirname, pkg);
		if (ckvolseq(path, part, nparts) == 0)
			break;
	}
}
