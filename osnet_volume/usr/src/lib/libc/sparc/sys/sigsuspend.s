/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)sigsuspend.s	1.7	96/05/02 SMI"	/* SVr4.0 1.3	*/

/* C library -- sigsuspend					*/
/* int sigsuspend (sigset_t *set);				*/

	.file	"sigsuspend.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.weak	_libc_sigsuspend;
	.type	_libc_sigsuspend, #function
	_libc_sigsuspend = sigsuspend

	SYSCALL(sigsuspend)
	RET

	SET_SIZE(sigsuspend)
