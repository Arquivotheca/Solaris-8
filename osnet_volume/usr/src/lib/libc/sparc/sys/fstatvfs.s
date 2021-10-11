/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)fstatvfs.s	1.8	96/02/26 SMI"	/* SVr4.0 1.1	*/

/* C library -- fstatvfs					*/
/* int fstatvfs(int fildes, struct statvfs *buf)		*/

	.file	"fstatvfs.s"

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(fstatvfs,function)
#else
	ANSI_PRAGMA_WEAK(fstatvfs64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	SYSCALL(fstatvfs)
	RETC

	SET_SIZE(fstatvfs)

/* C library -- fstatvfs64					*/
/* int fstatvfs64(int fildes, struct statvfs64 *buf)		*/

#else /* _FILE_OFFSET_BITS == 64 */

	SYSCALL(fstatvfs64)
	RETC

	SET_SIZE(fstatvfs64)
	
#endif
