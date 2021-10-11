/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)time.h	1.6	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		time.h
 *
 *   Description:	time_t, tm, time-related function prototypes
 *
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All rights reserved. 	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _TIME_H
#define	_TIME_H

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef NULL
#define	NULL	0
#endif

#ifndef _SIZE_T
#define	_SIZE_T
typedef unsigned long	size_t;
#endif
#ifndef _CLOCK_T
#define	_CLOCK_T
typedef long	clock_t;
#endif
#ifndef _TIME_T
#define	_TIME_T
typedef long	time_t;
#endif

#define	CLOCKS_PER_SEC		1000000

struct	tm {	/* see ctime(3) */
	long	tm_sec;
	long	tm_min;
	long	tm_hour;
	long	tm_mday;
	long	tm_mon;
	long	tm_year;
	long	tm_wday;
	long	tm_yday;
	long	tm_isdst;
};

#if defined(__STDC__)

extern clock_t clock(void);
extern double difftime(time_t, time_t);
extern time_t mktime(struct tm _far *);
extern time_t time(time_t _far *);
extern char _far *asctime(const struct tm _far *);
extern char _far *ctime (const time_t _far *);
extern struct tm _far *gmtime(const time_t _far *);
extern struct tm _far *localtime(const time_t _far *);
extern size_t strftime(char _far *, size_t, const char _far *, const struct tm _far *);

#if __STDC__ == 0 || defined(_POSIX_SOURCE) || defined(_XOPEN_SOURCE)
extern void tzset(void);

extern char _far *tzname[2];

#ifndef CLK_TCK
#define	CLK_TCK	_sysconf(3)	/* 3B2 clock ticks per second */
				/* 3 is _SC_CLK_TCK */
#endif

#if (__STDC__ == 0 && !defined(_POSIX_SOURCE)) || defined(_XOPEN_SOURCE)
extern long timezone;
extern long daylight;
#endif

#endif

#if __STDC__ == 0 && !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
extern long cftime(char _far *, char _far *, const time_t _far *);
extern long ascftime(char _far *, const char _far *, const struct tm _far *);
extern long altzone;
extern struct tm _far *getdate(const char _far *);
extern long getdate_err;
#endif

#else

extern long clock();
extern double difftime();
extern time_t mktime();
extern time_t time();
extern size_t strftime();
extern struct tm _far *gmtime(), _far *localtime();
extern char _far *ctime(), _far *asctime();
extern long cftime(), ascftime();
extern void tzset();

extern long timezone, altzone;
extern long daylight;
extern char _far *tzname[2];

extern struct tm _far *getdate();
extern long getdate_err;

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _TIME_H */
