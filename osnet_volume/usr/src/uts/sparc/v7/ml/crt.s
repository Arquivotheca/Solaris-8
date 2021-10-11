/*
 * Copyright (c) 1988-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)crt.s	1.5	99/10/04 SMI"	/* From SunOS 4.1 1.6 */

#if defined(lint)
#include <sys/types.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#endif  /* lint */

#include <sys/asm_linkage.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/simulate.h>

/*
 * C run time subroutines.
 *
 *	Those beginning in `.' are not callable from C and hence do not
 *	get lint prototypes.
 */

#if !defined(lint)

/*
 * Structure return
 */
#define UNIMP		0
#define MASK		0x00000fff
#define STRUCT_VAL_OFF	(16*4)

	RTENTRY(.stret4)
	RTENTRY(.stret8)
	!
	! see if key matches: if not, structure value not expected,
	! so just return
	!
	ld	[%i7 + 8], %o3
	and	%o1, MASK, %o4
	sethi	%hi(UNIMP), %o5
	or	%o4, %o5, %o5
	cmp	%o5, %o3
	be,a	0f
	ld	[%fp + STRUCT_VAL_OFF], %i0	! set expected return value
	ret
	restore
0:						! copy the struct
	subcc	%o1, 4, %o1
	ld	[%o0 + %o1], %o4
	bg	0b
	st	%o4, [%i0 + %o1]		! delay slot
	add	%i7, 0x4, %i7			! bump return address
	ret
	restore
	SET_SIZE(.stret4)
	SET_SIZE(.stret8)

	RTENTRY(.stret2)
	!
	! see if key matches: if not, structure value not expected,
	! so just return
	!
	ld	[%i7 + 8], %o3
	and	%o1, MASK, %o4
	sethi	%hi(UNIMP), %o5
	or	%o4, %o5, %o5
	cmp	%o5, %o3
	be,a	0f
	ld	[%fp + STRUCT_VAL_OFF], %i0	! set expected return value
	ret
	restore
0:						! copy the struct
	subcc	%o1, 2, %o1
	lduh	[%o0 + %o1], %o4
	bg	0b
	sth	%o4, [%i0 + %o1]		! delay slot
	add	%i7, 0x4, %i7			! bump return address
	ret
	restore
	SET_SIZE(.stret2)

	!  Hand coded 64-bit multiply routines that depend on V8 instructions

	RTENTRY(__mul64)
	ALTENTRY(__umul64)
	orcc	%o0, %o2, %g0	! short-cut: can we use 32x32 multiply?
	bnz	1f		! if not, go compute all three partial products
	nop
	umul	%o1, %o3, %o1
	retl
	rd	%y, %o0
1:
	umul	%o3, %o0, %o0
	umul	%o1, %o2, %o2
	umul	%o1, %o3, %o1
	add	%o0, %o2, %o0
	rd	%y, %o3
	retl
	add	%o0, %o3, %o0
	SET_SIZE(__mul64)
	SET_SIZE(__umul64)

#endif /* !lint */
