/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)unshare.c	1.15	99/02/21 SMI"

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *	PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *     Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *  (c) 1986,1987,1988,1989,1996,1999  Sun Microsystems, Inc
 *  (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *            All rights reserved.
 *
 */
/*
 * nfs unshare
 */
#include <stdio.h>
#include <string.h>
#include <varargs.h>
#include <unistd.h>
#include <sys/param.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include "../lib/sharetab.h"
#include "../lib/nfslogtab.h"

#define	RET_OK		0
#define	RET_ERR		32

static int do_unshare();
static void pr_err();
static int sharetab_del();
static int nfslogtab_deactivate();
static void usage();

int
main(argc, argv)
	int argc;
	char **argv;
{
	char dir[MAXPATHLEN];

	if (argc != 2) {
		usage();
		exit(1);
	}

	/* Don't drop core if the NFS module isn't loaded. */
	signal(SIGSYS, SIG_IGN);

	if (realpath(argv[1], dir) == NULL) {
		pr_err("%s: %s\n", argv[1], strerror(errno));
		exit(RET_ERR);
	}

	return (do_unshare(dir));
}

static int
do_unshare(path)
	char *path;
{
	int logging = 0;

	if (exportfs(path, NULL) < 0) {
		if (errno == EINVAL)
			pr_err("%s: not shared\n", path);
		else
			pr_err("%s: %s\n", path, strerror(errno));
		return (RET_ERR);
	}

	if (sharetab_del(path, &logging) < 0)
		return (RET_ERR);

	if (logging) {
		if (nfslogtab_deactivate(path) < 0)
			return (RET_ERR);
	}

	return (RET_OK);
}

/*
 * Remove an entry from the sharetab file.
 */
static int
sharetab_del(path, logging)
	char *path;
	int *logging;
{
	FILE *f;

	f = fopen(SHARETAB, "r+");
	if (f == NULL) {
		pr_err("%s: %s\n", SHARETAB, strerror(errno));
		return (-1);
	}
	if (lockf(fileno(f), F_LOCK, 0L) < 0) {
		pr_err("cannot lock %s: %s\n", SHARETAB, strerror(errno));
		(void) fclose(f);
		return (-1);
	}
	if (remshare(f, path, logging) < 0) {
		pr_err("remshare\n");
		return (-1);
	}
	(void) fclose(f);
	return (0);
}

/*
 * Deactivate an entry from the nfslogtab file.
 */
static int
nfslogtab_deactivate(path)
	char *path;
{
	FILE *f;
	int error = 0;

	f = fopen(NFSLOGTAB, "r+");
	if (f == NULL) {
		error = errno;
		pr_err("%s: %s\n", NFSLOGTAB, strerror(error));
		goto out;
	}
	if (lockf(fileno(f), F_LOCK, 0L) < 0) {
		error = errno;
		pr_err("cannot lock %s: %s\n", NFSLOGTAB, strerror(error));
		goto out;
	}
	if (logtab_deactivate(f, NULL, path, NULL) == -1) {
		error = -1;
		pr_err("logtab_deactivate\n");
		goto out;
	}

out:	if (error) {
		pr_err("could not deactivate %s entry in %s\n",
		path, NFSLOGTAB);
	}

	if (f != NULL)
		(void) fclose(f);

	return (error);
}

static void
usage()
{
	(void) fprintf(stderr, "Usage: unshare { pathname | resource }\n");
}

/*VARARGS1*/
static void
pr_err(fmt, va_alist)
	char *fmt;
	va_dcl
{
	va_list ap;

	va_start(ap);
	(void) fprintf(stderr, "nfs unshare: ");
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
}
