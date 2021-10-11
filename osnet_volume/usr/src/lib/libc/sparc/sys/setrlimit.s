/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)setrlimit.s	1.8	96/02/26 SMI"	/* SVr4.0 1.1	*/

/* C library -- setrlimit					*/
/* int setrlimit(int resource, const struct rlimit *rlp)	*/

	.file	"setrlimit.s"

#include <sys/asm_linkage.h>

#if (_FILE_OFFSET_BITS != 64)
	ANSI_PRAGMA_WEAK(setrlimit,function)
#else
	ANSI_PRAGMA_WEAK(setrlimit64,function)
#endif

#include "SYS.h"

#if (_FILE_OFFSET_BITS != 64)
	
	SYSCALL(setrlimit)
	RET

	SET_SIZE(setrlimit)

#else /* _FILE_OFFSET_BITS == 64 */

	SYSCALL(setrlimit64)
	RET

	SET_SIZE(setrlimit64)

#endif
