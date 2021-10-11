/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)getclass.c	1.5	97/05/14 SMI"	/* SVr4.0 1.9	*/
/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "string.h"
#include "errno.h"
#include "sys/types.h"

#include "lp.h"
#include "class.h"

/**
 ** getclass() - READ CLASS FROM TO DISK
 **/

CLASS *
getclass(char *name)
{
	static long		lastdir		= -1;

	static CLASS		clsbuf;

	char			*file,
				buf[BUFSIZ];

	int fd;


	if (!name || !*name) {
		errno = EINVAL;
		return (0);
	}

	/*
	 * Getting ``all''? If so, jump into the directory
	 * wherever we left off.
	 */
	if (STREQU(NAME_ALL, name)) {
		if (!(name = next_file(Lp_A_Classes, &lastdir)))
			return (0);
	} else
		lastdir = -1;

	/*
	 * Get the class list.
	 */

	if (!(file = getclassfile(name)))
		return (0);

	if ((fd = open_locked(file, "r", 0)) < 0) {
		Free (file);
		return (0);
	}
	Free (file);

	if (!(clsbuf.name = Strdup(name))) {
		close(fd);
		errno = ENOMEM;
		return (0);
	}

	clsbuf.members = 0;
	errno = 0;
	while (fdgets(buf, BUFSIZ, fd)) {
		buf[strlen(buf) - 1] = 0;
		addlist (&clsbuf.members, buf);
	}
	if (errno != 0) {
		int			save_errno = errno;

		freelist (clsbuf.members);
		Free (clsbuf.name);
		close(fd);
		errno = save_errno;
		return (0);
	}
	close(fd);

	if (!clsbuf.members) {
		Free (clsbuf.name);
		errno = EBADF;
		return (0);
	}

	return (&clsbuf);
}
