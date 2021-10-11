/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)predepend.c	1.8	93/03/10 SMI"	/* SVr4.0 1.3.1.1 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <pkglocs.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "libadm.h"

extern char	*pkgname, pkgloc[];
extern int	warnflag;

#define	ERR_RMLINK	"unable to remove options file <%s>"
#define	ERR_SYMLINK	"unable to create symbloic link from <%s> to <%s>"
#define	ERR_PREDEPEND	"unable to create predepend file <%s>"

void
predepend(char *oldpkg)
{
	FILE	*fp;
	char	path[PATH_MAX];
	char	spath[PATH_MAX];
	struct stat statbuf;

	oldpkg = strtok(oldpkg, " \t\n");
	if (oldpkg == NULL)
		return;

	(void) sprintf(path, "%s/predepend", pkgloc);
	if ((fp = fopen(path, "w")) == NULL) {
		progerr(gettext(ERR_PREDEPEND), path);
		warnflag++;
		return;
	}
	(void) fprintf(fp, "%s\n", pkgname);
	(void) fclose(fp);

	do {
		(void) sprintf(spath, "%s/%s.name", get_PKGOLD(), oldpkg);
		if (lstat(spath, &statbuf) == 0) {
			/* options file already exists */
			if (statbuf.st_mode & S_IFLNK) {
				/* remove current link */
				if (unlink(spath)) {
					progerr(gettext(ERR_RMLINK), spath);
					warnflag++;
				}
			}
		}
		if (symlink(path, spath)) {
			progerr(gettext(ERR_SYMLINK), path, spath);
			warnflag++;
		}
	} while (oldpkg = strtok(NULL, " \t\n"));
}
