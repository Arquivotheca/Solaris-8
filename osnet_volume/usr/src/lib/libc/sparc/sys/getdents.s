/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)getdents.s	1.8	96/02/26 SMI"	/* SVr4.0 1.2.1.7	*/

/* C library -- getdents					*/
/* int getdents (int fildes, struct dirent *buf, size_t count)	*/

	.file	"getdents.s"

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(getdents,function)
#else
	ANSI_PRAGMA_WEAK(getdents64,function)
#endif	

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	SYSCALL(getdents)
	RET

	SET_SIZE(getdents)

#else
/* C library -- getdents64					*/
/* int getdents64 (int fildes, struct dirent64 *buf, size_t count)	*/

	SYSCALL(getdents64)
	RET

	SET_SIZE(getdents64)
#endif


