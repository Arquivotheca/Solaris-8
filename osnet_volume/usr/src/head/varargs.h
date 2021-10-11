/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _VARARGS_H
#define	_VARARGS_H

#pragma ident	"@(#)varargs.h	1.40	99/03/11 SMI"

#include <sys/va_list.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The type associated with va_list is defined in <va_list.h> under the
 * implementation name __va_list.  This protects the ANSI-C, POSIX and
 * XPG namespaces.  Including this file allows (requires) the name va_list
 * to exist in the these namespaces.  Programs written to these source
 * standards should be migrating to using <stdarg.h>.
 */
#ifndef	_VA_LIST
#define	_VA_LIST
typedef __va_list va_list;
#endif

/*
 * Many compilation systems depend upon the use of special functions
 * built into the the compilation system to handle variable argument
 * lists and stack allocations.  The method to obtain this in SunOS
 * is to define the feature test macro "__BUILTIN_VA_ARG_INCR" which
 * enables the following special built-in functions:
 *	__builtin_alloca
 *	__builtin_va_alist
 *      __builtin_va_arg_incr
 * It is intended that the compilation system define this feature test
 * macro, not the user of the system.
 *
 * The tests on the processor type are to provide a transitional period
 * for existing compilation systems, and may be removed in a future
 * release.
 */

#if (defined(__BUILTIN_VA_ARG_INCR) || defined(__sparc) || defined(__i386))
#define	va_alist __builtin_va_alist
#endif
#if defined(__sparcv9)
#define	va_dcl long va_alist;
#else
#define	va_dcl int va_alist;
#endif

#if (defined(__BUILTIN_VA_ARG_INCR) || defined(__sparc) || defined(__i386)) && \
	!defined(__lint)

/*
 * Instruction set architectures which use a simple pointer to the
 * argument save area share a common implementation.
 */
#define	va_start(list)		list = (char *) &va_alist
#define	va_arg(list, mode)	((mode *)__builtin_va_arg_incr((mode *)list))[0]

#else	/* defined(__BUILTIN_VA_ARG_INCR) && ... !defined(__lint) */

/*
 * The following are appropriate implementations for most implementations
 * which have completely stack based calling conventions.  These are also
 * appropriate for lint usage on all systems where a va_list is a simple
 * pointer.
 */
#define	va_start(list)		list = (char *) &va_alist
#define	va_arg(list, mode) \
	((mode *)(list = (void *)((char *)list + sizeof (mode))))[-1]

#endif	/* defined(__BUILTIN_VA_ARG_INCR) ... !(defined(__lint) */

#define	va_end(list)

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

#endif	/* _VARARGS_H */
