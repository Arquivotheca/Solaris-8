/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)delclass.c	1.4	93/11/18 SMI"	/* SVr4.0 1.9	*/
/* LINTLIBRARY */

#include "stdio.h"
#include "errno.h"
#include "string.h"
#include "sys/types.h"
#include "string.h"

#include "lp.h"
#include "class.h"

static int		_delclass ( char * );

/**
 ** delclass() - WRITE CLASS OUT TO DISK
 **/

int
delclass(char *name)
{
	long			lastdir;

	if (!name || !*name) {
		errno = EINVAL;
		return (-1);
	}

	if (STREQU(NAME_ALL, name)) {
		lastdir = -1;
		while ((name = next_file(Lp_A_Classes, &lastdir)))
			if (_delclass(name) == -1)
				return (-1);
		return (0);
	} else
		return (_delclass(name));
}

/**
 ** _delclass()
 **/

static int
#if	defined(__STDC__)
_delclass (
	char *			name
)
#else
_delclass (name)
	char			*name;
#endif
{
	char			*path;

	if (!(path = getclassfile(name)))
		return (-1);
	if (rmfile(path) == -1) {
		Free (path);
		return (-1);
	}
	Free (path);
	return (0);
}

