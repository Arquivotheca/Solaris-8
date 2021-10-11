/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)pkginfo.c	1.32	95/06/20 SMI"	/* SVr4.0  1.16.2.1	*/

/* 5-20-92 	added suport for newroot function */

#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>
#include <libintl.h>
#include <string.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <pkginfo.h>
#include <sys/types.h>
#include <sys/param.h>
#include <pkgstrct.h>
#include <pkglocs.h>
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"

extern char	*pkgdir, *errstr;
extern int	pkginfofind(char *path, char *pkg_dir, char *pkginst);

#define	ERR_USAGE	"usage:\n" \
			"%s [-q] [-pi] [-x|l] [options] [pkg ...]\n" \
			"%s -d device [-q] [-x|l] [options] [pkg ...]\n" \
			"where\n" \
			"  -q #quiet mode\n" \
			"  -p #select partially installed packages\n" \
			"  -i #select completely installed packages\n" \
			"  -x #extracted listing\n" \
			"  -l #long listing\n" \
			"  -r #relocation base \n" \
			"and options may include:\n" \
			"  -c category, [category...]\n" \
			"  -a architecture\n" \
			"  -v version\n"

#define	ERR_INCOMP0	"-L and -l/-x/-r flags are incompatable"
#define	ERR_INCOMP1	"-l and -x/-r flags are not compatable"
#define	ERR_INCOMP2	"-x and -l/-r flags are not compatable"
#define	ERR_INCOMP3	"-r and -x/-x flags are not compatable"
#define	ERR_NOINFO	"ERROR: information for \"%s\" was not found"
#define	ERR_BADINFO	"pkginfo file is corrupt or missing"
#define	ERR_ROOT_SET	"Could not set install root from the environment."
#define	ERR_ROOT_CMD	"Command line install root contends with environment."

#define	nblock(size)	((size + (DEV_BSIZE - 1)) / DEV_BSIZE)
#define	MAXCATG	64

static char	*device = NULL;
static char	*parmlst[] = {
	"DESC", "PSTAMP", "INSTDATE", "VSTOCK", "SERIALNUM", "HOTLINE",
	"EMAIL", NULL
};

static char	contents[PATH_MAX];
static int	errflg = 0;
static int	qflag = 0;
static int	iflag = -1;
static int	pflag = -1;
static int	lflag = 0;
static int	Lflag = 0;
static int	Nflag = 0;
static int	xflag = 0;
static int	rflag = 0; 		/* bug # 1081606 */
static struct cfent	entry;
static char	**pkg = NULL;
static int	pkgcnt = 0;
static char	*ckcatg[MAXCATG] = {NULL};
static int	ncatg = 0;
static char	*ckvers = NULL;
static char	*ckarch = NULL;

static struct cfstat {
	char	pkginst[15];
	short	exec;
	short	dirs;
	short	link;
	short	partial;
	short	spooled;
	short	installed;
	short	info;
	short	shared;
	short	setuid;
	long	tblks;
	struct cfstat *next;
} *data;
static struct pkginfo info;

static struct	cfstat *fpkg(char *pkginst);
static int	iscatg(char *list);
static int	selectp(char *p);
static void	usage(void), look_for_installed(void),
		report(void), rdcontents(void);
static void	pkgusage(struct cfstat *dp, struct cfent *pentry);
static void	getinfo(struct cfstat *dp);
static void	dumpinfo(struct cfstat *dp);

/* should be in string.h? */
extern int	strcasecmp(const char *s1, const char *s2);

main(int argc, char **argv)
{
	extern char	*optarg;
	extern int	optind;
	int	c;
	char	*ir = NULL; /* added for newroot function */

	pkgdir = NULL;
	errstr = NULL;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	(void) set_prog_name(argv[0]);


	if (!set_inst_root(getenv("PKG_INSTALL_ROOT"))) {
		progerr(gettext(ERR_ROOT_SET));
		exit(1);
	}

	while ((c = getopt(argc, argv, "LNR:xv:a:d:qrpilc:?")) != EOF) {
		switch (c) {
		    case 'v':
			ckvers = optarg;
			break;

		    case 'a':
			ckarch = optarg;
			break;

		    case 'd':
			/* -d could specify stream or mountable device */
			device = flex_device(optarg, 1);
			break;

		    case 'q':
			qflag++;
			break;

		    case 'i':
			iflag = 1;
			if (pflag > 0)
				usage();
			pflag = 0;
			break;

		    case 'p':
			pflag = 1;
			if (iflag > 0)
				usage();
			iflag = 0;
			break;

		    case 'N':
			Nflag++;
			break;

		    case 'L':
			if (xflag || lflag || rflag) {
				progerr(gettext(ERR_INCOMP0));
				usage();
			}
			Lflag++;
			break;

		    case 'l':
			if (xflag || rflag) {
				progerr(gettext(ERR_INCOMP1));
				usage();
			}
			lflag++;
			break;

		    case 'x':
			/* bug # 1081606 */
			if (lflag || rflag) {
				progerr(gettext(ERR_INCOMP2));
				usage();
			}
			xflag++;
			break;

		    case 'r':
			if (lflag || xflag || Lflag) {
				progerr(gettext(ERR_INCOMP0));
				usage();
			}
			rflag++;
			break;

		    case 'c':
			ckcatg[ncatg++] = strtok(optarg, " \t\n, ");
			while (ckcatg[ncatg] = strtok(NULL, " \t\n, "))
				ncatg++;
			break;

		/* added for newroot functions */
		    case 'R':
			if (!set_inst_root(optarg)) {
				progerr(gettext(ERR_ROOT_CMD));
				exit(1);
			}
			break;

		    default:
			usage();
		}
	}

	/*
	 * implement the newroot option
	 */
	set_PKGpaths(get_inst_root());	/* set up /var... directories */

	pkg = &argv[optind];
	pkgcnt = (argc - optind);

	if (pkg[0] && strcmp(pkg[0], "all") == NULL) {
		pkgcnt = 0;
		pkg[0] = NULL;
	}

	if (pkgdir == NULL)
		pkgdir = get_PKGLOC(); 	/* we need this later */

	/* convert device appropriately */
	if (pkghead(device))
		exit(1);

	look_for_installed();

	if (lflag && strcmp(pkgdir, get_PKGLOC()) == NULL) {
		/* look at contents file */
		(void) sprintf(contents, "%s/contents", get_PKGADM());
		rdcontents();
	}

	report();

	(void) pkghead(NULL);
	exit(errflg ? 1 : 0);
	/*NOTREACHED*/
#ifdef lint
	return (0);
#endif	/* lint */
}


static void
report(void)
{
	struct cfstat *dp, *choice;
	int	i, output;

	output = 0;
	for (;;) {
		choice = (struct cfstat *)0;
		for (dp = data; dp; dp = dp->next) {
			/* get information about this package */
			if (dp->installed < 0)
				continue; /* already used */
			if (Lflag && pkgcnt) {
				choice = dp;
				break;
			} else if (!choice ||
			    (strcmp(choice->pkginst, dp->pkginst) > 0))
				choice = dp;
		}
		if (!choice)
			break; /* no more packages */

		if (pkginfo(&info, choice->pkginst, ckarch, ckvers)) {
			choice->installed = (-1);
			continue;
		}

		/*
		 * Confirm that the pkginfo file contains the
		 * required information.
		 */
		if (info.name == NULL || *(info.name) == NULL ||
		    info.arch == NULL || *(info.arch) == NULL ||
		    info.version == NULL || *(info.version) == NULL ||
		    info.catg == NULL || *(info.catg) == NULL) {
			progerr(gettext(ERR_BADINFO));
			errflg++;
			return;
		}

		/* is it in an appropriate catgory? */
		if (iscatg(info.catg)) {
			choice->installed = (-1);
			continue;
		}

		if (!pflag &&
			/* don't include partially installed packages */
			(choice->partial || (info.status == PI_PARTIAL) ||
				(info.status == PI_UNKNOWN))) {
			choice->installed = (-1);
			continue;
		}

		if (Nflag && (info.status == PI_PRESVR4)) {
			/* don't include preSVR4 packages */
			choice->installed = (-1);
			continue;
		}

		if (!iflag && ((info.status == PI_INSTALLED) ||
		    (info.status == PI_PRESVR4))) {
			/* don't include completely installed packages */
			choice->installed = (-1);
			continue;
		}

		output++;
		dumpinfo(choice);
		choice->installed = (-1);
		if (pkgcnt && !qflag) {
			i = selectp(choice->pkginst);
			if (i >= 0)
				pkg[i] = NULL;
		}
	}
	if (!output)
		errflg++;

	if (qflag)
		return;

	/* verify that each package listed on command line was output */
	for (i = 0; i < pkgcnt; ++i) {
		if (pkg[i]) {
			logerr(gettext(ERR_NOINFO), pkg[i]);
			errflg++;
		}
	}
	(void) pkginfo(&info, NULL); /* free up all memory and open fds */
}

static void
dumpinfo(struct cfstat *dp)
{
	register int i;
	static char *fmt = "%10s:  %s\n";
	char *pt, category[128];

	if (qflag)
		return; /* print nothing */

	if (rflag) {
		(void) puts((info.basedir) ? info.basedir : "none");
		return;
	}

	if (Lflag) {
		(void) puts(info.pkginst);
		return;
	} else if (xflag) {
		(void) printf("%-14.14s  %s\n", info.pkginst, info.name);
		if (info.arch || info.version)
			(void) printf("%14.14s  ", "");
		if (info.arch)
			(void) printf("(%s) ", info.arch);
		if (info.version)
			(void) printf("%s", info.version);
		if (info.arch || info.version)
			(void) printf("\n");
		return;
	} else if (!lflag) {
		if (info.catg)
			(void) sscanf(info.catg, "%[^, \t\n]", category);
		else if (info.status == PI_PRESVR4)
			(void) strcpy(category, "preSVR4");
		else
			(void) strcpy(category, "(unknown)");
		(void) fprintf(stdout, "%-11.11s %-14.14s %s\n", category,
			info.pkginst, info.name);
		return;
	}
	if (info.pkginst)
		(void) printf(fmt, gettext("PKGINST"), info.pkginst);
	if (info.name)
		(void) printf(fmt, gettext("NAME"), info.name);
	if (lflag && info.catg)
		(void) printf(fmt, gettext("CATEGORY"), info.catg);
	if (lflag && info.arch)
		(void) printf(fmt, gettext("ARCH"), info.arch);
	if (info.version)
		(void) printf(fmt, gettext("VERSION"), info.version);
	if (info.basedir)
		(void) printf(fmt, gettext("BASEDIR"), info.basedir);
	if (info.vendor)
		(void) printf(fmt, gettext("VENDOR"), info.vendor);

	if (info.status == PI_PRESVR4)
		(void) printf(fmt, gettext("STATUS"), "preSVR4");
	else {
		for (i = 0; parmlst[i]; ++i) {
			if ((pt = pkgparam(info.pkginst, parmlst[i])) != NULL &&
			    *pt)
				(void) printf(fmt, parmlst[i], pt);
		}
		if (info.status == PI_SPOOLED)
			(void) printf(fmt, gettext("STATUS"),
			    gettext("spooled"));
		else if (info.status == PI_PARTIAL)
			(void) printf(fmt, gettext("STATUS"),
			    gettext("partially installed"));
		else if (info.status == PI_INSTALLED)
			(void) printf(fmt, gettext("STATUS"),
			    gettext("completely installed"));
		else
			(void) printf(fmt, gettext("STATUS"),
			    gettext("(unknown)"));
	}
	(void) pkgparam(NULL, NULL);

	if (!lflag) {
		(void) putchar('\n');
		return;
	}

	if (info.status != PI_PRESVR4) {
		if (strcmp(pkgdir, get_PKGLOC()))
			getinfo(dp);

		if (dp->spooled)
			(void) printf(
			    gettext("%10s:  %5d spooled pathnames\n"),
			    gettext("FILES"), dp->spooled);
		if (dp->installed)
			(void) printf(
			    gettext("%10s:  %5d installed pathnames\n"),
			    gettext("FILES"), dp->installed);
		if (dp->partial)
			(void) printf(
			    gettext("%18d partially installed pathnames\n"),
			    dp->partial);
		if (dp->shared)
			(void) printf(gettext("%18d shared pathnames\n"),
				dp->shared);
		if (dp->link)
			(void) printf(gettext("%18d linked files\n"), dp->link);
		if (dp->dirs)
			(void) printf(gettext("%18d directories\n"), dp->dirs);
		if (dp->exec)
			(void) printf(gettext("%18d executables\n"), dp->exec);
		if (dp->setuid)
			(void) printf(
			    gettext("%18d setuid/setgid executables\n"),
			    dp->setuid);
		if (dp->info)
			(void) printf(
			    gettext("%18d package information files\n"),
			    dp->info+1); /* pkgmap counts! */

		if (dp->tblks)
			(void) printf(gettext("%18ld blocks used (approx)\n"),
				dp->tblks);
	}
	(void) putchar('\n');
}

static struct cfstat *
fpkg(char *pkginst)
{
	struct cfstat *dp, *last;

	dp = data;
	last = (struct cfstat *)0;
	while (dp) {
		if (strcmp(dp->pkginst, pkginst) == NULL)
			return (dp);
		last = dp;
		dp = dp->next;
	}
	dp = (struct cfstat *)calloc(1, sizeof (struct cfstat));
	if (!dp) {
		progerr(gettext("no memory, malloc() failed"));
		exit(1);
	}
	if (!last)
		data = dp;
	else
		last->next = dp; /* link list */
	(void) strncpy(dp->pkginst, pkginst, 14);
	return (dp);
}

#define	SEPAR	','

static int
iscatg(char *list)
{
	register int i;
	register char *pt;
	int	match;

	if (!ckcatg[0])
		return (0); /* no specification implies all packages */
	if (info.status == PI_PRESVR4) {
		for (i = 0; ckcatg[i]; /* void */) {
			if (strcmp(ckcatg[i++], "preSVR4") == NULL)
				return (0);
		}
		return (1);
	}
	if (!list)
		return (1); /* no category specified in pkginfo is a bug */

	match = 0;
	do {
		if (pt = strchr(list, ','))
			*pt = '\0';

		for (i = 0; ckcatg[i]; /* void */) {
			/* bug id 1081607 */
			if (!strcasecmp(list, ckcatg[i++])) {
				match++;
				break;
			}
		}

		if (pt)
			*pt++ = ',';
		if (match)
			return (0);
		list = pt; /* points to next one */
	} while (pt);
	return (1);
}

static void
look_for_installed(void)
{
	struct dirent *drp;
	struct stat	status;
	DIR	*dirfp;
	char	path[PATH_MAX];
	int	n;

	if (strcmp(pkgdir, get_PKGLOC()) == NULL &&
	    (dirfp = opendir(get_PKGOLD()))) {
		while (drp = readdir(dirfp)) {
			if (drp->d_name[0] == '.')
				continue;
			n = strlen(drp->d_name);
			if ((n > 5) &&
			    strcmp(&drp->d_name[n-5], ".name") == NULL) {
				(void) sprintf(path, "%s/%s", get_PKGOLD(),
					drp->d_name);
				if (lstat(path, &status))
					continue;
				if ((status.st_mode & S_IFMT) != S_IFREG)
					continue;
				drp->d_name[n-5] = '\0';
				if (!pkgcnt || (selectp(drp->d_name) >= 0))
					(void) fpkg(drp->d_name);
			}
		}
		(void) closedir(dirfp);
	}

	if ((dirfp = opendir(pkgdir)) == NULL)
		return;

	while (drp = readdir(dirfp)) {
		if (drp->d_name[0] == '.')
			continue;

		if (pkgcnt && (selectp(drp->d_name) < 0))
			continue;

		if (!pkginfofind(path, pkgdir, drp->d_name))
			continue; /* doesn't appear to be a package */

		(void) fpkg(drp->d_name);
	}
	(void) closedir(dirfp);
}

static int
selectp(char *p)
{
	register int i;

	for (i = 0; i < pkgcnt; ++i) {
		if (pkg[i] && pkgnmchk(p, pkg[i], 1) == 0)
			return (i);
	}
	return (-1);
}

static void
rdcontents(void)
{
	FILE *fp1;
	struct cfstat *dp;
	struct pinfo *pinfo;
	int n;

	if ((fp1 = fopen(contents, "r")) == NULL) {
		progerr(gettext("unable open \"%s\" for reading"), contents);
		exit(1);
	}

	/* check the contents file to look for referenced packages */
	while ((n = srchcfile(&entry, "*", fp1, NULL)) > 0) {
		for (pinfo = entry.pinfo; pinfo; pinfo = pinfo->next) {
			/* see if entry is used by indicated packaged */
			if (pkgcnt && (selectp(pinfo->pkg) < 0))
				continue;

			dp = fpkg(pinfo->pkg);
			pkgusage(dp, &entry);

			if (entry.npkgs > 1)
				dp->shared++;

			/*
			 * Only objects specifically tagged with '!' event
			 * character are considered "partial", everything
			 * else is considered "installed" (even server
			 * objects).
			 */
			switch (pinfo->status) {
			case '!' :
				dp->partial++;
				break;
			default :
				dp->installed++;
				break;
			}
		}
	}
	if (n < 0) {
		progerr(gettext("bad entry read in contents file"));
		logerr(gettext("pathname: %s"),
		    (entry.path && *entry.path) ? entry.path : "Unknown");
		logerr(gettext("problem: %s"),
		    (errstr && *errstr) ? errstr : "Unknown");
		exit(1);
	}

	(void) fclose(fp1);
}

static void
getinfo(struct cfstat *dp)
{
	FILE *fp;
	int n;
	char pkgmap[256];

	(void) sprintf(pkgmap, "%s/%s/pkgmap", pkgdir, dp->pkginst);
	if ((fp = fopen(pkgmap, "r")) == NULL) {
		progerr(gettext("unable open \"%s\" for reading"), pkgmap);
		exit(1);
	}
	dp->spooled = 1; /* pkgmap counts! */
	while ((n = gpkgmap(&entry, fp)) > 0) {
		dp->spooled++;
		pkgusage(dp, &entry);
	}
	if (n < 0) {
		progerr(gettext("bad entry read in pkgmap file"));
		logerr(gettext("pathname: %s"),
		    (entry.path && *entry.path) ? entry.path : "Unknown");
		logerr(gettext("problem: %s"),
		    (errstr && *errstr) ? errstr : "Unknown");
		exit(1);
	}
	(void) fclose(fp);
}

static void
pkgusage(struct cfstat *dp, struct cfent *pentry)
{
	if (pentry->ftype == 'i') {
		dp->info++;
		return;
	} else if (pentry->ftype == 'l') {
		dp->link++;
	} else {
		if ((pentry->ftype == 'd') || (pentry->ftype == 'x'))
			dp->dirs++;

		/* Only collect mode stats if they would be meaningful. */
		if (pentry->ainfo.mode != BADMODE) {
			if (pentry->ainfo.mode & 06000)
				dp->setuid++;
			if (!strchr("dxcbp", pentry->ftype) &&
			(pentry->ainfo.mode & 0111))
				dp->exec++;
		}
	}

	if (strchr("ifve", pentry->ftype))
		dp->tblks += nblock(pentry->cinfo.size);
}

static void
usage(void)
{
	char *prog = get_prog_name();

	/* bug # 1081606 */
	(void) fprintf(stderr, gettext(ERR_USAGE), prog, prog);

	exit(1);
}

int
quit(int retval)
{
	exit(retval);
	/*NOTREACHED*/
	return (0);
}
