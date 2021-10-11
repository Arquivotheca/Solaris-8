/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)kill.s	1.2	97/07/02 SMI"	/* SVr4.0 1.8	*/

/* C library -- kill						*/
/* int kill (pid_t pid, int sig);				*/

	.file	"kill.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	.weak	_libc_kill
	.type	_libc_kill, #function
	_libc_kill = _kill

	SYSCALL(kill)
	RETC

	SET_SIZE(kill)
