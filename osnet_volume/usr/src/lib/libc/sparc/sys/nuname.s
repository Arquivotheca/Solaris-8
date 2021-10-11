/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)nuname.s	1.6	92/07/14 SMI"	/* SVr4.0 1.1	*/

/* C library -- nuname						*/
/* int  nuname(struct utsname *name)				*/

	.file	"nuname.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(nuname,function)

#include "SYS.h"

	ENTRY(nuname)	/* Try to explain this name madness!	*/
	SYSTRAP(uname)
	SYSCERROR
	RET

	SET_SIZE(nuname)
