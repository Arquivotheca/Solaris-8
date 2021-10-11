/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)sigaltstk.s	1.5	92/07/14 SMI"	/* SVr4.0 1.3	*/

/* C library -- sigaltstack					*/
/* int sigaltstack (stack_t *ss, stack_t *oss);			*/

	.file	"sigaltstk.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(sigaltstack,function)

#include "SYS.h"

	SYSCALL(sigaltstack)
	RET

	SET_SIZE(sigaltstack)
