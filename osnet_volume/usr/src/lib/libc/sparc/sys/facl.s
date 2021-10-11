/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/
/*	Copyright (c) 1994 by Sun Microsystems, Inc.		*/

.ident	"@(#)facl.s	1.1	94/06/14 SMI"	/* SVr4.0 1.1	*/

/* C library -- facl						*/
/* int facl(int fd, int cmd, int cnt, struct aclent *buf)	*/

	.file	"facl.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(facl,function)

#include "SYS.h"

	SYSCALL(facl)
	RET

	SET_SIZE(facl)
