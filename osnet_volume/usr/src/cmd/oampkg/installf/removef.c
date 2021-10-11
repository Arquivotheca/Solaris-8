/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)removef.c	1.13	94/10/21 SMI"	/* SVr4.0 1.8.1.1	*/

/*  5-20-92	added newroot function */

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
#include "install.h"
#include "libinst.h"
#include "libadm.h"

#define	MALSIZ	64
#define	ERR_MEMORY	"memory allocation failure, errno=%d"
#define	ERR_RELPATH	"ERROR: relative pathname <%s> ignored"

extern struct cfextra **extlist;
extern int	eptnum, warnflag;
extern void	quit(int exitval);

/* installf.c */
extern int	cfentcmp(const void *p1, const void *p2);

void
removef(int argc, char *argv[])
{
	struct cfextra *new;
	char	path[PATH_MAX];
	int	flag, n;
	char	*special, *save_path, *mountp;

	flag = strcmp(argv[0], "-") == 0;

	/* read stdin to obtain entries, which need to be sorted */
	extlist = (struct cfextra **) calloc(MALSIZ,
		sizeof (struct cfextra *));

	eptnum = 0;
	new = NULL;
	for (;;) {
		if (flag) {
			if (fgets(path, PATH_MAX, stdin) == NULL)
				break;
		} else {
			if (argc-- <= 0)
				break;
			/*
			 * This strips the install root from the path using
			 * a questionable algorithm. This should go away as
			 * we define more precisely the command line syntax
			 * with our '-R' option. - JST
			 */
			(void) strcpy(path, orig_path(argv[argc]));
		}
		if (path[0] != '/') {
			logerr(gettext(ERR_RELPATH), path);
			if (new)
				free(new);
			warnflag++;
			continue;
		}
		new = (struct cfextra *) calloc(1, sizeof (struct cfextra));
		if (new == NULL) {
			progerr(gettext(ERR_MEMORY), errno);
			quit(99);
		}
		new->cf_ent.ftype = '-';

		eval_path(&(new->server_path), &(new->client_path),
		    &(new->map_path), path);

		new->cf_ent.path = new->client_path;

		extlist[eptnum] = new;
		if ((++eptnum % MALSIZ) == 0) {
			extlist = (struct cfextra **) realloc((void *)extlist,
			    (unsigned)
			    (sizeof (struct cfextra)*(eptnum+MALSIZ)));
			if (!extlist) {
				progerr(gettext(ERR_MEMORY), errno);
				quit(99);
			}
		}
	}
	extlist[eptnum] = (struct cfextra *)NULL;

	qsort((char *)extlist,
		(unsigned)eptnum, sizeof (struct cfextra *), cfentcmp);
}
