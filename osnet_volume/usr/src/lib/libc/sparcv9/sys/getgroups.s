/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)getgroups.s	1.1	96/12/04 SMI"	/* SVr4.0 1.1	*/

/* C library -- getgroups					*/
/* int getgroups(int gidsetsize, uid_t grouplist[]);		*/

	.file	"getgroups.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getgroups,function)

#include "SYS.h"

	SYSCALL(getgroups)
	RET

	SET_SIZE(getgroups)
