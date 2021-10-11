/*	Copyright (c) 1988 AT&T	*/
/*	  All rights reserved.	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ucontext.s	1.8	97/11/06 SMI"

/* C library -- setcontext and getcontext			*/
/* int getcontext (ucontext_t *ucp);				*/
/* int setcontext (ucontext_t *ucp);				*/

	.file	"ucontext.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

#define	SUBSYS_getcontext	0
#define	SUBSYS_setcontext	1

/*
	ENTRY(__getcontext)
	mov	%o0, %o1
	b	.sys
	mov	SUBSYS_getcontext, %o0
*/

	ENTRY(__setcontext)
	mov	%o0, %o1
	mov	SUBSYS_setcontext, %o0

.sys:
	SYSTRAP(context)
	SYSCERROR
	RET

	SET_SIZE(__setcontext)
