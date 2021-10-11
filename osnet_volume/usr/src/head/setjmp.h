/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */


#ifndef _SETJMP_H
#define	_SETJMP_H

#pragma ident	"@(#)setjmp.h	1.36	99/08/10 SMI"	/* SVr4.0 1.9.2.9 */

#include <iso/setjmp_iso.h>

/*
 * Allow global visibility for symbols defined in
 * C++ "std" namespace in <iso/setjmp_iso.h>.
 */
#if __cplusplus >= 199711L
using std::jmp_buf;
using std::longjmp;
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)

#if __STDC__ == 0 || defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
	defined(__EXTENSIONS__)
/* non-ANSI standard compilation */

#if defined(_LP64) || defined(_I32LPx)
typedef long sigjmp_buf[_SIGJBLEN];
#else
typedef int sigjmp_buf[_SIGJBLEN];
#endif

extern int sigsetjmp(sigjmp_buf, int);
#pragma unknown_control_flow(sigsetjmp)
extern void siglongjmp(sigjmp_buf, int);
#endif

#else /* __STDC__ */

#if defined(_LP64) || defined(_I32LPx)
typedef long sigjmp_buf[_SIGJBLEN];
#else
typedef int sigjmp_buf[_SIGJBLEN];
#endif

extern int sigsetjmp();
#pragma unknown_control_flow(sigsetjmp)
extern void siglongjmp();

#endif  /* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _SETJMP_H */
