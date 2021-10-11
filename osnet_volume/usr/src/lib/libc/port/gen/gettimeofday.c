/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)gettimeofday.c	1.9	94/05/16 SMI"	/* SVr4.0 1.3	*/

#ifndef DSHLIB
#ifdef __STDC__
	#pragma weak gettimeofday = _gettimeofday
	#pragma weak settimeofday = _settimeofday
#endif
#endif
#include "synonyms.h"
#include <sys/time.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/evecb.h>
#include <sys/hrtcntl.h>
#include <errno.h>

extern int stime();

/*
 * Get the time of day information.
 * BSD compatibility on top of SVr4 facilities:
 * u_sec always zero, and don't do anything with timezone pointer.
 */
int
gettimeofday(struct timeval *tp, void *tzp)
{
#if 0	/* Kludge until hrt stuff is working */
	hrtime_t	tod;
	long		rval;

        if (tp == NULL)
                return (0);

	tod.hrt_res = MICROSEC;
	rval = hrtcntl(HRT_TOFD, CLK_STD, NULL, &tod);
	if (rval != 0)
		return(-1);

        tp->tv_sec = (long)tod.hrt_secs;
        tp->tv_usec = tod.hrt_rem;
#else	/* 0 */
	if (time(&(tp->tv_sec)) == (time_t)-1)
		return(-1);
	tp->tv_usec = 0;
#endif	/* 0 */
        return (0);
}

/*
 * Set the time.
 * Don't do anything with the timezone information.
 */
int
settimeofday(tp)
	struct timeval *tp;
{
        time_t t;                       /* time in seconds */

        if (tp == NULL)
                return (0);

	if (tp->tv_sec < 0 || tp->tv_usec < 0 ||
		tp->tv_usec >= MICROSEC) {
		errno = EINVAL;
		return(-1);
	}

        t = (time_t) tp->tv_sec;
        if (tp->tv_usec >= (MICROSEC/2))
                /* round up */
                t++;

        return(stime(&t));
}
