/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) Sun Microsystems Inc. 1991		*/

/*
 *	Note that this file does not exist in the SVr4 base, but
 *	is largely derived from sys/hrtcntl.h, hence the AT&T
 *	copyright is propagated.  SVr4.0 1.9
 */

#ident	"@(#)dispadmin.h	1.3	92/07/14 SMI"

/*
 * The following is an excerpt from <sys/hrtcntl.h>. HRT timers are not 
 * supported by SunOS (which will support the POSIX definition). Dispadmin 
 * uses the hrt routine _hrtnewres because it coincidentally does the 
 * right thing. These defines allow this routine to be locally included 
 * in dispadmin (rather than exported in libc). This should be improved in 
 * the long term.
 */ 

/*
 *	Definitions for specifying rounding mode.
 */

#define HRT_TRUNC	0	/* Round results down.	*/
#define HRT_RND		1	/* Round results (rnd up if fractional	*/
				/*   part >= .5 otherwise round down).	*/
#define	HRT_RNDUP	2	/* Always round results up.	*/

/*
 *	Structure used to represent a high-resolution time-of-day
 *	or interval.
 */

typedef struct hrtimer {
	ulong	hrt_secs;	/* Seconds.				*/
	long	hrt_rem;	/* A value less than a second.		*/
	ulong	hrt_res;	/* The resolution of hrt_rem.		*/
} hrtimer_t;
