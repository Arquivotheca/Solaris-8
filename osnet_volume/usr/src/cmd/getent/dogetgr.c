#ident	"@(#)dogetgr.c	1.4	94/01/26 SMI"

/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <grp.h>
#include <stdlib.h>
#include "getent.h"


static int
putgrent(const struct group *grp, FILE *fp)
{
	char **mem;
	int rc = 0;

	if (grp == NULL) {
		return (1);
	}

	if (fprintf(fp, "%s:%s:%d:",
		    grp->gr_name != NULL ? grp->gr_name : "",
		    grp->gr_passwd != NULL ? grp->gr_passwd : "",
		    grp->gr_gid) == EOF)
		rc = 1;

	mem = grp ->gr_mem;

	if (mem != NULL) {
		if (*mem != NULL)
			if (fputs(*mem++, fp) == EOF)
				rc = 1;

		while (*mem != NULL)
			if (fprintf(fp, ",%s", *mem++) == EOF)
				rc = 1;
	}
	if (putc('\n', fp) == EOF)
		rc = 1;
	return (rc);
}

int
dogetgr(const char **list)
{
	struct group *grp;
	int rc = EXC_SUCCESS;
	char *ptr;
	gid_t gid;

	if (list == NULL || *list == NULL) {
		while ((grp = getgrent()) != NULL)
			(void) putgrent(grp, stdout);
	} else {
		for (; *list != NULL; list++) {
			gid = strtol(*list, &ptr, 10);
			if (ptr == *list)
				grp = getgrnam(*list);
			else
				grp = getgrgid(gid);
			if (grp == NULL)
				rc = EXC_NAME_NOT_FOUND;
			else
				(void) putgrent(grp, stdout);
		}
	}

	return (rc);
}
