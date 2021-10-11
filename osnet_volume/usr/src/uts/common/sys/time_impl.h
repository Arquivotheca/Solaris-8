/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Implementation-private.  This header should not be included
 * directly by an application.  The application should instead
 * include <time.h> which includes this header.
 */

#ifndef _SYS_TIME_IMPL_H
#define	_SYS_TIME_IMPL_H

#pragma ident	"@(#)time_impl.h	1.5	99/10/05 SMI"

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_ASM

#ifndef _TIME_T
#define	_TIME_T
typedef	long	time_t;		/* time of day in seconds */
#endif	/* _TIME_T */

/*
 * Time expressed in seconds and nanoseconds
 */

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(_POSIX_C_SOURCE > 2) || defined(__EXTENSIONS__)
typedef struct  timespec {		/* definition per POSIX.4 */
	time_t		tv_sec;		/* seconds */
	long		tv_nsec;	/* and nanoseconds */
} timespec_t;

#if defined(_SYSCALL32)

#include <sys/types32.h>

#define	TIMESPEC32_TO_TIMESPEC(ts, ts32)	{	\
	(ts)->tv_sec = (time_t)(ts32)->tv_sec;		\
	(ts)->tv_nsec = (ts32)->tv_nsec;		\
}

#define	TIMESPEC_TO_TIMESPEC32(ts32, ts)	{	\
	(ts32)->tv_sec = (time32_t)(ts)->tv_sec;	\
	(ts32)->tv_nsec = (ts)->tv_nsec;		\
}

#define	TIMESPEC_OVERFLOW(ts)		\
	((ts)->tv_sec < TIME32_MIN || (ts)->tv_sec > TIME32_MAX)

#endif	/* _SYSCALL32 */

typedef struct timespec timestruc_t;	/* definition per SVr4 */

#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE_))... */

/*
 * The following has been left in for backward compatibility. Portable
 * applications should not use the structure name timestruc.
 */

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)
#define	timestruc	timespec	/* structure name per SVr4 */
#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

/*
 * Timer specification
 */
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(_POSIX_C_SOURCE > 2) || defined(__EXTENSIONS__)
typedef struct itimerspec {		/* definition per POSIX.4 */
	struct timespec	it_interval;	/* timer period */
	struct timespec	it_value;	/* timer expiration */
} itimerspec_t;

#if defined(_SYSCALL32)

#define	ITIMERSPEC32_TO_ITIMERSPEC(it, it32)	{	\
	TIMESPEC32_TO_TIMESPEC(&(it)->it_interval, &(it32)->it_interval); \
	TIMESPEC32_TO_TIMESPEC(&(it)->it_value, &(it32)->it_value);	\
}

#define	ITIMERSPEC_TO_ITIMERSPEC32(it32, it)	{	\
	TIMESPEC_TO_TIMESPEC32(&(it32)->it_interval, &(it)->it_interval); \
	TIMESPEC_TO_TIMESPEC32(&(it32)->it_value, &(it)->it_value);	\
}

#define	ITIMERSPEC_OVERFLOW(it)				\
	(TIMESPEC_OVERFLOW(&(it)->it_interval) &&	\
	TIMESPEC_OVERFLOW(&(it)->it_value))

#endif	/* _SYSCALL32 */

#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) ... */

#endif	/* _ASM */

#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) || \
	(_POSIX_C_SOURCE > 2) || defined(__EXTENSIONS__)

#define	__CLOCK_REALTIME0	0	/* wall clock, bound to LWP */
#define	CLOCK_VIRTUAL		1	/* user CPU usage clock */
#define	CLOCK_PROF		2	/* user and system CPU usage clock */
#define	__CLOCK_REALTIME3	3	/* wall clock, not bound */
#define	CLOCK_HIGHRES		4	/* high resolution clock */

#ifdef _KERNEL
#define	CLOCK_MAX		5
#endif

/*
 * Define CLOCK_REALTIME as per-process if PTHREADS or explicitly requested
 *   NOTE: In the future, per-LWP semantics will be removed and
 *   __CLOCK_REALTIME0 will have per-process semantics (see timer_create(3R))
 */
#if (_POSIX_C_SOURCE >= 199506L) || defined(_POSIX_PER_PROCESS_TIMER_SOURCE)
#define	CLOCK_REALTIME	__CLOCK_REALTIME3
#else
#define	CLOCK_REALTIME	__CLOCK_REALTIME0
#endif

#define	TIMER_RELTIME	0x0		/* set timer relative */
#define	TIMER_ABSTIME	0x1		/* set timer absolute */

#endif	/* (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))... */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TIME_IMPL_H */
