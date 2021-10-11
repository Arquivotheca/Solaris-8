/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)pause.s	1.7	96/05/02 SMI"	/* SVr4.0 1.8	*/

/* C library - pause						*/
/* int pause (void);						*/

	.file	"pause.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

	.weak	_libc_pause;
	.type	_libc_pause, #function
	_libc_pause = pause

	SYSCALL(pause)
	RET

	SET_SIZE(pause)
