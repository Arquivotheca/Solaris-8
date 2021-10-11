/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)lstat.s	1.8	96/02/26 SMI"	/* SVr4.0 1.1	*/

/* C library -- lstat						*/
/* error = lstat(const char *path, struct lstat *buf)		*/

	.file	"lstat.s"

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(lstat,function)
#else
	ANSI_PRAGMA_WEAK(lstat64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	SYSCALL(lstat)
	RETC

	SET_SIZE(lstat)

#else
/* C library -- lstat64 - transitional large file API		*/
/* error = lstat64(const char *path, struct stat64 *buf)	*/
	
	SYSCALL(lstat64)
	RETC

	SET_SIZE(lstat64)

#endif
