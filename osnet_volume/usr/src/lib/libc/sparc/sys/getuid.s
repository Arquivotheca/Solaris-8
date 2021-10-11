/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)getuid.s	1.5	92/07/14 SMI"	/* SVr4.0 1.7	*/

/* C library -- getuid						*/
/* uid_t getuid (void);						*/

	.file	"getuid.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getuid,function)

#include "SYS.h"

	SYSCALL_NOERROR(getuid)
	RET

	SET_SIZE(getuid)
