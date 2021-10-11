/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)lseek.s	1.9	96/02/26 SMI"	/* SVr4.0 1.8	*/

/* C library -- lseek						*/
/* off_t lseek(int fildes, off_t offset, int whence);		*/

	.file	"lseek.s"

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(lseek,function)
#else
	ANSI_PRAGMA_WEAK(lseek64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	SYSCALL(lseek)
	RET

	SET_SIZE(lseek)

#else
/* C library -- lseek64 transitional large file API		*/
/* off64_t lseek64(int fildes, off64_t offset, int whence);	*/

	ENTRY(lseek64)
	
	SYSTRAP(llseek)
	SYSCERROR64
	RET

	SET_SIZE(lseek64)

#endif
