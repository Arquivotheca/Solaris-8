/*
 * Copyright (c) 1993, 1996-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright 1989, 1994 by Mortice Kern Systems Inc.
 * All rights reserved.
 */

#ifndef	_REGEX_H
#define	_REGEX_H

#pragma ident	"@(#)regex.h	1.21	99/06/08 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _WCHAR_T
#define	_WCHAR_T
#if defined(_LP64)
typedef int	wchar_t;
#else
typedef long    wchar_t;
#endif
#endif	/* _WCHAR_T */

typedef ssize_t regoff_t;

/* regcomp flags */
#define	REG_EXTENDED	0x01		/* Use Extended Regular Expressions */
#define	REG_NEWLINE	0x08		/* Treat \n as regular character */
#define	REG_ICASE	0x04		/* Ignore case in match */
#define	REG_NOSUB	0x02		/* Don't set subexpression */
#define	REG_EGREP	0x1000		/* running as egrep(1) */

/* non-standard flags */
#define	REG_DELIM	0x10		/* string[0] is delimiter */
#define	REG_DEBUG	0x20		/* Debug recomp and regexec */
#define	REG_ANCHOR	0x40		/* Implicit ^ and $ */
#define	REG_WORDS	0x80		/* \< and \> match word boundries */

/* internal flags */
#define	REG_MUST	0x100		/* check for regmust substring */

/* regexec flags */
#define	REG_NOTBOL	0x200		/* string is not BOL */
#define	REG_NOTEOL	0x400		/* string has no EOL */
#define	REG_NOOPT	0x800		/* don't do regmust optimization */

/* regcomp and regexec return codes */
#define	REG_OK		0		/* success (non-standard) */
#define	REG_NOMATCH	1		/* regexec failed to match */
#define	REG_ECOLLATE	2		/* invalid collation element ref. */
#define	REG_EESCAPE	3		/* trailing \ in pattern */
#define	REG_ENEWLINE	4		/* \n found before end of pattern */
#define	REG_ENSUB	5		/* more than 9 \( \) pairs (OBS) */
#define	REG_ESUBREG	6		/* number in \[0-9] invalid */
#define	REG_EBRACK	7		/* [ ] imbalance */
#define	REG_EPAREN	8		/* ( ) imbalance */
#define	REG_EBRACE	9		/* \{ \} imbalance */
#define	REG_ERANGE	10		/* invalid endpoint in range */
#define	REG_ESPACE	11		/* no memory for compiled pattern */
#define	REG_BADRPT	12		/* invalid repetition */
#define	REG_ECTYPE	13		/* invalid char-class type */
#define	REG_BADPAT	14		/* syntax error */
#define	REG_BADBR	15		/* \{ \} contents bad */
#define	REG_EFATAL	16		/* internal error, not POSIX.2 */
#define	REG_ECHAR	17		/* invalid mulitbyte character */
#define	REG_STACK	18		/* backtrack stack overflow */
#define	REG_ENOSYS	19		/* function not supported (XPG4) */
#define	REG__LAST	20		/* first unused code */
#define	REG_EBOL	21		/* ^ anchor and not BOL */
#define	REG_EEOL	22		/* $ anchor and not EOL */
#define	_REG_BACKREF_MAX 9		/* Max # of subexp. backreference */

typedef struct {		/* regcomp() data saved for regexec() */
	size_t  re_nsub;	/* # of subexpressions in RE pattern */

	/*
	 * Internal use only
	 */
	void	*re_comp;	/* compiled RE; freed by regfree() */
	int	re_cflags;	/* saved cflags for regexec() */
	size_t	re_erroff;	/* RE pattern error offset */
	size_t	re_len;		/* # wchar_t chars in compiled pattern */
	struct _regex_ext_t *re_sc;	/* for binary compatibility */
} regex_t;

/* subexpression positions */
typedef struct {
#ifdef __STDC__
	const char	*rm_sp, *rm_ep;	/* Start pointer, end pointer */
#else
	char		*rm_sp, *rm_ep;	/* Start pointer, end pointer */
#endif
	regoff_t	rm_so, rm_eo;	/* Start offset, end offset */
	int		rm_ss, rm_es;	/* Used internally */
} regmatch_t;


/*
 * Additional API and structs to support regular expression manipulations
 * on wide characters.
 */

#if defined(__STDC__)
extern int regcomp(regex_t *, const char *, int);
extern int regexec(const regex_t *, const char *, size_t, regmatch_t [], int);
extern size_t regerror(int, const regex_t *, char *, size_t);
extern void regfree(regex_t *);

#else  /* defined(__STDC__) */

extern int regcomp();
extern int regexec();
extern size_t regerror();
extern void regfree();

#endif  /* defined(__STDC__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _REGEX_H */
