/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * declarations of functions found in libgen
 */

#ifndef _LIBGEN_H
#define	_LIBGEN_H

#pragma ident	"@(#)libgen.h	1.15	97/02/12 SMI"	/* SVr4.0 2.4.2.8 */

#include <sys/feature_tests.h>

#include <sys/types.h>
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#include <time.h>
#include <stdio.h>
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef __STDC__
extern char * basename(char *);
#else
extern char * basename();
#endif

#ifdef __STDC__
extern char * regcmp(const char *, ...);
#else
extern char * regcmp();
#endif

#ifdef __STDC__
extern char * dirname(char *);
#else
extern char * dirname();
#endif

#ifdef __STDC__
extern char * regex(const char *, const char *, ...);
#else
extern char * regex();
#endif

#ifdef _REENTRANT
#ifdef __STDC__
extern char **____loc1(void);
#else
extern char **____loc1();
#endif
#define	__loc1 (*(____loc1()))
#else
extern char *__loc1;	/* TO BE WITHDRAWN */
#endif

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

#ifdef __STDC__
extern char * bgets(char *, size_t, FILE *, char *);
#else
extern char * bgets();
#endif

#ifdef __STDC__
extern size_t bufsplit(char *, size_t, char **);
#else
extern size_t bufsplit();
#endif

#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	copylist	copylist64
#else
#define	copylist		copylist64
#endif
#endif	/* !_LP64 && _FILE_OFFSET_BITS == 64 */

#if defined(_LP64) && defined(_LARGEFILE64_SOURCE)
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname	copylist64	copylist
#else
#define	copylist64		copylist
#endif
#endif	/* _LP64 && _LARGEFILE64_SOURCE */

#ifdef __STDC__
extern char * copylist(const char *, off_t *);
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern char * copylist64(const char *, off64_t *);
#endif	/* _LARGEFILE64_SOURCE... */
#else
extern char * copylist();
#ifdef _LARGEFILE64_SOURCE
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern char * copylist64();
#endif	/* _LARGEFILE64_SOURCE... */
#endif
#endif

#ifdef __STDC__
extern int eaccess(const char *, int);
#else
extern int eaccess();
#endif

#ifdef __STDC__
extern int gmatch(const char *, const char *);
#else
extern int gmatch();
#endif

#ifdef __STDC__
extern int isencrypt(const char *, size_t);
#else
extern int isencrypt();
#endif

#ifdef __STDC__
extern int mkdirp(const char *, mode_t);
#else
extern int mkdirp();
#endif

#ifdef __STDC__
extern int p2open(const char *, FILE *[2]);
#else
extern int p2open();
#endif

#ifdef __STDC__
extern int p2close(FILE *[2]);
#else
extern int p2close();
#endif

#ifdef __STDC__
extern char * pathfind(const char *, const char *, const char *);
#else
extern char * pathfind();
#endif

#ifdef _REENTRANT
#define	__i_size (*(___i_size()))
#else
extern int __i_size;
#endif

#ifdef __STDC__
extern int rmdirp(char *, char *);
#else
extern int rmdirp();
#endif

#ifdef __STDC__
extern char * strcadd(char *, const char *);
#else
extern char * strcadd();
#endif

#ifdef __STDC__
extern char * strccpy(char *, const char *);
#else
extern char * strccpy();
#endif

#ifdef __STDC__
extern char * streadd(char *, const char *, const char *);
#else
extern char * streadd();
#endif

#ifdef __STDC__
extern char * strecpy(char *, const char *, const char *);
#else
extern char * strecpy();
#endif

#ifdef __STDC__
extern int strfind(const char *, const char *);
#else
extern int strfind();
#endif

#ifdef __STDC__
extern char * strrspn(const char *, const char *);
#else
extern char * strrspn();
#endif

#ifdef __STDC__
extern char * strtrns(const char *, const char *, const char *, char *);
#else
extern char * strtrns();
#endif

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBGEN_H */
