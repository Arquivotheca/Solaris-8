/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)stat.s	1.7	96/02/26 SMI"	/* SVr4.0 1.8	*/

/* C library -- stat						*/
/* int stat (const char *path, struct stat *buf);		*/

	.file	"stat.s"

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(stat,function)
#else
	ANSI_PRAGMA_WEAK(stat64,function)
#endif
	
#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	SYSCALL(stat)
	RETC

	SET_SIZE(stat)

/* C library -- stat64 - transitional API			*/
/* int stat64 (const char *path, struct stat64 *buf);		*/

#else /* _FILE_OFFSET_BITS == 64 */

	SYSCALL(stat64)
	RET

	SET_SIZE(stat64)
#endif
