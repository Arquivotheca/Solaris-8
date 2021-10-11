/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1994 by Sun Microsystems, Inc.		*/

.ident	"@(#)_so_setsockopt.s	1.2	96/06/13 SMI"

/* C library -- _so_setsockopt					*/
/* int _so_setsockopt (int socket, int level, int option_name,	*/
/*	 void *option_value, size_t option_len, int vers);	*/

	.file	"_so_setsockopt.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL2(_so_setsockopt,setsockopt)
	RET

	SET_SIZE(_so_setsockopt)
