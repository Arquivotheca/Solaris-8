/*
 * Copyright (c) 1997-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _DIRENT_H
#define	_DIRENT_H

#pragma ident	"@(#)dirent.h	1.29	99/03/11 SMI"	/* SVr4.0 1.6.1.5   */

#include <sys/feature_tests.h>

#include <sys/types.h>
#include <sys/dirent.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))

#define	MAXNAMLEN	512		/* maximum filename length */
#define	DIRBUF		1048		/* buffer size for fs-indep. dirs */

#endif /* defined(__EXTENSIONS__) || (!defined(_POSIX_C_SOURCE) ... */

#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)

typedef struct {
	int	dd_fd;		/* file descriptor */
	int	dd_loc;		/* offset in block */
	int	dd_size;	/* amount of valid data */
	char	*dd_buf;	/* directory block */
} DIR;				/* stream data from opendir() */


#else

typedef struct {
	int	d_fd;		/* file descriptor */
	int	d_loc;		/* offset in block */
	int	d_size;		/* amount of valid data */
	char	*d_buf;		/* directory block */
} DIR;				/* stream data from opendir() */

#endif /* !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) ...  */

#if defined(__STDC__)

/* large file compilation environment setup */
#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	readdir	readdir64
#else
#define	readdir			readdir64
#endif
#endif	/* _FILE_OFFSET_BITS == 64 */

/* In the LP64 compilation environment, all APIs are already large file */
#if defined(_LP64) && defined(_LARGEFILE64_SOURCE)
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	readdir64	readdir
#else
#define	readdir64		readdir
#endif
#endif	/* _LP64 && _LARGEFILE64_SOURCE */

extern DIR		*opendir(const char *);
extern struct dirent	*readdir(DIR *);
#if defined(__EXTENSIONS__) || !defined(_POSIX_C_SOURCE) || \
	defined(_XOPEN_SOURCE)
extern long		telldir(DIR *);
extern void		seekdir(DIR *, long);
#endif /* defined(__EXTENSIONS__) || !defined(_POSIX_C_SOURCE) ... */
extern void		rewinddir(DIR *);
extern int		closedir(DIR *);

/* transitional large file interface */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern struct dirent64	*readdir64(DIR *);
#endif

#else

extern DIR		*opendir();
extern struct dirent	*readdir();
#if defined(__EXTENSIONS__) || !defined(_POSIX_C_SOURCE) || \
	defined(_XOPEN_SOURCE)
extern long		telldir();
extern void		seekdir();
#endif /* defined(__EXTENSIONS__) || !defined(_POSIX_C_SOURCE) ... */
extern void		rewinddir();
extern int		closedir();

/* transitional large file interface */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern struct dirent64	*readdir64();
#endif

#endif

#if defined(__EXTENSIONS__) || !defined(_POSIX_C_SOURCE) || \
	defined(_XOPEN_SOURCE)
#define	rewinddir(dirp)	seekdir(dirp, 0L)
#endif

/*
 * readdir_r() prototype is defined here.
 *
 * There are several variations, depending on whether compatibility with old
 * POSIX draft specifications or the final specification is desired and on
 * whether the large file compilation environment is active.  To combat a
 * combinatorial explosion, enabling large files implies using the final
 * specification (since the definition of the large file environment
 * considerably postdates that of the final readdir_r specification).
 *
 * In the LP64 compilation environment, all APIs are already large file,
 * and since there are no 64-bit applications that can have seen the
 * draft implementation, again, we use the final POSIX specification.
 */

#if	defined(__EXTENSIONS__) || defined(_REENTRANT) || \
	(_POSIX_C_SOURCE - 0 >= 199506L) || defined(_POSIX_PTHREAD_SEMANTICS)

#if	defined(__STDC__)

#if	!defined(_LP64) && _FILE_OFFSET_BITS == 32

#if	(_POSIX_C_SOURCE - 0 >= 199506L) || defined(_POSIX_PTHREAD_SEMANTICS)

#ifdef	__PRAGMA_REDEFINE_EXTNAME
extern int readdir_r(DIR *, struct dirent *, struct dirent **);
#pragma	redefine_extname readdir_r	__posix_readdir_r
#else	/* __PRAGMA_REDEFINE_EXTNAME */

static int
readdir_r(DIR *__dp, struct dirent *__ent, struct dirent **__res)
{
	extern int __posix_readdir_r(DIR *, struct dirent *, struct dirent **);
	return (__posix_readdir_r(__dp, __ent, __res));
}
#endif /* __PRAGMA_REDEFINE_EXTNAME */

#else  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

extern struct dirent *readdir_r(DIR *__dp, struct dirent *__ent);

#endif  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

#else	/* !_LP64 && _FILE_OFFSET_BITS == 32 */

#if defined(_LP64)
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname readdir64_r	readdir_r
#else
#define	readdir64_r		readdir_r
#endif
#else	/* _LP64 */
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname readdir_r	readdir64_r
#else
#define	readdir_r		readdir64_r
#endif
#endif	/* _LP64 */
extern int	readdir_r(DIR *, struct dirent *, struct dirent **);

#endif	/* !_LP64 && _FILE_OFFSET_BITS == 32 */

#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
/* transitional large file interface */
extern int 	readdir64_r(DIR *, struct dirent64 *, struct dirent64 **);
#endif

#else  /* __STDC__ */

#if	!defined(_LP64) && _FILE_OFFSET_BITS == 32

#if	(_POSIX_C_SOURCE - 0 >= 199506L) || defined(_POSIX_PTHREAD_SEMANTICS)

#ifdef __PRAGMA_REDEFINE_EXTNAME
extern int readdir_r();
#pragma redefine_extname readdir_r __posix_readdir_r
#else  /* __PRAGMA_REDEFINE_EXTNAME */

static int
readdir_r(DIR *__dp, struct dirent *__ent, struct dirent **__res)
{
	extern int __posix_readdir_r();
	return (__posix_readdir_r(__dp, __ent, __res));
}
#endif /* __PRAGMA_REDEFINE_EXTNAME */

#else  /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

extern struct dirent *readdir_r();

#endif /* (_POSIX_C_SOURCE - 0 >= 199506L) || ... */

#else	/* !_LP64 && _FILE_OFFSET_BITS == 32 */

#if defined(_LP64)
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname readdir64_r	readdir_r
#else
#define	readdir64_r	readdir
#endif
#else	/* _LP64 */
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname readdir_r readdir64_r
#else
#define	readdir_r readdir64_r
#endif
#endif	/* _LP64 */
extern int	readdir_r();

#endif	/* !_LP64 && _FILE_OFFSET_BITS == 32 */

#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
/* transitional large file interface */
extern int 	readdir64_r();
#endif

#endif /* __STDC__ */

#endif /* defined(__EXTENSIONS__) || (__STDC__ == 0 ... */

#ifdef	__cplusplus
}
#endif

#endif	/* _DIRENT_H */
