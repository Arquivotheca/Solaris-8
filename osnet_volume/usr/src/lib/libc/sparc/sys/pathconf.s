/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)pathconf.s	1.4	92/07/14 SMI"	/* SVr4.0 1.3	*/

/* C library -- pathconf					*/
/* long pathconf(char *path, int name)				*/

	.file	"pathconf.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(pathconf,function)

#include "SYS.h"

	SYSCALL(pathconf)
	RET

	SET_SIZE(pathconf)
