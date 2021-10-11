/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
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
 * must also be added to <stdarg.h.h>.
 */

#ifndef _ISO_STDARG_ISO_H
#define	_ISO_STDARG_ISO_H

#pragma ident	"@(#)stdarg_iso.h	1.1	99/08/09 SMI" /* SVr4.0 1.8 */

#include <sys/va_list.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The type associated with va_list is defined in <sys/va_list.h> under the
 * implementation name __va_list.  This protects the ANSI-C, POSIX and
 * XPG namespaces.  Including this file allows (requires) the name va_list
 * to exist in the these namespaces.
 */
#if __cplusplus >= 199711L
namespace std {
#endif

#if !defined(_VA_LIST) || __cplusplus >= 199711L
#define	_VA_LIST
typedef __va_list va_list;
#endif

#if __cplusplus >= 199711L
}
#endif /* end of namespace std */

/*
 * Many compilation systems depend upon the use of special functions
 * built into the the compilation system to handle variable argument
 * lists and stack allocations.  The method to obtain this in SunOS
 * is to define the feature test macro "__BUILTIN_VA_ARG_INCR" which
 * enables the following special built-in functions:
 *	__builtin_alloca
 *	__builtin_va_alist
 *	__builtin_va_arg_incr
 * It is intended that the compilation system define this feature test
 * macro, not the user of the system.
 *
 * The tests on the processor type are to provide a transitional period
 * for existing compilation systems, and may be removed in a future
 * release.
 */
#if (defined(__BUILTIN_VA_ARG_INCR) || defined(__sparc) || \
	defined(__i386)) && !defined(__lint)

/*
 * Instruction set architectures which use a simple pointer to the
 * argument save area share a common implementation.
 */
#define	va_start(list, name)	(void) (list = (__va_list) &__builtin_va_alist)
#define	va_arg(list, mode)	((mode *)__builtin_va_arg_incr((mode *)list))[0]

#elif defined(__ia64)

#define	VA_ALIGN	8

/* Figure out the argument slot length for an argument of type "t". */
#define	_ARGSIZEOF(t)	((sizeof (t) + VA_ALIGN - 1) & ~(VA_ALIGN - 1))

#if defined(__epcg__)
#define	va_start(list, name)	(__va_start(&list))
#else
#define	va_start(list, name)	(list = (__va_list)&name + _ARGSIZEOF(name))
#endif

#define	va_arg(list, t)	(*(t *)((list += _ARGSIZEOF(t)) - _ARGSIZEOF(t)))
#define	va_end(list)	(list = (__va_list)0)

#else	/* defined(__BUILTIN_VA_ARG_INCR) || defined(__sparc) || ...) */

/*
 * The following are appropriate implementations for most implementations
 * which have completely stack based calling conventions.  These are also
 * appropriate for lint usage on all systems where a va_list is a simple
 * pointer.
 */
#if __STDC__ != 0	/* -Xc compilation */
#define	va_start(list, name) (void) (list = (void *)((char *)&name + \
	((sizeof (name) + (sizeof (int) - 1)) & ~(sizeof (int) - 1))))
#else
#define	va_start(list, name) (void) (list = (void *)((char *)&...))
#endif	/* __STDC__ != 0 */
#define	va_arg(list, mode) \
	((mode *)(list = (void *)((char *)list + sizeof (mode))))[-1]

#endif	/* defined(__BUILTIN_VA_ARG_INCR) || defined(__sparc) || ...) */

#if !defined(__ia64)
extern void va_end(__va_list);
#define	va_end(list) (void)0
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _ISO_STDARG_ISO_H */
