/*
 * Copyright (c) 1997 Sun Microsystems, Inc.
 */

.ident	"@(#)strlen.s	1.8	97/02/09 SMI"

	.file	"strlen.s"

/*
 * strlen(s)
 *
 * Given string s, return length (not including the terminating null).
 *	
 * Fast assembler language version of the following C-program strlen
 * which represents the `standard' for the C-library.
 *
 *	size_t
 *	strlen(s)
 *	register const char *s;
 *	{
 *		register const char *s0 = s + 1;
 *	
 *		while (*s++ != '\0')
 *			;
 *		return (s - s0);
 *	}
 */

#include <sys/asm_linkage.h>
#include "synonyms.h"

	ENTRY(strlen)
	mov	%o0, %o1
	andcc	%o1, 3, %o3		! is src word aligned
	bz,pn	%icc, .nowalgnd
	clr	%o0			! length of non-zero bytes
	cmp	%o3, 2			! is src half-word aligned
	be,pn	%icc, .s2algn
	cmp	%o3, 3			! src is byte aligned
	ldub	[%o1], %o3		! move 1 or 3 bytes to align it
	inc	1, %o1			! in either case, safe to do a byte
	be,pt	%icc, .s3algn
	tst	%o3
.s1algn:bnz,a	%xcc, .s2algn		! now go align dest
	inc	1, %o0
	b,a	.done

.s2algn:lduh	[%o1], %o3		! know src is half-byte aligned
	inc	2, %o1
	srl	%o3, 8, %o4
	brnz,a	%o4, 1f			! is the first byte zero
	inc	%o0
	b,a	.done
1:	andcc	%o3, 0xff, %o3		! is the second byte zero
	bnz,a	%icc, .nowalgnd
	inc	%o0
	b,a	.done
.s3algn:bnz,a	%icc, .nowalgnd
	inc	1, %o0
	b,a	.done

.nowalgnd:
	! Use trick to check if any read bytes of a word are zero.
	! The following two constants will generate "byte carries"
	! and check if any bit in a byte is set.  If all characters
	! are 7 bits (unsigned) this trick always works.  Otherwise,
	! one might get a "found a zero" condition on the word, but
	! none of the bytes is actually zero.  See below.

	set	0x7efefeff, %o3
	set	0x81010100, %o4

3:	lduw	[%o1], %o2		! main loop
	inc	4, %o1
	add	%o2, %o3, %o5		! generate byte-carries
	xor	%o5, %o2, %o5		! see if orignal bits set
	and	%o5, %o4, %o5
	cmp	%o5, %o4		! if ==,  no zero bytes
	be,a	%icc, 3b
	inc	4, %o0

	! Check for the zero byte and increment the count appropriately.
	! If bit 31 was set, there may not be a zero byte; then we must go
	! test the next four bytes by returning to the main loop.

	sethi	%hi(0xff000000), %o5	! mask used to test for terminator
	andcc	%o2, %o5, %g0		! check if first byte was zero
	bnz,pt	%icc, 1f
	srl	%o5, 8, %o5
.done:	retl
	nop
1:	andcc	%o2, %o5, %g0		! check if second byte was zero
	bnz,pt	%icc, 1f
	srl	%o5, 8, %o5
.done1:	retl
	inc	%o0
1:	andcc 	%o2, %o5, %g0		! check if third byte was zero
	bnz,pt	%icc, 1f
	andcc	%o2, 0xff, %g0		! check if last byte is zero
.done2:	retl
	inc	2, %o0
1:	bnz,a,pn %icc, 3b
	inc	4, %o0			! count of bytes
.done3:	retl
	inc	3, %o0

	SET_SIZE(strlen)
