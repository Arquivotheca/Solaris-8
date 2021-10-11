/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)llseek.s	1.3	93/08/05 SMI"

/* C library -- llseek						*/
/* offset_t llseek(int fildes, offset_t offset, int whence);	*/

	.file	"llseek.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(llseek,function)

#include "SYS.h"

	SYSCALL64(llseek)
	RET

	SET_SIZE(llseek)
