/*
 *      Copyright (c) 1993, 1996 Sun Microsystems, Inc.  All Rights Reserved.
 *
 *      Sun considers its source code as an unpublished, proprietary
 *      trade secret, and it is available only under strict license
 *      provisions.  This copyright notice is placed here only to protect
 *      Sun in the event the source is deemed a published work.  Dissassembly,
 *      decompilation, or other means of reducing the object code to human
 *      readable form is prohibited by the license agreement under which
 *      this code is provided to the user or company in possession of this
 *      copy.
 *
 *      RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the
 *      Government is subject to restrictions as set forth in subparagraph
 *      (c)(1)(ii) of the Rights in Technical Data and Computer Software
 *      clause at DFARS 52.227-7013 and in similar clauses in the FAR and
 *      NASA FAR Supplement.
 *
 *
 *
 */

#ident	"@(#)clear_locks.c	1.5	96/02/12 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <libintl.h>
#include <locale.h>
#include <rpc/rpc.h>
#include <rpcsvc/nlm_prot.h>

#include <sys/systeminfo.h>
#include <netdb.h>

extern char *optarg;
extern int optind;

static int share_zap(char *, char *);

int
main(int argc, char *argv[])
{
	int i, c, ret;
	int sflag = 0;
	int errflg = 0;
	char myhostname[MAXHOSTNAMELEN];

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	/*
	 * Get the official hostname for this host
	 */
	sysinfo(SI_HOSTNAME, myhostname, sizeof (myhostname));

	while ((c = getopt(argc, argv, "s")) != EOF)
		switch (c) {
		case 's':
			sflag++;
			break;
		case '?':
			errflg++;
		}
	i = argc - optind;
	if (errflg || i != 1) {
		(void) fprintf(stderr,
				gettext("Usage: clear_locks [-s] hostname\n"));
		exit(2);
	}

	if (geteuid() != (uid_t)0) {
		(void) fprintf(stderr, gettext("clear_locks: must be root\n"));
		exit(1);
	}

	if (sflag) {
		(void) fprintf(stdout,
gettext("Clearing locks held for NFS client %s on server %s\n"),
				myhostname, argv[optind]);
		ret = share_zap(myhostname, argv[optind]);
	} else {
		(void) fprintf(stdout,
gettext("Clearing locks held for NFS client %s on server %s\n"),
				argv[optind], myhostname);
		ret = share_zap(argv[optind], myhostname);
	}

	return (ret);
}


/*
 * Request that host 'server' free all locks held by
 * host 'client'.
 */
static int
share_zap(char *client, char *server)
{
	struct nlm_notify notify;
	enum clnt_stat rslt;

	notify.state = 0;
	notify.name = client;
	rslt = rpc_call(server, NLM_PROG, NLM_VERSX, NLM_FREE_ALL,
		xdr_nlm_notify, (char *) &notify, xdr_void, 0, NULL);
	if (rslt != RPC_SUCCESS) {
		clnt_perrno(rslt);
		return (3);
	}
	(void) fprintf(stderr,
		gettext("clear of locks held for %s on %s returned success\n"),
		client, server);
	return (0);
}
