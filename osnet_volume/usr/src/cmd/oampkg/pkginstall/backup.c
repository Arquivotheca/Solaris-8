/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)backup.c	1.6	93/03/10 SMI"	/* SVr4.0 1.3 */

#include <stdio.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>

extern char	savlog[];
extern int	warnflag;

void
backup(char *path, int mode)
{
	static int	count = 0;
	static FILE	*fp;

	/* mode probably used in the future */
	if (count++ == 0) {
		if ((fp = fopen(savlog, "w")) == NULL) {
			logerr(gettext("WARNING: unable to open logfile <%s>"),
			    savlog);
			warnflag++;
		}
	}

	if (fp == NULL)
		return;

	(void) fprintf(fp, "%s%s", path, mode ? "\n" :
	    gettext(" <attributes only>\n"));
	/* we don't really back anything up; we just log the pathname */
}
