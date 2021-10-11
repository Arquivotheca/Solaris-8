/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)statfs.s	1.5	92/07/14 SMI"	/* SVr4.0 1.4	*/

/* C library -- statfs						*/
/* int statfs (const char *path, struct statfs *buf, int len,	*/
/*	       int fstyp);					*/

	.file	"statfs.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(statfs,function)

#include "SYS.h"

	SYSCALL(statfs)
	RETC

	SET_SIZE(statfs)
