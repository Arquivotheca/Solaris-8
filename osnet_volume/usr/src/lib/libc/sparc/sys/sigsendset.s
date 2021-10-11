/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)sigsendset.s	1.6	92/07/14 SMI"	/* SVr4.0 1.1	*/

/* C library -- sigsendset					*/
/* int sigsendset(procset_t *psp, int sig)			*/

	.file	"sigsendset.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(sigsendset,function)

#include "SYS.h"

	ENTRY(sigsendset)
	SYSTRAP(sigsendsys)
	SYSCERROR
	RET

	SET_SIZE(sigsendset)
