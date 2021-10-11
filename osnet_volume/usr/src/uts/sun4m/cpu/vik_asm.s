/*
 * Copyright (c) 1990-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vik_asm.s	1.21	98/02/01 SMI"

/*
 * assembly code support for modules based on the TI VIKING chip set.
 */

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
#include <sys/machtrap.h>
#include <sys/intreg.h>
#include <sys/x_call.h>

#if defined(lint)

void
vik_mmu_getasyncflt(void)
{}

int
vik_mmu_probe(void)
{return(0);}

void
bpt_reg(void)
{}

void
vik_pac_init(void)
{}

#ifdef notdef
void
vik_turn_cache_on(void)
{}
#endif

#else	/* lint */

	ENTRY_NP(vik_mmu_getasyncflt)
! %%%	need to work out how to talk with
! %%%	viking write pipe.
! get mfsr/mfar registers
! Viking/NE: a store buffer error will cause both a trap 0x2b and
! 	     a broadcast l15 interrupt. The trap will be taken first, but
! 	     afterwards there will be a pending l15 interrupt waiting for
! 	     this module. 
! Viking/E:  async fault status is in the mxcc error register
! 
! %o0    = MFSR
! %o0+4  = MFAR
! %o0+8  = -1 in Mbus mode;
!	 = Error register <63:32>
! %o0+12 = Error register <31:0>
!

	set	RMMU_FAV_REG, %o1		! MFSR and MFAR
	lda	[%o1]ASI_MOD, %o1
	st	%o1, [%o0+4]
	set	RMMU_FSR_REG, %o1
	lda	[%o1]ASI_MOD, %o1
	st	%o1, [%o0]

	sethi	%hi(mxcc), %o1
	ld	[%o1+%lo(mxcc)], %o1
	cmp	%o1, %g0
	be	1f
	mov	-1, %o4

	set	MXCC_ERROR, %o1
	ldda	[%o1]ASI_MXCC, %o2		! get 64-bit error register
	st	%o3, [%o0+12]			! %o3 has PA<31 : 0>
	mov	%o2, %o4 			! %o2 (high nibble) status
	sub	%g0, 1, %o2			! set 0xFFFFFFFF to %o2
	sub	%g0, 1, %o3			! set 0xFFFFFFFF to %o3
	stda	%o2, [%o1]ASI_MXCC		! clear error register
1:
	retl
	st	%o4, [%o0+8]
	SET_SIZE(vik_mmu_getasyncflt)


/*
 * Probe routine that takes advantage of multilevel probes.
 * It always returns what the level 3 pte looks like, even if
 * the address is mapped by a level 2 or 1 pte.
 */
	ENTRY(vik_mmu_probe)
	and	%o0, MMU_PAGEMASK, %o2		! and off page offset bits
	or	%o2, FT_ALL << 8, %o3
	lda	[%o3]ASI_FLPR, %o0		! probe all
	cmp	%o0, 0				! if invalid, we're done
	be	probe_done
	or	%o2, FT_RGN << 8, %o3		! probe rgn
	lda	[%o3]ASI_FLPR, %o0
	and	%o0, 3, %o3
	cmp	%o3, MMU_ET_PTE
	bne	1f				! branch if not a pte
	srl	%o2, MMU_STD_PAGESHIFT, %o3	! figure offset into rgn
	and	%o3, MMU_STD_FIRSTMASK, %o3
	sll	%o3, 8, %o3			! shift into pfn position
	b	probe_done
	add	%o0, %o3, %o0			! add it to pfn
1:
	or	%o2, FT_SEG << 8, %o3
	lda	[%o3]ASI_FLPR, %o0		! probe seg
	and	%o0, 3, %o3
	cmp	%o3, MMU_ET_PTE
	bne	1f
	srl	%o2, MMU_STD_PAGESHIFT, %o3	! figure offset into seg
	and	%o3, MMU_STD_SECONDMASK, %o3
	sll	%o3, 8, %o3			! shift into pfn position
	b	probe_done
	add	%o0, %o3, %o0			! add it to pfn
1:
	or	%o2, FT_PAGE << 8, %o3
	lda	[%o3]ASI_FLPR, %o0		! probe page
probe_done:
	set	RMMU_FSR_REG, %o2		! setup to clear fsr
	cmp	%o1, 0
	be	1f				! if fsr not wanted, don't st
	lda	[%o2]ASI_MOD, %o2		! clear fsr
	st	%o2, [%o1]			! return fsr
1:
	retl
	nop
	SET_SIZE(vik_mmu_probe)

/*
 * Breakpoint register
 * Set or unset bits
 */
	ENTRY(bpt_reg)
	lda	[%g0]ASI_MBAR, %o2
	or	%o0, %o2, %o0
	andn	%o0, %o1, %o0

	! Due to a Viking bug:
	! A cache snoop at the same time as a store to asi's 0x40-0x4c
	! may result in the snoop data being written to the destination
	! register instead of the intended data.  The workaround is
	! to re-read the register and re-write the data if there's
	! a mismatch (repeat as necessary).
1:
	sta	%o0, [%g0]ASI_MBAR
	lda	[%g0]ASI_MBAR, %o3
	cmp	%o3, %o0
	bne	1b
	nop

	retl
	nop
	SET_SIZE(bpt_reg)

	ENTRY(vik_mmu_chk_wdreset)
	set     RMMU_FSR_REG, %o0
	lda     [%o0]ASI_MOD, %o0
	set	MMU_SFSR_EM, %o1
	retl
	and	%o0, %o1, %o0
	SET_SIZE(vik_mmu_chk_wdreset)

/*
 *	Note: it is extremely
 *	rare to need to flush physical caches,
 *	like just when going from cacheable to
 *	noncacheable. It is also fairly hard
 *	to flush the viking cache. However, we do have to flush the
 *	caches for the sake of SX graphics accelerator.
 */

	ENTRY(vik_pac_init_asm)
/*
 *	Don't clear the tags here.  The viking prom will clear the cache
 *	tags and leave the cache on.
 *	If we clear the tags here, all hell will break loose since the
 *	proms cache themselves.
 *
 *	set	0x80000000, %g1	
 *	sta	%g1, [%g1]ASI_ICFCLR	! clear i-$ lock bits
 *	sta	%g0, [%g0]ASI_ICFCLR	! clear i-$ valid bits 
 *	sta	%g1, [%g1]ASI_DCFCLR	! clear d-$ lock bits
 *	sta	%g0, [%g0]ASI_DCFCLR	! clear d-$ valid bits
 */

	set	RMMU_CTL_REG, %o5	! initialize viking
	lda	[%o5]ASI_MOD, %o4	! read mcr
	andn	%o4, %o0, %o4		! turn some bits off
	or	%o4, %o1, %o4		! turn some bits on
	sta	%o4, [%o5]ASI_MOD	! update mcr
	retl
	nop
	SET_SIZE(vik_pac_init_asm)


/* %%% TODO: initialize MXCC
 *		- write 0xFFFFFFFFFFFFFFFF to mxcc error register.  individual
 *		  bits in this register are write-1-to-clear.  This register 
 *		  is not affected by system reset.
 *		- initialize MXCC control register
 *		  CE <bit 2>: E$ enable.
 *		  PE <bit 3>: Parity enable.
 *		  MC <bit 4>: Multiple command enable
 *		  PF <bit 5>: Prefetch enable
 *		  RC <bit 9>: Read reference count only
 */
	ENTRY(vik_mxcc_init_asm)	! (clrbits, setbits)
	set	MXCC_ERROR, %o4
	sub	%g0, 1, %o2		! set 0xFFFFFFFFFFFFFFFF to %o2 and %o3
	sub	%g0, 1, %o3		! set 0xFFFFFFFFFFFFFFFF to %o2 and %o3
	stda	%o2, [%o4]ASI_MXCC

	set	MXCC_CNTL, %o4
	lda	[%o4]ASI_MXCC, %o5	! read mxcc control reg
	andn	%o5, %o0, %o5		! turn some bits off
	or	%o5, %o1, %o5		! turn some bits on
	retl
	sta	%o5, [%o4]ASI_MXCC	! update mxcc control reg
	SET_SIZE(vik_mxcc_init_asm)

	ENTRY(vik_turn_cache_on)
! %%% what do we stuff in here?
!     do we need to turn on MXCC here?
	retl
	nop
	SET_SIZE(vik_turn_cache_on)
#ifdef netdef
	set	CACHE_VIK_ON, %o2		!MBus mode
	set	RMMU_CTL_REG, %o0
	lda	[%o0]ASI_MOD, %o1
	andcc	%o1, CPU_VIK_MB, %g0
	bnz	1f				!MBus mode
	nop
	set	CACHE_VIK_ON_E, %o2		!CC mode
1:	or	%o1, %o2, %o1
	retl
	sta	%o1, [%o0]ASI_MOD
	SET_SIZE(vik_turn_cache_on)

#endif

#endif	/* lint */


#ifdef lint

int
mxcc_pac_parity_chk_dis(void)
{return(0);}

#else	/* lint */

	ENTRY_NP(mxcc_pac_parity_chk_dis)
	set	MMU_SFSR_UD, %o3
	btst	%o3, %o0
	bnz	1f
	set	MMU_SFSR_P, %o4
	btst	%o4, %o0
	bnz	1f
	set	MXCC_ERR_CP, %o5
	btst	%o5, %o1
	bnz	1f

	nop				! nop REQUIRED to meet non impl-depend
	retl				! handling of DCTI rules.  See
	mov	%g0, %o0		! SPARC arch V8 fig. 5-11, 5-12
1:
	set	MXCC_CNTL, %o1
	lda	[%o1]ASI_MXCC, %o2	! read mxcc control reg
	set	MXCC_PE, %o3
	andn	%o2, %o3, %o2

	set	RMMU_CTL_REG, %o3	! initialize viking
	lda	[%o3]ASI_MOD, %o4	! read control register
	set	CPU_VIK_PE, %o5
	andn	%o4, %o5, %o4

	sta	%o2, [%o1]ASI_MXCC	! Turn off E$ parity bit
	sta	%o4, [%o3]ASI_MOD	! Turn off Viking parity bit

	retl
	mov	0x1, %o0
	SET_SIZE(mxcc_pac_parity_chk_dis)

#endif	/* lint */


#ifdef	lint
/*ARGSUSED*/
void
vik_mxcc_pageflush(pfnum)
	u_int pfnum;
{}

#else	/* lint */

	ENTRY_NP(vik_mxcc_pageflush)		! flush page from MXCC
	/*
	 *  %o0 has the PFN of the page to flush.
	 *  The algorithm uses the MXCC's stream copy registers
	 *  to read the data and write it back to memory.  This will cause the
	 *  MXCC to issue a CWI to all other caches.
	 *  Note that we must do this for every subblock, not just every
	 *  line in the MXCC.
	 *  Also note that this will only flush paddrs's with PA<35:32> = 0.
	 *  Full 36-bit flushing is left as an exercise to the reader.
	 */

	set	MXCC_STRM_SRC, %o4
	set	MXCC_STRM_DEST, %o5
	mov	1 << 4, %o2			! enable Cacheability (bit 36)
	sll	%o0, MMU_PAGESHIFT, %o3		

1:
	/* Read the data into the Stream Data Register */
	stda	%o2, [%o4]ASI_MXCC	
	/* and write it back from whence it came */
	stda	%o2, [%o5]ASI_MXCC	

	add	%o3, MXCC_STRM_SIZE, %o3	! should this be a variable?
	andcc	%o3, MMU_PAGEOFFSET, %g0	! done?
	bnz	1b
	nop

pf_wait_dst:
	ldda	[%o5]ASI_MXCC, %o2		! wait for RDY bit in dest reg
	cmp	%o2, %g0
	bge	pf_wait_dst
	nop

	retl
	clr	%o0
	
	SET_SIZE(vik_mxcc_pageflush)
#endif lint

#ifdef lint
/*ARGSUSED*/
void
vik_mxcc_checkpfnum(u_int pfnum)
{}
#else 	/* lint */

	ENTRY_NP(vik_mxcc_checkpfnum)
/*
 *  Now we verify that our job is done right.
 */
	!
	! set up to check MXCC tags
	! %o0	target pfn to flush
	! %o1	Saved PSR
	! %o2	scratch
	! %o3 	scratch
	! %o4	target tag to search for
	! %o5 	MXCC line size
	!
	sethi	%hi(mxcc_linesize), %o5	
	ld	[%o5 + %lo(mxcc_linesize)], %o5
	sethi	%hi(mxcc_tagblockmask), %o2
	ld	[%o2 + %lo(mxcc_tagblockmask)], %o2

	sll	%o0, MMU_PAGESHIFT, %o3		! %o3 has physaddr

	/* Identify which block we are to look for */
	/* We only need to look at PA<31:19> */
	srl	%o3, 19, %o4

	and	%o3, %o2, %o2			! tag address =
	set	MXCC_TAGSADDR, %o3		!  MXCC_TAGSADDR |
	or	%o3, %o2, %o0			!  (physaddr & blkmask)

ecache_top:
	ldda	[%o0]ASI_MXCC, %o2		! load tag
	/*
	 *  Note:  since we already decided to work
	 *  with only 32 bits, we don't look at PA<35-32> in %o2
	 */
	srl	%o3, 19, %o2			! PA is in bits 19-31 of %o3

	cmp	%o2, %o4			! match our target paddr?
	bne	next_eline			! no, goto next line
	nop

	set	MXCC_TAGSVALID, %o2
	andcc	%o3, %o2, %g0			! any blocks in line valid?
	bz	next_eline			! no, goto next line
	nop

	set	MXCC_TAGSOWNED, %o2
	andcc	%o3, %o2, %g0			! any blocks in line owned?
	bz	next_eline			! no, goto next line
	nop

	ba	eflush_fail
	mov	-1, %o0

next_eline:
	add	%o0, %o5, %o0			! setup for next line in page
	andcc	%o0, MMU_PAGEOFFSET, %g0	! done?
	bne	ecache_top
	nop

	mov	%g0, %o0

eflush_fail:

	retl
	nop

	SET_SIZE(vik_mxcc_checkpfnum)

#endif	/* lint */

#ifdef	lint
/*ARGSUSED*/
void
vik_pac_pageflush(pfnum)
	u_int pfnum;
{ }

#else	/* lint */
	ENTRY_NP(vik_pac_pageflush)
	! Flush page from Viking I and D Caches.
	! This routine is invoked for Viking/No MXCC machines.
	! This routine will only ever be run on a UP machine.
	! We need not splhi since we are in the process of setting
	! up a translation for the page and we already hold the
	! hat_mutex.
	!
	! setup to check dcache tags
	!
	! %g1 virtual address of alias to trigger flush
	! %o4 & %o5 ptag of line
	! %o3 ptag to flush
	! %o2 current line
	! %o1 current set
	! %o0 tag address
	!
	! We use pp_base as an alias for forcing replacement of dirty lines. 
	! cannnot use physical addresses for
	! an alias because then we have to use the MMU bypass ASI (ASI_MEM)
	! and to indicate that it is a cacheable access (it has to be because
	! we are forcing line replacement) we have to set the Viking MMU
	! control register Alternate Cacheable (AC) bit. Doing this seems to
	! cause an almost immediate watchdog. 

1:
	mov	%o0, %o3		! The physical page number
	set	pp_base, %g1		! Form alias for flushing
	ld	[%g1], %g1
	andn	%g1, MMU_PAGEOFFSET, %g1

	clr	%o1			! set = 0
	clr	%o2			! line = 0

dcache_top:

	set	VIK_PTAGADDR, %o0	! ptagaddr = VIK_PTAGADDR |
	sll	%o1, 5, %o4		!  set << 5 | line << 26
	or	%o4, %o0, %o0
	sll	%o2, 26, %o4
	or	%o4, %o0, %o0
	ldda	[%o0]ASI_DCT, %o4	! load ptag (%o5 = paddr %o4 = tag info)
	cmp	%o5, %o3		! match?
	bne	next_dline		! no, check next line in same set
	nop

	set	VIK_PTAGVALID, %o5
	btst	%o5, %o4		! line valid?
	bz	next_dline		! no, check next line
	nop

	set	VIK_PTAGDIRTY, %o5
	btst	%o5, %o4		! line dirty?
	bz	next_dline		! no, just kill valid bit
	nop

	!
	! Force writeback of all lines in this set by doing 8 loads
	! to it.  The 1st 4 will fill the set, the next 4 will replace
	! the previous 4.  The result will be 4 clean lines.  Then invalidate
	! all the lines in case the lines we loaded shouldn't be cached.
	!
	sll	%o2, 26, %o4
	xor	%o0, %o4, %o0		! reset ptagaddr to line 0
	sll	%o1, 5, %o4		! form alias for current set in %o4
	add	%o4, %g1, %o4
	clr	%o2			! use %o2 as counter

2:
	ld	[%o4], %o5
	set	MMU_PAGESIZE, %o5	! Same offset, 8 different pages
	add	%o4, %o5, %o4
	cmp	%o2, 7
	bne,a	2b
	inc	%o2			! DELAY

3:
	ba	next_dset
	nop

next_dline:
	cmp	%o2, 3			! last line?
	bne,a	dcache_top
	inc	%o2			! DELAY
next_dset:
	clr	%o2			! reset line #
	cmp	%o1, 127		! last set?
	bne,a	dcache_top
	inc	%o1			! DELAY

	/* 
	 *  There is no need to flush the Icache, since it is
	 *  always consistent with memory.
	 */
	retl
	nop
	SET_SIZE(vik_pac_pageflush)

#endif	/* lint */


#ifdef	lint
/*ARGSUSED*/
void
vik_pac_checkpfnum(pfnum)
	u_int pfnum;
{ }

#else	/* lint */
	ENTRY_NP(vik_pac_checkpfnum)
	!
	! Check if pfnum is in the cache, return 1 for cache-hit, 0 for 
	! cache-miss. This routine is invoked for Viking/No MXCC machines.
	!
	! setup to check dcache tags
	!
	! %g1 physical alias to trigger flush
	! %o4 & %o5 ptag of line
	! %o3 ptag to flush
	! %o2 current line
	! %o1 current set
	! %o0 tag address
	!
1:
	mov	%o0, %o3		! physical page frame number
	clr	%o2			! line = 0
	clr	%o1			! set = 0
dc_top:
	set	VIK_PTAGADDR, %o0	! ptagaddr = VIK_PTAGADDR |
	sll	%o1, 5, %o4		!  set << 5 | line << 26
	or	%o4, %o0, %o0
	sll	%o2, 26, %o4
	or	%o4, %o0, %o0

	ldda	[%o0]ASI_DCT, %o4	! load ptag (%o5 = paddr %o4 = tag info)
	cmp	%o5, %o3		! match?
	bne	dc_nextline		! no, check next line in same set
	nop

	set	VIK_PTAGVALID, %o5
	btst	%o5, %o4		! line valid?
	bz	dc_nextline		! no, check next line
	nop
	retl
	set	1, %o0			! return 1
dc_nextline:
	cmp	%o2, 3			! last line?
	bne,a	dc_top
	inc	%o2			! DELAY
dc_nextset:
	clr	%o2			! reset line #
	cmp	%o1, 127		! last set?
	bne,a	dc_top
	inc	%o1			! DELAY
	!
	! setup to check icache tags
	!
	! this is done just like the dcache check except we don't have to
	! worry about dirty lines
	!
	clr	%o2			! line = 0
	clr	%o1			! set = 0
1:
	set	VIK_PTAGADDR, %o0	! ptagaddr = VIK_PTAGADDR |
	sll	%o1, 5, %o4		!  set << 5 | line << 26
	or	%o4, %o0, %o0
	sll	%o2, 26, %o4
	or	%o4, %o0, %o0
	ldda	[%o0]ASI_ICT, %o4	! load ptag
	cmp	%o5, %o3		! match?
	bne	ic_nextline		! no, check next line in same set
	nop
	set	VIK_PTAGVALID, %o5
	btst	%o5, %o4		! line valid?
	bz	ic_nextline		! no, check next line
	nop
	retl
	set	1, %o0
ic_nextline:
	cmp	%o2, 4			! last line?
	bne,a	1b
	inc	%o2			! DELAY
ic_nextset:
	clr	%o2			! reset line #
	cmp	%o1, 127		! last set?
	bne,a	1b
	inc	%o1			! DELAY
out1:
	retl
	set 	0, %o0			!
	SET_SIZE(vik_pac_checkpfnum)

#endif	/* lint */


/*
 * Code to workaround bug #1137125 in the Viking processor. The code
 * disables traps, replaces all lines of the DCache, locks lines 1-3 of each
 * set (Cannot lock line 0), invalidates lines 1-3 of each set and zero fills
 * lines 1-3 of each set, enables traps and returns.
 */

#if defined(lint)
void
vik_1137125_wa(void)
{ }
#else	/* lint */
	ENTRY_NP(vik_1137125_wa)
	save	%sp, -SA(MINFRAME), %sp

	mov	%psr, %g1
	andn	%g1, PSR_ET, %g2	! disable traps
	mov	%g2, %psr
	nop; nop; nop;

	! Quick'n'dirty displacement flush of dcache (required for Viking-only)
	clr	%o0
	set	_start, %o1
	set	0x8000, %o3
	mov	%o1, %o2
1:
	ld	[%o2], %g0
	add	%o0, 32, %o0
	cmp	%o0, %o3
	ble	1b
	add	%o1, %o0, %o2

	! Start mucking with tags
	!
	! Register usage:
	!
	!	%i0: increment to get to the next line in the set (line number
	!	     is set in bits <26-27> of the PTAG.
	!	%i3: maximum line number (line 3)
	! 	%i5: the Viking STAG address format
	! 	%i4: the Viking PTAG address format
	! 	%l2: temporary holding register for STAG or PTAG address format.
	!	     encoding current set #, line # in the set and double word
	!	     in the line
	!	%o0, %o1 contain data to be written to STAG (0xe)
	!	%o2, %o3 contain data to be written to PTAG (0)
	!
	!	The code works like this
	!	disable_traps;
	!	replace-lines-in the dcache (to handle Viking/No ECache)
	!	for (set = 0; set < 128; set++) {
	!		lock lines 1-3;
	!		for (line = 1; line < 4; line ++) {
	!			invalidate line;
	!			for (dbl_word = 0; dbl_word < 4; dbl_word++)
	!				zero fill double word;
	!		}
	!	}
	!	enable traps;
	!	return;

	sethi	%hi(VIK_STAGADDR), %i5
	mov	0x0, %o0		! upper 32-bits stag data
	set	0xe, %o1		! lower 32-bits stag data
	sethi	%hi(VIK_PTAGADDR), %i4

	mov	0, %i2
	or	%i2, %i5, %l2		! beginning stag addr
vhwb_nextset:
	stda	%o0, [%l2]ASI_DCT	! set stag to 0xe (lock lines 1-3)

	sethi	%hi(0x04000000),%i0	! line 1 << 26
	sethi	%hi(0x0c000000),%i3	! line 3 << 26
	or	%i0, %i4, %l2
vhwb_nextline:
	or	%i2, %l2, %l2
	mov	0, %o2
	mov	0, %o3
	stda	%o2, [%l2]ASI_DCT	! clear ptag (invalidate line)

	! zero fill the 4 double words of the current line

	mov	0, %l3			! start with double word 0
	or	%i2, %i0, %i1		! Save of a copy of current PTAG
	sll	%l3, 3, %l2		! in %i1


vhwb_nextdword:
	or	%i1, %l2, %l2		! Add in double word to the PTAG
	mov	0, %o2
	mov	0, %o3
	stda	%o2, [%l2]ASI_DCD	! clear dword
	add	%l3, 1, %l3		! increment double word#
	cmp	%l3, 3
	ble	vhwb_nextdword
	sll	%l3, 3, %l2

	sethi	%hi(0x04000000),%l2	! 1 << 26
	add	%i0, %l2, %i0		! increment line#
	cmp	%i0, %i3		! %i3 = maxline
	ble,a	vhwb_nextline
	or	%i0, %i4, %l2		! Delay: Increment line # in
					! PTAG

	add	%i2, 32, %i2		! increment set#
	cmp	%i2, 4064		! Done locking all sets?
	ble	vhwb_nextset		! No, lock lines in next set
	or	%i2, %i5, %l2		! Delay: Increment set # in
					! PTAG

	mov	%g1, %psr		! Enable traps and return
	nop; nop; nop;

	ret
	restore
	SET_SIZE(vik_1137125_wa)

#endif /* lint */

#if defined(lint)
/*
 * This routine is only needed because we have to check whether
 * or not the SuperSPARC PTP2 needs to be worked around.  Instead
 * of modifying the SRMMU code we interpose this function.  If
 * the workaround is needed it calls srmmu_mmu_flushctx, otherwise
 * it calls srmmu_mmu_flushrgn.  See module_vik.c for a description
 * of the PTP2 bug.
 */
/*ARGSUSED*/
void
vik_mmu_flushrgn(caddr_t addr, u_int ctx)
{ }
#else	/* lint */
	ENTRY_NP(vik_mmu_flushrgn)
	sethi	%hi(viking_ptp2_bug), %o2	! is ptp2 workaround...
	ld	[%o2 + %lo(viking_ptp2_bug)], %o2 ! ... needed?
	orcc    %g0, %o2, %g0
	be	srmmu_mmu_flushrgn		! no, call flushrgn
	nop
	mov	%o1, %o0			! massage args for flushctx
	b	srmmu_mmu_flushctx
	nop
	SET_SIZE(vik_mmu_flushrgn)
#endif	/* lint */


/*
 * vik_xcall_medpri - Viking tlb_flush cross-call: handles
 * medium priority (level13) cross-calls in the trap window.
 */
#if defined(lint)
void
vik_xcall_medpri(void)
{ }
#else /* lint */
	ENTRY_NP(vik_xcall_medpri)

	/*
	 * NOTE: traps disabled for the entire function
	 *
	 * top of function register usage
	 * %l7 = cpu's interrupt reg pointer
	 * %l6 = scratch
	 * %l5 = cpu index (word offset) then xc_mboxes
	 * %l4 = scratch
	 * %l3 = scratch then cpu pointer
	 * %l2 = saved npc
	 * %l1 = saved pc
	 * %l0 = saved psr
	 */

	CPU_INDEX(%l5)			! get CPU number in l5
	sll	%l5, 2, %l5		! make word offset
	set	v_interrupt_addr, %l7	! base of #cpu ptrs to interrupt regs
	add	%l7, %l5, %l7		! ptr to our ptr
	ld	[%l7], %l7		! %l4 = &cpu's intr pending reg
	ld	[%l7], %l6		! %l4 = interrupts pending
	set	IR_SOFT_INT(13), %l4	! Soft interrupts are in bits <17-31>
	andcc	%l4, %l6, %g0		! Is soft level 13 interrupt pending?
	bz,a	interrupt_prologue	! No, let interrupt_prologue service it
	mov	13, %l4			! pass the intr # in %l4

	!! Check for x-call
	set	cpu, %l4
	ld	[%l4 + %l5], %l3	! read cpu[cpuindex]

        ! if (cpup->cpu_m.xc_pend[X_CALL_MEDPRI] == 0)
	!    goto interrupt_prologue
	ld	[%l3 + XC_PEND_MEDPRI], %l4
	tst	%l4
	bz,a	interrupt_prologue
	mov	13, %l4			! pass the intr # in %l4

	!! Can not use fast-path during CAPTURE/RELEASE protocol
	sethi	%hi(doing_capture_release), %l4
	ld	[%l4 + %lo(doing_capture_release)], %l4
	tst	%l4
	bnz	interrupt_prologue	
	mov	13, %l4			! pass the intr # in %l4

        ! if (cpup->cpu_m.xc_state[X_CALL_MEDPRI] == XC_DONE)
	!    goto interrupt_prologue
	ld	[%l3 + XC_STATE_MEDPRI], %l4
	cmp	%l4, XC_DONE
	beq,a	interrupt_prologue
	mov	13, %l4			! pass the intr # in %l4

	! if (xc_mboxes[X_CALL_MEDPRI].func == tlbflush), handle here
	set	xc_mboxes, %l5
	ld	[%l5 + XC_MBOX_FUNC_MEDPRI], %l4
	set	srmmu_mmu_flushpagectx, %l6
	cmp	%l4, %l6
	bne	1f
	nop

	/*
	 * srmmu_mmu_flushpagectx(caddr_t vaddr, u_int ctx)
	 */

	!! Clear softint condition, we're handling this one
	set	IR_SOFT_INT(13), %l4	! Soft interrupts are in bits <17-31>
	st	%l4, [%l7 + 4]		! Clear soft level13
	ld	[%l7], %g0		! Wait for change to propate through ?

	/*
	 * Updated register usage until after RESTORE CONTEXT is finished
	 * %l7 = ctx
	 * %l6 = vaddr
	 * %l5 = xc_mboxes/RMMU_CTX_REG
	 * %l4 = saved ctx
	 * %l3 = cpu pointer
	 */

	ld	[%l5 + XC_MBOX_ARG1_MEDPRI], %l6	/* vaddr */
	ba	.vik_flush_common
	or      %l6, FT_PAGE<<8, %l6

	/*
	 * vik_mmu_flushrgn(caddr_t addr, u_int ctx)
	 */
1:
	set	vik_mmu_flushrgn, %l6
	cmp	%l4, %l6
	bne	2f
	nop

	/* see vik_mmu_flushrgn earlier in this file */
	sethi	%hi(viking_ptp2_bug), %l6	! is ptp2 workaround...
	ld	[%l6 + %lo(viking_ptp2_bug)], %l6 ! ... needed?
	tst	%l6
	be	.vik_flushrgn			! no, call flushrgn
	nop
	b	.vik_flushctx
	nop

	/*
	 * srmmu_mmu_flushrgn(caddr_t addr, u_int ctx)
	 */
2:
	set	srmmu_mmu_flushrgn, %l6
	cmp	%l4, %l6
	bne	3f
	nop

.vik_flushrgn:
	!! Clear softint condition, we're handling this one
	set	IR_SOFT_INT(13), %l4	! Soft interrupts are in bits <17-31>
	st	%l4, [%l7 + 4]		! Clear soft level13
	ld	[%l7], %g0		! Wait for change to propate through ?

	ld	[%l5 + XC_MBOX_ARG1_MEDPRI], %l6	/* vaddr */
	ba	.vik_flush_common
	or      %l6, FT_RGN<<8, %l6


	/*
	 * srmmu_mmu_flushseg(caddr_t addr, u_int ctx)
	 */
3:
	set	srmmu_mmu_flushseg, %l6
	cmp	%l4, %l6
	bne	4f
	nop

	!! Clear softint condition, we're handling this one
	set	IR_SOFT_INT(13), %l4	! Soft interrupts are in bits <17-31>
	st	%l4, [%l7 + 4]		! Clear soft level13
	ld	[%l7], %g0		! Wait for change to propate through ?

	ld	[%l5 + XC_MBOX_ARG1_MEDPRI], %l6	/* vaddr */
	or      %l6, FT_SEG<<8, %l6

.vik_flush_common:
	ld	[%l5 + XC_MBOX_ARG2_MEDPRI], %l7	/* ctx */

	!! BORROW_CONTEXT
	set     RMMU_CTX_REG, %l5
	lda     [%l5]ASI_MOD, %l4	! Stash away current context
	sta     %l7, [%l5]ASI_MOD	! Switch to new context

	!! FLUSH
	sta     %g0, [%l6]ASI_FLPR

	!! RESTORE_CONTEXT
	ba	.vik_xcall_finish
	sta     %l4, [%l5]ASI_MOD	! restore previous context

	!! FINISHED, goto .vik_xcall_finish


	/*
	 * srmmu_mmu_flushctx(int ctx)
	 */
4:
	set	srmmu_mmu_flushctx, %l6
	cmp	%l4, %l6
	bne,a	interrupt_prologue
	mov	13, %l4			! pass the intr # in %l4

.vik_flushctx:
	!! Clear softint condition, we're handling this one
	set	IR_SOFT_INT(13), %l4	! Soft interrupts are in bits <17-31>
	st	%l4, [%l7 + 4]		! Clear soft level13
	ld	[%l7], %g0		! Wait for change to propate through ?

	set	FT_CTX<<8, %l6
	ld	[%l5 + XC_MBOX_ARG1_MEDPRI], %l7	/* ctx */

	!! BORROW_CONTEXT
	set     RMMU_CTX_REG, %l5
	lda     [%l5]ASI_MOD, %l4	! Stash away current context
	sta     %l7, [%l5]ASI_MOD	! Switch to new context

	!! FLUSH
	sta     %g0, [%l6]ASI_FLPR

	cmp	%l4, %l7		! if we're in the context we're flushing
	beq,a	4f			! then change to KCONTEXT
	mov	KCONTEXT, %l4
4:
	!! RESTORE_CONTEXT
	sta     %l4, [%l5]ASI_MOD	! restore previous context
	!! FINISHED, goto .vik_xcall_finish


.vik_xcall_finish:
	/*
	 * Finish the x-call protocol
	 */
	
	! cpup->cpu_m.xc_pend[X_CALL_MEDPRI] = 0
	st	%g0, [%l3 + XC_PEND_MEDPRI]

	! cpup->cpu_m.xc_retval[X_CALL_MEDPRI] = 0
	st	%g0, [%l3 + XC_RETVAL_MEDPRI]

	! cpup->cpu_m.xc_ack[X_CALL_MEDPRI] = 1
	mov	1, %l4
	st	%l4, [%l3 + XC_ACK_MEDPRI]

	!! finished, go home
	mov	%l0, %psr
	nop; nop; nop
	jmp	%l1
	rett	%l2
	
	SET_SIZE(vik_xcall_medpri)
#endif /* !defined(lint) */
