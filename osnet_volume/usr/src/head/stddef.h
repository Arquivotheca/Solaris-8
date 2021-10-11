/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _STDDEF_H
#define	_STDDEF_H

#pragma ident	"@(#)stddef.h	1.16	99/08/10 SMI"	/* SVr4.0 1.5 */

#include <sys/isa_defs.h>
#include <iso/stddef_iso.h>

/*
 * Allow global visibility for symbols defined in
 * C++ "std" namespace in <iso/stddef_iso.h>.
 */
#if __cplusplus >= 199711L
using std::ptrdiff_t;
using std::size_t;
#endif

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
#endif  /* !_WCHAR_T */

#ifdef	__cplusplus
}
#endif

#endif	/* _STDDEF_H */
