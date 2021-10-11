/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * Copyright 1985, 1992 by Mortice Kern Systems Inc.  All rights reserved.
 */

#ifndef	_WORDEXP_H
#define	_WORDEXP_H

#pragma ident	"@(#)wordexp.h	1.4	95/03/08 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef	struct	wordexp_t {
	size_t	we_wordc;		/* Count of paths matched by pattern */
	char	**we_wordv;		/* List of matched pathnames */
	size_t	we_offs;		/* # of slots reserved in we_pathv */
	/* following are internal to the implementation */
	char	**we_wordp;		/* we_pathv + we_offs */
	int	we_wordn;		/* # of elements allocated */
} wordexp_t;

/*
 * wordexp flags.
 */
#define	WRDE_APPEND	0x0001		/* append to existing wordexp_t */
#define	WRDE_DOOFFS	0x0002		/* use we_offs */
#define	WRDE_NOCMD	0x0004		/* don't allow $() */
#define	WRDE_REUSE	0x0008
#define	WRDE_SHOWERR	0x0010		/* don't 2>/dev/null */
#define	WRDE_UNDEF	0x0020		/* set -u */

/*
 * wordexp errors.
 */
#define	WRDE_ERRNO	(2)		/* error in "errno" */
#define	WRDE_BADCHAR	(3)		/* shell syntax character */
#define	WRDE_BADVAL	(4)		/* undefined variable expanded */
#define	WRDE_CMDSUB	(5)		/* prohibited $() */
#define	WRDE_NOSPACE	(6)		/* no memory */
#define	WRDE_SYNTAX	(7)		/* bad syntax */
#define	WRDE_NOSYS	(8)		/* function not supported (XPG4) */

#ifdef __STDC__
extern int	wordexp(const char *, wordexp_t *, int);
extern void	wordfree(wordexp_t *);
#else
extern int	wordexp();
extern void	wordfree();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _WORDEXP_H */
