/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#ident	"@(#)ip_ocsum.s	1.6	97/05/24 SMI"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/asm_linkage.h>
#include <sys/vtrace.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/machthread.h>
#include <sys/machparam.h>

#if defined(lint)
#include <sys/types.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

/*
 * ip_ocsum(address, halfword_count, sum)
 * Do a 16 bit one's complement sum of a given number of (16-bit)
 * halfwords. The halfword pointer must not be odd.
 *	%o0 address; %o1 count; %o2 sum accumulator; %o4 temp
 * 	%g2 and %g3 used in main loop
 *
 * (from @(#)ocsum.s 1.3 89/02/24 SMI)
 *
 */

#if defined(lint) 

/* ARGSUSED */
unsigned int
ip_ocsum(u_short *address, int halfword_count, unsigned int sum)
{ return (0); }

#else	/* lint */

	ENTRY(ip_ocsum)
	cmp	%o1, 31		! less than 62 bytes?
	bl,a	.dohw		!   just do halfwords
	tst	%o1		! delay slot, test count

	btst	31, %o0		! (delay slot)
	bz,a	2f		! if 32 byte aligned, skip
	subcc	%o1, 64, %o1	! delay, decrement count to aid testing

	!
	! Do first halfwords until 32-byte aligned
	!
1:
	lduh	[%o0], %o4	! read data
	add	%o0, 2, %o0	! increment address
	add	%o2, %o4, %o2	! add to accumulator, don't need carry yet
	btst	31, %o0		! 32 byte aligned?
	bnz	1b
	sub	%o1, 1, %o1	! decrement count

	subcc	%o1, 64, %o1	! decrement count to aid testing
2:
	!
	! We have at least 32 bytes (62 minus at most 30 spent in the
	! alignment loop above. When we have less than 128 bytes we 
	! jump into the middle of the loop with the count and address
	! adjusted. When jumping into the middle of the loop we need to
	! clear the carry bit as well.
	!
	bge	4f		! count >= 0? (i.e. more than 128 bytes)
	clr	%o5		! delay - %o5 for add at loop top or bottom

	!
	! Handle between 64 and 128 bytes by jumping into the halfway
	! point of the loop.
	!
	addcc	%o1, 32, %o1
	bcc	3f	
	addcc	%g0, %g0, %g0	! delay, clear carry bit

	ba	5f
	sub	%o0, 64, %o0	! delay
3:
	!
	! Handle between 32 and 64 bytes
	!
	add	%o1, 16, %o1
	ba	6f
	sub	%o0, 96, %o0	! delay
	!
	! loop to add in 128 byte chunks
	! The loads and adds are staggered to help avoid load/use
	! interlocks on highly pipelined implementations. 32-bit loads are
	! used since these will group with the addxcc on Viking.
	!
4:
	!
	! Final %o5 add done at top of loop or after loop
	!
	addcc	%o2, %o5, %o2

	ld	[%o0], %o4
	ld	[%o0+4], %o5
	addxcc	%o2, %o4, %o2
	ld	[%o0+8], %o4
	addxcc	%o2, %o5, %o2
	ld	[%o0+12], %o5
	addxcc	%o2, %o4, %o2
	ld	[%o0+16], %o4
	addxcc	%o2, %o5, %o2
	ld	[%o0+20], %o5
	addxcc	%o2, %o4, %o2
	ld	[%o0+24], %o4
	addxcc	%o2, %o5, %o2
	ld	[%o0+28], %o5
	addxcc	%o2, %o4, %o2
	addxcc	%o2, %o5, %o2

	ld	[%o0+32+0], %o4
	ld	[%o0+32+4], %o5
	addxcc	%o2, %o4, %o2
	ld	[%o0+32+8], %o4
	addxcc	%o2, %o5, %o2
	ld	[%o0+32+12], %o5
	addxcc	%o2, %o4, %o2
	ld	[%o0+32+16], %o4
	addxcc	%o2, %o5, %o2
	ld	[%o0+32+20], %o5
	addxcc	%o2, %o4, %o2
	ld	[%o0+32+24], %o4
	addxcc	%o2, %o5, %o2
	ld	[%o0+32+28], %o5
	addxcc	%o2, %o4, %o2
	addxcc	%o2, %o5, %o2
5:
	ld	[%o0+64+0], %o4
	ld	[%o0+64+4], %o5
	addxcc	%o2, %o4, %o2
	ld	[%o0+64+8], %o4
	addxcc	%o2, %o5, %o2
	ld	[%o0+64+12], %o5
	addxcc	%o2, %o4, %o2
	ld	[%o0+64+16], %o4
	addxcc	%o2, %o5, %o2
	ld	[%o0+64+20], %o5
	addxcc	%o2, %o4, %o2
	ld	[%o0+64+24], %o4
	addxcc	%o2, %o5, %o2
	ld	[%o0+64+28], %o5
	addxcc	%o2, %o4, %o2
	addxcc	%o2, %o5, %o2
6:
	ld	[%o0+96+0], %o4
	ld	[%o0+96+4], %o5
	addxcc	%o2, %o4, %o2
	ld	[%o0+96+8], %o4
	addxcc	%o2, %o5, %o2
	ld	[%o0+96+12], %o5
	addxcc	%o2, %o4, %o2
	ld	[%o0+96+16], %o4
	addxcc	%o2, %o5, %o2
	ld	[%o0+96+20], %o5
	addxcc	%o2, %o4, %o2
	ld	[%o0+96+24], %o4
	addxcc	%o2, %o5, %o2
	ld	[%o0+96+28], %o5
	addxcc	%o2, %o4, %o2
	!
	! Final %o5 add done at top of loop or after loop
	!

	addxcc	%o2, 0, %o2	! if final carry, add it in
	subcc	%o1, 64, %o1	! decrement count (in halfwords)
	bge	4b
	add	%o0, 128, %o0	! delay slot, increment address
	
	!
	! Final %o5 add done at top of loop or after loop
	!
	addcc	%o2, %o5, %o2	! add to accumulator with carry
	add	%o1, 64, %o1	! add back in
	addxcc	%o2, 0, %o2	! if final carry, add it in
	!
	! Do any remaining halfwords
	!
	b	.dohw
	tst	%o1		! delay slot, for more to do

3:
	add	%o0, 2, %o0	! increment address
	addcc	%o2, %o4, %o2	! add to accumulator
	addxcc	%o2, 0, %o2	! if carry, add it in
	subcc	%o1, 1, %o1	! decrement count
.dohw:
	bg,a	3b		! more to do?
	lduh	[%o0], %o4	! read data

	!
	! at this point the 32-bit accumulator
	! has the result that needs to be returned in 16-bits
	!
	sll	%o2, 16, %o4	! put low halfword in high halfword %o4
	addcc	%o4, %o2, %o2	! add the 2 halfwords in high %o2, set carry
	srl	%o2, 16, %o2	! shift to low halfword
	retl			! return
	addxcc	%o2, 0, %o0	! add in carry if any. result in %o0
	SET_SIZE(ip_ocsum)

#endif 	/* lint */


/*
 * ip_ocsum_copy(address, halfword_count, sum, dest)
 * Do a 16 bit one's complement sum of a given number of (16-bit)
 * halfwords. The halfword pointer must not be odd.
 *	%i0 address; %i1 count; %i2 accumulator; %i3 dest; 
 *	%i5 saved sump
 *	%o4 set if doing copyin/copyout
 *	%o5 saved t_lofault
 * Note: Assumes dest has the same relative allignment as address.
 */

#if defined(lint)

/* ARGSUSED */
unsigned int
ip_ocsum_copy(u_short *address, int halfword_count, 
    unsigned int sum, u_short *dest)
{ return (0); }

#else	/* lint */

	ENTRY(ip_ocsum_copy)
	save	%sp, -SA(MINFRAME), %sp	! get another window
	ba	.setup_done
	clr	%o4

	!
	! Entry point for copyin and copyout.
	! %i2 contains pointer to sum
	! %i4 contains lofault routine
	!
.do_ocsum_copy:
	save	%sp, -SA(MINFRAME), %sp	! get another window
	ld	[THREAD_REG + T_LOFAULT], %o5	! save t_lofault
	mov	%i2, %i5			! save pointer to sum
	st	%i4, [THREAD_REG + T_LOFAULT]	! install new vector
	mov	1, %o4	
	ld	[%i2], %i2

.setup_done:
	cmp	%i1, 31		! less than 62 bytes?
	bl,a	.dohw_copy	!   just do halfwords
	tst	%i1		! delay slot, test count

	btst	31, %i0		! (delay slot)
	bz,a	2f		! if 32 byte aligned, skip
	subcc	%i1, 64, %i1	! delay, decrement count to aid testing

	!
	! Do first halfwords until 32-byte aligned
	!
1:
	lduh	[%i0], %l0	! read data
	add	%i0, 2, %i0	! increment address
	add	%i2, %l0, %i2	! add to accumulator, don't need carry yet
	sth	%l0, [%i3]	! *dest++
	add	%i3, 2, %i3
	btst	31, %i0		! 32 byte aligned?
	bnz	1b
	sub	%i1, 1, %i1	! decrement count

	subcc	%i1, 64, %i1	! decrement count to aid testing
2:
	!
	! We have at least 32 bytes (62 minus at most 30 spent in the
	! alignment loop above. When we have less than 128 bytes we 
	! jump into the middle of the loop with the count, address, and dest
	! adjusted.
	!
	bge,a	4f		! count >= 0? (i.e. > 128 bytes)
	nop			! delay

	!
	! Handle between 64 and 128 bytes by jumping into the halfway
	! point of the loop.
	!
	addcc	%i1, 32, %i1
	bcc	3f	
	addcc	%g0, %g0, %g0	! delay, clear carry bit

	sub	%i3, 64, %i3
	ba	5f
	sub	%i0, 64, %i0	! delay
3:
	!
	! Handle between 32 and 64 bytes
	!
	add	%i1, 16, %i1
	sub	%i3, 96, %i3
	ba	6f
	sub	%i0, 96, %i0	! delay
	!
	! loop to add in 128 byte chunks
	! The loads and adds are staggered to help avoid load/use
	! interlocks on highly pipelined implementations, and double
	! loads are used for 64-bit wide memory systems.
	! We use 32 bit stores since they group with the addxcc on Viking.
	!
4:
#define ldd2st
#ifdef ldd2st
	ldd	[%i0], %l0
	ldd	[%i0+8], %l2
	ldd	[%i0+16], %l4
	ldd	[%i0+24], %l6
	st	%l0, [%i3]
	addcc	%i2, %l0, %i2
	st	%l1, [%i3+4]
	addxcc	%i2, %l1, %i2
	st	%l2, [%i3+8]
	addxcc	%i2, %l2, %i2
	st	%l3, [%i3+12]
	addxcc	%i2, %l3, %i2
	st	%l4, [%i3+16]
	addxcc	%i2, %l4, %i2
	st	%l5, [%i3+20]
	addxcc	%i2, %l5, %i2
	st	%l6, [%i3+24]
	addxcc	%i2, %l6, %i2
	st	%l7, [%i3+28]
	addxcc	%i2, %l7, %i2

	ldd	[%i0+32+0], %l0
	ldd	[%i0+32+8], %l2
	ldd	[%i0+32+16], %l4
	ldd	[%i0+32+24], %l6
	st	%l0, [%i3+32+0]
	addxcc	%i2, %l0, %i2
	st	%l1, [%i3+32+4]
	addxcc	%i2, %l1, %i2
	st	%l2, [%i3+32+8]
	addxcc	%i2, %l2, %i2
	st	%l3, [%i3+32+12]
	addxcc	%i2, %l3, %i2
	st	%l4, [%i3+32+16]
	addxcc	%i2, %l4, %i2
	st	%l5, [%i3+32+20]
	addxcc	%i2, %l5, %i2
	st	%l6, [%i3+32+24]
	addxcc	%i2, %l6, %i2
	st	%l7, [%i3+32+28]
	addxcc	%i2, %l7, %i2
5:
	ldd	[%i0+64+0], %l0
	ldd	[%i0+64+8], %l2
	ldd	[%i0+64+16], %l4
	ldd	[%i0+64+24], %l6
	st	%l0, [%i3+64+0]
	addxcc	%i2, %l0, %i2
	st	%l1, [%i3+64+4]
	addxcc	%i2, %l1, %i2
	st	%l2, [%i3+64+8]
	addxcc	%i2, %l2, %i2
	st	%l3, [%i3+64+12]
	addxcc	%i2, %l3, %i2
	st	%l4, [%i3+64+16]
	addxcc	%i2, %l4, %i2
	st	%l5, [%i3+64+20]
	addxcc	%i2, %l5, %i2
	st	%l6, [%i3+64+24]
	addxcc	%i2, %l6, %i2
	st	%l7, [%i3+64+28]
	addxcc	%i2, %l7, %i2
6:
	ldd	[%i0+96+0], %l0
	ldd	[%i0+96+8], %l2
	ldd	[%i0+96+16], %l4
	ldd	[%i0+96+24], %l6
	st	%l0, [%i3+96+0]
	addxcc	%i2, %l0, %i2
	st	%l1, [%i3+96+4]
	addxcc	%i2, %l1, %i2
	st	%l2, [%i3+96+8]
	addxcc	%i2, %l2, %i2
	st	%l3, [%i3+96+12]
	addxcc	%i2, %l3, %i2
	st	%l4, [%i3+96+16]
	addxcc	%i2, %l4, %i2
	st	%l5, [%i3+96+20]
	addxcc	%i2, %l5, %i2
	st	%l6, [%i3+96+24]
	addxcc	%i2, %l6, %i2
	st	%l7, [%i3+96+28]
	addxcc	%i2, %l7, %i2
#else
	ld	[%i0], %l0
	ld	[%i0+4], %l1
	addcc	%i2, %l0, %i2
	ld	[%i0+8], %l2
	addxcc	%i2, %l1, %i2
	ld	[%i0+12], %l3
	addxcc	%i2, %l2, %i2
	ld	[%i0+16], %l4
	addxcc	%i2, %l3, %i2
	ld	[%i0+20], %l5
	addxcc	%i2, %l4, %i2
	ld	[%i0+24], %l6
	addxcc	%i2, %l5, %i2
	ld	[%i0+28], %l7
	addxcc	%i2, %l6, %i2
	std	%l0, [%i3]
	std	%l2, [%i3+8]
	std	%l4, [%i3+16]
	std	%l6, [%i3+24]
	addxcc	%i2, %l7, %i2

	ld	[%i0+32+0], %l0
	ld	[%i0+32+4], %l1
	addxcc	%i2, %l0, %i2
	ld	[%i0+32+8], %l2
	addxcc	%i2, %l1, %i2
	ld	[%i0+32+12], %l3
	addxcc	%i2, %l2, %i2
	ld	[%i0+32+16], %l4
	addxcc	%i2, %l3, %i2
	ld	[%i0+32+20], %l5
	addxcc	%i2, %l4, %i2
	ld	[%i0+32+24], %l6
	addxcc	%i2, %l5, %i2
	ld	[%i0+32+28], %l7
	addxcc	%i2, %l6, %i2
	std	%l0, [%i3+32+0]
	std	%l2, [%i3+32+8]
	std	%l4, [%i3+32+16]
	std	%l6, [%i3+32+24]
	addxcc	%i2, %l7, %i2
5:
	ld	[%i0+64+0], %l0
	ld	[%i0+64+4], %l1
	addxcc	%i2, %l0, %i2
	ld	[%i0+64+8], %l2
	addxcc	%i2, %l1, %i2
	ld	[%i0+64+12], %l3
	addxcc	%i2, %l2, %i2
	ld	[%i0+64+16], %l4
	addxcc	%i2, %l3, %i2
	ld	[%i0+64+20], %l5
	addxcc	%i2, %l4, %i2
	ld	[%i0+64+24], %l6
	addxcc	%i2, %l5, %i2
	ld	[%i0+64+28], %l7
	addxcc	%i2, %l6, %i2
	std	%l0, [%i3+64+0]
	std	%l2, [%i3+64+8]
	std	%l4, [%i3+64+16]
	std	%l6, [%i3+64+24]
	addxcc	%i2, %l7, %i2
6:
	ld	[%i0+96+0], %l0
	ld	[%i0+96+4], %l1
	addxcc	%i2, %l0, %i2
	ld	[%i0+96+8], %l2
	addxcc	%i2, %l1, %i2
	ld	[%i0+96+12], %l3
	addxcc	%i2, %l2, %i2
	ld	[%i0+96+16], %l4
	addxcc	%i2, %l3, %i2
	ld	[%i0+96+20], %l5
	addxcc	%i2, %l4, %i2
	ld	[%i0+96+24], %l6
	addxcc	%i2, %l5, %i2
	ld	[%i0+96+28], %l7
	addxcc	%i2, %l6, %i2
	std	%l0, [%i3+96+0]
	std	%l2, [%i3+96+8]
	std	%l4, [%i3+96+16]
	std	%l6, [%i3+96+24]
	addxcc	%i2, %l7, %i2
#endif

	add	%i3, 128, %i3	! increment dest
	addxcc	%i2, 0, %i2	! if final carry, add it in
	subcc	%i1, 64, %i1	! decrement count (in halfwords)
	bge	4b
	add	%i0, 128, %i0	! delay slot, increment address

	add	%i1, 64, %i1	! add back in
	!
	! Do any remaining halfwords
	!
	b	.dohw_copy
	tst	%i1		! delay slot, for more to do

3:
	add	%i0, 2, %i0	! increment address
	addcc	%i2, %l0, %i2	! add to accumulator
	sth	%l0, [%i3]	! *dest++
	add	%i3, 2, %i3
	addxcc	%i2, 0, %i2	! if carry, add it in
	subcc	%i1, 1, %i1	! decrement count
.dohw_copy:
	bg,a	3b		! more to do?
	lduh	[%i0], %l0	! read data

	!
	! at this point the 32-bit accumulator
	! has the result that needs to be returned in 16-bits
	!
	sll	%i2, 16, %l2	! put low halfword in high halfword %l2
	addcc	%l2, %i2, %i2	! add the 2 halfwords in high %i2, set carry
	srl	%i2, 16, %i2	! shift to low halfword
	addxcc	%i2, 0, %i0	! add in carry if any. result in %i0
	tst	%o4
	be,a	.done
	nop

	st	%i0, [%i5]
	clr	%i0
	st	%o5, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
.done:
	ret			! return
	restore
	SET_SIZE(ip_ocsum_copy)

#endif	/* lint */

/*
 * Copy kernel data to user space.
 *
 * int
 * ip_ocsum_copyout(caddr_t kaddr, u_int len, u_int *sump, caddr_t uaddr)
 */

#ifdef	lint

/*ARGSUSED*/
int
ip_ocsum_copyout(caddr_t kaddr, u_int len, u_int *sump, caddr_t uaddr)
{ return (0); }

#else	/* lint */

	ENTRY(ip_ocsum_copyout)

	sethi	%hi(KERNELBASE), %o5	! test uaddr < KERNELBASE
	cmp	%o3, %o5
	sethi	%hi(.sumcopyioerr), %o4	! .sumcopyioerr is lofault value
	bleu	.do_ocsum_copy
	or	%o4, %lo(.sumcopyioerr), %o4	! delay
	
	ba,a	.ocsum_copyfault
	SET_SIZE(ip_ocsum_copyout)

#endif	/* lint */
	
/*
 * Copy user data to kernel space.
 *
 * int
 * ip_ocsum_copyin(caddr_t uaddr, u_int len, u_int *sump, caddr_t kaddr)
 */

#ifdef	lint

/*ARGSUSED*/
int
ip_ocsum_copyin(caddr_t uaddr, u_int len, u_int *sump, caddr_t kaddr)
{ return (0); }

#else	/* lint */

	ENTRY(ip_ocsum_copyin)

	sethi	%hi(KERNELBASE), %o5	! test uaddr < KERNELBASE
	cmp	%o0, %o5
	sethi	%hi(.sumcopyioerr), %o4	! .sumcopyioerr is lofault value
	bleu	.do_ocsum_copy
	or	%o4, %lo(.sumcopyioerr), %o4	! delay

.ocsum_copyfault:
	retl				! return failure
	mov	EFAULT, %o0

/*
 * We got here because of a fault during ip_ocsum_copy{in,out}.
 * Errno value is in %g1.
 */
.sumcopyioerr:
	st	%o5, [THREAD_REG + T_LOFAULT]	! restore old t_lofault
	ret
	restore	%g1, 0, %o0
	SET_SIZE(ip_ocsum_copyin)
#endif	/* lint */

