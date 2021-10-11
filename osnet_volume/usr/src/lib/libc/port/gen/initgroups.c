/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)initgroups.c	1.15	96/11/27 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#pragma weak initgroups = _initgroups

#include "synonyms.h"
#include "shlib.h"
#include <stdlib.h>
#include <errno.h>
#include <grp.h>
#include <sys/types.h>
#include <unistd.h>

/* Private interface to the groups code in getgrnam.c */
extern int _getgroupsbymember(const char *, gid_t[], int, int);

int
initgroups(const char *uname, gid_t agroup)
{
	gid_t *groups;
	long ngroups_max;
	int ngroups;
	int errsave, retsave;

	if ((ngroups_max = sysconf(_SC_NGROUPS_MAX)) < 0) {
		/* ==== Hope sysconf() set errno to something sensible */
		return (-1);
	}
	/*
	 * ngroups_max is the maximum number of supplemental groups per
	 * process. if no supplemental groups are allowed, we're done.
	 */
	if (ngroups_max == 0)
		return (0);

	if ((groups = (gid_t *)calloc(ngroups_max, sizeof (gid_t))) == 0) {
		errno = ENOMEM;
		return (-1);
	}
	groups[0] = agroup;

	ngroups = _getgroupsbymember(uname, groups, (int)ngroups_max,
					(agroup >= 0) ? 1 : 0);
	if (ngroups < 0) {
		/* XXX -- man page does not define a value for errno in */
		/* this case.  Should be looked into sometime.	*/
		free(groups);
		return (-1);
	}

	retsave = setgroups(ngroups, groups);
	errsave = errno;

	free(groups);

	errno = errsave;
	return (retsave);
}
