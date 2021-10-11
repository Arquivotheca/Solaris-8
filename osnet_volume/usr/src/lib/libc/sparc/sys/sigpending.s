/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)sigpending.s	1.7	96/05/02 SMI"	/* SVr4.0 1.4	*/

/* C library -- sigpending					*/
/* int sigpending (sigset_t *set);				*/

	.file	"sigpending.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

#define	SUBSYS_sigpending	1
#define	SUBSYS_sigfillset	2
#define	SUBSYS_mt_sigpending	3

	.weak	_libc_sigpending;
	.type	_libc_sigpending, #function
	_libc_sigpending = sigpending

	ENTRY(sigpending)
	mov	%o0, %o1
	ba	.sys
	mov	SUBSYS_sigpending, %o0

        ENTRY(__mt_sigpending)
        mov     %o0, %o1
        ba      .sys
        mov     SUBSYS_mt_sigpending, %o0

	ENTRY(__sigfillset)
	mov	%o0, %o1
	mov	SUBSYS_sigfillset, %o0

.sys:
	SYSTRAP(sigpending)
	SYSCERROR
	RET

	SET_SIZE(sigpending)
	SET_SIZE(__sigfillset)
	SET_SIZE(__mt_sigpending)
