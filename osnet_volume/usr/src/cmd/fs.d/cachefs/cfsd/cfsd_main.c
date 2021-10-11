/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cfsd_main.c	1.11	99/12/08 SMI"

/*
 * -----------------------------------------------------------------
 *			cfsd_main.c
 *
 * Main routines for cachefs daemon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h> /* for pmap_unset */
#include <string.h> /* strcmp */
#include <signal.h>
#include <unistd.h> /* setsid */
#include <sys/types.h>
#include <memory.h>
#include <stropts.h>
#include <netconfig.h>
#include <libintl.h>
#include <locale.h>
#include <thread.h>
#include <sys/resource.h> /* rlimit */
#include <synch.h>
#include <mdbug/mdbug.h>
#include <common/cachefsd.h>
#include <sys/fs/cachefs_fs.h>
#include <sys/fs/cachefs_dlog.h>
#include <sys/fs/cachefs_ioctl.h>
#include "cfsd.h"
#include "cfsd_kmod.h"
#include "cfsd_maptbl.h"
#include "cfsd_logfile.h"
#include "cfsd_fscache.h"
#include "cfsd_cache.h"
#include "cfsd_all.h"
#include "cfsd_subr.h"

#define	RPCGEN_ACTION(X) X
#include "cachefsd_tbl.i"

#ifndef SIG_PF
#define	SIG_PF void(*)(int)
#endif

typedef bool_t (* LOCAL)(void *, void *, struct svc_req *);

/* global definitions */
cfsd_all_object_t *all_object_p = NULL;

/* forward references */
void msgout(char *msgp);
void cachefsdprog_1(struct svc_req *rqstp, register SVCXPRT *transp);
void sigusr1_handler(int, siginfo_t *, void *);

/*
 * -----------------------------------------------------------------
 *			main
 *
 * Description:
 *	main routine for the chart daemon.
 * Arguments:
 *	argc
 *	argv
 * Returns:
 *	Returns 0 for a normal exit, !0 if an error occurred.
 * Preconditions:
 *	precond(argv)
 */

int
main(int argc, char **argv)
{
	pid_t pid;
	int xx;
	char mname[FMNAMESZ + 1];
	int opt_fork = 0;
	int opt_mt = 0;
	char *opt_root = NULL;
	int c;
	char *msgp;
	int size;
	struct rlimit rl;
	struct sigaction nact;
	int ofd = -1;
#if 0
	sigset_t nset;
	int pmclose;
#endif
	char *netid;
	struct netconfig *nconf = NULL;
	SVCXPRT *transp;
	cfsd_fscache_object_t *fscache_object_p;
	int mode;
	/* selectable maximum RPC request record size */
	int maxrecsz = RPC_MAXDATASIZE;

	dbug_enter("main");
	dbug_process("cfsadmin");

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	/* verify root */
	if (getuid() != 0) {
		fprintf(stderr, gettext("%s: must be run by root\n"), argv[0]);
		dbug_leave("main");
		return (1);
	}

	/* Increase number of file descriptors to maximum allowable */
	xx = getrlimit(RLIMIT_NOFILE, &rl);
	if (xx < 0) {
		dbug_print(("error",
		    "getrlimit/RLIMIT_NOFILE failed %d", errno));
		dbug_leave("main");
		return (1);
	}
	rl.rlim_cur = rl.rlim_max;
	xx = setrlimit(RLIMIT_NOFILE, &rl);
	if (xx < 0) {
		dbug_print(("error",
		    "setrlimit/RLIMIT_NOFILE failed %d", errno));
		dbug_leave("main");
		return (1);
	}

	while ((c = getopt(argc, argv, "fmr:#:")) != EOF) {
		switch (c) {
		case 'f':
			opt_fork = 1;
			break;

		case 'm':
			/*
			 * XXX don't use this until race between mount
			 * and umount is fixed.
			 */
			opt_mt = 1;
			break;

		case 'r':
			opt_root = optarg;
			break;

		case '#':	/* dbug args */
			msgp = dbug_push(optarg);
			if (msgp) {
				printf("dbug_push failed \"%s\"\n", msgp);
				dbug_leave("main");
				return (1);
			}
			ofd = db_getfd();
			break;

		default:
			printf(gettext("illegal switch\n"));
			dbug_leave("main");
			return (1);
		}
	}

	/* XXX need some way to prevent multiple daemons from running */

	dbug_print(("info", "cachefsd started..."));

	if (opt_mt) {
		dbug_print(("info", "MT_AUTO mode set"));
		mode = RPC_SVC_MT_AUTO;
		if (!rpc_control(RPC_SVC_MTMODE_SET, &mode)) {
			msgout(gettext("unable to set automatic MT mode."));
			dbug_leave("main");
			return (1);
		}
	}

	/* Enable non-blocking mode and maximum record size checks for
	 * connection oriented transports.
	 */
	if (!rpc_control(RPC_SVC_CONNMAXREC_SET, &maxrecsz)) {
		msgout(gettext("unable to set max RPC record size"));
	}
		
#if 0
	/* XXX change to sigaction */
	(void) sigset(SIGPIPE, SIG_IGN);
	(void) sigset(SIGUSR1, sigusr1_handler);

#else
	/* ignore sigpipe */
	nact.sa_handler = SIG_IGN;
	nact.sa_sigaction = NULL;
	sigemptyset(&nact.sa_mask);
	nact.sa_flags = 0;
	xx = sigaction(SIGPIPE, &nact, NULL);
	if (xx) {
		dbug_print(("error", "sigaction/SIGPIPE failed %d", errno));
	}

	/* catch sigusr1 signals, used to wake up threads */
	nact.sa_handler = NULL;
	nact.sa_sigaction = sigusr1_handler;
	sigemptyset(&nact.sa_mask);
	nact.sa_flags = SA_SIGINFO;
	xx = sigaction(SIGUSR1, &nact, NULL);
	if (xx) {
		dbug_print(("error", "sigaction failed %d", errno));
	}
#if 0
	/* sigset() does this, but we don't need it */
	sigemptyset(&nset);
	xx = sigaddset(&nset, SIGUSR1);
	if (xx == 0)
		xx = sigprocmask(SIG_UNBLOCK, &nset, NULL);
	if (xx) {
		dbug_print(("error", "sigprocmask failed %d", errno));
	}
#endif
#endif

	/* do not set up rpc services if just taking care of root */
	if (opt_root) {
		dbug_print(("info", "handling just root"));

		/* make the fscache object */
		fscache_object_p =
		    cfsd_fscache_create("rootcache", opt_root, 1);

		/* init the fscache object with mount information */
		fscache_lock(fscache_object_p);
		fscache_object_p->i_refcnt++;
		fscache_setup(fscache_object_p);
		fscache_object_p->i_mounted = 1;
		fscache_unlock(fscache_object_p);

		if (fscache_object_p->i_disconnectable &&
		    fscache_object_p->i_mounted) {
			pid = fork();
			if (pid < 0) {
				perror(gettext("cannot fork"));
				cfsd_fscache_destroy(fscache_object_p);
				dbug_leave("main");
				return (1);
			}
			if (pid) {
				cfsd_fscache_destroy(fscache_object_p);
				dbug_leave("main");
				return (0);
			}
			rl.rlim_max = 0;
			getrlimit(RLIMIT_NOFILE, &rl);
			if ((size = rl.rlim_max) == 0) {
				cfsd_fscache_destroy(fscache_object_p);
				dbug_leave("main");
				return (1);
			}
			for (xx = 0; xx < size; xx++) {
				if (xx != ofd)
				    if (close(xx))
					dbug_print(("err",
					    "cannot close fd %d, %d",
					    xx, errno));
			}
			xx = open("/dev/console", O_RDWR);
			(void) dup2(xx, 1);
			(void) dup2(xx, 2);
			setsid();

			fscache_process(fscache_object_p);
		} else {
			/* not disconnectable */
			cfsd_fscache_destroy(fscache_object_p);
			dbug_leave("main");
			return (1);
		}
		cfsd_fscache_destroy(fscache_object_p);
		dbug_leave("main");
		return (0);
	}

	/* if a inetd started us */
	else if (!ioctl(0, I_LOOK, mname) &&
		((strcmp(mname, "sockmod") == 0) ||
		(strcmp(mname, "timod") == 0))) {
		dbug_print(("info", "started by inetd"));

		if ((netid = getenv("NLSPROVIDER")) != NULL) {
			if ((nconf = getnetconfigent(netid)) == NULL)
				msgout(gettext("cannot get transport info"));
#if 0
			pmclose = (t_getstate(0) != T_DATAXFER);
#endif
		}
#if 0
		/* started from inetd */
		else {
			pmclose = 1;
		}
#endif
		if (strcmp(mname, "sockmod") == 0) {
			if (ioctl(0, I_POP, 0) || ioctl(0, I_PUSH, "timod")) {
				msgout(
				    gettext("could not get the right module"));
				dbug_leave("main");
				return (1);
			}
		}
		if ((transp = svc_tli_create(0, nconf, NULL, 0, 0)) == NULL) {
			msgout(gettext("cannot create server handle"));
			dbug_leave("main");
			return (1);
		}
		if (nconf)
			freenetconfigent(nconf);
		xx = svc_reg(transp, CACHEFSDPROG, CACHEFSDVERS,
			cachefsdprog_1, 0);
		if (!xx) {
			msgout(gettext(
			    "unable to reg (CACHEFSDPROG, CACHEFSDVERS)."));
			dbug_leave("main");
			return (1);
		}
#if 0
		if (pmclose) {
			(void) signal(SIGALRM, (SIG_PF) closedown);
			(void) alarm(_RPCSVC_CLOSEDOWN/2);
		}
#endif
	}

	/* else if started by hand */
	else {
		/* if we should fork */
		if (opt_fork) {
			dbug_print(("info", "forking"));
			pid = fork();
			if (pid < 0) {
				perror(gettext("cannot fork"));
				dbug_leave("main");
				return (1);
			}
			if (pid) {
				dbug_leave("main");
				return (0);
			}
			rl.rlim_max = 0;
			getrlimit(RLIMIT_NOFILE, &rl);
			if ((size = rl.rlim_max) == 0) {
				dbug_leave("main");
				return (1);
			}
			for (xx = 0; xx < size; xx++) {
				if (xx != ofd)
				    if (close(xx))
					dbug_print(("err",
					    "cannot close fd %d, %d",
					    xx, errno));
			}
			xx = open("/dev/console", 2);
			(void) dup2(xx, 1);
			(void) dup2(xx, 2);
			setsid();
		}

		xx = svc_create(cachefsdprog_1, CACHEFSDPROG, CACHEFSDVERS,
		    "tcp");
#if 0
		xx = svc_create(cachefsdprog_1, CACHEFSDPROG, CACHEFSDVERS,
		    "netpath");
#endif
		if (!xx) {
			msgout(gettext("unable to create (CACHEFSDPROG, "
				"CACHEFSDVERS for netpath."));
			dbug_leave("main");
			return (1);
		}
	}

	/* find existing caches and mounted file systems */
	all_object_p = cfsd_all_create();
	subr_cache_setup(all_object_p);

	/* process requests */
	svc_run();

	msgout(gettext("svc_run returned"));
	cfsd_all_destroy(all_object_p);
	dbug_leave("main");
	return (1);
}


/*
 * -----------------------------------------------------------------
 *			msgout
 *
 * Description:
 *	Outputs an error message to stderr.
 * Arguments:
 *	msgp
 * Returns:
 * Preconditions:
 *	precond(msgp)
 */

void
msgout(char *msgp)
{
	dbug_enter("msgout");
	dbug_precond(msgp);

	(void) fprintf(stderr, "%s\n", msgp);
	dbug_leave("msgout");
}

/*
 * -----------------------------------------------------------------
 *			closedown
 *
 * Description:
 * Arguments:
 *	sig
 * Returns:
 * Preconditions:
 */
/* XXX bob: need to shut down daemon if no requests after 5 minutes */
/* and no chart mounts */
#if 0
void
closedown(int sig)
{
	mutex_lock(&_svcstate_lock);
	if (_rpcsvcstate == _IDLE) {
		int size;
		int i, openfd;
		struct t_info tinfo;

		if (!t_getinfo(0, &tinfo) && (tinfo.servtype == T_CLTS))
			exit(0);

		size = svc_max_pollfd;
		for (i = 0, openfd = 0; i < size && openfd < 2; i++) {
			if (svc_pollfd[i].fd >= 0) {
				openfd++;
			}
		}

		if (openfd <= 1) {
			msgout(gettext("daemon exiting"));
			exit(0);
		}
	}
	if (_rpcsvcstate == _SERVED)
		_rpcsvcstate = _IDLE;

	mutex_unlock(&_svcstate_lock);
	(void) signal(SIGALRM, (SIG_PF) closedown);
	(void) alarm(_RPCSVC_CLOSEDOWN/2);
}
#endif

/*
 * -----------------------------------------------------------------
 *			cachefsdprog_1
 *
 * Description:
 * Arguments:
 *	rqstp
 *	transp
 * Returns:
 * Preconditions:
 *	precond(rqstp)
 *	precond(transp)
 */
void
cachefsdprog_1(struct svc_req *rqstp, register SVCXPRT *transp)
{
	int index;
	struct rpcgen_table *rtp;
	void *argumentp = NULL;
	void *resultp = NULL;
	LOCAL local;
	int xx;

	dbug_enter("cachefsdprog_1");

	dbug_precond(rqstp);
	dbug_precond(transp);

	/* make sure a valid command number */
	index = rqstp->rq_proc;
	if ((index < 0) || (cachefsdprog_1_nproc <= index)) {
		msgout(gettext("bad message"));
		svcerr_noproc(transp);
		dbug_leave("cachefsdprog_1");
		return;
	}

	/* get command information */
	rtp = &cachefsdprog_1_table[index];

	/* get memory for the arguments */
	if (rtp->len_arg != 0)
		argumentp = (void *)cfsd_calloc(rtp->len_arg);

	/* get memory for the results */
	if (rtp->len_res != 0)
		resultp = (void *)cfsd_calloc(rtp->len_res);

	/* get the arguments */
	if (rtp->xdr_arg && argumentp) {
		if (!svc_getargs(transp, rtp->xdr_arg, (caddr_t)argumentp)) {
			svcerr_decode(transp);
			cfsd_free(argumentp);
			cfsd_free(resultp);
			dbug_leave("cachefsdprog_1");
			return;
		}
	}

	/* call the routine to process the command */
	local = (LOCAL)rtp->proc;
	xx = (*local)(argumentp, resultp, rqstp);

	/* if the command could not be processed */
	if (xx == 0) {
		svcerr_systemerr(transp);
	}

	/* else send the results back to the caller */
	else {
		xx = svc_sendreply(transp, rtp->xdr_res, (caddr_t)resultp);
		if (!xx)
			svcerr_systemerr(transp);

		/* free the results */
		xx = cachefsdprog_1_freeresult(transp, rtp->xdr_res,
		    (caddr_t)resultp);
		if (xx == 0)
			msgout(gettext("unable to free results"));
	}

	/* free the passed in arguments */
	if (!svc_freeargs(transp, rtp->xdr_arg, (caddr_t)argumentp)) {
		msgout(gettext("unable to free arguments"));
		abort();
	}

	if (argumentp)
		cfsd_free(argumentp);
	if (resultp)
		cfsd_free(resultp);
	dbug_leave("cachefsdprog_1");
}

/*
 *			sigusr1_handler
 *
 * Description:
 *	Catches sigusr1 signal so threads wake up.
 * Arguments:
 * Returns:
 * Preconditions:
 */
void
sigusr1_handler(int sig, siginfo_t *sp, void *vp)
{
#if 0
	char buffy[BUFSIZ];
	sprintf(buffy, "thread %d, handle sigusr1\n", thr_self());
	write(2, buffy, strlen(buffy));
#endif
}
