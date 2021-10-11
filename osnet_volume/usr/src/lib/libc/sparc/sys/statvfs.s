/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)statvfs.s	1.8	96/02/26 SMI"	/* SVr4.0 1.1	*/

/* C library -- statvfs						*/
/* int statvfs(const char *path, struct statvfs *statbuf)	*/

	.file	"statvfs.s"

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(statvfs,function)
#else
	ANSI_PRAGMA_WEAK(statvfs64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	SYSCALL(statvfs)
	RETC

	SET_SIZE(statvfs)

/* C library -- statvfs64					*/
/* int statvfs64(const char *path, struct statvfs64 *statbuf)	*/

#else /* _FILE_OFFSET_BITS == 64 */

	SYSCALL(statvfs64)
	RETC

	SET_SIZE(statvfs64)
	
#endif
