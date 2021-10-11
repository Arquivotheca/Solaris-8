/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_ASSERT_H
#define	_ASSERT_H

#pragma ident	"@(#)assert.h	1.9	92/07/14 SMI"	/* SVr4.0 1.6.1.4 */

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)
extern void __assert(const char *, const char *, int);
#else
extern void _assert();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _ASSERT_H */

/*
 * Note that the ANSI C Standard requires all headers to be idempotent except
 * <assert.h> which is explicitly required not to be idempotent (section 4.1.2).
 * Therefore, it is by intent that the header guards (#ifndef _ASSERT_H) do
 * not span this entire file.
 */

#undef	assert

#ifdef	NDEBUG

#define	assert(EX) ((void)0)

#else

#if defined(__STDC__)
#define	assert(EX) (void)((EX) || (__assert(#EX, __FILE__, __LINE__), 0))
#else
#define	assert(EX) (void)((EX) || (_assert("EX", __FILE__, __LINE__), 0))
#endif	/* __STDC__ */

#endif	/* NDEBUG */
