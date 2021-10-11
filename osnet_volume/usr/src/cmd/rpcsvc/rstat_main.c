/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)rstat_main.c 1.5	96/05/10 SMI"

#include "rstat.h"
#include "rstat_v2.h"
#include <stdio.h>
#include <stdlib.h> /* getenv, exit */
#include <signal.h>
#include <sys/types.h>
#include <memory.h>
#include <stropts.h>
#include <netconfig.h>
#include <sys/resource.h> /* rlimit */
#include <syslog.h>

#ifdef __STDC__
#define	SIG_PF void(*)(int)
#endif

#ifdef DEBUG
#define	RPC_SVC_FG
#endif


int _rpcpmstart;		/* Started by a port monitor ? */
int _rpcfdtype;			/* Whether Stream or Datagram ? */
int _rpcsvcdirty;		/* Still serving ? */

static void _msgout(/*char *msg*/);
static void closedown();

extern void rstatprog_4(/*struct svc_req *rqstp, SVCXPRT *transp*/);
extern void rstatprog_3(/*struct svc_req *rqstp, SVCXPRT *transp*/);
extern void rstatprog_2(/*struct svc_req *rqstp, SVCXPRT *transp*/);

main()
{
	pid_t pid;
	int i;

	/*
	 * If stdin looks like a TLI endpoint, we assume
	 * that we were started by a port monitor. If
	 * t_getstate fails with TBADF, this is not a
	 * TLI endpoint.
	 */
	if (t_getstate(0) != -1 || t_errno != TBADF) {
		char *netid;
		struct netconfig *nconf = NULL;
		SVCXPRT *transp;

		_rpcpmstart = 1;
		openlog("rstatd", LOG_PID, LOG_DAEMON);
		if ((netid = getenv("NLSPROVIDER")) == NULL) {
#ifdef DEBUG
			_msgout("cannot get transport name");
#endif
		} else if ((nconf = getnetconfigent(netid)) == NULL) {
#ifdef DEBUG
			_msgout("cannot get transport info");
#endif
		}
		if ((transp = svc_tli_create(0, nconf, NULL, 0, 0)) == NULL) {
			_msgout("cannot create server handle");
			exit(1);
		}
		if (nconf)
			freenetconfigent(nconf);
		if (!svc_reg(transp, RSTATPROG, RSTATVERS_VAR, rstatprog_4,
		    0)) {
    _msgout("unable to register (RSTATPROG, RSTATVERS_VAR).");
			exit(1);
		}
		if (!svc_reg(transp, RSTATPROG, RSTATVERS_TIME, rstatprog_3,
		    0)) {
    _msgout("unable to register (RSTATPROG, RSTATVERS_TIME).");
			exit(1);
		}
		if (!svc_reg(transp, RSTATPROG, RSTATVERS_SWTCH, rstatprog_2,
		    0)) {
    _msgout("unable to register (RSTATPROG, RSTATVERS_SWTCH).");
			exit(1);
		}
		svc_run();
		exit(1);
		/* NOTREACHED */
	}
	else {
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
		openlog("rstatd", LOG_PID, LOG_DAEMON);
#endif
	}
	if (!svc_create(rstatprog_4, RSTATPROG, RSTATVERS_VAR, "datagram_v")) {
    _msgout("unable to create (RSTATPROG, RSTATVERS_VAR) for datagram_v.");
		exit(1);
	}
	if (!svc_create(rstatprog_3, RSTATPROG, RSTATVERS_TIME,
	    "datagram_v")) {
    _msgout("unable to create (RSTATPROG, RSTATVERS_TIME) for datagram_v.");
		exit(1);
	}
	if (!svc_create(rstatprog_4, RSTATPROG, RSTATVERS_VAR, "circuit_v")) {
    _msgout("unable to create (RSTATPROG, RSTATVERS_VAR) for circuit_v.");
		exit(1);
	}
	if (!svc_create(rstatprog_3, RSTATPROG, RSTATVERS_TIME, "circuit_v")) {
    _msgout("unable to create (RSTATPROG, RSTATVERS_TIME) for circuit_v.");
		exit(1);
	}

	/*
	 * V2 supported on datagram transports *only*
	 */
	if (!svc_create(rstatprog_2, RSTATPROG, RSTATVERS_SWTCH,
	    "datagram_v")) {
    _msgout("unable to create (RSTATPROG, RSTATVERS_SWTCH) for datagram_v.");
		exit(1);
	}

	svc_run();
	_msgout("svc_run returned");
	exit(1);
	/* NOTREACHED */
}

static
void _msgout(msg)
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
