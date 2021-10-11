/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1994 by Sun Microsystems, Inc.		*/

.ident	"@(#)_so_connect.s	1.3	98/07/27 SMI"

/* C library -- _so_connect						*/
/* int _so_connect (int socket, struct sockaddr *name, int namelen, int vers); */

	.file	"_so_connect.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL2(_so_connect,connect)
	RET

	SET_SIZE(_so_connect)
