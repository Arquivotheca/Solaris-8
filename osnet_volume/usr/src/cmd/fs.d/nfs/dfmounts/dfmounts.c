/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)dfmounts.c	1.9	93/09/28 SMI"	/* SVr4.0 1.3	*/

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
 * nfs dfmounts
 */
#include <stdio.h>
#include <stdlib.h>
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
#include <locale.h>

static int hflg;

static void pr_mounts(char *);
static void freemntlist(struct mountbody *);
static int sortpath(const void *, const void *);
static void usage(void);
static void pr_err();

main(argc, argv)
	int argc;
	char **argv;
{

	char hostbuf[256];
	extern int optind;
	int i, c;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

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
			pr_mounts(argv[i]);
	} else {
		if (gethostname(hostbuf, sizeof (hostbuf)) < 0) {
			perror("nfs dfmounts: gethostname");
			exit(1);
		}
		pr_mounts(hostbuf);
	}

	return (0);
}

#define	NTABLEENTRIES	2048
static struct mountbody *table[NTABLEENTRIES];
static struct timeval	rpc_totout_new = {15, 0};

/*
 * Print the filesystems on "host" that are currently mounted by a client.
 */

static void
pr_mounts(host)
	char *host;
{
	CLIENT *cl;
	struct mountbody *ml = NULL;
	struct mountbody **tb, **endtb;
	enum clnt_stat err;
	char *lastpath;
	char *lastclient;
	int tail = 0;
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
			pr_err(gettext("can't contact server: %s\n"),
				clnt_spcreateerror(host));
			__rpc_control(2, (char *) &rpc_totout_old);
			return;
		}
	}

	__rpc_control(2, (char *) &rpc_totout_old);
	tout.tv_sec = 10;
	tout.tv_usec = 0;

	if (err = clnt_call(cl, MOUNTPROC_DUMP, xdr_void,
			    0, xdr_mountlist, (caddr_t) &ml, tout)) {
		pr_err("%s\n", clnt_sperrno(err));
		clnt_destroy(cl);
		return;
	}

	if (ml == NULL)
		return;	/* no mounts */

	if (!hflg) {
		printf("%-8s %10s %-24s  %s",
			gettext("RESOURCE"), gettext("SERVER"),
			gettext("PATHNAME"), gettext("CLIENTS"));
		hflg++;
	}

	/*
	 * Create an array describing the mounts, so that we can sort them.
	 */
	tb = table;
	for (; ml != NULL && tb < &table[NTABLEENTRIES]; ml = ml->ml_next)
		*tb++ = ml;
	if (ml != NULL && tb == &table[NTABLEENTRIES])
		pr_err(gettext("table overflow:  only %d entries shown\n"),
			NTABLEENTRIES);
	endtb = tb;
	qsort(table, endtb - table, sizeof (struct mountbody *), sortpath);

	/*
	 * Print out the sorted array.  Group entries for the same
	 * filesystem together, and ignore duplicate entries.
	 */
	lastpath = "";
	lastclient = "";
	for (tb = table; tb < endtb; tb++) {
		if (*((*tb)->ml_directory) == '\0' ||
		    *((*tb)->ml_hostname) == '\0')
			continue;
		if (strcmp(lastpath, (*tb)->ml_directory) == 0) {
			if (strcmp(lastclient, (*tb)->ml_hostname) == 0) {
				continue; 	/* ignore duplicate */
			}
		} else {
			printf("\n%-8s %10s %-24s ",
				"  -", host, (*tb)->ml_directory);
			lastpath = (*tb)->ml_directory;
			tail = 0;
		}
		if (tail++)
			printf(",");
		printf("%s", (*tb)->ml_hostname);
		lastclient = (*tb)->ml_hostname;
	}
	printf("\n");

	freemntlist(ml);
	clnt_destroy(cl);
}

static void
freemntlist(ml)
	struct mountbody *ml;
{
	register struct mountbody *old;

	while (ml) {
		if (ml->ml_hostname)
			free(ml->ml_hostname);
		if (ml->ml_directory)
			free(ml->ml_directory);
		old = ml;
		ml = ml->ml_next;
		free(old);
	}
}

/*
 * Compare two structs for mounted filesystems.  The primary sort key is
 * the name of the exported filesystem.  There is also a secondary sort on
 * the name of the client, so that duplicate entries (same path and
 * hostname) will sort together.
 *
 * Returns < 0 if the first entry sorts before the second entry, 0 if they
 * sort the same, and > 0 if the first entry sorts after the second entry.
 */

static int
sortpath(a, b)
	const void *a, *b;
{
	const struct mountbody **m1, **m2;
	int result;

	m1 = (const struct mountbody **)a;
	m2 = (const struct mountbody **)b;

	result = strcmp((*m1)->ml_directory, (*m2)->ml_directory);
	if (result == 0) {
		result = strcmp((*m1)->ml_hostname, (*m2)->ml_hostname);
	}

	return (result);
}

static void
usage()
{
	(void) fprintf(stderr, gettext("Usage: dfmounts [-h] [host ...]\n"));
}

/* VARARGS1 */
static void
pr_err(fmt, va_alist)
	char *fmt;
	va_dcl
{
	va_list ap;

	va_start(ap);
	(void) fprintf(stderr, "nfs dfmounts: ");
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
}
