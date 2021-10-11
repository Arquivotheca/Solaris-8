/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * assembly code support for modules based on the
 * Cypress CY7C604 or CY7C605 Cache Controller and
 * Memory Management Units.
 */

#pragma ident	"@(#)ross605_asm.s	1.21	99/04/13 SMI"

#if defined(lint)
#include <sys/types.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/machparam.h>
#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/psr.h>
#include <sys/trap.h>
#include <sys/devaddr.h>
#include <sys/async.h>

#if defined(lint)

/* ARGSUSED */
caddr_t
ross_mmu_getasyncflt(caddr_t afsrbuf)
{ return (caddr_t)0;}

#else	/* lint */

	.seg    ".text"
	.align	4

	!
	! This approach is based upon the latest errata 
	! (received 5/8/89) for "Sun-4M SYSTEM ARCHITECTURE"
	! which specifies that for Ross modules, in order to
	! fix a race condition, the AFSR must be read first,
	! and then the AFAR must only be read if the AFV bit 
	! of the AFSR is set.  Furthermore, the AFAR is
	! guaranteed not to go away until *it* is read.
	! Note that generic SRMMU behaves differently, in
	! that the AFAR must be read first, since reading
	! the AFSR unlocks these registers.
	!
	ENTRY(ross_mmu_getasyncflt)
	set	RMMU_AFS_REG, %o1
	lda	[%o1]ASI_MOD, %o1
	st	%o1, [%o0]
	btst	AFSREG_AFV, %o1
	bz	1f
	set	RMMU_AFA_REG, %o1
	lda	[%o1]ASI_MOD, %o1
	st	%o1, [%o0+4]
1:
	set	-1, %o1
	retl
	st	%o1, [%o0+8]
	SET_SIZE(ross_mmu_getasyncflt)

#endif	/* lint */

#if defined(lint)

int
ross_mmu_probe(void)
{return(0);}

#else	/* lint */

/*
 * Probe routine that does a tablewalk in software.
 * It always returns what the level 3 pte looks like, even if
 * the address is mapped by a level 2 or 1 pte.
 */
	ENTRY(ross_mmu_probe)
	and	%o0, MMU_PAGEMASK, %o2		! and off page offset
	set	RMMU_CTP_REG, %o3
	lda	[%o3]ASI_MOD, %o3		! ctx table ptr
	set	RMMU_CTX_REG, %o4
	lda	[%o4]ASI_MOD, %o4		! ctx
	sll	%o3, 4, %o3			! phys base of ctx table
	sll	%o4, 2, %o4
	add	%o3, %o4, %o3			! phys addr of level 0
	lda	[%o3]ASI_MEM, %o3
	and	%o3, 3, %o0
	cmp	%o0, MMU_ET_INVALID		! check for invalid
	be	probe_invalid
	srl	%o3, 2, %o3			! else it's a ptp
	sll	%o3, MMU_STD_PTPSHIFT, %o3	! phys base of level 1 table
	srl	%o2, MMU_STD_RGNSHIFT, %o4
	and	%o4, 0xFF, %o4			! level 1 index
	sll	%o4, 2, %o4
	add	%o3, %o4, %o3			! phys addr of level 1
	lda	[%o3]ASI_MEM, %o3
	and	%o3, 3, %o0
	cmp	%o0, MMU_ET_INVALID		! check for invalid
	be	probe_invalid
	cmp	%o0, MMU_ET_PTP			! check for ptp
	be	1f
	srl	%o2, MMU_STD_PAGESHIFT, %o4	! else it's a pte
	and	%o4, MMU_STD_FIRSTMASK, %o4	! rgn offset
	sll	%o4, 8, %o4			! shift into pfn position
	b	probe_valid
	add	%o3, %o4, %o0			! add it to pfn
1:
	srl	%o3, 2, %o3
	sll	%o3, MMU_STD_PTPSHIFT, %o3	! phys base of level 2 table
	srl	%o2, MMU_STD_SEGSHIFT, %o4
	and	%o4, 0x3F, %o4			! level 2 index
	sll	%o4, 2, %o4
	add	%o3, %o4, %o3			! phys addr of level 2
	lda	[%o3]ASI_MEM, %o3
	and	%o3, 3, %o0
	cmp	%o0, MMU_ET_INVALID		! check for invalid
	be	probe_invalid
	cmp	%o0, MMU_ET_PTP			! check for ptp
	be	1f
	srl	%o2, MMU_STD_PAGESHIFT, %o4	! else it's a pte
	and	%o4, MMU_STD_SECONDMASK, %o4	! seg offset
	sll	%o4, 8, %o4			! shift into pfn position
	b	probe_valid
	add	%o3, %o4, %o0			! add it to pfn
1:
	srl	%o3, 2, %o3
	sll	%o3, MMU_STD_PTPSHIFT, %o3	! phys base of level 3 table
	srl	%o2, MMU_STD_PAGESHIFT, %o4
	and	%o4, 0x3F, %o4			! level 3 index
	sll	%o4, 2, %o4
	add	%o3, %o4, %o3			! phys addr of level 3
	lda	[%o3]ASI_MEM, %o0
	cmp	%o0, 0
	be	probe_invalid
	nop
probe_valid:
	cmp	%o1, 0
	bne,a	1f				! if fsr not wanted, don't st
	st	%g0, [%o1]			! st 0 since it's valid
1:
	retl
	nop
probe_invalid:
	cmp	%o1, 0
	be	1f				! if fsr not wanted, don't st
	or	%o2, FT_ALL << 8, %o2
	lda	[%o2]ASI_FLPR, %g0		! probe all to set fsr
	set	RMMU_FSR_REG, %o2		! setup to clear fsr
	lda	[%o2]ASI_MOD, %o2		! clear fsr
	st	%o2, [%o1]			! return fsr
1:
	retl
	nop

#endif	/* lint */

#if defined(lint)

int
ross_mmu_ltic_ramcam(void)
{ return (0);}

#else	/* lint */

	!
	! Lock Translation In Cache
	!
	! This is the routine that does all the hard work for locking
	! translations in the cache; it allocates a slot in the TLB,
	! locks it down, and fills it with the supplied RAM/CAM info.
	!
	! Return value is "0" for success, or "-1" for error.
	!
	ENTRY(ross_mmu_ltic_ramcam)

! Reserve an entry in the TLB
	set	RMMU_TRCR, %o4
	lda	[%o4]ASI_MOD, %o2
	add	%o2, 1, %o2
	and	%o2, 0x3F, %o2		! only allow locking half the TLB
	cmp	%o2, 0x30		! too many locked?
	bge,a	9f			! if so,
	sub	%g0, 1, %o0		!   return an error.
	sll	%o2, 8, %o3		! set both RC and IRC
	or	%o3, %o2, %o3		! as per manual

! Locate the access port to the reserved entry
	sub	%o2, 1, %o2		! retrieve new reserved slot number
	sll	%o2, 3, %o2		! offset is eight bytes per entry

! Update the TLB, and flush stale entries.

! CAVEAT -- if we are locking in a translation
! that includes this executing text and it
! is at a different level from the translation
! currently being used to map this text, then
! we could get garbage for the text translation
! for the once cycle between updating the locked
! entry and flushing the old translation out
! of the cache. So, don't do it.

	sta	%o3, [%o4]ASI_MOD	! update TRCR
	set	FT_ALL<<8, %o4		! set up for flush
	stda	%o0, [%o2]ASI_DTLB	! add the locked entry
	sta	%g0, [%o4]ASI_FLPR	! flush unlocked TLB entries

	mov	%g0, %o0		! return success
9:
	retl			! <psr delay>
	nop			! <psr delay>
	SET_SIZE(ross_mmu_ltic_ramcam)

#endif	/* lint */


#if defined(lint)

/* ARGSUSED */
void
ross_cache_init(void)
{}

#else	/* lint */

/*
 * Virtual Address Cache routines.
 *
 *	Standard register allocation:
 *
 * %g1	scratchpad / jump vector / alt ctx reg ptr
 * %o0	virtual address			ctxflush puts context here
 * %o1	incoming size / loop counter
 * %o2	context number to borrow
 * %o3	saved context number
 * %o4	srmmu ctx reg ptr
 * %o5	saved psr
 * %o6	reserved (stack pointer)
 * %o7	reserved (return address)
 */

#define	CACHE_LINES	2048

#define	CACHE_LINESHFT	5
#define	CACHE_LINESZ	(1<<CACHE_LINESHFT)
#define	CACHE_LINEMASK	(CACHE_LINESZ-1)

#define	CACHE_BYTES	(CACHE_LINES<<CACHE_LINESHFT)

#define	MPTAG_OFFSET	0x40000

	.seg	".data"
	.align	4

.global	vac_copyback
.global	cache_mode


	.seg	".text"
	.align	4

	ENTRY_NP(ross_cache_init)

	! This code must be in assembly since the compiler might generate
	! loads or stores that result in cache inconistencies leading to
	! strange failures later on.

	set	RMMU_CTL_REG, %o5
	lda	[%o5]ASI_MOD, %o4	! read control reg
	set	vac_copyback, %o3
	ld	[%o3], %o2
	tst	%o2			! find out if copyback should be on
	bnz,a	1f
	or	%o4, CPU_CB, %o4	! yes, set it
	andn	%o4, CPU_CB, %o4	! no, clear it
1:
	sta	%o4, [%o5]ASI_MOD	! update control reg

	set	CACHE_BYTES, %o1	! cache bytes to init
	set	MPTAG_OFFSET, %o2	! offset for MPTAGs
2:
	deccc	CACHE_LINESZ, %o1
	sta	%g0, [%o1 + %o2]0xE	! clear MPTAG
	bne	2b
	sta	%g0, [%o1]0xE		! clear PVTAG

	! Cache is ready to go.  Loads and stores are now safe.

	lda	[%o5]ASI_MOD, %o4	! see if copyback is on (by testing
					! h/w since bit might be read-only)
	andcc	%o4, CPU_CB, %g0
	bz	3f
	nop

	set	cache, %o5
	ld	[%o5], %o4
	or	%o4, CACHE_WRITEBACK, %o4
	st	%o4, [%o5]
	set	wb_msg, %o4
	ba,a	4f

3:
	set	wt_msg, %o4
4:
	set	cache_mode, %o5
	st	%o4, [%o5]

	retl
	nop

wb_msg:	.asciz	"write back"
wt_msg:	.asciz	"write through"
	.align	4

	SET_SIZE(ross_cache_init)

#endif	/* lint */

#if defined(lint)

void
ross_vac_usrflush(void)
{}

#else	/* lint */

#define	TAG_OFFSET	0x40000		/* Cy7C605 MPtag offset */
#define	VM_BITS		0x60		/* C77C605 MPtag M and V bits */

	!
	! ross_vac_usrflush: flush all user data from the cache
	!
	ENTRY(ross_vac_usrflush)
        set     CACHE_LINES*CACHE_LINESZ, %o1
1:      deccc   CACHE_LINESZ, %o1
        bne     1b
        sta     %g0, [%o1]ASI_FCU
        retl
        nop
	SET_SIZE(ross_vac_usrflush)

#endif	/* lint */

#if defined(lint)

#else	/* lint */

/*
 * BORROW_CONTEXT: temporarily set the context number
 * to that given in %o2. See above for register assignments.
 * NOTE: all interrupts below level 15 are disabled until
 * the next RESTORE_CONTEXT. Do we want to disable traps
 * entirely, to prevent L15s from being serviced in a borrowed
 * context number? (trying this out now)
 */

#define	BORROW_CONTEXT			\
	mov	%psr, %o5;		\
	andn	%o5, PSR_ET, %o4;	\
	mov	%o4, %psr;		\
	nop ;	nop;			\
\
	set	RMMU_CTP_REG, %o4;	\
	lda	[%o4]ASI_MOD, %o4;	\
	sll	%o2, 2, %o3;		\
	sll	%o4, 4, %o4;		\
	add	%o4, %o3, %o4;		\
	lda	[%o4]ASI_MEM, %o4;	\
	and	%o4, 3, %o4;		\
	subcc	%o4, MMU_ET_PTP, %g0;	\
\
	set	RMMU_CTX_REG, %o4;	\
	bne	1f;			\
	lda	[%o4]ASI_MOD, %o3;	\
	sta	%o2, [%o4]ASI_MOD;	\
1:
/*
 * RESTORE_CONTEXT: back out from whatever BORROW_CONTEXT did.
 * NOTE: asssumes two cycles of PSR DELAY follow the macro;
 * currently, all uses are followed by "retl ; nop".
 */
#define	RESTORE_CONTEXT			\
	sta	%o3, [%o4]ASI_MOD;	\
	mov	%o5, %psr;		\
	nop;nop

#endif	/* lint */


#if defined(lint)

/* ARGSUSED */
void
ross_vac_flush(caddr_t va, int sz)
{}

#else	/* lint */

	!
	! flush data in this range from the cache
	!
	ENTRY(ross_vac_flush)
        and     %o0, CACHE_LINEMASK, %g1        ! figure align error on start
        sub     %o0, %g1, %o0                   ! push start back that much
        add     %o1, %g1, %o1                   ! add it to size

        BORROW_CONTEXT

1:	deccc   CACHE_LINESZ, %o1
        sta     %g0, [%o0]ASI_FCP
        bcc     1b
        inc     CACHE_LINESZ, %o0
        RESTORE_CONTEXT
        retl
        nop
	SET_SIZE(ross_vac_flush)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
ross_turn_cache_on(void)
{}

#else	/* lint */

	ENTRY_NP(ross_turn_cache_on)
	set	0x100, %o2			! cache enable
	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1
	or	%o1, %o2, %o1
	sta	%o1, [%o0]ASI_MOD
	retl
	nop
	SET_SIZE(ross_turn_cache_on)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
int
check_cache(void)
{ return (0);}

#else	/* lint */

	ENTRY(check_cache)
	set	RMMU_CTL_REG, %o0
	retl
	lda	[%o0]ASI_MOD, %o0
	SET_SIZE(check_cache)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
ross_vac_allflush(u_int flags)
{}

#else	/* lint */

	ENTRY(ross_vac_allflush)

	! if FL_TLB bit is not set, skip flushing the TLB
	andcc	%o0, FL_TLB, %o0
	bz	0f
	nop

	or      %g0, FT_ALL<<8, %o0             ! flush entire tlb
	sta     %g0, [%o0]ASI_FLPR              ! do the flush
0:
	set     CACHE_BYTES, %o1                ! look at whole cache
	set     TAG_OFFSET, %o2                 ! MPtag ofset
	set	_start+CACHE_BYTES, %o5		! base of safe area to read
	b       2f                              ! enter loop at bottom test
	nop
1:               		                ! top of loop: compare, flush
	andcc   %o4, VM_BITS, %o3               ! test valid and modified
	cmp     %o3, VM_BITS                    ! are both on?
	beq,a   2f                              ! if we matched,
	ld      [%o5+%o1], %g0                  !   force cache line replace
2:                              		! end of loop: again?
	deccc   CACHE_LINESZ, %o1               ! any more to do?
	bcc,a   1b                              ! if so, loop back
	lda     [%o2+%o1]ASI_DCT, %o4           ! read corresponding MPtag

	retl
	nop
	SET_SIZE(ross_vac_allflush)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
ross_vac_ctxflush(u_int cxn, u_int flags)
{}

#else	/* lint */

	ENTRY(ross_vac_ctxflush)

	mov	%o0, %o2
	BORROW_CONTEXT

	! flush TLB if flags has FL_TLB bit set
	andcc	%o1, FL_TLB,	%o1
	bz	0f
	nop

	! First flush the TLB
	set     FT_CTX<<8, %o0
	sta     %g0, [%o0]ASI_FLPR                      ! do the flush
0:
	! Flush the VAC if it is on.
	sethi   %hi(vac), %g1
	ld      [%g1 + %lo(vac)], %g1
	tst     %g1
	bz	1f
	nop
	! Now flush the VAC
	mov	0, %o0
	set     CACHE_LINES, %o1
2:	deccc   %o1
	sta     %g0, [%o0]ASI_FCC
	bne	2b
	inc     CACHE_LINESZ, %o0
1:
	subcc	%o2, %o3, %g0
	bne	1f
	nop
	mov	KCONTEXT, %o3
1:
	RESTORE_CONTEXT
	retl
	nop	! psr delay
	SET_SIZE(ross_vac_ctxflush)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
ross_vac_rgnflush(caddr_t addr, u_int cxn, u_int flags)
{}

#else	/* lint */

	ENTRY(ross_vac_rgnflush)

	mov	%o2, %g2		! save flags for later use
	mov	%o1, %o2

	BORROW_CONTEXT

	! if FL_TLB bit is not set, skip flushing the TLB
	andcc	%g2, FL_TLB, %g2
	bz	0f

	! Save %o0 in %o1 for the VAC stuff below.
	mov	%o0, %o1
	! First flush the TLB
	or      %o0, FT_RGN<<8, %o0
	sta     %g0, [%o0]ASI_FLPR                      ! do the flush
0:
	! Flush the VAC if it is on.
	sethi   %hi(vac), %g1
	ld      [%g1 + %lo(vac)], %g1
	tst	%g1
	bz	1f
	nop
	! Restore %o0
	mov	%o1, %o0
	! Now flush the VAC
	set     CACHE_LINES, %o1
2:	deccc   %o1
	sta     %g0, [%o0]ASI_FCR
	bne	2b
	inc     CACHE_LINESZ, %o0
1:
	RESTORE_CONTEXT
	retl
	nop
	SET_SIZE(ross_vac_rgnflush)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
ross_vac_segflush(caddr_t addr, u_int cxn, u_int flags)
{}

#else	/* lint */

	ENTRY(ross_vac_segflush)

	mov	%o2, %g2		! save flags for later use
	mov	%o1, %o2

	BORROW_CONTEXT

	! if FL_TLB bit is not set, skip flushing the TLB
	andcc	%g2, FL_TLB, %g2
	bz	0f

        ! Save %o0 in %o1 for the VAC stuff below. 
        mov     %o0, %o1
	! First flush the TLB
	or      %o0, FT_SEG<<8, %o0
	sta     %g0, [%o0]ASI_FLPR                      ! do the flush
0:
	! Flush the VAC if it is on.
	sethi   %hi(vac), %g1
	ld      [%g1 + %lo(vac)], %g1
	tst	%g1
	bz	1f
	nop
	! Restore %o0
	mov	%o1, %o0

	! Now flush the VAC
	set     CACHE_LINES, %o1
2:	deccc   %o1
        sta     %g0, [%o0]ASI_FCS
	bne	2b
        inc     CACHE_LINESZ, %o0
1:
        RESTORE_CONTEXT
        retl
        nop
	SET_SIZE(ross_vac_segflush)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
ross_vac_pageflush(caddr_t vaddr, int cxn, u_int flags)
{}

#else	/* lint */

	ENTRY(ross_vac_pageflush)

	mov	%o2, %g2		! save flags for later use
	mov	%o1, %o2

        BORROW_CONTEXT

	! if FL_TLB bit is not set, skip flushing the TLB
	andcc	%g2, FL_TLB, %g2
	bz	0f

        ! Save %o0 in %o1 for the VAC stuff below.
        mov     %o0, %o1
        ! First flush the TLB
        or      %o0, FT_PAGE<<8, %o0
        sta     %g0, [%o0]ASI_FLPR                      ! do the flush

	! Flush the VAC if it is on.
0:	sethi   %hi(vac), %g1
	ld      [%g1 + %lo(vac)], %g1
	tst	%g1
	bz	1f
	nop
	! Restore %o0
	mov	%o1, %o0
	! Now flush the VAC
	set     PAGESIZE, %o1
2:	deccc   CACHE_LINESZ, %o1
        sta     %g0, [%o0]ASI_FCP
        bne     2b
	inc     CACHE_LINESZ, %o0
1:
        RESTORE_CONTEXT
        retl
        nop
	SET_SIZE(ross_vac_pageflush)

#endif 	/* lint */
