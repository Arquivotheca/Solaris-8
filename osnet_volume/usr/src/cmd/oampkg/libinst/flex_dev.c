/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

/*LINTLIBRARY*/
#ident	"@(#)flex_dev.c	1.7	93/03/31"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <libintl.h>
#include <pkglib.h>
#include "libadm.h"

#define	ERR_CHDIR	"unable to chdir back to <%s>, errno=%d"
#define	ERR_GETCWD	"unable to determine the current working directory, " \
			"errno=%d"

char *
flex_device(char *device_name, int dev_ok)
{
	char		new_device_name[PATH_MAX];
	char		*np = device_name;
	char		*cwd = NULL;

	if (!device_name || !*device_name)		/* NULL or empty */
		return (np);

	if (dev_ok == 1 && listdev(np) != (char **) NULL) /* device.tab */
		return (np);

	if (!strncmp(np, "/dev", 4))			/* /dev path */
		return (np);

	if ((cwd = getcwd(NULL, PATH_MAX)) == NULL) {
		progerr(gettext(ERR_GETCWD), errno);
		exit(99);
	}

	if (realpath(np, new_device_name) == NULL) {	/* path */
		if (chdir(cwd) == -1) {
			progerr(gettext(ERR_CHDIR), cwd, errno);
			(void) free(cwd);
			exit(99);
		}
		if (*np != '/' && dev_ok == 2) {
			(void) sprintf(new_device_name, "%s/%s", cwd, np);
			canonize(new_device_name);
			if ((np = strdup(new_device_name)) == NULL)
				np = device_name;
		}
		(void) free(cwd);
		return (np);
	}

	if (strcmp(np, new_device_name)) {
		if ((np = strdup(new_device_name)) == NULL)
			np = device_name;
	}

	(void) free(cwd);
	return (np);
}
