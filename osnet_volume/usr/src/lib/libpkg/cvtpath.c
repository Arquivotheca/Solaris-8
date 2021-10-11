/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)cvtpath.c	1.7	93/04/01 SMI"	/* SVr4.0  1.4	*/

#include <string.h>

extern char *root, *basedir; 		/* WHERE? */

void
cvtpath(char *path, char *copy)
{
	*copy++ = '/';
	if (root || (basedir && (*path != '/'))) {
		if (root && ((basedir == NULL) || (path[0] == '/') ||
		    (basedir[0] != '/'))) {
			/* look in root */
			(void) strcpy(copy, root + (*root == '/' ? 1 : 0));
			copy += strlen(copy);
			if (copy[-1] != '/')
				*copy++ = '/';
		}
		if (basedir && (*path != '/')) {
			(void) strcpy(copy,
			    basedir + (*basedir == '/' ? 1 : 0));
			copy += strlen(copy);
			if (copy[-1] != '/')
				*copy++ = '/';
		}
	}
	(void) strcpy(copy, path + (*path == '/' ? 1 : 0));
}
