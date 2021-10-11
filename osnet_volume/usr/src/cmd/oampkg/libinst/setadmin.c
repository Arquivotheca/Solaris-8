/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)setadmin.c	1.12	99/06/22 SMI"	/* SVr4.0 1.6	*/

/*  5-20-93    added newroot support  */

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pkglocs.h>
#include <locale.h>
#include <libintl.h>
#include "pkglib.h"
#include "install.h"
#include "libadm.h"
#include "libinst.h"

#define	DEFMAIL	"root"

extern struct admin adm;

extern int	warnflag;
extern void	quit(int exitval);

static struct {
	char	**memloc;
	char	*tag;
} admlist[] = {
	&adm.mail, 	"mail",
	&adm.instance, 	"instance",
	&adm.partial, 	"partial",
	&adm.runlevel, 	"runlevel",
	&adm.idepend, 	"idepend",
	&adm.rdepend, 	"rdepend",
	&adm.space, 	"space",
	&adm.setuid, 	"setuid",
	&adm.conflict, 	"conflict",
	&adm.action, 	"action",
	&adm.basedir, 	"basedir",
	NULL
};

void
setadmin(char *file)
{
	FILE	*fp;
	int	i;
	char	param[64];
	char	*value;
	char	path[PATH_MAX];
	int	mail = 0;

	if (file == NULL)
		file = "default";
	else if (!strcmp(file, "none")) {
		adm.basedir = "ask";
		return;
	}

	if (file[0] == '/')
		(void) strcpy(path, file);
	else {
		(void) sprintf(path, "%s/admin/%s", get_PKGADM(), file);
		if (access(path, R_OK)) {
			(void) sprintf(path, "%s/admin/%s", PKGADM, file);
		}
	}

	if ((fp = fopen(path, "r")) == NULL) {
		progerr(gettext("unable to open admin file <%s>"), file);
		quit(99);
	}

	param[0] = '\0';
	while (value = fpkgparam(fp, param)) {
		if (!strcmp(param, "mail"))
			mail = 1;
		if (value[0] == '\0') {
			param[0] = '\0';
			continue; /* same as not being set at all */
		}
		for (i = 0; admlist[i].memloc; i++) {
			if (!strcmp(param, admlist[i].tag)) {
				*admlist[i].memloc = value;
				break;
			}
		}
		if (admlist[i].memloc == NULL) {
			logerr(gettext("WARNING: unknown admin parameter <%s>"),
			    param);
			warnflag++;
			free(value);
		}
		param[0] = '\0';
	}

	(void) fclose(fp);

	if (!mail)
		adm.mail = DEFMAIL; 	/* if we don't assign anything to it */
}
