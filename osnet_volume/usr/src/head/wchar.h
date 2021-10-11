/*
 * Copyright (c) 1993, 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_WCHAR_H
#define	_WCHAR_H

#pragma ident	"@(#)wchar.h	1.33	99/08/30 SMI"

#include <iso/wchar_iso.h>

/*
 * Allow global visibility for symbols defined in
 * C++ "std" namespace in <iso/wchar_iso.h>.
 */
#if __cplusplus >= 199711L
using std::wint_t;
using std::size_t;
using std::time_t;
using std::tm;
using std::mbstate_t;
using std::fgetwc;
using std::fgetws;
using std::fputwc;
using std::fputws;
using std::ungetwc;
using std::getwc;
using std::getwchar;
using std::putwc;
using std::putwchar;
using std::wcstod;
using std::wcstol;
using std::wcstoul;
using std::wcscat;
using std::wcschr;
using std::wcscmp;
using std::wcscoll;
using std::wcscpy;
using std::wcscspn;
using std::wcslen;
using std::wcsncat;
using std::wcsncmp;
using std::wcsncpy;
using std::wcspbrk;
using std::wcsrchr;
using std::wcsspn;
using std::wcsxfrm;
using std::wcstok;
using std::wcsftime;
/* not XPG4 and not XPG4v2 */
#if (!(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 == 4)))
using std::btowc;
using std::fwprintf;
using std::fwscanf;
using std::fwide;
using std::mbsinit;
using std::mbrlen;
using std::mbrtowc;
using std::mbsrtowcs;
using std::swprintf;
using std::swscanf;
using std::vfwprintf;
using std::vwprintf;
using std::vswprintf;
using std::wcrtomb;
using std::wcsrtombs;
using std::wcsstr;
using std::wctob;
using std::wmemchr;
using std::wmemcmp;
using std::wmemcpy;
using std::wmemmove;
using std::wmemset;
using std::wprintf;
using std::wscanf;
#endif /* not XPG4 and not XPG4v2 */
#endif /* __cplusplus >= 199711L */

#ifdef	__cplusplus
extern "C" {
#endif

#if (__STDC__ - 0 == 0) || defined(_XOPEN_SOURCE) || defined(__EXTENSIONS__)
#if !defined(_WCTYPE_T) || __cplusplus >= 199711L
#define	_WCTYPE_T
typedef	int	wctype_t;
#endif
#endif /* (__STDC__ - 0 == 0) || defined(_XOPEN_SOURCE)... */

#ifdef __STDC__

#if (__STDC__ - 0 == 0) || defined(_XOPEN_SOURCE) || defined(__EXTENSIONS__)
extern int iswalpha(wint_t);
extern int iswupper(wint_t);
extern int iswlower(wint_t);
extern int iswdigit(wint_t);
extern int iswxdigit(wint_t);
extern int iswalnum(wint_t);
extern int iswspace(wint_t);
extern int iswpunct(wint_t);
extern int iswprint(wint_t);
extern int iswgraph(wint_t);
extern int iswcntrl(wint_t);
extern int iswctype(wint_t, wctype_t);
extern wint_t towlower(wint_t);
extern wint_t towupper(wint_t);
extern wchar_t *wcswcs(const wchar_t *, const wchar_t *);
extern int wcswidth(const wchar_t *, size_t);
extern int wcwidth(const wchar_t);
extern wctype_t wctype(const char *);
#endif /* #if (__STDC__ - 0 == 0) || defined(_XOPEN_SOURCE)... */

#else /* __STDC__ */

#if (__STDC__ - 0 == 0) || defined(_XOPEN_SOURCE) || defined(__EXTENSIONS__)
extern  int iswalpha();
extern  int iswupper();
extern  int iswlower();
extern  int iswdigit();
extern  int iswxdigit();
extern  int iswalnum();
extern  int iswspace();
extern  int iswpunct();
extern  int iswprint();
extern  int iswgraph();
extern  int iswcntrl();
extern  int iswctype();
extern  wint_t towlower();
extern  wint_t towupper();
extern wchar_t *wcswcs();
extern int wcswidth();
extern int wcwidth();
extern wctype_t wctype();
#endif /* (__STDC__ - 0 == 0) || defined(_XOPEN_SOURCE)... */

#endif /* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _WCHAR_H */
