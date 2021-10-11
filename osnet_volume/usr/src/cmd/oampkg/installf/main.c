/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)main.c	1.32	99/10/07 SMI"	/* SVr4.0 1.13.3.1 */

/*  5-20-92	added newroot function */

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <pkginfo.h>
#include <pkgstrct.h>
#include <pkglocs.h>
#include <pkglib.h>
#include <locale.h>
#include <libintl.h>
#include "install.h"
#include "libinst.h"
#include "libadm.h"

extern char	*errstr;
extern char	dbst; 			/* libinst/pkgdbmerg.c */

#define	BASEDIR	"/BASEDIR/"

#define	INSTALF	(*prog == 'i')
#define	REMOVEF	(*prog == 'r')

#define	MSG_MANMOUNT	"Assuming mounts were provided."

#define	ERR_ROOT_SET	"Could not set install root from the environment."
#define	ERR_ROOT_CMD	"Command line install root contends with environment."
#define	ERR_CLASSLONG	"classname argument too long"
#define	ERR_CLASSCHAR	"bad character in classname"
#define	ERR_INVAL	"package instance <%s> is invalid"
#define	ERR_NOTINST	"package instance <%s> is not installed"
#define	ERR_MERG	"unable to merge contents file"
#define	ERR_SORT	"unable to sort contents file"
#define	ERR_NOTROOT	"You must be \"root\" for %s to execute properly."
#define	ERR_USAGE0	"usage:\n" \
	"\t%s [[-M|-A] -R host_path] [-V ...] pkginst path " \
	"[path ...]\n" \
	"\t%s [[-M|-A] -R host_path] [-V ...] pkginst path\n"

#define	ERR_USAGE1	"usage:\n" \
	"\t%s [[-M] -R host_path] [-V ...] [-c class] <pkginst> " \
	"<path>\n" \
	"\t%s [[-M] -R host_path] [-V ...] [-c class] <pkginst> " \
	"<path> <specs>\n" \
	"\t   where <specs> may be defined as:\n" \
	"\t\tf <mode> <owner> <group>\n" \
	"\t\tv <mode> <owner> <group>\n" \
	"\t\te <mode> <owner> <group>\n" \
	"\t\td <mode> <owner> <group>\n" \
	"\t\tx <mode> <owner> <group>\n" \
	"\t\tp <mode> <owner> <group>\n" \
	"\t\tc <major> <minor> <mode> <owner> <group>\n" \
	"\t\tb <major> <minor> <mode> <owner> <group>\n" \
	"\t\ts <path>=<srcpath>\n" \
	"\t\tl <path>=<srcpath>\n" \
	"\t%s [[-M] -R host_path] [-V ...] [-c class] -f pkginst\n"

#define	CMD_SORT	"sort +0 -1"

#define	LINK	1

char	*classname = NULL;

struct cfextra **extlist;

char	*pkginst;

char	*uniTmp;
char 	*abi_sym_ptr;

int	eptnum, sortflag, nointeract, nosetuid, nocnflct;
int	warnflag = 0;

void	quit(int retcode);
void	usage(void);

/* libadm/pkgparam.c */
extern void	set_PKGADM(char *newpath);
extern void	set_PKGLOC(char *newpath);

/* removef.c */
extern void	removef(int argc, char *argv[]);

/* installf.c */
extern int	installf(int argc, char *argv[]);

/* dofinal.c */
extern int	dofinal(FILE *fp, FILE *fpo, int rmflag, char *myclass);

main(int argc, char **argv)
{
	FILE	*mapfp, *tmpfp, *pp;
	struct cfent *ept;
	char	*pt;
	int	c, n, dbchg, err;
	int	pkgrmremote = 0;	/* don't remove remote files */
	int	map_client = 1;
	int	fflag = 0;
	char	*cmd, *tp;
	char	line[1024];
	char	*prog;

	char	outbuf[PATH_MAX];
	char	*vfstab_file = NULL;

	extern char	*optarg;
	extern int	optind;

	(void) signal(SIGHUP, exit);
	(void) signal(SIGINT, exit);
	(void) signal(SIGQUIT, exit);

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	prog = set_prog_name(argv[0]);

	if (getuid()) {
		progerr(gettext(ERR_NOTROOT), prog);
		exit(1);
	}

	/* bug id 4244631, not ABI compliant */
	abi_sym_ptr = getenv("PKG_NONABI_SYMLINKS");
	if (abi_sym_ptr && strncasecmp(abi_sym_ptr, "TRUE", 4) == 0)
		set_nonABI_symlinks();

	/* bugId 4012147 */
	if ((uniTmp = getenv("PKG_NO_UNIFIED")) != NULL)
		map_client = 0;
	if (!set_inst_root(getenv("PKG_INSTALL_ROOT"))) {
		progerr(gettext(ERR_ROOT_SET));
		exit(1);
	}

	while ((c = getopt(argc, argv, "c:V:fAMR:?")) != EOF) {
		switch (c) {
		    case 'f':
			fflag++;
			break;

		    case 'c':
			classname = optarg;
			/* validate that classname is acceptable */
			if (strlen(classname) > (size_t)CLSSIZ) {
				progerr(gettext(ERR_CLASSLONG));
				exit(1);
			}
			for (pt = classname; *pt; pt++) {
				if (!isalpha(*pt) && !isdigit(*pt)) {
					progerr(gettext(ERR_CLASSCHAR));
					exit(1);
				}
			}
			break;

		/*
		 * Don't map the client filesystem onto the server's. Assume
		 * the mounts have been made for us.
		 */
		    case 'M':
			map_client = 0;
			break;

		/*
		 * Allow admin to establish the client filesystem using a
		 * vfstab-like file of stable format.
		 */
		    case 'V':
			vfstab_file = flex_device(optarg, 2);
			map_client = 1;
			break;

		    case 'A':
			pkgrmremote++;
			break;

		    case 'R':	/* added for newroot option */
			if (!set_inst_root(optarg)) {
				progerr(gettext(ERR_ROOT_CMD));
				exit(1);
			}
			break;

		    default:
			usage();
		}
	}

	if (pkgrmremote && (!is_an_inst_root() || fflag || INSTALF))
		usage();

	/*
	 * Get the mount table info and store internally.
	 */
	if (get_mntinfo(map_client, vfstab_file))
		exit(1);

	/*
	 * This function defines the standard /var/... directories used later
	 * to construct the paths to the various databases.
	 */
	(void) set_PKGpaths(get_inst_root());

	/*
	 * If this is being installed on a client whose /var filesystem is
	 * mounted in some odd way, remap the administrative paths to the
	 * real filesystem. This could be avoided by simply mounting up the
	 * client now; but we aren't yet to the point in the process where
	 * modification of the filesystem is permitted.
	 */
	if (is_an_inst_root()) {
		int fsys_value;

		fsys_value = fsys(get_PKGLOC());
		if (use_srvr_map_n(fsys_value))
			set_PKGLOC(server_map(get_PKGLOC(), fsys_value));

		fsys_value = fsys(get_PKGADM());
		if (use_srvr_map_n(fsys_value))
			set_PKGADM(server_map(get_PKGADM(), fsys_value));
	}

	sortflag = 0;
	if ((pkginst = argv[optind++]) == NULL)
		usage();

	/*
	 * The following is used to setup the environment. Note that the
	 * variable 'BASEDIR' is only meaningful for this utility if there
	 * is an install root, recorded in PKG_INSTALL_ROOT. Otherwise, this
	 * utility can create a file or directory anywhere unfettered by
	 * the basedir associated with the package instance.
	 */
	if ((err = set_basedirs(0, NULL, pkginst, 1)) != 0)
		exit(err);

	if (INSTALF)
		mkbasedir(0, get_basedir());

	if (fflag) {
		/* installf and removef must only have pkginst */
		if (optind != argc)
			usage();
	} else {
		/*
		 * installf and removef must have at minimum
		 * pkginst & pathname specified on command line
		 */
		if (optind >= argc)
			usage();
	}
	if (REMOVEF) {
		if (classname)
			usage();
	}
	if (pkgnmchk(pkginst, "all", 0)) {
		progerr(gettext(ERR_INVAL), pkginst);
		exit(1);
	}
	if (fpkginst(pkginst, NULL, NULL) == NULL) {
		progerr(gettext(ERR_NOTINST), pkginst);
		exit(1);
	}

	/* Until 2.9, set it from the execption list */
	if (pkginst && exception_pkg(pkginst, LINK))
		set_nonABI_symlinks();

	/*
	 * This maps the client filesystems into the server's space.
	 */
	if (map_client && !mount_client())
		logerr(gettext(MSG_MANMOUNT));

	if (!ocfile(&mapfp, &tmpfp, 0L))
		quit(1);

	if (fflag)
		dbchg = dofinal(mapfp, tmpfp, REMOVEF, classname);
	else {
		if (INSTALF) {
			dbst = INST_RDY;
			if (installf(argc-optind, &argv[optind]))
				quit(1);
		} else {
			dbst = RM_RDY;
			removef(argc-optind, &argv[optind]);
		}

		dbchg = pkgdbmerg(mapfp, tmpfp, extlist, 0);
		if (dbchg < 0) {
			progerr(gettext(ERR_MERG));
			quit(99);
		}
	}
	if (dbchg) {
		if ((n = swapcfile(mapfp, tmpfp, pkginst)) == RESULT_WRN)
			warnflag++;
		else if (n == RESULT_ERR)
			quit(99);

		relslock();	/* Unlock the database. */
	}

	if (REMOVEF && !fflag) {
		for (n = 0; extlist[n]; n++) {
			ept = &(extlist[n]->cf_ent);

			if (!extlist[n]->mstat.shared) {
				/*
				 * Only output paths that can be deleted.
				 * so need to skip if the object is owned
				 * by a remote server and removal is not
				 * being forced.
				 */
				if (ept->pinfo &&
				    (ept->pinfo->status == SERVED_FILE) &&
				    !pkgrmremote)
					continue;

				c = 0;
				if (is_a_cl_basedir() &&
				    !is_an_inst_root()) {
					c = strlen(get_client_basedir());
					(void) sprintf(outbuf, "%s/%s\n",
					    get_basedir(),
					    &(ept->path[c]));
				} else if (is_an_inst_root())
					(void) sprintf(outbuf, "%s/%s\n",
					    get_inst_root(),
					    &(ept->path[c]));
				else
					(void) sprintf(outbuf, "%s\n",
					    &(ept->path[c]));
				canonize(outbuf);
				(void) printf("%s", outbuf);
			}
		}
	} else if (INSTALF && !fflag) {
		for (n = 0; extlist[n]; n++) {
			ept = &(extlist[n]->cf_ent);

			if (strchr("dxcbp", ept->ftype)) {
				tp = fixpath(ept->path);
				(void) averify(1, &ept->ftype,
				    tp, &ept->ainfo);
			}
		}
	}

	/* Sort the contents files if needed */
	if (sortflag) {
		int n;

		warnflag += (ocfile(&mapfp, &tmpfp, 0L)) ? 0 : 1;
		if (!warnflag) {
			cmd = (char *)malloc(strlen(CMD_SORT) +
				strlen(get_PKGADM()) + strlen("/contents") + 5);
			(void) sprintf(cmd,
			    "%s %s/contents", CMD_SORT, get_PKGADM());
			pp = popen(cmd, "r");
			if (pp == NULL) {
				(void) fclose(mapfp);
				(void) fclose(tmpfp);
				free(cmd);
				progerr(gettext(ERR_SORT));
				quit(1);
			}
			while (fgets(line, 1024, pp) != NULL) {
				if (line[0] != DUP_ENTRY)
					(void) fputs(line, tmpfp);
			}
			free(cmd);
			(void) pclose(pp);
			if ((n = swapcfile(mapfp, tmpfp, pkginst)) ==
			    RESULT_WRN)
				warnflag++;
			else if (n == RESULT_ERR)
				quit(99);

			relslock();	/* Unlock the database. */
		}
	}
	quit(warnflag ? 1 : 0);
}

void
quit(int n)
{
	unmount_client();

	exit(n);
}

void
usage(void)
{
	char *prog = get_prog_name();

	if (REMOVEF) {
		(void) fprintf(stderr, gettext(ERR_USAGE0), prog, prog);
	} else {
		(void) fprintf(stderr, gettext(ERR_USAGE1), prog, prog, prog);
	}
	exit(1);
}
