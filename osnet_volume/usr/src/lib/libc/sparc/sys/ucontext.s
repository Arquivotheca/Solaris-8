/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)ucontext.s	1.5	92/07/14 SMI"	/* SVr4.0 1.4	*/

/* C library -- setcontext and getcontext			*/
/* int getcontext (ucontext_t *ucp);				*/
/* int setcontext (ucontext_t *ucp);				*/

	.file	"ucontext.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(setcontext,function)

#include "SYS.h"

#define	SUBSYS_getcontext	0
#define	SUBSYS_setcontext	1

	ENTRY(__getcontext)
	mov	%o0, %o1
	b	.sys
	mov	SUBSYS_getcontext, %o0

	ENTRY(setcontext)
	mov	%o0, %o1
	mov	SUBSYS_setcontext, %o0

.sys:
	SYSTRAP(context)
	SYSCERROR
	RET

	SET_SIZE(__getcontext)
	SET_SIZE(setcontext)
