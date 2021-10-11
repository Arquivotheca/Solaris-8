/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)quit.c	1.7	93/03/09 SMI"	/* SVr4.0  1.1.2.1	*/
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <pkgdev.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "libadm.h"

extern struct pkgdev pkgdev;
extern char	pkgloc[], *t_pkgmap, *t_pkginfo;

extern int	started;

#define	MSG_COMPLETE	"## Packaging complete.\n"
#define	MSG_TERM	"## Packaging terminated at user request.\n"
#define	MSG_ERROR	"## Packaging was not successful.\n"

void
quit(int retcode)
{
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGHUP, SIG_IGN);

	if (retcode == 3)
		(void) fprintf(stderr, gettext(MSG_TERM));
	else if (retcode)
		(void) fprintf(stderr, gettext(MSG_ERROR));
	else
		(void) fprintf(stderr, gettext(MSG_COMPLETE));

	if (retcode && started)
		(void) rrmdir(pkgloc); /* clean up output directory */

	if (pkgdev.mount)
		(void) pkgumount(&pkgdev);

	if (t_pkgmap)
		(void) unlink(t_pkgmap);
	if (t_pkginfo)
		(void) unlink(t_pkginfo);
	exit(retcode);
}
