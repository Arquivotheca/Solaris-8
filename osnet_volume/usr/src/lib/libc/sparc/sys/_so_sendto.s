/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1994 by Sun Microsystems, Inc.		*/

.ident	"@(#)_so_sendto.s	1.2	96/06/13 SMI"

/* C library -- _so_sendto					*/
/*
 * int _so_sendto (int socket, void *buffer, size_t len, int flags,
 *		 struct sockaddr *address, size_t address_len);
 */

	.file	"_so_sendto.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL2_RESTART(_so_sendto,sendto)
	RET

	SET_SIZE(_so_sendto)
