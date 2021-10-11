/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1995,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)main.c	1.32	99/08/30 SMI"	/* SVr4.0  1.11.4.2	*/

/*  5-20-92 	added newroot functions */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <locale.h>
#include <libintl.h>
#include <pkgstrct.h>
#include <pkgdev.h>
#include <pkginfo.h>
#include <pkglocs.h>
#include "install.h"
#include "msgdefs.h"
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"

extern char	*pkgdir;

#define	ASK_CONFIRM	"Do you want to remove this package?"
#define	ERR_ROOT_CMD	"Command line install root contends with environment."
#define	ERR_ROOT_SET	"Could not set install root from the environment."
#define	ERR_WITH_S	"illegal combination of options with \"s\"."
#define	ERR_WITH_A	"illegal option combination \"-M\" with \"-A\"."
#define	ERR_USAGE	"usage:\n" \
			"\t%s [-a admin] [-n] [[-M|-A] -R host_path] " \
			"[-V fs_file] [-v] [pkg [pkg ...]]" \
			"\n\t%s -s spool [pkg [pkg ...]]\n"

struct admin adm;	/* holds info about installation admin */
struct pkgdev pkgdev;	/* holds info about the installation device */
int	doreboot;	/* non-zero if reboot required after installation */
int	ireboot;	/* non-zero if immediate reboot required */
int	failflag;	/* non-zero if fatal error has occurred */
int	warnflag;	/* non-zero if non-fatal error has occurred */
int	intrflag;	/* non-zero if any pkg installation was interrupted */
int	admnflag;	/* non-zero if any pkg installation was interrupted */
int	nullflag;	/* non-zero if any pkg installation was interrupted */
int	nointeract;	/* non-zero if no interaction with user should occur */
int	pkgrmremote;	/* remove package objects stored remotely  */
int	pkgverbose;	/* non-zero if verbose mode is selected */
int	npkgs;		/* the number of packages yet to be installed */
int	started;
char	*pkginst;	/* current package (source) instance to process */
char	*tmpdir;	/* location to place temporary files */
char	*vfstab_file = NULL;

void	(*func)(), ckreturn(int retcode);

static int	interrupted;
static char	*admnfile;	/* file to use for installation admin */

static int	doremove(int nodelete), pkgremove(int nodelete);

/* quit.c */
extern void	quit(int retcode), trap(int signo);

/* presvr4.c */
extern int	presvr4(char *pkg);

static void
usage(void)
{
	char	*prog = get_prog_name();

	(void) fprintf(stderr, gettext(ERR_USAGE), prog, prog);
	exit(1);
}

/*
 * BugID #1136942:
 * Assume the package complies with the standards as regards user
 * interaction during procedure scripts.
 */
static int	old_pkg = 0;
static int	old_symlinks = 0;

static int  no_map_client = 0;

main(int argc, char **argv)
{
	int	i, c, n;
	int	repeat;
	int	nodelete = 0;	/* do not delete files or run scripts */
	char	ans[MAX_INPUT],
		**pkglist,	/* points to array of packages */
		*device = 0;
	extern int	optind;
	extern char	*optarg;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	(void) set_prog_name(argv[0]);

	/* Read PKG_INSTALL_ROOT from the environment, if it's there. */
	if (!set_inst_root(getenv("PKG_INSTALL_ROOT"))) {
		progerr(gettext(ERR_ROOT_SET));
		exit(1);
	}

	while ((c = getopt(argc, argv, "FMR:s:a:V:vAn?")) != EOF) {
		switch (c) {
		    case 's':
			device = flex_device(optarg, 1);
			break;

		    case 'a':
			admnfile = flex_device(optarg, 0);
			break;

		/*
		 * Allow admin to establish the client filesystem using a
		 * vfstab-like file of stable format.
		 */
		    case 'V':
			vfstab_file = flex_device(optarg, 2);
			no_map_client = 0;
			break;

		/*
		 * Allow admin to remove objects from a service area via
		 * a reference client.
		 */
		    case 'A':
			pkgrmremote++;
			break;

		    case 'v':
			pkgverbose++;
			break;

		    case 'M':
			no_map_client = 1;
			break;

		    case 'n':
			nointeract++;
			break;

		    case 'F':
			nodelete++;
			break;

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

	if ((admnfile || pkgrmremote || pkgverbose ||
	    is_an_inst_root()) && device) {
		progerr(gettext(ERR_WITH_S));
		usage();
	}

	if (no_map_client && pkgrmremote) {
		progerr(gettext(ERR_WITH_A));
		usage();
	}

	if (nointeract && (optind == argc))
		usage();

	func = signal(SIGINT, trap);
	if (func != SIG_DFL)
		(void) signal(SIGINT, func);
	(void) signal(SIGHUP, trap);

	tmpdir = getenv("TMPDIR");
	if (tmpdir == NULL)
		tmpdir = P_tmpdir;
	/* initialize path parameters */

	set_PKGpaths(get_inst_root());

	/*
	 * initialize installation admin parameters
	 */
	if (device == NULL)
		setadmin(admnfile);

	/*
	 * See if user wants this to be handled as an old style pkg.
	 * NOTE : the ``exception_pkg()'' stuff is to be used only
	 * through on495. This function comes out for on1095. See
	 * PSARC 1993-546. -- JST
	 */
	if (getenv("NONABI_SCRIPTS") != NULL)
		old_pkg = 1;

	/*
	 * See if the user wants to process symlinks consistent with
	 * the old behavior.
	 */

	if (getenv("PKG_NONABI_SYMLINKS") != NULL)
		old_symlinks = 1;

	if (devtype((device ? device : get_PKGLOC()), &pkgdev) ||
	    pkgdev.dirname == NULL) {
		progerr(gettext("bad device <%s> specified"),
		    device ? device : get_PKGLOC());
		quit(1);
	}

	pkgdir = pkgdev.dirname;
	repeat = ((optind >= argc) && pkgdev.mount);

again:

	if (pkgdev.mount) {
		if (n = pkgmount(&pkgdev, NULL, 0, 0, 1))
			quit(n);
	}

	if (chdir(pkgdev.dirname)) {
		progerr(gettext(ERR_CHDIR), pkgdev.dirname);
		quit(99);
	}

	pkglist = gpkglist(pkgdev.dirname, &argv[optind]);
	if (pkglist == NULL) {
		if (errno == ENOPKG) {
			/* check for existence of pre-SVR4 package */
			progerr(gettext(ERR_NOPKGS), pkgdev.dirname);
			quit(1);
		} else {
			switch (errno) {
			    case ESRCH:
				quit(1);
				break;

			    case EINTR:
				quit(3);
				break;

			    default:
				progerr(
				    gettext("internal error in gpkglist()"));
				quit(99);
			}
		}
	}

	for (npkgs = 0; pkglist[npkgs]; /* void */)
		npkgs++;

	interrupted = 0;
	for (i = 0; (pkginst = pkglist[i]) != NULL; i++) {
		started = 0;
		if (ireboot) {
			ptext(stderr, gettext(MSG_SUSPEND), pkginst);
			continue;
		}
		if (interrupted) {
			if (npkgs == 1)
				echo(gettext(MSG_1MORETODO));
			else
				echo(gettext(MSG_MORETODO), npkgs);
			if (nointeract)
				quit(0);
			if (n = ckyorn(ans, NULL, NULL, NULL,
			    gettext(ASK_CONTINUE)))
				quit(n);
			if (strchr("yY", *ans) == NULL)
				quit(0);
		}
		interrupted = 0;
		ckreturn(doremove(nodelete));
		npkgs--;
	}
	if (pkgdev.mount) {
		(void) chdir("/");
		if (pkgumount(&pkgdev)) {
			progerr(gettext("unable to unmount <%s>"),
				pkgdev.bdevice);
			quit(99);
		}
	}
	if (!ireboot && repeat)
		goto again;
	quit(0);
	/*NOTREACHED*/
#ifdef lint
	return (0);
#endif	/* lint */
}

static int
doremove(nodelete)
{
	struct pkginfo info;
	char	ans[MAX_INPUT];
	int	n;
	char	*prog = get_prog_name();

	info.pkginst = NULL;
	if (pkginfo(&info, pkginst, NULL, NULL)) {
		progerr(gettext("instance <%s> does not exist"), pkginst);
		return (2);
	}

	if (!nointeract) {
		if (info.status == PI_SPOOLED)
			echo(gettext(INFO_SPOOLED));
		else {
			if (getuid()) {
				progerr(gettext(ERR_NOTROOT), prog);
				exit(1);
			}
			echo(gettext(INFO_INSTALL));
		}
		echo("   %-14.14s  %s", info.pkginst, info.name);
		if (info.arch || info.version)
			(void) fprintf(stderr, "   %14.14s  ", "");
		if (info.arch)
			(void) fprintf(stderr, "(%s) ", info.arch);
		if (info.version)
			(void) fprintf(stderr, "%s", info.version);
		if (info.arch || info.version)
			(void) fprintf(stderr, "\n");

		if (n = ckyorn(ans, NULL, NULL, NULL,
		    gettext(ASK_CONFIRM)))
			quit(n);
		if (strchr("yY", *ans) == NULL)
			return (0);
	}

	if (info.status == PI_PRESVR4)
		return (presvr4(pkginst));

	if (info.status == PI_SPOOLED) {
		/* removal from a directory */
		echo(gettext(INFO_RMSPOOL), pkginst);
		return (rrmdir(pkginst));
	}

	if (getuid()) {
		progerr(gettext(ERR_NOTROOT), prog);
		exit(1);
	}

	return (pkgremove(nodelete));
}

/*
 *  function which checks the indicated return value
 *  and indicates disposition of installation
 */
void
ckreturn(int retcode)
{

	switch (retcode) {
	    case  0:
	    case 10:
	    case 20:
		break; /* empty case */

	    case  1:
	    case 11:
	    case 21:
		failflag++;
		interrupted++;
		break;

	    case  2:
	    case 12:
	    case 22:
		warnflag++;
		interrupted++;
		break;

	    case  3:
	    case 13:
	    case 23:
		intrflag++;
		interrupted++;
		break;

	    case  4:
	    case 14:
	    case 24:
		admnflag++;
		interrupted++;
		break;

	    case  5:
	    case 15:
	    case 25:
		nullflag++;
		interrupted++;
		break;

	    default:
		failflag++;
		interrupted++;
		return;
	}
	if (retcode >= 20)
		ireboot++;
	else if (retcode >= 10)
		doreboot++;
}

#define	MAXARGS 15

static int
pkgremove(int nodelete)
{
	void	(*tmpfunc)();
	char	*arg[MAXARGS], path[PATH_MAX];
	int	n, nargs;

	(void) sprintf(path, "%s/pkgremove", PKGBIN);

	nargs = 0;
	arg[nargs++] = path;
	/*
	 * bug ID #1136942:
	 * old_pkg refers to a pkg requiring operator interaction
	 * during a procedure script -- JST
	 */
	if (old_pkg)
		arg[nargs++] = "-o";
	if (old_symlinks)
		arg[nargs++] = "-y";
	if (no_map_client)
		arg[nargs++] = "-M";
	if (pkgrmremote)
		arg[nargs++] = "-A";
	if (pkgverbose)
		arg[nargs++] = "-v";
	if (nointeract)
		arg[nargs++] = "-n";
	if (admnfile) {
		arg[nargs++] = "-a";
		arg[nargs++] = admnfile;
	}
	if (vfstab_file) {
		arg[nargs++] = "-V";
		arg[nargs++] = vfstab_file;
	}
	if (is_an_inst_root()) {
		arg[nargs++] = "-R";
		arg[nargs++] = get_inst_root();
	}

	if (nodelete)
		arg[nargs++] = "-F";

	arg[nargs++] = "-N";
	arg[nargs++] = get_prog_name();
	arg[nargs++] = pkginst;
	arg[nargs++] = NULL;

	tmpfunc = signal(SIGINT, func);
	n = pkgexecv(NULL, NULL, NULL, NULL, arg);
	(void) signal(SIGINT, tmpfunc);

	(void) signal(SIGINT, func);
	return (n);
}
