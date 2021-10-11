/*
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
 * All Rights Reserved
 */

/*
 * COPYRIGHT NOTICE
 *
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 *
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED
 */

/*
 * (C) COPYRIGHT International Business Machines Corp. 1985, 1989
 * All Rights Reserved
 * Licensed Materials - Property of IBM
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
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
 * must also be added to <locale.h>.
 */

#ifndef	_ISO_LOCALE_ISO_H
#define	_ISO_LOCALE_ISO_H

#pragma ident	"@(#)locale_iso.h	1.1	99/08/09 SMI"

#include <sys/feature_tests.h>

#ifdef __cplusplus
extern "C" {
#endif

#if __cplusplus >= 199711L
namespace std {
#endif

struct lconv {
	char *decimal_point;	/* decimal point character */
	char *thousands_sep;	/* thousands separator */
	char *grouping;			/* digit grouping */
	char *int_curr_symbol;	/* international currency symbol */
	char *currency_symbol;	/* national currency symbol */
	char *mon_decimal_point;	/* currency decimal point */
	char *mon_thousands_sep;	/* currency thousands separator */
	char *mon_grouping;		/* currency digits grouping */
	char *positive_sign;	/* currency plus sign */
	char *negative_sign;	/* currency minus sign */
	char int_frac_digits;	/* internat currency fractional digits */
	char frac_digits;		/* currency fractional digits */
	char p_cs_precedes;		/* currency plus location */
	char p_sep_by_space;	/* currency plus space ind. */
	char n_cs_precedes;		/* currency minus location */
	char n_sep_by_space;	/* currency minus space ind. */
	char p_sign_posn;		/* currency plus position */
	char n_sign_posn;		/* currency minus position */
};

#define	LC_CTYPE	0	/* locale's ctype handline */
#define	LC_NUMERIC	1	/* locale's decimal handling */
#define	LC_TIME		2	/* locale's time handling */
#define	LC_COLLATE	3	/* locale's collation data */
#define	LC_MONETARY	4	/* locale's monetary handling */
#define	LC_MESSAGES	5	/* locale's messages handling */
#define	LC_ALL		6	/* name of locale's category name */

#ifndef	NULL
#if defined(_LP64) && !defined(__cplusplus)
#define	NULL    0L
#else
#define	NULL    0
#endif
#endif

#if	defined(__STDC__)
extern char	*setlocale(int, const char *);
extern struct lconv *localeconv(void);
#else
extern char   *setlocale();
extern struct lconv	*localeconv();
#endif

#if __cplusplus >= 199711L
} 
#endif /* end of namespace std */

#ifdef	__cplusplus
}
#endif

#endif	/* _ISO_LOCALE_ISO_H */
