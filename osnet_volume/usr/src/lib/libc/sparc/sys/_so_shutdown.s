/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1994 by Sun Microsystems, Inc.		*/

.ident	"@(#)_so_shutdown.s	1.2	96/06/13 SMI"

/* C library -- _so_shutdown					*/
/* int _so_shutdown (int fildes, int how, int vers);		*/

	.file	"_so_shutdown.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL2(_so_shutdown,shutdown)
	RET

	SET_SIZE(_so_shutdown)
