/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootparam_prot_svc.c	1.9	99/08/24 SMI"


#include <stdio.h>
#include <signal.h>
#include <rpc/rpc.h>
#include <memory.h>
#include <stropts.h>
#include <netconfig.h>
#include <stropts.h>
#include <sys/resource.h>
#include <sys/termios.h>
#include <syslog.h>
#include <rpcsvc/bootparam_prot.h>

#define	_RPCSVC_CLOSEDOWN 120

int debug = 0;

static void background();
static void bootparamprog_1();
void msgout();
static void closedown();

static int server_child = 0;	/* program was started by another server */
static int _rpcfdtype;		/* Whether Stream or Datagram ? */
static int _rpcsvcdirty;	/* Still serving ? */

main(argc, argv)
	int argc;
	char *argv[];
{
	pid_t pid;
	int i;
	struct rlimit rl;
	extern char *optarg;
	extern int optind;
	int c;
	char *progname = argv[0];
	int connmaxrec = RPC_MAXDATASIZE;

	while ((c = getopt(argc, argv, "d")) != -1)
		switch ((char)c) {
		case 'd':
			debug++;
			break;
		default:
			fprintf(stderr, "usage: %s [-d]\n", progname);
			exit(1);
		}


	/*
	 * Set non-blocking mode and maximum record size for
	 * connection oriented RPC transports.
	 */
	if (!rpc_control(RPC_SVC_CONNMAXREC_SET, &connmaxrec)) {
		msgout("unable to set maximum RPC record size");
	}

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
		int pmclose;
		extern char *getenv();

		if ((netid = getenv("NLSPROVIDER")) == NULL) {
			if (debug)
				msgout("cannot get transport name");
		} else if ((nconf = getnetconfigent(netid)) == NULL) {
			if (debug)
				msgout("cannot get transport info");
		}
		pmclose = (t_getstate(0) != T_DATAXFER);
		if ((transp = svc_tli_create(0, nconf, NULL, 0, 0)) == NULL) {
			msgout("cannot create server handle");
			exit(1);
		}
		if (nconf)
			freenetconfigent(nconf);
		if (!svc_reg(transp, BOOTPARAMPROG, BOOTPARAMVERS,
							bootparamprog_1, 0)) {
			msgout(
"unable to register (BOOTPARAMPROG, BOOTPARAMVERS).");
			exit(1);
		}
		if (pmclose) {
			(void) signal(SIGALRM, closedown);
			(void) alarm(_RPCSVC_CLOSEDOWN);
		}

		svc_run();
		exit(1);
		/* NOTREACHED */
	}

	/*
	 * run this process in the background only if it was started from
	 * a shell and the debug flag was not given.
	 */
	if (!server_child && !debug) {
		pid = fork();
		if (pid < 0) {
			perror("cannot fork");
			exit(1);
		}
		if (pid)
			exit(0);

		rl.rlim_max = 0;
		getrlimit(RLIMIT_NOFILE, &rl);
		if (rl.rlim_max)
			for (i = 0; i < rl.rlim_max; i++)
				(void) close(i);

		setsid();
	}

	/*
	 * messges go to syslog if the program was started by
	 * another server, or if it was run from the command line without
	 * the debug flag.
	 */
	if (server_child || !debug)
		openlog("bootparam_prot", LOG_PID, LOG_DAEMON);

	if (debug) {
		if (debug == 1)
			msgout("in debug mode.");
		else
			msgout("in debug mode (level %d).", debug);
	}

	if (!svc_create(bootparamprog_1, BOOTPARAMPROG, BOOTPARAMVERS,
			"netpath")) {
		msgout(
"unable to create (BOOTPARAMPROG, BOOTPARAMVERS) for netpath.");
		exit(1);
	}

	svc_run();
	msgout("svc_run returned");
	exit(1);
	/* NOTREACHED */
}

static void
bootparamprog_1(rqstp, transp)
	struct svc_req *rqstp;
	register SVCXPRT *transp;
{
	union {
		bp_whoami_arg bootparamproc_whoami_1_arg;
		bp_getfile_arg bootparamproc_getfile_1_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	_rpcsvcdirty = 1;
	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply(transp, xdr_void, (char *)NULL);
		_rpcsvcdirty = 0;
		return;

	case BOOTPARAMPROC_WHOAMI:
		xdr_argument = xdr_bp_whoami_arg;
		xdr_result = xdr_bp_whoami_res;
		local = (char *(*)()) bootparamproc_whoami_1;
		break;

	case BOOTPARAMPROC_GETFILE:
		xdr_argument = xdr_bp_getfile_arg;
		xdr_result = xdr_bp_getfile_res;
		local = (char *(*)()) bootparamproc_getfile_1;
		break;

	default:
		svcerr_noproc(transp);
		_rpcsvcdirty = 0;
		return;
	}
	(void) memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t)&argument)) {
		svcerr_decode(transp);
		_rpcsvcdirty = 0;
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t)&argument)) {
		msgout("unable to free arguments");
		exit(1);
	}
	_rpcsvcdirty = 0;
}

void
msgout(fmt, a1, a2, a3, a4, a5, a6, a7, a8)
	char *fmt, *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8;
{
	/*
	 * messges go to syslog if the program was started by
	 * another server, or if it was run from the command line without
	 * the debug flag.
	 */
	if (server_child || !debug)
		syslog(LOG_ERR, fmt, a1, a2, a3, a4, a5, a6, a7, a8);
	else {
		(void) fprintf(stderr, fmt, a1, a2, a3, a4, a5, a6, a7, a8);
		(void) fprintf(stderr, "\n");
	}
}

static void
closedown()
{
	if (_rpcsvcdirty == 0) {
		int size;
		int i, openfd;
		struct t_info tinfo;

		if (!t_getinfo(0, &tinfo) && (tinfo.servtype == T_CLTS))
			exit(0);
		size = svc_max_pollfd;
		for (i = 0, openfd = 0; i < size && openfd < 2; i++)
			if (svc_pollfd[i].fd >= 0)
				openfd++;
		if (openfd <= 1)
			exit(0);
	}
	(void) alarm(_RPCSVC_CLOSEDOWN);
}
