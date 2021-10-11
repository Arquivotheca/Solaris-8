/*	wctype.h	1.13 89/11/02 SMI; JLE	*/
/*	from AT&T JAE 2.1			*/
/*	definitions for international functions	*/

/*
 * Copyright (c) 1991,1997-1999, by Sun Microsystems, Inc.
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
 * must also be added to <wctype.h>.
 */

#ifndef	_ISO_WCTYPE_ISO_H
#define	_ISO_WCTYPE_ISO_H

#pragma ident	"@(#)wctype_iso.h	1.1	99/08/09 SMI"

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if __cplusplus >= 199711L
namespace std {
#endif

#if !defined(_WINT_T) || __cplusplus >= 199711L
#define	_WINT_T
#if defined(_LP64)
typedef	int	wint_t;
#else
typedef long	wint_t;
#endif
#endif  /* !defined(_WINT_T) || __cplusplus >= 199711L */

#if !defined(_WCTYPE_T) || __cplusplus >= 199711L
#define	_WCTYPE_T
typedef	int	wctype_t;
#endif

typedef unsigned int	wctrans_t;

/* not XPG4 and not XPG4v2 */
#if (!(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 == 4)))
#ifndef WEOF
#define	WEOF	((wint_t) (-1))
#endif
#endif /* not XPG4 and not XPG4v2 */

#ifdef __STDC__
extern	int iswalnum(wint_t);
extern	int iswalpha(wint_t);
extern	int iswcntrl(wint_t);
extern	int iswdigit(wint_t);
extern	int iswgraph(wint_t);
extern	int iswlower(wint_t);
extern	int iswprint(wint_t);
extern	int iswpunct(wint_t);
extern	int iswspace(wint_t);
extern	int iswupper(wint_t);
extern	int iswxdigit(wint_t);
/* tow* also become functions */
extern	wint_t towlower(wint_t);
extern	wint_t towupper(wint_t);
extern	wctrans_t wctrans(const char *);
extern	wint_t towctrans(wint_t, wctrans_t);
extern  int iswctype(wint_t, wctype_t);
extern  wctype_t wctype(const char *);
#else
extern  int iswalnum();
extern  int iswalpha();
extern  int iswcntrl();
extern  int iswdigit();
extern  int iswgraph();
extern  int iswlower();
extern  int iswprint();
extern  int iswpunct();
extern  int iswspace();
extern  int iswupper();
extern  int iswxdigit();
/* tow* also become functions */
extern  wint_t towlower();
extern  wint_t towupper();
extern	wctrans_t wctrans();
extern	wint_t towctrans();
extern  int iswctype();
extern  wctype_t wctype();
#endif

/* bit definition for character class */

#define	_E1	0x00000100	/* phonogram (international use) */
#define	_E2	0x00000200	/* ideogram (international use) */
#define	_E3	0x00000400	/* English (international use) */
#define	_E4	0x00000800	/* number (international use) */
#define	_E5	0x00001000	/* special (international use) */
#define	_E6	0x00002000	/* other characters (international use) */
#define	_E7	0x00004000	/* reserved (international use) */
#define	_E8	0x00008000	/* reserved (international use) */

#define	_E9	0x00010000
#define	_E10	0x00020000
#define	_E11	0x00040000
#define	_E12	0x00080000
#define	_E13	0x00100000
#define	_E14	0x00200000
#define	_E15	0x00400000
#define	_E16	0x00800000
#define	_E17	0x01000000
#define	_E18	0x02000000
#define	_E19	0x04000000
#define	_E20	0x08000000
#define	_E21	0x10000000
#define	_E22	0x20000000
#define	_E23	0x40000000
#define	_E24	0x80000000

#if __cplusplus >= 199711L
}
#endif /* end of namespace std */

#ifdef	__cplusplus
}
#endif

#endif	/* _ISO_WCTYPE_ISO_H */
