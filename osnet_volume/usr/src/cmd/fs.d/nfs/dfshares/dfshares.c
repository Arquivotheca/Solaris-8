/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)dfshares.c	1.7	93/03/30 SMI"	/* SVr4.0 1.4	*/

/*
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice

Notice of copyright on this source code product does not indicate
publication.

	(c) 1986, 1987, 1988, 1989  Sun Microsystems, Inc
	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
		All rights reserved.

*/

/*
 * nfs dfshares
 */
#include <stdio.h>
#include <varargs.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpc/rpcb_clnt.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <nfs/nfs.h>
#include <rpcsvc/mount.h>

int hflg;
void pr_exports();
void free_ex();
void usage();

main(argc, argv)
	int argc;
	char **argv;
{

	char hostbuf[256];
	extern int optind;
	extern char *optarg;
	int i, c;

	while ((c = getopt(argc, argv, "h")) != EOF) {
		switch (c) {
		case 'h':
			hflg++;
			break;
		default:
			usage();
			exit(1);
		}
	}

	if (optind < argc) {
		for (i = optind; i < argc; i++)
			pr_exports(argv[i]);
	} else {
		if (gethostname(hostbuf, sizeof (hostbuf)) < 0) {
			perror("nfs dfshares: gethostname");
			exit(1);
		}
		pr_exports(hostbuf);
	}

	exit(0);
}

struct	timeval	rpc_totout_new = {15, 0};

void
pr_exports(host)
	char *host;
{
	CLIENT *cl;
	struct exportnode *ex = NULL;
	enum clnt_stat err;
	struct	timeval	tout, rpc_totout_old;

	__rpc_control(1, (char *) &rpc_totout_old);
	__rpc_control(2, (char *) &rpc_totout_new);

	/*
	 * First try circuit, then drop back to datagram if
	 * circuit is unavailable (an old version of mountd perhaps)
	 * Using circuit is preferred because it can handle
	 * arbitrarily long export lists.
	 */
	cl = clnt_create(host, MOUNTPROG, MOUNTVERS, "circuit_n");
	if (cl == NULL) {
		if (rpc_createerr.cf_stat == RPC_PROGNOTREGISTERED)
			cl = clnt_create(host, MOUNTPROG, MOUNTVERS,
					"datagram_n");
		if (cl == NULL) {
			(void) fprintf(stderr, "nfs dfshares:");
			clnt_pcreateerror(host);
			__rpc_control(2, (char *) &rpc_totout_old);
			exit(1);
		}
	}

	__rpc_control(2, (char *) &rpc_totout_old);
	tout.tv_sec = 10;
	tout.tv_usec = 0;

	if (err = clnt_call(cl, MOUNTPROC_EXPORT, xdr_void,
	    0, xdr_exports, (caddr_t) &ex, tout)) {
		(void) fprintf(stderr, "nfs dfshares: %s\n", clnt_sperrno(err));
		clnt_destroy(cl);
		exit(1);
	}

	if (ex == NULL) {
		clnt_destroy(cl);
		exit(1);
	}

	if (!hflg) {
		printf("%-35s %12s %-8s  %s\n",
			"RESOURCE", "SERVER", "ACCESS", "TRANSPORT");
		hflg++;
	}

	while (ex) {
		printf("%10s:%-24s %12s %-8s  %s\n",
			host, ex->ex_dir, host, " -", " -");
		ex = ex->ex_next;
	}
	free_ex(ex);
	clnt_destroy(cl);
}

void
free_ex(ex)
	struct exportnode *ex;
{
	struct groupnode *gr, *tmpgr;
	struct exportnode *tmpex;

	while (ex) {
		free(ex->ex_dir);
		gr = ex->ex_groups;
		while (gr) {
			tmpgr = gr->gr_next;
			free((char *)gr);
			gr = tmpgr;
		}
		tmpex = ex;
		ex = ex->ex_next;
		free((char *)tmpex);
	}
}

void
usage()
{
	(void) fprintf(stderr, "Usage: dfshares [-h] [host ...]\n");
}
