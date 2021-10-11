/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)poll.s	1.2	98/02/27 SMI"	/* SVr4.0 1.5	*/

/* C library -- poll						*/
/* int poll(struct poll fds[], unsigned long nfds, int timeout);*/

	.file	"poll.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.weak   _libc_poll;
	.type   _libc_poll, #function
	_libc_poll = poll

	SYSCALL(poll)
	RET

	SET_SIZE(poll)
