/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1994 by Sun Microsystems, Inc.		*/

.ident	"@(#)_so_socketpair.s	1.2	96/06/13 SMI"

/* C library -- _so_socketpair					*/
/*
 * int _so_socketpair (int sv[2]);
 */

	.file	"_so_socketpair.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL2(_so_socketpair,so_socketpair)
	RET

	SET_SIZE(_so_socketpair)
