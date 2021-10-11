/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)stty.s	1.5	92/07/14 SMI"	/* SVr4.0 1.6	*/

/* C library -- stty						*/
/* int stty (char *path, struct ?? buf, int len);		*/

	.file	"stty.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(stty,function)

#include "SYS.h"

	SYSCALL(stty)
	RET

	SET_SIZE(stty)
