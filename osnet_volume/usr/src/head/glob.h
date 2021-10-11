/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * Copyright 1985, 1992 by Mortice Kern Systems Inc.  All rights reserved.
 */

#ifndef	_GLOB_H
#define	_GLOB_H

#pragma ident	"@(#)glob.h	1.4	95/03/08 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef	struct	glob_t	{
	size_t	gl_pathc;		/* Count of paths matched by pattern */
	char	**gl_pathv;		/* List of matched pathnames */
	size_t	gl_offs;		/* # of slots reserved in gl_pathv */
	/* following are internal to the implementation */
	char	**gl_pathp;		/* gl_pathv + gl_offs */
	int	gl_pathn;		/* # of elements allocated */
}	glob_t;

/*
 * "flags" argument to glob function.
 */
#define	GLOB_ERR	0x0001		/* Don't continue on directory error */
#define	GLOB_MARK	0x0002		/* Mark directories with trailing / */
#define	GLOB_NOSORT	0x0004		/* Don't sort pathnames */
#define	GLOB_NOCHECK	0x0008		/* Return unquoted arg if no match */
#define	GLOB_DOOFFS	0x0010		/* Ignore gl_offs unless set */
#define	GLOB_APPEND	0x0020		/* Append to previous glob_t */
#define	GLOB_NOESCAPE	0x0040		/* Backslashes do not quote M-chars */

/*
 * Error returns from "glob"
 */
#define	GLOB_NOSYS	(-4)		/* function not supported (XPG4) */
#define	GLOB_NOMATCH	(-3)		/* Pattern does not match */
#define	GLOB_NOSPACE	(-2)		/* Not enough memory */
#define	GLOB_ABORTED	(-1)		/* GLOB_ERR set or errfunc return!=0 */

#if defined(__STDC__)
extern int glob(const char *, int, int(*)(const char *, int), glob_t *);
extern void globfree(glob_t *);
#else
extern int glob();
extern void globfree();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _GLOB_H */
