/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)seteguid.s	1.5	92/07/14 SMI"	/* SVr4.0 1.2	*/

/* C library -- setegid, seteuid				*/
/* int setegid (gid_t gid)					*/

	.file	"seteguid.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(setegid,function)
	ANSI_PRAGMA_WEAK(seteuid,function)

#include "SYS.h"

	SYSCALL(setegid)
	RET

	SET_SIZE(setegid)

/* int seteuid (uid_t uid)					*/

	SYSCALL(seteuid)
	RET

	SET_SIZE(seteuid)
