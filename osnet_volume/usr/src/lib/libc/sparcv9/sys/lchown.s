/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)lchown.s	1.1	96/12/04 SMI"	/* SVr4.0 1.1	*/

/* C library -- lchown						*/
/* int lchown(const char *path, uid_t owner, gid_t group)	*/

	.file	"lchown.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lchown,function)

#include "SYS.h"

	SYSCALL(lchown)
	RETC

	SET_SIZE(lchown)
