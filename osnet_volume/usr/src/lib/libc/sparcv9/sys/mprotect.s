/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)mprotect.s	1.1	96/12/04 SMI"	/* SVr4.0 1.1	*/

/* C library -- mprotect					*/
/* int mprotect(caddr_t addr, size_t len, int prot)		*/

	.file	"mprotect.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(mprotect,function)

#include "SYS.h"

	SYSCALL(mprotect)
	RETC

	SET_SIZE(mprotect)
