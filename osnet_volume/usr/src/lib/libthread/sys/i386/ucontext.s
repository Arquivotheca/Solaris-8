/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)ucontext.s 1.1	93/08/20 SMI"	/* SVr4.0 1.4	*/

/* C library -- setcontext and getcontext			*/
/* int getcontext (ucontext_t *ucp);				*/
/* int setcontext (ucontext_t *ucp);				*/

	.file	"ucontext.s"

#include <sys/asm_linkage.h>

#include "i386/SYS.h"

#define	SUBSYS_getcontext	0
#define	SUBSYS_setcontext	1

	fwdef(_getcontext)
	popl	%edx
	pushl	$ SUBSYS_getcontext
	pushl	%edx
	jmp	.sys

	fwdef(_setcontext)
	popl	%edx
	pushl	$ SUBSYS_setcontext
	pushl	%edx

.sys:
	SYSTRAP(context)
	popl	%edx
	movl	%edx, 0(%esp)
	SYSCERROR(context)
	RET
