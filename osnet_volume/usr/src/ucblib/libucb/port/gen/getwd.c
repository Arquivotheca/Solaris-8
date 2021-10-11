/*
 * Copyright (c) 1995-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)getwd.c	1.7	1.7 SMI"

/*LINTLIBRARY*/

#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

char *
getwd(char *pathname)
{
	char *c;
	long val;

	if ((val = pathconf(".", _PC_PATH_MAX)) == -1L)
		val = MAXPATHLEN + 1;

	if ((c = getcwd(pathname, val)) == NULL) {
		if (errno == EACCES)
			(void) strcpy(pathname,
				"getwd: a parent directory cannot be read");
		else if (errno == ERANGE)
			(void) strcpy(pathname, "getwd: buffer too small");
		else
			(void) strcpy(pathname, "getwd: failure occurred");
		return (0);
	}

	return (c);
}
