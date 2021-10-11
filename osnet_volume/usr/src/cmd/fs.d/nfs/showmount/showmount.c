/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)showmount.c	1.10	94/03/05 SMI"	/* SVr4.0 1.4	*/


/*
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		PROPRIETARY NOTICE(Combined)

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
 * showmount
 */
#include <stdio.h>
#include <varargs.h>
#include <rpc/rpc.h>
#include <rpc/rpcb_clnt.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#include <nfs/nfs.h>
#include <rpcsvc/mount.h>
#include <locale.h>

int sorthost();
int sortpath();
void pr_err();
void printex();
void usage();

/*
 * Dynamically-sized array of pointers to mountlist entries.  Each element
 * points into the linked list returned by the RPC call.  We use an array
 * so that we can conveniently sort the entries.
 */
static struct mountbody **table;

struct	timeval	rpc_totout_new = {15, 0};

main(argc, argv)
	int argc;
	char **argv;
{
	int aflg = 0, dflg = 0, eflg = 0;
	int err;
	struct mountbody *result_list = NULL;
	struct mountbody *ml = NULL;
	struct mountbody **tb;		/* pointer into table */
	char *host, hostbuf[256];
	char *last;
	CLIENT *cl;
	extern int optind;
	extern char *optarg;
	int c;
	struct	timeval	tout, rpc_totout_old;
	int	numentries;
	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "ade")) != EOF) {
		switch (c) {
		case 'a':
			aflg++;
			break;
		case 'd':
			dflg++;
			break;
		case 'e':
			eflg++;
			break;
		default:
			usage();
			exit(1);
		}
	}

	switch (argc - optind) {
	case 0:		/* no args */
		if (gethostname(hostbuf, sizeof (hostbuf)) < 0) {
			pr_err("gethostname: %s\n", strerror(errno));
			exit(1);
		}
		host = hostbuf;
		break;
	case 1:		/* one arg */
		host = argv[optind];
		break;
	default:	/* too many args */
		usage();
		exit(1);
	}

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
			pr_err("");
			clnt_pcreateerror(host);
			__rpc_control(2, (char *) &rpc_totout_old);
			exit(1);
		}
	}

	__rpc_control(2, (char *) &rpc_totout_old);

	if (eflg) {
		printex(cl, host);
		if (aflg + dflg == 0) {
			exit(0);
		}
	}

	tout.tv_sec = 10;
	tout.tv_usec = 0;

	if (err = clnt_call(cl, MOUNTPROC_DUMP,
			    xdr_void, 0, xdr_mountlist,
				(caddr_t)&result_list, tout)) {
		pr_err("%s\n", clnt_sperrno(err));
		exit(1);
	}

	/*
	 * Count the number of entries in the list.  If the list is empty,
	 * quit now.
	 */
	numentries = 0;
	for (ml = result_list; ml != NULL; ml = ml->ml_next)
		numentries++;
	if (numentries == 0)
		exit(0);

	/*
	 * Allocate memory for the array and initialize the array.
	 */

	table = (struct mountbody **)calloc(numentries,
						sizeof (struct mountbody *));
	if (table == NULL) {
		pr_err(gettext("not enough memory for %d entries\n"),
		    numentries);
		exit(1);
	}
	for (ml = result_list, tb = &table[0];
	    ml != NULL;
	    ml = ml->ml_next, tb++) {
		*tb = ml;
	}

	/*
	 * Sort the entries and print the results.
	 */

	if (dflg)
	    qsort(table, numentries, sizeof (struct mountbody *), sortpath);
	else
	    qsort(table, numentries, sizeof (struct mountbody *), sorthost);
	if (aflg) {
		for (tb = table; tb < table + numentries; tb++)
			printf("%s:%s\n", (*tb)->ml_hostname,
			    (*tb)->ml_directory);
	} else if (dflg) {
		last = "";
		for (tb = table; tb < table + numentries; tb++) {
			if (strcmp(last, (*tb)->ml_directory))
				printf("%s\n", (*tb)->ml_directory);
			last = (*tb)->ml_directory;
		}
	} else {
		last = "";
		for (tb = table; tb < table + numentries; tb++) {
			if (strcmp(last, (*tb)->ml_hostname))
				printf("%s\n", (*tb)->ml_hostname);
			last = (*tb)->ml_hostname;
		}
	}
	return (0);
}

sorthost(a, b)
	struct mountbody **a, **b;
{
	return (strcmp((*a)->ml_hostname, (*b)->ml_hostname));
}

sortpath(a, b)
	struct mountbody **a, **b;
{
	return (strcmp((*a)->ml_directory, (*b)->ml_directory));
}

void
usage()
{
	(void) fprintf(stderr,
			gettext("Usage: showmount [-a] [-d] [-e] [host]\n"));
}

void
pr_err(fmt, va_alist)
	char *fmt;
	va_dcl
{
	va_list ap;

	va_start(ap);
	(void) fprintf(stderr, "showmount: ");
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void
printex(cl, host)
	CLIENT *cl;
	char *host;
{
	struct exportnode *ex = NULL;
	struct exportnode *e;
	struct groupnode *gr;
	enum clnt_stat err;
	int max;
	struct	timeval	tout;

	tout.tv_sec = 10;
	tout.tv_usec = 0;

	if (err = clnt_call(cl, MOUNTPROC_EXPORT,
	    xdr_void, 0, xdr_exports, (caddr_t) &ex, tout)) {
		pr_err("%s\n", clnt_sperrno(err));
		exit(1);
	}

	if (ex == NULL) {
		printf(gettext("no exported file systems for %s\n"), host);
	} else {
		printf(gettext("export list for %s:\n"), host);
	}
	max = 0;
	for (e = ex; e != NULL; e = e->ex_next) {
		if (strlen(e->ex_dir) > max) {
			max = strlen(e->ex_dir);
		}
	}
	while (ex) {
		printf("%-*s ", max, ex->ex_dir);
		gr = ex->ex_groups;
		if (gr == NULL) {
			printf(gettext("(everyone)"));
		}
		while (gr) {
			printf("%s", gr->gr_name);
			gr = gr->gr_next;
			if (gr) {
				printf(",");
			}
		}
		printf("\n");
		ex = ex->ex_next;
	}
}
