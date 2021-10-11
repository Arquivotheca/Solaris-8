/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)merginfo.c	1.29	96/04/05 SMI"	/* SVr4.0 1.8.2.1 */

/*  5-20-92	added newroot functions */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <locale.h>
#include <libintl.h>
#include "pkglib.h"
#include "install.h"
#include "libadm.h"
#include "libinst.h"
#include "pkginstall.h"

extern char	instdir[], pkgbin[], pkgloc[], savlog[], *pkginst, **environ;

static char 	*infoloc;


/*
 * These are parameters to be excluded from the pkginfo file or at
 * least not to be inserted in the usual way.
 */
#define	TREEHEIGHT	4
static char *x_label[] =
{
	"AAAA1",	/* fill to avoid constraint tests in loop */
	"AAAA2",
	"AAAA3",
	"BASEDIR=",
	"CLASSES=",
	"CLIENT_BASEDIR=",
	"INST_DATADIR=",
	"PKG_CAS_PASSRELATIVE=",
	"PKG_DST_QKVERIFY=",
	"PKG_INIT_INSTALL=",
	"PKG_INSTALL_ROOT=",
	"PKG_SRC_NOVERIFY=",
	"zzzz1",	/* fill to avoid constraint tests in loop */
	"zzzz2",
	"zzzz3"
};

static int x_len[] =
{
	5,
	5,
	5,
	8,
	8,
	15,
	13,
	21,
	17,
	17,
	17,
	17,
	5,
	5,
	5
};

/*
 * While pkgsav has to be set up with reference to the server for package
 * scripts, it has to be client-relative in the pkginfo file. This function
 * is used to set the client-relative value for use in the pkginfo file.
 */
void
set_infoloc(char *path)
{
	if (path && *path) {
		if (is_an_inst_root())
			/* Strip the server portion of the path. */
			infoloc = orig_path(path);
		else
			infoloc = strdup(path);
	}
}

void
merginfo(struct cl_attr **pclass)
{
	char	*ir = get_inst_root();
	struct	dirent *dp;
	FILE	*fp;
	DIR	*pdirfp;
	char	path[PATH_MAX], temp[PATH_MAX];
	int	i, j, found;
	int	out;
	char 	*tmppt;
	int	nc;

	/* variables for exclusion list */
	int	cont;
	int	count, list_sz = (sizeof (x_label) / sizeof (char *));
	int	list_cntr;	/* starting point for binary search */
	register int	pos;		/* current position */
	register int	level;		/* current height in the tree */
	register int 	incr;		/* increment for step */
	int	result;		/* result of strcmp */
	register char **	x_ptr = x_label;

	/* remove savelog from previous attempts */
	(void) unlink(savlog);

	/*
	 * output packaging environment to create a pkginfo file in pkgloc[]
	 */
	(void) sprintf(path, "%s/%s", pkgloc, PKGINFO);
	if ((fp = fopen(path, "w")) == NULL) {
		progerr(gettext("unable to open <%s> for writing"), path);
		quit(99);
	}
	out = 0;
	(void) fputs("CLASSES=", fp);
	if (pclass) {
		(void) fprintf(fp, "%s", pclass[0]->name);
		out++;
		for (i = 1; pclass[i]; i++) {
			(void) fprintf(fp, " %s", pclass[i]->name);
			out++;
		}
	}
	nc = cl_getn();
	for (i = 0; i < nc; i++) {
		found = 0;
		if (pclass) {
			for (j = 0; pclass[j]; ++j) {
				if (cl_idx(pclass[j]->name) != -1) {
					found++;
					break;
				}
			}
		}
		if (!found) {
			if (out > 0)
				(void) fprintf(fp, " %s", cl_nam(i));
			else {
				(void) fprintf(fp, "%s", cl_nam(i));
				out++;
			}
		}
	}
	(void) fputc('\n', fp);

	/*
	 * NOTE : BASEDIR below is relative to the machine that
	 * *runs* the package. If there's an install root, this
	 * is actually the CLIENT_BASEDIR wrt the machine
	 * doing the pkgadd'ing here. -- JST
	 */
	if (is_a_basedir())
		(void) fprintf(fp, "BASEDIR=%s\n",
		    get_info_basedir());

	/*
	 * output all other environment parameters except those which
	 * are relevant only to install.
	 */
	list_cntr = list_sz >> 1;
	for (i = 0; environ[i]; i++) {
		cont = 0;
		incr = list_cntr + 1;
		for (level = TREEHEIGHT, 	/* start at the top level */
		    pos = list_cntr;		/*   ... in the middle */
		    level;		/* for as long as we're in the tree */
		    level--, pos += (result < 0) ? incr : -incr) {
			result = strncmp(*(x_ptr + pos), environ[i],
			    x_len[pos]);

			if (result == 0) {
				cont = 1;
				break;
			}

			incr = incr >> 1;
		}
		if (cont)
			continue;

		if ((strncmp(environ[i], "PKGSAV=", 7) == 0)) {
			(void) fprintf(fp, "PKGSAV=%s/%s/save\n",
			    infoloc, pkginst);
			continue;
		}
		(void) fputs(environ[i], fp);
		(void) fputc('\n', fp);
	}
	(void) fclose(fp);

	/*
	 * copy all packaging scripts to appropriate directory
	 */
	(void) sprintf(path, "%s/install", instdir);
	if ((pdirfp = opendir(path)) == NULL)
		return;
	while ((dp = readdir(pdirfp)) != NULL) {
		if (dp->d_name[0] == '.')
			continue;

		(void) sprintf(path, "%s/install/%s", instdir, dp->d_name);
		(void) sprintf(temp, "%s/%s", pkgbin, dp->d_name);
		if (cppath(KEEPMODE, path, temp, 0644)) {
			progerr(gettext("unable to save copy of <%s> in %s"),
			    dp->d_name,
			    pkgbin);
			quit(99);
		}
	}
	(void) closedir(pdirfp);

#if 0	/* Not wanted 3/24/93 */
	/*
	 * added to copy the pkgmap file into /var/sadm/pkg/pkginst with the
	 * package info file
	 */
	(void) sprintf(temp, "%s/pkgmap", pkgloc);
	(void) sprintf(path, "%s/pkgmap", instdir);
	(void) cppath(0, path, temp, 0644);
#endif	/* 0 */
}
