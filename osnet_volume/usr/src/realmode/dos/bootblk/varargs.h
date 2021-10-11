/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _VARARGS_H
#define	_VARARGS_H

#ident	"@(#)varargs.h	1.3	99/01/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/***
*varargs.h - XENIX style macros for variable argument functions
*
*       Copyright (c) 1985-1992, Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       This file defines XENIX style macros for accessing arguments of a
*       function which takes a variable number of arguments.
*       [System V]
*
****/

typedef char *va_list;

/*
 * define a macro to compute the size of a type, variable or expression,
 * rounded up to the nearest multiple of sizeof(int). This number is its
 * size as function argument (Intel architecture). Note that the macro
 * depends on sizeof(int) being a power of 2!
 */

#define _INTSIZEOF(n)    ( (sizeof(n) + sizeof(int) - 1) & ~(sizeof(int) - 1) )

#define va_dcl va_list va_alist;
#define va_start(ap) ap = (va_list)&va_alist
#define va_arg(ap,t) ( *(t *)((ap += _INTSIZEOF(t)) - _INTSIZEOF(t)) )
#define va_end(ap) ap = (va_list)0

#ifdef	__cplusplus
}
#endif

#endif /* _VARARGS_H */
