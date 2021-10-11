/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/


/* C library -- gettimeofday					*/
/* int gettimeofday (struct timeval *tp);			*/

	.file	"gettimeofday.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(gettimeofday,function)

#include "SYS.h"

/*
 * The interface below calls the trap (0x27) to get the timestamp in 
 * secs and nsecs. It than converts the nsecs value into usecs before
 * it returns. It uses the following logic to do the divide by 1000:
 *
 *		int
 *		div1000(int nsec)
 *		{
 *		u_long usec;
 *
 *		usec = nsec + (nsec >> 2);
 *		usec = nsec + (usec >> 1);
 *		usec = nsec + (usec >> 2);
 *		usec = nsec + (usec >> 4);
 *		usec = nsec - (usec >> 3);
 *		usec = nsec + (usec >> 2);
 *		usec = nsec + (usec >> 3);
 *		usec = nsec + (usec >> 4);
 *		usec = nsec + (usec >> 1);
 *		usec = nsec + (usec >> 6);
 *		return (usec >> 10);
 *		}
 */

	ENTRY(_gettimeofday)
	brz,pn	%o0, 1f
	mov	%o0, %o5
	ta	ST_GETHRESTIME
	stn	%o0, [%o5]
	udivx	%o1, 1000, %o2		! o2 = ns / 1000
	stn	%o2, [%o5 + CLONGSIZE]
1:	RETC
	SET_SIZE(_gettimeofday)
