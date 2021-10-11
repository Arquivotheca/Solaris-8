/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)fpgetsticky.s	1.6	92/07/14 SMI"	/* SVr4.0 1.11	*/

	.file	"fpgetsticky.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpgetsticky,function)

#include "synonyms.h"

	ENTRY(fpgetsticky)
	add	%sp, -SA(MINFRAME), %sp	! get an additional word of storage
	set	0x000003e0, %o4		! mask of accrued exception bits
	st	%fsr, [%sp+ARGPUSH]	! get fsr value
	ld	[%sp+ARGPUSH], %o0	! load into register
	and	%o0, %o4, %o0		! mask off bits of interest
	srl	%o0, 5, %o0		! return accrued exception value
	retl
	add	%sp, SA(MINFRAME), %sp	! reclaim stack space

	SET_SIZE(fpgetsticky)
