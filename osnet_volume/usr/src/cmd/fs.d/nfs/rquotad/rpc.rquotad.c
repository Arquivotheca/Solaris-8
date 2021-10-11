#pragma ident	"@(#)rpc.rquotad.c	1.7	98/03/16 SMI"

/*
 * Copyright (c) 1985,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <stropts.h>
#include <sys/netconfig.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/param.h>
#include <sys/time.h>
#ifdef notdef
#include <netconfig.h>
#endif
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/fs/ufs_quota.h>
#include <netdir.h>
#include <rpc/rpc.h>
#include <rpcsvc/rquota.h>
#include <tiuser.h>
#include <unistd.h>

#define	QFNAME		"quotas"	/* name of quota file */
#define	RPCSVC_CLOSEDOWN 120		/* 2 minutes */

struct fsquot {
	struct fsquot *fsq_next;
	char *fsq_dir;
	char *fsq_devname;
	dev_t fsq_dev;
};

struct fsquot *fsqlist = NULL;

typedef struct authunix_parms *authp;

extern int errno;

static int request_pending;		/* Request in progress ? */

void closedown();
void dispatch();
struct fsquot *findfsq();
void freefs();
int  getdiskquota();
void getquota();
int  hasquota();
void log_cant_reply();
void setupfs();

/*ARGSUSED*/
main(argc, argv)
	int argc;
	char **argv;
{
	register SVCXPRT *transp;
	int i;
	struct rlimit rl;

	/*
	 * If stdin looks like a TLI endpoint, we assume
	 * that we were started by a port monitor. If
	 * t_getstate fails with TBADF, this is not a
	 * TLI endpoint.
	 */
	if (t_getstate(0) != -1 || t_errno != TBADF) {
		char *netid;
		struct netconfig *nconf = NULL;

		openlog("rquotad", LOG_PID, LOG_DAEMON);

		if ((netid = getenv("NLSPROVIDER")) == NULL) {
			netid = "udp";
		}
		if ((nconf = getnetconfigent(netid)) == NULL) {
			syslog(LOG_ERR, "cannot get transport info");
		}

		if ((transp = svc_tli_create(0, nconf, NULL, 0, 0)) == NULL) {
			syslog(LOG_ERR, "cannot create server handle");
			exit(1);
		}
		if (nconf)
			freenetconfigent(nconf);

		if (!svc_reg(transp, RQUOTAPROG, RQUOTAVERS, dispatch, 0)) {
			syslog(LOG_ERR,
				"unable to register (RQUOTAPROG, RQUOTAVERS).");
			exit(1);
		}

		(void) signal(SIGALRM, (void(*)(int)) closedown);
		(void) alarm(RPCSVC_CLOSEDOWN);

		svc_run();
		exit(1);
		/* NOTREACHED */
	}

	/*
	 * Started from a shell - fork the daemon.
	 */

	switch (fork()) {
	case 0:		/* child */
		break;
	case -1:
		perror("rquotad: can't fork");
		exit(1);
	default:	/* parent */
		exit(0);
	}

	/*
	 * Close existing file descriptors, open "/dev/null" as
	 * standard input, output, and error, and detach from
	 * controlling terminal.
	 */
	getrlimit(RLIMIT_NOFILE, &rl);
	for (i = 0; i < rl.rlim_max; i++)
		(void) close(i);
	(void) open("/dev/null", O_RDONLY);
	(void) open("/dev/null", O_WRONLY);
	(void) dup(1);
	(void) setsid();

	openlog("rquotad", LOG_PID, LOG_DAEMON);

	/*
	 * Create datagram service
	 */
	if (svc_create(dispatch, RQUOTAPROG, RQUOTAVERS, "datagram_v") == 0) {
		syslog(LOG_ERR, "couldn't register datagram_v service");
		exit(1);
	}

	/*
	 * Start serving
	 */
	svc_run();
	syslog(LOG_ERR, "Error: svc_run shouldn't have returned");
	exit(1);
	/* NOTREACHED */
}

void
dispatch(rqstp, transp)
	register struct svc_req *rqstp;
	register SVCXPRT *transp;
{

	request_pending = 1;

	switch (rqstp->rq_proc) {
	case NULLPROC:
		errno = 0;
		if (!svc_sendreply(transp, xdr_void, 0))
			log_cant_reply(transp);
		break;

	case RQUOTAPROC_GETQUOTA:
	case RQUOTAPROC_GETACTIVEQUOTA:
		getquota(rqstp, transp);
		break;

	default:
		svcerr_noproc(transp);
		break;
	}

	request_pending = 0;
}

void
closedown()
{
	if (!request_pending) {
		int i, openfd;
		struct t_info tinfo;

		if (!t_getinfo(0, &tinfo) && (tinfo.servtype == T_CLTS))
			exit(0);

		for (i = 0, openfd = 0; i < svc_max_pollfd && openfd < 2; i++) {
			if (svc_pollfd[i].fd >= 0)
				openfd++;
		}

		if (openfd <= 1)
			exit(0);
	}
	(void) alarm(RPCSVC_CLOSEDOWN);
}

#include <sys/errno.h>

void
getquota(rqstp, transp)
	register struct svc_req *rqstp;
	register SVCXPRT *transp;
{
	struct getquota_args gqa;
	struct getquota_rslt gqr;
	struct dqblk dqblk;
	struct fsquot *fsqp;
	struct timeval tv;
	bool_t qactive;

	gqa.gqa_pathp = NULL;		/* let xdr allocate the storage */
	if (!svc_getargs(transp, xdr_getquota_args, (caddr_t)&gqa)) {
		svcerr_decode(transp);
		return;
	}
	/*
	 * This authentication is really bogus with the current rpc
	 * authentication scheme. One day we will have something for real.
	 */
	if (rqstp->rq_cred.oa_flavor != AUTH_UNIX ||
	    (((authp) rqstp->rq_clntcred)->aup_uid != 0 &&
		((authp) rqstp->rq_clntcred)->aup_uid != (uid_t)gqa.gqa_uid)) {
		gqr.status = Q_EPERM;
		goto sendreply;
	}
	fsqp = findfsq(gqa.gqa_pathp);
	if (fsqp == NULL) {
		gqr.status = Q_NOQUOTA;
		goto sendreply;
	}

	if (quotactl(Q_GETQUOTA, fsqp->fsq_dir, (uid_t)gqa.gqa_uid, &dqblk) !=
	    0) {
		qactive = FALSE;
		if ((errno == ENOENT) ||
			(rqstp->rq_proc != RQUOTAPROC_GETQUOTA)) {
			gqr.status = Q_NOQUOTA;
			goto sendreply;
		}

		/*
		 * If there is no quotas file, don't bother to sync it.
		 */
		if (errno != ENOENT) {
			if (quotactl(Q_ALLSYNC, fsqp->fsq_dir,
			    (uid_t)gqa.gqa_uid, &dqblk) < 0 &&
				errno == EINVAL)
				syslog(LOG_WARNING,
				    "Quotas are not compiled into this kernel");
			if (getdiskquota(fsqp, (uid_t)gqa.gqa_uid, &dqblk) ==
			    0) {
				gqr.status = Q_NOQUOTA;
				goto sendreply;
			}
		}
	} else {
		qactive = TRUE;
	}
	/*
	 * We send the remaining time instead of the absolute time
	 * because clock skew between machines should be much greater
	 * than rpc delay.
	 */
#define	gqrslt getquota_rslt_u.gqr_rquota

	gettimeofday(&tv, NULL);
	gqr.status = Q_OK;
	gqr.gqrslt.rq_active	= qactive;
	gqr.gqrslt.rq_bsize	= DEV_BSIZE;
	gqr.gqrslt.rq_bhardlimit = dqblk.dqb_bhardlimit;
	gqr.gqrslt.rq_bsoftlimit = dqblk.dqb_bsoftlimit;
	gqr.gqrslt.rq_curblocks = dqblk.dqb_curblocks;
	gqr.gqrslt.rq_fhardlimit = dqblk.dqb_fhardlimit;
	gqr.gqrslt.rq_fsoftlimit = dqblk.dqb_fsoftlimit;
	gqr.gqrslt.rq_curfiles	= dqblk.dqb_curfiles;
	gqr.gqrslt.rq_btimeleft	= dqblk.dqb_btimelimit - tv.tv_sec;
	gqr.gqrslt.rq_ftimeleft	= dqblk.dqb_ftimelimit - tv.tv_sec;
sendreply:
	errno = 0;
	if (!svc_sendreply(transp, xdr_getquota_rslt, (caddr_t)&gqr))
		log_cant_reply(transp);
}

quotactl(cmd, mountp, uid, dqp)
	int	cmd;
	char	*mountp;
	uid_t	uid;
	struct dqblk *dqp;
{
	int 		fd;
	int 		status;
	struct quotctl 	quota;
	char		mountpoint[256];
	FILE		*fstab;
	struct mnttab	mntp;

	if ((mountp == NULL) && (cmd == Q_ALLSYNC)) {
		/*
		 * Find the mount point of any ufs file system. this is
		 * because the ioctl that implements the quotactl call has
		 * to go to a real file, and not to the block device.
		 */
		if ((fstab = fopen(MNTTAB, "r")) == NULL) {
			syslog(LOG_ERR, "can not open %s: %m ", MNTTAB);
			return (-1);
		}
		fd = -1;
		while ((status = getmntent(fstab, &mntp)) == NULL) {
			if (strcmp(mntp.mnt_fstype, MNTTYPE_UFS) != 0 ||
				!(hasmntopt(&mntp, MNTOPT_RQ) ||
				hasmntopt(&mntp, MNTOPT_QUOTA)))
				continue;
			(void) strcpy(mountpoint, mntp.mnt_mountp);
			strcat(mountpoint, "/quotas");
			if ((fd = open(mountpoint, O_RDWR)) >= 0)
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

		if ((fd = open(mountpoint, O_RDONLY)) < 0) {
			errno = ENOENT;
			syslog(LOG_ERR, "can not open %s: %m ", mountpoint);
			return (-1);
		}
	}
	quota.op = cmd;
	quota.uid = uid;
	quota.addr = (caddr_t)dqp;

	status = ioctl(fd, Q_QUOTACTL, &quota);

	close(fd);
	return (status);
}

/*
 * Return the quota information for the given path.  Returns NULL if none
 * was found.
 */

struct fsquot *
findfsq(dir)
	char *dir;
{
	struct stat sb;
	register struct fsquot *fsqp;
	static time_t lastmtime = 0; 	/* mount table's previous mtime */

	/*
	 * If we've never looked at the mount table, or it has changed
	 * since the last time, rebuild the list of quota'd file systems
	 * and remember the current mod time for the mount table.
	 */

	if (stat(MNTTAB, &sb) < 0) {
		syslog(LOG_ERR, "can't stat %s: %m", MNTTAB);
		return (NULL);
	}
	if (lastmtime == 0 || sb.st_mtime != lastmtime) {
		freefs();
		setupfs();
		lastmtime = sb.st_mtime;
	}

	/*
	 * Try to find the given path in the list of file systems with
	 * quotas.
	 */

	if (fsqlist == NULL)
		return (NULL);
	if (stat(dir, &sb) < 0)
		return (NULL);
	for (fsqp = fsqlist; fsqp != NULL; fsqp = fsqp->fsq_next) {
		if (sb.st_dev == fsqp->fsq_dev)
			return (fsqp);
	}
	return (NULL);
}

void
setupfs()
{
	register struct fsquot *fsqp;
	FILE *mt;
	struct mnttab m;
	struct stat sb;
	char qfilename[MAXPATHLEN];

	mt = fopen(MNTTAB, "r");
	if (mt == NULL) {
		syslog(LOG_ERR, "can't read %s: %m", MNTTAB);
		return;
	}

	while (getmntent(mt, &m) == 0) {
		if (strcmp(m.mnt_fstype, MNTTYPE_UFS) != 0)
			continue;
		if (!hasquota(m.mnt_mntopts)) {
			sprintf(qfilename, "%s/%s", m.mnt_mountp, QFNAME);
			if (stat(qfilename, &sb) < 0)
				continue;
		}
		if (stat(m.mnt_special, &sb) < 0 ||
		    (sb.st_mode & S_IFMT) != S_IFBLK)
			continue;
		fsqp = (struct fsquot *)malloc(sizeof (struct fsquot));
		if (fsqp == NULL) {
			syslog(LOG_ERR, "out of memory");
			exit(1);
		}
		fsqp->fsq_next = fsqlist;
		fsqp->fsq_dir = (char *)malloc(strlen(m.mnt_mountp) + 1);
		fsqp->fsq_devname = (char *)malloc(strlen(m.mnt_special) + 1);
		if (fsqp->fsq_dir == NULL || fsqp->fsq_devname == NULL) {
			syslog(LOG_ERR, "out of memory");
			exit(1);
		}
		strcpy(fsqp->fsq_dir, m.mnt_mountp);
		strcpy(fsqp->fsq_devname, m.mnt_special);
		fsqp->fsq_dev = sb.st_rdev;
		fsqlist = fsqp;
	}
	(void) fclose(mt);
}

/*
 * Free the memory used by the current list of quota'd file systems.  Nulls
 * out the list.
 */

void
freefs()
{
	register struct fsquot *fsqp;

	while ((fsqp = fsqlist) != NULL) {
		fsqlist = fsqp->fsq_next;
		free(fsqp->fsq_dir);
		free(fsqp->fsq_devname);
		free(fsqp);
	}
}

int
getdiskquota(fsqp, uid, dqp)
	struct fsquot *fsqp;
	uid_t uid;
	struct dqblk *dqp;
{
	int fd;
	char qfilename[MAXPATHLEN];

	sprintf(qfilename, "%s/%s", fsqp->fsq_dir, QFNAME);
	if ((fd = open(qfilename, O_RDONLY)) < 0)
		return (0);
	lseek(fd, (long)dqoff(uid), L_SET);
	if (read(fd, dqp, sizeof (struct dqblk)) != sizeof (struct dqblk)) {
		close(fd);
		return (0);
	}
	close(fd);
	if (dqp->dqb_bhardlimit == 0 && dqp->dqb_bsoftlimit == 0 &&
	    dqp->dqb_fhardlimit == 0 && dqp->dqb_fsoftlimit == 0) {
		return (0);
	}
	return (1);
}

/*
 * Get the client's hostname from the transport handle
 * If the name is not available then return "(anon)".
 */
struct nd_hostservlist *
getclientsnames(transp)
	SVCXPRT *transp;
{
	struct netbuf *nbuf;
	struct netconfig *nconf;
	static struct nd_hostservlist	*serv;
	static struct nd_hostservlist	anon_hsl;
	static struct nd_hostserv	anon_hs;
	static char anon_hname[] = "(anon)";
	static char anon_sname[] = "";

	/* Set up anonymous client */
	anon_hs.h_host = anon_hname;
	anon_hs.h_serv = anon_sname;
	anon_hsl.h_cnt = 1;
	anon_hsl.h_hostservs = &anon_hs;

	if (serv) {
		netdir_free((char *)serv, ND_HOSTSERVLIST);
		serv = NULL;
	}
	nconf = getnetconfigent(transp->xp_netid);
	if (nconf == NULL) {
		syslog(LOG_ERR, "%s: getnetconfigent failed",
			transp->xp_netid);
		return (&anon_hsl);
	}

	nbuf = svc_getrpccaller(transp);
	if (nbuf == NULL) {
		freenetconfigent(nconf);
		return (&anon_hsl);
	}
	if (netdir_getbyaddr(nconf, &serv, nbuf)) {
		freenetconfigent(nconf);
		return (&anon_hsl);
	}
	freenetconfigent(nconf);
	return (serv);
}

void
log_cant_reply(transp)
	SVCXPRT *transp;
{
	int saverrno;
	struct nd_hostservlist *clnames;
	register char *name;

	saverrno = errno;	/* save error code */
	clnames = getclientsnames(transp);
	if (clnames == NULL)
		return;
	name = clnames->h_hostservs->h_host;

	errno = saverrno;
	if (errno == 0)
		syslog(LOG_ERR, "couldn't send reply to %s", name);
	else
		syslog(LOG_ERR, "couldn't send reply to %s: %m", name);
}

char *mntopts[] = { MNTOPT_QUOTA, NULL };
#define	QUOTA    0

/*
 * Return 1 if "quota" appears in the options string
 */
int
hasquota(opts)
	char *opts;
{
	char *value;

	if (opts == NULL)
		return (0);
	while (*opts != '\0') {
		if (getsubopt(&opts, mntopts, &value) == QUOTA)
			return (1);
	}

	return (0);
}
