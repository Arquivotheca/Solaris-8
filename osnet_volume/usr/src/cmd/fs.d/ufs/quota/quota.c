/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)quota.c	1.27	98/05/01 SMI"	/* SVr4.0 1.8	*/
/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * Copyright (c) 1986,1987,1988,1989,1996,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */
/*
 * Disk quota reporting program.
 */
#include <stdio.h>
#include <sys/mnttab.h>
#include <ctype.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/mntent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/fs/ufs_quota.h>

int	vflag;
int	nolocalquota;

extern int	optind;
extern char	*optarg;

#define	QFNAME	"quotas"

#if DEV_BSIZE < 1024
#define	kb(x)	((x) / (1024 / DEV_BSIZE))
#else
#define	kb(x)	((x) * (DEV_BSIZE / 1024))
#endif

static int getnfsquota(char *, char *, uid_t, struct dqblk *);

main(argc, argv)
	int argc;
	char *argv[];
{
	int	opt;
	int	i;
	int	status = 0;

	while ((opt = getopt(argc, argv, "vV")) != EOF) {
		switch (opt) {

		case 'v':
			vflag++;
			break;

		case 'V':		/* Print command line */
			{
			char	*opt_text;
			int	opt_count;

			(void) fprintf(stdout, "quota -F UFS ");
			for (opt_count = 1; opt_count < argc; opt_count++) {
				opt_text = argv[opt_count];
				if (opt_text)
				    (void) fprintf(stdout, " %s ", opt_text);
			}
			(void) fprintf(stdout, "\n");
			}
			break;

		case '?':
			fprintf(stderr, "quota: %c: unknown option\n",
				opt);
			fprintf(stderr, "ufs usage: quota [-v] [username]\n");
			exit(32);
		}
	}
	if (quotactl(Q_ALLSYNC, NULL, (uid_t)0, NULL) < 0 && errno == EINVAL) {
		if (vflag)
			fprintf(stderr, "There are no quotas on this system\n");
		nolocalquota++;
	}
	if (argc == optind) {
		showuid(getuid());
		exit(0);
	}
	for (i = optind; i < argc; i++) {
		if (alldigits(argv[i])) {
			showuid((uid_t)atoi(argv[i]));
		} else
			status |= showname(argv[i]);
	}
	exit(status);
}

showuid(uid)
	uid_t uid;
{
	struct passwd *pwd = getpwuid(uid);

	if (uid == 0) {
		if (vflag)
			printf("no disk quota for uid 0\n");
		return;
	}
	if (pwd == NULL)
		showquotas(uid, "(no account)");
	else
		showquotas(uid, pwd->pw_name);
}

int
showname(name)
	char *name;
{
	struct passwd *pwd = getpwnam(name);

	if (pwd == NULL) {
		fprintf(stderr, "quota: %s: unknown user\n", name);
		return (32);
	}
	if (pwd->pw_uid == 0) {
		if (vflag)
			printf("no disk quota for %s (uid 0)\n", name);
		return (0);
	}
	showquotas(pwd->pw_uid, name);
	return (0);
}

#include "../../nfs/lib/replica.h"

showquotas(uid, name)
	uid_t uid;
	char *name;
{
	struct mnttab mnt;
	FILE *mtab;
	struct dqblk dqblk;
	uid_t myuid;

	myuid = getuid();
	if (uid != myuid && myuid != 0) {
		printf("quota: %s (uid %d): permission denied\n", name, uid);
		exit(32);
	}
	if (vflag)
		heading(uid, name);
	mtab = fopen(MNTTAB, "r");
	while (getmntent(mtab, &mnt) == NULL) {
		if (strcmp(mnt.mnt_fstype, MNTTYPE_UFS) == 0) {
			if (nolocalquota ||
			    (quotactl(Q_GETQUOTA,
				mnt.mnt_mountp, uid, &dqblk) != 0 &&
				!(vflag && getdiskquota(&mnt, uid, &dqblk))))
					continue;
		} else if (strcmp(mnt.mnt_fstype, MNTTYPE_NFS) == 0) {

			struct replica *rl;
			int count;

			if (hasopt(MNTOPT_NOQUOTA, mnt.mnt_mntopts))
				continue;

			/*
			 * Skip quota processing if mounted with public
			 * option. We are not likely to be able to pierce
			 * a fire wall to contact the quota server.
			 */
			if (hasopt(MNTOPT_PUBLIC, mnt.mnt_mntopts))
				continue;

			rl = parse_replica(mnt.mnt_special, &count);

			if (rl == NULL) {

				if (count < 0)
					fprintf(stderr, "cannot find hostname "
					    "and/or pathname for %s\n",
					    mnt.mnt_mountp);
				else
					fprintf(stderr, "no memory to parse "
					    "mnttab entry for %s\n",
					    mnt.mnt_mountp);
				continue;
			}

			/*
			 * We skip quota reporting on mounts with replicas
			 * for the following reasons:
			 *
			 * (1) Very little point in reporting quotas on
			 * a set of read-only replicas ... how will the
			 * user correct the problem?
			 *
			 * (2) Which replica would we report the quota
			 * for? If we pick the current replica, what
			 * happens when a fail over event occurs? The
			 * next time quota is run, the quota will look
			 * all different, or there won't even be one.
			 * This has the potential to break scripts.
			 *
			 * If we prnt quouta for all replicas, how do
			 * we present the output without breaking scripts?
			 */

			if (count > 1) {
				free_replica(rl, count);
				continue;
			}

			/*
			 * Skip file systems mounted using public fh.
			 * We are not likely to be able to pierce
			 * a fire wall to contact the quota server.
			 */
			if (strcmp(rl[0].host, "nfs") == 0 &&
			    strncmp(rl[0].path, "//", 2) == 0) {
				free_replica(rl, count);
				continue;
			}

			if (!getnfsquota(rl[0].host, rl[0].path, uid, &dqblk)) {
				free_replica(rl, count);
				continue;
			}

			free_replica(rl, count);

		} else {
			continue;
		}
		if (dqblk.dqb_bsoftlimit == 0 && dqblk.dqb_bhardlimit == 0 &&
		    dqblk.dqb_fsoftlimit == 0 && dqblk.dqb_fhardlimit == 0)
			continue;
		if (vflag)
			prquota(&mnt, &dqblk);
		else
			warn(&mnt, &dqblk);
	}
	fclose(mtab);
}

warn(mntp, dqp)
	struct mnttab *mntp;
	struct dqblk *dqp;
{
	struct timeval tv;

	time(&(tv.tv_sec));
	tv.tv_usec = 0;
	if (dqp->dqb_bhardlimit &&
		dqp->dqb_curblocks >= dqp->dqb_bhardlimit) {
		printf("Block limit reached on %s\n", mntp->mnt_mountp);
	} else if (dqp->dqb_bsoftlimit &&
		dqp->dqb_curblocks >= dqp->dqb_bsoftlimit) {
		if (dqp->dqb_btimelimit == 0) {
			printf("Over disk quota on %s, remove %dK\n",
			    mntp->mnt_mountp,
			    kb(dqp->dqb_curblocks - dqp->dqb_bsoftlimit + 1));
		} else if (dqp->dqb_btimelimit > tv.tv_sec) {
			char btimeleft[80];

			fmttime(btimeleft, dqp->dqb_btimelimit - tv.tv_sec);
			printf("Over disk quota on %s, remove %dK within %s\n",
			    mntp->mnt_mountp,
			    kb(dqp->dqb_curblocks - dqp->dqb_bsoftlimit + 1),
			    btimeleft);
		} else {
			printf(
		"Over disk quota on %s, time limit has expired, remove %dK\n",
			    mntp->mnt_mountp,
			    kb(dqp->dqb_curblocks - dqp->dqb_bsoftlimit + 1));
		}
	}
	if (dqp->dqb_fhardlimit &&
	    dqp->dqb_curfiles >= dqp->dqb_fhardlimit) {
		printf("File count limit reached on %s\n", mntp->mnt_mountp);
	} else if (dqp->dqb_fsoftlimit &&
	    dqp->dqb_curfiles >= dqp->dqb_fsoftlimit) {
		if (dqp->dqb_ftimelimit == 0) {
			printf("Over file quota on %s, remove %d file%s\n",
			    mntp->mnt_mountp,
			    dqp->dqb_curfiles - dqp->dqb_fsoftlimit + 1,
			    ((dqp->dqb_curfiles - dqp->dqb_fsoftlimit + 1) > 1 ?
				"s" : ""));
		} else if (dqp->dqb_ftimelimit > tv.tv_sec) {
			char ftimeleft[80];

			fmttime(ftimeleft, dqp->dqb_ftimelimit - tv.tv_sec);
			printf(
"Over file quota on %s, remove %d file%s within %s\n",
			    mntp->mnt_mountp,
			    dqp->dqb_curfiles - dqp->dqb_fsoftlimit + 1,
			    ((dqp->dqb_curfiles - dqp->dqb_fsoftlimit + 1) > 1 ?
				"s" : ""), ftimeleft);
		} else {
			printf(
"Over file quota on %s, time limit has expired, remove %d file%s\n",
			    mntp->mnt_mountp,
			    dqp->dqb_curfiles - dqp->dqb_fsoftlimit + 1,
			    ((dqp->dqb_curfiles - dqp->dqb_fsoftlimit + 1) > 1 ?
				"s" : ""));
		}
	}
}

heading(uid, name)
	uid_t uid;
	char *name;
{
	printf("Disk quotas for %s (uid %ld):\n", name, (long)uid);
	printf("%-12s %7s%7s%7s%12s%7s%7s%7s%12s\n",
		"Filesystem",
		"usage",
		"quota",
		"limit",
		"timeleft",
		"files",
		"quota",
		"limit",
		"timeleft");
}

prquota(mntp, dqp)
	register struct mnttab *mntp;
	register struct dqblk *dqp;
{
	struct timeval tv;
	char ftimeleft[80], btimeleft[80];
	char *cp;

	time(&(tv.tv_sec));
	tv.tv_usec = 0;
	if (dqp->dqb_bsoftlimit && dqp->dqb_curblocks >= dqp->dqb_bsoftlimit) {
		if (dqp->dqb_btimelimit == 0) {
			strcpy(btimeleft, "NOT STARTED");
		} else if (dqp->dqb_btimelimit > tv.tv_sec) {
			fmttime(btimeleft, dqp->dqb_btimelimit - tv.tv_sec);
		} else {
			strcpy(btimeleft, "EXPIRED");
		}
	} else {
		btimeleft[0] = '\0';
	}
	if (dqp->dqb_fsoftlimit && dqp->dqb_curfiles >= dqp->dqb_fsoftlimit) {
		if (dqp->dqb_ftimelimit == 0) {
			strcpy(ftimeleft, "NOT STARTED");
		} else if (dqp->dqb_ftimelimit > tv.tv_sec) {
			fmttime(ftimeleft, dqp->dqb_ftimelimit - tv.tv_sec);
		} else {
			strcpy(ftimeleft, "EXPIRED");
		}
	} else {
		ftimeleft[0] = '\0';
	}
	if (strlen(mntp->mnt_mountp) > 12) {
		printf("%s\n", mntp->mnt_mountp);
		cp = "";
	} else {
		cp = mntp->mnt_mountp;
	}
	printf("%-12.12s %7d %6d %6d %11s %6d %6d %6d %11s\n",
	    cp,
	    kb(dqp->dqb_curblocks),
	    kb(dqp->dqb_bsoftlimit),
	    kb(dqp->dqb_bhardlimit),
	    btimeleft,
	    dqp->dqb_curfiles,
	    dqp->dqb_fsoftlimit,
	    dqp->dqb_fhardlimit,
	    ftimeleft);
}

fmttime(buf, time)
	char *buf;
	register long time;
{
	int i;
	static struct {
		int c_secs;		/* conversion units in secs */
		char * c_str;		/* unit string */
	} cunits [] = {
		{60*60*24*28, "months"},
		{60*60*24*7, "weeks"},
		{60*60*24, "days"},
		{60*60, "hours"},
		{60, "mins"},
		{1, "secs"}
	};

	if (time <= 0) {
		strcpy(buf, "EXPIRED");
		return;
	}
	for (i = 0; i < sizeof (cunits)/sizeof (cunits[0]); i++) {
		if (time >= cunits[i].c_secs)
			break;
	}
	sprintf(buf, "%.1f %s", (double)time/cunits[i].c_secs, cunits[i].c_str);
}

alldigits(s)
	register char *s;
{
	register c;

	c = *s++;
	do {
		if (!isdigit(c))
			return (0);
	} while (c = *s++);
	return (1);
}

int
getdiskquota(mntp, uid, dqp)
	struct mnttab *mntp;
	uid_t uid;
	struct dqblk *dqp;
{
	int fd;
	dev_t fsdev;
	struct stat64 statb;
	char qfilename[MAXPATHLEN];
	extern int errno;

	if (stat64(mntp->mnt_special, &statb) < 0 ||
	    (statb.st_mode & S_IFMT) != S_IFBLK)
		return (0);
	fsdev = statb.st_rdev;
	sprintf(qfilename, "%s/%s", mntp->mnt_mountp, QFNAME);
	if (stat64(qfilename, &statb) < 0 || statb.st_dev != fsdev)
		return (0);
	if ((fd = open64(qfilename, O_RDONLY)) < 0)
		return (0);
	(void) llseek(fd, (offset_t)dqoff(uid), L_SET);
	switch (read(fd, dqp, sizeof (struct dqblk))) {
	case 0:				/* EOF */
		/*
		 * Convert implicit 0 quota (EOF)
		 * into an explicit one (zero'ed dqblk).
		 */
		memset((caddr_t)dqp, 0, sizeof (struct dqblk));
		break;

	case sizeof (struct dqblk):	/* OK */
		break;

	default:			/* ERROR */
		close(fd);
		return (0);
	}
	close(fd);
	return (1);
}

#include <sys/errno.h>

quotactl(cmd, mountp, uid, addr)
	int		cmd;
	char		*mountp;
	uid_t		uid;
	caddr_t		addr;
{
	int		fd;
	int		status;
	struct quotctl	quota;
	char		mountpoint[256];

	FILE		*fstab;
	struct mnttab	mnt;


	if ((mountp == NULL) && (cmd == Q_ALLSYNC)) {
	/*
	 * Find the mount point of any mounted file system. This is
	 * because the ioctl that implements the quotactl call has
	 * to go to a real file, and not to the block device.
	 */
		if ((fstab = fopen(MNTTAB, "r")) == NULL) {
			fprintf(stderr, "%s: ", MNTTAB);
			perror("open");
			exit(32);
		}
		fd = -1;
		while ((status = getmntent(fstab, &mnt)) == NULL) {
			if (strcmp(mnt.mnt_fstype, MNTTYPE_UFS) != 0 ||
				hasopt(MNTOPT_RO, mnt.mnt_mntopts))
				continue;
			(void) strcpy(mountpoint, mnt.mnt_mountp);
			strcat(mountpoint, "/quotas");
			if ((fd = open64(mountpoint, O_RDONLY)) == -1)
				break;
		}
		fclose(fstab);
		if (fd == -1) {
			errno = ENOENT;
			return (-1);
		}
	} else {
		if (mountp == NULL || mountp[0] == '\0') {
			errno = ENOENT;
			return (-1);
		}
		(void) strcpy(mountpoint, mountp);
		strcat(mountpoint, "/quotas");
		if ((fd = open64(mountpoint, O_RDONLY)) < 0)
			return (-1);
	}	/* else */
	quota.op = cmd;
	quota.uid = uid;
	quota.addr = addr;
	status = ioctl(fd, Q_QUOTACTL, &quota);
	if (fd != 0)
		close(fd);
	return (status);
}


/*
 * Return 1 if opt appears in optlist
 */
int
hasopt(opt, optlist)
	char *opt, *optlist;
{
	char *value;
	char *opts[2];

	opts[0] = opt;
	opts[1] = NULL;

	if (optlist == NULL)
		return (0);
	while (*optlist != '\0') {
		if (getsubopt(&optlist, opts, &value) == 0)
			return (1);
	}
	return (0);
}

#include <rpc/rpc.h>
#include <netdb.h>
#include <rpcsvc/rquota.h>

static int
getnfsquota(char *hostp, char *path, uid_t uid, struct dqblk *dqp)
{
	struct getquota_args gq_args;
	struct getquota_rslt gq_rslt;
	struct rquota *rquota;
	extern char *strchr();

	gq_args.gqa_pathp = path;
	gq_args.gqa_uid = uid;
	if (callaurpc(hostp, RQUOTAPROG, RQUOTAVERS,
	    (vflag? RQUOTAPROC_GETQUOTA: RQUOTAPROC_GETACTIVEQUOTA),
	    xdr_getquota_args, &gq_args, xdr_getquota_rslt, &gq_rslt) != 0) {
		return (0);
	}
	switch (gq_rslt.status) {
	case Q_OK:
		{
		struct timeval tv;
		u_longlong_t limit;

		rquota = &gq_rslt.getquota_rslt_u.gqr_rquota;

		if (!vflag && rquota->rq_active == FALSE)
			return (0);
		gettimeofday(&tv, NULL);
		limit = (u_longlong_t)(rquota->rq_bhardlimit) *
		    rquota->rq_bsize / DEV_BSIZE;
		dqp->dqb_bhardlimit = limit;
		limit = (u_longlong_t)(rquota->rq_bsoftlimit) *
		    rquota->rq_bsize / DEV_BSIZE;
		dqp->dqb_bsoftlimit = limit;
		limit = (u_longlong_t)(rquota->rq_curblocks) *
		    rquota->rq_bsize / DEV_BSIZE;
		dqp->dqb_curblocks = limit;
		dqp->dqb_fhardlimit = rquota->rq_fhardlimit;
		dqp->dqb_fsoftlimit = rquota->rq_fsoftlimit;
		dqp->dqb_curfiles = rquota->rq_curfiles;
		dqp->dqb_btimelimit =
		    tv.tv_sec + rquota->rq_btimeleft;
		dqp->dqb_ftimelimit =
		    tv.tv_sec + rquota->rq_ftimeleft;
		return (1);
		}

	case Q_NOQUOTA:
		break;

	case Q_EPERM:
		fprintf(stderr, "quota permission error, host: %s\n", hostp);
		break;

	default:
		fprintf(stderr, "bad rpc result, host: %s\n",  hostp);
		break;
	}
	return (0);
}

callaurpc(host, prognum, versnum, procnum, inproc, in, outproc, out)
	char *host;
	xdrproc_t inproc, outproc;
	char *in, *out;
{
	static enum clnt_stat clnt_stat;
	struct timeval tottimeout;

	static CLIENT *cl = NULL;
	static int oldprognum, oldversnum;
	static char oldhost[MAXHOSTNAMELEN+1];

	/*
	 * Cache the client handle in case there are lots
	 * of entries in the /etc/mnttab for the same
	 * server. If the server returns an error, don't
	 * make further calls.
	 */
	if (cl == NULL || oldprognum != prognum || oldversnum != versnum ||
		strcmp(oldhost, host) != 0) {
		if (cl) {
			clnt_destroy(cl);
			cl = NULL;
		}
		cl = clnt_create(host, prognum, versnum, "udp");
		if (cl == NULL)
			return ((int) RPC_TIMEDOUT);
		if ((cl->cl_auth = authunix_create_default()) == NULL) {
			clnt_destroy(cl);
			return (RPC_CANTSEND);
		}
		oldprognum = prognum;
		oldversnum = versnum;
		(void) strcpy(oldhost, host);
		clnt_stat = RPC_SUCCESS;
	}

	if (clnt_stat != RPC_SUCCESS)
		return ((int) clnt_stat);	/* don't bother retrying */

	tottimeout.tv_sec  = 5;
	tottimeout.tv_usec = 0;
	clnt_stat = clnt_call(cl, procnum, inproc, in,
	    outproc, out, tottimeout);

	return ((int) clnt_stat);
}
