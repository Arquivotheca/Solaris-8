/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vik_overflow.s	1.14	99/04/13 SMI"

/* From: 5.1: srmmu/ml/overflow.s 1.24 92/12/15 SMI */

/*
 * The entry points in this file are not callable from C and hence are of no
 * interest to lint.
 */

#if defined(lint)
#include <sys/types.h>
#include <sys/thread.h>
#endif

#include <sys/asm_linkage.h>
#include <sys/machparam.h>
#include <sys/machpcb.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/machthread.h>

#if !defined(lint)

#include "assym.h"

/*
 * These should probably be somewhere else.
 */
#define	FLP_TYPE(b)	(((b) << 8) & 0x00000F00)
VIK_NWINDOWS = 8
VIK_WINMASK = 0xff

/*
 * sun4m Viking window overflow trap handler.
 *
 *     Supervisor overflow is straight forward.  For user overflow we have
 *     to worry about the possibility that the stack page may not exist, or
 *     might be swapped out.
 *
 * On entry:
 *
 *     %l1, %l2 = %pc, %npc
 *         %l1=-1 => return to %l2+8, no rett
 *         (the intention is to use this in the future to clean up locore.s,
 *         although more work is needed here first).
 *     %l3 = %wim
 *
 * Trap window:
 *
 *     %l0 = %psr
 *     %l3 = saved %g4
 *     %l4 = saved target window %i4 (our %o4)
 *     %l5 = saved target window %i5 (our %o5)
 *     %l6 = scratch
 *     %l7 = new wim
 *
 * Target window:
 *
 *     %i4 = scratch, result of probe
 *     %i5 = scratch, base address of page to probe
 *     %g1 = mpcb pointer
 *     %g2 = scratch
 *     %g4 = %sp or address where user window should be saved
 */
	.align	32
	ENTRY_NP(vik_window_overflow)
#ifdef TRACE
	CPU_ADDR(%l7, %l4)		! get CPU struct ptr to %l7 using %l4
	!
	! See if event is enabled, using %l4 and %l5 as scratch
	!
	VT_ASM_TEST_FT(TR_FAC_TRAP, TR_KERNEL_WINDOW_UNDERFLOW, %l7, %l4, %l5)
	!
	! We now have: %l7 = cpup, %l4 = event, %l5 = event info, and
	! the condition codes are set (ZF means not enabled)
	!
	bz	9f			! event not enabled
	sll	%l4, 16, %l4		! %l4 = (event << 16), %l5 = info
	or	%l4, %l5, %l4		! %l4 = (event << 16) | info
	st	%l3, [%l7 + CPU_TRACE_SCRATCH]		! save %l3
	st	%l6, [%l7 + CPU_TRACE_SCRATCH + 4]	! save %l6
	!
	! Dump the trace record.  The args are:
	! 	(event << 16) | info, data, cpup, and three scratch registers.
	! In this case, the data is the trapped PC (%l1).
	!
	TRACE_DUMP_1(%l4, %l1, %l7, %l3, %l5, %l6)
	!
	! Trace done, restore saved registers
	!
	ld	[%l7 + CPU_TRACE_SCRATCH], %l3		! restore %l3
	ld	[%l7 + CPU_TRACE_SCRATCH + 4], %l6	! restore %l6
9:
#endif  /* TRACE */
	mov	%psr, %l0
	!-
	mov	%o4, %l4		! save %o4, %o5
	srl	%l3, 1, %l6		! new WIM = ror(old WIM, 1, NWINDOWS)
	btst	PSR_PS, %l0		! supervisor mode trap?
	!-
	mov	%o5, %l5
	sll	%l3, VIK_NWINDOWS - 1, %l7
	mov	%g4, %l3		! save %g4
	!-
	or	%l6, %l7, %l7		! %l7 = new wim
	bnz	vik_wo_super
	save	%l7, 0, %i4		! (delay slot) get into window to be
					! saved
vik_wo_user_sp:
	mov	%sp, %g4		! set address for user window save
	!-
	! Must install the new wim after performing the save to enter the
	! target window, but before performing the restore to return to the
	! trap window.  Otherwise we would get a window trap while traps are
	! disabled which would cause a reset.

vik_wo_user:
	!
	! Handle user overflow.
	!
	! Need to check that the stack pointer is reasonable, and do a probe to
	! see if the page is present.
	!
	!	**********************************************************
	!	* THIS CODE IS MORE SUBTLE THAN IT MIGHT AT FIRST APPEAR *
	!	* DO NOT MODIFY IT WITHOUT READING THE UNDERFLOW HANDLER *
	!	**********************************************************
	!
	mov	%i4, %wim			! install new wim
	!-
	add	%g4, 7*8, %i5			! (wim delay)
						! address of last std in %i5
	andn	%i5, MMU_STD_PAGESIZE - 8, %i5	! (wim delay)
						! cut to page boundary, leaving
						! lowest few bits unchanged
	!-
	andn	%g4, MMU_STD_PAGESIZE - 1, %i4	! (wim delay)
						! page base address in %i4
	cmp	%i4, %i5			! do we cross a page boundary,
	bne	vik_wo_poorly_aligned		! or is the stack misaligned?
	!-
	or	%i4, FLP_TYPE(MMU_FLP_ALL), %i5	! (delay slot) address to use
						! for probe
	!-
	!
	! Good, do not cross a page boundary, not misaligned.
	! Probe and check result.
	! If the `Mod' bit is clear or the permissions are not
	! right, then take the long way round.
	!
	lda	[%i5]ASI_FLPR, %i4		! probe - MP safe!
	!-
	and	%i4, PTE_PERMS(4|1)|PTE_MOD(1), %i5	! ignore bit 1
	cmp	%i5, PTE_PERMS(1)|PTE_MOD(1)
	!-
	be,a	1f
	!-
	std	%l0, [%g4 + 0*8]		! (delay slot) start saving
	!-
	andn	%g4, MMU_STD_PAGESIZE - 1, %i5
	ba,a	vik_wo_fault			! no PTE, or wrong permissions
	!-
1:
	!
	! Good, permission must be 1 or 3 - user RW or user RWX.
	!
	std	%l2, [%g4 + 1*8]		! puke the registers
	!-
	std	%l4, [%g4 + 2*8]
	!-
	std	%l6, [%g4 + 3*8]
	!-
	clr	%i4
	clr	%i5
	!-
	std	%i0, [%g4 + 4*8]
	!-
	clr	%i0
	clr	%i1
	!-
	std	%i2, [%g4 + 5*8]
	!-
	clr	%i2
	clr	%i3
	!-
	restore					! back into trap window
	!-
	std	%l4, [%g4 + 6*8]
	!-
	clr	%l4
	clr	%l5
	!-
	clr	%l6
	clr	%l7
	st	%o6, [%g4 + 7*8 + 0]
	!-
	st	%o7, [%g4 + 7*8 + 4]
	! We can save two cycles by spliting two more of the std's into st's
	! and putting the st's into the same instruction groups as the clr
	! pairs.  However Viking modules with a secondary cache use write
	! through.  The write buffer is 8 double words deep.  Thus splitting
	! up the stores risks incurring a write stall, which would be far more
	! serious than the two cycles we hope to save.
	!
	! If anyone has accurate documentation on the behavior of the Viking
	! write buffer, could they check to see if at least one of these std's
	! can be split.

vik_wo_out:
	!
	! Return from user overflow.
	!
	! If the CLEAN_WINDOWS flag is set in the pcb we need to clean out the
	! new window.  Quicker to always do this than find out if we have to.
	!
	! According to the ABI the values in %l1 and %l2 need not be cleared,
	! but it only costs one cycle, so lets be nice.
	!
	orcc	%l1, %g0, %o6		! put pc, npc in an unobtrusive place
	mov	%l2, %o7
	mov	%l3, %g4		! restore %g4
	clr	%l3
	clr	%l2
	bneg	1f			! possibly return with a rett?
	!-
	mov	%l0, %psr		! (delay slot) reinstall PSR_CC
	!-
0:
	clr	%l1			! (psr delay)
	clr	%l0			! (psr delay)
	jmp	%o6			! (psr delay) reexecute save
	!-
	rett	%o7
	!-
1:
	cmp	%l1, -1			! return with a rett?
	bne	0b
	!-
	mov	%l0, %psr		! (delay slot) reinstall PSR_CC
	!-
	clr	%l1			! (psr delay) return without a rett
	jmp	%o7+8			! (psr delay)
	!-
	clr	%l0			! (psr delay)
	!-

vik_wo_poorly_aligned:
	!
	! The stack must either cross a page boundary or be misaligned.
	!
	btst	7, %g4
	bnz	vik_wo_misaligned
	nop

	!
	! The stack crosses a page boundary.  Do save in two parts.  Probe
	! and store to the first page, then probe and store to the second
	! page.
	!
	! We could just use the old "no-fault" scheme, but by using the probe
	! approach we are less dependent on that particular intricacy of the
	! MMU, or at least we will be if locore.s ever gets cleaned up.
	!
	!	**********************************************************
	!	* THIS CODE IS MORE SUBTLE THAN IT MIGHT AT FIRST APPEAR *
	!	* DO NOT MODIFY IT WITHOUT READING THE UNDERFLOW HANDLER *
	!	**********************************************************
	!

	!
	! Probe the first page.
	!
	lda	[%i5]ASI_FLPR, %i4		! probe first page - MP safe!
	and	%i4, PTE_PERMS(4|1), %i5	! ignore bit 1
	cmp	%i5, PTE_PERMS(1)
	bne,a	vik_wo_fault			! no PTE, or wrong permissions
	andn	%g4, MMU_STD_PAGESIZE - 1, %i5

	! Good, permission must be 1 or 3 - user RW or user RWX.

	!
	! Save only those values stored on the first page.
	!

	orn	%g4, MMU_STD_PAGESIZE - 1, %i5	! used by the following macro

#define	STD_FIRSTPAGE(OFFSET, REGS)	\
	addcc	%i5, (OFFSET), %g0;	\
	bneg,a	.+8;			\
	std	REGS, [%g4 + (OFFSET)]

	STD_FIRSTPAGE(0*8, %l0)
	STD_FIRSTPAGE(1*8, %l2)
	STD_FIRSTPAGE(2*8, %l4)
	STD_FIRSTPAGE(3*8, %l6)
	STD_FIRSTPAGE(4*8, %i0)
	STD_FIRSTPAGE(5*8, %i2)
	addcc	%i5, 6*8, %g0			! %i4 %i5 are in the trap page
	bpos	0f
	nop
	restore
	mov	%g0, %wim		! clear wim to allow save to invalid win
	nop; nop			! wim delay
	std	%l4, [%g4 + 6*8]	! wim delay
	save	%l7, 0, %i4
	mov	%i4, %wim
	nop; nop; nop
0:
	! STD_FIRSTPAGE(7*8, %i6)

	! The squashing of annulled load/store instructions as used above and
	! below had better occur before they perform a TLB data access.  This
	! is what the documentation seems to indicate, but it would be nice to
	! have something concrete.

	! Before doing the second probe make sure the immediately preceeding
	! TLB access was for a code fetch, and not one of the above std's.
	! The branch, even though untaken, ensures we have done a fetch, as
	! opposed to executing out of the prefetch buffer, the save, restore
	! ensure the branch has reached D2, where the fetch is performed,
	! before the next instruction is executed.
	bn	.
	restore
	mov	%g0, %wim		! clear wim to allow save to invalid win
	nop; nop; nop
	save	%l7, 0, %i4
	mov	%i4, %wim
	nop; nop; nop

	!
	! Probe the second page.
	!
	andn	%g4, MMU_STD_PAGESIZE - 1, %i5	! first page base address
	sub	%i5, - MMU_STD_PAGESIZE, %i5	! second page base address
	or	%i5, FLP_TYPE(MMU_FLP_ALL), %i5	! address to use for probe
	lda	[%i5]ASI_FLPR, %i4		! probe second page - MP safe
	and	%i4, PTE_PERMS(4|1), %i5	! ignore bit 1
	cmp	%i5, PTE_PERMS(1)
	be	0f
	nop

	! Probe failed.
	andn	%g4, MMU_STD_PAGESIZE - 1, %i5	! second page base address into
	sub	%i5, - MMU_STD_PAGESIZE, %i5	! %i5
	ba,a	vik_wo_fault			! no PTE, or wrong permissions

0:
	! Good, permission must be 1 or 3 - user RW or user RWX.

	!
	! Save only those values stored on the second page.
	!

	orn	%g4, MMU_STD_PAGESIZE - 1, %i5	! used by the following macro

#define STD_SECONDPAGE(OFFSET, REGS)	\
	addcc	%i5, (OFFSET), %g0;	\
	bpos,a	.+8;			\
	std	REGS, [%g4 + (OFFSET)]

	! STD_SECONDPAGE(0*8, %l0)
	STD_SECONDPAGE(1*8, %l2)
	STD_SECONDPAGE(2*8, %l4)
	STD_SECONDPAGE(3*8, %l6)
	STD_SECONDPAGE(4*8, %i0)
	STD_SECONDPAGE(5*8, %i2)
	addcc	%i5, 6*8, %g0			! %i4 %i5 are in the trap page
	bneg	0f
	nop
	restore
	mov	%g0, %wim		! clear wim to allow save to invalid win
	nop; nop;
	std	%l4, [%g4 + 6*8]	! wim delay
	save	%l7, 0, %i4
	mov	%i4, %wim
	nop; nop; nop
0:
	STD_SECONDPAGE(7*8, %i6)

	clr	%i0
	clr	%i1
	clr	%i2
	clr	%i3
	clr	%i4
	clr	%i5

	restore

	clr	%l4
	clr	%l5
	clr	%l6
	clr	%l7

	ba,a	vik_wo_out			! and exit

vik_wo_fault:
	!
	! Probe returned bad.  No PTE or wrong permissions.  Fake a page fault.
	!
	! Probe result in %i4, faulting page address in %i5.
	!

	! Copy fault status and address back to the trap window as %l4 %l5,
	! restore faulting window %i4 %i5.
	restore				! back into trap window

	mov	%o4, %l6		! copy back status, address
	mov	%o5, %l7

	mov	%l4, %o4		! restore faulting window %i4, %i5
	mov	%l5, %o5

	mov	%l6, %l4		! status, address into %l4, %l5
	mov	%l7, %l5

	!
	! If the PTE was not found, as opposed to simply having the wrong
	! permissions, the probe will have set bits in the fault status
	! register.
	!
	! I am not sure if following such a probe we need to reset the fault
	! status register to prevent the overwrite bit being turned on by the
	! next page fault.  We will do so to be on the safe side.
	!
	set	RMMU_FAV_REG, %l7
	lda	[%l7]ASI_MOD, %l7	! read MFAR, probably not needed
	set	RMMU_FSR_REG, %l6
	lda	[%l6]ASI_MOD, %l6	! read MFSR

	!
	! Replace %l4 with reason for fault, a faked up MFSR value.
	!

	tst	%l4			! was PTE found?
	bz,a	0f
	mov	MMU_SFSR_AT_STORE | MMU_SFSR_FAV | MMU_SFSR_FT_INV, %l4
					! fake MFSR - no page

	set	MMU_SFSR_AT_STORE | MMU_SFSR_FAV | MMU_SFSR_FT_PRIV, %l4
					! fake MFSR - privilege

0:
	btst	PSR_PS, %l0		! supervisor mode fault?
	bnz	vik_wo_fault_super
	nop

	!
	! The fault occured while in user mode.  Save the window to the window
	! buffer in the pcb then fake a page fault.
	!

	mov	%g1, %l6		! save %g1 %g2 in %l6 %l7
	mov	%g2, %l7

	CPU_ADDR(%g1, %g2)		! get cpu struct pointer
	ld	[%g1 + CPU_MPCB], %g1	! get mpcb pointer

	mov	%wim, %g2		! save new wim in %g2

	mov	%g0, %wim		! clear wim to allow save to invalid win
	nop; nop; nop

	save
	mov	%g2, %wim		! reinstall new wim
	nop; nop

	st	%g4, [%g1 + MPCB_SPBUF]	! wim delay - save %sp/%g4 to sp buffer
	SAVE_WINDOW(%g1 + MPCB_WBUF)	! save window to window buffer

	restore

	! Compute mpcb->mpcb_uwm, a mask of the windows still containing
	! user data.  This will comprise all the window apart from the current
	! one, and the one we just saved.
	!
	mov	%l3, %g4		! restore %g4
	sll	%g2, 1, %l3		! old wim = rol(wim, 1, NWINDOWS)
	or	%l3, %g2, %g2		! new wim | old wim
	srl	%g2, VIK_NWINDOWS, %l3	! compute possible wrap of old wim
	or	%g2, %l3, %g2		! combine old and new wims
	not	%g2			! compute windows containing data
	and	%g2, VIK_WINMASK, %g2	! mask valid windows
	st	%g2, [%g1 + MPCB_UWM]	! mpcb_uwm = ~(old WIM | new WIM)

	mov	1, %g2
	st	%g2, [%g1 + MPCB_WBCNT]	! mpcb_wbcnt = 1

	!
	! Set up state to handle the fault.
	!
	mov	%g1, %sp		! setup kernel stack
	ld	[%g1 + MPCB_THREAD], %l3 ! get thread pointer
	mov	%l6, %g1		! restore %g1 %g2
	mov	%l7, %g2

	SAVE_GLOBALS(%sp + MINFRAME)		! create fault frame
	SAVE_OUTS(%sp + MINFRAME)
	st	%l0, [%sp + MINFRAME + PSR*4]	! psr
	st	%l1, [%sp + MINFRAME + PC*4]	! pc
	st	%l2, [%sp + MINFRAME + nPC*4]	! npc

	mov	%l3, THREAD_REG			! set global thread pointer

	wr	%l0, PSR_ET, %psr		! enable traps
	nop; nop; nop

	!
	! Call fault handler.
	!
	mov	T_WIN_OVERFLOW, %o0		! fault type
	add	%sp, MINFRAME, %o1		! pointer to fault frame
	mov	%l5, %o2			! fault address, fake MFAR
	mov	%l4, %o3			! fault status, fake MFSR
	mov	S_WRITE, %o4			! access type
	call	trap				! trap(T_WIN_OVERFLOW,
						!	rp, addr, be, S_WRITE)
	nop

	ba,a	_sys_rtt	! use the standard trap handler code to return
				! to the user

vik_wo_fault_super:
	!
	! The fault occured trying to save a user window while in supervisor
	! mode.  Add the window to the window buffer in the pcb.  It will be
	! processed later when returning to user mode.
	!
	mov	%g1, %l6		! save %g1 %g2 in %l6 %l7
	mov	%g2, %l7

	CPU_ADDR(%g1, %g2)		! get cpu struct pointer
	ld	[%g1 + CPU_MPCB], %g1	! get mpcb pointer

	mov	%wim, %g2		! save new wim in %g2

	mov	%g0, %wim		! clear wim for save
	nop; nop; nop

	save
	mov	%g2, %wim		! reinstall new wim
	nop; nop; nop

	ld	[%g1 + MPCB_WBCNT], %g2	! mpcb_wbcnt*4 is offset into mpcb_spbuf
	sll	%g2, 2, %g2
	add	%g1, %g2, %g2
	st	%g4, [%g2 + MPCB_SPBUF]	! save %sp to window buffer,
					! addr pcb + mpcb_spbuf + mpcb_wbcnt*4

	ld	[%g1 + MPCB_WBCNT], %g2	! mpcb_wbcnt*64 is offset into mpcb_wbuf
	sll	%g2, 6, %g2
	add	%g1, %g2, %g2
	SAVE_WINDOW(%g2 + MPCB_WBUF)	! save window to window buffer,
					! address mpcb+mpcb_wbuf+mpcb_wbcnt*64

	ld	[%g1 + MPCB_THREAD], %g4
chkt_intr:				! follow thread->t_intr to get a
	ld	[%g4 + T_INTR], %g2	! non-interrupt thread
	tst	%g2
	bnz,a	chkt_intr
	mov	%g2, %g4
	ld	[%g1 + MPCB_WBCNT], %g2	! increment pcb_wbcnt to reflect save
	inc	%g2
	st	%g2, [%g1 + MPCB_WBCNT]
	stb	%g2, [%g4 + T_ASTFLAG]	! set AST so WBCNT will be seen

	restore

	mov	%l6, %g1		! restore %g1 %g2
	mov	%l7, %g2
	mov	%l3, %g4		! restore %g4

	cmp	%l1, -1			! return with a rett?
	be	0f
	mov	%l0, %psr		! (delay slot) reinstall PSR_CC
	nop				! (psr delay)
	nop				! (psr delay)
	jmp	%l1			! (psr delay) reexecute save
	rett	%l2
0:
	nop				! (psr delay)
	jmp	%l2+8			! (psr delay) return without a rett
	nop				! (psr delay)

vik_wo_misaligned:
	!
	! An overflow trap has happened with a misaligned %sp.
	!
	! If the fault occured while in user mode fake a low level memory
	! alignment trap.  Otherwise the fault occured trying to save a user
	! window while in supervisor mode.  Have the window added to the window
	! buffer in the pcb.  It will be processed later when returning to user
	! mode.
	!
	restore				! back into trap window

	mov	%l4, %o4		! restore faulting window %i4, %i5
	mov	%l5, %o5

	btst	PSR_PS, %l0		! supervisor mode fault?
	bnz	vik_wo_fault_super	! save window to pcb window buffer
	nop

	mov	%wim, %l4
	sll	%l4, 1, %g4		! old wim = rol(wim, 1, NWINDOWS)
	srl	%l4, VIK_NWINDOWS - 1, %l4
	wr	%l4, %g4, %wim		! restore old %wim

	mov	%l3, %g4		! (wim delay) restore %g4
	! Have already set up %l0, %l1, %l2 = %psr, %pc, %npc.
	ba	sys_trap		! (wim delay)
	mov	T_ALIGNMENT, %l4	! (wim delay) fake alignment trap

	.align	32
vik_wo_super:
	!
	! Handle supervisor overflow.
	!
	! The register window we are about to push out could be a user window.
	! We need to test mpcb->mpcb_uwm, which has a bit set for each
	! user window still in the register file.
	!
	! Checking for this is painfully slow.  Better overall performance
	! might be obtained by dumping out all user windows immediately upon
	! entering the kernel.  Or by having two bits set in the WIM, the
	! second of which points at window defining the user-kernel interface,
	! checking if we might be saving a user window would then be very fast
	! (user windows should still be pushed out as per normal).
	!
	mov	%i4, %wim		! install new WIM
	!-
	CPU_ADDR(%i4, %i5)		! (wim delay) get cpu struct pointer
					! 5 cycles, ouch!
	!-
	!-
	ld	[%i4 + CPU_MPCB], %i4	! get mpcb pointer
	!-
	!-
	tst	%i4			! kernel thread (no lwp)?
	bnz,a	0f
	!-
	ld	[%i4 + MPCB_UWM], %i5	! (delay slot) get mpcb_uwm in %i5
	!-
	ba	2f			! skip pcb_uwm check for kernel threads
	std	%l0, [%sp + 0*8]	! (delay slot) store first pair
	!-
0:
	tst	%i5			! saving a user window?
	bnz,a	1f			! yes - test for user or shared
	restore				! (delay slot) get back to trap window
	!--

	! We are saving a kernel window.

	std	%l0, [%sp + 0*8]
	!-
2:
	std	%l2, [%sp + 1*8]
	!-
	std	%l4, [%sp + 2*8]
	!-
	std	%l6, [%sp + 3*8]
	!-
	restore %sp, 0, %g4
	!-
	std	%o0, [%g4 + 4*8]
	!-
	std	%o2, [%g4 + 5*8]
	!-
	st	%l4, [%g4 + 6*8 + 0]
	cmp	%l1, -1			! return with a rett?
	be	0f
	!-
	mov	%l0, %psr		! (delay slot) reinstall PSR_CC
	!-
	std	%o6, [%g4 + 7*8]	! (psr delay)
	!-
	st	%l5, [%g4 + 6*8 + 4]	! (psr delay)
	mov	%l3, %g4		! (psr delay) restore %g4
	jmp	%l1
	!-
	rett	%l2
	!-
0:
	st	%l5, [%g4 + 6*8 + 4]	! (psr delay)
	jmp	%l2+8			! (psr delay) return without a rett
	!-
	std	%o6, [%g4 + 7*8]	! (psr delay)
	!-
	!
	! We are saving a user window, branch to user mode overflow handler.
	!
1:
	mov	%g0, %wim		! clear wim for save
	!-
	bclr	%l7, %o5		! (wim delay) clear current window
					! from mpcb_uwm
	ld	[%o4 + MPCB_SWM], %g4	! (wim delay) shared window mask
	btst	%l7, %g4
	st	%o5, [%o4 + MPCB_UWM]	! (wim delay) update mpcb_uwm
	bz,a	vik_wo_user_sp		! not the shared window
	!-
	save	%l7, 0, %i4		! (delay slot) get into window to save
	!
	! Save kernel's copy of shared window
	!
	save				! move to window to be saved
	clr	[%i4 + MPCB_SWM]	! clear shared window mask
	ld	[%i4 + MPCB_REGS + SP*4], %g4 ! user stack pointer
	!
	! Save kernel's copy of shared window
	!
	std	%l0, [%sp + 0*8]
	!-
	std	%l2, [%sp + 1*8]
	!-
	std	%l4, [%sp + 2*8]
	!-
	std	%l6, [%sp + 3*8]
	!-
	restore %sp, 0, %o4		! move back to trap window (wim is 0)
	!-
	std	%o0, [%o4 + 4*8]
	!-
	std	%o2, [%o4 + 5*8]
	!-
	std	%l4, [%o4 + 6*8]
	!-
	std	%o6, [%o4 + 7*8]
	b	vik_wo_user		! go save user copy
	save	%l7, 0, %i4

	SET_SIZE(vik_window_overflow)

#endif	/* lint */
