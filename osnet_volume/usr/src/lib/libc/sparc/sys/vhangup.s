/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)vhangup.s	1.2	92/07/14 SMI"	/* SVr4.0 1.6	*/

/* C library -- vhangup						*/
/* int vhangup()						*/
 
	.file	"vhangup.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(vhangup,function)

#include "SYS.h"

	SYSCALL(vhangup)
	RET

	SET_SIZE(vhangup)

