#ident	"@(#)dogetpw.c	1.4	94/01/26 SMI"

/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <pwd.h>
#include <stdlib.h>
#include "getent.h"

/*
 * getpwnam - get entries from password database
 */
int
dogetpw(const char **list)
{
	struct passwd *pwp;
	int rc = EXC_SUCCESS;
	char *ptr;
	uid_t uid;


	if (list == NULL || *list == NULL) {
		while ((pwp = getpwent()) != NULL)
			(void) putpwent(pwp, stdout);
	} else {
		for (; *list != NULL; list++) {
			uid = strtol(*list, &ptr, 10);
			if (ptr == *list)
				pwp = getpwnam(*list);
			else
				pwp = getpwuid(uid);
			if (pwp == NULL)
				rc = EXC_NAME_NOT_FOUND;
			else
				(void) putpwent(pwp, stdout);
		}
	}

	return (rc);
}
