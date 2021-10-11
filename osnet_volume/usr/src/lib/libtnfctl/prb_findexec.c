/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma ident	"@(#)prb_findexec.c	1.2	96/05/20 SMI"

/*
 * interfaces to find an executable (from libc code)
 */

/* Copyright (c) 1988 AT&T */
/* All Rights Reserved   */

/* THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	 */
/* The copyright notice above does not evidence any		 */
/* actual or intended publication of such source code.	 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "prb_proc_int.h"

static const char *exec_cat(const char *s1, const char *s2, char *si);

prb_status_t
find_executable(const char *name, char *ret_path)
{
	const char	 *pathstr;
	char		fname[PATH_MAX + 2];
	const char	 *cp;
	struct stat	 stat_buf;

	if (*name == '\0') {
		return (prb_status_map(ENOENT));
	}
	if ((pathstr = getenv("PATH")) == NULL) {
		if (geteuid() == 0 || getuid() == 0)
			pathstr = "/usr/sbin:/usr/bin";
		else
			pathstr = "/usr/bin:";
	}
	cp = strchr(name, '/') ? (const char *) "" : pathstr;

	do {
		cp = exec_cat(cp, name, fname);
		if (stat(fname, &stat_buf) != -1) {
			/* successful find of the file */
			(void) strncpy(ret_path, fname, PATH_MAX + 2);
			return (PRB_STATUS_OK);
		}
	} while (cp);

	return (prb_status_map(ENOENT));
}



static const char *
exec_cat(const char *s1, const char *s2, char *si)
{
	char		   *s;
	/* number of characters in s2 */
	int			 cnt = PATH_MAX + 1;

	s = si;
	while (*s1 && *s1 != ':') {
		if (cnt > 0) {
			*s++ = *s1++;
			cnt--;
		} else
			s1++;
	}
	if (si != s && cnt > 0) {
		*s++ = '/';
		cnt--;
	}
	while (*s2 && cnt > 0) {
		*s++ = *s2++;
		cnt--;
	}
	*s = '\0';
	return (*s1 ? ++s1 : 0);
}
