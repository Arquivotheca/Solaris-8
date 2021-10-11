/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)__sigrt.s	1.4	95/09/08 SMI"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.file "__sigrt.s"


/*
 * int
 * __sigqueue(pid, signo, value)
 *	pid_t pid;
 *	int signo;
 *	const union sigval value;
 */

	ENTRY(__sigqueue)
	ld	[%o2],%o2	/* SPARC ABI passes union in memory */
	SYSTRAP(sigqueue)
	SYSCERROR
	RET
	SET_SIZE(__sigqueue)

/*
 * int
 * _libc_sigtimedwait(set, info, timeout)
 *	const sigset_t *set;
 *	siginfo_t *info;
 *	const struct timespec *timeout;
 */

	ENTRY(_libc_sigtimedwait)
	SYSTRAP(sigtimedwait)
	SYSCERROR
	RET
	SET_SIZE(_libc_sigtimedwait)
