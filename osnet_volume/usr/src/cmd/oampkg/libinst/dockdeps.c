/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)dockdeps.c	1.9	93/07/14 SMI"	/* SVr4.0 1.5.2.1	*/

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <pkgstrct.h>
#include <pkginfo.h>
#include <locale.h>
#include <libintl.h>
#include "libinst.h"
#include <pkglib.h>
#include "libadm.h"

#define	ERR_PKGABRV	"illegal package abbreviation <%s> in dependency file"
#define	ERR_PRENCI	"The <%s> package \"%s\" is a prerequisite package " \
			"and is not completely installed."
#define	ERR_PREREQ	"The <%s> package \"%s\" is a prerequisite package " \
			"and should be installed."
#define	ERR_VALINST	" Allowable instances include (in order of " \
			" preference:)\n"
#define	ERR_DEPONME	"The <%s> package depends on the package currently " \
			"being removed."

#define	ERR_DEPNAM	"The <%s> package \"%s\" depends on the package " \
			"currently being removed."

extern void	quit(int exitval);

#define	LSIZE	256
#define	NVERS	50

static struct pkginfo info;

static char	type;
static char	abbrev[128],
		wabbrev[128],
		name[128],
		file[128],
		rmpkg[PKGSIZ+1],
		*rmpkginst,
		*vlist[NVERS],
		*alist[NVERS];
static int	nlist;
static int	errflg = 0;
static int	pkgexist, pkgokay;

static void	ckrdeps(void);
static void	ckpreq(FILE *fp, char *dname);
static void	deponme(char *pkginst, char *pkgname);
static void	prereq(char *pkginst, char *pkgname);
static void	incompat(char *pkginst, char *pkgname);
static int	getline(FILE *fp);

int
dockdeps(char *depfile, int rflag)
{
	FILE *fp;
	register int i;
	char *inst;

	if (rflag) {
		/* check removal dependencies */
		rmpkginst = depfile;
		(void) strncpy(rmpkg, rmpkginst, PKGSIZ);
		(void) strtok(rmpkg, ".");
		(void) sprintf(file, "%s/%s/install/depend", pkgdir, rmpkginst);
		if ((fp = fopen(file, "r")) == NULL)
			goto done;
	} else {
		if ((fp = fopen(depfile, "r")) == NULL) {
			progerr(
			    gettext("unable to open depend file <%s>"),
			    depfile);
			quit(99);
		}
	}

	while (getline(fp)) {
		switch (type) {
		    case 'I':
		    case 'P':
			if (rflag)
				continue;
			break;

		    case 'R':
			if (!rflag)
				continue;
			break;

		    default:
			errflg++;
			progerr(
			    gettext("unknown dependency type specified: %c\n"),
			    type);
			break;
		}

		/* check to see if any versions listed are installed */
		pkgexist = pkgokay = 0;
		i = 0;
		if (strchr(abbrev, '.'))
			progerr(gettext(ERR_PKGABRV), abbrev);
		(void) sprintf(wabbrev, "%s.*", abbrev);

		do {
			inst = fpkginst(wabbrev, alist[i], vlist[i]);
			if (inst && (pkginfo(&info, inst, NULL, NULL) == 0)) {
				pkgexist++;
				if ((info.status == PI_INSTALLED) ||
				    (info.status == PI_PRESVR4))
					pkgokay++;
			}
		} while (++i < nlist);
		(void) fpkginst(NULL); 	/* force closing/rewind of files */

		if (!info.name)
			info.name = name;

		switch (type) {
		    case 'I':
			incompat(abbrev, info.name);
			break;

		    case 'P':
			prereq(abbrev, name);
			break;

		    case 'R':
			deponme(abbrev, info.name);
		}
	}
	(void) fclose(fp);

done:
	if (rflag)
		ckrdeps();

	return (errflg);
}

static void
incompat(char *pkginst, char *pkgname)
{
	char buf[512];

	if (!pkgexist)
		return;

	errflg++;
	logerr(gettext("WARNING:"));
	(void) sprintf(buf, gettext("A version of <%s> package \"%s\" \
(which is incompatible with the package that is being installed) \
is currently installed and must be removed."), pkginst, pkgname);
	puttext(stderr, buf, 4, 0);
	(void) putc('\n', stderr);
}

static void
prereq(char *pkginst, char *pkgname)
{
	register int i;
	char buf[512];

	if (pkgokay)
		return;

	errflg++;
	logerr(gettext("WARNING:"));
	if (pkgexist) {
		(void) sprintf(buf, gettext(ERR_PRENCI), pkginst, pkgname);
		puttext(stderr, buf, 4, 0);
		(void) putc('\n', stderr);
	} else {
		(void) sprintf(buf, gettext(ERR_PREREQ), pkginst, pkgname);
		if (nlist) {
			(void) strcat(buf, gettext(ERR_VALINST));
		}
		puttext(stderr, buf, 4, 0);
		(void) putc('\n', stderr);
		for (i = 0; i < nlist; i++) {
			(void) printf("          ");
			if (alist[i])
				(void) printf("(%s) ", alist[i]);
			if (vlist[i])
				(void) printf("%s", vlist[i]);
			(void) printf("\n");
		}
	}
}

static void
deponme(char *pkginst, char *pkgname)
{
	char buf[512];

	if (!pkgexist)
		return;

	errflg++;
	logerr(gettext("WARNING:"));
	if (!pkgname || !pkgname[0])
		(void) sprintf(buf, gettext(ERR_DEPONME), pkginst);
	else
		(void) sprintf(buf, gettext(ERR_DEPNAM), pkginst, pkgname);
	puttext(stderr, buf, 4, 0);
	(void) putc('\n', stderr);
}

static int
getline(FILE *fp)
{
	register int i, c, found;
	char *pt, *new, line[LSIZE];

	abbrev[0] = name[0] = type = '\0';

	for (i = 0; i < nlist; i++) {
		if (alist[i]) {
			free(alist[i]);
			alist[i] = NULL;
		}
		if (vlist[i]) {
			free(vlist[i]);
			vlist[i] = NULL;
		}
	}
	alist[0] = vlist[0] = NULL;

	found = (-1);
	nlist = 0;
	while ((c = getc(fp)) != EOF) {
		(void) ungetc(c, fp);
		if ((found >= 0) && !isspace(c))
			return (1);

		if (!fgets(line, LSIZE, fp))
			break;

		for (pt = line; isspace(*pt); /* void */)
			pt++;
		if (!*pt || (*pt == '#'))
			continue;

		if (pt == line) {
			/* begin new definition */
			(void) sscanf(line, "%c %s %[^\n]",
				&type, abbrev, name);
			found++;
			continue;
		}
		if (found < 0)
			return (0);

		if (*pt == '(') {
			/* architecture is specified */
			if (new = strchr(pt, ')'))
				*new++ = '\0';
			else
				return (-1); /* bad specification */
			alist[found] = qstrdup(pt+1);
			pt = new;
		}
		while (isspace(*pt))
			pt++;
		if (*pt) {
			vlist[found] = qstrdup(pt);
			if (pt = strchr(vlist[found], '\n'))
				*pt = '\0';
		}
		found++;
		nlist++;
	}
	return ((found >= 0) ? 1 : 0);
}

static void
ckrdeps(void)
{
	struct dirent *drp;
	DIR	*dirfp;
	FILE	*fp;
	char	depfile[PATH_MAX+1];

	if ((dirfp = opendir(pkgdir)) == NULL)
		return;

	while ((drp = readdir(dirfp)) != NULL) {
		if (drp->d_name[0] == '.')
			continue;

		if (!strcmp(drp->d_name, rmpkginst))
			continue; /* others don't include me */
		(void) sprintf(depfile, "%s/%s/install/depend",
			pkgdir, drp->d_name);

		if ((fp = fopen(depfile, "r")) == NULL)
			continue;

		ckpreq(fp, drp->d_name);
	}
	(void) closedir(dirfp);
}

static void
ckpreq(FILE *fp, char *dname)
{
	register int i;
	char	*inst;

	while (getline(fp)) {
		if (type != 'P')
			continue;

		if (strcmp(abbrev, rmpkg))
			continue;

		/* see if package is installed */
		i = 0;
		if (!strchr(abbrev, '.'))
			(void) strcat(abbrev, ".*");
		pkgexist = 1;

		do {
			if (inst = fpkginst(abbrev, alist[i], vlist[i])) {
				if (!strcmp(inst, rmpkginst)) {
					deponme(dname, "");
					(void) fclose(fp);
					(void) fpkginst(NULL);
					return;
				}
			}
		} while (++i < nlist);
		(void) fpkginst(NULL);
	}
	(void) fclose(fp);
}
