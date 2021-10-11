/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vpopen.c	1.1	99/01/11 SMI"

/* vpopen - view path version of the open system call */

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include "vp.h"

#define	READ	0

int
vpopen(char *path, int oflag)
{
	char	buf[MAXPATH + 1];
	int	returncode;
	int	i;

	if ((returncode = open(path, oflag, 0666)) == -1 && *path != '/' &&
	    oflag == READ) {
		vpinit((char *)0);
		for (i = 1; i < vpndirs; i++) {
			(void) sprintf(buf, "%s/%s", vpdirs[i], path);
			if ((returncode = open(buf, oflag, 0666)) != -1) {
				break;
			}
		}
	}
	return (returncode);
}
