/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)syssun.s	1.5	92/07/14 SMI"	/* SVr4.0 1.3.1.5	*/

/* C library -- syssun (from sys3b2)				*/
/* int syssun(cmd, ... );					*/

	.file	"syssun.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(syssun,function)

#include "SYS.h"

	SYSCALL(syssun)
	RET

	SET_SIZE(syssun)
