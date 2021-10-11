/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All Rights reserved.
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_FTW_H
#define	_FTW_H

#pragma ident	"@(#)ftw.h	1.20	98/03/31 SMI"	/* SVr4.0 1.3.1.9 */

#include <sys/feature_tests.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Codes for the third argument to the user-supplied function
 *	which is passed as the second argument to ftwalk
 */

#define	FTW_F	0	/* file */
#define	FTW_D	1	/* directory */
#define	FTW_DNR	2	/* directory without read permission */
#define	FTW_NS	3	/* unknown type, stat failed */
#define	FTW_SL	4	/* symbolic link */
#define	FTW_DP	6	/* directory */
#define	FTW_SLN	7	/* symbolic link that points to nonexistent file */

/*
 *	Codes for the fourth argument to ftwalk.  You can specify the
 *	union of these flags.
 */

#define	FTW_PHYS	01  /* use lstat instead of stat */
#define	FTW_MOUNT	02  /* do not cross a mount point */
#define	FTW_CHDIR	04  /* chdir to each directory before reading */
#define	FTW_DEPTH	010 /* call descendents before calling the parent */

#if defined(__EXTENSIONS__) || !defined(_XOPEN_SOURCE) || defined(_XPG4_2)
struct FTW
{
#if defined(_XPG4_2)
	int	__quit;
#else
	int	quit;
#endif
	int	base;
	int	level;
};
#endif /* defined(__EXTENSIONS__) || !defined(_XOPEN_SOURCE) ... */

/*
 * legal values for quit
 */

#define	FTW_SKD		1
#define	FTW_FOLLOW	2
#define	FTW_PRUNE	4

/* large file compilation environment setup */
#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	_xftw	_xftw64
#pragma redefine_extname	_ftw	_ftw64
#if !defined(_XOPEN_SOURCE) || defined(_XPG5)
#pragma redefine_extname	nftw	nftw64
#endif
#else	/* __PRAGMA_REDEFINE_EXTNAME */
#define	_xftw			_xftw64
#define	_ftw			_ftw64
#if !defined(_XOPEN_SOURCE) || defined(_XPG5)
#define	nftw			nftw64
#endif
#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif /* !_LP64 && _FILE_OFFSET_BITS == 64 */

/* In the LP64 compilation environment, all APIs are already large file */
#if defined(_LP64) && defined(_LARGEFILE64_SOURCE)
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname	_xftw64		_xftw
#pragma	redefine_extname	_ftw64		_ftw
#if !defined(_XOPEN_SOURCE) || defined(_XPG5)
#pragma	redefine_extname	nftw64		nftw
#endif
#else	/* __PRAGMA_REDEFINE_EXTNAME */
#define	_xftw64		_xftw
#define	_ftw64		_ftw
#if !defined(_XOPEN_SOURCE) || defined(_XPG5)
#define	nftw64		nftw
#endif
#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif	/* _LP64 && _LARGEFILE64_SOURCE */

#if defined(__STDC__)

extern int ftw(const char *, int (*)(const char *, const struct stat *, int),
	int);
extern int _xftw(const int, const char *, int (*)(const char *,
	const struct stat *, int), int);
#if defined(__EXTENSIONS__) || !defined(_XOPEN_SOURCE) || defined(_XPG4_2)
extern int nftw(const char *, int (*)(const char *, const struct stat *,
	int, struct FTW *), int, int);
#endif /* defined(__EXTENSIONS__) || !defined(_XOPEN_SOURCE) ... */

/*
 * transitional large file interface versions
 */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern int ftw64(const char *,
	int (*)(const char *, const struct stat64 *, int), int);
extern int _xftw64(const int, const char *, int (*)(const char *,
	const struct stat64 *, int), int);
#if !defined(_XOPEN_SOURCE)
extern int nftw64(const char *, int (*)(const char *, const struct stat64 *,
	int, struct FTW *), int, int);
#endif /* !defined(_XOPEN_SOURCE) */
#endif /* _LARGEFILE64_SOURCE .. */

#else /* __STDC__ */

extern int ftw(), _xftw();

#if defined(__EXTENSIONS__) || !defined(_XOPEN_SOURCE) || defined(_XPG4_2)
extern int nftw();
#endif /* defined(__EXTENSIONS__) || !defined(_XOPEN_SOURCE) ... */

/* transitional large file interface versions */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern int ftw64();
extern int _xftw64();
#if !defined(_XOPEN_SOURCE)
extern int nftw64();
#endif /* !defined(_XOPEN_SOURCE) */
#endif /* _LARGEFILE64_SOURCE .. */

#endif /* __STDC__ */

#define	_XFTWVER	2	/* version of file tree walk */

#define	ftw(path, fn, depth)	(_xftw(_XFTWVER, path, fn, depth))

#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
#define	ftw64(path, fn, depth)	(_xftw64(_XFTWVER, path, fn, depth))
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _FTW_H */
