/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1994 by Sun Microsystems, Inc.		*/

.ident	"@(#)_so_getsockopt.s	1.1	96/12/04 SMI"

/* C library -- _so_getsockopt					*/
/* int _so_getsockopt (int socket, int level, int option_name,	*/
/*	 void *option_value, size_t *option_len, int vers);	*/

	.file	"_so_getsockopt.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL2(_so_getsockopt,getsockopt)
	RET

	SET_SIZE(_so_getsockopt)
