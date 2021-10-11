/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)main.c	1.34	99/08/30 SMI"	/* SVr4.0  1.12.7.1	*/

/*  5-20-92	added newroot functions */

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>	/* mkdir() definition? */
#include <signal.h>
#include <errno.h>
#include <pkgdev.h>
#include <pkginfo.h>
#include <pkglocs.h>
#include <locale.h>
#include <libintl.h>
#include "install.h"
#include "libinst.h"
#include <pkglib.h>
#include "libadm.h"
#include "msgdefs.h"

extern int	ckquit;
extern int	ds_totread;

/* main.c */
extern void	ckreturn(int retcode);

/* quit.c */
extern void	quit(int retcode), trap(int signo);

/* presvr4.c */
extern int 	presvr4(char **ppkg);

/* option to disable checksum */
static int	dochecksum = 1;

struct	admin	adm;		/* holds info about installation admin */
struct	pkgdev	pkgdev;		/* holds info about the installation device */
int doreboot;		/* non-zero if reboot required after installation */
int ireboot;	/* non-zero if immediate reboot required */
int failflag;	/* non-zero if fatal error has occurred */
int warnflag;	/* non-zero if non-fatal error has occurred */
int intrflag;	/* non-zero if any pkg installation was interrupted */
int admnflag;	/* non-zero if any pkg installation was interrupted */
int nullflag;	/* non-zero if any pkg installation was interrupted */
int nointeract;	/* non-zero if no interaction with user should occur */
int pkgverbose;	/* non-zero if verbose mode is selected */
int npkgs;		/* the number of packages yet to be installed */

char	*pkginst;	/* current package (source) instance to process */
char	*respfile;	/* response pathname (or NULL) */
char	*tmpdir;	/* location to place temporary files */
char	*ids_name;	/* name of data stream device */
void	(*func)();

static	char	*device,
		*admnfile,	/* file to use for installation admin */
		*respdir,	/* used is respfile is a directory spec */
		*spoolto;	/* optarg specified with -s (or NULL) */
static	int	interrupted,	/* if last pkg installation was quit */
		askflag,	/* non-zero if this is the "pkgask" process */
		needconsult;	/* it is essential to ask the admin now */

#define	MSG_PROC_CONT	"\nProcessing continuation packages from <%s>"
#define	MSG_PROC_INST	"\nProcessing package instance <%s> from <%s>"

#define	ERR_ROOT_CMD	"Command line install root contends with environment."

#define	MAXARGS 30

static void	usage(void);
static int	pkginstall(char *ir);
static int  no_map_client = 0;
static int	silent = 0;
static char    *rw_block_size = NULL;
static int	init_install = 0;
static char	*vfstab_file = NULL;
static char	*pkgdrtarg = NULL;
static char	*pkgcontsrc = NULL;

/*
 * Assume the package is ABI and POSIX compliant as regards user
 * interactiion during procedure scripts.
 */
static int	old_pkg = 0;

/* Assume pkg should be installed according to the ABI */
static int	old_symlinks = 0;

main(int argc, char **argv)
{
	static	char	Spoolto[PATH_MAX];
	int	i, c, n,
		repeat;
	char	ans[MAX_INPUT],
		path[PATH_MAX],
		**pkglist;	/* points to array of packages */
	char	*prog;
	extern char	*optarg;
	extern int	optind;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	prog = set_prog_name(argv[0]);

	askflag = (strcmp(prog, "pkgask") == 0);

	device = NULL;

	while ((c = getopt(argc, argv, "B:Ss:Cd:a:R:r:D:c:IV:vnM?")) != EOF) {
		switch (c) {
		/* Bug fix #1081861  Install too slow. */
		    case 'B':
			rw_block_size = optarg;
			break;

		/* Bug fix #1069891  suppress copyright. */
		    case 'S':
			silent++;
			break;

		/*
		 * Informs scripts that this is
		 * an initial install by setting the environment parameter
		 * PKG_INIT_INSTALL=TRUE for all scripts. They may use it as
		 * they see fit, safe in the knowledge that the target
		 * filesystem is tabula rasa.
		 */
		    case 'I':
			init_install++;
			break;

		    case 'M':
			no_map_client = 1;
			break;

		/*
		 * Informs the pkginstall portion to mount up a
		 * client filesystem based upon the
		 * supplied vfstab-like file of stable format.
		 */
		    case 'V':
			vfstab_file = flex_device(optarg, 2);
			no_map_client = 0;
			break;

		    case 'v':
			pkgverbose++;
			break;

		    case 's':
			spoolto = flex_device(optarg, 1);
			break;

		    case 'd':
			device = flex_device(optarg, 1);
			break;

		    case 'a':
			admnfile = flex_device(optarg, 0);
			break;

		    case 'r':
			respfile = flex_device(optarg, 2);
			if (isdir(respfile) == 0)
				respdir = respfile;
			break;

		    case 'n':
			nointeract++;
			break;

		/*
		 * Not a public interface: This disables checksum tests on
		 * the source files. It speeds up installation a little bit.
		 */
		    case 'C':
			dochecksum = 0;
			break;

		/* Change the root. */
		    case 'R':
			if (!set_inst_root(optarg)) {
				progerr(gettext(ERR_ROOT_CMD));
				exit(1);
			}
			break;

		/*
		 * Not a public interface: This allows designation of a
		 * dryrun file. This pkgadd will create dryrun files
		 * in the directory provided.
		 */
		    case 'D':
			pkgdrtarg = flex_device(optarg, 0);
			break;

		/*
		 * Not a public interface: This allows designation of a
		 * continuation file. It is the same format as a dryrun file
		 * but it is used to take up where the dryrun left off.
		 */
		    case 'c':
			pkgcontsrc = flex_device(optarg, 0);
			break;

		    default:
			usage();
		}
	}

	/*
	 * Later, it may be decided to pursue this ability to continue to an
	 * actual installation based only on the dryrun data. At this time,
	 * it is too risky.
	 */
	if (pkgcontsrc && !pkgdrtarg) {
		progerr(gettext("live continue mode is not supported"));
		usage();
	}

	if (askflag && (spoolto || nointeract))
		usage();

	if (spoolto && (nointeract || admnfile || respfile))
		usage();

	func = signal(SIGINT, trap);
	if (func != SIG_DFL)
		(void) signal(SIGINT, func);
	(void) signal(SIGHUP, trap);

	/*
	 * initialize installation admin parameters
	 */
	set_PKGpaths(get_inst_root());

	setadmin(admnfile);

	if (device == NULL) {
		device = devattr("spool", "pathname");
		if (device == NULL) {
			progerr(gettext(ERR_NODEVICE));
			quit(1);
		}
	}

	if (spoolto && is_an_inst_root()) {
		(void) strcpy(Spoolto, get_inst_root());
		(void) strcat(Spoolto, spoolto);
		spoolto = Spoolto;
	}

	if (spoolto)
		quit(pkgtrans(device, spoolto, &argv[optind], 0));


	if (getuid()) {
		progerr(gettext(ERR_NOTROOT), prog);
		exit(1);
	}

	/*
	 * process response file argument
	 */
	if (respfile) {
		if (respfile[0] != '/') {
			progerr(
			    gettext("response file <%s> must be full pathname"),
			    respfile);
			quit(1);
		}
		if (respdir == NULL) {
			if (askflag) {
				if (access(respfile, 0) == 0) {
					progerr(gettext(ERR_NORESP), respfile);
					quit(1);
				}
			} else if (access(respfile, 0)) {
				progerr(gettext(ERR_ACCRESP), respfile);
				quit(1);
			}
		}
	} else if (askflag) {
		progerr(gettext("response file (to write) is required"));
		usage();
		quit(1);
	}

	tmpdir = getenv("TMPDIR");
	if (tmpdir == NULL)
		tmpdir = P_tmpdir;

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

	if (devtype(device, &pkgdev)) {
		progerr(gettext("bad device <%s> specified"), device);
		quit(1);
	}

again:

	if (ids_name) {
		(void) ds_close(1);
		rrmdir(pkgdev.dirname);
	}
	ids_name = NULL;
	if (pkgdev.bdevice) {
		if (n = _getvol(pkgdev.bdevice, NULL, NULL,
		    gettext("Insert %v into %p."), pkgdev.norewind)) {
			if (n == 3)
				quit(3);
			if (n == 2)
				progerr(gettext("unknown device <%s>"),
				pkgdev.name);
			else {
				progerr(gettext(ERR_PKGVOL));
				logerr(gettext("getvol() returned <%d>"), n);
			}
			quit(99);
		}
		if (ds_readbuf(pkgdev.cdevice))
			ids_name = pkgdev.cdevice;
	}

	if (pkgdev.cdevice && !pkgdev.bdevice)
		ids_name = pkgdev.cdevice;
	else if (pkgdev.pathname)
		ids_name = pkgdev.pathname;

	if (ids_name) {
		/* initialize datastream */
		pkgdev.dirname = tempnam(tmpdir, "dstream");
		if (!pkgdev.dirname || mkdir(pkgdev.dirname, 0755)) {
			progerr(gettext(ERR_STREAMDIR));
			quit(99);
		}
	}

	repeat = (optind >= argc);

	if (!ids_name && pkgdev.mount) {
		pkgdev.rdonly++;
		if (n = pkgmount(&pkgdev, NULL, 0, 0, 0))
			goto again;
	}

	if (ids_name) {
		/* use character device to force rewind of datastream */
		if (pkgdev.cdevice && !pkgdev.bdevice &&
		(n = _getvol(pkgdev.name, NULL, NULL, NULL, pkgdev.norewind))) {
			if (n == 3)
				quit(3);
			if (n == 2)
				progerr(gettext("unknown device <%s>"),
				    pkgdev.name);
			else {
				progerr(gettext(ERR_PKGVOL));
				logerr(gettext("getvol() returned <%d>"), n);
			}
			quit(99);
		}
		if (chdir(pkgdev.dirname)) {
			progerr(gettext(ERR_CHDIR), pkgdev.dirname);
			quit(99);
		}
		if (ds_init(ids_name, &argv[optind], pkgdev.norewind)) {
			progerr(gettext(ERR_DSINIT), ids_name);
			quit(99);
		}
	}

	pkglist = gpkglist(pkgdev.dirname, &argv[optind]);
	if (pkglist == NULL) {
		if (errno == ENOPKG) {
			/* check for existence of pre-SVR4 package */
			(void) sprintf(path, "%s/install/INSTALL",
				pkgdev.dirname);
			if (access(path, 0) == 0) {
				pkginst = ((optind < argc) ?
					argv[optind++] : NULL);
				ckreturn(presvr4(&pkginst));
				if (repeat || (optind < argc))
					goto again;
				quit(0);
			}
			progerr(gettext(ERR_NOPKGS), pkgdev.dirname);
			quit(1);
		} else {
			/* some internal error */
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
				break;
			}
		}
	}
	if (ids_name)
		ds_order(pkglist);

	for (npkgs = 0; pkglist[npkgs]; /* void */)
		npkgs++;

	interrupted = 0;
	for (i = 0; (pkginst = pkglist[i]) != NULL; i++) {
		if (ireboot && !askflag) {
			ptext(stderr, gettext(MSG_SUSPEND), pkginst);
			continue;
		}
		if (interrupted) {
			if (npkgs == 1) {
				if (askflag)
					echo(gettext(MSG_1MORE_PROC));
				else
					echo(gettext(MSG_1MORE_INST));
			} else {
				if (askflag)
					echo(gettext(MSG_MORE_PROC), npkgs);
				else
					echo(gettext(MSG_MORE_INST), npkgs);
			}
			if (nointeract) {
				if (needconsult) {
					quit(0);
				} else {
					ckquit = 1;
				}
			} else {
				ckquit = 0;
				if (n = ckyorn(ans, NULL, NULL, NULL,
				    gettext(ASK_CONTINUE)))
					quit(n);
				ckquit = 1;
				if (strchr("yY", *ans) == NULL)
					quit(0);
			}
		}
		if (askflag) {
			if (respdir) {
				(void) sprintf(path, "%s/%s", respdir, pkginst);
				respfile = path;
			} else if (npkgs > 1) {
				progerr(
				    gettext("too many packages referenced!"));
				quit(1);
			}
		}
		interrupted = 0;

		echo(gettext(MSG_PROC_INST), pkginst, device);

		/*
		 * If we're installing another package in the same session,
		 * the second through nth pkginstall, must continue from
		 * where the prior one left off. For this reason, the
		 * continuation feature (implied by the nature of the
		 * command) is used for the remaining packages.
		 */
		if (i == 1 && pkgdrtarg)
			pkgcontsrc = pkgdrtarg;

		ckreturn(pkginstall(get_inst_root()));
		npkgs--;
		if ((npkgs <= 0) && (pkgdev.mount || ids_name)) {
			(void) chdir("/");
			if (!ids_name)
				(void) pkgumount(&pkgdev);
		}
	}
	if (!ireboot && repeat && !pkgdev.pathname)
		goto again;
	quit(0);
	/*NOTREACHED*/
#ifdef lint
	return (0);
#endif	/* lint */
}

static int
pkginstall(char *ir)
{
	void	(*tmpfunc)();
	char	*arg[MAXARGS], path[PATH_MAX];
	char	buffer[256];
	int	n, nargs, dparts;

	(void) sprintf(path, "%s/pkginstall", PKGBIN);

	nargs = 0;
	arg[nargs++] = path;
	/* Bug fix #1081861  Install too slow */
	if (rw_block_size != NULL) {
		arg[nargs++] = "-B";
		arg[nargs++] = rw_block_size;
	}

	/* option to disable checksum */
	if (!dochecksum)
		arg[nargs++] = "-C";

	/*
	 * bug ID #1136942:
	 * old_pkg refers to a pkg requiring operator interaction
	 * during a procedure script (common before on1093) -- JST
	 */
	if (old_pkg)
		arg[nargs++] = "-o";
	if (old_symlinks)
		arg[nargs++] = "-y";
	/* Bug fix #1069891  suppress copyright */
	if (silent)
		arg[nargs++] = "-S";
	if (init_install)
		arg[nargs++] = "-I";
	if (no_map_client)
		arg[nargs++] = "-M";
	if (pkgverbose)
		arg[nargs++] = "-v";
	if (askflag)
		arg[nargs++] = "-i";
	else if (nointeract)
		arg[nargs++] = "-n";
	if (admnfile) {
		arg[nargs++] = "-a";
		arg[nargs++] = admnfile;
	}
	if (pkgdrtarg) {
		arg[nargs++] = "-D";
		arg[nargs++] = pkgdrtarg;
	}
	if (pkgcontsrc) {
		arg[nargs++] = "-c";
		arg[nargs++] = pkgcontsrc;
	}
	if (vfstab_file) {
		arg[nargs++] = "-V";
		arg[nargs++] = vfstab_file;
	}
	if (respfile) {
		arg[nargs++] = "-r";
		arg[nargs++] = respfile;
	}
	/* option to change the root */
	if (ir && *ir) {
		arg[nargs++] = "-R";
		arg[nargs++] = ir;
	}

	if (ids_name != NULL) {
		arg[nargs++] = "-d";
		arg[nargs++] = ids_name;
		dparts = ds_findpkg(ids_name, pkginst);
		if (dparts < 1) {
			progerr(gettext(ERR_DSARCH), pkginst);
			quit(99);
		}
		arg[nargs++] = "-p";
		ds_putinfo(buffer);
		arg[nargs++] = buffer;
	} else if (pkgdev.mount != NULL) {
		arg[nargs++] = "-d";
		arg[nargs++] = pkgdev.bdevice;
		arg[nargs++] = "-m";
		arg[nargs++] = pkgdev.mount;
		if (pkgdev.fstyp != NULL) {
			arg[nargs++] = "-f";
			arg[nargs++] = pkgdev.fstyp;
		}
	}
	arg[nargs++] = "-N";
	arg[nargs++] = get_prog_name();
	arg[nargs++] = pkgdev.dirname;
	arg[nargs++] = pkginst;
	arg[nargs++] = NULL;

	tmpfunc = signal(SIGINT, func);
	n = pkgexecv(NULL, NULL, NULL, NULL, arg);
	(void) signal(SIGINT, tmpfunc);
	if (ids_name != NULL)
		ds_totread += dparts; /* increment number of parts written */
	return (n);
}

/*
 *  function which checks the indicated return value
 *  and indicates disposition of installation
 */
void
ckreturn(int retcode)
{
	needconsult = 0;

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
		needconsult++;
		break;

	    case  2:
	    case 12:
	    case 22:
		warnflag++;
		interrupted++;
		needconsult++;
		break;

	    case  3:
	    case 13:
	    case 23:
		intrflag++;
		interrupted++;
		needconsult++;
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
		needconsult++;
		break;

	    default:
		failflag++;
		interrupted++;
		needconsult++;
		return;
	}

	if (retcode >= 20)
		ireboot++;
	else if (retcode >= 10)
		doreboot++;
}

static void
usage(void)
{
	char *prog = get_prog_name();

	if (askflag) {
		(void) fprintf(stderr, gettext(ERR_USAGE0), prog);
	} else {
		(void) fprintf(stderr, gettext(ERR_USAGE1), prog, prog);
	}
	exit(1);
	/*NOTREACHED*/
}
