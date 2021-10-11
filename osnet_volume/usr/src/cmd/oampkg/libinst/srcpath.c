/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)srcpath.c	1.5	93/03/09 SMI"	/* SVr4.0 1.1	*/

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

char *
srcpath(char *d, char *p, int part, int nparts)
{
	static char tmppath[PATH_MAX];
	char	*copy;

	copy = tmppath;
	if (d) {
		(void) strcpy(copy, d);
		copy += strlen(copy);
	} else
		copy[0] = '\0';

	if (nparts > 1) {
		(void) sprintf(copy,
			((p[0] == '/') ? "/root.%d%s" : "/reloc.%d/%s"),
			part, p);
	} else {
		(void) sprintf(copy,
			((p[0] == '/') ? "/root%s" : "/reloc/%s"), p);
	}
	return (tmppath);
}
