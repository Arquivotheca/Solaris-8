/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)time.h	1.6	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		time.h
 *
 *   Description:	various time-related datastructure, e.g., timeval,
 *			timestruc, hrtimer_t, timezone, etc.
 *
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All rights reserved. 	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef _SYS_TIME_H
#define	_SYS_TIME_H

/*
 * Structure returned by gettimeofday(2) system call,
 * and used in other calls.
 */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(_POSIX_SOURCE)

struct timeval {
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* and microseconds */
};

struct timezone {
	long	tz_minuteswest;	/* minutes west of Greenwich */
	long	tz_dsttime;	/* type of dst correction */
};
#define	DST_NONE	0	/* not on dst */
#define	DST_USA		1	/* USA style dst */
#define	DST_AUST	2	/* Australian style dst */
#define	DST_WET		3	/* Western European dst */
#define	DST_MET		4	/* Middle European dst */
#define	DST_EET		5	/* Eastern European dst */
#define	DST_CAN		6	/* Canada */
#define	DST_GB		7	/* Great Britain and Eire */
#define	DST_RUM		8	/* Rumania */
#define	DST_TUR		9	/* Turkey */
#define	DST_AUSTALT	10	/* Australian style with shift in 1986 */

/*
 * Operations on timevals.
 *
 * NB: timercmp does not work for >= or <=.
 */
#define	timerisset(tvp)		((tvp)->tv_sec || (tvp)->tv_usec)
#define	timercmp(tvp, uvp, cmp)	\
	((tvp)->tv_sec cmp (uvp)->tv_sec || \
	    (tvp)->tv_sec == (uvp)->tv_sec && (tvp)->tv_usec cmp (uvp)->tv_usec)
#define	timerclear(tvp)		(tvp)->tv_sec = (tvp)->tv_usec = 0

/*
 * Names of the interval timers, and structure
 * defining a timer setting.
 */
#define	ITIMER_REAL	0
#define	ITIMER_VIRTUAL	1
#define	ITIMER_PROF	2
#define	ITIMER_REALPROF	3

struct	itimerval {
	struct	timeval it_interval;	/* timer interval */
	struct	timeval it_value;	/* current value */
};

/*
 *	Definitions for commonly used resolutions.
 */

#define	SEC		1
#define	MILLISEC	1000
#define	MICROSEC	1000000
#define	NANOSEC		1000000000

#endif /* !defined(_POSIX_SOURCE) */

/*
 * Time expressed in seconds and nanoseconds
 */
typedef struct	timestruc {
	time_t 		tv_sec;		/* seconds */
	long		tv_nsec;	/* and nanoseconds */
} timestruc_t;

/*
 * Time expressed as a 64-bit free running counter. Use
 * "long long" for this when it becomes available.
 */
typedef	longlong_t	hrtimer_t;

#ifdef _KERNEL

/*
 * Bump a timestruc by a small number of nsec
 */

#define	BUMPTIME(t, nsec, flag) { \
	register timestruc_t	*tp = (t); \
\
	if ((tp->tv_nsec += (nsec)) >= NANOSEC) { \
		tp->tv_nsec -= NANOSEC; \
		tp->tv_sec++; \
		flag = 1; \
	} \
}

/*
 * result = a - b;
 */
#define	SUBTIME(result, a, b) { \
\
	(result).tv_sec = (a).tv_sec - (b).tv_sec; \
	if (((result).tv_nsec = (a).tv_nsec - (b).tv_nsec) < 0) { \
		(result).tv_sec--; \
		(result).tv_nsec += NANOSEC; \
	} \
}

/*
 * result = a + b;
 */
#define	ADDTIME(result, a, b) { \
\
	(result).tv_sec = (a).tv_sec + (b).tv_sec; \
	if (((result).tv_nsec = (a).tv_nsec + (b).tv_nsec) >= NANOSEC) { \
		(result).tv_sec++; \
		(result).tv_nsec -= NANOSEC; \
	} \
}

/*
 * result += a;
 */
#define	ADD_TO_TIME(result, a) { \
\
	(result).tv_sec += (a).tv_sec; \
	if (((result).tv_nsec += (a).tv_nsec) >= NANOSEC) { \
		(result).tv_sec++; \
		(result).tv_nsec -= NANOSEC; \
	} \
}

extern	timestruc_t	hrestime;

#ifdef __STDC__
extern	void	gethrtime(hrtimer_t _far *);
extern	void	hrtadd(hrtimer_t _far *result, hrtimer_t _far *a, hrtimer_t _far *b);
extern	void	hrtsub(hrtimer_t _far *result, hrtimer_t _far *a, hrtimer_t _far *b);
extern	long	hrtcmp(hrtimer_t _far *a, hrtimer_t _far *b);
extern	long	hrtiszero(hrtimer_t _far *);
extern	void	hrtzero(hrtimer_t _far *);
#else
extern	void	gethrtime();
extern	void	hrtadd();
extern	void	hrtsub();
extern	long	hrtcmp();
extern	long	hrtiszero();
extern	void	hrtzero();
#endif /* __STDC__ */

#endif /* _KERNEL */

#if !defined(_KERNEL) && !defined(_POSIX_SOURCE)
#if defined(__STDC__)
long adjtime(struct timeval _far *, struct timeval _far *);
long getitimer(long, struct itimerval _far *);
long setitimer(long, struct itimerval _far *, struct itimerval _far *);
#endif /* __STDC__ */
#if !defined(_XOPEN_SOURCE)

#include <time.h>
#endif
#endif /* !defined(_KERNEL) && !defined(_POSIX_SOURCE) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TIME_H */
