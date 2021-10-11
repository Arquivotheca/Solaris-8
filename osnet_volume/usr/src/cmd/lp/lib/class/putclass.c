/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)putclass.c	1.4	97/05/14 SMI"	/* SVr4.0 1.7	*/
/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "errno.h"
#include "sys/types.h"
#include "string.h"

#include "lp.h"
#include "class.h"

/**
 ** putclass() - WRITE CLASS OUT TO DISK
 **/

int
putclass(char *name, CLASS *clsbufp)
{
	char			*file;
	int fd;

	if (!name || !*name) {
		errno = EINVAL;
		return (-1);
	}

	if (STREQU(NAME_ALL, name)) {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * Open the class file and write out the class members.
	 */

	if (!(file = getclassfile(name)))
		return (-1);

	if ((fd = open_locked(file, "w", MODE_READ)) < 0) {
		Free (file);
		return (-1);
	}
	Free (file);

	errno = 0;
	fdprintlist(fd, clsbufp->members);
	if (errno != 0) {
		close(fd);
		return (-1);
	}
	close(fd);

	return (0);
}
