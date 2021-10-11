/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)fork.s	1.3	97/03/12 SMI"

/*
 * C library -- fork
 * pid_t fork(void)
 */

/*
 * From the syscall:
 * %o1 == 0 in parent process, %o1 == 1 in child process.
 * %o0 == pid of child in parent, %o0 == pid of parent in child.
 *
 * The child process gets a zero return value from fork; the parent
 * gets the pid of the child.
 */

	.file	"fork.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(_libc_fork)
	SYSTRAP(fork)
	SYSCERROR
	movrnz	%o1, 0, %o0
	RET
	SET_SIZE(_libc_fork)

