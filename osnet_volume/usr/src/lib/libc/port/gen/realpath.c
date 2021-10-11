/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1987-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)realpath.c	1.12	98/01/07 SMI"	/* SVr4.0 1.2 */

/* LINTLIBRARY */

#pragma weak realpath = _realpath

#include "synonyms.h"
#include <sys/types.h>
#include <dirent.h>
#include <sys/param.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*
 * Canonicalize the path given in file_name, resolving away all symbolic link
 * components.  Store the result into the buffer named by resolved_name, which
 * must be long enough (MAXPATHLEN bytes will suffice).  Returns NULL
 * on failure and resolved_name on success.  On failure, to maintain
 * compatibility with the past, the contents of file_name will be copied
 * into resolved_name.
 */
char *
realpath(const char *file_name, char *resolved_name)
{
	char cwd[PATH_MAX];
	int len;

	if (file_name == NULL || resolved_name == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	/*
	 * Call resolvepath() to resolve all the symlinks in file_name,
	 * eliminate embedded "." components, and collapse embedded ".."
	 * components.  We may be left with leading ".." components.
	 */
	if ((len = resolvepath(file_name, resolved_name, PATH_MAX)) < 0) {
		(void) strncpy(resolved_name, file_name, PATH_MAX);
		return (NULL);		/* errno set by resolvepath() */
	}

	if (len >= PATH_MAX)	/* "can't happen" */
		len = PATH_MAX - 1;
	resolved_name[len] = '\0';

	if (*resolved_name == '/')	/* nothing more to do */
		return (resolved_name);

	/*
	 * Prepend the current working directory to the relative path.
	 * If the relative path is not empty (or "."), collapse all of the
	 * resulting embedded ".." components with trailing cwd components.
	 * We know that getcwd() returns a path name free of symlinks.
	 */
	if (getcwd(cwd, sizeof (cwd)) == NULL) {
		(void) strncpy(resolved_name, file_name, PATH_MAX);
		return (NULL);		/* errno set by getcwd() */
	}

	if (len != 0 && strcmp(resolved_name, ".") != 0) {
		char *relpath = resolved_name;
		char *endcwd = cwd + strlen(cwd);

		/*
		 * Eliminate ".." components from the relative path
		 * left-to-right, components from cwd right-to-left.
		 */
		relpath[len++] = '/';
		while (len >= 3 && strncmp(relpath, "../", 3) == 0) {
			relpath += 3;
			len -= 3;
			while (*--endcwd != '/')
				continue;
		}
		if (len == 0) {
			/* the relative path was all ".." components */
			*endcwd = '\0';
		} else {
			/* there are non-null components on both sides */
			relpath[--len] = '\0';
			*endcwd++ = '/';
			if (endcwd + len >= cwd + PATH_MAX) {
				(void) strncpy(resolved_name,
				    file_name, PATH_MAX);
				errno = ENAMETOOLONG;
				return (NULL);
			}
			(void) strcpy(endcwd, relpath);
		}
	}

	(void) strcpy(resolved_name, cwd);
	return (resolved_name);
}
