/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*
 *
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 */

#pragma ident	"@(#)times.c	1.4	97/06/26 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/times.h>
#include "libc.h"

/*
 * Backwards compatible times.
 * BSD times() returns 0 if successful, vs sys5's times()
 * whih returns the elapsed real times in ticks.
 */

/*
 * This is defined in sys/_times.s
 * This extern cannot be in libc.h due to name conflict with synonyms.h
 */
extern int _times(struct tms *);

clock_t
times(struct tms *tmsp)
{
	int	error;

	errno = 0;
	if (!tmsp) {
		errno = EFAULT;
		return (-1);
	}

	error = _times(tmsp);
	return (error == -1 ? error : 0);
}
