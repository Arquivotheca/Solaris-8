/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)proc_get_info.c	1.1	97/12/23 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "libproc.h"

/*
 * These several routines simply get the indicated /proc structures
 * for a process identified by process ID.  They are convenience
 * functions for one-time operations.  They do the mechanics of
 * open() / read() / close() of the necessary /proc files so the
 * caller's code can look relatively less cluttered.
 */

/*
 * 'ngroups' is the number of supplementary group entries allocated in
 * the caller's cred structure.  It should equal zero or one unless extra
 * space has been allocated for the group list by the caller, like this:
 *    credp = malloc(sizeof (prcred_t) + (ngroups - 1) * sizeof (gid_t));
 */
int
proc_get_cred(pid_t pid, prcred_t *credp, int ngroups)
{
	char fname[64];
	int fd;
	int rv = -1;
	ssize_t minsize = sizeof (*credp) - sizeof (gid_t);
	size_t size = minsize + ngroups * sizeof (gid_t);

	(void) snprintf(fname, sizeof (fname), "/proc/%d/cred", (int)pid);
	if ((fd = open(fname, O_RDONLY)) >= 0) {
		if (read(fd, credp, size) >= minsize)
			rv = 0;
		(void) close(fd);
	}
	return (rv);
}

int
proc_get_psinfo(pid_t pid, psinfo_t *psp)
{
	char fname[64];
	int fd;
	int rv = -1;

	(void) snprintf(fname, sizeof (fname), "/proc/%d/psinfo", (int)pid);
	if ((fd = open(fname, O_RDONLY)) >= 0) {
		if (read(fd, psp, sizeof (*psp)) == sizeof (*psp))
			rv = 0;
		(void) close(fd);
	}
	return (rv);
}

int
proc_get_status(pid_t pid, pstatus_t *psp)
{
	char fname[64];
	int fd;
	int rv = -1;

	(void) snprintf(fname, sizeof (fname), "/proc/%d/status", (int)pid);
	if ((fd = open(fname, O_RDONLY)) >= 0) {
		if (read(fd, psp, sizeof (*psp)) == sizeof (*psp))
			rv = 0;
		(void) close(fd);
	}
	return (rv);
}

/*
 * Get the process's aux vector.
 * 'naux' is the number of aux entries in the caller's buffer.
 * We return the number of aux entries actually fetched from
 * the process (less than or equal to 'naux') or -1 on failure.
 */
int
proc_get_auxv(pid_t pid, auxv_t *pauxv, int naux)
{
	char fname[64];
	int fd;
	int rv = -1;

	(void) snprintf(fname, sizeof (fname), "/proc/%d/auxv", (int)pid);
	if ((fd = open(fname, O_RDONLY)) >= 0) {
		if ((rv = read(fd, pauxv, naux * sizeof (auxv_t))) >= 0)
			rv /= sizeof (auxv_t);
		(void) close(fd);
	}
	return (rv);
}
