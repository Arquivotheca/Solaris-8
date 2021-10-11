/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)lsub.s	1.10	98/01/29 SMI"	/* SVr4.0 1.1	*/

/*
 * Double long subtraction routine.  Ported from pdp 11/70 version
 * with considerable effort.  All supplied comments were ported.
 * Ported from m32 version to sparc. No comments about difficulty.
 *
 *	dl_t
 *	lsub (lop, rop)
 *		dl_t	lop;
 *		dl_t	rop;
 */

	.file	"lsub.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lsub,function)

#include "synonyms.h"

	ENTRY(lsub)

	ld	[%o7+8],%o4		! Instruction at ret-addr should be a
	cmp     %o4,8			! 'unimp 8' indicating a valid call.
	be	1f			! if OK, go forward.
	nop				! delay instruction.
	jmp	%o7+8			! return
	nop				! delay instruction.

1:
	ld	[%o0+0],%o2		! fetch lop.dl_hop
	ld	[%o0+4],%o3		! fetch lop.dl_lop
	ld	[%o1+0],%o4		! fetch rop.dl_hop
	ld	[%o1+4],%o5		! fetch rop.dl_lop
	subcc	%o3,%o5,%o3		! lop.dl_lop - rop.dl_lop (set carry)
	subxcc	%o2,%o4,%o2		! lop.dl_hop - rop.dl_hop - <carry>
	ld	[%sp+(16*4)],%o0	! address to store result into
	st	%o2,[%o0]		! store result.dl_hop
	jmp	%o7+12			! return
	st	%o3,[%o0+4]		! store result.dl_lop

	SET_SIZE(lsub)
