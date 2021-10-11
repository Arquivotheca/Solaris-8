/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)fpgetmask.s	1.6	92/07/14 SMI"	/* SVr4.0 1.11	*/

	.file	"fpgetmask.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpgetmask,function)

#include "synonyms.h"

	ENTRY(fpgetmask)
	add	%sp, -SA(MINFRAME), %sp	! get an additional word of storage
	set	0x0f800000, %o4		! mask of trap enable bits
	st	%fsr, [%sp+ARGPUSH]	! get fsr value
	ld	[%sp+ARGPUSH], %o0	! load into register
	and	%o0, %o4, %o0		! mask off bits of interest
	srl	%o0, 23, %o0		! return trap enable value
	retl
	add	%sp, SA(MINFRAME), %sp	! reclaim stack space

	SET_SIZE(fpgetmask)
