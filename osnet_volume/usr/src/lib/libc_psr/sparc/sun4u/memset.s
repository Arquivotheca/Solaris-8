/*
 * Copyright (c) 1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)memset.s	1.12	98/12/18 SMI"

	.file	"memset.s"
/*
 * char *memset(sp, c, n)
 *
 * Set an array of n chars starting at sp to the character c.
 * Return sp.
 *
 * Fast assembler language version of the following C-program for memset
 * which represents the `standard' for the C-library.
 *
 *	void *
 *	memset(void *sp1, int c, size_t n)
 *	{
 *	    if (n != 0) {
 *		char *sp = sp1;
 *		do {
 *		    *sp++ = (char)c;
 *		} while (--n != 0);
 *	    }
 *	    return (sp1);
 *	}
 */

#include <sys/asm_linkage.h>
#include <sys/spitasi.h>

	ANSI_PRAGMA_WEAK(memset,function)

#include "synonyms.h"

	ENTRY(memset)

	mov	%o0, %o5		! copy sp1 before using it
	cmp	%o2, 16			! if small counts, just write bytes
	bgu,a	%ncc, .wrbig
        andcc   %o5, 7, %o3             ! is sp1 aligned on a 8 byte bound
	
.wrchar:deccc   %o2                     ! byte clearing loop
        inc     %o5
	bgeu,a,pt %ncc, .wrchar
        stb     %o1, [%o5 + -1]         ! we've already incremented the address

        retl
        sub 	%o0, %g0, %o0

.wrbig:
        bz      %ncc, .blkchk           ! already double aligned
        sub     %o3, 8, %o3
        neg     %o3                     ! bytes till double aligned
 
        sub     %o2, %o3, %o2           ! update o2 with new count

	! Set %o3 bytes till double aligned
1:	stb     %o1, [%o5]		! there is at least 1 byte to set
	deccc   %o3                     ! byte clearing loop 
        bgu	%ncc, 1b
        inc     %o5 
	
	! Now sp1 is double aligned

.blkchk:
	and     %o1, 0xff, %o1          ! generate a word filled with c
	sll     %o1, 8, %o4
        or      %o1, %o4, %o1		! now o1 has 2 bytes of c
        sll     %o1, 16, %o4
        or      %o1, %o4, %o1		! now o1 has 4 bytes of c
	sllx	%o1, 32, %o4
	or	%o1, %o4, %g1		! now g1 has 8 bytes of c
	

        cmp     %o2, 320                ! if cnt < 256 + 64 -  no Block ld/st
        bgeu,a,pt %ncc, blkwr		!   do block write
        andcc   %o5, 63, %o3            ! is sp1 block aligned

.dwwr:
	! Store double words
	andn	%o2, 7, %o4		! o4 has 8 byte aligned cnt

2:
	stx	%g1, [%o5]
	add	%o5, 8, %o5	
	subcc	%o4, 8, %o4
	bgu	%ncc, 2b
	sub	%o2, 8, %o2

3:
	! Store bytes
	deccc   %o2                     ! byte clearing loop
        inc     %o5
        bgeu,a,pt %ncc, 3b
        stb     %o1, [%o5 + -1]         ! we've already incremented the address

dbldone:
        retl
        sub 	%o0, %g0, %o0
        
blkwr:
        bz,pn   %ncc, .blalign          ! now block aligned
        sub     %o3, 64, %o3
        neg     %o3                     ! bytes till block aligned

        ! Store %o3 bytes till dst is block (64 byte) aligned. use
        ! double word stores.
        andn    %o3, 7, %o4             ! o4 has 8 byte aligned cnt

2:
        stx     %g1, [%o5]
        add     %o5, 8, %o5      
        sub   	%o3, 8, %o3    
        subcc   %o4, 8, %o4    
        bgu	%ncc, 2b
        sub     %o2, 8, %o2
 
3:
        ! Store bytes
        deccc   %o3                     ! byte clearing loop
	blu,pn	%ncc, .blalign
        stb     %o1, [%o5]         	! we've already incremented the address
        ba,pt	%ncc, 3b
        inc     %o5
	

        ! sp1 is block aligned                                     
.blalign:
	save    %sp, -SA(MINFRAME + 8), %sp
        membar  #StoreLoad
        rd      %fprs, %l3              ! l3 = fprs

        ! if fprs.fef == 0, set it. Checking it, requires 2 instructions.
        ! So set it anyway, without checking.
        wr      %g0, 0x4, %fprs         ! fprs.fef = 1

	andn	%i2, 63, %i4		! calc number of blocks
	
	stx	%g1, [%sp+SA(MINFRAME)+STACK_BIAS] ! store it on the stack
	ldd	[%sp+SA(MINFRAME)+STACK_BIAS], %d0 ! load into a fp reg

	fmovd	%d0, %d2	
	fmovd	%d0, %d4	
	fmovd	%d0, %d6	
	fmovd	%d0, %d8	
	fmovd	%d0, %d10	
	fmovd	%d0, %d12	
	fmovd	%d0, %d14		! 1st quadrant has 64 bytes of C

4:
        stda    %d0, [%i5]ASI_BLK_P 
        add     %i5, 64, %i5      
        subcc   %i4, 64, %i4 
        bgu	%ncc, 4b
        sub     %i2, 64, %i2 

	! Set the remaining doubles

	andn    %i2, 7, %i4             ! calc doubles left after blkcpy
	subcc   %i4, 8, %i4
	blu,a,pn %ncc, 6f
	nop
5:	
	std     %d0, [%i5]
        sub     %i2, 8, %i2 
	subcc   %i4, 8, %i4
	bgeu,pt	%ncc, 5b
        add     %i5, 8, %i5      
6:
	! Set the remaining bytes
	tst	%i2
	bz,pt	%ncc, exit
#if 1 /* XXX */
	  nop				! delay slot

	/*
	 * XXX - partial stores through SysIO break (See bug 1200071).
	 * Until a hardware fix of some sort is in place we'll just do
	 * this the slow way.
	 */
7:
	stb	%i1, [%i5]		! there is at least 1 byte to set
	deccc	%i2			! byte clearing loop 
	bgu,pt	%ncc, 7b
	  inc	%i5 
#else
	! Terminate the copy with a partial store.
	! The data should be at d0
	dec	%i2			! needed to get the mask right
	add	%i5, %i2, %i4
	edge8	%i5, %i4, %i4
	stda	%d0, [%i5]%i4, ASI_PST8_P
#endif
 

exit:
        membar  #StoreLoad|#StoreStore
	and     %l3, 0x4, %l3           ! fprs.du = fprs.dl = 0
        wr      %l3, %g0, %fprs         ! fprs = l3 - restore fprs
	ret
	restore	%i0, %g0, %o0

	SET_SIZE(memset)
