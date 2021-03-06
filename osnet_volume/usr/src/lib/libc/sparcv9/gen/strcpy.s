/*
 * Copyright (c) 1997 Sun Microsystems, Inc.
 */

.ident	"@(#)strcpy.s	1.10	97/02/09 SMI"

	.file	"strcpy.s"

/*
 * strcpy(s1, s2)
 *
 * Copy string s2 to s1.  s1 must be large enough. Return s1.
 *
 * Fast assembler language version of the following C-program strcpy
 * which represents the `standard' for the C-library.
 *
 *	char *
 *	strcpy(s1, s2)
 *	register char *s1;
 *	register const char *s2;
 *	{
 *		char *os1 = s1;
 *	
 *		while(*s1++ = *s2++)
 *			;
 *		return(os1);
 *	}
 *
 * NOTE:  This routine was copied from the kernel routine nstrcpy()
 *        in the kernel file sparc_subr.s.  To keep the look of the code
 *        similar to nstrcpy, the code fragment which calculate the length
 *        of the string has been left in place, though commented out.
 *        In this way the code is the same line for line as in the kernel.
 */

#include <sys/asm_linkage.h>
#include "synonyms.h"

#define	DEST	%i0
#define	SRC	%i1
#define DESTSV	%i2
#define ADDMSK	%l0
#define	ANDMSK	%l1
#define	MSKB0	%l2
#define	MSKB1	%l3
#define	MSKB2	%l4
#define SL	%o0
#define	SR	%o1
#define	MSKB3	0xff

	ENTRY(strcpy)
	save    %sp, -SA(WINDOWSIZE), %sp	! get a new window
	! clr	%l6
	andcc	SRC, 3, %i3		! is src word aligned
	bz,pn	%icc, .aldest
	mov	DEST, DESTSV		! save return value
	cmp	%i3, 2			! is src half-word aligned
	be,pn	%icc, .s2algn
	cmp	%i3, 3			! src is byte aligned
	ldub	[SRC], %i3		! move 1 or 3 bytes to align it
	inc	1, SRC
	stb	%i3, [DEST]		! move a byte to align src
	be,pt	%icc, .s3algn
	tst	%i3
.s1algn:	
	bnz,pt	%icc, .s2algn		! now go align dest
	inc	1, DEST
	b,a	.done
.s2algn:	
	lduh	[SRC], %i3		! know src is 2 byte alinged
	inc	2, SRC
	srl	%i3, 8, %i4
	tst	%i4
	stb	%i4, [DEST]
	bnz,a,pt %icc, 1f
	stb	%i3, [DEST + 1]
	! inc	1, %l6
	b,a	.done
1:	andcc	%i3, MSKB3, %i3
	bnz,pt	%icc, .aldest
	inc	2, DEST
	b,a	.done
.s3algn:	
	bnz,pt	%icc, .aldest
	inc	1, DEST
	b,a	.done

.aldest: 
	set     0x7efefeff, ADDMSK	! masks to test for terminating null
	set     0x81010100, ANDMSK
	sethi	%hi(0xff000000), MSKB0
	sethi	%hi(0x00ff0000), MSKB1

	! source address is now aligned
	andcc	DEST, 3, %i3		! is destination word aligned?
	bz,pn	%icc, .w4str
	srl	MSKB1, 8, MSKB2		! generate 0x0000ff00 mask
	cmp	%i3, 2			! is destination halfword aligned?
	be,pn	%icc, .w2str			
	cmp	%i3, 3			! worst case, dest is byte alinged
.w1str:	
	lduw	[SRC], %i3		! read a word
	inc	4, SRC			! point to next one
	be,pt	%icc, .w3str
	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz,pt	%icc, 1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	b,a	.done1
1:	bnz,pt	%icc, 1f
	andcc 	%i3, MSKB2, %g0		! check if third byte was zero
	b,a	.w1done2
1:	bnz,pn	%icc, 1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero
	b,a	.w1done3
1:	bz,pt	%icc, .w1done4
	srl	%i3, 24, %o3		! move three bytes to align dest
	stb	%o3, [DEST]
	srl	%i3, 8, %o3
	sth	%o3, [DEST + 1]
	inc	3, DEST			! destination now aligned
	mov	%i3, %o3
	mov	24, SL			! case I: XXXB in the first word
	b	8f
	mov	8, SR

2:	inc	4, DEST
8:	sll	%o3, SL, %o2		! save remaining byte

	andcc	%o2, MSKB0, %g0		! check if this byte is a zero. We
					! DON'T want to fetch next word unless
					! we have to.  It could have a segment
					! violation if we are at page boundary.
	bz,pn	%icc, .done1
	nop	
	lduw	[SRC], %o3		! read a word
	inc	4, SRC			! point to next one
	srl	%o3, SR, %i3
	or	%i3, %o2, %i3

	add	%i3, ADDMSK, %l5	! check for a zero byte 
	xor	%l5, %i3, %l5
	and	%l5, ANDMSK, %l5
	cmp	%l5, ANDMSK
	be,a,pt	%icc, 2b		! If eqaul, no zero byte in word, loop.
					! This is a sufficient condition only,
					! NOT a necessary condition.
	st	%i3, [DEST]

	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz,pt	%icc, 1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	b,a	.done1
1:	bnz,pt	%icc, 1f
	andcc 	%i3, MSKB2, %g0		! check if third byte was zero
	b,a	.w1done2

1:	bnz,pn	%icc, 1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero
	b,a	.w1done3
1:	st	%i3, [DEST]		! it is safe to write the word now
	bnz,pn	%icc, 8b		! if not zero, go read another word
	inc	4, DEST			! else finished
	b,a	.done

2:	inc	4, DEST
7:	sll	%o3, SL, %o2		! save remaining byte
	andcc	%o2, MSKB0, %g0		! check if any of the three bytes is
					! zero. We
					! DON'T want to fetch next word unless
					! we have to.  It could have a segment
					! violation if we are at page boundary.
	bnz,pt	%icc, 3f
	andcc	%o2, MSKB1, %g0		! check if second byte was zero
	b,a	.done1
3:	bnz,pt	%icc, 4f
	andcc 	%o2, MSKB2, %g0		! check if third byte was zero
	mov	%o2, %i3		! this is what is expected
	b,a	.w1done2
4:	bz,pn	%icc, .w1done3
	mov	%o2, %i3		! this is what is expected
	lduw	[SRC], %o3		! read a word
	inc	4, SRC			! point to next one
	srl	%o3, SR, %i3
	or	%i3, %o2, %i3

	andcc	%i3, MSKB3, %g0		! check if the new byte is zero
	st	%i3, [DEST]		! it is safe to store
	bnz,pt	%icc, 7b
	inc	4, DEST			! since we have only one byte to check
	b,a	.done			! do it w/o the ADD/XOR/AND magic

.w1done4:
	stb	%i3, [DEST + 3]
	! inc	1, %l6
.w1done3:
	srl	%i3, 8, %o3
	stb	%o3, [DEST + 2]
	! inc	1, %l6
.w1done2:
	srl	%i3, 24, %o3
	stb	%o3, [DEST]
	srl	%i3, 16, %o3
	! inc	2, %l6
	b	.done
	stb	%o3, [DEST + 1]

.w3str:	
	bnz,pt	%icc, 1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	b,a	.done1
1:	bnz,pt	%icc, 1f
	andcc 	%i3, MSKB2, %g0		! check if third byte was zero
	b,a	.w1done2
1:	bnz,pn	%icc, 1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero
	b,a	.w1done3
1:	bz,pt	%icc, .w1done4
	srl	%i3, 24, %o3
	stb	%o3, [DEST]
	inc	1, DEST
	mov	%i3, %o3
	mov	8, SL			! case II: XBBB in the first word
	b	7b
	mov	24, SR			! shift amounts are different

.w2done4:
	srl	%i3, 16, %o3
	sth	%o3, [DEST]
	! inc	4, %l6
	b	.done
	sth	%i3, [DEST + 2]
	
.w2str:	
	lduw	[SRC], %i3		! read a word
	inc	4, SRC			! point to next one
	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz,pt	%icc, 1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	b,a	.done1
1:	bz,pn	%icc, .done2

	srl	%i3, 16, %o3
	sth	%o3, [DEST]
	inc	2, DEST
	b	9f
	mov	%i3, %o3

2:	inc	4, DEST
9:	sll	%o3, 16, %i3		! save rest
	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz,pn	%icc, 1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	b,a	.done1
1:	bnz,a,pt %icc, 1f
	ld	[SRC], %o3		! read a word
	b,a	.done2
1:	inc	4, SRC			! point to next one
	srl	%o3, 16, %o2
	or	%o2, %i3, %i3

	add	%i3, ADDMSK, %l5	! check for a zero byte 
	xor	%l5, %i3, %l5
	and	%l5, ANDMSK, %l5
	cmp	%l5, ANDMSK
	be,a,pt	%icc, 2b		! if no zero byte in word, loop
	st	%i3, [DEST]

	andcc 	%i3, MSKB2, %g0		! check if third byte was zero
	bnz,pt	%icc, 1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero
	b,a	.done3
1:	st	%i3, [DEST]		! it is safe to write the word now
	bnz,pt	%icc, 9b		! if not zero, go read another word
	inc	4, DEST			! else fall through
	b,a	.done

2:	inc	4, DEST
.w4str:	
	lduw	[SRC], %i3		! read a word
	inc	4, %i1			! point to next one

	add	%i3, ADDMSK, %l5	! check for a zero byte 
	xor	%l5, %i3, %l5
	and	%l5, ANDMSK, %l5
	cmp	%l5, ANDMSK
	be,a,pt	%icc, 2b		! if no zero byte in word, loop
	st	%i3, [DEST]

	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz,pt	%icc, 1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	b,a	.done1
1:	bnz,pt	%icc, 1f
	andcc 	%i3, MSKB2, %g0		! check if third byte was zero
	b,a	.done2
1:	bnz,pt	%icc, 1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero
	b,a	.done3
1:	st	%i3, [DEST]		! it is safe to write the word now
	bnz,pt	%icc, .w4str		! if not zero, go read another word
	inc	4, DEST			! else fall through

.done:	
	!sub	DEST, DESTSV, %l0
	!add	%l0, %l6, %l0
	!st	%l0, [LEN]
	ret			! last byte of word was the terminating zero 
	restore	DESTSV, %g0, %o0

.done1:	
	stb	%g0, [DEST]	! first byte of word was the terminating zero
	!sub	DEST, DESTSV, %l0
	!inc	1, %l6
	!add	%l0, %l6, %l0
	!st	%l0, [LEN]
	ret	
	restore	DESTSV, %g0, %o0

.done2:	
	srl	%i3, 16, %i4	! second byte of word was the terminating zero
	sth	%i4, [DEST]
	!sub	DEST, DESTSV, %l0
	!inc	2, %l6
	!add	%l0, %l6, %l0
	!st	%l0, [LEN]
	ret	
	restore	DESTSV, %g0, %o0

.done3:	
	srl	%i3, 16, %i4	! third byte of word was the terminating zero
	sth	%i4, [DEST]
	stb	%g0, [DEST + 2]
	!sub	DEST, DESTSV, %l0
	!inc	3, %l6
	!add	%l0, %l6, %l0
	!st	%l0, [LEN]
	ret	
	restore	DESTSV, %g0, %o0
	SET_SIZE(strcpy)

