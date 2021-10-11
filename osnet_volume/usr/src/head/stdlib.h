/*
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _STDLIB_H
#define	_STDLIB_H

#pragma ident	"@(#)stdlib.h	1.47	99/11/03 SMI"	/* SVr4.0 1.22	*/

#include <iso/stdlib_iso.h>

#if defined(__EXTENSIONS__) || \
	(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4))
#include <sys/wait.h>
#endif

/*
 * Allow global visibility for symbols defined in
 * C++ "std" namespace in <iso/stdlib_iso.h>.
 */
#if __cplusplus >= 199711L
using std::div_t;
using std::ldiv_t;
using std::size_t;
using std::abort;
using std::abs;
using std::atexit;
using std::atof;
using std::atoi;
using std::atol;
using std::bsearch;
using std::calloc;
using std::div;
using std::exit;
using std::free;
using std::getenv;
using std::labs;
using std::ldiv;
using std::malloc;
using std::mblen;
using std::mbstowcs;
using std::mbtowc;
using std::qsort;
using std::rand;
using std::realloc;
using std::srand;
using std::strtod;
using std::strtol;
using std::strtoul;
using std::system;
using std::wcstombs;
using std::wctomb;
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
typedef struct {
	long long	quot;
	long long	rem;
} lldiv_t;
#endif	/* __STDC__ - 0 == 0 && !defined(_NO_LONGLONG) */

#ifndef _UID_T
#define	_UID_T
#if defined(_LP64) || defined(_I32LPx)
typedef	int	uid_t;			/* UID type		*/
#else
typedef long	uid_t;			/* (historical version) */
#endif
#endif	/* !_UID_T */

#if defined(__STDC__)

/* large file compilation environment setup */
#if !defined(_LP64) && _FILE_OFFSET_BITS == 64

#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	mkstemp		mkstemp64
#else	/* __PRAGMA_REDEFINE_EXTNAME */
#define	mkstemp			mkstemp64
#endif	/* __PRAGMA_REDEFINE_EXTNAME */

#endif	/* _FILE_OFFSET_BITS == 64 */

/* In the LP64 compilation environment, all APIs are already large file */
#if defined(_LP64) && defined(_LARGEFILE64_SOURCE)

#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	mkstemp64	mkstemp
#else	/* __PRAGMA_REDEFINE_EXTNAME */
#define	mkstemp64		mkstemp
#endif	/* __PRAGMA_REDEFINE_EXTNAME */

#endif	/* _LP64 && _LARGEFILE64_SOURCE */

#if defined(__EXTENSIONS__) || defined(_REENTRANT) || \
	(_POSIX_C_SOURCE - 0 >= 199506L)
extern int rand_r(unsigned int *);
#endif	/* defined(__EXTENSIONS__) || defined(_REENTRANT).. */

extern void _exithandle(void);

#if defined(__EXTENSIONS__) || \
	(__STDC__ == 0 && !defined(_POSIX_C_SOURCE)) || \
	(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4))
extern double drand48(void);
extern double erand48(unsigned short *);
extern long jrand48(unsigned short *);
extern void lcong48(unsigned short *);
extern long lrand48(void);
extern long mrand48(void);
extern long nrand48(unsigned short *);
extern unsigned short *seed48(unsigned short *);
extern void srand48(long);
extern int putenv(char *);
extern void setkey(const char *);
#endif /* defined(__EXTENSIONS__) || (__STDC__ == 0 && ... */


#if (defined(__EXTENSIONS__) || \
	(__STDC__ == 0 && !defined(_POSIX_C_SOURCE))) && \
	((_XOPEN_VERSION - 0 < 4) && (_XOPEN_SOURCE_EXTENDED - 0 < 1))

#ifndef	_SSIZE_T
#define	_SSIZE_T
#if defined(_LP64) || defined(_I32LPx)
typedef long	ssize_t;	/* size of something in bytes or -1 */
#else
typedef int	ssize_t;	/* (historical version) */
#endif
#endif	/* !_SSIZE_T */

extern void swab(const char *, char *, ssize_t);
#endif /* defined(__EXTENSIONS__) ||  (__STDC__ == 0 && ... */

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64)
extern int	mkstemp(char *);
#endif /* _POSIX_C_SOURCE */

#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern int	mkstemp64(char *);
#endif	/* _LARGEFILE64_SOURCE... */

#if defined(__EXTENSIONS__) || \
	(__STDC__ == 0 && !defined(_POSIX_C_SOURCE) && \
	!defined(_XOPEN_SOURCE)) || defined(_XPG4_2)
extern long a64l(const char *);
extern char *ecvt(double, int, int *, int *);
extern char *fcvt(double, int, int *, int *);
extern char *gcvt(double, int, char *);
extern int getsubopt(char **, char *const *, char **);
extern int  grantpt(int);
extern char *initstate(unsigned, char *, size_t);
extern char *l64a(long);
extern char *mktemp(char *);
extern char *ptsname(int);
extern long random(void);
extern char *realpath(const char *, char *);
extern char *setstate(const char *);
extern void srandom(unsigned);
extern int ttyslot(void);
extern int  unlockpt(int);
extern void *valloc(size_t);
#endif /* defined(__EXTENSIONS__) || ... || defined(_XPG4_2) */

#if defined(__EXTENSIONS__) || \
	(__STDC__ == 0 && !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))
extern int dup2(int, int);
extern char *qecvt(long double, int, int *, int *);
extern char *qfcvt(long double, int, int *, int *);
extern char *qgcvt(long double, int, char *);
extern char *getcwd(char *, size_t);
extern const char *getexecname(void);
extern char *getlogin(void);
extern int getopt(int, char *const *, const char *);
extern char *optarg;
extern int optind, opterr, optopt;
extern char *getpass(const char *);
extern char *getpassphrase(const char *);
extern int getpw(uid_t, char *);
extern int isatty(int);
extern void *memalign(size_t, size_t);
extern char *ttyname(int);

#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
extern long long atoll(const char *);
extern long long llabs(long long);
extern lldiv_t lldiv(long long, long long);
extern char *lltostr(long long, char *);
extern long long strtoll(const char *, char **, int);
extern unsigned long long strtoull(const char *, char **, int);
extern char *ulltostr(unsigned long long, char *);
#endif	/* __STDC__ - 0 == 0 && !defined(_NO_LONGLONG) */

#endif /* defined(__EXTENSIONS__) || (__STDC__ == 0 && ... */

#else /* not __STDC__ */

#if defined(__EXTENSIONS__) || defined(_REENTRANT) || \
	(_POSIX_C_SOURCE - 0 >= 199506L)
extern int rand_r();
#endif	/* defined(__EXTENSIONS__) || defined(_REENTRANT).. */

extern void _exithandle();

#if defined(__EXTENSIONS__) || \
	(__STDC__ == 0 && !defined(_POSIX_C_SOURCE)) || \
	(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4))
extern double drand48();
extern double erand48();
extern long jrand48();
extern void lcong48();
extern long lrand48();
extern long mrand48();
extern long nrand48();
extern unsigned short *seed48();
extern void srand48();
extern int putenv();
extern void setkey();
#endif /* defined(__EXTENSIONS__) || (__STDC__ == 0 && ... */

#if (defined(__EXTENSIONS__) || \
	(__STDC__ == 0 && !defined(_POSIX_C_SOURCE))) && \
	((_XOPEN_VERSION - 0 < 4) && (_XOPEN_SOURCE_EXTENDED - 0 < 1))
extern void swab();
#endif

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(_XPG4_2) || defined(__EXTENSIONS__) || \
	(defined(_LARGEFILE_SOURCE) && _FILE_OFFSET_BITS == 64)
extern int	mkstemp();
#endif	/* _POSIX_C_SOURCE... */

#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern int	mkstemp64();
#endif	/* _LARGEFILE64_SOURCE... */

#if defined(__EXTENSIONS__) || \
	(__STDC__ == 0 && !defined(_POSIX_C_SOURCE) && \
	!defined(_XOPEN_SOURCE)) || defined(_XPG4_2)
extern long a64l();
extern char *ecvt();
extern char *fcvt();
extern char *gcvt();
extern int getsubopt();
extern int grantpt();
extern char *initstate();
extern char *l64a();
extern char *mktemp();
extern char *ptsname();
extern long random();
extern char *realpath();
extern char *setstate();
extern void srandom();
extern int ttyslot();
extern void *valloc();
extern int  unlockpt();
#endif /* defined(__EXTENSIONS__) || ... || defined(_XPG4_2) */

#if defined(__EXTENSIONS__) || \
	(__STDC__ == 0 && !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))
extern int dup2();
extern char *qecvt();
extern char *qfcvt();
extern char *qgcvt();
extern char *getcwd();
extern char *getexecname();
extern char *getlogin();
extern int getopt();
extern char *optarg;
extern int optind, opterr, optopt;
extern char *getpass();
extern char *getpassphrase();
extern int getpw();
extern int isatty();
extern void *memalign();
extern char *ttyname();

#if __STDC__ - 0 == 0 && !defined(_NO_LONGLONG)
extern long long atoll();
extern long long llabs();
extern lldiv_t lldiv();
extern char *lltostr();
extern long long strtoll();
extern unsigned long long strtoull();
extern char *ulltostr();
#endif  /* __STDC__ - 0 == 0 && !defined(_NO_LONGLONG) */
#endif	/* defined(__EXTENSIONS__) || (__STDC__ == 0 && ... */

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _STDLIB_H */
