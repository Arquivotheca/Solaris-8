/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)delmap.c	1.17	95/01/18 SMI"	/* SVr4.0  1.8.1.1	*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pkgstrct.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"

extern int	dbchg, warnflag, otherstoo;
extern char	*errstr, *pkginst;
extern void	quit(int exitval);

#define	EPTMALLOC	128

#define	ERR_WRENT	"write of entry failed, errno=%d"
#define	ERR_MEMORY	"no memory, errno=%d"

struct cfent	**eptlist;
int	eptnum;

void
delmap(int flag)
{
	struct cfent *ept;
	struct pinfo *pinfo;
	FILE	*fp, *fpo;
	int	n;

	if (!ocfile(&fp, &fpo, 0L))
		quit(99);

	/* re-use any memory used to store pathnames */
	(void) pathdup(NULL);

	if (eptlist != NULL)
		free(eptlist);
	eptlist = (struct cfent **) calloc(EPTMALLOC, sizeof (struct cfent *));
	if (eptlist == NULL) {
		progerr(gettext(ERR_MEMORY), errno);
		quit(99);
	}

	ept = (struct cfent *) calloc(1, (unsigned) sizeof (struct cfent));
	if (!ept) {
		progerr(gettext(ERR_MEMORY), errno);
		quit(99);
	}

	eptnum = 0;
	while (n = srchcfile(ept, "*", fp, NULL)) {
		if (n < 0) {
			progerr(gettext("bad read of contents file"));
			progerr(gettext("pathname=%s"),
			    (ept->path && *ept->path) ? ept->path :
			    "Unknown");
			progerr(gettext("problem=%s"),
			    (errstr && *errstr) ? errstr : "Unknown");
			exit(99);
		}
		pinfo = eptstat(ept, pkginst, (flag ? '@' : '-'));
		if (ept->npkgs > 0) {
			if (putcfile(ept, fpo)) {
				progerr(gettext(ERR_WRENT), errno);
				quit(99);
			}
		}

		if (flag || (pinfo == NULL))
			continue;

		dbchg++;

		/*
		 * the path name is used by this package
		 * and this package only, or it is marked
		 * as an edittable file by this package
		 */
		if (!pinfo->editflag && otherstoo && ept->ftype != 'e')
			ept->ftype = '\0';
		if (*pinfo->aclass)
			(void) strcpy(ept->pkg_class, pinfo->aclass);
		eptlist[eptnum] = ept;

		ept->path = pathdup(ept->path);
		if (ept->ainfo.local != NULL)
			ept->ainfo.local = pathdup(ept->ainfo.local);

		ept = (struct cfent *) calloc(1, sizeof (struct cfent));
		if ((++eptnum % EPTMALLOC) == 0) {
			eptlist = (struct cfent **) realloc(eptlist,
			(eptnum+EPTMALLOC)*sizeof (struct cfent *));
			if (eptlist == NULL) {
				progerr(gettext(ERR_MEMORY), errno);
				quit(99);
			}
		}
	}
	eptlist[eptnum] = (struct cfent *) NULL;

	if ((n = swapcfile(fp, fpo, (dbchg ? pkginst : NULL))) == RESULT_WRN)
		warnflag++;
	else if (n == RESULT_ERR)
		quit(99);
}
