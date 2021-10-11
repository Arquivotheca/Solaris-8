/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1994 by Sun Microsystems, Inc.		*/

.ident	"@(#)_so_recvmsg.s	1.2	96/06/13 SMI"

/* C library -- _so_recvmsg					*/
/* int _so_recvmsg (int socket, struct msghdr *msg, int flags);	*/

	.file	"_so_recvmsg.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL2_RESTART(_so_recvmsg,recvmsg)
	RET

	SET_SIZE(_so_recvmsg)
