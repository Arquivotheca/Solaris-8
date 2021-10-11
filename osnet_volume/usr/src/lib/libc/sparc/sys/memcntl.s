/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)memcntl.s	1.6	92/07/14 SMI"	/* SVr4.0 1.3	*/

/* C library -- memcntl	*/
/* int memcntl(caddr_t addr, size_t len, int cmd,
	       int attr, int arg)				*/

	.file	"memcntl.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(memcntl,function)

#include "SYS.h"

	SYSCALL(memcntl)
	RETC

	SET_SIZE(memcntl)
