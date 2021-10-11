/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _STDARG_H
#define	_STDARG_H

#pragma ident	"@(#)stdarg.h	1.45	99/08/10 SMI"	/* SVr4.0 1.8	*/

#if defined(__STDC__)

#include <iso/stdarg_iso.h>

/*
 * Allow global visibility for symbols defined in
 * C++ "std" namespace in <iso/stdarg_iso.h>.
 */
#if __cplusplus >= 199711L
using std::va_list;
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * va_copy is a Solaris extension to provide a portable way to perform
 * a variable argument list ``bookmarking'' function.
 */
#if defined(__EXTENSIONS__) || ((__STDC__ - 0 == 0) && \
	!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))

#define	va_copy(to, from)	((to) = (from))

#endif	/* defined(__EXTENSIONS__) || ((__STDC__ - 0 == 0) && ... ) */

#ifdef	__cplusplus
}
#endif

#else	/* __STDC__ */

#include <varargs.h>

#endif	/* __STDC__ */

#endif	/* _STDARG_H */
