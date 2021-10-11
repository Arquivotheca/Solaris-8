/*      Copyright (c) 1996 by Sun Microsystems, Inc.            */

.ident	"@(#)__fcntl.s 1.2	97/01/09 SMI"


/* C library -- fcntl						*/
/* int fcntl (int fildes, int cmd [, arg])			*/

	.file	"__fcntl.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	SYSCALL2_RESTART(__fcntl,fcntl)
	RET
	
	SET_SIZE(__fcntl)
