/*
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	This module is created for NLS on Jun.04.86		*/

#ifndef	_WIDEC_H
#define	_WIDEC_H

#pragma ident	"@(#)widec.h	1.22	99/04/14 SMI"

#include <sys/feature_tests.h>

#if defined(__STDC__)
#include <stdio.h>	/* For definition of FILE */
#endif
#include <euc.h>
#include <wchar.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)
/* Character based input and output functions */
extern wchar_t	*getws(wchar_t *);
extern int	putws(const wchar_t *);

#if !defined(__lint)
#define	getwc(p)	fgetwc(p)
#define	putwc(x, p)	fputwc((x), (p))
#define	getwchar()	getwc(stdin)
#define	putwchar(x)	putwc((x), stdout)
#endif

/* wchar_t string operation functions */
extern wchar_t	*strtows(wchar_t *, char *);
extern wchar_t	*wscpy(wchar_t *, const wchar_t *);
extern wchar_t	*wsncpy(wchar_t *, const wchar_t *, size_t);
extern wchar_t	*wscat(wchar_t *, const wchar_t *);
extern wchar_t	*wsncat(wchar_t *, const wchar_t *, size_t);
extern wchar_t	*wschr(const wchar_t *, wchar_t);
extern wchar_t	*wsrchr(const wchar_t *, wchar_t);
extern wchar_t	*wspbrk(const wchar_t *, const wchar_t *);
extern wchar_t	*wstok(wchar_t *, const wchar_t *);
extern char	*wstostr(char *, wchar_t *);

extern int	wscmp(const wchar_t *, const wchar_t *);
extern int	wsncmp(const wchar_t *, const wchar_t *, size_t);
extern size_t	wslen(const wchar_t *);
extern size_t	wsspn(const wchar_t *, const wchar_t *);
extern size_t	wscspn(const wchar_t *, const wchar_t *);
extern int	wscoll(const wchar_t *, const wchar_t *);
extern size_t	wsxfrm(wchar_t *, const wchar_t *, size_t);

#if __STDC__ == 0 && !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)

extern wchar_t	*wsdup(const wchar_t *);
extern int	wscol(const wchar_t *);
extern double	wstod(const wchar_t *, wchar_t **);
extern long	wstol(const wchar_t *, wchar_t **, int);
extern int	wscasecmp(const wchar_t *, const wchar_t *);
extern int	wsncasecmp(const wchar_t *, const wchar_t *, size_t);
extern int	wsprintf(wchar_t *, const char *, ...);
#if !defined(_NO_LONGLONG)
extern long long	wstoll(const wchar_t *, wchar_t **, int);
#endif	/* !defined(_NO_LONGLONG) */

#endif

#else	/* !defined(__STDC__) */
/* Character based input and output functions */
extern wchar_t		*getws();
extern int		putws();

#define	getwc(p)	fgetwc(p)
#define	putwc(x, p)	fputwc((x), (p))
#define	getwchar()	getwc(stdin)
#define	putwchar(x)	putwc((x), stdout)

/* wchar_t string operation functions */
extern wchar_t
	*strtows(),
	*wscpy(),
	*wsncpy(),
	*wscat(),
	*wsncat(),
	*wschr(),
	*wsrchr(),
	*wspbrk(),
	*wstok();

extern int
	wscmp(),
	wsncmp(),
	wslen(),
	wsspn(),
	wscspn(),
	wscoll(),
	wsxfrm();

extern char	*wstostr();
extern wchar_t	*wsdup();
extern int	wscol();
extern double	wstod();
extern long	wstol();
extern int	wscasecmp();
extern int	wsncasecmp();
extern int	wsprintf();
#if !defined(_NO_LONGLONG)
extern long long	wstoll();
#endif	/* !defined(_NO_LONGLONG) */

#endif	/* !defined(__STDC__) */

/* Returns the code set number for the process code c. */
#define	WCHAR_SHIFT	7
#define	WCHAR_S_MASK	0x7f
#define	wcsetno(c) \
	(((c)&0x20000000)?(((c)&0x10000000)?1:3):(((c)&0x10000000)?2:0))


/* Aliases... */
#define	windex		wschr
#define	wrindex		wsrchr

#define	watol(s)	wstol((s), (wchar_t **)0, 10)
#if !defined(_NO_LONGLONG) && !defined(__lint)
#define	watoll(s)	wstoll((s), (wchar_t **)0, 10)
#endif	/* !defined(_NO_LONGLONG) ... */
#define	watoi(s)	((int)wstol((s), (wchar_t **)0, 10))
#define	watof(s)	wstod((s), (wchar_t **)0)

/*
 * other macros.
 */
#define	WCHAR_CSMASK	0x30000000
#define	EUCMASK		0x30000000
#define	WCHAR_CS0	0x00000000
#define	WCHAR_CS1	0x30000000
#define	WCHAR_CS2	0x10000000
#define	WCHAR_CS3	0x20000000
#define	WCHAR_BYTE_OF(wc, i) (((wc&~0x30000000)>>(7*(3-i)))&0x7f)

#ifdef	__cplusplus
}
#endif

#endif	/* _WIDEC_H */
