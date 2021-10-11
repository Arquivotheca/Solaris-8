/* LINTLIBRARY */
/* PROTOLIB1 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)nfsd.c	1.48	99/08/13 SMI"	/* SVr4.0 1.9   */

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
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1989,1994,1996-1997,1999 by Sun Microsystems, Inc.
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		    All rights reserved.
 *
 */

/* NFS server */

#include <sys/param.h>
#include <sys/types.h>
#include <syslog.h>
#include <tiuser.h>
#include <rpc/rpc.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/file.h>
#include <nfs/nfs.h>
#include <nfs/nfs_acl.h>
#include <nfs/nfssys.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <netconfig.h>
#include <netdir.h>
#include <string.h>
#include <unistd.h>
#include <stropts.h>
#include <sys/tihdr.h>
#include <poll.h>
#include <sys/tiuser.h>
#include <netinet/tcp.h>
#include "nfs_tbind.h"

static	int	nfssvc(int, struct netbuf, struct netconfig *);
static int nfssvcpool(int maxservers);
static	void	usage(void);

extern	int	_nfssys(int, void *);

static	char	*MyName;
static	NETSELDECL(defaultproviders)[] = { "/dev/tcp6", "/dev/tcp", "/dev/udp",
					    "/dev/udp6", NULL };
/* static	NETSELDECL(defaultprotos)[] =	{ NC_UDP, NC_TCP, NULL }; */
/*
 * The following are all globals used by routines in nfs_tbind.c.
 */
size_t	end_listen_fds;		/* used by conn_close_oldest() */
size_t	num_fds = 0;		/* used by multiple routines */
int	listen_backlog = 32;	/* used by bind_to_{provider,proto}() */
int	num_servers;		/* used by cots_listen_event() */
int	(*Mysvc)(int, struct netbuf, struct netconfig *) = nfssvc;
				/* used by cots_listen_event() */
int	max_conns_allowed = -1;	/* used by cots_listen_event() */

static void sigflush(int);

main(int ac, char **av)
{
	char *dir = "/";
	int allflag = 0;
	int maxservers = 1;	/* zero allows inifinte nubmer of threads */
	int logmaxservers = 0;
	int pid;
	int i;
	int opt_cnt;
	char *provider = (char *)NULL;
	struct protob *protobp0, *protobp;
	NETSELDECL(proto) = NULL;
	NETSELPDECL(providerp);

	MyName = *av;
	opt_cnt = 0;

	if (geteuid() != 0) {
		(void) fprintf(stderr, "%s must be run as root\n", av[0]);
		exit(1);
	}

	while ((i = getopt(ac, av, "ac:p:t:l:")) != EOF) {
		switch (i) {
		case 'a':
			allflag = 1;
			opt_cnt++;
			break;

		case 'c':
			max_conns_allowed = atoi(optarg);
			if (max_conns_allowed <= 0)
				usage();
			break;

		case 'p':
			proto = optarg;
			opt_cnt++;
			break;

		case 't':
			provider = optarg;
			opt_cnt++;
			break;

		case 'l':
			listen_backlog = atoi(optarg);
			if (listen_backlog < 0)
				usage();
			break;

		case '?':
			usage();
			/* NOTREACHED */
		}
	}

	/*
	 * Conflict options error messages.
	 */
	if (opt_cnt > 1) {
		(void) fprintf(stderr, "\nConflict options:");
		(void) fprintf(stderr,
			" only one of a/p/t options can be specified.\n\n");
		usage();
	}

	/*
	 * If there is exactly one more argument, it is the number of
	 * servers.
	 */
	if (optind == ac - 1) {
		maxservers = atoi(av[optind]);
		if (maxservers <= 0)
			usage();
	}
	/*
	 * If there are two or more arguments, then this is a usage error.
	 */
	else if (optind < ac - 1)
		usage();
	/*
	 * There are no additional arguments, we use a default number of
	 * servers.  We will log this.
	 */
	else
		logmaxservers = 1;

	/*
	 * Set current and root dir to server root
	 */
	if (chroot(dir) < 0) {
		(void) fprintf(stderr, "%s:  ", MyName);
		perror(dir);
		exit(1);
	}
	if (chdir(dir) < 0) {
		(void) fprintf(stderr, "%s:  ", MyName);
		perror(dir);
		exit(1);
	}

#ifndef DEBUG
	/*
	 * Background
	 */
	pid = fork();
	if (pid < 0) {
		perror("nfsd: fork");
		exit(1);
	}
	if (pid != 0)
		exit(0);

	{
		struct rlimit rl;

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
	}
#endif
	openlog(MyName, LOG_PID | LOG_NDELAY, LOG_DAEMON);

	sigset(SIGTERM, sigflush);

	if (logmaxservers) {
		(void) syslog(LOG_INFO,
			"Number of servers not specified. Using default of %d.",
			maxservers);
	}

	/*
	 * Set up kernel RPC thread pool for the NFS server.
	 */
	if (nfssvcpool(maxservers)) {
		(void) syslog(LOG_ERR,
			"Can't set up kernel NFS service: %m. Exiting");
		exit(1);
	}

	/*
	 * Build a protocol block list for registration.
	 */
	protobp0 = protobp = (struct protob *)malloc(sizeof (struct protob));
	protobp->serv = "NFS";
	protobp->versmin = NFS_VERSMIN;
	protobp->versmax = NFS_VERSMAX;
	protobp->program = NFS_PROGRAM;

	protobp->next = (struct protob *)malloc(sizeof (struct protob));
	protobp = protobp->next;
	protobp->serv = "NFS_ACL";		/* not used */
	protobp->versmin = NFS_ACL_VERSMIN;
	protobp->versmax = NFS_ACL_VERSMAX;
	protobp->program = NFS_ACL_PROGRAM;
	protobp->next = (struct protob *)NULL;

	if (allflag) {
		if (do_all(protobp0, nfssvc) == -1)
			exit(1);
	} else if (proto) {
		/* there's more than one match for the same protocol */
		struct netconfig *nconf;
		NCONF_HANDLE *nc;
		bool_t	protoFound = FALSE;
		if ((nc = setnetconfig()) == (NCONF_HANDLE *) NULL) {
			syslog(LOG_ERR, "setnetconfig failed: %m");
			goto done;
		}
		while (nconf = getnetconfig(nc)) {
			if (strcmp(nconf->nc_proto, proto) == 0) {
				protoFound = TRUE;
				do_one(nconf->nc_device, NULL,
					protobp0, nfssvc);
			}
		}
		(void) endnetconfig(nc);
		if (protoFound == FALSE)
			syslog(LOG_ERR, "couldn't find netconfig entry \
for protocol %s", proto);

	} else if (provider)
		do_one(provider, proto, protobp0, nfssvc);
	else {
		for (providerp = defaultproviders;
			*providerp != NULL; providerp++) {
			provider = *providerp;
			do_one(provider, NULL, protobp0, nfssvc);
		}
	}
done:

	free(protobp);
	free(protobp0);

	if (num_fds == 0) {
		(void) syslog(LOG_ERR,
		"Could not start NFS service for any protocol. Exiting.");
		exit(1);
	}

	end_listen_fds = num_fds;

	/*
	 * Poll for non-data control events on the transport descriptors.
	 */
	poll_for_action();

	/*
	 * If we get here, something failed in poll_for_action().
	 */
	return (1);
}

static int
nfssvcpool(int maxservers)
{
	struct svcpool_args npa;

	npa.id = NFS_SVCPOOL_ID;
	npa.maxthreads = maxservers;
	npa.redline = 0;
	npa.qsize = 0;
	npa.timeout = 0;
	npa.stksize = 0;
	npa.max_same_xprt = 0;
	return (_nfssys(SVCPOOL_CREATE, &npa));
}

/*
 * Establish NFS service thread.
 */
static int
nfssvc(int fd, struct netbuf addrmask, struct netconfig *nconf)
{
	struct nfs_svc_args nsa;

	nsa.fd = fd;
	nsa.netid = nconf->nc_netid;
	nsa.addrmask = addrmask;
	return (_nfssys(NFS_SVC, &nsa));
}

static void
usage(void)
{
	(void) fprintf(stderr,
"usage: %s [ -a ] [ -c max_conns ] [ -p protocol ] [ -t transport ] ", MyName);
	(void) fprintf(stderr, "[ -l listen_backlog ] [ nservers ]\n");
	(void) fprintf(stderr,
"\twhere -a causes <nservers> to be started on each appropriate transport, \n");
	(void) fprintf(stderr,
"\tmax_conns is the maximum number of concurrent connections allowed,\n");
	(void) fprintf(stderr, "\t\t and max_conns must be a decimal number");
	(void) fprintf(stderr, "> zero,\n");
	(void) fprintf(stderr, "\tprotocol is a protocol identifier,\n");
	(void) fprintf(stderr,
		"\ttransport is a transport provider name (i.e. device),\n");
	(void) fprintf(stderr,
		"\tlisten_backlog is the TCP listen backlog,\n");
	(void) fprintf(stderr,
		"\tand <nservers> must be a decimal number > zero.\n");
	exit(1);
}

/*
 * Issue nfssys system call to flush all logging buffers asynchronously.
 *
 * NOTICE: It is extremely important to flush NFS logging buffers when
 *	   nfsd exits. When the system is halted or rebooted nfslogd
 *	   may not have an opportunity to flush the buffers.
 */
static void
nfsl_flush()
{
	struct nfsl_flush_args nfa;

	memset((void *)&nfa, 0, sizeof (nfa));
	nfa.version = NFSL_FLUSH_ARGS_VERS;
	nfa.directive = NFSL_ALL;	/* flush all asynchronously */

	if (_nfssys(LOG_FLUSH, &nfa) < 0)
		syslog(LOG_ERR, "_nfssys(LOG_FLUSH) failed: %s\n",
			strerror(errno));
}

/*
 * SIGTERM handler.
 * Flush logging buffers and exit.
 */
static void
sigflush(int sig)
{
	nfsl_flush();
	exit(0);
}
