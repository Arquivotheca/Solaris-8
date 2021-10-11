/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_daemon_main.c	1.22	98/08/30 SMI"

#include <stdio.h>
#include <stdlib.h> /* getenv, exit */
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <memory.h>
#include <stropts.h>
#include <netconfig.h>
#include <sys/resource.h> /* rlimit */
#include <syslog.h>

#include "dr_subr.h"

#ifdef DEBUG
#define	RPC_SVC_FG
#endif

#define	_RPCSVC_CLOSEDOWN 120
/*
 * Normally if you rpcgen -K -1, then main of the server will be created
 * to never exit, however since we have our own main we have to manually
 * comment out the code (closedown) used to close the server down after
 * some quiet period.
 */
#define	DR_DAEMON_NEVEREXIT

int _rpcpmstart;		/* Started by a port monitor ? */
	/* States a server can be in wrt request */

#define	_IDLE 0
#define	_SERVED 1
#define	_SERVING 2
#define	DR_AP_GID 1

static int _rpcsvcstate = _IDLE;	 /* Set when a request is serviced */

/*
 * noapdaemon may be used to disable communication with the
 * ap_daemon.  By default it is enabled.  The -a flag may be used to
 * disable it.
 */
int noapdaemon = 0;

#ifdef	_XFIRE
extern void drprog_4();
#endif

static void
_msgout(msg)
	char *msg;
{
#ifdef RPC_SVC_FG
	if (_rpcpmstart)
		syslog(LOG_ERR, msg);
	else
		(void) fprintf(stderr, "%s\n", msg);
#else
	syslog(LOG_ERR, msg);
#endif
}

/* ARGSUSED */
static void
closedown(sig)
	int sig;
{
	if (_rpcsvcstate == _IDLE) {
		extern fd_set svc_fdset;
		static int size;
		int i, openfd;
		struct t_info tinfo;

		if (!t_getinfo(0, &tinfo) && (tinfo.servtype == T_CLTS))
			exit(0);
		if (size == 0) {
			struct rlimit rl;

			rl.rlim_max = 0;
			getrlimit(RLIMIT_NOFILE, &rl);
			if ((size = rl.rlim_max) == 0) {
				return;
			}
		}
		for (i = 0, openfd = 0; i < size && openfd < 2; i++)
			if (FD_ISSET(i, &svc_fdset))
				openfd++;
		if (openfd <= 1)
			exit(0);
	}
	if (_rpcsvcstate == _SERVED)
		_rpcsvcstate = _IDLE;

	(void) signal(SIGALRM, (void(*)()) closedown);
	(void) alarm(_RPCSVC_CLOSEDOWN/2);
}

/*
 * argscan
 *
 * Check for verbose flag or testconfig file.
 */
static void
argscan(int argc, char *argv[])
{
	int c;
	extern char *optarg;
#ifdef DR_TEST_CONFIG
	extern int optind;
#endif DR_TEST_CONFIG

#ifdef DR_TEST_VECTOR
	while ((c = getopt(argc, argv, "apsv:f:")) != EOF) {
#else
	while ((c = getopt(argc, argv, "apsv:")) != EOF) {
#endif DR_TEST_VECTOR
		switch (c) {
		case 'a':
			noapdaemon = 1;
			break;
		case 'p':
			no_net_unplumb = 1;
			break;
		case 's':
			no_smtd_kill = 1;
			break;
		case 'v':
			verbose = atol(optarg);
			break;
#ifdef DR_TEST_VECTOR
		case 'f':
			dr_fail_op = atol(optarg);
			if (dr_fail_op <= 0) {
				_msgout("-f arg must be > 0");
				exit(-1);
			}
			break;
#endif DR_TEST_VECTOR
		}
	}

#ifdef DR_TEST_CONFIG
	if (optind < argc) {
		syslog(LOG_ERR, "processing config file [%s]", argv[optind]);
		if (process_dr_config(argv[1]))
			syslog(LOG_ERR, "config file [%s] error", argv[optind]);
	} else {
		syslog(LOG_ERR, "no config file");
	}
#endif DR_TEST_CONFIG
}

void
main(int argc, char *argv[])
{
	pid_t pid;
	int i;
	char mname[FMNAMESZ + 1];

	if (getuid() != 0) {
		_msgout("permission denied");
		exit(1);
	}

	/*
	 * Check for a verbose flag which overrides the
	 * default verbosity level.
	 */
	if (argc > 1) {
		argscan(argc, argv);
	}

	if (!ioctl(0, I_LOOK, mname) &&
		(!(strcmp(mname, "sockmod")) || !(strcmp(mname, "timod")))) {
		struct netconfig *nconf = NULL;
		SVCXPRT *transp;
#ifndef DR_DAEMON_NEVEREXIT
		int pmclose;
		char *netid;
#endif
		_rpcpmstart = 1;
		openlog("dr_daemon", LOG_PID, LOG_DAEMON);

#ifndef DR_DAEMON_NEVEREXIT
		if ((netid = getenv("NLSPROVIDER")) == NULL) {
		/* started from inetd */
			pmclose = 1;
		} else {
			if ((nconf = getnetconfigent(netid)) == NULL)
				_msgout("cannot get transport info");

			pmclose = (t_getstate(0) != T_DATAXFER);
		}
#endif DR_DAEMON_NEVEREXIT
		if (strcmp(mname, "sockmod") == 0) {
			if (ioctl(0, I_POP, 0) || ioctl(0, I_PUSH, "timod")) {
				_msgout("could not get the right module");
				exit(1);
			}
		}
		if ((transp = svc_tli_create(0, nconf, NULL, 0, 0)) == NULL) {
			_msgout("cannot create server handle");
			exit(1);
		}
		if (nconf)
			freenetconfigent(nconf);
#ifdef	_XFIRE
		if (!svc_reg(transp, DRPROG, DRVERS, drprog_4, 0)) {
#endif
			_msgout("unable to register (DRPROG, DRVERS).");
			exit(1);
		}
#ifndef DR_DAEMON_NEVEREXIT
		if (pmclose) {
			(void) signal(SIGALRM, (void(*)()) closedown);
			(void) alarm(_RPCSVC_CLOSEDOWN/2);
		}
#endif DR_DAEMON_NEVEREXIT


		/*
		 * If AP communication is enabled (no -a flag), initialize
		 * the AP interaction code.  We place the call here before
		 * we start accepting RPC requests but after we've setup the
		 * syslog() reporting parameters.
		 */
		if (!noapdaemon) {
#ifdef	AP
			if (setgid(DR_AP_GID) == -1)
				perror("cannot set gid for AP interaction");
			else
#endif	AP
				dr_ap_init();
		}

		hash_drv_aliases();
		dr_init_sysmap();

		svc_run();
		exit(1);
		/* NOTREACHED */
	}	else {
#ifndef RPC_SVC_FG
		int size;
		struct rlimit rl;
		pid = fork();
		if (pid < 0) {
			perror("cannot fork");
			exit(1);
		}
		if (pid)
			exit(0);
		rl.rlim_max = 0;
		getrlimit(RLIMIT_NOFILE, &rl);
		if ((size = rl.rlim_max) == 0)
			exit(1);
		for (i = 0; i < size; i++)
			(void) close(i);
		i = open("/dev/console", 2);
		(void) dup2(i, 1);
		(void) dup2(i, 2);
		setsid();
		openlog("dr_daemon", LOG_PID, LOG_DAEMON);
#endif
	}
#ifdef	_XFIRE
	if (!svc_create(drprog_4, DRPROG, DRVERS, "netpath")) {
#endif
		_msgout("unable to create (DRPROG, DRVERS) for netpath.");
		exit(1);
	}

	/*
	 * If AP communication is enabled (no -a flag), initialize
	 * the AP interaction code. We place the call here before
	 * we start accepting RPC requests but after we've setup the
	 * syslog() reporting parameters.
	 */
	if (!noapdaemon) {
		dr_ap_init();
	}

	hash_drv_aliases();
	dr_init_sysmap();

	svc_run();
	_msgout("svc_run returned");
	exit(1);
	/* NOTREACHED */
}
