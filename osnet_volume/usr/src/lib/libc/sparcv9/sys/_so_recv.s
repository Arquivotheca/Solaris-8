/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1994 by Sun Microsystems, Inc.		*/

.ident	"@(#)_so_recv.s	1.1	96/12/04 SMI"

/* C library -- _so_recv						*/
/* int _so_recv (int socket, void *buffer, size_t len, int flags);	*/

	.file	"_so_recv.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL2_RESTART(_so_recv,recv)
	RET

	SET_SIZE(_so_recv)
