/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)mkstemp.c	1.11	97/06/21 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

/*
******************************************************************


		PROPRIETARY NOTICE(Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice

Notice of copyright on this source code product does not indicate
publication.

	(c) 1986-1996  Sun Microsystems, Inc
	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
		All rights reserved.
******************************************************************* */

#include <sys/isa_defs.h>
#include <sys/feature_tests.h>

#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#pragma weak mkstemp64 = _mkstemp64
#else
#pragma weak mkstemp = _mkstemp
#endif

#include "synonyms.h"
#include <sys/fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <alloca.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


int
_mkstemp(char *as)
{
	int	fd;
	char	*tstr, *str, *mkret;

	if (as == NULL || *as == NULL)
		return (-1);

	tstr = alloca(strlen(as) + 1);
	(void) strcpy(tstr, as);

	str = tstr + (strlen(tstr) - 1);

	/*
	 * The following for() loop is doing work.  mktemp() will generate
	 * a different name each time through the loop.  So if the first
	 * name is used then keep trying until you find a free filename.
	 */

	for (; ; ) {
		if (*str == 'X') { /* If no trailing X's don't call mktemp. */
			mkret = mktemp(as);
			if (*mkret == '\0') {
				return (-1);
			}
		}
#if _FILE_OFFSET_BITS == 64
		if ((fd = open64(as, O_CREAT|O_EXCL|O_RDWR, 0600)) != -1) {
			return (fd);
		}
#else
		if ((fd = open(as, O_CREAT|O_EXCL|O_RDWR, 0600)) != -1) {
			return (fd);
		}
#endif  /* _FILE_OFFSET_BITS == 64 */

		/*
		 * If the error condition is other than EEXIST or if the
		 * file exists and there are no X's in the string
		 * return -1.
		 */

		if ((errno != EEXIST) || (*str != 'X')) {
			return (-1);
		}
		(void) strcpy(as, tstr);
	}
}
