/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)qstrdup.c	1.8	93/03/10 SMI"	/* SVr4.0 1.1.1.1	*/

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>

extern void	quit(int exitval);

#define	ERR_MEMORY	"memory allocation failure, errno=%d"

char *
qstrdup(char *s)
{
	register char *pt = NULL;

	if (s && *s) {
		pt = (char *) calloc((unsigned)(strlen(s) + 1), sizeof (char));
		if (pt == NULL) {
			progerr(gettext(ERR_MEMORY), errno);
			quit(99);
		}
		(void) strcpy(pt, s);
	}
	return (pt);
}
