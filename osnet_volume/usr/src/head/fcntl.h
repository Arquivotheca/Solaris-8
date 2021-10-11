/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_FCNTL_H
#define	_FCNTL_H

#pragma ident	"@(#)fcntl.h	1.14	97/12/05 SMI"	/* SVr4.0 1.6.1.7 */

#include <sys/feature_tests.h>
#if defined(__EXTENSIONS__) || \
	(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4))
#include <sys/stat.h>
#endif
#include <sys/types.h>
#include <sys/fcntl.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__EXTENSIONS__) || \
	(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4))

/* Symbolic constants for the "lseek" routine. */

#ifndef	SEEK_SET
#define	SEEK_SET	0	/* Set file pointer to "offset" */
#endif

#ifndef	SEEK_CUR
#define	SEEK_CUR	1	/* Set file pointer to current plus "offset" */
#endif

#ifndef	SEEK_END
#define	SEEK_END	2	/* Set file pointer to EOF plus "offset" */
#endif

#endif /* defined(__EXTENSIONS__) || (defined(_XOPEN_SOURCE) ... */

/* large file compilation environment setup */
#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	open	open64
#pragma redefine_extname	creat	creat64
#else
#define	open			open64
#define	creat			creat64
#endif
#endif	/* !_LP64 && _FILE_OFFSET_BITS == 64 */

#if defined(_LP64) && defined(_LARGEFILE64_SOURCE)
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname	open64	open
#pragma	redefine_extname	creat64	creat
#else
#define	open64				open
#define	creat64				creat
#endif
#endif	/* _LP64 && _LARGEFILE64_SOURCE */

#if defined(__STDC__)

extern int fcntl(int, int, ...);
extern int open(const char *, int, ...);
extern int creat(const char *, mode_t);
#if defined(__EXTENSIONS__) || \
	(!defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE))
extern int directio(int, int);
#endif

/* transitional large file interface versions */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern int open64(const char *, int, ...);
extern int creat64(const char *, mode_t);
#endif

#else	/* defined(__STDC__) */

extern int fcntl();
extern int open();
extern int creat();
#if defined(__EXTENSIONS__) || \
	(!defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE))
extern int directio();
#endif

/* transitional large file interface versions */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern int open64();
extern int creat64();
#endif

#endif	/* defined(__STDC__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _FCNTL_H */
