/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)sigprocmsk.s	1.1	96/12/04 SMI"	/* SVr4.0 1.3	*/

/* C library -- sigprocmask					*/
/* int sigprocmask (int how, sigset_t *set, sigset_t *oset)	*/

	.file	"sigprocmsk.s"

#include <sys/asm_linkage.h>


#include "SYS.h"

	ENTRY(_libc_sigprocmask)
	SYSTRAP(sigprocmask)
	SYSCERROR

	RET

	SET_SIZE(_libc_sigprocmask)
