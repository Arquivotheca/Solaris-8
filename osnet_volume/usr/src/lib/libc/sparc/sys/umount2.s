/*	Copyright (c) 1999 by Sun Microsystems, Inc.		*/
/*	  All Rights Reserved	*/

.ident	"@(#)umount2.s	1.2	99/04/15 SMI"	/* SVr4.0 1.6	*/

/* C library -- umount2						*/
/* int umount2 (const char *file, int flag);			*/

	.file	"umount2.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(umount2,function)

#include "SYS.h"

	SYSCALL(umount2)
	RETC

	SET_SIZE(umount2)
