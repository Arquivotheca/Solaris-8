/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vpstat.c	1.1	99/01/11 SMI"

/* vpstat - view path version of the stat system call */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "vp.h"

int
vpstat(char *path, struct stat *statp)
{
	char	buf[MAXPATH + 1];
	int	returncode;
	int	i;

	if ((returncode = stat(path, statp)) == -1 && path[0] != '/') {
		vpinit((char *)0);
		for (i = 1; i < vpndirs; i++) {
			(void) sprintf(buf, "%s/%s", vpdirs[i], path);
			if ((returncode = stat(buf, statp)) != -1) {
				break;
			}
		}
	}
	return (returncode);
}
