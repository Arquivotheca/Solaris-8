/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)predepend.c	1.8	93/03/10 SMI"
		/* SVr4.0  1.2.1.1	*/

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <locale.h>
#include <libintl.h>
#include <pkglocs.h>
#include "pkglib.h"
#include "libinst.h"
#include "libadm.h"

extern int	warnflag;

#define	ERR_UNLINK	"unable to unlink <%s>"

void
predepend(char *oldpkg)
{
	struct stat status;
	char	spath[PATH_MAX];

	oldpkg = strtok(oldpkg, " \t\n");
	if (oldpkg == NULL)
		return;

	do {
		(void) sprintf(spath, "%s/%s.name", get_PKGOLD(), oldpkg);
		if (lstat(spath, &status) == 0) {
			if (status.st_mode & S_IFLNK) {
				if (unlink(spath)) {
					progerr(gettext(ERR_UNLINK), spath);
					warnflag++;
				}
				return;
			}
		}
	} while (oldpkg = strtok(NULL, " \t\n"));
}
