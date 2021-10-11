/*
 * Copyright (c) 1987-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)copy.s	1.47	99/07/27 SMI"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/asm_linkage.h>
#include <sys/vtrace.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/machthread.h>
#include <sys/bcopy_if.h>

#if !defined(lint)
#include "assym.h"
#endif	/* lint */

/*
 * limit use of the bcopy buffer to transfers of at least this size
 * if the tranfer isn't at least two cache lines in size, forget it
 */
#define	BCOPY_LIMIT     0x40

/* 
 * srmmu uses a different define for probing the mmu.  Set the correct
 * define in the case of the srmmu.
 */
#ifdef SRMMU
#undef	ASI_PM
#define	ASI_PM		ASI_FLPR
#endif SRMMU

#ifdef TRACE

	.section	".text"
TR_bcopy_start:
	.asciz "bcopy_start:caller %K src %x dest %x size %d"
	.align 4
TR_kcopy_start:
	.asciz "kcopy_start:caller %K src %x dest %x size %d"
	.align 4
TR_pgcopy_start:
	.asciz "pgcopy_start:caller %K src %x dest %x size %d"
	.align 4
TR_copyout_start:
	.asciz "copyout_start:caller %K src %x dest %x size %d"
	.align 4
TR_copyin_start:
	.asciz "copyin_start:caller %K src %x dest %x size %d"
	.align 4
TR_copyout_noerr_start:
	.asciz "copyout_noerr_start:caller %K src %x dest %x size %d"
	.align 4
TR_copyin_noerr_start:
	.asciz "copyin_noerr_start:caller %K src %x dest %x size %d"
	.align 4


TR_copy_end:
	.asciz "copy_end"
	.align 4

TR_copy_fault:
	.asciz "copy_fault"
	.align 4
TR_copyout_fault:
	.asciz "copyout_fault"
	.align 4
TR_copyin_fault:
	.asciz "copyin_fault"
	.align 4

#define	TRACE_IN(a, b)		\
	save	%sp, -SA(MINFRAME), %sp;	\
	mov	%i0, %l0;	\
	mov	%i1, %l1;	\
	mov	%i2, %l2;	\
	TRACE_ASM_4(%o5, TR_FAC_BCOPY, a, b, %i7, %l0, %l1, %l2);	\
	mov	%l0, %i0;	\
	mov	%l1, %i1;	\
	mov	%l2, %i2;	\
	restore %g0, 0, %g0;

#else	/* TRACE */

#define TRACE_IN(a, b)

#endif	/* TRACE */

/* #define DEBUG */

/*
 * Copy a block of storage, returning an error code if `from' or
 * `to' takes a kernel pagefault which cannot be resolved.
 * Returns errno value on pagefault error, 0 if all ok
 */

#if defined(lint)

/* ARGSUSED */
int
kcopy(const void *from, void *to, size_t count)
{ return(0); }

#else	/* lint */

	.seg	".text"
	.align	4

	ENTRY(kcopy)
#ifdef DEBUG
 
        sethi   %hi(KERNELBASE), %o3
        cmp     %o0, %o3
        blu     1f
        nop
 
        cmp     %o1, %o3
        bgeu     3f
        nop
 
1:
        set     2f, %o0
        call    panic
        nop
 
2:      .asciz  "kcopy: arguments below kernelbase"
        .align 4
 
3:
#endif DEBUG

	TRACE_IN(TR_KCOPY_START, TR_kcopy_start)

	sethi	%hi(.copyerr), %o3	! copyerr is lofault value
	b	.do_copy		! common code
	or	%o3, %lo(.copyerr), %o3

/*
 * We got here because of a fault during kcopy.
 * Errno value is in %g1.
 */
.copyerr:
	TRACE_ASM_0 (%o2, TR_FAC_BCOPY, TR_COPY_FAULT, TR_copy_fault);

	st	%l6, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	ret
	restore	%g1, 0, %o0
	SET_SIZE(kcopy)

#endif	/* lint */

/*
 * Copy a block of storage - must not overlap (from + len <= to).
 * Registers: l6 - saved t_lofault
 */

#if defined(lint)

/* ARGSUSED */
void
bcopy_asm(const void *from, void *to, size_t count)
{}

#else	/* lint */

	ENTRY(bcopy_asm)

	TRACE_IN(TR_BCOPY_START, TR_bcopy_start)
#ifdef DEBUG
	tst	%o2
	bz	3f
	nop
 
        sethi   %hi(KERNELBASE), %o3
        cmp     %o0, %o3
        blu     1f
        nop
 
        cmp     %o1, %o3
        bgeu     3f
        nop
 
1:
        set     2f, %o0
        call    panic
        nop

2:      .asciz  "bcopy: arguments below kernelbase"
        .align 4
 
3:
#endif DEBUG


	ld	[THREAD_REG + T_LOFAULT], %o3
.do_copy:
	save	%sp, -SA(MINFRAME), %sp	! get another window
	ld	[THREAD_REG + T_LOFAULT], %l6	! save t_lofault
.bcopy_cmn:
	cmp	%i2, 8			! for small counts
	bl	.bytecp			! just copy bytes
	st	%i3, [THREAD_REG + T_LOFAULT]	! install new vector
	!
	! use aligned transfers where possible
	!
.bcopy_obmem:
	xor	%i0, %i1, %o4		! xor from and to address
	btst	7, %o4			! if lower three bits zero
	bz	.aldoubcp		! can align on double boundary
	.empty	! assembler complaints about label
.bcopy_words:
	xor	%i0, %i1, %o4		! xor from and to address
	btst	3, %o4			! if lower two bits zero
	bz	.alwordcp		! can align on word boundary
	btst	3, %i0			! delay slot, from address unaligned?
	!
	! use aligned reads and writes where possible
	! this differs from wordcp in that it copes
	! with odd alignment between source and destnation
	! using word reads and writes with the proper shifts
	! in between to align transfers to and from memory
	! i0 - src address, i1 - dest address, i2 - count
	! i3, i4 - tmps for used generating complete word
	! i5 (word to write)
	! l0 size in bits of upper part of source word (US)
	! l1 size in bits of lower part of source word (LS = 32 - US)
	! l2 size in bits of upper part of destination word (UD)
	! l3 size in bits of lower part of destination word (LD = 32 - UD)
	! l4 number of bytes leftover after aligned transfers complete
	! l5 the number 32
	!
	mov	32, %l5			! load an oft-needed constant
	bz	.align_dst_only
	btst	3, %i1			! is destnation address aligned?
	clr	%i4			! clear registers used in either case
	bz	.align_src_only
	clr	%l0
	!
	! both source and destination addresses are unaligned
	!
1:					! align source
	ldub	[%i0], %i3		! read a byte from source address
	add	%i0, 1, %i0		! increment source address
	or	%i4, %i3, %i4		! or in with previous bytes (if any)
	btst	3, %i0			! is source aligned?
	add	%l0, 8, %l0		! increment size of upper source (US)
	bnz,a	1b
	sll	%i4, 8, %i4		! make room for next byte

	sub	%l5, %l0, %l1		! generate shift left count (LS)
	sll	%i4, %l1, %i4		! prepare to get rest
	ld	[%i0], %i3		! read a word
	add	%i0, 4, %i0		! increment source address
	srl	%i3, %l0, %i5		! upper src bits into lower dst bits
	or	%i4, %i5, %i5		! merge
	mov	24, %l3			! align destination
1:
	srl	%i5, %l3, %i4		! prepare to write a single byte
	stb	%i4, [%i1]		! write a byte
	add	%i1, 1, %i1		! increment destination address
	sub	%i2, 1, %i2		! decrement count
	btst	3, %i1			! is destination aligned?
	bnz,a	1b
	sub	%l3, 8, %l3		! delay slot, decrement shift count (LD)
	sub	%l5, %l3, %l2		! generate shift left count (UD)
	sll	%i5, %l2, %i5		! move leftover into upper bytes
	cmp	%l2, %l0		! cmp # req'd to fill dst w old src left
	bg	.more_needed		! need more to fill than we have
	nop

	sll	%i3, %l1, %i3		! clear upper used byte(s)
	srl	%i3, %l1, %i3
	! get the odd bytes between alignments
	sub	%l0, %l2, %l0		! regenerate shift count
	sub	%l5, %l0, %l1		! generate new shift left count (LS)
	and	%i2, 3, %l4		! must do remaining bytes if count%4 > 0
	andn	%i2, 3, %i2		! # of aligned bytes that can be moved
	srl	%i3, %l0, %i4
	or	%i5, %i4, %i5
	st	%i5, [%i1]		! write a word
	subcc	%i2, 4, %i2		! decrement count
	bz	.unalign_out
	add	%i1, 4, %i1		! increment destination address

	b	2f
	sll	%i3, %l1, %i5		! get leftover into upper bits
.more_needed:
	sll	%i3, %l0, %i3		! save remaining byte(s)
	srl	%i3, %l0, %i3
	sub	%l2, %l0, %l1		! regenerate shift count
	sub	%l5, %l1, %l0		! generate new shift left count
	sll	%i3, %l1, %i4		! move to fill empty space
	b	3f
	or	%i5, %i4, %i5		! merge to complete word
	!
	! the source address is aligned and destination is not
	!
.align_dst_only:
	ld	[%i0], %i4		! read a word
	add	%i0, 4, %i0		! increment source address
	mov	24, %l0			! initial shift alignment count
1:
	srl	%i4, %l0, %i3		! prepare to write a single byte
	stb	%i3, [%i1]		! write a byte
	add	%i1, 1, %i1		! increment destination address
	sub	%i2, 1, %i2		! decrement count
	btst	3, %i1			! is destination aligned?
	bnz,a	1b
	sub	%l0, 8, %l0		! delay slot, decrement shift count
.xfer:
	sub	%l5, %l0, %l1		! generate shift left count
	sll	%i4, %l1, %i5		! get leftover
3:
	and	%i2, 3, %l4		! must do remaining bytes if count%4 > 0
	andn	%i2, 3, %i2		! # of aligned bytes that can be moved
2:
	ld	[%i0], %i3		! read a source word
	add	%i0, 4, %i0		! increment source address
	srl	%i3, %l0, %i4		! upper src bits into lower dst bits
	or	%i5, %i4, %i5		! merge with upper dest bits (leftover)
	st	%i5, [%i1]		! write a destination word
	subcc	%i2, 4, %i2		! decrement count
	bz	.unalign_out		! check if done
	add	%i1, 4, %i1		! increment destination address
	b	2b			! loop
	sll	%i3, %l1, %i5		! get leftover
.unalign_out:
	tst	%l4			! any bytes leftover?
	bz	.cpdone
	.empty				! allow next instruction in delay slot
1:
	sub	%l0, 8, %l0		! decrement shift
	srl	%i3, %l0, %i4		! upper src byte into lower dst byte
	stb	%i4, [%i1]		! write a byte
	subcc	%l4, 1, %l4		! decrement count
	bz	.cpdone			! done?
	add	%i1, 1, %i1		! increment destination
	tst	%l0			! any more previously read bytes
	bnz	1b			! we have leftover bytes
	mov	%l4, %i2		! delay slot, mv cnt where dbytecp wants
	b	.dbytecp		! let dbytecp do the rest
	sub	%i0, %i1, %i0		! i0 gets the difference of src and dst
	!
	! the destination address is aligned and the source is not
	!
.align_src_only:
	ldub	[%i0], %i3		! read a byte from source address
	add	%i0, 1, %i0		! increment source address
	or	%i4, %i3, %i4		! or in with previous bytes (if any)
	btst	3, %i0			! is source aligned?
	add	%l0, 8, %l0		! increment shift count (US)
	bnz,a	.align_src_only
	sll	%i4, 8, %i4		! make room for next byte
	b,a	.xfer
	!
	! if from address unaligned for double-word moves,
	! move bytes till it is, if count is < 56 it could take
	! longer to align the thing than to do the transfer
	! in word size chunks right away
	!
.aldoubcp:
	cmp	%i2, 32			! if count < 32, use wordcp, it takes
	bl,a	.alwordcp		! longer to align doubles than words
	mov	3, %o0			! mask for word alignment
	call	.alignit		! copy bytes until aligned
	mov	7, %o0			! mask for double alignment

	!
	! source and destination are now double-word aligned
	! see if transfer is large enough to gain by loop unrolling
	!
	cmp	%i2, 256		! if less than 256 bytes
	bge,a	.blkcopy		! just copy double-words (overwrite i3)
	mov	0x100, %i3		! blk copy chunk size for unrolled loop
	!
	! i3 has aligned count returned by alignit
	!
	and	%i2, 7, %i2		! unaligned leftover count
	sub	%i0, %i1, %i0		! i0 gets the difference of src and dst
5:
	ldd	[%i0+%i1], %o4		! read from address
	std	%o4, [%i1]		! write at destination address
	subcc	%i3, 8, %i3		! dec count
	bg	5b
	add	%i1, 8, %i1		! delay slot, inc to address
.wcpchk:
	cmp	%i2, 4			! see if we can copy a word
	bl	.dbytecp		! if 3 or less bytes use bytecp
	.empty
	!
	! for leftover bytes we fall into wordcp, if needed
	!
.wordcp:
	and	%i2, 3, %i2		! unaligned leftover count
5:
	ld	[%i0+%i1], %o4		! read from address
	st	%o4, [%i1]		! write at destination address
	subcc	%i3, 4, %i3		! dec count
	bg	5b
	add	%i1, 4, %i1		! delay slot, inc to address
	b,a	.dbytecp

	! we come here to align copies on word boundaries
.alwordcp:
	call	.alignit		! go word-align it
	mov	3, %o0			! bits that must be zero to be aligned
	b	.wordcp
	sub	%i0, %i1, %i0		! i0 gets the difference of src and dst

	!
	! byte copy, works with any alignment
	!
.bytecp:
	b	.dbytecp
	sub	%i0, %i1, %i0		! i0 gets difference of src and dst

	!
	! differenced byte copy, works with any alignment
	! assumes dest in %i1 and (source - dest) in %i0
	!
1:
	stb	%o4, [%i1]		! write to address
	inc	%i1			! inc to address
.dbytecp:
	deccc	%i2			! dec count
	bge,a	1b			! loop till done
	ldub	[%i0+%i1], %o4		! read from address
.cpdone:
	st	%l6, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	TRACE_ASM_0 (%o2, TR_FAC_BCOPY, TR_COPY_END, TR_copy_end)
	ret
	restore %g0, 0, %o0		! return (0)

/*
 * Common code used to align transfers on word and doubleword
 * boudaries.  Aligns source and destination and returns a count
 * of aligned bytes to transfer in %i3
 */
1:
	inc	%i0			! inc from
	stb	%o4, [%i1]		! write a byte
	inc	%i1			! inc to
	dec	%i2			! dec count
.alignit:
	btst	%o0, %i0		! %o0 is bit mask to check for alignment
	bnz,a	1b
	ldub	[%i0], %o4		! read next byte

	retl
	andn	%i2, %o0, %i3		! return size of aligned bytes

	!
	! loops have been unrolled so that 64 instructions (16 cache-lines)
	! are used; 256 bytes are moved each time through the loop
	! i0 - from; i1 - to; i2 - count; i3 - chunksize; o4,o5 -tmp
	!
	! We read a whole cache line and then we write it to
	! minimize thrashing.
	!

.blkcopy:
	ldd	[%i0+0xf8], %l0		! 0xfc
	ldd	[%i0+0xf0], %l2
	std	%l0, [%i1+0xf8]
	std	%l2, [%i1+0xf0]

	ldd	[%i0+0xe8], %l0		! 0xec
	ldd	[%i0+0xe0], %l2
	std	%l0, [%i1+0xe8]
	std	%l2, [%i1+0xe0]

	ldd	[%i0+0xd8], %l0		! 0xdc
	ldd	[%i0+0xd0], %l2
	std	%l0, [%i1+0xd8]
	std	%l2, [%i1+0xd0]

	ldd	[%i0+0xc8], %l0		! 0xcc
	ldd	[%i0+0xc0], %l2
	std	%l0, [%i1+0xc8]
	std	%l2, [%i1+0xc0]

	ldd	[%i0+0xb8], %l0		! 0xbc
	ldd	[%i0+0xb0], %l2
	std	%l0, [%i1+0xb8]
	std	%l2, [%i1+0xb0]

	ldd	[%i0+0xa8], %l0		! 0xac
	ldd	[%i0+0xa0], %l2
	std	%l0, [%i1+0xa8]
	std	%l2, [%i1+0xa0]

	ldd	[%i0+0x98], %l0		! 0x9c
	ldd	[%i0+0x90], %l2
	std	%l0, [%i1+0x98]
	std	%l2, [%i1+0x90]

	ldd	[%i0+0x88], %l0		! 0x8c
	ldd	[%i0+0x80], %l2
	std	%l0, [%i1+0x88]
	std	%l2, [%i1+0x80]

	ldd	[%i0+0x78], %l0		! 0x7c
	ldd	[%i0+0x70], %l2
	std	%l0, [%i1+0x78]
	std	%l2, [%i1+0x70]

	ldd	[%i0+0x68], %l0		! 0x6c
	ldd	[%i0+0x60], %l2
	std	%l0, [%i1+0x68]
	std	%l2, [%i1+0x60]

	ldd	[%i0+0x58], %l0		! 0x5c
	ldd	[%i0+0x50], %l2
	std	%l0, [%i1+0x58]
	std	%l2, [%i1+0x50]

	ldd	[%i0+0x48], %l0		! 0x4c
	ldd	[%i0+0x40], %l2
	std	%l0, [%i1+0x48]
	std	%l2, [%i1+0x40]

	ldd	[%i0+0x38], %l0		! 0x3c
	ldd	[%i0+0x30], %l2
	std	%l0, [%i1+0x38]
	std	%l2, [%i1+0x30]

	ldd	[%i0+0x28], %l0		! 0x2c
	ldd	[%i0+0x20], %l2
	std	%l0, [%i1+0x28]
	std	%l2, [%i1+0x20]

	ldd	[%i0+0x18], %l0		! 0x1c
	ldd	[%i0+0x10], %l2
	std	%l0, [%i1+0x18]
	std	%l2, [%i1+0x10]

	ldd	[%i0+0x8], %l0		! 0x0c
	ldd	[%i0], %l2
	std	%l0, [%i1+0x8]
	std	%l2, [%i1]

.instr:
	sub	%i2, %i3, %i2		! decrement count
	add	%i0, %i3, %i0		! increment from address
	cmp	%i2, 0x100		! enough to do another block?
	bge	.blkcopy		! yes, do another chunk
	add	%i1, %i3, %i1		! increment to address
	tst	%i2			! all done yet?
	ble	.cpdone			! yes, return
	cmp	%i2, 15			! can we do more cache lines
	bg,a	1f
	andn	%i2, 15, %i3		! %i3 bytes left, aligned (to 16 bytes)
	andn	%i2, 3, %i3		! %i3 bytes left, aligned to 4 bytes
	b	.wcpchk
	sub	%i0, %i1, %i0		! create diff of src and dest addr
1:
	set	.instr, %o5		! address of copy instructions
	sub	%o5, %i3, %o5		! jmp address relative to instr
	jmp	%o5
	nop
	SET_SIZE(bcopy_asm)

#endif	/* lint */

/*
 * Block copy with possibly overlapped operands.
 */

#if defined(lint)

/* ARGSUSED */
void
ovbcopy(const void *from, void *to, size_t count)
{}

#else	/* lint */

	ENTRY(ovbcopy)
	tst	%o2			! check count
	bg,a	1f			! nothing to do or bad arguments
	subcc	%o0, %o1, %o3		! difference of from and to address

	retl				! return
	nop
1:
	bneg,a	2f
	neg	%o3			! if < 0, make it positive
2:	cmp	%o2, %o3		! cmp size and abs(from - to)
	ble	bcopy_asm		! if size <= abs(diff): use bcopy,
	.empty				!   no overlap
	cmp	%o0, %o1		! compare from and to addresses
	blu	.ov_bkwd		! if from < to, copy backwards
	nop
	!
	! Copy forwards.
	!
.ov_fwd:
	ldub	[%o0], %o3		! read from address
	inc	%o0			! inc from address
	stb	%o3, [%o1]		! write to address
	deccc	%o2			! dec count
	bg	.ov_fwd			! loop till done
	inc	%o1			! inc to address

	retl				! return
	nop
	!
	! Copy backwards.
	!
.ov_bkwd:
	deccc	%o2			! dec count
	ldub	[%o0 + %o2], %o3	! get byte at end of src
	bg	.ov_bkwd		! loop till done
	stb	%o3, [%o1 + %o2]	! delay slot, store at end of dst

	retl				! return
	nop
	SET_SIZE(ovbcopy)

#endif	/* lint */

/*
 * Zero a block of storage, returning an error code if we
 * take a kernel pagefault which cannot be resolved.
 * Returns errno value on pagefault error, 0 if all ok
 */

#if defined(lint)

/* ARGSUSED */
int
kzero(void *addr, size_t count)
{ return(0); }

#else	/* lint */

	ENTRY(kzero)
#ifdef DEBUG
        sethi   %hi(KERNELBASE), %o2
        cmp     %o0, %o2
        bgeu    3f
        nop

        set     2f, %o0
        call    panic
        nop

2:      .asciz  "kzero: Argument is in user address space"
        .align 4

3:

#endif DEBUG
	sethi	%hi(.zeroerr), %o2
	b	.do_zero
	or	%o2, %lo(.zeroerr), %o2

/*
 * We got here because of a fault during kzero.
 * Errno value is in %g1.
 */
.zeroerr:
	st	%o5, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	retl
	mov	%g1, %o0
	SET_SIZE(kzero)

#endif	/* lint */

/*
 * Zero a block of storage.
 */

#if defined(lint)

/* ARGSUSED */
void
bzero_asm(void *addr, size_t count)
{}

#else	/* lint */

	ENTRY(bzero_asm)
#ifdef DEBUG
        sethi   %hi(KERNELBASE), %o2
        cmp     %o0, %o2
        bgeu    3f
        nop
 
        set     2f, %o0
        call    panic
        nop
 
2:      .asciz  "bzero: Argument is in user address space"
        .align 4
 
3:

#endif DEBUG
	ld	[THREAD_REG + T_LOFAULT], %o2
.do_zero:
	ld	[THREAD_REG + T_LOFAULT], %o5	! save t_lofault
	cmp	%o1, 15			! check for small counts
	bl	.byteclr		! just clear bytes
	st	%o2, [THREAD_REG + T_LOFAULT]	! install new vector
	!
	! Check for word alignment.
	!
	btst	3, %o0
	bz	.bzero_obmem
	mov	0x100, %o3		! constant size of main loop
	!
	!
	! clear bytes until word aligned
	!
1:	clrb	[%o0]
	add	%o0, 1, %o0
	btst	3, %o0
	bnz	1b
	sub	%o1, 1, %o1
	!
	! Word aligned.
	!
	! obmem, if needed move a word to become double-word aligned.
	!
.bzero_obmem:
	btst	7, %o0			! is double aligned?
	bz	.bzero_nobuf
	clr	%g1			! clr g1 for second half of double %g0
	clr	[%o0]			! clr to double boundry
	sub	%o1, 4, %o1
	b	.bzero_nobuf
	add	%o0, 4, %o0

	!std	%g0, [%o0+0xf8]
.bzero_blk:
	std	%g0, [%o0+0xf0]
	std	%g0, [%o0+0xe8]
	std	%g0, [%o0+0xe0]
	std	%g0, [%o0+0xd8]
	std	%g0, [%o0+0xd0]
	std	%g0, [%o0+0xc8]
	std	%g0, [%o0+0xc0]
	std	%g0, [%o0+0xb8]
	std	%g0, [%o0+0xb0]
	std	%g0, [%o0+0xa8]
	std	%g0, [%o0+0xa0]
	std	%g0, [%o0+0x98]
	std	%g0, [%o0+0x90]
	std	%g0, [%o0+0x88]
	std	%g0, [%o0+0x80]
	std	%g0, [%o0+0x78]
	std	%g0, [%o0+0x70]
	std	%g0, [%o0+0x68]
	std	%g0, [%o0+0x60]
	std	%g0, [%o0+0x58]
	std	%g0, [%o0+0x50]
	std	%g0, [%o0+0x48]
	std	%g0, [%o0+0x40]
	std	%g0, [%o0+0x38]
	std	%g0, [%o0+0x30]
	std	%g0, [%o0+0x28]
	std	%g0, [%o0+0x20]
	std	%g0, [%o0+0x18]
	std	%g0, [%o0+0x10]
	std	%g0, [%o0+0x08]
	std	%g0, [%o0]
.zinst:
	add	%o0, %o3, %o0		! increment source address
	sub	%o1, %o3, %o1		! decrement count
.bzero_nobuf:
	cmp	%o1, 0x100		! can we do whole chunk?
	bge,a	.bzero_blk
	std	%g0, [%o0+0xf8]		! do first double of chunk

	cmp	%o1, 7			! can we zero any more double words
	ble	.byteclr		! too small go zero bytes

	andn	%o1, 7, %o3		! %o3 bytes left, double-word aligned
	srl	%o3, 1, %o2		! using doubles, need 1 instr / 2 words
	set	.zinst, %o4		! address of clr instructions
	sub	%o4, %o2, %o4		! jmp address relative to instr
	jmp	%o4
	nop
	!
	! do leftover bytes
	!
3:
	add	%o0, 1, %o0		! increment address
.byteclr:
	subcc	%o1, 1, %o1		! decrement count
	bge,a	3b
	clrb	[%o0]			! zero a byte

	st	%o5, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	retl
	mov	0, %o0			! return (0)

	SET_SIZE(bzero_asm)

#endif	/* lint */

/*
 * Copy a null terminated string from one point to another in
 * the kernel address space.
 * NOTE - don't use %o5 in this routine as copy{in,out}str uses it.
 *
 * copystr(from, to, maxlength, lencopied)
 *	caddr_t from, to;
 *	u_int maxlength, *lencopied;
 */

#if defined(lint)

/* ARGSUSED */
int
copystr(const char *from, char *to, size_t maxlength, size_t *lencopied)
{ return(0); }

#else	/* lint */

	ENTRY(copystr)
#ifdef DEBUG
        sethi   %hi(KERNELBASE), %o4
        cmp     %o0, %o4
        blu     1f
        nop
 
        cmp     %o1, %o4
        bgeu    3f
        nop
 
1:
        set     2f, %o0
        call    panic
        nop
 
2:      .asciz  "copystr: Args in User Space"
        .align  4
 
3:
#endif DEBUG
.do_copystr:
	mov	%o2, %o4		! save original count
	tst	%o2
	bg,a	1f
	sub	%o0, %o1, %o0		! o0 gets the difference of src and dst

	!
	! maxlength <= 0
	!
	bz	.cs_out			! maxlength = 0
	mov	ENAMETOOLONG, %o0

	retl				! maxlength < 0
	mov	EFAULT, %o0			! return failure

	!
	! Do a byte by byte loop.
	! We do this instead of a word by word copy because most strings
	! are small and this takes a small number of cache lines.
	!
0:
	stb	%g1, [%o1]		! store byte
	tst	%g1			! null byte?
	bnz	1f
	add	%o1, 1, %o1		! incr dst addr

	b	.cs_out			! last byte in string
	mov	0, %o0			! ret code = 0
1:
	subcc	%o2, 1, %o2		! test count
	bge,a	0b
	ldub	[%o0+%o1], %g1		! delay slot, get source byte

	mov	0, %o2			! max number of bytes moved
	mov	ENAMETOOLONG, %o0	! ret code = ENAMETOOLONG
.cs_out:
	tst	%o3			! want length?
	bz	2f
	.empty
	sub	%o4, %o2, %o4		! compute length and store it
	st	%o4, [%o3]
2:
	retl
	nop				! return (ret code)
	SET_SIZE(copystr)

#endif	/* lint */

/*
 * Transfer data to and from user space -
 * Note that these routines can cause faults
 * It is assumed that the kernel has nothing at
 * less than KERNELBASE in the virtual address space.
 *
 * Note that copyin(9F) and copyout(9F) are part of the
 * DDI/DKI which specifies that they return '-1' on "errors."
 *
 * Sigh.
 *
 * So there's two extremely similar routines - xcopyin() and xcopyout()
 * which return the errno that we've faithfully computed.  This
 * allows other callers (e.g. uiomove(9F)) to work correctly.
 * Given that these are used pretty heavily, we expand the calling
 * sequences inline for all flavours (rather than making wrappers).
 */

/*
 * Copy kernel data to user space.
 */

#if defined(lint)

/*ARGSUSED*/
int
default_copyout(const void *kaddr, void *uaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(default_copyout)

	TRACE_IN(TR_COPYOUT_START, TR_copyout_start)

	sethi	%hi(KERNELBASE), %g1	! test uaddr < KERNELBASE
#ifdef DEBUG
        cmp     %o0, %g1
        bgeu    3f
        nop
 
        set     2f, %o0
        call    panic
        nop
2:
        .asciz  "copyout: from arg not in KERNEL space"
        .align 4
 
3:
#endif DEBUG
	cmp	%o1, %g1
	sethi	%hi(.copyioerr), %o3	! .copyioerr is lofault value
	bleu	.do_copy		! common code
	or	%o3, %lo(.copyioerr), %o3

	TRACE_ASM_0 (%o2, TR_FAC_BCOPY, TR_COPYOUT_FAULT, TR_copyout_fault);

	retl				! return failure
	mov	-1, %o0
	SET_SIZE(default_copyout)

#endif	/* lint */

#ifdef	lint

/*ARGSUSED*/
int
default_xcopyout(const void *kaddr, void *uaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(default_xcopyout)

	TRACE_IN(TR_COPYOUT_START, TR_copyout_start)

	sethi	%hi(KERNELBASE), %g1	! test uaddr < KERNELBASE
	cmp	%o1, %g1
	sethi	%hi(.xcopyioerr), %o3	! .xcopyioerr is lofault value
	bleu	.do_copy		! common code
	or	%o3, %lo(.xcopyioerr), %o3

	TRACE_ASM_0(%o2, TR_FAC_BCOPY, TR_COPYOUT_FAULT, TR_copyout_fault);

	retl				! return failure
	mov	EFAULT, %o0
	SET_SIZE(default_xcopyout)

#endif	/* lint */
	
/*
 * Copy user data to kernel space.
 */

#if defined(lint)

/*ARGSUSED*/
int
default_copyin(const void *uaddr, void *kaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(default_copyin)

	TRACE_IN(TR_COPYIN_START, TR_copyin_start)

	sethi	%hi(KERNELBASE), %g1	! test uaddr < KERNELBASE
#ifdef DEBUG
        cmp     %o1, %g1
        bgeu    3f
        nop
 
        set     2f, %o0
        call    panic
        nop
2:
        .asciz  "copyin: to arg not in KERNEL space"
        .align  4
 
3:
#endif DEBUG
	cmp	%o0, %g1
	sethi	%hi(.copyioerr), %o3	! .copyioerr is lofault value
	bleu	.do_copy		! common code
	or	%o3, %lo(.copyioerr), %o3

	TRACE_ASM_0 (%o2, TR_FAC_BCOPY, TR_COPYIN_FAULT, TR_copyin_fault);

	retl				! return failure
	mov	-1, %o0

/*
 * We got here because of a fault during default_copy{in,out}.
 * Errno value is in %g1, but DDI/DKI says return -1 (sigh).
 */
.copyioerr:
	st	%l6, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	ret
	restore	%g0, -1, %o0
	SET_SIZE(default_copyin)

#endif	/* lint */

#ifdef	lint

/*ARGSUSED*/
int
default_xcopyin(const void *uaddr, void *kaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(default_xcopyin)

	TRACE_IN(TR_COPYIN_START, TR_copyin_start)

	sethi	%hi(KERNELBASE), %g1	! test uaddr < KERNELBASE
	cmp	%o0, %g1
	sethi	%hi(.xcopyioerr), %o3	! .xcopyioerr is lofault value
	bleu	.do_copy		! common code
	or	%o3, %lo(.xcopyioerr), %o3

	TRACE_ASM_0 (%o2, TR_FAC_BCOPY, TR_COPYIN_FAULT, TR_copyin_fault);

	retl				! return failure
	mov	EFAULT, %o0

/*
 * We got here because of a fault during xcopy{in,out}.
 * Errno value is in %g1.
 */
.xcopyioerr:
	st	%l6, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	ret
	restore	%g1, 0, %o0
	SET_SIZE(default_xcopyin)

#endif	/* lint */

/*
 * Copy a null terminated string from the user address space into
 * the kernel address space.
 */

#if defined(lint)

/* ARGSUSED */
int
default_copyinstr(const char *uaddr, char *kaddr, size_t maxlength,
    size_t *lencopied)
{ return (0); }

#else	/* lint */

	ENTRY(default_copyinstr)
	sethi	%hi(KERNELBASE), %g1	! test uaddr < KERNELBASE
#ifdef DEBUG
        cmp     %o1, %g1
        bgeu    3f
        nop
 
        set     2f, %o0
        call    panic
        nop
 
2:      .asciz  "copyinstr: To arg not in kernel space"
        .align  4
3:
#endif DEBUG
	cmp	%o0, %g1
	bgeu	.copystrerr
	mov	%o7, %o5		! save return address

.cs_common:
	set	.copystrerr, %g1
	call	.do_copystr
	st	%g1, [THREAD_REG + T_LOFAULT]	! catch faults

	jmp	%o5 + 8			! return (results of copystr)
	clr	[THREAD_REG + T_LOFAULT]		! clear fault catcher
	SET_SIZE(default_copyinstr)

#endif	/* lint */

/*
 * Copy a null terminated string from the kernel
 * address space to the user address space.
 */

#if defined(lint)

/* ARGSUSED */
int
default_copyoutstr(const char *kaddr, char *uaddr, size_t maxlength,
    size_t *lencopied)
{ return (0); }

#else	/* lint */

	ENTRY(default_copyoutstr)
	sethi	%hi(KERNELBASE), %g1	! test uaddr < KERNELBASE
#ifdef DEBUG
        cmp     %o0, %g1
        bgeu    3f
        nop
 
        set     2f, %o0
        call    panic
        nop
 
2:      .asciz  "copyoutstr: From arg in user space"
        .align  4
3:
#endif DEBUG
	cmp	%o1, %g1
	blu	.cs_common
	mov	%o7, %o5		! save return address
	! fall through

/*
 * Fault while trying to move from or to user space.
 * Set and return error code.
 */
.copystrerr:
	mov	EFAULT, %o0
	jmp	%o5 + 8			! return failure
	clr	[THREAD_REG + T_LOFAULT]
	SET_SIZE(default_copyoutstr)

#endif	/* lint */


#ifdef IOC_DW_BUG
#if defined(lint)

/*
 * bcopy_asm_toio()
 * Like bcopy_asm(), but used only for writes to I/O space.
 * We scan the data to make sure we don't std any doublewords
 * that will trip over an IOC hardware bug.
 */

/* ARGSUSED */
void
bcopy_asm_toio(const void *from, void *to, size_t count)
{}

#else	/* lint */

	ENTRY(bcopy_asm_toio)

	TRACE_IN(TR_BCOPY_START, TR_bcopy_start)

	ld	[THREAD_REG + T_LOFAULT], %o3
.do_copy_nodw:
	save	%sp, -SA(MINFRAME), %sp	! get another window
	ld	[THREAD_REG + T_LOFAULT], %l6	! save t_lofault
.bcopy_cmn_nodw:
	cmp	%i2, 12			! for small counts
	bl	.bytecp			! just copy bytes
	st	%i3, [THREAD_REG + T_LOFAULT]	! install new vector
	!
	! use aligned transfers where possible
	!
.bcopy_obmem_nodw:
	xor	%i0, %i1, %o4		! xor from and to address
	btst	7, %o4			! if lower three bits zero
	bz	.aldoubcp_nodw		! can align on double boundary
	nop
	b	.bcopy_words
	nop

.aldoubcp_nodw:
	cmp	%i2, 56			! if count < 56, use wordcp, it takes
	bl,a	.alwordcp		! longer to align doubles than words
	mov	3, %o0			! mask for word alignment
	call	.alignit		! copy bytes until aligned
	mov	7, %o0			! mask for double alignment

	!
	! i3 has aligned count returned by alignit
	!
	and	%i2, 7, %i2		! unaligned leftover count
	sub	%i0, %i1, %i0		! i0 gets the difference of src and dst

	!
	! Here's where the IOC bug hits us.  Instead of just doing a ldd
	! and std, we need to test for a certain dword data pattern.
	! If the the test is positive, we do 2 word writes.  If negative,
	! then we do the normal std, which is what will happen 99.9999% of
	! the time.  Yes, I knew you could say "ugly".
	!
	sethi	%hi(0x40000000), %i4	! IOC Bug pattern word 0: 0x4000000F
	add	%i4, 0xf, %i4		!
	sethi	%hi(0xe0200000), %i5	! IOC Bug pattern word 1: 0xE0200000
	sethi	%hi(0x74000000), %l0	! IOC Bug mask word 0: 0x7400000F
	or	%l0, 0xf, %l0		! 
	sethi	%hi(0xf0f80000), %l1	! IOC Bug mask word 1: 0xF0F80000
5:
	ldd	[%i0+%i1], %o4		! read dword from address

	and	%o4, %l0, %l2		! dword[0] & bugmask[0]
	cmp	%i4, %l2		! == bugpattern word 0?
	bne,a	dec_counter		! no? do std then
	std	%o4, [%i1]		! std destination address
	
	and	%o5, %l1, %l3		! dword[1] & bugmask[1]
	cmp	%i5, %l3		! == bugpattern word 1?
	bne,a	dec_counter		! no? do std then
	std	%o4, [%i1]		! std destination address

do_stword:
	st	%o4, [%i1]		! write 1st word at destination address
	st	%o5, [%i1+4]		! write 2nd word at destination address

dec_counter:
	subcc	%i3, 8, %i3		! dec count
	bg	5b
	add	%i1, 8, %i1		! delay slot, inc to address
	b	.wcpchk
	nop

	SET_SIZE(bcopy_asm_toio)

#endif	/* lint */
#endif	/* IOC_DW_BUG */



/*
 * Copy a block of storage - must not overlap (from + len <= to).
 * No fault handler installed (to be called under on_fault())
 */

#if defined(lint)

/* ARGSUSED */
void
copyin_noerr(const void *ufrom, void *kto, size_t count)
{}

#else	/* lint */

	ENTRY(copyin_noerr)

	TRACE_IN(TR_COPYIN_NOERR_START, TR_copyin_noerr_start)

#ifdef DEBUG
	sethi	%hi(KERNELBASE), %g1	! test uaddr < KERNELBASE
	cmp	%o0, %g1
	bgu	1f
	nop
	cmp	%o1, %g1
	bgu	3f
	nop

1:
	set	2f, %o0
	call	panic
	nop

	!no return

2: 	
	.asciz	"copyin_noerr: address args not in correct user/kernel space"
	.align	4
3:	

#endif DEBUG
	ba	.do_copy
	ld	[THREAD_REG + T_LOFAULT], %o3

	SET_SIZE(copyin_noerr)
#endif lint

/*
 * Copy a block of storage - must not overlap (from + len <= to).
 * No fault handler installed (to be called under on_fault())
 */

#if defined(lint)

/* ARGSUSED */
void
copyout_noerr(const void *kfrom, void *uto, size_t count)
{}

#else	/* lint */

	ENTRY(copyout_noerr)

	TRACE_IN(TR_COPYOUT_NOERR_START, TR_copyout_noerr_start)


#ifdef DEBUG
	sethi	%hi(KERNELBASE), %g1	! test uaddr < KERNELBASE
	cmp	%o1, %g1
	bgu	1f
	nop
	cmp	%o0, %g1		! test for kaddr
	bgu	3f
	nop

1:
	set	2f, %o0
	call	panic
	nop

	!no return

2: 	
	.asciz	"copyout_noerr: address args not in correct user/kernel space"
	.align 4
3:
#endif DEBUG
	ba	.do_copy
	ld	[THREAD_REG + T_LOFAULT], %o3

	SET_SIZE(copyout_noerr)
#endif lint


/*
 * Zero a block of storage in user space
 */

#if defined(lint)

/* ARGSUSED */
void
uzero(void *addr, size_t count)
{}

#else lint

	ENTRY(uzero)


#ifdef DEBUG
	sethi	%hi(KERNELBASE), %g1	! test uaddr < KERNELBASE
	cmp	%o0, %g1
	bleu	3f	
	nop

	set	2f, %o0
	call	panic
	nop

2:	
	.asciz	"uzero: address arg is not user space"
	.align 4
3:
#endif DEBUG

	ba	.do_zero
	ld	[THREAD_REG + T_LOFAULT], %o2
	
	SET_SIZE(uzero)
#endif lint

/*
 * copy a block of storage in user space
 */
 
#if defined(lint)
 
/* ARGSUSED */
void
ucopy(const void *ufrom, void *uto, size_t ulength)
{}
 
#else lint
 
        ENTRY(ucopy)
 
 
#ifdef DEBUG
        sethi   %hi(KERNELBASE), %g1    ! test uaddr < KERNELBASE
        cmp     %o1, %g1
        bgu   	1f 
        nop

	cmp	%o0, %g1
	bleu	3f	
	nop

1: 
        set     2f, %o0
        call    panic
        nop
 
2:
	.asciz  "ucopy: address arg is not user space"
	.align 4
3:
#endif DEBUG

	ba	.do_copy
        ld      [THREAD_REG + T_LOFAULT], %o3

	SET_SIZE(ucopy)
#endif lint
