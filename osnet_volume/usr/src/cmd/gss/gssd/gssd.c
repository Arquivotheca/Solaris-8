/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Usermode daemon which assists the kernel when handling gssapi calls.
 * It is gssd that actually implements all gssapi calls.
 * Some calls, such as gss_sign, are implemented in the kernel on a per
 * mechanism basis.
 */

#pragma ident	"@(#)gssd.c	1.21	99/06/07 SMI"

#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/rpc_com.h>
#include <sys/syslog.h>
#include <sys/termios.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>
#include <stdlib.h>
#include <stropts.h>
#include <fcntl.h>
#include <strings.h>
#include <syslog.h>
#include "gssd.h"

int gssd_debug = 0;		/* enable debugging printfs */

void gssprog_1();
void gssd_setup(char *);
static void usage(void);
static void detachfromtty(void);
extern int svc_create_local_service();

/* following declarations needed in rpcgen-generated code */
int _rpcpmstart = 0;		/* Started by a port monitor ? */
int _rpcfdtype;			/* Whether Stream or Datagram ? */
int _rpcsvcdirty;		/* Still serving ? */

int
main(argc, argv)
int argc;
char **argv;
{
	register SVCXPRT *transp;
	int maxrecsz = RPC_MAXDATASIZE;
	extern int optind;
	int c;
	char mname[FMNAMESZ + 1];
	extern int _getuid();

	/* set locale and domain for internationalization */
	setlocale(LC_ALL, "");
	textdomain(TEXT_DOMAIN);


	/*
	 * take special note that "_getuid()" is called here. This is necessary
	 * since we must fake out the mechanism libraries calls to getuid()
	 * with a special routine that is provided as part of gssd. However,
	 * the call below MUST call the real getuid() to ensure it is running
	 * as root.
	*/

#ifdef DEBUG
	(void) setuid(0);		/* DEBUG: set ruid to root */
#endif DEBUG
	if (_getuid()) {
		(void) fprintf(stderr,
				gettext("[%s] must be run as root\n"), argv[0]);
#ifdef DEBUG
		(void) fprintf(stderr, gettext(" warning only\n"));
#else !DEBUG
		exit(1);
#endif DEBUG
	}

	gssd_setup(argv[0]);

	while ((c = getopt(argc, argv, "d")) != -1)
		switch (c) {
		    case 'd':
			/* turn on debugging */
			gssd_debug = 1;
			break;
		    default:
			usage();
		}

	if (optind != argc) {
		usage();
	}

	/*
	 * Started by inetd if name of module just below stream
	 * head is either a sockmod or timod.
	 */
	if (!ioctl(0, I_LOOK, mname) &&
		((strcmp(mname, "sockmod") == 0) ||
			(strcmp(mname, "timod") == 0))) {

		char *netid;
		struct netconfig *nconf;

		openlog("gssd", LOG_PID, LOG_DAEMON);

		if ((netid = getenv("NLSPROVIDER")) ==  NULL) {
			netid = "ticotsord";
		}

		if ((nconf = getnetconfigent(netid)) == NULL) {
			syslog(LOG_ERR, gettext("cannot get transport info"));
			exit(1);
		}

		if (strcmp(mname, "sockmod") == 0) {
			if (ioctl(0, I_POP, 0) || ioctl(0, I_PUSH, "timod")) {
				syslog(LOG_ERR,
					gettext("could not get the "
						"right module"));
				exit(1);
			}
		}
		if (!rpc_control(RPC_SVC_CONNMAXREC_SET, &maxrecsz)) {
			syslog(LOG_ERR,
				gettext("unable to set RPC max record size"));
			exit(1);
		}
		/* XXX - is nconf even needed here? */
		if ((transp = svc_tli_create(0, nconf, NULL, 0, 0)) == NULL) {
			syslog(LOG_ERR, gettext("cannot create server handle"));
			exit(1);
		}

		/*
		 * We use a NULL nconf because GSSPROG has already been
		 * registered with rpcbind.
		 */
		if (!svc_reg(transp, GSSPROG, GSSVERS, gssprog_1, NULL)) {
			syslog(LOG_ERR,
				gettext("unable to register "
					"(GSSPROG, GSSVERS)"));
			exit(1);
		}

		if (nconf)
			freenetconfigent(nconf);
	} else {

		if (!gssd_debug)
			detachfromtty();

		openlog("gssd", LOG_PID, LOG_DAEMON);

		if (svc_create_local_service(gssprog_1, GSSPROG, GSSVERS,
		    "netpath", "gssd") == 0) {
			syslog(LOG_ERR, gettext("unable to create service"));
			exit(1);
		}
	}


	if (gssd_debug) {
		fprintf(stderr,
		    gettext("gssd start: \n"));
	}
	svc_run();
	abort();
	/*NOTREACHED*/
#ifdef	lint
	return (1);
#endif
}

static void
usage(void)
{
	(void) fprintf(stderr, gettext("usage: gssd [-dg]\n"));
	exit(1);
}


/*
 * detach from tty
 */
static void
detachfromtty(void)
{
	register int i;
	struct rlimit rl;

	switch (fork()) {
	case -1:
		perror(gettext("gssd: can not fork"));
		exit(1);
		/*NOTREACHED*/
	case 0:
		break;
	default:
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
}

/*ARGSUSED*/
int
gssprog_1_freeresult(SVCXPRT *transport, xdrproc_t xdr_res, caddr_t res)
{
	xdr_free(xdr_res, res);
	return (1);
}
