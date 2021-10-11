/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_VARARGS_H
#define	_SYS_VARARGS_H

#pragma ident	"@(#)varargs.h	1.45	99/05/04 SMI"	/* UCB 4.1 83/05/03 */

#ifdef	__cplusplus
extern "C" {
#endif

#include	<sys/va_list.h>

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

/*
 * The type associated with va_list is defined in <sys/va_list.h> under the
 * implementation name __va_list.  This protects the ANSI-C, POSIX and
 * XPG namespaces.  Including this file allows (requires) the name va_list
 * to exist in the these namespaces.
 */
#ifndef	_VA_LIST
#define	_VA_LIST
typedef __va_list va_list;
#endif

/*
 * When __STDC__ is defined, this file provides stdarg semantics despite
 * the name of the file.
 */
#if defined(__STDC__)

#if (defined(__BUILTIN_VA_ARG_INCR) || defined(sparc) || defined(__sparc) || \
    defined(i386) || defined(__i386)) && !(defined(lint) || defined(__lint))

#define	va_start(list, name)	(void) (list = (va_list) &__builtin_va_alist)
#define	va_arg(list, mode)	((mode *)__builtin_va_arg_incr((mode *)list))[0]

#elif defined(__ia64)

#define	VA_ALIGN	8

/* Figure out the argument slot length for an argument of type "t". */
#define	_ARGSIZEOF(t)	((sizeof (t) + VA_ALIGN - 1) & ~(VA_ALIGN - 1))

#if defined(__epcg__)
#define	va_start(list, name)	(__va_start(&list))
#else
#define	va_start(list, name)	(list = (va_list)&name + _ARGSIZEOF(name))
#endif

#define	va_arg(list, t)	(*(t *)((list += _ARGSIZEOF(t)) - _ARGSIZEOF(t)))
#define	va_end(list)	(list = (va_list)0)

#else	/* defined(__BUILTIN_VA_ARG_INCR) && !(defined(lint) || ... */

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

#endif	/* defined(__BUILTIN_VA_ARG_INCR) && !(defined(lint) || ... */

#ifndef __ia64
extern void va_end(va_list);
#define	va_end(list) (void)0
#endif

#else	/* ! __STDC__ */

/*
 * In the absence of __STDC__, this file provides traditional varargs
 * semantics.
 */

#if (defined(__BUILTIN_VA_ARG_INCR) || defined(sparc) || defined(__sparc) || \
    defined(i386) || defined(__i386))
#define	va_alist __builtin_va_alist
#endif
#define	va_dcl int va_alist;

#if (defined(__BUILTIN_VA_ARG_INCR) || defined(sparc) || defined(__sparc) || \
    defined(i386) || defined(__i386)) && !(defined(lint) || defined(__lint))

/*
 * Instruction set architectures which use a simple pointer to the
 * argument save area share a common implementation.
 */
#define	va_start(list)		list = (char *)&va_alist
#define	va_arg(list, mode)	((mode *)__builtin_va_arg_incr((mode *)list))[0]

#elif defined(__ia64)

#define	VA_ALIGN	8

/* Figure out the argument slot length for an argument of type "t". */
#define	_ARGSIZEOF(t)	((sizeof (t) + VA_ALIGN - 1) & ~(VA_ALIGN - 1))

#define	va_dcl	va_list	va_alist;

#if defined(__epcg__)
#define	va_start(list)	(__va_start(&list))
#else
#define	va_start(list)	(list = (va_list)&va_alist)
#endif

#define	va_arg(list, t)	(*(t *)((list += _ARGSIZEOF(t)) - _ARGSIZEOF(t)))
#define	va_end(list)	(list = (va_list)0)

#else	/* defined(__BUILTIN_VA_ARG_INCR) && !(defined(lint) || ... ) */

/*
 * The following are appropriate implementations for most implementations
 * which have completely stack based calling conventions.  These are also
 * appropriate for lint usage on all systems where a va_list is a simple
 * pointer.
 */
#define	va_start(list)		list = (char *)&va_alist
#define	va_arg(list, mode)	((mode *)(list += sizeof (mode)))[-1]

#endif	/* defined(__BUILTIN_VA_ARG_INCR) && !(defined(lint) || ... ) */

#define	va_end(list)

#endif	/* __STDC__ */

/*
 * va_copy is a Solaris extension to provide a portable way to perform
 * a variable argument list ``bookmarking'' function.
 */
#define	va_copy(to, from)	((to) = (from))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VARARGS_H */
