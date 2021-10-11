/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)getrlimit.s	1.8	96/02/26 SMI"	/* SVr4.0 1.1	*/

/* C library -- getrlimit					*/
/* int getrlimit(int resources, struct rlimit *rlp)		*/

	.file	"getrlimit.s"

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(getrlimit,function)
#else
	ANSI_PRAGMA_WEAK(getrlimit64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	SYSCALL(getrlimit)
	RET

	SET_SIZE(getrlimit)

/* C library -- getrlimit64					*/
/* int getrlimit64(int resources, struct rlimit64 *rlp)		*/
	
#else /* _FILE_OFFSET_BITS == 64 */

	SYSCALL(getrlimit64)
	RET

	SET_SIZE(getrlimit64)

#endif
