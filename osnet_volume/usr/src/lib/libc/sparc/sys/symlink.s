/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)symlink.s	1.6	92/07/14 SMI"	/* SVr4.0 1.1	*/

/* C library -- symlink						*/
/* int symlink(const char *name1, const char *name2)		*/

	.file	"symlink.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(symlink,function)

#include "SYS.h"

	SYSCALL(symlink)
	RETC

	SET_SIZE(symlink)
