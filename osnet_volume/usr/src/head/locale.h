/*
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#ifndef _LOCALE_H
#define	_LOCALE_H

#pragma ident	"@(#)locale.h	1.19	99/08/10 SMI"

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
 * OSF/1 1.2
 */
/*
 * @(#)$RCSfile: locale.h,v $ $Revision: 1.11.4.3 $ (OSF)
 * $Date: 1992/10/26 20:29:12 $
 */
/*
 * COMPONENT_NAME: (LIBCLOC) Locale Related Data Structures and API
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1985, 1989
 * All Rights Reserved
 * Licensed Materials - Property of IBM
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.24  com/inc/locale.h, libcnls, bos320, 9132320h 8/7/91
 */

#include <iso/locale_iso.h>

#if (__STDC__ - 0 == 0 && \
	!defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE)) || \
	defined(__EXTENSIONS__)
#include <libintl.h>
#endif

/*
 * Allow global visibility for symbols defined in
 * C++ "std" namespace in <iso/locale_iso.h>.
 */
#if __cplusplus >= 199711L
using std::lconv;
using std::setlocale;
using std::localeconv;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define	_LastCategory	LC_MESSAGES	/* This must be last category */

#define	_ValidCategory(c) \
	(((int)(c) >= LC_CTYPE) && ((int)(c) <= _LastCategory) || \
	((int)c == LC_ALL))

#ifdef	__cplusplus
}
#endif

#endif	/* _LOCALE_H */
