/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)filtertable.c	1.4	97/05/14 SMI"	/* SVr4.0 1.6	*/
/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "errno.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

#include "lp.h"
#include "filters.h"

/**
 ** get_and_load() - LOAD REGULAR FILTER TABLE
 **/

int
get_and_load()
{
	register char		*file;

	if (!(file = getfilterfile(FILTERTABLE)))
		return (-1);
	if (loadfilters(file) == -1) {
		Free (file);
		return (-1);
	}
	Free (file);
	return (0);
}

/**
 ** open_filtertable()
 **/

int
open_filtertable(char *file, char *mode)
{
	int			freeit;

	int fd;

	if (!file) {
		if (!(file = getfilterfile(FILTERTABLE)))
			return (0);
		freeit = 1;
	} else
		freeit = 0;
	
	fd = open_locked(file, mode, MODE_READ);

	if (freeit)
		Free (file);

	return (fd);
}
