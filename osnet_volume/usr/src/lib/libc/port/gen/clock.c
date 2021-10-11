/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)clock.c	1.13	97/02/17 SMI"	/* SVr4.0 1.6.2.3	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <sys/times.h>
#include <time.h>
#include <sys/param.h>	/* for HZ (clock frequency in Hz) */

#define	TIMES(B)	(B.tms_utime+B.tms_stime+B.tms_cutime+B.tms_cstime)


clock_t
clock(void)
{
	struct tms buffer;
	static int Hz = 0;
	static clock_t first;
	extern int gethz(void);		/* XXX should be in a header file! */

	if (times(&buffer) == (clock_t)-1)
		return ((clock_t)-1);
	if (Hz == 0) {
		if ((Hz = gethz()) == 0)
			Hz = HZ;
		first = TIMES(buffer);
	}

	return ((TIMES(buffer) - first) * (CLOCKS_PER_SEC/Hz));
}
