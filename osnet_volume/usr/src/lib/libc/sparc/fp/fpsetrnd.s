/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)fpsetrnd.s	1.8	92/07/14 SMI"	/* SVr4.0 1.5.1.9	*/

	.file	"fpsetrnd.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpsetround,function)

#include "synonyms.h"

	ENTRY(fpsetround)
	add	%sp, -SA(MINFRAME), %sp	! get an additional word of storage
	set	0xc0000000, %o4		! mask of round control bits
	sll	%o0, 30, %o1		! move input bits into position
	st	%fsr, [%sp+ARGPUSH]	! get fsr value
	ld	[%sp+ARGPUSH], %o0	! load into register
	and	%o1, %o4, %o1		! generate new fsr value
	andn	%o0, %o4, %o2
	or	%o1, %o2, %o1
	st	%o1, [%sp+ARGPUSH]	! move new fsr value to memory
	ld	[%sp+ARGPUSH], %fsr	! load fsr with new value
	srl	%o0, 30, %o0		! return old round control value
	retl
	add	%sp, SA(MINFRAME), %sp	! reclaim stack space

	SET_SIZE(fpsetround)
