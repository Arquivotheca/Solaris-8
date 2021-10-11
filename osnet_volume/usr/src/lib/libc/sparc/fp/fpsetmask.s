/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)fpsetmask.s	1.7	92/07/21 SMI"
		/* SVr4.0 1.4.1.9	*/

/*
 * fp_except fpsetmask(mask)
 * 	fp_except mask;
 * set exception masks as defined by user and return
 * previous setting
 * any sticky bit set whose corresponding mask is dis-abled
 * is cleared
 */

	.file	"fpsetmask.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpsetmask,function)

#include "synonyms.h"

	ENTRY(fpsetmask)
	add	%sp, -SA(MINFRAME), %sp	! get an additional word of storage
	set	0x0f800000, %o4		! mask of trap enable bits
	sll	%o0, 23, %o1		! move input bits into position
	st	%fsr, [%sp+ARGPUSH]	! get fsr value
	ld	[%sp+ARGPUSH], %o0	! load into register
	and	%o1, %o4, %o1		! generate new fsr value
	andn	%o0, %o4, %o2
	or	%o1, %o2, %o1
	st	%o1, [%sp+ARGPUSH]	! move new fsr value to memory
	ld	[%sp+ARGPUSH], %fsr	! load fsr with new value
	and	%o0, %o4, %o0		! mask off bits of interest in old fsr
	srl	%o0, 23, %o0		! return old trap enable value
	retl
	add	%sp, SA(MINFRAME), %sp	! reclaim stack space

	SET_SIZE(fpsetmask)
