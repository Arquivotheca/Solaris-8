/*
 * Copyright (c) 1993, 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * An application should not include this header directly.  Instead it
 * should be included only through the inclusion of other Sun headers.
 *
 * The contents of this header is limited to identifiers specified in the
 * C Standard.  Any new identifiers specified in future amendments to the
 * C Standard must be placed in this header.  If these new identifiers
 * are required to also be in the C++ Standard "std" namespace, then for
 * anything other than macro definitions, corresponding "using" directives
 * must also be added to <wchar.h>.
 */

#ifndef	_ISO_WCHAR_ISO_H
#define	_ISO_WCHAR_ISO_H

#pragma ident	"@(#)wchar_iso.h	1.2	99/11/10 SMI"

#include <sys/feature_tests.h>
#include <stdio_tag.h>
#include <wchar_impl.h>
#include <iso/time_iso.h>

#if (defined(__cplusplus) && (__cplusplus - 0 < 54321L)) || \
	(!defined(__cplusplus) && (__STDC__ - 0 == 0)) || \
	defined(__EXTENSIONS__)
#include <stdio.h>
#endif	/*  (defined(__cplusplus) && (__cplusplus - 0 < 54321L)) ... */

#if (__STDC__ - 0 == 0) || defined(__EXTENSIONS__)
#include <ctype.h>
#include <stddef.h>
#endif /* (__STDC__ - 0 == 0) || defined(__EXTENSIONS__) */

#include <sys/va_list.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if __cplusplus >= 199711L
namespace std {
#endif

#ifndef _WCHAR_T
#define	_WCHAR_T
#if defined(_LP64)
typedef	int	wchar_t;
#else
typedef	long	wchar_t;
#endif
#endif	/* !_WCHAR_T */

#if !defined(_WINT_T) || __cplusplus >= 199711L
#define	_WINT_T
#if defined(_LP64)
typedef	int	wint_t;
#else
typedef	long	wint_t;
#endif
#endif	/* !defined(_WINT_T) || __cplusplus >= 199711L */

#if !defined(_SIZE_T) || __cplusplus >= 199711L
#define	_SIZE_T
#if defined(_LP64) || defined(_I32LPx)
typedef	unsigned long	size_t;		/* size of something in bytes */
#else
typedef unsigned int	size_t;		/* (historical version) */
#endif
#endif  /* !defined(_SIZE_T) || __cplusplus >= 199711L */

#ifndef NULL
#if defined(_LP64) && !defined(__cplusplus)
#define	NULL	0L
#else
#define	NULL	0
#endif
#endif /* !NULL */

#ifndef WEOF
#define	WEOF	((wint_t) (-1))
#endif

/* not XPG4 and not XPG4v2 */
#if (!(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 == 4)))
#ifndef	WCHAR_MAX
#define	WCHAR_MAX	2147483647
#endif
#ifndef	WCHAR_MIN
#define	WCHAR_MIN	(-2147483647-1)
#endif
#endif /* not XPG4 and not XPG4v2 */

#if !defined(_MBSTATE_T) || __cplusplus >= 199711L
#define	_MBSTATE_T
typedef __mbstate_t	mbstate_t;
#endif	/* _MBSTATE_T */

#if defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4) && \
	!defined(_FILEDEFED) || __cplusplus >= 199711L
#define	_FILEDEFED
typedef __FILE FILE;
#endif

#ifdef __STDC__

extern wint_t fgetwc(__FILE *);
extern wchar_t *fgetws(wchar_t *, int, __FILE *);
extern wint_t fputwc(wint_t, __FILE *);
extern int fputws(const wchar_t *, __FILE *);
extern wint_t ungetwc(wint_t, __FILE *);
extern wint_t getwc(__FILE *);
extern wint_t getwchar(void);
extern wint_t putwc(wint_t, __FILE *);
extern wint_t putwchar(wint_t);
extern double wcstod(const wchar_t *, wchar_t **);
extern long wcstol(const wchar_t *, wchar_t **, int);
extern unsigned long wcstoul(const wchar_t *, wchar_t **, int);
extern wchar_t *wcscat(wchar_t *, const wchar_t *);
extern int wcscmp(const wchar_t *, const wchar_t *);
extern int wcscoll(const wchar_t *, const wchar_t *);
extern wchar_t *wcscpy(wchar_t *, const wchar_t *);
extern size_t wcscspn(const wchar_t *, const wchar_t *);
extern size_t wcslen(const wchar_t *);
extern wchar_t *wcsncat(wchar_t *, const wchar_t *, size_t);
extern int wcsncmp(const wchar_t *, const wchar_t *, size_t);
extern wchar_t *wcsncpy(wchar_t *, const wchar_t *, size_t);
extern size_t wcsspn(const wchar_t *, const wchar_t *);
extern size_t wcsxfrm(wchar_t *, const wchar_t *, size_t);
#if __cplusplus >= 199711L
extern const wchar_t *wcschr(const wchar_t *, wchar_t);
extern "C++" {
	inline wchar_t *wcschr(wchar_t *__ws, wchar_t __wc) {
		return (wchar_t *)wcschr((const wchar_t *)__ws, __wc);
	}
}
extern const wchar_t *wcspbrk(const wchar_t *, const wchar_t *);
extern "C++" {
	inline wchar_t *wcspbrk(wchar_t *__ws1, const wchar_t *__ws2) {
		return (wchar_t *)wcspbrk((const wchar_t *)__ws1, __ws2);
	}
}
extern const wchar_t *wcsrchr(const wchar_t *, wchar_t);
extern "C++" {
	inline wchar_t *wcsrchr(wchar_t *__ws, wchar_t __wc) {
		return (wchar_t *)wcsrchr((const wchar_t *)__ws, __wc);
	}
}
#else /* __cplusplus >= 199711L */
extern wchar_t *wcschr(const wchar_t *, wchar_t);
extern wchar_t *wcspbrk(const wchar_t *, const wchar_t *);
extern wchar_t *wcsrchr(const wchar_t *, wchar_t);
#endif /* __cplusplus >= 199711L */

#if (!defined(_MSE_INT_H))
#if (defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 == 4)) /* XPG4 or XPG4v2 */
extern wchar_t *wcstok(wchar_t *, const wchar_t *);
extern size_t wcsftime(wchar_t *, size_t, const char *, const struct tm *);
#else	/* XPG4 or XPG4v2 */
#ifdef __PRAGMA_REDEFINE_EXTNAME
extern wchar_t *wcstok(wchar_t *, const wchar_t *, wchar_t **);
extern size_t wcsftime(wchar_t *, size_t, const wchar_t *, const struct tm *);
#pragma redefine_extname wcstok	__wcstok_xpg5
#pragma redefine_extname wcsftime	__wcsftime_xpg5
#else	/* __PRAGMA_REDEFINE_EXTNAME */
extern wchar_t *__wcstok_xpg5(wchar_t *, const wchar_t *, wchar_t **);
extern size_t __wcsftime_xpg5(wchar_t *, size_t, const wchar_t *,
	const struct tm *);
#define	wcstok	__wcstok_xpg5
#define	wcsftime	__wcsftime_xpg5
#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif	/* XPG4 or XPG4v2 */
#endif	/* !defined(_MSE_INT_H) */

/* not XPG4 and not XPG4v2 */
#if (!(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 == 4)))
extern wint_t	btowc(int);
extern int	fwprintf(__FILE *, const wchar_t *, ...);
extern int	fwscanf(__FILE *, const wchar_t *, ...);
extern int	fwide(__FILE *, int);
extern int	mbsinit(const mbstate_t *);
extern size_t	mbrlen(const char *, size_t, mbstate_t *);
extern size_t	mbrtowc(wchar_t *, const char *, size_t, mbstate_t *);
extern size_t	mbsrtowcs(wchar_t *, const char **, size_t, mbstate_t *);
extern int	swprintf(wchar_t *, size_t, const wchar_t *, ...);
extern int	swscanf(const wchar_t *, const wchar_t *, ...);
extern int	vfwprintf(__FILE *, const wchar_t *, __va_list);
extern int	vwprintf(const wchar_t *, __va_list);
extern int	vswprintf(wchar_t *, size_t, const wchar_t *, __va_list);
extern size_t	wcrtomb(char *, wchar_t, mbstate_t *);
extern size_t	wcsrtombs(char *, const wchar_t **, size_t, mbstate_t *);
extern int	wctob(wint_t);
extern int	wmemcmp(const wchar_t *, const wchar_t *, size_t);
extern wchar_t	*wmemcpy(wchar_t *, const wchar_t *, size_t);
extern wchar_t	*wmemmove(wchar_t *, const wchar_t *, size_t);
extern wchar_t	*wmemset(wchar_t *, wchar_t, size_t);
extern int	wprintf(const wchar_t *, ...);
extern int	wscanf(const wchar_t *, ...);
#if __cplusplus >= 199711L
extern const wchar_t *wcsstr(const wchar_t *, const wchar_t *);
extern "C++" {
	inline wchar_t *wcsstr(wchar_t *__ws1, const wchar_t *__ws2) {
		return (wchar_t *)wcsstr((const wchar_t *)__ws1, __ws2);
	}
}
extern const wchar_t *wmemchr(const wchar_t *, wchar_t, size_t);
extern "C++" {
	inline wchar_t *wmemchr(wchar_t *__ws, wchar_t __wc, size_t __n) {
		return (wchar_t *)wmemchr((const wchar_t *)__ws, __wc, __n);
	}
}
#else /* __cplusplus >= 199711L */
extern wchar_t	*wcsstr(const wchar_t *, const wchar_t *);
extern wchar_t	*wmemchr(const wchar_t *, wchar_t, size_t);
#endif /* __cplusplus >= 199711L */
#endif /* not XPG4 and not XPG4v2 */

#else /* __STDC__ */

extern  wint_t fgetwc();
extern  wchar_t *fgetws();
extern  wint_t fputwc();
extern  int fputws();
extern  wint_t  ungetwc();
extern wint_t getwc();
extern wint_t getwchar();
extern wint_t putwc();
extern wint_t putwchar();
extern wint_t ungetwc();
extern double wcstod();
extern long wcstol();
extern unsigned long wcstoul();
extern wchar_t *wcscat();
extern wchar_t *wcschr();
extern int wcscmp();
extern int wcscoll();
extern wchar_t *wcscpy();
extern size_t wcscspn();
extern size_t wcslen();
extern wchar_t *wcsncat();
extern int wcsncmp();
extern wchar_t *wcsncpy();
extern wchar_t *wcspbrk();
extern wchar_t *wcsrchr();
extern size_t wcsspn();
extern size_t wcsxfrm();

#if (!defined(_MSE_INT_H))
#if (defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 == 4)) /* XPG4 or XPG4v2 */
extern wchar_t *wcstok();
extern size_t wcsftime();
#else	/* XPG4 or XPG4v2 */
#ifdef __PRAGMA_REDEFINE_EXTNAME
extern wchar_t *wcstok();
extern size_t wcsftime();
#pragma redefine_extname wcstok	__wcstok_xpg5
#pragma redefine_extname wcsftime	__wcsftime_xpg5
#else	/* __PRAGMA_REDEFINE_EXTNAME */
extern wchar_t *__wcstok_xpg5();
extern size_t __wcsftime_xpg5();
#define	wcstok	__wcstok_xpg5
#define	wcsftime	__wcsftime_xpg5
#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif	/* XPG4 or XPG4v2 */
#endif	/* defined(_MSE_INT_H) */

/* not XPG4 and not XPG4v2 */
#if (!(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 == 4)))
extern wint_t	btowc();
extern int	fwprintf();
extern int	fwscanf();
extern int	fwide();
extern int	mbsinit();
extern size_t	mbrlen();
extern size_t	mbrtowc();
extern size_t	mbsrtowcs();
extern int	swprintf();
extern int	swscanf();
extern int	vfwprintf();
extern int	vwprintf();
extern int	vswprintf();
extern size_t	wcrtomb();
extern size_t	wcsrtombs();
extern wchar_t	*wcsstr();
extern int	wctob();
extern wchar_t	*wmemchr();
extern int	wmemcmp();
extern wchar_t	*wmemcpy();
extern wchar_t	*wmemmove();
extern wchar_t	*wmemset();
extern int	wprintf();
extern int	wscanf();
#endif /* not XPG4 and not XPG4v2 */

#endif /* __STDC__ */

#if (!defined(_MSE_INT_H))
/* not XPG4 and not XPG4v2 */
#if (!(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 == 4)))
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname fgetwc	__fgetwc_xpg5
#pragma redefine_extname getwc	__getwc_xpg5
#pragma redefine_extname getwchar	__getwchar_xpg5
#pragma redefine_extname fputwc	__fputwc_xpg5
#pragma redefine_extname putwc	__putwc_xpg5
#pragma redefine_extname putwchar	__putwchar_xpg5
#pragma redefine_extname fgetws	__fgetws_xpg5
#pragma redefine_extname fputws	__fputws_xpg5
#pragma redefine_extname ungetwc	__ungetwc_xpg5
#else	/* __PRAGMA_REDEFINE_EXTNAME */
#ifdef __STDC__
extern wint_t __fgetwc_xpg5(__FILE *);
extern wint_t __getwc_xpg5(__FILE *);
extern wint_t __getwchar_xpg5(void);
extern wint_t __fputwc_xpg5(wint_t, __FILE *);
extern wint_t __putwc_xpg5(wint_t, __FILE *);
extern wint_t __putwchar_xpg5(wint_t);
extern wchar_t *__fgetws_xpg5(wchar_t *, int, __FILE *);
extern int __fputws_xpg5(const wchar_t *, __FILE *);
extern wint_t __ungetwc_xpg5(wint_t, __FILE *);
#else
extern wint_t __fgetwc_xpg5();
extern wint_t __getwc_xpg5();
extern wint_t __getwchar_xpg5();
extern wint_t __fputwc_xpg5();
extern wint_t __putwc_xpg5();
extern wint_t __putwchar_xpg5();
extern wchar_t *__fgetws_xpg5();
extern int __fputws_xpg5();
extern wint_t __ungetwc_xpg5();
#endif	/* __STDC__ */
#define	fgetwc	__fgetwc_xpg5
#define	getwc	__getwc_xpg5
#define	getwchar	__getwchar_xpg5
#define	fputwc	__fputwc_xpg5
#define	putwc	__putwc_xpg5
#define	putwchar	__putwchar_xpg5
#define	fgetws	__fgetws_xpg5
#define	fputws	__fputws_xpg5
#define	ungetwc	__ungetwc_xpg5
#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif /* not XPG4 and not XPG4v2 */
#endif /* defined(_MSE_INT_H) */

#if __cplusplus >= 199711L
}
#endif /* end of namespace std */

#ifdef	__cplusplus
}
#endif

#endif	/* _ISO_WCHAR_ISO_H */
