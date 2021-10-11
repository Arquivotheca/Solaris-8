/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)sysfs.s	1.5	92/07/14 SMI"	/* SVr4.0 1.5	*/

/* C library -- sysfs						*/
/* int sysfs (int opcode, const char *fsname);			*/
/* int sysfs (int opcode, int fs_index, char *buf);		*/
/* int sysfs (int opcode);					*/

	.file	"sysfs.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(sysfs,function)

#include "SYS.h"

	SYSCALL(sysfs)
	RET

	SET_SIZE(sysfs)
