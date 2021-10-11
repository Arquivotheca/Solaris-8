/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ross625_underflow.s	1.9	99/04/13 SMI"

/*
 * The sole entry points in this file are not callable from C and
 * hence are of no interest to lint.
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
#include <sys/traptrace.h>

#if !defined(lint)

#include "assym.h"

#define ROSS625_NWINDOWS 8

/*
 * Ross 625 Window underflow trap handler.
 *
 * On entry:
 *
 *	%l0, %l1, %l2 = %psr, %pc, %npc (%l1=-1 => return to %l2+8, no rett)
 *	%l3 = %wim
 *
 * Register usage:
 *
 *	%l4 = scratch
 *	%l5 = scratch
 *	%l6 = nwindows - 1
 *	%l7 = scratch
 *	%g1 = scratch
 */
	ENTRY_NP(ross625_window_underflow)
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
	!	(event << 16) | info, data, cpup, and three scratch registers.
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

	set	ROSS625_NWINDOWS - 1, %l6
	! sethi   %hi(nwin_minus_one), %l6
	! ld	[%l6+%lo(nwin_minus_one)], %l6

	sll	%l3, 1, %l4		! next WIM = rol(WIM, 1, NW)
	srl	%l3, %l6, %l5
	or	%l5, %l4, %l5
	mov	%l5, %wim		! install it
	btst	PSR_PS, %l0		! (wim delay 1) test for user trap
	bnz	.wu_super		! super trap is the odd case
	restore				! delay slot

	!
	! User underflow.  Window to be restored is a user window.
	! We must check whether the user stack is resident where the
	! the window will be restored from, which is pointerd to by
	! the windows sp.  The sp is the fp of the window which tried
	! to do the restore so that it is still valid.
	!
	restore				! get into window to be restored
	!
	! In order to restore the window from the stack, the stack
	! must be aligned on a word boundary, and the part of the
	! stack where the restore will be done must be present.
	! We first check for alignment.
	!
	btst	0x7, %sp		! test sp alignment
	bnz	.wu_misaligned		! very odd case
	nop

	!Security Check
	set	KERNELBASE, %l4		!if in supervisor space
	cmp	%l4, %sp
	bleu	.wu_stack_not_user	!branch out to nonresident
	nop

	!
	! Most of the time the stack is resident in main memory,
	! so we don't verify its presence before attempting to restore
	! the window from the stack.  Rather, we simply set the
	! no-fault bit of the SRMMU's control register, so that
	! doing the restore won't cause another trap in the case
	! where the stack is not present.  By checking the
	! synchronous fault status register, we can determine
	! whether the restore actually worked (ie. stack was present),
	! or whether we first need to fault in the stack.
	! Other sun4 trap handlers first probe for the stack, and
	! then, if the stack is present, they restore from the stack.
	! This approach CANNOT be used with a multiprocessor system
	! because of a race condition: between the time that the
	! stack is probed, and the restore from the stack is done, the
	! stack could be stolen by the page daemon.

	set	RMMU_FSR_REG, %l5
	lda	[%l5]ASI_MOD, %g0	! clear SFSR

	lda	[%g0]ASI_MOD, %l4
	or	%l4, MMCREG_NF, %l4	! set no-fault bit in
	sta	%l4, [%g0]ASI_MOD	! control register

	TRACE_UNFL(TT_UF_USR, %sp, %l1, %l2, %l3)
	RESTORE_WINDOW(%sp)
	save
	save

	lda	[%g0]ASI_MOD, %l4
	andn	%l4, MMCREG_NF, %l4	! clear no-fault bit
	sta	%l4, [%g0]ASI_MOD	! in control register

	set	RMMU_FAV_REG, %l5
	lda	[%l5]ASI_MOD, %l5	! read SFAR
	set	RMMU_FSR_REG, %l4
	lda	[%l4]ASI_MOD, %l4	! read SFSR

	b	.wu_chk_flt		! check is there is a fault
	nop

.wu_stack_not_user:
	save
	save
	mov	MMU_SFSR_FAV | MMU_SFSR_FT_PRIV, %l4	!fake up SFSR
	b	.wu_stack_not_res
	mov	%sp, %l5		!fake SFAR

.wu_chk_flt:
	btst	SFSREG_FAV, %l4		! did a fault occurr?
	bne	.wu_stack_not_res	! yes branch
	nop

.wu_out:
	!
	! Maintain clean windows. We only need to clean the registers
	! used in underflow as we know this is a user window.
	!
	! This used to be optional, depending on the CLEAN_WINDOWS flag
	! in the PCB.  Now, it takes longer to find the current PCB than
	! to clean the window.
	!
	mov	%l1, %o6		! put pc, npc in an unobtrusive place
	mov	%l2, %o7
	clr	%l1			! clean the used ones
	clr	%l2
	clr	%l3
	clr	%l4
	clr	%l5
	clr	%l6
	cmp	%o6, -1			! return with a rett?
	bne	0f
	mov	%l0, %psr		! reinstall system PSR_CC
	clr	%l7
	jmp	%o7+8			! return to caller
	clr	%l0
0:
	clr	%l7
	clr	%l0
	jmp	%o6			! reexecute restore
	rett	%o7

.wu_super:
	!
	! Supervisor underflow.
	! We do one more restore to get into the window to be restored.
	! The first one was done in the delay slot coming here.
	! We then restore from the stack.
	!
	restore				! get into window to be restored

	TRACE_UNFL(TT_UF_SYS, %sp, %l1, %l2, %l3)
	RESTORE_WINDOW(%sp)
	save				! get back to original window
	save
	cmp	%l1, -1			! return with a rett?
	bne	0f
	mov	%l0, %psr		! reinstall sup PSR_CC
	nop
	jmp	%l2+8			! return to caller
	nop
0:
	nop				! psr delay
	nop
	jmp	%l1			! reexecute restore
	rett	%l2

.wu_stack_not_res:
	!
	! Restore area on user stack is not resident.
	! We punt and fake a page fault so that trap can bring the page in.
	! If the page fault is successful we will reexecute the restore,
	! and underflow with the page now resident.
	!
	CPU_ADDR(%l7, %l6)		! load CPU address into %l7 using %l6
	ld	[%l7 + CPU_THREAD], %l6	! load thread pointer
	ld	[%l7 + CPU_MPCB], %l7	! setup kernel stack
	SAVE_GLOBALS(%l7 + MINFRAME)	! mpcb doubles as stack
	mov	%l6, THREAD_REG		! set global thread pointer

	restore				! back to last user window
	mov	%psr, %g4		! get CWP
	save				! back to trap window

	!
	! Now that we are back in the trap window, note that
	! we have access to the following saved values:
	!
	! %l7 -- computed sp
	! %l4 -- fault status (SFSR)
	! %l5 -- fault address (SFAR)
	!
	! Save remaining user state
	!
	mov	%l7, %sp		! setup kernel stack
	SAVE_OUTS(%sp + MINFRAME)
	st	%l0, [%sp + MINFRAME + PSR*4] ! psr
	st	%l1, [%sp + MINFRAME + PC*4] ! pc
	st	%l2, [%sp + MINFRAME + nPC*4] ! npc

	mov	%l3, %wim		! reinstall old wim
	mov	1, %g1			! UWM = 0x01 << CWP
	sll	%g1, %g4, %g1
	st	%g1, [%l7 + MPCB_UWM]	! setup mpcb->mpcb_uwm
	clr	[%l7 + MPCB_WBCNT]

	wr	%l0, PSR_ET, %psr	! enable traps
	mov	T_WIN_UNDERFLOW, %o0
	add	%sp, MINFRAME, %o1
	mov	%l5, %o2		! fault address
	mov	%l4, %o3		! fault status
	call	trap			! trap(T_WIN_UNDERFLOW,
	mov	S_READ, %o4		!	rp, addr, be, S_READ)

	b,a	_sys_rtt

	!
	! A user underflow trap has happened with a misaligned sp.
	! Fake a memory alignment trap.
	!
.wu_misaligned:
	save				! get back to orig window
	save
	mov	%l3, %wim		! restore old wim, so regs are dumped
	b	sys_trap
	mov	T_ALIGNMENT, %l4	! delay slot, fake alignment trap
	SET_SIZE(ross625_window_underflow)

#endif	/* lint */
