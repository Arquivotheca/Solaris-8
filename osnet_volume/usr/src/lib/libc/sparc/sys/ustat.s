/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)ustat.s	1.7	92/07/14 SMI"	/* SVr4.0 1.6	*/

/* C library -- ustat						*/
/* int ustat (dev_t dev, struct ustat *buf);			*/

	.file	"ustat.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(ustat,function)

#include "SYS.h"

#define SUBSYS_ustat	2

	ENTRY(ustat)
	mov	%o0, %o2	! re-arrange argument order for utssys
	mov	%o1, %o0
	mov	%o2, %o1
	mov	SUBSYS_ustat, %o2	! 3rd param is the type
	SYSTRAP(utssys)
	SYSCERROR
	RETC

	SET_SIZE(ustat)
