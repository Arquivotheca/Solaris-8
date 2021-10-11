/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)alloca.s	1.14	98/01/30 SMI"

	.file	"alloca.s"

#include <sys/asm_linkage.h>
#include <sys/stack.h>

	!
	! o0: # bytes of space to allocate, already rounded to 0 mod 8
	! o1: %sp-relative offset of tmp area
	! o2: %sp-relative offset of end of tmp area
	!
	! we want to bump %sp by the requested size
	! then copy the tmp area to its new home
	! this is necessary as we could theoretically
	! be in the middle of a complicated expression.
	!
	ENTRY(__builtin_alloca)
	add	%sp, STACK_BIAS, %g1	! save current sp + STACK_BIAS
	sub	%sp, %o0, %sp		! bump to new value
	add	%sp, STACK_BIAS, %g5
	! copy loop: should do nothing gracefully
	b	2f
	subcc	%o2, %o1, %o5		! number of bytes to move
1:	
	ldx	[%g1 + %o1], %o4	! load from old temp area
	stx	%o4, [%g5 + %o1]	! store to new temp area
	add	%o1, 8, %o1
2:	bg,pt	%xcc, 1b
	subcc	%o5, 8, %o5
	! now return new %sp + end-of-temp
	add	%sp, %o2, %o0
	retl
	add	%o0, STACK_BIAS, %o0
	SET_SIZE(__builtin_alloca)
