/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1989-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)wait.s	1.4	97/08/20 SMI"	/* SVr4.0 1.9	*/

/* C library -- wait						*/
/* pid_t wait (int *stat_loc);					*/
/* pid_t wait ((int *)0);					*/

	.file	"wait.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.weak	_libc_wait;
	.type	_libc_wait, #function
	_libc_wait = wait

	SYSCALL_RESTART(wait)
	ldx	[%sp + SAVE_OFFSET], %o2
	brnz,a,pt	%o2, 1f
	st	%o1, [%o2]
1:
	RET

	SET_SIZE(wait)
