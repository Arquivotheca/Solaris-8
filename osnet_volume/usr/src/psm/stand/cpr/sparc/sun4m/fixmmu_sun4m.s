/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fixmmu_sun4m.s	1.13	96/04/16 SMI"

#include <sys/asm_linkage.h>
#include <sys/machparam.h>
#include <sys/mmu.h>
#include <sys/module_ross625.h>
#include <v7/sys/psr.h>

#if defined(lint)

int
srmmu_mmu_getctp(void)
{ return (0); }

/* ARGSUSED */
int
srmmu_mmu_probe(int pa)
{ return (0); }

void  
srmmu_mmu_flushall(void)
{}

/* ARGSUSED */
void 
srmmu_mmu_flushpage(caddr_t base)
{}

/* ARGSUSED */
void
stphys(int physaddr, int value)
{}

/* ARGSUSED */
int
ldphys(int physaddr)
{ return(0); }

/* ARGSUSED */
int
move_page(u_int src_va, u_int dest_va)
{ return (0); }

void
ross625_turn_cache_on(void)
{}

/* ARGSUSED */
void
tsu_turn_cache_on()
{}

#else	/* lint */

	!
	! misc srmmu manipulation routines
	!
	! copied from ../sun4m/ml/module_srmmu_asm.s
	!
        ENTRY(srmmu_mmu_getctp)
        set     RMMU_CTP_REG, %o1       ! get srmmu context table ptr
        retl
        lda     [%o1]ASI_MOD, %o0
        SET_SIZE(srmmu_mmu_getctp)

        ENTRY(srmmu_mmu_probe)
        and     %o0, MMU_PAGEMASK, %o0  ! virtual page number
        or      %o0, FT_ALL<<8, %o0     ! match criteria
        retl
        lda     [%o0]ASI_FLPR, %o0
        SET_SIZE(srmmu_mmu_probe)

        ENTRY(srmmu_mmu_flushall)       
        or      %g0, FT_ALL<<8, %o0     ! flush entire mmu
        sta     %g0, [%o0]ASI_FLPR      ! do the flush
	nop				! XXXXXX: kludge for Tsunami
	nop				! XXXXXX: kludge for Tsunami
	nop				! XXXXXX: kludge for Tsunami
	nop				! XXXXXX: kludge for Tsunami
	nop				! XXXXXX: kludge for Tsunami
        retl
        nop                             ! MMU delay
        SET_SIZE(srmmu_mmu_flushall)       
 
        ENTRY(srmmu_mmu_flushpage)      
        or      %o0, FT_PAGE<<8, %o0
        sta     %g0, [%o0]ASI_FLPR      ! do the flush
        retl
        nop                             ! PSR or MMU delay
        SET_SIZE(srmmu_mmu_flushpage)      
 


	!
	! load value at physical address
	!
	! copied from ../sun4m/ml/subr_4m.s
	!
	ENTRY(ldphys)
	sethi	%hi(mxcc), %o4
	ld	[%o4+%lo(mxcc)], %o5
	tst	%o5
	bz,a	1f
	lda     [%o0]ASI_MEM, %o0

	sethi	%hi(use_table_walk), %o4
	ld	[%o4+%lo(use_table_walk)], %o5
	tst	%o5			! use_cache off in CC mode means
	bz,a	1f			! don't cache the srmmu tables.
	lda     [%o0]ASI_MEM, %o0

	! For Viking E-$, it is necessary to set the AC bit of the
	! module control register to indicate that this access
	! is cacheable.
	mov     %psr, %o4               ! get psr
	or      %o4, PSR_PIL, %o5       ! block traps
	mov     %o5, %psr               ! new psr
	nop; nop; nop                   ! PSR delay
	lda     [%g0]ASI_MOD, %o3       ! get MMU CSR
	set     CPU_VIK_AC, %o5         ! AC bit
	or      %o3, %o5, %o5           ! or in AC bit
	sta     %o5, [%g0]ASI_MOD       ! store new CSR
	lda     [%o0]ASI_MEM, %o0
	sta     %o3, [%g0]ASI_MOD       ! restore CSR
	mov     %o4, %psr               ! restore psr
	nop; nop; nop                   ! PSR delay
1:	retl
	nop
	SET_SIZE(ldphys)



	!
	! Store value at physical address
	!
	! void	stphys(physaddr, value)
	!
	ENTRY(stphys)
	sethi	%hi(mxcc), %o4
	ld	[%o4+%lo(mxcc)], %o5
	tst	%o5
	bz,a	1f
	sta     %o1, [%o0]ASI_MEM

	sethi	%hi(use_table_walk), %o4
	ld	[%o4+%lo(use_table_walk)], %o5
	tst	%o5			! use_cache off in CC mode means
	bz,a	1f			! don't cache the srmmu tables.
	sta     %o1, [%o0]ASI_MEM

	! For Viking E-$, it is necessary to set the AC bit of the
	! module control register to indicate that this access
	! is cacheable.
	mov     %psr, %o4               ! get psr
	or      %o4, PSR_PIL, %o5       ! block traps
	mov     %o5, %psr               ! new psr
	nop; nop; nop                   ! PSR delay
	lda     [%g0]ASI_MOD, %o3       ! get MMU CSR
	set     CPU_VIK_AC, %o5         ! AC bit
	or      %o3, %o5, %o5           ! or in AC bit
	sta     %o5, [%g0]ASI_MOD       ! store new CSR
	sta     %o1, [%o0]ASI_MEM       ! the actual stphys
	sta     %o3, [%g0]ASI_MOD       ! restore CSR
	mov     %o4, %psr               ! restore psr
	nop; nop; nop                   ! PSR delay
1:	retl
	nop
	SET_SIZE(stphys)



	!
	! move_page: used to relocate a boot page
	!
	! copy a page worth of data from src_va=%o0 to tmp_va=%o1
	!
	! assume all addreses are aligned
	!
	ENTRY(move_page)
	set     MMU_PAGESIZE, %o2	! do a whole page

.copy_loop:
	ldd [%o0+0x8], %o4		! copy 16 bytes at a time
	std %o4, [%o1+0x8]
	ldd [%o0+0x0], %o4
	std %o4, [%o1+0x0]

	add %o0, 0x10, %o0		! incr src addr
	subcc %o2, 0x10, %o2
	bg,a .copy_loop
	add %o1, 0x10, %o1		! incr dest addr

	retl
	nop
	SET_SIZE(move_page)

        ENTRY(tsu_turn_cache_on)
        sta     %g0, [%g0]ASI_ICFCLR    ! flash clear icache
        sta     %g0, [%g0]ASI_DCFCLR    ! flash clear dcache
        set     0x300, %o2      	! i & d cache enable
        set     RMMU_CTL_REG, %o0
        lda     [%o0]ASI_MOD, %o1
        or      %o1, %o2, %o1
        sta     %o1, [%o0]ASI_MOD
        retl
        nop
        SET_SIZE(tsu_turn_cache_on)


	/*
	 * copied from uts/sun4m/ml/module_ross625_asm.s
	 * Sun's obp will turn on cache, the reason we turn on it again
	 * is for PFU.
	 */

	ENTRY(ross625_turn_cache_on)
	!
	! Turn off and clear icahce
	!
	rd	RT620_ICCR, %o0
	andn	%o0, RT620_ICCR_ICE, %o0
	wr	%o0, RT620_ICCR
	sta	%g0, [%g0]RT620_ASI_IC

	!
	! Turn on RT620 I-cache
	!
	or	%o0, RT620_ICCR_ICE, %o0
	wr	%o0, RT620_ICCR

	!
	! Check RT625 E-cache
	!
	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1
	btst	RT625_CTL_CE, %o1
	bnz	1f
	nop

	!
	! Clear the cache tags
	!
	set	4096, %o2		! cache entries
	set	64, %o3			! cache linesize
	mov	0, %o4
2:	sta	%g0, [%o4]RT625_ASI_CTAG
	subcc	%o2, 1, %o2
	bnz	2b
	add	%o4, %o3, %o4

	!
	! Turn on RT625
	!
	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1
	or	%o1, RT625_CTL_CE, %o1	! cache enable
	or	%o1, RT625_CTL_CM, %o1	! copyback mode
	sta	%o1, [%o0]ASI_MOD

1:
	retl
	nop
	SET_SIZE(ross625_turn_cache_on)

#endif		/* lint */
