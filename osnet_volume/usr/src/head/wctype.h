/*	wctype.h	1.13 89/11/02 SMI; JLE	*/
/*	from AT&T JAE 2.1			*/
/*	definitions for international functions	*/

/*
 * Copyright (c) 1991-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_WCTYPE_H
#define	_WCTYPE_H

#pragma ident	"@(#)wctype.h	1.18	99/08/10 SMI"

#include <iso/wctype_iso.h>
#if ((!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) && \
	!defined(_POSIX_SOURCE) && (__STDC__ - 0 != 1)) || \
	defined(__EXTENSIONS__))
#include <ctype.h>
#include <wchar.h>
#endif

/*
 * Allow global visibility for symbols defined in
 * C++ "std" namespace in <iso/wctype_iso.h>.
 */
#if __cplusplus >= 199711L
using std::wint_t;
using std::wctrans_t;
using std::iswalnum;
using std::iswalpha;
using std::iswcntrl;
using std::iswdigit;
using std::iswgraph;
using std::iswlower;
using std::iswprint;
using std::iswpunct;
using std::iswspace;
using std::iswupper;
using std::iswxdigit;
using std::towlower;
using std::towupper;
using std::wctrans;
using std::towctrans;
using std::iswctype;
using std::wctype;
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/* do not allow any of the following in a strictly conforming application */
#if ((!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) && \
	!defined(_POSIX_SOURCE) && (__STDC__ - 0 != 1)) || \
	defined(__EXTENSIONS__))

/*
 * data structure for supplementary code set
 * for character class and conversion
 */
struct	_wctype {
	wchar_t	tmin;		/* minimum code for wctype */
	wchar_t	tmax;		/* maximum code for wctype */
	unsigned char  *index;	/* class index */
	unsigned int   *type;	/* class type */
	wchar_t	cmin;		/* minimum code for conversion */
	wchar_t	cmax;		/* maximum code for conversion */
	wchar_t *code;		/* conversion code */
};

/* character classification functions */

/* iswascii is still a macro */
#define	iswascii(c)	isascii(c)

/* isw*, except iswascii(), are not macros any more.  They become functions */
#ifdef __STDC__

extern	unsigned _iswctype(wchar_t, int);
extern	wchar_t _trwctype(wchar_t, int);
/* is* also become functions */
extern	int isphonogram(wint_t);
extern	int isideogram(wint_t);
extern	int isenglish(wint_t);
extern	int isnumber(wint_t);
extern	int isspecial(wint_t);
#else

extern  unsigned _iswctype();
extern  wchar_t _trwctype();
/* is* also become functions */
extern  int isphonogram();
extern  int isideogram();
extern  int isenglish();
extern  int isnumber();
extern  int isspecial();
#endif

#define	iscodeset0(c)	isascii(c)
#define	iscodeset1(c)	(((c) & WCHAR_CSMASK) == WCHAR_CS1)
#define	iscodeset2(c)	(((c) & WCHAR_CSMASK) == WCHAR_CS2)
#define	iscodeset3(c)	(((c) & WCHAR_CSMASK) == WCHAR_CS3)

#endif /* ((!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) ... */

#ifdef	__cplusplus
}
#endif

#endif	/* _WCTYPE_H */
