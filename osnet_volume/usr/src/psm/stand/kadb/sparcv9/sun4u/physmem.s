/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)physmem.s	1.4	97/12/03 SMI"

/*
 * Physical memory access for 64-bit sun4u KADB.
 */

#ifdef lint
#include <sys/types.h>
#endif

#include <sys/asm_linkage.h>
#include <sys/spitasi.h>

#ifndef lint
	.section	RODATA
	.align		4

readloops:
	.xword	r1
	.xword	r2
	.xword	r4
	.xword	r8

writeloops:
	.xword	w1
	.xword	w2
	.xword	w4
	.xword	w8
#endif


#ifdef lint
/*ARGSUSED*/
int
physmem_read(unsigned long p, caddr_t c, size_t s) { return 0; }
#else
	ENTRY(physmem_read)

	orcc	    %o2, %o2, %g0	! If caller requested 0 bytes,
	bnz,pt	    %xcc, 1f		! do nothing (successfully)
	nop

	retl
	mov	    0, %o0

1:
	or	    %o0, %o2, %o3
	and	    %o3, 0x7, %o3	! %o3 = (p | s) & 7
	set	    0x9098, %o5		! this literal is an 8-element
	sllx	    %o3,   1, %o3	! array of 2-bit values
	srlx	    %o5, %o3, %o3	! %o3 = %o5[%o3]
	and	    %o3, 0x18, %o3	! The result (%o3) is the power of
					! two for the access size required
					! (1, 2, 4, or 8 bytes), scaled by
					! 8 for indexing (see below).

	setx	    readloops, %o4, %o5	! table of copying loops
	ldx	    [%o5 + %o3], %o3	! %o3 is already scaled
	jmp	    %o3+4		! select a copying loop
	nop				! ... skipping initial "add"


	/*
	 *  Read from physical memory, 1-byte accesses
	 */
r1:	add	    %o1, 1, %o1
	lduba	    [%o0]ASI_MEM, %g1
	subcc	    %o2, 1, %o2
	stb	    %g1, [%o1]
	bnz,a,pt    %xcc, r1
	add	    %o0, 1, %o0

	retl
	mov	    0, %o0


	/*
	 *  Read from physical memory, 2-byte accesses
	 */
r2:	add	    %o1, 2, %o1
	lduha	    [%o0]ASI_MEM, %g1
	subcc	    %o2, 2, %o2
	sth	    %g1, [%o1]
	bnz,a,pt    %xcc, r2
	add	    %o0, 2, %o0

	retl
	mov	    0, %o0


	/*
	 *  Read from physical memory, 4-byte accesses
	 */
r4:	add	    %o1, 4, %o1
	lduwa	    [%o0]ASI_MEM, %g1
	subcc	    %o2, 4, %o2
	stw	    %g1, [%o1]
	bnz,a,pt    %xcc, r4
	add	    %o0, 4, %o0

	retl
	mov	    0, %o0


	/*
	 *  Read from physical memory, 8-byte accesses
	 */
r8:	add	    %o1, 8, %o1
	ldxa	    [%o0]ASI_MEM, %g1
	subcc	    %o2, 8, %o2
	stx	    %g1, [%o1]
	bnz,a,pt    %xcc, r8
	add	    %o0, 8, %o0

	retl
	mov	    0, %o0


	SET_SIZE(physmem_read)
#endif

#ifdef lint
/*ARGSUSED*/
int
physmem_write(unsigned long p, caddr_t c, size_t s) { return 0; }
#else
	ENTRY(physmem_write)

	orcc	    %o2, %o2, %g0	! If caller requested 0 bytes,
	bnz,pt	    %xcc, 1f		! do nothing (successfully)
	nop

	retl
	mov	    0, %o0

1:
	or	    %o0, %o2, %o3
	and	    %o3, 0x7, %o3	! %o3 = (p | s) & 7
	set	    0x9098, %o5		! this literal is an 8-element
	sllx	    %o3,   1, %o3	! array of 2-bit values
	srlx	    %o5, %o3, %o3	! %o3 = %o5[%o3]
	and	    %o3, 0x18, %o3	! The result (%o3) is the power of
					! two for the access size required
					! (1, 2, 4, or 8 bytes), scaled by
					! 8 for indexing (see below).

	setx	    writeloops, %o4, %o5 ! table of copying loops
	lduw	    [%o5 + %o3], %o3	! %o3 is already scaled
	jmp	    %o3+4		! select a copying loop
	nop				! ... skipping initial "add"


	/*
	 *  Write to physical memory, 1-byte accesses
	 */
w1:	add	    %o0, 1, %o0
	ldub	    [%o1], %g1
	subcc	    %o2, 1, %o2
	stba	    %g1, [%o0]ASI_MEM
	bnz,a,pt    %xcc, w1
	add	    %o1, 1, %o1

	retl
	mov	    0, %o0


	/*
	 *  Write to physical memory, 2-byte accesses
	 */
w2:	add	    %o0, 2, %o0
	lduh	    [%o1], %g1
	subcc	    %o2, 2, %o2
	stha	    %g1, [%o0]ASI_MEM
	bnz,a,pt    %xcc, w2
	add	    %o1, 2, %o1

	retl
	mov	    0, %o0


	/*
	 *  Write to physical memory, 4-byte accesses
	 */
w4:	add	    %o0, 4, %o0
	lduw	    [%o1], %g1
	subcc	    %o2, 4, %o2
	stwa	    %g1, [%o0]ASI_MEM
	bnz,a,pt    %xcc, w4
	add	    %o1, 4, %o1

	retl
	mov	    0, %o0


	/*
	 *  Write to physical memory, 8-byte accesses
	 */
w8:	add	    %o0, 8, %o0
	ldx	    [%o1], %g1
	subcc	    %o2, 8, %o2
	stxa	    %g1, [%o0]ASI_MEM
	bnz,a,pt    %xcc, w8
	add	    %o1, 8, %o1

	retl
	mov	    0, %o0


	SET_SIZE(physmem_write)
#endif
