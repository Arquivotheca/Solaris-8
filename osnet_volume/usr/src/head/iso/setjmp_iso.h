/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
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
 * must also be added to <setjmp.h>.
 */

#ifndef _ISO_SETJMP_ISO_H
#define	_ISO_SETJMP_ISO_H

#pragma ident	"@(#)setjmp_iso.h	1.1	99/08/09 SMI"
/* SVr4.0 1.9.2.9 */

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _JBLEN

/*
 * The sizes of the jump-buffer (_JBLEN) and the sigjump-buffer
 * (_SIGJBLEN) are defined by the appropriate, processor specific,
 * ABI.
 */
#if defined(__i386)
#define	_JBLEN		10	/* ABI value */
#define	_SIGJBLEN	128	/* ABI value */
#elif defined(__ia64)
#define	_JBLEN		512	/* XXX - not really - ABI value */
#define	_SIGJBLEN	1024	/* ABI value */
/* XXX - needs work */
#elif defined(__sparcv9)
#define	_JBLEN		12	/* ABI value */
#define	_SIGJBLEN	19	/* ABI value */
#elif defined(__sparc)
#define	_JBLEN		12	/* ABI value */
#define	_SIGJBLEN	19	/* ABI value */
#else
#error "ISA not supported"
#endif

#if __cplusplus >= 199711L
namespace std {
#endif

#if defined(__i386) || defined(__sparc)
#if defined(_LP64) || defined(_I32LPx)
typedef long	jmp_buf[_JBLEN];
#else
typedef int	jmp_buf[_JBLEN];
#endif
#elif defined(__ia64)
typedef long double jmp_buf[_JBLEN];
#else
#error "ISA not supported"
#endif

#if defined(__STDC__)

extern int setjmp(jmp_buf);
#pragma unknown_control_flow(setjmp)
extern int _setjmp(jmp_buf);
#pragma unknown_control_flow(_setjmp)
extern void longjmp(jmp_buf, int);
extern void _longjmp(jmp_buf, int);

#else

extern int setjmp();
#pragma unknown_control_flow(setjmp)
extern int _setjmp();
#pragma unknown_control_flow(_setjmp)
extern void longjmp();
extern void _longjmp();

#endif  /* __STDC__ */

#if __cplusplus >= 199711L
}
#endif /* end of namespace std */

#if __cplusplus >= 199711L
using std::setjmp;
#endif

#if (__STDC__ - 0 != 0) || __cplusplus >= 199711L
#define	setjmp(env)	setjmp(env)
#endif

#endif  /* _JBLEN */

#ifdef	__cplusplus
}
#endif

#endif	/* _ISO_SETJMP_ISO_H */
