/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)default.c	1.4	97/05/14 SMI"	/* SVr4.0 1.6	*/
/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "stdlib.h"

#include "lp.h"

/**
 ** getdefault() - READ THE NAME OF THE DEFAULT DESTINATION FROM DISK
 **/

char *
getdefault(void)
{
	return (loadline(Lp_Default));
}

/**
 ** putdefault() - WRITE THE NAME OF THE DEFAULT DESTINATION TO DISK
 **/

int
putdefault(char *dflt)
{
	int fd;

	if (!dflt || !*dflt)
		return (deldefault());

	if ((fd = open_locked(Lp_Default, "w", MODE_READ)) < 0)
		return (-1);

	fdprintf(fd, "%s\n", dflt);

	close(fd);
	return (0);
}

/**
 ** deldefault() - REMOVE THE NAME OF THE DEFAULT DESTINATION
 **/

int
deldefault(void)
{
	return (rmfile(Lp_Default));
}
