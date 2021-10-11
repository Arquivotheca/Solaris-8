/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *	Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef _TIME_H
#define	_TIME_H

#pragma ident	"@(#)time.h	1.39	99/08/10 SMI"	/* SVr4.0 1.18 */

#include <sys/feature_tests.h>
#include <sys/types.h>
#include <iso/time_iso.h>

/*
 * Allow global visibility for symbols defined in
 * C++ "std" namespace in <iso/time_iso.h>.
 */
#if __cplusplus >= 199711L
using std::size_t;
using std::clock_t;
using std::time_t;
using std::tm;
using std::asctime;
using std::clock;
using std::ctime;
using std::difftime;
using std::gmtime;
using std::localtime;
using std::mktime;
using std::time;
using std::strftime;
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _CLOCKID_T
#define	_CLOCKID_T
typedef int	clockid_t;
#endif

#ifndef _TIMER_T
#define	_TIMER_T
typedef int	timer_t;
#endif

#if defined(__STDC__)

#if	defined(__EXTENSIONS__) || defined(_REENTRANT) || \
	    (_POSIX_C_SOURCE - 0 >= 199506L)
extern struct tm *gmtime_r(const time_t *, struct tm *);
extern struct tm *localtime_r(const time_t *, struct tm *);
#endif	/* defined(__EXTENSIONS__) || defined(_REENTRANT) .. */

#if (__STDC__ == 0 && !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(defined(_XOPEN_SOURCE) && _XOPEN_VERSION - 0 >= 4) || \
	defined(__EXTENSIONS__)
extern char *strptime(const char *, const char *, struct tm *);

#ifdef _STRPTIME_DONTZERO
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname strptime __strptime_dontzero
#else	/* __PRAGMA_REDEFINE_EXTNAME */
extern char *__strptime_dontzero(const char *, const char *, struct tm *);
#define	strptime	__strptime_dontzero
#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif	/* _STRPTIME_DONTZERO */

#endif /* (__STDC__ == 0 && !defined(_POSIX_C_SOURCE)... */

#if defined(__EXTENSIONS__) || ((__STDC__ - 0 == 0) && \
	!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	(_POSIX_C_SOURCE > 2)
#include <sys/time_impl.h>

/*
 * Neither X/Open nor POSIX allow the inclusion of <signal.h> for the
 * definition of the sigevent structure.  Both require the inclusion
 * of <signal.h> and <time.h> when using the timer_create() function.
 * However, X/Open also specifies that the sigevent structure be defined
 * in <time.h> as described in the header <signal.h>.  This prevents
 * compiler warnings for applications that only include <time.h> and not
 * also <signal.h>.  The sigval union and the sigevent structure is
 * therefore defined both here and in <sys/siginfo.h> which gets included
 * via inclusion of <signal.h>.
 */
#ifndef	_SIGVAL
#define	_SIGVAL
union sigval {
	int	sival_int;	/* integer value */
	void	*sival_ptr;	/* pointer value */
};
#endif	/* _SIGVAL */

#ifndef	_SIGEVENT
#define	_SIGEVENT
struct sigevent {
	int		sigev_notify;	/* notification mode */
	int		sigev_signo;	/* signal number */
	union sigval	sigev_value;	/* signal value */
	void		(*sigev_notify_function)(union sigval);
	pthread_attr_t	*sigev_notify_attributes;
	int		__sigev_pad2;
};
#endif	/* _SIGEVENT */

extern int clock_getres(clockid_t, struct timespec *);
extern int clock_gettime(clockid_t, struct timespec *);
extern int clock_settime(clockid_t, const struct timespec *);
extern int timer_create(clockid_t, struct sigevent *, timer_t *);
extern int timer_delete(timer_t);
extern int timer_getoverrun(timer_t);
extern int timer_gettime(timer_t, struct itimerspec *);
extern int timer_settime(timer_t, int, const struct itimerspec *,
		struct itimerspec *);
extern int nanosleep(const struct timespec *, struct timespec *);
#endif /* defined(__EXTENSIONS__) || ((__STDC__ - 0 == 0 && ... */

#if defined(__EXTENSIONS__) || __STDC__ == 0 || \
		defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE)
extern void tzset(void);

extern char *tzname[2];

#ifndef CLK_TCK
extern long _sysconf(int);	/* System Private interface to sysconf() */
#define	CLK_TCK	((clock_t) _sysconf(3))	/* clock ticks per second */
				/* 3 is _SC_CLK_TCK */
#endif

#if defined(__EXTENSIONS__) || (__STDC__ == 0 && \
		!defined(_POSIX_C_SOURCE)) || defined(_XOPEN_SOURCE)
extern long timezone;
extern int daylight;
#endif

#endif

#if __STDC__ == 0 && !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
extern int cftime(char *, char *, const time_t *);
extern int ascftime(char *, const char *, const struct tm *);
extern long altzone;
#endif

#if (__STDC__ == 0 && !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern struct tm *getdate(const char *);

#ifdef	_REENTRANT
#undef getdate_err
#define	getdate_err *(int *)_getdate_err_addr()
extern int *_getdate_err_addr(void);
#else
extern int getdate_err;
#endif /* _REENTRANT */
#endif /* __STDC__ == 0 && !defined(_POSIX_C_SOURCE) ... */

#else /* __STDC__ */

extern char *strptime();
extern int cftime(), ascftime();
extern void tzset();

#ifdef _STRPTIME_DONTZERO
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname strptime	__strptime_dontzero
#else	/* __PRAGMA_REDEFINE_EXTNAME */
extern char *__strptime_dontzero();
#define	strptime	__strptime_dontzero
#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif	/* _STRPTIME_DONTZERO */


#if	defined(__EXTENSIONS__) || defined(_REENTRANT) || \
	    (_POSIX_C_SOURCE - 0 >= 199506L)
extern struct tm *gmtime_r();
extern struct tm *localtime_r();
#endif	/* defined(__EXTENSIONS__) || defined(_REENTRANT) .. */

extern long timezone, altzone;
extern int daylight;
extern char *tzname[2];

#if (__STDC__ == 0 && !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__)
extern struct tm *getdate();
#ifdef	_REENTRANT
#undef getdate_err
#define	getdate_err *(int *)_getdate_err_addr()
extern int *_getdate_err_addr();
#else
extern int getdate_err;
#endif /* _REENTRANT */
#endif /* __STDC__ == 0 && !defined(_POSIX_C_SOURCE) ... */


#endif	/* __STDC__ */

/*
 * ctime_r() & asctime_r() prototypes are defined here.
 */

/*
 * Previous releases of Solaris, starting at 2.3, provided definitions of
 * various functions as specified in POSIX.1c, Draft 6.  For some of these
 * functions, the final POSIX 1003.1c standard had a different number of
 * arguments and return values.
 *
 * The following segment of this header provides support for the standard
 * interfaces while supporting applications written under earlier
 * releases.  The application defines appropriate values of the feature
 * test macros _POSIX_C_SOURCE and _POSIX_PTHREAD_SEMANTICS to indicate
 * whether it was written to expect the Draft 6 or standard versions of
 * these interfaces, before including this header.  This header then
 * provides a mapping from the source version of the interface to an
 * appropriate binary interface.  Such mappings permit an application
 * to be built from libraries and objects which have mixed expectations
 * of the definitions of these functions.
 *
 * For applications using the Draft 6 definitions, the binary symbol is
 * the same as the source symbol, and no explicit mapping is needed.  For
 * the standard interface, the function func() is mapped to the binary
 * symbol _posix_func().  The preferred mechanism for the remapping is a
 * compiler #pragma.  If the compiler does not provide such a #pragma, the
 * header file defines a static function func() which calls the
 * _posix_func() version; this is required if the application needs to
 * take the address of func().
 *
 * NOTE: Support for the Draft 6 definitions is provided for compatibility
 * only.  New applications/libraries should use the standard definitions.
 */

#if	defined(__EXTENSIONS__) || defined(_REENTRANT) || \
	(_POSIX_C_SOURCE - 0 >= 199506L) || defined(_POSIX_PTHREAD_SEMANTICS)

#if	defined(__STDC__)

#if	(_POSIX_C_SOURCE - 0 >= 199506L) || defined(_POSIX_PTHREAD_SEMANTICS)

#ifdef __PRAGMA_REDEFINE_EXTNAME
extern char *asctime_r(const struct tm *, char *);
extern char *ctime_r(const time_t *, char *);
#pragma redefine_extname ctime_r __posix_ctime_r
#pragma redefine_extname asctime_r __posix_asctime_r
#else  /* __PRAGMA_REDEFINE_EXTNAME */

static char *
asctime_r(const struct tm *__tm, char *__buf)
{
	extern char *__posix_asctime_r(const struct tm *, char *);
	return (__posix_asctime_r(__tm, __buf));
}
static char *
ctime_r(const time_t *__time, char *__buf)
{
	extern char *__posix_ctime_r(const time_t *, char *);
	return (__posix_ctime_r(__time, __buf));
}
#endif /* __PRAGMA_REDEFINE_EXTNAME */

#else  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

extern char *asctime_r(const struct tm *, char *, int);
extern char *ctime_r(const time_t *, char *, int);

#endif  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

#else  /* __STDC__ */

#if	(_POSIX_C_SOURCE - 0 >= 199506L) || defined(_POSIX_PTHREAD_SEMANTICS)

#ifdef __PRAGMA_REDEFINE_EXTNAME
extern char *asctime_r();
extern char *ctime_r();
#pragma redefine_extname asctime_r __posix_asctime_r
#pragma redefine_extname ctime_r __posix_ctime_r
#else  /* __PRAGMA_REDEFINE_EXTNAME */

static char *
asctime_r(__tm, __buf)
	struct tm *__tm;
	char *__buf;
{
	extern char *__posix_asctime_r();
	return (__posix_asctime_r(__tm, __buf));
}
static char *
ctime_r(__time, __buf)
	time_t *__time;
	char *__buf;
{
	extern char *__posix_ctime_r();
	return (__posix_ctime_r(__time, __buf));
}
#endif /* __PRAGMA_REDEFINE_EXTNAME */

#else  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

extern char *asctime_r();
extern char *ctime_r();

#endif /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

#endif /* __STDC__ */

#endif /* defined(__EXTENSIONS__) || (__STDC__ == 0 ... */

#ifdef	__cplusplus
}
#endif

#endif	/* _TIME_H */
