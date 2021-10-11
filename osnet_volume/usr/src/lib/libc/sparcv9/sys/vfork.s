/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)vfork.s	1.3	97/03/12 SMI"

/*
 * C library -- fork
 * pid_t vfork(void)
 */

/*
 * From the syscall:
 * %o1 == 0 in parent process, %o1 == 1 in child process.
 * %o0 == pid of child in parent, %o0 == pid of parent in child.
 *
 * The child process gets a zero return value from fork; the parent
 * gets the pid of the child.
 *
 * Note that since the SPARC architecture maintains stack maintence
 * information (return pc, sp, fp) in the register windows, both parent
 * and child can execute in a common address space without conflict.
 */

	.file	"vfork.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(vfork,function)

#include "SYS.h"

	SYSCALL(vfork)
	movrnz	%o1, 0, %o0
	RET
	SET_SIZE(vfork)

