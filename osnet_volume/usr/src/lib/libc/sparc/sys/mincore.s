/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)mincore.s	1.6	92/07/14 SMI"	/* SVr4.0 1.1	*/

/* C library -- mincore						*/
/* int mincore(caddr_t addr, size_t len, char *vec)		*/

	.file	"mincore.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(mincore,function)

#include "SYS.h"

	SYSCALL(mincore)
	RETC

	SET_SIZE(mincore)
