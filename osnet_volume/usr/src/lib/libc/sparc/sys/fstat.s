/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)fstat.s	1.7	96/02/26 SMI"	/* SVr4.0 1.8	*/

/* C library -- fstat						*/
/* int fstat (int fildes, struct stat *buf)			*/

	.file	"fstat.s"

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(fstat,function)
#else
	ANSI_PRAGMA_WEAK(fstat64,function)
#endif
	

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	SYSCALL(fstat)
	RETC

	SET_SIZE(fstat)
#else
/* C library -- fstat64 transitional large file API		*/
/* int fstat64 (int fildes, struct stat64 *buf)			*/

	SYSCALL(fstat64)
	RETC

	SET_SIZE(fstat64)

#endif
