/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1994,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ckentry.c	1.23	98/12/19 SMI"	/* SVr4.0  1.2.1.1	*/

#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pkgstrct.h>
#include <locale.h>
#include <libintl.h>
#include <unistd.h>
#include <stdlib.h>
#include "pkglib.h"
#include "install.h"
#include "libadm.h"
#include "libinst.h"

extern int	Lflag, lflag, aflag, cflag, fflag, qflag, nflag, xflag, vflag;
extern char	*errstr, *basedir, *device, errbuf[], pkgspool[];

#define	nxtentry(p) \
		(maptyp ? srchcfile(p, "*", fp, NULL) : gpkgmap(p, fp))

#define	ERR_SPOOLED	"ERROR: unable to locate spooled object <%s>"
#define	MSG_NET_OBJ	"It is remote and may be available from the network."
#define	ERR_REPOS	"unable to reset file position from xdir()"
#define	ERR_RMHIDDEN	"unable to remove hidden file"
#define	ERR_HIDDEN	"ERROR: hidden file in exclusive directory"

static char	*findspool(struct cfent *ept);
static int	xdir(int maptyp, FILE *fp, char *dirname);

int
ckentry(int envflag, int maptyp, struct cfent *ept, FILE *fp)
{
	int	a_err, c_err,
		errflg;
	char	*path;
	char	*ir = get_inst_root();

	if (ept->ftype != 'i') {
		if (envflag)
			mappath(2, ept->path);
		if (!device)
			basepath(ept->path, maptyp ? NULL : basedir, ir);
	}
	canonize(ept->path);
	if (strchr("sl", ept->ftype)) {
		if (envflag)				/* -e option */
			mappath(2, ept->ainfo.local);
		if (!RELATIVE(ept->ainfo.local)) {	/* Absolute Path */
			if (!device) {
				if (ept->ftype == 'l')	/* Hard Link */
					basepath(ept->ainfo.local, NULL, ir);
			}
		}
		if (!RELATIVE(ept->ainfo.local))	/* Absolute Path */
			canonize(ept->ainfo.local);
	}
	if (envflag) {
		if (!strchr("isl", ept->ftype)) {
			mapvar(2, ept->ainfo.owner);
			mapvar(2, ept->ainfo.group);
		}
	}

	if (lflag) {
		tputcfent(ept, stdout);
		return (0);
	} else if (Lflag)
		return (putcfile(ept, stdout));

	errflg = 0;
	if (device) {
		if (strchr("dxslcbp", ept->ftype))
			return (0);
		if ((path = findspool(ept)) == NULL) {
			logerr(gettext(ERR_SPOOLED), ept->path);
			return (-1);
		}

		/*
		 * If the package file attributes are to be sync'd up with
		 * the pkgmap, we fix the attributes here.
		 */
		if (fflag) {
			a_err = 0;
			/* Clear dangerous bits. */
			ept->ainfo.mode = (ept->ainfo.mode & S_IAMB);
			/*
			 * Make sure the file is readable by the world and
			 * writeable by root.
			 */
			ept->ainfo.mode |= 0644;
			if (!strchr("in", ept->ftype)) {
				/* Set the safe attributes. */
				if (a_err = averify(fflag, &ept->ftype,
				    path, &ept->ainfo)) {
					errflg++;
					if (!qflag || (a_err != VE_EXIST)) {
						logerr(gettext("ERROR: %s"),
						    ept->path);
						logerr(errbuf);
					}
					if (a_err == VE_EXIST)
						return (-1);
				}
			}
		}
		c_err = cverify(fflag,  &ept->ftype, path, &ept->cinfo);
		if (c_err) {
			logerr(gettext("ERROR: %s"), path);
			logerr(errbuf);
			return (-1);
		}
	} else {
		a_err = 0;
		if (aflag && !strchr("in", ept->ftype)) {
			/* validate attributes */
			if (a_err = averify(fflag, &ept->ftype, ept->path,
			    &ept->ainfo)) {
				errflg++;
				if (!qflag || (a_err != VE_EXIST)) {
					logerr(gettext("ERROR: %s"),
					    ept->path);
					logerr(errbuf);
					if (maptyp && ept->pinfo->status ==
					    SERVED_FILE)
						logerr(gettext(MSG_NET_OBJ));
				}
				if (a_err == VE_EXIST)
					return (-1);
			}
		}
		if (cflag && strchr("fev", ept->ftype) &&
		    (!nflag || ept->ftype != 'v') && /* bug # 1082144 */
		    (!nflag || ept->ftype != 'e')) {
			/* validate contents */
			if (c_err = cverify(fflag, &ept->ftype, ept->path,
			    &ept->cinfo)) {
				errflg++;
				if (!qflag || (c_err != VE_EXIST)) {
					if (!a_err)
						logerr(gettext("ERROR: %s"),
						    ept->path);
					logerr(errbuf);
					if (maptyp && ept->pinfo->status ==
					    SERVED_FILE)
						logerr(gettext(MSG_NET_OBJ));
				}
				if (c_err == VE_EXIST)
					return (-1);
			}
		}
		if (xflag && (ept->ftype == 'x')) {
			/* must do verbose here since ept->path will change */
			path = strdup(ept->path);
			if (xdir(maptyp, fp, path))
				errflg++;
			(void) strcpy(ept->path, path);
			free(path);
		}
	}
	if (vflag)
		(void) fprintf(stderr, "%s\n", ept->path);
	return (errflg);
}

static int
xdir(int maptyp, FILE *fp, char *dirname)
{
	struct dirent *drp;
	struct cfent mine;
	struct pinfo *pinfo;
	DIR	*dirfp;
	long	pos;
	int	n, len, dirfound,
		errflg;
	char	badpath[PATH_MAX+1];

	pos = ftell(fp);

	if ((dirfp = opendir(dirname)) == NULL) {
		progerr(gettext("unable to open directory <%s>"), dirname);
		return (-1);
	}
	len = strlen(dirname);

	errflg = 0;
	(void) memset((char *)&mine, '\0', sizeof (struct cfent));
	while ((drp = readdir(dirfp)) != NULL) {
		if (strcmp(drp->d_name, ".") == NULL ||
		    strcmp(drp->d_name, "..") == NULL)
			continue;
		dirfound = 0;
		while ((n = nxtentry(&mine)) != 0) {
			if (n < 0) {
				logerr(gettext("ERROR: garbled entry"));
				logerr(gettext("pathname: %s"),
				    (mine.path && *mine.path) ? mine.path :
				    "Unknown");
				logerr(gettext("problem: %s"),
				    (errstr && *errstr) ? errstr : "Unknown");
				exit(99);
			}
			if (strncmp(mine.path, dirname, len) ||
			(mine.path[len] != '/'))
				break;
			if (strcmp(drp->d_name, &mine.path[len+1]) == NULL) {
				dirfound++;
				break;
			}
		}
		if (fseek(fp, pos, 0)) {
			progerr(gettext(ERR_REPOS));
			exit(99);
		}
		if (!dirfound) {
			(void) sprintf(badpath, "%s/%s", dirname,
			    drp->d_name);
			if (fflag) {
				if (unlink(badpath)) {
					errflg++;
					logerr(gettext("ERROR: %s"), badpath);
					logerr(gettext(ERR_RMHIDDEN));
				}
			} else {
				errflg++;
				logerr(gettext("ERROR: %s"), badpath);
				logerr(gettext(ERR_HIDDEN));
			}
		}
	}

	if (maptyp) {
		/* clear memory we've used */
		while ((pinfo = mine.pinfo) != NULL) {
			mine.pinfo = pinfo->next;
			free((char *)pinfo);
		}
	}

	(void) closedir(dirfp);
	return (errflg);
}

static char *
findspool(struct cfent *ept)
{
	static char path[2*PATH_MAX+1];
	char host[PATH_MAX+1];

	(void) strcpy(host, pkgspool);
	if (ept->ftype == 'i') {
		if (strcmp(ept->path, "pkginfo"))
			(void) strcat(host, "/install");
	} else if (ept->path[0] == '/')
		(void) strcat(host, "/root");
	else
		(void) strcat(host, "/reloc");

	(void) sprintf(path, "%s/%s", host,
		ept->path + (ept->path[0] == '/'));
	if (access(path, 0) == 0)
		return (path);

	if ((ept->ftype != 'i') && (ept->volno > 0)) {
		(void) sprintf(path, "%s.%d/%s", host, ept->volno,
			ept->path + (ept->path[0] == '/'));
		if (access(path, 0) == 0)
			return (path);
	}
	return (NULL);
}
