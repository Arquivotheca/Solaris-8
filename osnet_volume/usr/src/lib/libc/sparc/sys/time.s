/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)time.s	1.6	92/07/14 SMI"	/* SVr4.0 1.5	*/

/* C library -- time						*/
/* time_t time (time_t *tloc);					*/

	.file	"time.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(time,function)

#include "SYS.h"

	ENTRY(time)
	mov	%o0, %o2	! save pointer
	SYSTRAP(time)
	tst	%o2		! pointer is non-null?
	bnz,a	1f
	st	%o0, [%o2]
1:
	RET

	SET_SIZE(time)
