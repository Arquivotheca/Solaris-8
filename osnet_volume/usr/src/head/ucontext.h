/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _UCONTEXT_H
#define	_UCONTEXT_H

#pragma ident	"@(#)ucontext.h	1.13	98/07/27 SMI"	/* SVr4.0 1.2	*/

#include <sys/ucontext.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)

extern int getcontext(ucontext_t *);
#pragma unknown_control_flow(getcontext)
extern int setcontext(const ucontext_t *);
extern int swapcontext(ucontext_t *, const ucontext_t *);
extern void makecontext(ucontext_t *, void(*)(), int, ...);

#else

extern int getcontext();
#pragma unknown_control_flow(getcontext)
extern int setcontext();
extern int swapcontext();
extern void makecontext();

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _UCONTEXT_H */
