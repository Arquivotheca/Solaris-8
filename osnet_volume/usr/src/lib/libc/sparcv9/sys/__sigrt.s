/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)__sigrt.s	1.3	98/04/01 SMI"

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
