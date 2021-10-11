/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)pipe.s	1.12	92/07/14 SMI"	/* SVr4.0 1.8	*/

/* C library -- pipe						*/
/* int pipe (int fildes[2]);					*/

	.file	"pipe.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(pipe,function)

#include "SYS.h"

	ENTRY(pipe)
	mov	%o0, %o2		/* save ptr to array	*/
	SYSTRAP(pipe)
	SYSCERROR
	st	%o0, [%o2]
	st	%o1, [%o2 + 4]
	RETC

	SET_SIZE(pipe)
