/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1995,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)main.c	1.71	99/10/07 SMI"	/* SVr4.0 1.19.6.1 */

#include <stdio.h>
#include <time.h>
#include <wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <ulimit.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <locale.h>
#include <libintl.h>
#include <pkgstrct.h>
#include <pkginfo.h>
#include <pkgdev.h>
#include <pkglocs.h>
#include <pwd.h>
#include <pkglib.h>
#include "libadm.h"
#include "libinst.h"
#include "dryrun.h"
#include "pkginstall.h"

extern char	**environ;
extern char	*pkgabrv, *pkgname, *pkgarch, *pkgvers, pkgwild[];

struct cfextra **extlist;

static int	vcfile(void);
static int	rdonly(char *p);
static void	copyright(void), usage(void);
static void	unpack(void);
static void	rm_icas(char *casdir);
static void	do_pkgask(void);
static int	ck_instbase(void);
static int	mv_pkgdirs(void);
static int	merg_pkginfos(struct cl_attr **pclass);
static int	cp_pkgdirs(void);
static void set_dryrun_dir_loc(void);
static int	merg_respfile(void);
static void	ck_w_dryrun(int (*func)(), int type);
static char	path[PATH_MAX];

static char	*ro_params[] = {
	"PATH", "NAME", "PKG", "PKGINST",
	"VERSION", "ARCH",
	"INSTDATE", "CATEGORY",
	NULL
};

#define	DEFPATH		"/sbin:/usr/sbin:/usr/bin"
#define	MALSIZ	4	/* best guess at likely maximum value of MAXINST */
#define	LSIZE	256	/* maximum line size supported in copyright file */
#define	SCRIPT	0	/* Tells exception_pkg() which pkg list to use */
#define	LINK	1

#define	MSG_MANMOUNT	"Assuming mounts have been provided."

#define	ERR_USAGE	"usage:\n" \
	"\tpkginstall [-o] [-n] [-d device] " \
	"[-m mountpt [-f fstype]] [-v] " \
	"[[-M] -R host_path] [-V fs_file] [-b bindir] [-a admin_file] " \
	"[-r resp_file] [-N calling_prog] directory pkginst\n"

#define	ERR_CREAT_CONT	"unable to create contents file <%s>"
#define	ERR_ROOT_SET	"Could not set install root from the environment."
#define	ERR_ROOT_CMD	"Command line install root contends with environment."
#define	ERR_MEMORY	"memory allocation failure, errno=%d"
#define	ERR_INTONLY	"unable to install <%s> without user interaction"
#define	ERR_NOREQUEST	"package does not contain an interactive request script"
#define	ERR_LOCKFILE	"unable to create lockfile <%s>"
#define	ERR_PKGINFO	"unable to open pkginfo file <%s>"
#define	ERR_PKGBINCP	"unable to copy <%s>\n\tto <%s>"
#define	ERR_PKGBINREN	"unable to rename <%s>\n\tto <%s>"
#define	ERR_RESPONSE	"unable to open response file <%s>"
#define	ERR_PKGMAP	"unable to open pkgmap file <%s>"
#define	ERR_MKDIR	"unable to make temporary directory <%s>"
#define	ERR_RMDIR	"unable to remove directory <%s> and its contents"
#define	ERR_CHDIR	"unable to change directory to <%s>"
#define	ERR_ADMBD	"%s is already installed at %s. Admin file will " \
			    "force a duplicate installation at %s."
#define	ERR_NEWBD	"%s is already installed at %s. Duplicate " \
			    "installation attempted at %s."
#define	ERR_DSTREAM	"unable to unpack datastream"
#define	ERR_DSTREAMSEQ	"datastream seqeunce corruption"
#define	ERR_DSTREAMCNT	"datastream early termination problem"
#define	ERR_RDONLY 	"read-only parameter <%s> cannot be assigned a value"
#define	ERR_REQUEST	"request script did not complete successfully"
#define	WRN_CHKINSTALL	"checkinstall script suspends"
#define	ERR_CHKINSTALL	"checkinstall script did not complete successfully"
#define	ERR_PREINSTALL	"preinstall script did not complete successfully"
#define	ERR_POSTINSTALL	"postinstall script did not complete successfully"
#define	ERR_OPRESVR4	"unable to unlink options file <%s>"
#define	ERR_SYSINFO	"unable to process installed package information, " \
			"errno=%d"
#define	ERR_NOTROOT	"You must be \"root\" for %s to execute properly."
#define	ERR_BADULIMIT	"cannot process invalid ULIMIT value of <%s>."
#define	MSG_INST_ONE	"   %d package pathname is already properly installed."
#define	MSG_INST_MANY	"   %d package pathnames are already properly " \
			"installed."
#define	MSG_BASE_USED	"Using <%s> as the package base directory."

struct admin	adm;
struct pkgdev	pkgdev;

int	nocnflct, nosetuid;
int	dbchg;
int	rprcflag;
int	iflag;
int	dparts = 0;

int	dreboot = 0;
int	ireboot = 0;
int	warnflag = 0;
int	failflag = 0;
int	started = 0;
int	update = 0;
int	nointeract = 0;
int	pkgverbose = 0;
ulong	pkgmap_blks = 0L;
int	opresvr4 = 0;
int	maxinst = 1;
char    *rw_block_size = NULL;

char	*pkginst,
	*msgtext,
	instdir[PATH_MAX],
	pkgloc[PATH_MAX],
	pkgbin[PATH_MAX],
	pkgloc_sav[PATH_MAX],
	pkgsav[PATH_MAX],
	ilockfile[PATH_MAX],
	rlockfile[PATH_MAX],
	savlog[PATH_MAX],
	tmpdir[PATH_MAX];

/*
 * BugID #1136942:
 * The following variable is the name of the device to which stdin
 * is connected during execution of a procedure script. PROC_STDIN is
 * correct for all ABI compliant packages. For non-ABI-compliant
 * packages, the '-o' command line switch changes this to PROC_XSTDIN
 * to allow user interaction during these scripts. -- JST
 */
static char	*script_in = PROC_STDIN;	/* assume ABI compliance */

static char	*pkgdrtarg = NULL;
static char	*pkgcontsrc = NULL;
static int	non_abi_scripts = 0;	/* bug id 1136942, not ABI compliant */
static char	*respfile = NULL;
static char	*srcinst = NULL;
static	int	silent = 0;

main(int argc, char *argv[])
{
	extern char	*optarg;
	extern int	optind;

	static char *cpio_names[] = {
	    "root",
	    "root.cpio",
	    "reloc",
	    "reloc.cpio",
	    "root.Z",
	    "root.cpio.Z",
	    "reloc.Z",
	    "reloc.cpio.Z",
	    0
	};

	struct pkginfo *prvinfo;
	FILE	*mapfp, *tmpfp;
	FILE	*fp;
	int		map_client = 1;
	int		c, n, err, init_install = 0, called_by_gui = 0;
	time_t	clock;
	int		npkgs, part, nparts;
	char	*pt, *env1,
		*device,
		**np, /* bug id 1096956 */
		*admnfile = NULL,
		*abi_comp_ptr,
		*abi_sym_ptr,
		*vfstab_file = NULL;
	char	cmdbin[PATH_MAX],
		p_pkginfo[PATH_MAX],
		p_pkgmap[PATH_MAX],
		script[PATH_MAX],
		cbuf[64],
		param[64];
	void	(*func)();
	int		is_comp_arch; 		/* bug id 1096956 */
	int		live_continue = 0;
	struct 	cl_attr	**pclass = NULL;
	struct	stat	statb;
	struct	statvfs	svfsb;

	(void) memset(path, '\0', sizeof (path));
	(void) memset(cmdbin, '\0', sizeof (cmdbin));
	(void) memset(script, '\0', sizeof (script));
	(void) memset(cbuf, '\0', sizeof (cbuf));
	(void) memset(param, '\0', sizeof (param));

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);
	(void) setbuf(stdout, NULL);

	setmapmode(MAPINSTALL);

	(void) set_prog_name(argv[0]);

	(void) umask(0022);

	device = NULL;

	if (!set_inst_root(getenv("PKG_INSTALL_ROOT"))) {
		progerr(gettext(ERR_ROOT_SET));
		exit(1);
	}

	while ((c = getopt(argc, argv,
	    "B:N:CSR:MFf:p:d:m:b:r:IiV:D:c:vynoa:?")) != EOF) {
		switch (c) {

		    case 'B':
			rw_block_size = optarg;
			break;

		    case 'S':
			silent++;
			break;

		    case 'I':
			init_install++;
			break;

		    case 'M':
			map_client = 0;
			break;

		/*
		 * Not a public interface: This is a private interface
		 * allowing admin to establish the client filesystem using a
		 * vfstab-like file of stable format.
		 */
		    case 'V':
			vfstab_file = flex_device(optarg, 2);
			map_client = 1;
			break;

		    case 'v':
			pkgverbose++;
			break;

		    case 'N':
			(void) set_prog_name(optarg);
			break;

		    case 'p':
			dparts = ds_getinfo(optarg);
			break;

		    case 'i':
			iflag++;
			break;

		    case 'f':
			pkgdev.fstyp = optarg;
			break;

		    case 'b':
			(void) strcpy(cmdbin, optarg);
			break;

		    case 'd':
			device = flex_device(optarg, 1);
			break;

		    case 'm':
			pkgdev.mount = optarg;
			pkgdev.rdonly++;
			pkgdev.mntflg++;
			break;

		    case 'r':
			respfile = flex_device(optarg, 2);
			break;

		    case 'n':
			nointeract++;
			break;

		    case 'a':
			admnfile = flex_device(optarg, 0);
			break;

		    /* This is an old non-ABI package */
		    case 'o':
			non_abi_scripts++;
			break;

		    /* This pkg needs to processed as an old non-ABI symlink */
		    case 'y':
			set_nonABI_symlinks();
			break;

		    case 'C':
			(void) checksum_off();
			break;

		    case 'R':
			if (!set_inst_root(optarg)) {
				progerr(gettext(ERR_ROOT_CMD));
				exit(1);
			}
			break;

		/* Designate a dryrun file. */
		    case 'D':
			pkgdrtarg = optarg;
			set_dryrun_mode();
			set_dr_info(DR_TYPE, INSTALL_TYPE);
			break;

		/* Designate a continuation file. */
		    case 'c':
			pkgcontsrc = optarg;
			set_continue_mode();
			set_dr_info(DR_TYPE, INSTALL_TYPE);
			init_contfile(pkgcontsrc);
			break;

		    default:
			usage();
		}
	}

	if (in_continue_mode() && !in_dryrun_mode()) {
		progerr(gettext("live continue mode is not supported"));
		usage();
	}

	if (iflag && (respfile == NULL))
		usage();

	if (device) {
		if (pkgdev.mount)
			pkgdev.bdevice = device;
		else
			pkgdev.cdevice = device;
	}
	if (pkgdev.fstyp && !pkgdev.mount) {
		progerr(gettext("-f option requires -m option"));
		usage();
	}

	/* BEGIN DATA GATHERING PHASE */
	/*
	 * Get the mount table info and store internally.
	 */
	if (in_continue_mode()) {
		if (!read_continuation())
			quit(99);
		if (!in_dryrun_mode())
			live_continue = 1;
	}

	if (!in_continue_mode()) {
		if (get_mntinfo(map_client, vfstab_file))
			quit(99);
	}

	/*
	 * This function defines the standard /var/... directories used later
	 * to construct the paths to the various databases.
	 */
	set_PKGpaths(get_inst_root());

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

	/*
	 * Initialize pkginfo PKGSAV entry, just in case we dryrun to
	 * somewhere else.
	 */
	set_infoloc(get_PKGLOC());

	pkgdev.dirname = argv[optind++];
	srcinst = argv[optind++];
	if (optind != argc)
		usage();

	(void) pkgparam(NULL, NULL);  /* close up prior pkg file if needed */

	/*
	 * Initialize installation admin parameters by reading
	 * the adminfile.
	 */
	if (!iflag && !live_continue)
		setadmin(admnfile);

	func = signal(SIGINT, trap);
	if (func != SIG_DFL)
		(void) signal(SIGINT, func);
	(void) signal(SIGHUP, trap);

	ckdirs();	/* create /var... directories if necessary */
	tzset();

	(void) sprintf(instdir, "%s/%s", pkgdev.dirname, srcinst);

	if (pt = getenv("TMPDIR"))
		(void) sprintf(tmpdir, "%s/installXXXXXX", pt);
	else
		(void) strcpy(tmpdir, "/tmp/installXXXXXX");

	if ((mktemp(tmpdir) == NULL) || mkdir(tmpdir, 0771)) {
		progerr(gettext(ERR_MKDIR), tmpdir);
		quit(99);
	}

	/*
	 * If the environment has a CLIENT_BASEDIR, that takes precedence
	 * over anything we will construct. We need to save it here because
	 * in three lines, the current environment goes away.
	 */
	(void) set_env_cbdir();	/* copy over environ */

	getuserlocale();

	environ = NULL;		/* Now putparam can be used */

	putuserlocale();

	if (init_install)
		putparam("PKG_INIT_INSTALL", "TRUE");

	putparam("INST_DATADIR", pkgdev.dirname);

	if (non_abi_scripts)
		putparam("NONABI_SCRIPTS", "TRUE");

	if (nonABI_symlinks())
		putparam("PKG_NONABI_SYMLINKS", "TRUE");

	if (!cmdbin[0])
		(void) strcpy(cmdbin, PKGBIN);
	(void) sprintf(path, "%s:%s", DEFPATH, cmdbin);
	putparam("PATH", path);
	putparam("OAMBASE", OAMBASE);

	(void) sprintf(p_pkginfo, "%s/%s", instdir, PKGINFO);
	(void) sprintf(p_pkgmap, "%s/%s", instdir, PKGMAP);

	/*
	 * This tests the pkginfo and pkgmap files for validity and
	 * puts all delivered pkginfo variables (except for PATH) into
	 * our environment. This is where a delivered pkginfo BASEDIR
	 * would come from. See set_basedirs() below.
	 */
	if (pkgenv(srcinst, p_pkginfo, p_pkgmap))
		quit(1);

	echo("\n%s", pkgname);
	echo("(%s) %s", pkgarch, pkgvers);

	/*
	 *  If this script was invoked by 'pkgask', just
	 *  execute request script and quit (do_pkgask()).
	 */
	if (iflag)
		do_pkgask();

	/*
	 * OK, now we're serious. Verify existence of the contents file then
	 * initialize and lock the state file.
	 */
	if (!vcfile())
		quit(99);

	if (!in_dryrun_mode()) {
		if (!lockinst(get_prog_name(), srcinst))
			quit(99);
	}

	/* Now do all the various setups based on ABI compliance */

	/* Read the environment (from pkginfo or '-o') ... */
	abi_comp_ptr = getenv("NONABI_SCRIPTS");

	/* Read the environment (from pkginfo or '-y') ... */
	abi_sym_ptr = getenv("PKG_NONABI_SYMLINKS");

	/* bug id 4244631, not ABI compliant */
	if (abi_comp_ptr && strncasecmp(abi_comp_ptr, "TRUE", 4) == 0) {
		script_in = PROC_XSTDIN;
		non_abi_scripts = 1;
	}

	/*
	 * Until on1095, set it from exception package names as
	 * well.
	 */
	else if (exception_pkg(srcinst, SCRIPT)) {
		putparam("NONABI_SCRIPTS", "TRUE");
		script_in = PROC_XSTDIN;
		non_abi_scripts = 1;
	}

	/* Set symlinks to be processed the old way */
	if (abi_sym_ptr && strncasecmp(abi_sym_ptr, "TRUE", 4) == 0) {
		set_nonABI_symlinks();
	}

	/* Until 2.9, set it from the execption list */
	else if (exception_pkg(srcinst, LINK)) {
		putparam("PKG_NONABI_SYMLINKS", "TRUE");
		set_nonABI_symlinks();
	}

	/*
	 * At this point, script_in, non_abi_scripts & the environment are
	 * all set correctly for the ABI status of the package.
	 */
	if (pt = getenv("MAXINST"))
		maxinst = atol(pt);

	/*
	 *  verify that we are not trying to install an
	 *  INTONLY package with no interaction
	 */
	if (pt = getenv("INTONLY")) {
		if (iflag || nointeract) {
			progerr(gettext(ERR_INTONLY), pkgabrv);
			quit(1);
		}
	}

	if (!silent && !pkgdev.cdevice)
		copyright();

	if (getuid()) {
		progerr(gettext(ERR_NOTROOT), get_prog_name());
		quit(1);
	}

	/*
	 * inspect the system to determine if any instances of the
	 * package being installed already exist on the system
	 */
	npkgs = 0;
	prvinfo = (struct pkginfo *) calloc(MALSIZ, sizeof (struct pkginfo));
	if (prvinfo == NULL) {
		progerr(gettext(ERR_MEMORY), errno);
		quit(99);
	}

	for (;;) {
		if (pkginfo(&prvinfo[npkgs], pkgwild, NULL, NULL)) {
			if ((errno == ESRCH) || (errno == ENOENT))
				break;
			progerr(gettext(ERR_SYSINFO), errno);
			quit(99);
		}
		if ((++npkgs % MALSIZ) == 0) {
			prvinfo = (struct pkginfo *) realloc(prvinfo,
				(npkgs+MALSIZ) * sizeof (struct pkginfo));
			if (prvinfo == NULL) {
				progerr(gettext(ERR_MEMORY), errno);
				quit(99);
			}
		}
	}

	pkginst = getinst(prvinfo, npkgs);

	if (respfile)
		set_respfile(respfile, pkginst, RESP_RO);

	(void) sprintf(pkgloc, "%s/%s", get_PKGLOC(), pkginst);
	(void) sprintf(pkgbin, "%s/install", pkgloc);
	(void) sprintf(pkgsav, "%s/save", pkgloc);
	(void) sprintf(ilockfile, "%s/!I-Lock!", pkgloc);
	(void) sprintf(rlockfile, "%s/!R-Lock!", pkgloc);
	(void) sprintf(savlog, "%s/logs/%s", get_PKGADM(), pkginst);

	putparam("PKGINST", pkginst);
	putparam("PKGSAV", pkgsav);

	/*
	 * Be sure request script has access to PKG_INSTALL_ROOT if there is
	 * one
	 */
	put_path_params();
	if (!map_client)
		putparam("PKG_NO_UNIFIED", "TRUE");

	/*
	 * This maps the client filesystems into the server's space.
	 */
	if (map_client && !mount_client())
		logerr(gettext(MSG_MANMOUNT));

	/*
	 * If this is an UPDATE then either this is exactly the same version
	 * and architecture of an installed package or a different package is
	 * intended to entirely replace an installed package of the same name
	 * with a different VERSION or ARCH string.
	 */
	if (update) {
		/*
		 * If this version and architecture is already installed,
		 * merge the installed and installing parameters and inform
		 * all procedure scripts by defining UPDATE in the
		 * environment.
		 */
		if (is_samepkg()) {
			/*
			 * If it's the same ARCH and VERSION, then a merge
			 * and copy operation is necessary.
			 */
			if (n = merg_pkginfos(pclass))
				quit(n);

			if (n = cp_pkgdirs())
				quit(n);
		} else {
			/*
			 * If it's a different ARCH and/or VERSION then this
			 * is an "instance=overwrite" situation. The
			 * installed base needs to be confirmed and the
			 * package directories renamed.
			 */
			if (n = ck_instbase())
				quit(n);

			if (n = mv_pkgdirs())
				quit(n);
		}

		putparam("UPDATE", "yes");

	}

	if (in_dryrun_mode()) {
		set_dryrun_dir_loc();
	}

	/*
	 * Determine if the package has been partially installed on or
	 * removed from this system.
	 */
	ck_w_dryrun(ckpartial, PARTIAL);

	/*
	 *  make sure current runlevel is appropriate
	 */
	ck_w_dryrun(ckrunlevel, RUNLEVEL);

	if (pkgdev.cdevice) {
		/* get first volume which contains info files */
		unpack();
		if (!silent)
			copyright();
	}

	lockupd("request");

	/*
	 * If no response file has been provided, initialize response file by
	 * executing any request script provided by this package.
	 */
	if (!rdonly_respfile()) {
		(void) sprintf(path, "%s/install/request", instdir);
		n = reqexec(path);
		if (in_dryrun_mode())
			set_dr_info(REQUESTEXITCODE, n);

		ckreturn(n, gettext(ERR_REQUEST));
	}

	/* BEGIN ANALYSIS PHASE */
	lockupd("checkinstall");

	/* Execute checkinstall script if one is provided. */
	(void) sprintf(script, "%s/install/checkinstall", instdir);
	if (access(script, 0) == 0) {
		echo(gettext("## Executing checkinstall script."));
		n = chkexec(script);
		if (in_dryrun_mode())
			set_dr_info(CHECKEXITCODE, n);

		if (n == 3) {
			echo(gettext(WRN_CHKINSTALL));
			ckreturn(4, NULL);
		} else
			ckreturn(n, gettext(ERR_CHKINSTALL));
	}

	/*
	 * Now that the internal data structures are initialized, we can
	 * initialize the dryrun files (which may be the same files).
	 */
	if (pkgdrtarg)
		init_dryrunfile(pkgdrtarg);

	/*
	 * Look for all parameters in response file which begin with a
	 * capital letter, and place them in the environment.
	 */
	if (is_a_respfile())
		if (n = merg_respfile())
			quit(n);

	lockupd(gettext("analysis"));

	/*
	 * Determine package base directory and client base directory
	 * if appropriate. Then encapsulate them for future retrieval.
	 */
	if ((err = set_basedirs(isreloc(instdir), adm.basedir, pkginst,
	    nointeract)) != 0)
		quit(err);

	if (is_a_basedir()) {
		mkbasedir(!nointeract, get_basedir());
		echo(gettext(MSG_BASE_USED), get_basedir());
	}

	/*
	 * Store PKG_INSTALL_ROOT, BASEDIR & CLIENT_BASEDIR in our
	 * environment for later use by procedure scripts.
	 */
	put_path_params();

	/*
	 * the following two checks are done in the corresponding
	 * ck() routine, but are repeated here to avoid re-processing
	 * the database if we are administered to not include these
	 * processes
	 */
	if (ADM(setuid, "nochange"))
		nosetuid++;	/* Clear setuid/gid bits. */
	if (ADM(conflict, "nochange"))
		nocnflct++;	/* Don't install conflicting files. */

	/*
	 * Get the filesystem space information for the filesystem on which
	 * the "contents" file resides.
	 */
	if (!access(get_PKGADM(), F_OK)) {
		if (statvfs(get_PKGADM(), &svfsb) == -1) {
			progerr(gettext("statvfs(%s) failed"), get_PKGADM());
			logerr("(errno %d)", errno);
			quit(99);
		}
	} else {
		svfsb.f_bsize = 8192;
		svfsb.f_frsize = 1024;
	}

	/*
	 * Get the number of blocks used by the pkgmap, ocfile()
	 * needs this to properly determine its space requirements.
	 */
	if (stat(p_pkgmap, &statb) == -1) {
		progerr(gettext("unable to get space usage of <%s>"),
		    p_pkgmap);
		quit(99);
	}

	pkgmap_blks = nblk(statb.st_size, svfsb.f_bsize, svfsb.f_frsize);

	/*
	 * Merge information in memory with the "contents" file; this creates
	 * a temporary version of the "contents" file. Note that in dryrun
	 * mode, we still need to record the contents file data somewhere,
	 * but we do it in the dryrun directory.
	 */
	if (in_dryrun_mode()) {
		if (n = set_cfdir(pkgdrtarg))
			quit(n);
	} else {
		if (n = set_cfdir(NULL))
			quit(n);
	}

	if (!ocfile(&mapfp, &tmpfp, pkgmap_blks))
		quit(99);

	/*
	 * if cpio is being used,  tell pkgdbmerg since attributes will
	 * have to be check and repaired on all file and directories
	 */
	for (np = cpio_names; *np != NULL; np++) {
		(void) sprintf(path, "%s/%s", instdir, *np);
		if (iscpio(path, &is_comp_arch)) {
			is_WOS_arch();
			break;
		}
	}

	/* Establish the class list and the class attributes. */
	cl_sets(getenv("CLASSES"));
	find_CAS(I_ONLY, pkgbin, instdir);

	if ((fp = fopen(p_pkgmap, "r")) == NULL) {
		progerr(gettext(ERR_PKGMAP), p_pkgmap);
		quit(99);
	}

	/*
	 * This modifies the path list entries in memory to reflect
	 * how they should look after the merg is complete
	 */
	nparts = sortmap(&extlist, fp, mapfp, tmpfp);

	if ((n = files_installed()) > 0) {
		if (n > 1)
			echo(gettext(MSG_INST_MANY), n);
		else
			echo(gettext(MSG_INST_ONE), n);
	}

	/*
	 * Check ulimit requirement (provided in pkginfo). The purpose of
	 * this limit is to terminate pathological file growth resulting from
	 * file edits in scripts. It does not apply to files in the pkgmap
	 * and it does not apply to any database files manipulated by the
	 * installation service.
	 */
	if (pt = getenv("ULIMIT")) {
		if (assign_ulimit(pt) == -1) {
			progerr(gettext(ERR_BADULIMIT), pt);
			quit(99);
		}
	}

	/*
	 *  verify package information files are not corrupt
	 */
	ck_w_dryrun(ckpkgfiles, PKGFILES);

	/*
	 *  verify package dependencies
	 */
	ck_w_dryrun(ckdepend, DEPEND);

	/*
	 *  Check space requirements.
	 */
	ck_w_dryrun(ckspace, SPACE);

	/*
	 * Determine if any objects provided by this package conflict with
	 * the files of previously installed packages.
	 */
	ck_w_dryrun(ckconflct, CONFLICT);

	/*
	 * Determine if any objects provided by this package will be
	 * installed with setuid or setgid enabled.
	 */
	ck_w_dryrun(cksetuid, SETUID);

	/*
	 * Determine if any packaging scripts provided with this package will
	 * execute as a priviledged user.
	 */
	ck_w_dryrun(ckpriv, PRIV);

	/*
	 *  Verify neccessary package installation directories exist.
	 */
	ck_w_dryrun(ckpkgdirs, PKGDIRS);

	/*
	 * If we have assumed that we were installing setuid or conflicting
	 * files, and the user chose to do otherwise, we need to read in the
	 * package map again and re-merg with the "contents" file
	 */
	if (rprcflag)
		nparts = sortmap(&extlist, fp, mapfp, tmpfp);

	(void) fclose(fp);

	/* BEGIN INSTALLATION PHASE */
	if (in_dryrun_mode())
		echo(gettext("\nDryrunning install of %s as <%s>\n"),
		    pkgname, pkginst);
	else
		echo(gettext("\nInstalling %s as <%s>\n"), pkgname, pkginst);
	started++;

	/*
	 * This replaces the contents file with recently created temp version
	 * which contains information about the objects being installed.
	 * Under old lock protocol it closes both files and releases the
	 * locks. Beginning in Solaris 2.7, this lock method should be
	 * reviewed.
	 */
	if ((n = (swapcfile(mapfp, tmpfp, (dbchg ? pkginst : NULL)))) ==
	    RESULT_WRN)
		warnflag++;
	else if (n == RESULT_ERR)
		quit(99);

	/*
	 * Create install-specific lockfile to indicate start of
	 * installation. This is really just an information file. If the
	 * process dies, the initial lockfile (from lockinst(), is
	 * relinquished by the kernel, but this one remains in support of the
	 * post-mortem.
	 */
	if (creat(ilockfile, 0644) < 0) {
		progerr(gettext(ERR_LOCKFILE), ilockfile);
		quit(99);
	}

	(void) time(&clock);
	(void) cftime(cbuf, "%b %d \045Y \045H:\045M", &clock);
	putparam("INSTDATE", qstrdup(cbuf));

	/*
	 *  Store information about package being installed;
	 *  modify installation parameters as neccessary and
	 *  copy contents of 'install' directory into $pkgloc
	 */
	merginfo(pclass);

	/* If this was just a dryrun, then quit() will write out that file. */
	if (in_dryrun_mode()) {
		quit(0);
	}

	if (opresvr4) {
		/*
		 * we are overwriting a pre-svr4 package, so remove the file
		 * in /usr/options now
		 */
		(void) sprintf(path, "%s/%s.name", get_PKGOLD(), pkginst);
		if (unlink(path) && (errno != ENOENT)) {
			progerr(gettext(ERR_OPRESVR4), path);
			warnflag++;
		}
	}

	lockupd("preinstall");

	/*
	 * Execute preinstall script, if one was provided with the
	 * package. We check the package to avoid running an old
	 * preinstall script if one was provided with a prior instance.
	 */
	(void) sprintf(script, "%s/install/preinstall", instdir);
	if (access(script, 0) == 0) {
		/* execute script residing in pkgbin instead of media */
		(void) sprintf(script, "%s/preinstall", pkgbin);
		if (access(script, 0) == 0) {
			set_ulimit("preinstall", gettext(ERR_PREINSTALL));
			echo(gettext("## Executing preinstall script."));
			if (pkgverbose)
				ckreturn(pkgexecl(script_in, PROC_STDOUT,
				    PROC_USER, PROC_GRP, SHELL, "-x", script,
				    NULL), gettext(ERR_PREINSTALL));
			else
				ckreturn(pkgexecl(script_in, PROC_STDOUT,
				    PROC_USER, PROC_GRP, SHELL, script,
				    NULL), gettext(ERR_PREINSTALL));

			clr_ulimit();
			unlink(script);	/* no longer needed. */
		}
	}

	/*
	 * Check delivered package for a postinstall script while
	 * we're still on volume 1.
	 */
	(void) sprintf(script, "%s/install/postinstall", instdir);
	if (access(script, 0) == 0)
		(void) sprintf(script, "%s/postinstall", pkgbin);
	else
		script[0] = '\000';

	lockupd(gettext("install"));

	/*
	 *  install package one part (volume) at a time
	 */
	part = 1;
	while (part <= nparts) {
		if ((part > 1) && pkgdev.cdevice)
			unpack();
		instvol(extlist, srcinst, part, nparts);

		if (part++ >= nparts)
			break;
	}

	/*
	 * Now that all install class action scripts have been used, we
	 * delete them from the package directory.
	 */
	rm_icas(pkgbin);

	lockupd("postinstall");

	/*
	 * Execute postinstall script, if any
	 */
	if (*script && access(script, 0) == 0) {
		set_ulimit("postinstall", gettext(ERR_POSTINSTALL));
		echo(gettext("## Executing postinstall script."));
		if (pkgverbose)
			ckreturn(pkgexecl(script_in, PROC_STDOUT, PROC_USER,
			    PROC_GRP, SHELL, "-x", script, NULL),
			    gettext(ERR_POSTINSTALL));
		else
			ckreturn(pkgexecl(script_in, PROC_STDOUT, PROC_USER,
			    PROC_GRP, SHELL, script, NULL),
			    gettext(ERR_POSTINSTALL));

		clr_ulimit();
		unlink(script);	/* no longer needed */
	}

	if (!warnflag && !failflag) {
		if (pt = getenv("PREDEPEND"))
			predepend(pt);
		(void) unlink(rlockfile);
		(void) unlink(ilockfile);
		(void) unlink(savlog);
	}

	(void) unlockinst();	/* release generic lock */

	quit(0);
	/*NOTREACHED*/
#ifdef lint
	return (0);
#endif	/* lint */
}

/*
 * This function merges the environment data in the response file with the
 * current environment.
 */
static int
merg_respfile()
{
	int retcode = 0;
	char *resppath = get_respfile();
	char *locbasedir;
	char param[64], *value;
	FILE *fp;

	if ((fp = fopen(resppath, "r")) == NULL) {
		progerr(gettext(ERR_RESPONSE), resppath);
		return (99);
	}

	param[0] = '\0';

	while (value = fpkgparam(fp, param)) {
		if (!isupper(param[0])) {
			param[0] = '\0';
			continue;
		}

		if (rdonly(param)) {
			progerr(gettext(ERR_RDONLY), param);
			param[0] = '\0';
			continue;
		}

		/*
		 * If this is an update, and the response file
		 * specifies the BASEDIR, make sure it matches the
		 * existing installation base. If it doesn't, we have
		 * to quit.
		 */
		if (update && strcmp("BASEDIR", param) == 0) {
			locbasedir = getenv("BASEDIR");
			if (locbasedir && strcmp(value, locbasedir) != 0) {
				char *dotptr;
				/* Get srcinst down to a name. */
				if (dotptr = strchr(srcinst, '.'))
					*dotptr = '\000';
				progerr(gettext(ERR_NEWBD), srcinst,
				    locbasedir, value);
				retcode = 99;
			}
		}

		putparam(param, value);
		param[0] = '\0';
	}
	(void) fclose(fp);

	return (retcode);
}

/*
 * This scans the installed pkginfo file for the current BASEDIR. If this
 * BASEDIR is different from the current BASEDIR, there will definitely be
 * problems.
 */
static int
ck_instbase(void)
{
	int retcode = 0;
	char param[64], *value;
	char pkginfo_path[PATH_MAX];
	FILE *fp;

	/* Open the old pkginfo file. */
	(void) sprintf(pkginfo_path, "%s/%s", pkgloc, PKGINFO);
	if ((fp = fopen(pkginfo_path, "r")) == NULL) {
		progerr(gettext(ERR_PKGINFO), pkginfo_path);
		return (99);
	}

	param[0] = '\000';

	while (value = fpkgparam(fp, param)) {
		if (strcmp("BASEDIR", param) == 0) {
			if (adm.basedir && *(adm.basedir) &&
			    strchr("/$", *(adm.basedir))) {
				char *dotptr;

				/*
				 * Get srcinst down to a name.
				 */
				if (dotptr = strchr(srcinst, '.'))
					*dotptr = '\000';
				if (strcmp(value,
				    adm.basedir) != 0) {
					progerr(gettext(ERR_ADMBD), srcinst,
					    value, adm.basedir);
					retcode = 4;
					break;
				}
			} else if (ADM(basedir, "ask"))
				/*
				 * If it's going to ask later, let it know
				 * that it *must* agree with the BASEDIR we
				 * just picked up.
				 */
				adm.basedir = "update";

			putparam(param, value);
			break;
		}

		param[0] = '\0';
	}
	fclose(fp);

	return (retcode);
}

/*
 * Since this is an overwrite of a different version of the package, none of
 * the old files should remain, so we rename them.
 */
static int
mv_pkgdirs(void)
{
	/*
	 * If we're not in dryrun mode and we can find an old set of package
	 * files over which the new ones will be written, do the rename.
	 */
	if (!in_dryrun_mode() && pkgloc[0] && !access(pkgloc, F_OK)) {
		(void) sprintf(pkgloc_sav, "%s/.save.%s", get_PKGLOC(),
		    pkginst);
		if (pkgloc_sav[0] && !access(pkgloc_sav, F_OK))
			(void) rrmdir(pkgloc_sav);

		if (rename(pkgloc, pkgloc_sav) == -1) {
			progerr(gettext(ERR_PKGBINREN), pkgloc, pkgloc_sav);
			return (99);
		}
	}

	return (0);
}

/*
 * This function scans the installed pkginfo and merges that environment
 * with the installing environment according to the following rules.
 *
 *	1. CLASSES is a union of the installed and installing CLASSES lists.
 *	2. The installed BASEDIR takes precedence. If it doesn't agree with
 *	   an administratively imposed BASEDIR, an ERROR is issued.
 *	3. All other installing parameters are preserved.
 *	4. All installed parameters are added iff they do not overwrite an
 *	   existing installing parameter.
 *
 * This returns 0 for all OK or an error code if a fatal error occurred.
 */
static int
merg_pkginfos(struct cl_attr **pclass)
{
	int retcode = 0;
	char param[64], *value;
	char pkginfo_path[PATH_MAX];
	FILE *fp;

	/* Open the old pkginfo file. */
	(void) sprintf(pkginfo_path, "%s/%s", pkgloc, PKGINFO);
	if ((fp = fopen(pkginfo_path, "r")) == NULL) {
		progerr(gettext(ERR_PKGINFO), pkginfo_path);
		return (99);
	}

	param[0] = '\000';

	while (value = fpkgparam(fp, param)) {
		if (strcmp(param, "CLASSES") == 0)
			(void) setlist(&pclass, qstrdup(value));
		else if (strcmp("BASEDIR", param) == 0) {
			if (adm.basedir && *(adm.basedir) &&
			    strchr("/$", *(adm.basedir))) {
				char *dotptr;
				/*
				 * Get srcinst down to a
				 * name.
				 */
				if (dotptr = strchr(srcinst, '.'))
					*dotptr = '\000';
				if (strcmp(value, adm.basedir) != 0) {
					progerr(gettext(ERR_ADMBD), srcinst,
					    value, adm.basedir);
					retcode = 4;
					break;
				}
			} else if (ADM(basedir, "ask"))
				/*
				 * If it's going to ask
				 * later, let it know that it
				 * *must* agree with the
				 * BASEDIR we just picked up.
				 */
				adm.basedir = "update";

			putparam(param, value);
		} else if (getenv(param) == NULL)
			putparam(param, value);

		param[0] = '\0';
	}

	fclose(fp);

	return (retcode);
}

static void
set_dryrun_dir_loc(void)
{
	/* Set pkg location to the dryrun directory */
	set_PKGLOC(pkgdrtarg);
	(void) sprintf(pkgloc, "%s/%s", get_PKGLOC(), pkginst);
	(void) sprintf(pkgbin, "%s/install", pkgloc);
	(void) sprintf(pkgsav, "%s/save", pkgloc);
	(void) sprintf(ilockfile, "%s/!I-Lock!", pkgloc);
	(void) sprintf(rlockfile, "%s/!R-Lock!", pkgloc);
	(void) sprintf(savlog, "%s/logs/%s", get_PKGADM(), pkginst);
}

/*
 * If we are updating a pkg, then we need to copy the "old" pkgloc so that
 * any scripts that got removed in the new version aren't left around.  So we
 * copy it here to .save.pkgloc, then in quit() we can restore our state, or
 * remove it.
 */
static int
cp_pkgdirs(void)
{
	if (in_dryrun_mode()) {
		set_dryrun_dir_loc();
	}

	/*
	 * If we're not in dryrun mode and we can find an old set of package
	 * files over which the new ones will be written, do the copy.
	 */
	if (!in_dryrun_mode() && pkgloc[0] && !access(pkgloc, F_OK)) {
		char cmd[sizeof ("/usr/bin/cp -r  ") + PATH_MAX*2 + 1];
		int status;

		(void) sprintf(pkgloc_sav, "%s/.save.%s",
		    get_PKGLOC(), pkginst);
		/*
		 * Even though it takes a while, we use a recursive copy here
		 * because if the current pkgadd fails for any reason, we
		 * don't want to lose this data.
		 */
		sprintf(cmd, "/usr/bin/cp -r %s %s", pkgloc,
		    pkgloc_sav);
		status = system(cmd);
		if (status == -1 || WEXITSTATUS(status)) {
			progerr(gettext(ERR_PKGBINCP), pkgloc, pkgloc_sav);
			return (99);
		}
	}

	return (0);
}

/*
 * This implements the pkgask function. It just executes the request script
 * and stores the results in a response file.
 */
static void
do_pkgask()
{
	if (pkgdev.cdevice) {
		unpack();
		if (!silent)
			copyright();
	}
	(void) sprintf(path, "%s/install/request", instdir);
	if (access(path, 0)) {
		progerr(gettext(ERR_NOREQUEST));
		quit(1);
	}

	set_respfile(respfile, srcinst, RESP_WR);

	if (is_a_respfile())
		ckreturn(reqexec(path), gettext(ERR_REQUEST));
	else
		failflag++;

	if (warnflag || failflag) {
		(void) unlink(respfile);
		echo(gettext("\nResponse file <%s> was not created."),
		    get_respfile());
	} else
		echo(gettext("\nResponse file <%s> was created."),
		    get_respfile());
	quit(0);
}

/*
 * This function runs a check utility and acts appropriately based upon the
 * return code. It deals appropriately with the dryrun file if it is present.
 */
static void
ck_w_dryrun(int (*func)(), int type)
{
	int n;

	n = func();
	if (in_dryrun_mode())
		set_dr_info(type, !n);

	if (n)
		quit(n);

}

/*
 * This function deletes all install class action scripts from the package
 * directory on the root filesystem.
 */
static void
rm_icas(char *cas_dir)
{
	DIR	*pdirfp;
	struct	dirent *dp;
	char path[PATH_MAX];

	if ((pdirfp = opendir(cas_dir)) == NULL)
		return;

	while ((dp = readdir(pdirfp)) != NULL) {
		if (dp->d_name[0] == '.')
			continue;

		if (dp->d_name[0] == 'i' && dp->d_name[1] == '.') {
			(void) sprintf(path, "%s/%s", cas_dir, dp->d_name);
			unlink(path);
		}
	}
	(void) closedir(pdirfp);
}

void
ckreturn(int retcode, char *msg)
{
	switch (retcode) {
	    case 2:
	    case 12:
	    case 22:
		warnflag++;
		if (msg)
			progerr(msg);
		/*FALLTHRU*/
	    case 10:
	    case 20:
		if (retcode >= 10)
			dreboot++;
		if (retcode >= 20)
			ireboot++;
		/*FALLTHRU*/
	    case 0:
		break; /* okay */

	    case -1:
		retcode = 99;
		/*FALLTHRU*/
	    case 99:
	    case 1:
	    case 11:
	    case 21:
	    case 4:
	    case 14:
	    case 24:
	    case 5:
	    case 15:
	    case 25:
		if (msg)
			progerr(msg);
		/*FALLTHRU*/
	    case 3:
	    case 13:
	    case 23:
		quit(retcode);
		/* NOT REACHED */
	    default:
		if (msg)
			progerr(msg);
		quit(1);
	}
}

/*
 * This function verifies that the contents file is in place. If it is - no
 * change. If it isn't - this creates it.
 */
static int
vcfile(void)
{
	int	fd;
	char 	contents[PATH_MAX];

	(void) sprintf(contents, "%s/contents", get_PKGADM());
	if ((fd = open(contents, O_WRONLY | O_CREAT | O_EXCL, 0644)) < 0) {
		if (errno == EEXIST) {
			return (1);	/* Contents file is already there. */
		} else {	/* Can't make it. */
			progerr(gettext(ERR_CREAT_CONT), contents);
			logerr("(errno %d)", errno);
			return (0);
		}
	} else {	/* Contents file wasn't there, but is now. */
		echo(gettext("## Software contents file initialized"));
		(void) close(fd);
		return (1);
	}
}

#define	COPYRIGHT "install/copyright"

static void
copyright(void)
{
	FILE	*fp;
	char	line[LSIZE];
	char	path[PATH_MAX];

	/* Compose full path for copyright file */
	(void) sprintf(path, "%s/%s", instdir, COPYRIGHT);

	if ((fp = fopen(path, "r")) == NULL) {
		if (getenv("VENDOR") != NULL)
			echo(getenv("VENDOR"));
	} else {
		while (fgets(line, LSIZE, fp))
			(void) fprintf(stdout, "%s", line); /* bug #1083713 */
		(void) fclose(fp);
	}
}


static int
rdonly(char *p)
{
	int	i;

	for (i = 0; ro_params[i]; i++) {
		if (strcmp(p, ro_params[i]) == 0)
			return (1);
	}
	return (0);
}

static void
unpack(void)
{
	/*
	 * read in next part from stream, even if we decide
	 * later that we don't need it
	 */
	if (dparts < 1) {
		progerr(gettext(ERR_DSTREAMCNT));
		quit(99);
	}
	if ((access(instdir, 0) == 0) && rrmdir(instdir)) {
		progerr(gettext(ERR_RMDIR), instdir);
		quit(99);
	}
	if (mkdir(instdir, 0755)) {
		progerr(gettext(ERR_MKDIR), instdir);
		quit(99);
	}
	if (chdir(instdir)) {
		progerr(gettext(ERR_CHDIR), instdir);
		quit(99);
	}
	dparts--;
	if (ds_next(pkgdev.cdevice, instdir)) {
		progerr(gettext(ERR_DSTREAM));
		quit(99);
	}
	if (chdir(get_PKGADM())) {
		progerr(gettext(ERR_CHDIR), get_PKGADM());
		quit(99);
	}
}

static void
usage(void)
{
	(void) fprintf(stderr, gettext(ERR_USAGE));
	exit(1);
}
