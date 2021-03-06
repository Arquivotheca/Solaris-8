/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)clinfo.c	1.2	98/08/19 SMI"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <sys/cladm.h>

#if !defined(TEXT_DOMAIN)		/* should be defined by cc -D */
#define	TEXT_DOMAIN 	"SYS_TEST"	/* Use this only if it wasn't */
#endif

static char *cmdname;

static void errmsg(char *);
static void usage();

int
main(int argc, char **argv)
{
	int c, bootflags;
	nodeid_t nid, hid;
	char *cp = "";

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	cmdname = argv[0];	/* put actual command name in messages */

	if (_cladm(CL_INITIALIZE, CL_GET_BOOTFLAG, &bootflags) != 0) {
		errmsg("_cladm(CL_INITIALIZE, CL_GET_BOOTFLAG)");
		return (1);
	}

	while ((c = getopt(argc, argv, "bnh")) != EOF) {
		switch (c) {
		case 'b':	/* print boot flags */
			(void) printf("%s%u\n", cp, bootflags);
			break;

		case 'n':	/* print our node number */
			if (_cladm(CL_CONFIG, CL_NODEID, &nid) != 0) {
				errmsg("_cladm(CL_CONFIG, CL_NODEID)");
				return (1);
			}
			(void) printf("%s%u\n", cp, nid);
			break;

		case 'h':	/* print the highest node number */
			if (_cladm(CL_CONFIG, CL_HIGHEST_NODEID, &hid) != 0) {
				errmsg("_cladm(CL_CONFIG, CL_HIGHEST_NODEID)");
				return (1);
			}
			(void) printf("%s%u\n", cp, hid);
			break;

		default:
			usage();
			return (1);
		}
		cp = " ";
	}

	/*
	 * Return exit status of one (error) if not booted as a cluster.
	 */
	return (bootflags & CLUSTER_BOOTED ? 0 : 1);
}

static void
errmsg(char *msg)
{
	int save_error;

	save_error = errno;		/* in case fprintf changes errno */
	(void) fprintf(stderr, "%s: ", cmdname);
	errno = save_error;
	perror(msg);
}

static void
usage()
{
	(void) fprintf(stderr, "%s: %s -[bnh]\n", gettext("usage"), cmdname);
}
