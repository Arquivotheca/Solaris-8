/*
 *	Copyright (c) 1990, 1991 Sun Microsystems, Inc.
 */

#ident	"@(#)zero.s	1.1	92/03/05 SMI"

/*
 * Routine to zero out a section of memory (the tail end of the data segment
 * upto a page boundary).  First nibble off any bytes up to the first double
 * aligned word, and then clear doubles until the byte count is zero.  Note,
 * this assumes the count specified has been rounded to end on a double word
 * boundary.
 *
 *	%o0 = addr, %o1 = len
 */

	.seg	"text"
	.global	__zero
__zero:
	tst	%o1		! Have we any count at all?
	ba	3f		! Go find out,
	clr	%g1		!   but prepare for long loop

! Byte clearing loop

0:
	inc	%o0		! Bump address
	deccc	%o1		! Decrement count
3:
	bz	1f		! If no count left, exit
	andcc	%o0, 7, %g0	! Is the address less than double-word aligned?
	bnz,a	0b		! Branch if so
	clrb	[%o0]		!   but clear the current byte anyway

! Double clearing loop

2:
	std	%g0, [%o0]	! Clear next 8 bytes
	subcc	%o1, 8, %o1	! Decrement count
	bnz	2b		! Branch if any left
	inc	8, %o0		!   but always increment address by 8
1:
	retl			! Go home
	nop
