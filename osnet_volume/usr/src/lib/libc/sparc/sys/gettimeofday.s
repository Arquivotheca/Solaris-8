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
	tst	%o0
	be	1f
	mov	%o0, %o5
	ta	ST_GETHRESTIME
	st	%o0, [%o5]
	sra	%o1, 2, %o2		! o2 = ns * .25
	add	%o1, %o2, %o2		! o2 = ns * 1.25
	srl	%o2, 1, %o2		! o2 = o2 * 0.5 = ns * 0.625
	add	%o1, %o2, %o2		! o2 = ns * 1.625
	srl	%o2, 2, %o2		! o2 = o2 * .25 = ns * 0.40625
	add	%o1, %o2, %o2		! o2 = ns * 1.40625
	srl	%o2, 4, %o2		! o2 = o2 / 16 = ns * 0.1015625
	add	%o1, %o2, %o2		! o2 = ns * 1.1015625
	srl	%o2, 3, %o2		! o2 = o2 / 8 = ns * 0.1376953125
	sub	%o1, %o2, %o2		! o2 = ns * 0.862304...
	srl	%o2, 2, %o2		! o2 = ns * 0.215761...
	add	%o1, %o2, %o2		! o2 = ns * 1.215761
	srl	%o2, 3, %o2		! o2 = o2 / 8 = ns * 0.026947...
	add	%o1, %o2, %o2		! o2 = ns * 1.026947...
	srl	%o2, 4, %o2		! o2 = o2 / 16 = ns * 0.06418..
	add	%o1, %o2, %o2		! o2 = ns * 1.06418...
	srl	%o2, 1, %o2		! o2 = ns * 0.53209...
	add	%o1, %o2, %o2		! o2 = ns * 1.53209...
	srl	%o2, 6, %o2		! o2 = o2 / 64 = ns * 0.023938...
	add	%o1, %o2, %o1		! o2 = ns * 1.024
	srl	%o1, 10, %o1		! o2 = o2 / 1024 = microseconds
	st	%o1, [%o5+0x4]
1:	RETC
	SET_SIZE(_gettimeofday)
