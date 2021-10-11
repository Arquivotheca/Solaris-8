/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)mount.c	1.86	99/09/23 SMI"

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *	PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *	Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *  (c) 1986,1987,1988,1989,1996,1997-1999  Sun Microsystems, Inc
 *  (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 *
 */
/*
 * nfs mount
 */

#define	NFSCLIENT
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <varargs.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/param.h>
#include <rpc/rpc.h>
#include <errno.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/mount.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <nfs/nfs.h>
#include <nfs/mount.h>
#include <rpcsvc/mount.h>
#include <sys/pathconf.h>
#include <netdir.h>
#include <netconfig.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <syslog.h>
#include <fslib.h>
#include "replica.h"
#include <netinet/in.h>
#include <nfs/nfs_sec.h>
#include "nfs_subr.h"
#include "webnfs.h"

#ifndef	NFS_VERSMAX
#define	NFS_VERSMAX	3
#endif
#ifndef	NFS_VERSMIN
#define	NFS_VERSMIN	2
#endif

#define	RET_OK		0
#define	RET_RETRY	32
#define	RET_ERR		33
#define	RET_MNTERR	1000

/* number of transports to try */
#define	MNT_PREF_LISTLEN	2
#define	FIRST_TRY		1
#define	SECOND_TRY		2

#define	BIGRETRY	10000

/* maximum length of RPC header for NFS messages */
#define	NFS_RPC_HDR	432

#define	NFS_ARGS_EXTB_secdata(args, secdata) \
	{ (args)->nfs_args_ext = NFS_ARGS_EXTB, \
	(args)->nfs_ext_u.nfs_extB.secdata = secdata; }

extern int __clnt_bindresvport();
extern char *nfs_get_qop_name();
extern AUTH * nfs_create_ah();
extern enum snego_stat nfs_sec_nego();

static void usage(void);
static int retry(struct mnttab *, int);
static int set_args(int *, struct nfs_args *, char *, struct mnttab *);
static int get_fh_via_pub(struct nfs_args *, char *, char *, bool_t, bool_t,
	int *, struct netconfig **, ushort_t);
static int get_fh(struct nfs_args *, char *, char *, int *, bool_t);
static int make_secure(struct nfs_args *, char *, struct netconfig *, bool_t);
static int mount_nfs(struct mnttab *, int);
static int getaddr_nfs(struct nfs_args *, char *, struct netconfig **,
	bool_t, char *, ushort_t);
#ifdef __STDC__
static void pr_err(const char *fmt, ...);
#else
static void pr_err(char *fmt, va_dcl);
#endif
static void usage(void);
static struct netbuf *get_addr(char *, rpcprog_t, rpcvers_t,
	struct netconfig **, char *, ushort_t, struct t_info *,
	caddr_t *, bool_t get_pubfh, char *fspath);

static struct netbuf *get_the_addr(char *, rpcprog_t, rpcvers_t,
	struct netconfig *, ushort_t, struct t_info *, caddr_t *,
	bool_t, char *);

extern int self_check(char *);

static char typename[64];

static int bg;
static int posix = 0;
static int retries = BIGRETRY;
static ushort_t nfs_port = 0;
static char *nfs_proto = NULL;

static int mflg = 0;
static int Oflg = 0;	/* Overlay mounts */
static int qflg = 0;	/* quiet - don't print warnings on bad options */

static char *fstype = MNTTYPE_NFS;

static seconfig_t nfs_sec;
static int sec_opt = 0;	/* any security option ? */
static bool_t snego_done;

/*
 * These two variables control the NFS version number to be used.
 *
 * nfsvers defaults to 0 which means to use the highest number that
 * both the client and the server support.  It can also be set to
 * a particular value, either 2 or 3, to indicate the version
 * number of choice.  If the server (or the client) do not support
 * the version indicated, then the mount attempt will be failed.
 *
 * nfsvers_to_use is the actual version number found to use.  It
 * is determined in get_fh by pinging the various versions of the
 * NFS service on the server to see which responds positively.
 */
static rpcvers_t nfsvers = 0;
static rpcvers_t nfsvers_to_use = 0;

/*
 * This variable controls whether to try the public file handle.
 */
static bool_t public_opt;

main(int argc, char **argv)
{
	struct mnttab mnt;
	extern char *optarg;
	extern int optind;
	char optbuf[MAX_MNTOPT_STR];
	int ro = 0;
	int r;
	int c;
	char *myname;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	myname = strrchr(argv[0], '/');
	myname = myname ? myname + 1 : argv[0];
	(void) sprintf(typename, "%s %s", MNTTYPE_NFS, myname);
	argv[0] = typename;

	mnt.mnt_mntopts = optbuf;
	(void) strcpy(optbuf, "rw");

	/*
	 * Set options
	 */
	while ((c = getopt(argc, argv, "ro:mOq")) != EOF) {
		switch (c) {
		case 'r':
			ro++;
			break;
		case 'o':
			(void) strcpy(mnt.mnt_mntopts, optarg);
#ifdef LATER					/* XXX */
			if (strstr(optarg, MNTOPT_REMOUNT)) {
				/*
				 * If remount is specified, only rw is allowed.
				 */
				if ((strcmp(optarg, MNTOPT_REMOUNT) != 0) &&
				    (strcmp(optarg, "remount,rw") != 0) &&
				    (strcmp(optarg, "rw,remount") != 0)) {
					pr_err(gettext("Invalid options\n"));
					exit(RET_ERR);
				}
			}
#endif /* LATER */				/* XXX */
			break;
		case 'm':
			mflg++;
			break;
		case 'O':
			Oflg++;
			break;
		case 'q':
			qflg++;
			break;
		default:
			usage();
			exit(RET_ERR);
		}
	}
	if (argc - optind != 2) {
		usage();
		exit(RET_ERR);
	}

	mnt.mnt_special = argv[optind];
	mnt.mnt_mountp = argv[optind+1];

	if (geteuid() != 0) {
		pr_err(gettext("not super user\n"));
		exit(RET_ERR);
	}

	r = mount_nfs(&mnt, ro);
	if (r == RET_RETRY && retries)
		r = retry(&mnt, ro);

	/*
	 * exit(r);
	 */
	return (r);
}

static void
#ifdef __STDC__
pr_err(const char *fmt, ...)
#else
pr_err(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
	va_list ap;

	va_start(ap);
	(void) fprintf(stderr, "%s: ", typename);
	(void) vfprintf(stderr, fmt, ap);
	(void) fflush(stderr);
	va_end(ap);
}

static void
usage()
{
	(void) fprintf(stderr,
	    gettext("Usage: nfs mount [-r] [-o opts] [server:]path dir\n"));
	exit(RET_ERR);
}

static int
mount_nfs(struct mnttab *mntp, int ro)
{
	struct nfs_args *args = NULL, *argp = NULL, *prev_argp = NULL;
	struct netconfig *nconf = NULL;
	struct replica *list = NULL;
	int mntflags = 0;
	int i, r, n;
	int oldvers = 0, vers = 0;
	int last_error = RET_OK;
	int replicated = 0;
	char *p;
	bool_t url;
	bool_t use_pubfh;
	char *special = NULL;
	char *oldpath = NULL;
	char *newpath = NULL;

	mntp->mnt_fstype = MNTTYPE_NFS;

	if (ro) {
		mntflags |= MS_RDONLY;
		/* convert "rw"->"ro" */
		if (p = strstr(mntp->mnt_mntopts, "rw")) {
			if (*(p+2) == ',' || *(p+2) == '\0')
				*(p+1) = 'o';
		}
	}

	if (Oflg)
		mntflags |= MS_OVERLAY;

	list = parse_replica(mntp->mnt_special, &n);
	if (list == NULL) {
		if (n < 0)
			pr_err(gettext("nfs file system; use [host:]path\n"));
		else
			pr_err(gettext("no memory\n"));
		last_error = RET_ERR;
		goto out;
	}

	replicated = (n > 1);

	/*
	 * There are some free() calls and the bottom of this loop, so be
	 * careful about adding continue statements.
	 */
	for (i = 0; i < n; i++) {
		char *path;
		char *host;
		ushort_t port;


		argp = (struct nfs_args *)malloc(sizeof (*argp));
		if (argp == NULL) {
			pr_err(gettext("no memory\n"));
			last_error = RET_ERR;
			goto out;
		}
		memset(argp, 0, sizeof (*argp));
		if (prev_argp == NULL)
			args = argp;
		else
			prev_argp->nfs_ext_u.nfs_extB.next = argp;
		prev_argp = argp;

		memset(&nfs_sec, 0, sizeof (nfs_sec));
		sec_opt = 0;
		use_pubfh = FALSE;
		url = FALSE;
		port = 0;
		snego_done = FALSE;

		/*
		 * Looking for resources of the form
		 *	nfs://server_host[:port_number]/path_name
		 */
		if (strcmp(list[i].host, "nfs") == 0 && strncmp(list[i].path,
		    "//", 2) == 0) {
			char *sport;
			url = TRUE;
			oldpath = strdup(list[i].path);
			if (oldpath == NULL) {
				pr_err(gettext("memory allocation failure\n"));
				last_error = RET_ERR;
				goto out;
			}
			host = list[i].path+2;
			path = strchr(host, '/');

			if (path == NULL) {
				pr_err(gettext(
				    "illegal nfs url syntax\n"));
				last_error = RET_ERR;
				goto out;
			}

			*path = '\0';
			sport = strchr(host, ':');

			if (sport != NULL && sport < path) {
				*sport = '\0';
				port = htons((ushort_t)atoi(sport+1));
			}

			path++;
			if (*path == '\0')
				path = ".";

		} else {
			host = list[i].host;
			path = list[i].path;
		}

		if (r = set_args(&mntflags, argp, host, mntp)) {
			last_error = r;
			goto out;
		}

		if (public_opt == TRUE)
			use_pubfh = TRUE;

		if (port == 0) {

			port = nfs_port;

		} else if (nfs_port != 0 && nfs_port != port) {

			pr_err(gettext(
			    "port (%u) in nfs URL not the same"
			    " as port (%u) in port option\n"),
			    (unsigned int)ntohs(port),
			    (unsigned int)ntohs(nfs_port));
			last_error = RET_ERR;
			goto out;
		}


		if (replicated && !(mntflags & MS_RDONLY)) {
			pr_err(gettext(
				"replicated mounts must be read-only\n"));
			last_error = RET_ERR;
			goto out;
		}

		if (replicated && (argp->flags & NFSMNT_SOFT)) {
			pr_err(gettext(
				"replicated mounts must not be soft\n"));
			last_error = RET_ERR;
			goto out;
		}

		oldvers = vers;
		nconf = NULL;

		r = RET_ERR;

		/*
		 * If -o public was specified, and/or a URL was specified,
		 * then try the public file handle method.
		 */
		if ((use_pubfh == TRUE) || (url == TRUE)) {
			r = get_fh_via_pub(argp, host, path, url, use_pubfh,
				&vers, &nconf, port);

			if (r != RET_OK) {
				/*
				 * If -o public was specified, then return the
				 * error now.
				 */
				if (use_pubfh == TRUE) {
					last_error = r;
					goto out;
				}
			} else
				use_pubfh = TRUE;
		}

		if (r != RET_OK) {
			bool_t loud_on_mnt_err;

			/*
			 * This can happen if -o public is not specified,
			 * special is a URL, and server doesn't support
			 * public file handle.
			 */
			if (url) {
				URLparse(path);
			}

			/*
			 * If the path portion of the URL didn't have
			 * a leading / then there is good possibility
			 * that a mount without a leading slash will
			 * fail.
			 */
			if (url == TRUE && *path != '/')
				loud_on_mnt_err = FALSE;
			else
				loud_on_mnt_err = TRUE;

			r = get_fh(argp, host, path, &vers, loud_on_mnt_err);

			if (r != RET_OK) {

				/*
				 * If there was no leading / and the path was
				 * derived from a URL, then try again
				 * with a leading /.
				 */
				if ((r == RET_MNTERR) &&
				    (loud_on_mnt_err == FALSE)) {

					newpath = malloc(strlen(path)+2);

					if (newpath == NULL) {
						pr_err(gettext("memory "
						    "allocation failure\n"));
						last_error = RET_ERR;
						goto out;
					}

					strcpy(newpath, "/");
					strcat(newpath, path);

					r = get_fh(argp, host, newpath, &vers,
						TRUE);

					if (r == RET_OK)
						path = newpath;
				}

				/*
				 * map exit code back to RET_ERR.
				 */
				if (r == RET_MNTERR)
					r = RET_ERR;

				if (r != RET_OK) {
					last_error = r;
					goto out;
				}
			}
		}

		if (oldvers && vers != oldvers) {
			pr_err(
			    gettext("replicas must have the same version\n"));
			last_error = RET_ERR;
			goto out;
		}

		/*
		 * decide whether to use remote host's
		 * lockd or do local locking
		 */
		if (!(argp->flags & NFSMNT_LLOCK) && vers == NFS_VERSION &&
			remote_lock(host, argp->fh)) {
			(void) printf(gettext(
			    "WARNING: No network locking on %s:%s:"),
			    host, path);
			(void) printf(gettext(
			    " contact admin to install server change\n"));
				argp->flags |= NFSMNT_LLOCK;
		}

		if (self_check(host))
			argp->flags |= NFSMNT_LOOPBACK;

		if (use_pubfh == FALSE) {
			nconf = NULL;
			if (r = getaddr_nfs(argp, host, &nconf, FALSE, path,
			    port)) {
				last_error = r;
				goto out;
			}
		}

		if (make_secure(argp, host, nconf, use_pubfh) < 0) {
			last_error = RET_ERR;
			goto out;
		}

		if ((url == TRUE) && (use_pubfh == FALSE)) {
			/*
			 * Convert the special from
			 *	nfs://host/path
			 * to
			 *	host:path
			 */
			if (convert_special(&special, host, oldpath, path,
			    mntp->mnt_special) == -1) {
				(void) printf(gettext(
				    "could not convert URL nfs:%s to %s:%s\n"),
				    oldpath, host, path);
				last_error = RET_ERR;
				goto out;
			} else {
				mntp->mnt_special = special;
			}
		}

		if (oldpath != NULL) {
			free(oldpath);
			oldpath = NULL;
		}

		if (newpath != NULL) {
			free(newpath);
			newpath = NULL;
		}
	}

	mntflags |= MS_DATA | MS_OPTIONSTR;
	if (mflg)
		mntflags |= MS_NOMNTTAB;
	if (mount(mntp->mnt_special, mntp->mnt_mountp, mntflags, fstype, args,
		sizeof (*args), mntp->mnt_mntopts, MAX_MNTOPT_STR) < 0) {
		pr_err(gettext("mount: %s: %s\n"),
		    mntp->mnt_mountp, strerror(errno));
		last_error = RET_ERR;
		goto out;
	}

out:

	if (special != NULL)
		free(special);
	if (oldpath != NULL)
		free(oldpath);
	if (newpath != NULL)
		free(newpath);

	if (list)
		free_replica(list, n);
	argp = args;
	while (argp != NULL) {
		if (argp->fh)
			free(argp->fh);
		if (argp->pathconf)
			free(argp->pathconf);
		if (argp->knconf)
			free(argp->knconf);
		if (argp->addr)
			free(argp->addr);
		nfs_free_secdata(argp->nfs_ext_u.nfs_extB.secdata);
		prev_argp = argp;
		argp = argp->nfs_ext_u.nfs_extB.next;
		free(prev_argp);
	}

	return (last_error);
}

/*
 * These options are duplicated in uts/common/fs/nfs/nfs_dlinet.c
 * Changes must be made to both lists.
 */
static char *optlist[] = {
#define	OPT_RO		0
	MNTOPT_RO,
#define	OPT_RW		1
	MNTOPT_RW,
#define	OPT_QUOTA	2
	MNTOPT_QUOTA,
#define	OPT_NOQUOTA	3
	MNTOPT_NOQUOTA,
#define	OPT_SOFT	4
	MNTOPT_SOFT,
#define	OPT_HARD	5
	MNTOPT_HARD,
#define	OPT_SUID	6
	MNTOPT_SUID,
#define	OPT_NOSUID	7
	MNTOPT_NOSUID,
#define	OPT_GRPID	8
	MNTOPT_GRPID,
#define	OPT_REMOUNT	9
	MNTOPT_REMOUNT,
#define	OPT_NOSUB	10
	MNTOPT_NOSUB,
#define	OPT_INTR	11
	MNTOPT_INTR,
#define	OPT_NOINTR	12
	MNTOPT_NOINTR,
#define	OPT_PORT	13
	MNTOPT_PORT,
#define	OPT_SECURE	14
	MNTOPT_SECURE,
#define	OPT_RSIZE	15
	MNTOPT_RSIZE,
#define	OPT_WSIZE	16
	MNTOPT_WSIZE,
#define	OPT_TIMEO	17
	MNTOPT_TIMEO,
#define	OPT_RETRANS	18
	MNTOPT_RETRANS,
#define	OPT_ACTIMEO	19
	MNTOPT_ACTIMEO,
#define	OPT_ACREGMIN	20
	MNTOPT_ACREGMIN,
#define	OPT_ACREGMAX	21
	MNTOPT_ACREGMAX,
#define	OPT_ACDIRMIN	22
	MNTOPT_ACDIRMIN,
#define	OPT_ACDIRMAX	23
	MNTOPT_ACDIRMAX,
#define	OPT_BG		24
	MNTOPT_BG,
#define	OPT_FG		25
	MNTOPT_FG,
#define	OPT_RETRY	26
	MNTOPT_RETRY,
#define	OPT_NOAC	27
	MNTOPT_NOAC,
#define	OPT_NOCTO	28
	MNTOPT_NOCTO,
#define	OPT_LLOCK	29
	MNTOPT_LLOCK,
#define	OPT_POSIX	30
	MNTOPT_POSIX,
#define	OPT_VERS	31
	MNTOPT_VERS,
#define	OPT_PROTO	32
	MNTOPT_PROTO,
#define	OPT_SEMISOFT	33
	MNTOPT_SEMISOFT,
#define	OPT_NOPRINT	34
	MNTOPT_NOPRINT,
#define	OPT_SEC		35
	MNTOPT_SEC,
#define	OPT_LARGEFILES	36
	MNTOPT_LARGEFILES,
#define	OPT_NOLARGEFILES 37
	MNTOPT_NOLARGEFILES,
#define	OPT_PUBLIC	38
	MNTOPT_PUBLIC,
#define	OPT_DIRECTIO	39
	MNTOPT_FORCEDIRECTIO,
#define	OPT_NODIRECTIO	40
	MNTOPT_NOFORCEDIRECTIO,
	NULL
};

#define	bad(val) (val == NULL || !isdigit(*val))

static int
set_args(int *mntflags, struct nfs_args *args, char *fshost, struct mnttab *mnt)
{
	char *saveopt, *optstr, *opts, *newopts, *val;
	int largefiles = 0;
	int invalid = 0;

	args->flags = NFSMNT_INT;	/* default is "intr" */
	args->flags |= NFSMNT_HOSTNAME;
	args->flags |= NFSMNT_NEWARGS;	/* using extented nfs_args structure */
	args->hostname = fshost;

	optstr = opts = strdup(mnt->mnt_mntopts);
	newopts = malloc(strlen(mnt->mnt_mntopts) + 1);
	if (opts == NULL || newopts == NULL) {
		pr_err(gettext("no memory"));
		if (opts)
			free(opts);
		if (newopts)
			free(newopts);
		return (RET_ERR);
	}
	newopts[0] = '\0';

	while (*opts) {
		invalid = 0;
		saveopt = opts;
		switch (getsubopt(&opts, optlist, &val)) {
		case OPT_RO:
			*mntflags |= MS_RDONLY;
			break;
		case OPT_RW:
			*mntflags &= ~(MS_RDONLY);
			break;
		case OPT_QUOTA:
		case OPT_NOQUOTA:
			break;
		case OPT_SOFT:
			args->flags |= NFSMNT_SOFT;
			args->flags &= ~(NFSMNT_SEMISOFT);
			break;
		case OPT_SEMISOFT:
			args->flags |= NFSMNT_SOFT;
			args->flags |= NFSMNT_SEMISOFT;
			break;
		case OPT_HARD:
			args->flags &= ~(NFSMNT_SOFT);
			args->flags &= ~(NFSMNT_SEMISOFT);
			break;
		case OPT_SUID:
			*mntflags &= ~(MS_NOSUID);
			break;
		case OPT_NOSUID:
			*mntflags |= MS_NOSUID;
			break;
		case OPT_GRPID:
			args->flags |= NFSMNT_GRPID;
			break;
		case OPT_REMOUNT:
			*mntflags |= MS_REMOUNT;
			break;
		case OPT_INTR:
			args->flags |= NFSMNT_INT;
			break;
		case OPT_NOINTR:
			args->flags &= ~(NFSMNT_INT);
			break;
		case OPT_NOAC:
			args->flags |= NFSMNT_NOAC;
			break;
		case OPT_PORT:
			if (bad(val))
				goto badopt;
			nfs_port = htons((ushort_t)atoi(val));
			break;

		case OPT_SECURE:
			if (nfs_getseconfig_byname("dh", &nfs_sec)) {
			    pr_err(gettext("can not get \"dh\" from %s\n"),
						NFSSEC_CONF);
			    goto badopt;
			}
			sec_opt++;
			break;

		case OPT_NOCTO:
			args->flags |= NFSMNT_NOCTO;
			break;

		case OPT_RSIZE:
			args->flags |= NFSMNT_RSIZE;
			if (bad(val))
				goto badopt;
			args->rsize = atoi(val);
			break;
		case OPT_WSIZE:
			args->flags |= NFSMNT_WSIZE;
			if (bad(val))
				goto badopt;
			args->wsize = atoi(val);
			break;
		case OPT_TIMEO:
			args->flags |= NFSMNT_TIMEO;
			if (bad(val))
				goto badopt;
			args->timeo = atoi(val);
			break;
		case OPT_RETRANS:
			args->flags |= NFSMNT_RETRANS;
			if (bad(val))
				goto badopt;
			args->retrans = atoi(val);
			break;
		case OPT_ACTIMEO:
			args->flags |= NFSMNT_ACDIRMAX;
			args->flags |= NFSMNT_ACREGMAX;
			args->flags |= NFSMNT_ACDIRMIN;
			args->flags |= NFSMNT_ACREGMIN;
			if (bad(val))
				goto badopt;
			args->acdirmin = args->acregmin = args->acdirmax
				= args->acregmax = atoi(val);
			break;
		case OPT_ACREGMIN:
			args->flags |= NFSMNT_ACREGMIN;
			if (bad(val))
				goto badopt;
			args->acregmin = atoi(val);
			break;
		case OPT_ACREGMAX:
			args->flags |= NFSMNT_ACREGMAX;
			if (bad(val))
				goto badopt;
			args->acregmax = atoi(val);
			break;
		case OPT_ACDIRMIN:
			args->flags |= NFSMNT_ACDIRMIN;
			if (bad(val))
				goto badopt;
			args->acdirmin = atoi(val);
			break;
		case OPT_ACDIRMAX:
			args->flags |= NFSMNT_ACDIRMAX;
			if (bad(val))
				goto badopt;
			args->acdirmax = atoi(val);
			break;
		case OPT_BG:
			bg++;
			break;
		case OPT_FG:
			bg = 0;
			break;
		case OPT_RETRY:
			if (bad(val))
				goto badopt;
			retries = atoi(val);
			break;
		case OPT_LLOCK:
			args->flags |= NFSMNT_LLOCK;
			break;
		case OPT_POSIX:
			posix = 1;
			break;
		case OPT_VERS:
			if (bad(val))
				goto badopt;
			nfsvers = (rpcvers_t)atoi(val);
			break;
		case OPT_PROTO:
			nfs_proto = (char *)malloc(strlen(val)+1);
			(void) strcpy(nfs_proto, val);
			break;
		case OPT_NOPRINT:
			args->flags |= NFSMNT_NOPRINT;
			break;
		case OPT_LARGEFILES:
			largefiles = 1;
			break;
		case OPT_NOLARGEFILES:
			pr_err(gettext("NFS can't support \"nolargefiles\"\n"));
			free(optstr);
			return (RET_ERR);

		case OPT_SEC:
			if (nfs_getseconfig_byname(val, &nfs_sec)) {
			    pr_err(gettext("can not get \"%s\" from %s\n"),
						val, NFSSEC_CONF);
			    return (RET_ERR);
			}
			sec_opt++;
			break;

		case OPT_PUBLIC:
			public_opt = TRUE;
			break;

		case OPT_DIRECTIO:
			args->flags |= NFSMNT_DIRECTIO;
			break;

		case OPT_NODIRECTIO:
			args->flags &= ~(NFSMNT_DIRECTIO);
			break;

		default:
			invalid = 1;
			if (!qflg)
				(void) fprintf(stderr, gettext(
				    "mount: %s on %s - WARNING unknown option"
				    " \"%s\"\n"), mnt->mnt_special,
				    mnt->mnt_mountp, saveopt);
			break;
		}
		if (!invalid) {
			if (newopts[0])
				strcat(newopts, ",");
			strcat(newopts, saveopt);
		}
	}
	strcpy(mnt->mnt_mntopts, newopts);
	free(newopts);
	free(optstr);

	/* ensure that only one secure mode is requested */
	if (sec_opt > 1) {
		pr_err(gettext("Security options conflict\n"));
		return (RET_ERR);
	}

	/* ensure that the user isn't trying to get large files over V2 */
	if (nfsvers == NFS_VERSION && largefiles) {
		pr_err(gettext("NFS V2 can't support \"largefiles\"\n"));
		return (RET_ERR);
	}

	return (RET_OK);

badopt:
	pr_err(gettext("invalid option: \"%s\"\n"), saveopt);
	free(optstr);
	return (RET_ERR);
}

static int
make_secure(struct nfs_args *args, char *hostname, struct netconfig *nconf,
	bool_t use_pubfh)
{
	sec_data_t *secdata;
	int flags;
	struct netbuf *syncaddr;

	/*
	 * check to see if any secure mode is requested.
	 * if not, use default security mode.
	 */
	if (!snego_done && !sec_opt) {
		/*
		 *  Get default security mode.
		 *  AUTH_UNIX has been the default choice for a long time.
		 *  The better NFS security service becomes, the better chance
		 *  we will set stronger security service as the default NFS
		 *  security mode.
		 *
		 */
	    if (nfs_getseconfig_default(&nfs_sec)) {
		pr_err(gettext("error getting default security entry\n"));
		return (-1);
	    }
	}

	/*
	 * Get the network address for the time service on
	 * the server.  If an RPC based time service is
	 * not available then try the IP time service.
	 */
	flags = (int)0;
	syncaddr = NULL;
	/*
	 * Get the network address for the time service on
	 * the server.  If an RPC based time service is
	 * not available then try the IP time service.
	 *
	 * Eventurally, we want to move this code to nfs_clnt_secdata()
	 * when autod_nfs.c and mount.c can share the same get_the_addr()
	 * routine.
	 */
	if (nfs_sec.sc_rpcnum == AUTH_DES) {
		/*
		 * If using the public fh, we likely will not be able
		 * to contact the remote RPCBINDer, since it is possibly
		 * behind a firewall.
		 */
		if (use_pubfh == FALSE) {
			syncaddr = get_the_addr(hostname, RPCBPROG, RPCBVERS,
			    nconf, 0, NULL, NULL, FALSE, NULL);
		}

		if (syncaddr != NULL) {
			flags |= AUTH_F_RPCTIMESYNC;
		} else {
			struct nd_hostserv hs;
			struct nd_addrlist *retaddrs;

			hs.h_host = hostname;
			hs.h_serv = "rpcbind";
			if (netdir_getbyname(nconf, &hs, &retaddrs) != ND_OK)
				goto err;
			syncaddr = retaddrs->n_addrs;

			/* LINTED pointer alignment */
			if (strcmp(nconf->nc_protofmly, NC_INET) == 0)
				((struct sockaddr_in *)
					syncaddr->buf)->sin_port
					= htons(IPPORT_TIMESERVER);
			else if (strcmp(nconf->nc_protofmly, NC_INET6) == 0)
				((struct sockaddr_in6 *)
					syncaddr->buf)->sin6_port
					= htons(IPPORT_TIMESERVER);
		}
	}

	if (!(secdata = nfs_clnt_secdata(&nfs_sec, hostname, args->knconf,
					syncaddr, flags))) {
		pr_err("errors constructing security related data\n");
		return (-1);
	}

	NFS_ARGS_EXTB_secdata(args, secdata);
	return (0);

err:
	pr_err(gettext("%s: secure: no time service\n"), hostname);
	return (-1);
}

/*
 * Get the network address on "hostname" for program "prog"
 * with version "vers" by using the nconf configuration data
 * passed in.
 *
 * If the address of a netconfig pointer is null then
 * information is not sufficient and no netbuf will be returned.
 *
 * Finally, ping the null procedure of that service.
 *
 * A similar routine is also defined in ../../autofs/autod_nfs.c.
 * This is a potential routine to move to ../lib for common usage.
 */
static struct netbuf *
get_the_addr(char *hostname, ulong_t prog, ulong_t vers,
	struct netconfig *nconf, ushort_t port, struct t_info *tinfo,
	caddr_t *fhp, bool_t get_pubfh, char *fspath)
{
	struct netbuf *nb = NULL;
	struct t_bind *tbind = NULL;
	CLIENT *cl = NULL;
	struct timeval tv;
	int fd = -1;
	AUTH *ah = NULL;
	AUTH *new_ah = NULL;
	struct snego_t snego;

	if (nconf == NULL)
		return (NULL);

	if ((fd = t_open(nconf->nc_device, O_RDWR, tinfo)) == -1)
		    goto done;

	/* LINTED pointer alignment */
	if ((tbind = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR))
		== NULL)
		goto done;

	if (get_pubfh == TRUE) {
		struct nd_hostserv hs;
		struct nd_addrlist *retaddrs;
		hs.h_host = hostname;

		if (port == 0)
			hs.h_serv = "nfs";
		else
			hs.h_serv = NULL;

		if (netdir_getbyname(nconf, &hs, &retaddrs) != ND_OK) {
			goto done;
		}
		memcpy(tbind->addr.buf, retaddrs->n_addrs->buf,
			retaddrs->n_addrs->len);
		tbind->addr.len = retaddrs->n_addrs->len;
		netdir_free((void *)retaddrs, ND_ADDRLIST);
		(void) netdir_options(nconf, ND_SET_RESERVEDPORT, fd, NULL);

	} else {
		if (rpcb_getaddr(prog, vers, nconf, &tbind->addr,
		    hostname) == FALSE) {
			goto done;
		}
	}

	if (port) {
		/* LINTED pointer alignment */
		if (strcmp(nconf->nc_protofmly, NC_INET) == 0)
			((struct sockaddr_in *)tbind->addr.buf)->sin_port
				= port;
		else if (strcmp(nconf->nc_protofmly, NC_INET6) == 0)
			((struct sockaddr_in6 *)tbind->addr.buf)->sin6_port
				= port;

	}

	cl = clnt_tli_create(fd, nconf, &tbind->addr, prog, vers, 0, 0);
	if (cl == NULL)
		goto done;

	ah = authsys_create_default();
	if (ah != NULL)
		cl->cl_auth = ah;

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	(void) clnt_control(cl, CLSET_TIMEOUT, (char *)&tv);

	if (get_pubfh == TRUE) {
	    enum snego_stat sec;

	    if (!snego_done) {
		/*
		 * negotiate sec flavor.
		 */
		snego.cnt = 0;
		if ((sec = nfs_sec_nego(vers, cl, fspath, &snego)) ==
			SNEGO_SUCCESS) {
		    int jj;

		/*
		 * check if server supports the one
		 * specified in the sec= option.
		 */
		    if (sec_opt) {
			for (jj = 0; jj < snego.cnt; jj++) {
			    if (snego.array[jj] == nfs_sec.sc_nfsnum) {
				snego_done = TRUE;
				break;
			    }
			}
		    }

		/*
		 * find a common sec flavor
		 */
		    if (!snego_done) {
			if (sec_opt) {
			    pr_err(gettext(
				"Server does not support the security"
				" flavor specified.\n"));
			}
			for (jj = 0; jj < snego.cnt; jj++) {
			    if (!nfs_getseconfig_bynumber(snego.array[jj],
				&nfs_sec)) {
				snego_done = TRUE;
				if (sec_opt) {
				    pr_err(gettext(
					"Security flavor %d was negotiated and"
					" will be used.\n"),
					nfs_sec.sc_nfsnum);
				}
				break;
			    }
			}
		    }
		    if (!snego_done)
			return (NULL);

		/*
		 * Now that the flavor has been
		 * negotiated, get the fh.
		 *
		 * First, create an auth handle using the negotiated
		 * sec flavor in the next lookup to
		 * fetch the filehandle.
		 */
		    new_ah = nfs_create_ah(cl, hostname, &nfs_sec);
		    if (new_ah == NULL)
			goto done;
		    cl->cl_auth = new_ah;
		} else if (sec == SNEGO_ARRAY_TOO_SMALL || sec ==
		    SNEGO_FAILURE) {
			goto done;
		}
		/*
		 * Note that if sec == SNEGO_DEF_VALID
		 * default sec flavor is acceptable.
		 * Use it to get the filehandle.
		 */
	    }

	    if (vers == NFS_VERSION) {
		wnl_diropargs arg;
		wnl_diropres *res;

		memset((char *)&arg.dir, 0, sizeof (wnl_fh));
		arg.name = fspath;
		res = wnlproc_lookup_2(&arg, cl);

		if (res == NULL || res->status != NFS_OK)
		    goto done;
		*fhp = malloc(sizeof (wnl_fh));

		if (*fhp == NULL) {
		    pr_err(gettext("no memory\n"));
		    goto done;
		}

		memcpy((char *)*fhp,
		    (char *)&res->wnl_diropres_u.wnl_diropres.file,
		    sizeof (wnl_fh));
	    } else {
		WNL_LOOKUP3args arg;
		WNL_LOOKUP3res *res;
		nfs_fh3 *fh3p;

		memset((char *)&arg.what.dir, 0, sizeof (wnl_fh3));
		arg.what.name = fspath;
		res = wnlproc3_lookup_3(&arg, cl);

		if (res == NULL || res->status != NFS3_OK)
		    goto done;

		fh3p = (nfs_fh3 *)malloc(sizeof (*fh3p));

		if (fh3p == NULL) {
		    pr_err(gettext("no memory\n"));
		    CLNT_FREERES(cl, xdr_WNL_LOOKUP3res, (char *)res);
		    goto done;
		}

		fh3p->fh3_length =
		    res->WNL_LOOKUP3res_u.res_ok.object.data.data_len;
		memcpy(fh3p->fh3_u.data,
		    res->WNL_LOOKUP3res_u.res_ok.object.data.data_val,
		    fh3p->fh3_length);

		*fhp = (caddr_t)fh3p;

		CLNT_FREERES(cl, xdr_WNL_LOOKUP3res, (char *)res);
	    }
	} else {
		void *res;

		if (vers == NFS_VERSION)
		    res = wnlproc_null_2(NULL, cl);
		else
		    res = wnlproc3_null_3(NULL, cl);

		if (res == NULL)
		    goto done;
	}

	/*
	 * Make a copy of the netbuf to return
	 */
	nb = (struct netbuf *)malloc(sizeof (*nb));
	if (nb == NULL) {
		pr_err(gettext("no memory\n"));
		goto done;
	}
	*nb = tbind->addr;
	nb->buf = (char *)malloc(nb->maxlen);
	if (nb->buf == NULL) {
		pr_err(gettext("no memory\n"));
		free(nb);
		nb = NULL;
		goto done;
	}
	(void) memcpy(nb->buf, tbind->addr.buf, tbind->addr.len);

done:
	if (cl) {
	    if (ah != NULL) {
		if (new_ah != NULL)
		    AUTH_DESTROY(ah);
		AUTH_DESTROY(cl->cl_auth);
		cl->cl_auth = NULL;
	    }
	    clnt_destroy(cl);
	    cl = NULL;
	}
	if (tbind) {
		t_free((char *)tbind, T_BIND);
		tbind = NULL;
	}
	if (fd >= 0)
		(void) t_close(fd);
	return (nb);
}

/*
 * Get a network address on "hostname" for program "prog"
 * with version "vers".  If the port number is specified (non zero)
 * then try for a TCP/UDP transport and set the port number of the
 * resulting IP address.
 *
 * If the address of a netconfig pointer was passed and
 * if it's not null, use it as the netconfig otherwise
 * assign the address of the netconfig that was used to
 * establish contact with the service.
 *
 * A similar routine is also defined in ../../autofs/autod_nfs.c.
 * This is a potential routine to move to ../lib for common usage.
 */
static struct netbuf *
get_addr(char *hostname, ulong_t prog, ulong_t vers, struct netconfig **nconfp,
	char *proto, ushort_t port, struct t_info *tinfo, caddr_t *fhp,
	bool_t get_pubfh, char *fspath)
{
	struct netbuf *nb = NULL;
	struct netconfig *nconf = NULL;
	NCONF_HANDLE *nc = NULL;
	int nthtry = FIRST_TRY;

	if (nconfp && *nconfp)
		return (get_the_addr(hostname, prog, vers, *nconfp, port,
			tinfo, fhp, get_pubfh, fspath));
	/*
	 * No nconf passed in.
	 *
	 * Try to get a nconf from /etc/netconfig filtered by
	 * the NETPATH environment variable.
	 * First search for COTS, second for CLTS unless proto
	 * is specified.  When we retry, we reset the
	 * netconfig list so that we would search the whole list
	 * all over again.
	 */
	if ((nc = setnetpath()) == NULL)
		goto done;

	/*
	 * If proto is specified, then only search for the match,
	 * otherwise try COTS first, if failed, try CLTS.
	 */
	if (proto) {

		while (nconf = getnetpath(nc)) {
			if (strcmp(nconf->nc_proto, proto))
				continue;

			if ((port != 0) &&
				((strcmp(nconf->nc_protofmly, NC_INET) == 0 ||
				strcmp(nconf->nc_protofmly, NC_INET6) == 0) &&
				(strcmp(nconf->nc_proto, NC_TCP) != 0 &&
				strcmp(nconf->nc_proto, NC_UDP) != 0)))

				continue;

			else {
				nb = get_the_addr(hostname, prog,
					vers, nconf, port, tinfo,
						fhp, get_pubfh, fspath);

				if (nb == NULL)
					/*
					 * continue with same protocol
					 * selection
					 */
					continue;
				else
					break;
			}
		} /* end of while */

		if (nconf == NULL)
			goto done;

		if ((nb = get_the_addr(hostname, prog, vers, nconf, port,
				tinfo, fhp, get_pubfh, fspath)) == NULL)
			goto done;


	} else {
retry:
		while (nconf = getnetpath(nc)) {
			if (nconf->nc_flag & NC_VISIBLE) {
				if (nthtry == FIRST_TRY) {
					if ((nconf->nc_semantics ==
						NC_TPI_COTS_ORD) ||
					    (nconf->nc_semantics ==
						NC_TPI_COTS)) {

						if (port == 0)
							break;

						if ((strcmp(nconf->nc_protofmly,
							NC_INET) == 0 ||
							strcmp(nconf->
							nc_protofmly,
							NC_INET6) == 0) &&
						    (strcmp(nconf->nc_proto,
							NC_TCP) == 0))

							break;
					}
				}
				if (nthtry == SECOND_TRY) {
					if (nconf->nc_semantics ==
						NC_TPI_CLTS) {
						if (port == 0)
							break;
						if ((strcmp(nconf->nc_protofmly,
							NC_INET) == 0 ||
							strcmp(nconf->
							nc_protofmly, NC_INET6)
							== 0) &&
							(strcmp(
							nconf->nc_proto,
							NC_UDP) == 0))
							break;
					}
				}
			}
		} /* while */
		if (nconf == NULL) {
			if (++nthtry <= MNT_PREF_LISTLEN) {
				endnetpath(nc);
				if ((nc = setnetpath()) == NULL)
					goto done;
				goto retry;
			} else
				goto done;
		} else {
			if ((nb = get_the_addr(hostname, prog, vers, nconf,
				port, tinfo, fhp, get_pubfh, fspath))
				== NULL) {
				/*
				 * Continue the same search path in the
				 * netconfig db until no more matched
				 * nconf (nconf == NULL).
				 */
				goto retry;
			}

		}
	}

	/*
	 * Got nconf and nb.  Now dup the netconfig structure (nconf)
	 * and return it thru nconfp.
	 */
	*nconfp = getnetconfigent(nconf->nc_netid);
	if (*nconfp == NULL) {
		syslog(LOG_ERR, "no memory\n");
		free(nb);
		nb = NULL;
	}
done:
	if (nc)
		endnetpath(nc);
	return (nb);
}

/*
 * Get a file handle usinging multi-component lookup with the public
 * file handle.
 */
static int
get_fh_via_pub(struct nfs_args *args, char *fshost, char *fspath, bool_t url,
	bool_t loud, int *versp, struct netconfig **nconfp, ushort_t port)
{
	uint_t vers_min;
	uint_t vers_max;
	int r;
	char *path;

	if (nfsvers != 0) {
		vers_max = vers_min = nfsvers;
	} else {
		vers_max = NFS_VERSMAX;
		vers_min = NFS_VERSMIN;
	}

	if (url == FALSE) {
		path = malloc(strlen(fspath) + 2);
		if (path == NULL) {
			if (loud == TRUE)  {
				pr_err(gettext("no memory\n"));
			}
			return (RET_ERR);
		}

		path[0] = (char)WNL_NATIVEPATH;
		(void) strcpy(&path[1], fspath);

	} else  {
		path = fspath;
	}

	for (nfsvers_to_use = vers_max; nfsvers_to_use >= vers_min;
	    nfsvers_to_use--) {
		/*
		 * getaddr_nfs will also fill in the fh for us.
		 */
		r = getaddr_nfs(args, fshost, nconfp, TRUE, path, port);

		if (r == RET_OK) {
			/*
			 * Since we are using the public fh, and NLM is
			 * not firewall friendly, use local locking.
			 */
			args->flags |= NFSMNT_LLOCK;
			*versp = nfsvers_to_use;
			if (nfsvers_to_use == NFS_V3)
				fstype = MNTTYPE_NFS3;
			if (fspath != path)
				free(path);

			return (r);
		}
	}

	if (fspath != path) {
		free(path);
	}

	if (loud == TRUE) {
		pr_err(gettext("Could not use public filehandle in request to"
			" server %s\n"), fshost);
	}

	return (r);
}

/*
 * get fhandle of remote path from server's mountd
 */
static int
get_fh(struct nfs_args *args, char *fshost, char *fspath, int *versp,
	bool_t loud_on_mnt_err)
{
	static struct fhstatus fhs;
	static struct mountres3 mountres3;
	static struct pathcnf p;
	nfs_fh3 *fh3p;
	struct timeval timeout = { 25, 0};
	CLIENT *cl;
	enum clnt_stat rpc_stat;
	rpcvers_t outvers = 0;
	rpcvers_t vers_to_try;
	rpcvers_t vers_min;
	static int printed = 0;
	int count, i, *auths;
	char *msg;

	if (nfsvers == 2) {
		vers_to_try = MOUNTVERS_POSIX;
		vers_min = MOUNTVERS;
	} else if (nfsvers == 3) {
		vers_to_try = MOUNTVERS3;
		vers_min = MOUNTVERS3;
	} else {
		vers_to_try = MOUNTVERS3;
		vers_min = MOUNTVERS;
	}

	while ((cl = clnt_create_vers(fshost, MOUNTPROG, &outvers,
			vers_min, vers_to_try, "datagram_v")) == NULL) {
		if (rpc_createerr.cf_stat == RPC_UNKNOWNHOST) {
			pr_err(gettext("%s: %s\n"), fshost,
			    clnt_spcreateerror(""));
			return (RET_ERR);
		}

		/*
		 * We don't want to downgrade version on lost packets
		 */
		if (rpc_createerr.cf_stat == RPC_TIMEDOUT) {
			pr_err(gettext("%s: %s\n"), fshost,
			    clnt_spcreateerror(""));
			return (RET_RETRY);
		}

		/*
		 * back off and try the previous version - patch to the
		 * problem of version numbers not being contigous and
		 * clnt_create_vers failing (SunOS4.1 clients & SGI servers)
		 * The problem happens with most non-Sun servers who
		 * don't support mountd protocol #2. So, in case the
		 * call fails, we re-try the call anyway.
		 */
		vers_to_try--;
		if (vers_to_try < vers_min) {
			if (rpc_createerr.cf_stat == RPC_PROGVERSMISMATCH) {
				if (nfsvers == 0) {
					pr_err(gettext(
			"%s:%s: no applicable versions of NFS supported\n"),
					    fshost, fspath);
				} else {
					pr_err(gettext(
			"%s:%s: NFS Version %d not supported\n"),
					    fshost, fspath, nfsvers);
				}
				return (RET_ERR);
			}
			if (!printed) {
				pr_err(gettext("%s: %s\n"), fshost,
				    clnt_spcreateerror(""));
				printed = 1;
			}
			return (RET_RETRY);
		}
	}
	if (posix && outvers < MOUNTVERS_POSIX) {
		pr_err(gettext("%s: %s: no pathconf info\n"),
		    fshost, clnt_sperror(cl, ""));
		clnt_destroy(cl);
		return (RET_ERR);
	}

	if (__clnt_bindresvport(cl) < 0) {
		pr_err(gettext("Couldn't bind to reserved port\n"));
		clnt_destroy(cl);
		return (RET_RETRY);
	}

	if ((cl->cl_auth = authsys_create_default()) == NULL) {
		pr_err(
		    gettext("Couldn't create default authentication handle\n"));
		clnt_destroy(cl);
		return (RET_RETRY);
	}

	switch (outvers) {
	case MOUNTVERS:
	case MOUNTVERS_POSIX:
		*versp = nfsvers_to_use = NFS_VERSION;
		rpc_stat = clnt_call(cl, MOUNTPROC_MNT, xdr_dirpath,
			(caddr_t)&fspath, xdr_fhstatus, (caddr_t)&fhs, timeout);
		if (rpc_stat != RPC_SUCCESS) {
			pr_err(gettext("%s:%s: server not responding %s\n"),
			    fshost, fspath, clnt_sperror(cl, ""));
			clnt_destroy(cl);
			return (RET_RETRY);
		}

		if ((errno = fhs.fhs_status) != MNT_OK) {
			if (loud_on_mnt_err) {
			    if (errno == EACCES) {
				pr_err(gettext("%s:%s: access denied\n"),
				    fshost, fspath);
			    } else {
				pr_err(gettext("%s:%s: %s\n"), fshost, fspath,
				    strerror(errno));
			    }
			}
			clnt_destroy(cl);
			return (RET_MNTERR);
		}
		args->fh = malloc(sizeof (fhs.fhstatus_u.fhs_fhandle));
		if (args->fh == NULL) {
			pr_err(gettext("no memory\n"));
			return (RET_ERR);
		}
		memcpy((caddr_t)args->fh, (caddr_t)&fhs.fhstatus_u.fhs_fhandle,
			sizeof (fhs.fhstatus_u.fhs_fhandle));
		if (!errno && posix) {
			rpc_stat = clnt_call(cl, MOUNTPROC_PATHCONF,
				xdr_dirpath, (caddr_t)&fspath, xdr_ppathcnf,
				(caddr_t)&p, timeout);
			if (rpc_stat != RPC_SUCCESS) {
				pr_err(gettext(
				    "%s:%s: server not responding %s\n"),
				    fshost, fspath, clnt_sperror(cl, ""));
				free(args->fh);
				clnt_destroy(cl);
				return (RET_RETRY);
			}
			if (_PC_ISSET(_PC_ERROR, p.pc_mask)) {
				pr_err(gettext(
				    "%s:%s: no pathconf info\n"),
				    fshost, fspath);
				free(args->fh);
				clnt_destroy(cl);
				return (RET_ERR);
			}
			args->flags |= NFSMNT_POSIX;
			args->pathconf = malloc(sizeof (p));
			if (args->pathconf == NULL) {
				pr_err(gettext("no memory\n"));
				free(args->fh);
				clnt_destroy(cl);
				return (RET_ERR);
			}
			memcpy((caddr_t)args->pathconf, (caddr_t)&p,
				sizeof (p));
		}
		break;

	case MOUNTVERS3:
		*versp = nfsvers_to_use = NFS_V3;
		rpc_stat = clnt_call(cl, MOUNTPROC_MNT, xdr_dirpath,
				(caddr_t)&fspath,
				xdr_mountres3, (caddr_t)&mountres3, timeout);
		if (rpc_stat != RPC_SUCCESS) {
			pr_err(gettext("%s:%s: server not responding %s\n"),
			    fshost, fspath, clnt_sperror(cl, ""));
			clnt_destroy(cl);
			return (RET_RETRY);
		}

		/*
		 * Assume here that most of the MNT3ERR_*
		 * codes map into E* errors.
		 */
		if ((errno = mountres3.fhs_status) != MNT_OK) {
		    if (loud_on_mnt_err) {
			switch (errno) {
			case MNT3ERR_NAMETOOLONG:
				msg = "path name is too long";
				break;
			case MNT3ERR_NOTSUPP:
				msg = "operation not supported";
				break;
			case MNT3ERR_SERVERFAULT:
				msg = "server fault";
				break;
			default:
				msg = strerror(errno);
				break;
			}
			pr_err(gettext("%s:%s: %s\n"), fshost, fspath, msg);
		    }
		    clnt_destroy(cl);
		    return (RET_MNTERR);
		}

		fh3p = (nfs_fh3 *)malloc(sizeof (*fh3p));
		if (fh3p == NULL) {
			pr_err(gettext("no memory\n"));
			return (RET_ERR);
		}
		fh3p->fh3_length =
			mountres3.mountres3_u.mountinfo.fhandle.fhandle3_len;
		(void) memcpy(fh3p->fh3_u.data,
			mountres3.mountres3_u.mountinfo.fhandle.fhandle3_val,
			fh3p->fh3_length);
		args->fh = (caddr_t)fh3p;
		fstype = MNTTYPE_NFS3;

		/*
		 * Check the security flavor to be used.
		 *
		 * If "secure" or "sec=flavor" is a mount
		 * option, check if the server supports the "flavor".
		 * If the server does not support the flavor, return
		 * error.
		 *
		 * If no mount option is given then use the first supported
		 * security flavor (by the client) in the auth list returned
		 * from the server.
		 *
		 */
		auths =
		mountres3.mountres3_u.mountinfo.auth_flavors.auth_flavors_val;
		count =
		mountres3.mountres3_u.mountinfo.auth_flavors.auth_flavors_len;

		if (sec_opt) {
			for (i = 0; i < count; i++) {
				if (auths[i] == nfs_sec.sc_nfsnum)
				    break;
			}
			if (i >= count) {
				goto autherr;
			}
		} else {
		    if (count > 0) {
			for (i = 0; i < count; i++) {
			    if (!nfs_getseconfig_bynumber(auths[i], &nfs_sec)) {
				sec_opt++;
				break;
			    }
			}
			if (i >= count) {
			    goto autherr;
			}
		    }
		}
		break;
	default:
		pr_err(gettext("%s:%s: Unknown MOUNT version %d\n"),
		    fshost, fspath, outvers);
		clnt_destroy(cl);
		return (RET_ERR);
	}

	clnt_destroy(cl);
	return (RET_OK);

autherr:
	pr_err(gettext("server %s shares %s with these security modes, none\n\t"
		"of which are configured on this client:"), fshost, fspath);

	for (i = 0; i < count; i++) {
		printf(" %d", auths[i]);
	}
	printf("\n");
	clnt_destroy(cl);
	return (RET_ERR);
}

/*
 * Fill in the address for the server's NFS service and
 * fill in a knetconfig structure for the transport that
 * the service is available on.
 */
static int
getaddr_nfs(struct nfs_args *args, char *fshost, struct netconfig **nconfp,
	bool_t get_pubfh, char *fspath, ushort_t port)
{
	struct stat sb;
	struct netconfig *nconf;
	struct knetconfig *knconfp;
	static int printed = 0;
	struct t_info tinfo;

	args->addr = get_addr(fshost, NFS_PROGRAM, nfsvers_to_use, nconfp,
		nfs_proto, port, &tinfo, &args->fh, get_pubfh, fspath);

	if (args->addr == NULL) {
		/*
		 * We could have failed because the server had no public
		 * file handle support. So don't print a message and don't
		 * retry.
		 */
		if (get_pubfh == TRUE)
			return (RET_ERR);

		if (!printed) {
			pr_err(gettext("%s: NFS service not responding\n"),
			    fshost);
			printed = 1;
		}
		return (RET_RETRY);
	}
	nconf = *nconfp;

	if (stat(nconf->nc_device, &sb) < 0) {
		pr_err(gettext("getaddr_nfs: couldn't stat: %s: %s\n"),
		    nconf->nc_device, strerror(errno));
		return (RET_ERR);
	}

	knconfp = (struct knetconfig *)malloc(sizeof (*knconfp));
	if (!knconfp) {
		pr_err(gettext("no memory\n"));
		return (RET_ERR);
	}
	knconfp->knc_semantics = nconf->nc_semantics;
	knconfp->knc_protofmly = nconf->nc_protofmly;
	knconfp->knc_proto = nconf->nc_proto;
	knconfp->knc_rdev = sb.st_rdev;

	/* make sure we don't overload the transport */
	if (tinfo.tsdu > 0 && tinfo.tsdu < NFS_MAXDATA + NFS_RPC_HDR) {
		args->flags |= (NFSMNT_RSIZE | NFSMNT_WSIZE);
		if (args->rsize == 0 || args->rsize > tinfo.tsdu - NFS_RPC_HDR)
			args->rsize = tinfo.tsdu - NFS_RPC_HDR;
		if (args->wsize == 0 || args->wsize > tinfo.tsdu - NFS_RPC_HDR)
			args->wsize = tinfo.tsdu - NFS_RPC_HDR;
	}

	args->flags |= NFSMNT_KNCONF;
	args->knconf = knconfp;
	return (RET_OK);
}

static int
retry(struct mnttab *mntp, int ro)
{
	int delay = 5;
	int count = retries;
	int r;

	if (bg) {
		if (fork() > 0)
			return (RET_OK);
		pr_err(gettext("backgrounding: %s\n"), mntp->mnt_mountp);
	} else
		pr_err(gettext("retrying: %s\n"), mntp->mnt_mountp);

	while (count--) {
		if ((r = mount_nfs(mntp, ro)) == RET_OK) {
			pr_err(gettext("%s: mounted OK\n"), mntp->mnt_mountp);
			return (RET_OK);
		}
		if (r != RET_RETRY)
			break;

		if (count > 0) {
		    (void) sleep(delay);
		    delay *= 2;
		    if (delay > 120)
			    delay = 120;
		}
	}
	pr_err(gettext("giving up on: %s\n"), mntp->mnt_mountp);
	return (RET_ERR);
}
