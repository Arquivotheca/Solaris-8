/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *	Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

/*
 * An application should not include this header directly.  Instead it
 * should be included only through the inclusion of other Sun headers.
 *
 * The contents of this header is limited to identifiers specified in the
 * C Standard.  Any new identifiers specified in future amendments to the
 * C Standard must be placed in this header.  If these new identifiers
 * are required to also be in the C++ Standard "std" namespace, then for
 * anything other than macro definitions, corresponding "using" directives
 * must also be added to <time.h.h>.
 */

#ifndef _ISO_TIME_ISO_H
#define	_ISO_TIME_ISO_H

#pragma ident	"@(#)time_iso.h	1.1	99/08/09 SMI" /* SVr4.0 1.18 */

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if __cplusplus >= 199711L
namespace std {
#endif

#ifndef NULL
#if defined(_LP64) && !defined(__cplusplus)
#define	NULL	0L
#else
#define	NULL	0
#endif
#endif

#if !defined(_SIZE_T) || __cplusplus >= 199711L
#define	_SIZE_T
#if defined(_LP64) || defined(_I32LPx)
typedef	unsigned long	size_t;		/* size of something in bytes */
#else
typedef unsigned	size_t;		/* (historical version) */
#endif
#endif	/* !_SIZE_T */

#if !defined(_CLOCK_T) || __cplusplus >= 199711L
#define	_CLOCK_T
typedef	long	clock_t;
#endif	/* !_CLOCK_T */

#if !defined(_TIME_T) || __cplusplus >= 199711L
#define	_TIME_T
typedef	long	time_t;
#endif	/* !_TIME_T */

#define	CLOCKS_PER_SEC		1000000

struct	tm {	/* see ctime(3) */
	int	tm_sec;
	int	tm_min;
	int	tm_hour;
	int	tm_mday;
	int	tm_mon;
	int	tm_year;
	int	tm_wday;
	int	tm_yday;
	int	tm_isdst;
};


#if defined(__STDC__)

extern char *asctime(const struct tm *);
extern clock_t clock(void);
extern char *ctime(const time_t *);
extern double difftime(time_t, time_t);
extern struct tm *gmtime(const time_t *);
extern struct tm *localtime(const time_t *);
extern time_t mktime(struct tm *);
extern time_t time(time_t *);
extern size_t strftime(char *, size_t, const char *, const struct tm *);

#else /* __STDC__ */

extern char *asctime();
extern clock_t clock();
extern char *ctime();
extern double difftime();
extern struct tm *gmtime();
extern struct tm *localtime();
extern time_t mktime();
extern time_t time();
extern size_t strftime();

#endif	/* __STDC__ */

#if __cplusplus >= 199711L
}
#endif /* end of namespace std */

#ifdef	__cplusplus
}
#endif

#endif	/* _ISO_TIME_ISO_H */
