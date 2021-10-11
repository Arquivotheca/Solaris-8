/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)getdtblsize.c	1.6	96/11/14 SMI"	/* SVr4.0 1.2	*/

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

	(c) 1986, 1987, 1988, 1989  Sun Microsystems, Inc
	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
			All rights reserved.
******************************************************************* */

/*LINTLIBRARY*/
#include "synonyms.h"

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
/*
 * getdtablesize is implemented on top of getrlimit's
 * RLIMIT_NOFILE feature. The current (Soft) limit is returned.
 */
int
getdtablesize(void)
{
	struct rlimit rip;

	if (getrlimit(RLIMIT_NOFILE, &rip) != -1)
		return ((int)rip.rlim_cur);
}
