/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vpfopen.c	1.1	99/01/06 SMI"

/* vpfopen - view path version of the fopen library function */

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "vp.h"

FILE *
vpfopen(char *filename, char *type)
{
	char	buf[MAXPATH + 1];
	FILE	*returncode;
	int	i;

	if ((returncode = fopen(filename, type)) == NULL &&
	    filename[0] != '/' && strcmp(type, "r") == 0) {
		vpinit((char *)0);
		for (i = 1; i < vpndirs; i++) {
			(void) sprintf(buf, "%s/%s", vpdirs[i], filename);
			if ((returncode = fopen(buf, type)) != NULL) {
				break;
			}

		}
	}
	return (returncode);
}
