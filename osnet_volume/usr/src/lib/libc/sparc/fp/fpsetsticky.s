/*	Copyright (c) 1988 AT&T	*/
/*			All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)fpsetsticky.s	1.7	92/07/21 SMI"
		/* SVr4.0 1.4.1.7	*/

	.file	"fpsetsticky.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpsetsticky,function)

#include "synonyms.h"

	ENTRY(fpsetsticky)
	add	%sp, -SA(MINFRAME), %sp	! get an additional word of storage
	set	0x000003e0, %o4		! mask of accrued exception bits
	sll	%o0, 5, %o1		! move input bits into position
	st	%fsr, [%sp+ARGPUSH]	! get fsr value
	ld	[%sp+ARGPUSH], %o0	! load into register
	and	%o1, %o4, %o1		! generate new fsr value
	andn	%o0, %o4, %o2
	or	%o1, %o2, %o1
	st	%o1, [%sp+ARGPUSH]	! move new fsr value to memory
	ld	[%sp+ARGPUSH], %fsr	! load fsr with new value
	and	%o0, %o4, %o0		! mask off bits of interest in old fsr
	srl	%o0, 5, %o0		! return old accrued exception value
	retl
	add	%sp, SA(MINFRAME), %sp	! reclaim stack space

	SET_SIZE(fpsetsticky)
