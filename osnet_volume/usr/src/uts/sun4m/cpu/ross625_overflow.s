/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ross625_overflow.s	1.10	99/04/13 SMI"

/*
 * The entry points in this file are * not callable from C and hence are
 * of no interest to lint.
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
 * Ross 625 window overflow trap handler
 *
 * On entry:
 *
 *	%l0, %l1, %l2 = %psr, %pc, %npc (%l1=-1 => return to %l2+8, no rett)
 *	%l3 = %wim
 * Register usage:
 *	%l4 = scratch (sometimes saved %g2)
 *	%l5 = saved %g3
 *	%l6 = nwindows - 1 and scratch
 *	%l7 = saved %g1
 *	%g1 = new wim or scratch or SFSR
 *	%g2 = scratch while in the window to be saved or SFAR
 *	%g3 = PCB pointer
 *	%g4 = scratch
 */
	.align 32
	ENTRY_NP(ross625_window_overflow)
	mov	%psr, %l0

#ifdef TRACE
	CPU_ADDR(%l7, %l4)		! get CPU struct ptr to %l7 using %l4
	!
	! See if event is enabled, using %l4 and %l5 as scratch
	!
	VT_ASM_TEST_FT(TR_FAC_TRAP, TR_KERNEL_WINDOW_OVERFLOW, %l7, %l4, %l5)
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
#endif	/* TRACE */

	set	ROSS625_NWINDOWS - 1, %l6
	! sethi   %hi(nwin_minus_one), %l6
	! ld      [%l6+%lo(nwin_minus_one)], %l6

	mov	%g1, %l7		! save %g1
	mov	%g3, %l5		! save %g3

	srl	%l3, 1, %g1		! next WIM = %g1 = ror(WIM, 1, NW)
	sll	%l3, %l6, %l4
	or	%l4, %g1, %g1		! new WIM

	! Load thread pointer into %g3 (%g7 is usually THREAD_REG).
	! Even kernel traps do this for now, because kadb gets in here
	! with its own %g7.

	CPU_ADDR(%g3, %l4)		! get CPU struct ptr to %g3 using %l4
	ld	[%g3 + CPU_MPCB], %g3	! get mpcb pointer
	tst	%g3			! mpcb == 0 for kernel threads
	bz	.wo_kt_window		! skip uwm checking when lwp == 0
	btst	PSR_PS, %l0		! test for user or sup trap
	bnz	.wo_super		! super trap is the odd case
	nop

.wo_user_window:
	!
	! Window to be saved is a user window.
	!
	mov	%g2, %l4		! save %g2
	mov	%g4, %l6		! save %g4 (%l6 no longer holds NW-1)
	!
	! Must execute a save before we install the new wim
	! because, otherwise, the save would cause another
	! window overflow trap (watchdog).  Also, must install
	! the new wim here before doing the restore below, or
	! else the restore would cause a window overflow.  These
	! restrictions are responsible for alot of inefficiency
	! in this code.  The situation would be alot better if
	! the locals for the trap window were actually globals.
	!
	save				! get into window to be saved
	mov	%g1, %wim		! install new wim
	!
	! In order to save the window onto the stack, the stack
	! must be aligned on a word boundary, and the part of the
	! stack where the save will be done must be present.
	! We first check for alignment.
	!
	btst	0x7, %sp		! test sp alignment
	bnz	.wo_misaligned		! very odd case
	nop
	
	! Security Check to make sure that user is not
	! trying to save/restore unauthorized kernel pages
	! Check to see if we are touching non-user pages, if
	! so, fake up SFSR, SFAR to simulate an error
	!
	set     KERNELBASE, %g1
	cmp     %g1,%sp
	mov     MMU_SFSR_FAV | MMU_SFSR_FT_PRIV | MMU_SFSR_AT_STORE, %g1
	bleu    .wo_stack_not_res
	mov     %sp, %g2

	!
	! Most of the time the stack is resident in main memory,
	! so we don't verify its presence before attempting to save
	! the window onto the stack.  Rather, we simply set the
	! no-fault bit of the SRMMU's control register, so that
	! doing the save won't cause another trap in the case
	! where the stack is not present.  By checking the
	! synchronous fault status register, we can determine
	! whether the save actually worked (ie. stack was present),
	! or whether we first need to fault in the stack.
	! Other sun4 trap handlers first probe for the stack, and
	! then, if the stack is present, they store to the stack.
	! This approach CANNOT be used with a multiprocessor system
	! because of a race condition: between the time that the
	! stack is probed, and the store to the stack is done, the
	! stack could be stolen by the page daemon.

	set	RMMU_FSR_REG, %g1	! clear any old faults out
	lda	[%g1]ASI_MOD, %g0	! of the SFSR.

	lda	[%g0]ASI_MOD, %g2	! turn on no-fault bit in
	or	%g2, MMCREG_NF, %g1	! mmu control register to
	sta	%g1, [%g0]ASI_MOD	! prevent taking a fault.

	SAVE_WINDOW(%sp)		! try to save reg window

	sta	%g2, [%g0]ASI_MOD	! turn off no-fault bit

	set	RMMU_FAV_REG, %g2
	lda	[%g2]ASI_MOD, %g2	! read SFAR
	set	RMMU_FSR_REG, %g1	! clear any old faults out
	lda	[%g1]ASI_MOD, %g1	! read SFSR

	btst	MMU_SFSR_FAV, %g1	! did a fault occurr?
	bnz	.wo_stack_not_res	! yes, branch.
	nop

	TRACE_OVFL(TT_OV_USR, %sp, %g1, %g2, %g4)

	restore				! get back to orig window
	ld	[%g3 + MPCB_FLAGS], %l3	! check for clean window maintenance
	mov	%l7, %g1		! restore g1
	mov	%l4, %g2		! restore g2
	mov	%l5, %g3		! restore g3
!	mov	%l6, %g4		! restore g4
	cmp	%l1, -1			! return with a rett?
	bne,a	0f
	btst	CLEAN_WINDOWS, %l3
	mov	%l0, %psr		! reinstall system PSR_CC
	nop				! kernel code never needs clean_windows
	jmp	%l2+8			! return to caller
	nop
0:
	bnz	.clean_windows
	mov	%l0, %psr		! reinstall system PSR_CC
	nop
	nop
	jmp	%l1			! reexecute save
	rett	%l2

.wo_super:
	!
	! Overflow from supervisor mode.
	! if current thread is a simple kernel thread, then all of its
	! register windows reference the kernel thread's stack.
	! Otherwise the current thread has a lwp, and the register window
	! to save might be a user window. This is the case when
	! curmpcb->mpcb_uwm is not zero.  curmpcb->mpcb_uwm has
	! a bit set for each user window which is still in the register file.
	!
	ld	[%g3 + MPCB_UWM], %l4	! load mpcb->mpcb_uwm
	tst	%l4			! if uwm == 0
	bz	.wo_kt_window		! plain kernel window
	ld	[%g3 + MPCB_SWM], %l6	! get shared window mask
	btst	%g1, %l6		! are we in the shared user-trap window?
	bclr	%g1, %l4		! uwm &= ~(new wim)
	bz	.wo_user_window		! plain user window
	st	%l4, [%g3 + MPCB_UWM]	! update mpcb->pcb_uwm
	!
	! The window to be saved is where the kernel took a new-style trap from
	! user.  This window was borrowed for kernel use from the user, so it
	! must be saved both to the original user stack position and also to the
	! borrowed kernel stack position.
	!
.wo_shared_window:
	clr	[%g3 + MPCB_SWM]	! update shared trap window mask
	!
	! Window to be saved is a user window.
	!
	mov	%g2, %l4		! save %g2
	mov	%g4, %l6		! save %g4 (%l6 no longer holds NW-1)
	!
	! Must execute a save before we install the new wim
	! because, otherwise, the save would cause another
	! window overflow trap (watchdog).  Also, must install
	! the new wim here before doing the restore below, or
	! else the restore would cause a window underflow.  These
	! restrictions are responsible for alot of inefficiency
	! in this code.  The situation would be alot better if
	! the locals for the trap window were actually globals.
	!
	save				! get into window to be saved
	mov	%g1, %wim		! install new wim
	ld	[%g3 + MPCB_REGS + SP*4], %g4 ! user's saved stack pointer
	SAVE_WINDOW(%sp)		! save kernel copy of window

	TRACE_OVFL(TT_OV_SHRK, %sp, %g1, %g2, %g4)	/* no local regs here */
	!
	! Determine the user's real stack pointer.
	!
	set     KERNELBASE, %g1
	!
	! In order to save the window onto the stack, the stack
	! must be aligned on a word boundary, and the part of the
	! stack where the save will be done must be present.
	! We first check for alignment.
	!
	btst	0x7, %g4		! test sp alignment
	bnz	.wo_save_to_buf		! misaligned very odd case
	cmp     %g1, %g4		! delay - compare with KERNELBASE
	bleu    .wo_save_to_buf		! stack is above KERNELBASE
	mov     %g4, %g2
	!
	! Most of the time the stack is resident in main memory,
	! so we don't verify its presence before attempting to save
	! the window onto the stack.  Rather, we simply set the
	! no-fault bit of the SRMMU's control register, so that
	! doing the save won't cause another trap in the case
	! where the stack is not present.  By checking the
	! synchronous fault status register, we can determine
	! whether the save actually worked (ie. stack was present),
	! or whether we first need to fault in the stack.
	! Other sun4 trap handlers first probe for the stack, and
	! then, if the stack is present, they store to the stack.
	! This approach CANNOT be used with a multiprocessor system
	! because of a race condition: between the time that the
	! stack is probed, and the store to the stack is done, the
	! stack could be stolen by the page daemon.

	set	RMMU_FSR_REG, %g1	! clear any old faults out
	lda	[%g1]ASI_MOD, %g0	! of the SFSR.

	lda	[%g0]ASI_MOD, %g2	! turn on no-fault bit in
	or	%g2, MMCREG_NF, %g1	! mmu control register to
	sta	%g1, [%g0]ASI_MOD	! prevent taking a fault.

	SAVE_WINDOW(%g4)		! try to save reg window

	sta	%g2, [%g0]ASI_MOD	! turn off no-fault bit

	set	RMMU_FAV_REG, %g2
	lda	[%g2]ASI_MOD, %g2	! read SFAR
	set	RMMU_FSR_REG, %g1	! clear any old faults out
	lda	[%g1]ASI_MOD, %g1	! read SFSR

	btst	MMU_SFSR_FAV, %g1	! did a fault occurr?
	bnz	.wo_save_to_buf		! yes, branch.
	nop

	TRACE_OVFL(TT_OV_SHR, %g4, %l1, %l2, %l3)	/* locals OK to use */

	restore				! get back to orig window
	mov	%l7, %g1		! restore g1
	mov	%l4, %g2		! restore g2
	mov	%l5, %g3		! restore g3
	b	1f
	mov	%l6, %g4		! delay - restore g4

	!
	! Window to be saved is a supervisor window.
	! Put it on the stack.
	!
.wo_kt_window:
	save				! get into window to be saved
	mov	%g1, %wim		! install new wim
	SAVE_WINDOW(%sp)

	TRACE_OVFL(TT_OV_SYS, %sp, %l1, %l2, %l3) /* locals usable after SAVE */

	restore				! go back to trap window
1:
	cmp	%l1, -1			! return with a rett?
	bne	0f
	mov	%l0, %psr		! reinstall system PSR_CC
	mov	%l7, %g1		! restore g1
	jmp	%l2+8			! return to caller
	mov	%l5, %g3		! restore g3
0:
	mov	%l7, %g1		! restore g1
	mov	%l5, %g3		! restore g3
	jmp	%l1			! reexecute save
	rett	%l2

.wo_stack_not_res:
	!
	! The fault occurred, so the stack is not resident.
	!
	mov	%psr, %g4		! not in trap window.
	btst	PSR_PS, %g4		! test for user or sup trap
	bnz,a	.wo_save_to_buf		! sup trap, save window in PCB buf
	mov	%sp, %g4		! delay - sp to save
	!
	! We first save the window in the first window buffer in the PCB area.
	! Then we fake a user data fault. If the fault succeeds, we will
	! reexecute the save and overflow again, but this time the page
	! will be resident
	!
	st	%sp, [%g3 + MPCB_SPBUF]	! save sp
	SAVE_WINDOW(%g3 + MPCB_WBUF)

	TRACE_OVFL(TT_OV_BUF, %sp, %l1, %l2, %l3) /* locals usable after SAVE */
	restore				! get back into original window
	!
	! Set the save buffer ptr to next buffer
	!
	mov	1, %g4
	st	%g4, [%g3 + MPCB_WBCNT]	! mpcb->mpcb_wbcnt = 1
	!
	! Compute the user window mask (mpcb->mpcb_uwm), which is a
	! mask of which windows contain user data.  In this case it is all
	! the registers except the one at the old WIM and the one we just saved.
	!
	mov	%wim, %g4		! get new WIM
	or	%g4, %l3, %g4		! or in old WIM
					! note %l3 is now free.
	not	%g4
	sethi	%hi(winmask), %l3
	ld	[%l3 + %lo(winmask)], %l3
	andn	%g4, %l3, %g4		! apply winmask
	st	%g4, [%g3 + MPCB_UWM]	! mpcb->mpcb_uwm = ~(OWIM|NWIM)

	mov	%g3, %sp		! mpcb doubles as initial stack
	ld	[%g3 + MPCB_THREAD], %g3 ! get thread pointer
	mov	%g1, %l3		! save SFSR
	mov	%l7, %g1		! restore g1
	mov	%g2, %l7		! save SFAR
	mov	%l6, %g4		! restore g4
	mov	%g3, %l6		! save thread pointer
	mov	%l5, %g3		! restore g3
	mov	%l4, %g2		! restore g2
	SAVE_GLOBALS(%sp + MINFRAME)
	mov	%l6, THREAD_REG		! finally set thread pointer for trap
	SAVE_OUTS(%sp + MINFRAME)
	st	%l0, [%sp + MINFRAME + PSR*4] ! psr
	st	%l1, [%sp + MINFRAME + PC*4] ! pc
	st	%l2, [%sp + MINFRAME + nPC*4] ! npc
	mov	%l3, %o3		! fault status.
	wr	%l0, PSR_ET, %psr	! enable traps
	TRACE_ASM_1 (%o2, TR_FAC_TRAP, TR_TRAP_START, 0, T_WIN_OVERFLOW);
	mov	T_WIN_OVERFLOW, %o0
	add	%sp, MINFRAME, %o1
	mov	%l7, %o2		! fault address.
	call	trap			! trap(T_WIN_OVERFLOW,
	mov	S_WRITE, %o4		!	rp, addr, be, S_WRITE)

	b,a	_sys_rtt		! return

.wo_save_to_buf:
	!
	! The user's stack is not accessible while trying to save a user window
	! during a supervisor overflow.  We save the window in the PCB to
	! be processed when we return to the user.
	!
	ld	[%g3 + MPCB_THREAD], %g2
chkt_intr:				! follow thread->t_intr to get a
	ld	[%g2 + T_INTR], %g1	! non-interrupt thread
	tst	%g1
	bnz,a	chkt_intr
	mov	%g1, %g2
	mov	1, %g1
	stb	%g1, [%g2 + T_ASTFLAG]	! set AST so WBCNT will be seen
	ld	[%g3 + MPCB_WBCNT], %g1	! pcb_wbcnt*4 is offset into pcb_spbuf
	add	%g1, 1, %g2		! increment pcb_wbcnt to reflect save
	st	%g2, [%g3 + MPCB_WBCNT]

	sll	%g1, 2, %g2
	add	%g3, %g2, %g2		! save sp in mpcb_wbcnt*4 + pcb + spbuf
	st	%g4, [%g2 + MPCB_SPBUF]	! save user's sp from %g4
	sll	%g1, 6, %g1		! pcb_wbcnt*64 is offset into mpcb_wbuf
	add	%g3, MPCB_WBUF, %g2	! calculate mpcb_wbuf
	add	%g1, %g2, %g1		! save window to mpcb_wbuf+64*mpcb_wbcnt
	SAVE_WINDOW(%g1)
	TRACE_OVFL(TT_OV_BUFK, %g4, %l1, %l2, %l3) /* %l1-3 usable after SAVE */
	restore				! get back to orig window
	!
	!
	! Return to supervisor. Rett will not underflow since traps
	! were never disabled.
	!
	mov	%l7, %g1		! restore g1
	mov	%l4, %g2		! restore g2
	cmp	%l1, -1			! return with a rett?
	bne	0f
	mov	%l0, %psr		! reinstall system PSR_CC
	mov	%l5, %g3		! restore g3
	jmp	%l2+8			! return to caller
	mov	%l6, %g4		! restore g4
0:
	mov	%l5, %g3		! restore g3
	mov	%l6, %g4		! restore g4
	jmp	%l1			! reexecute save
	rett	%l2

	!
	! Misaligned sp.  If this is a userland trap fake a memory alignment
	! trap.  Otherwise, put the window in the window save buffer so that
	! we can catch it again later.
	!
.wo_misaligned:
	mov	%psr, %g1		! get psr (we are not in trap window)
	btst	PSR_PS, %g1		! test for user or sup trap
	bnz,a	.wo_save_to_buf		! sup trap, save window in PCB buf
	mov	%sp, %g4		! delay - stack pointer to put in buf

	restore				! get back to orig window
	mov	%l6, %g4		! restore g4
	mov	%l5, %g3		! restore g3
	mov	%l4, %g2		! restore g2
	mov	%l7, %g1		! restore g1
	mov	%l3, %wim		! restore old wim, so regs are dumped
	b	sys_trap
	mov	T_ALIGNMENT, %l4	! delay slot, fake alignment trap

	!
	! Maintain clean windows.
	!
.clean_windows:
	mov	%l1, %o6		! put pc, npc in an unobtrusive place
	mov	%l2, %o7
	clr	%l0			! clean the rest
	clr	%l1
	clr	%l2
	clr	%l3
	clr	%l4
	clr	%l5
	clr	%l6
	clr	%l7
	clr	%o0
	clr	%o1
	clr	%o2
	clr	%o3
	clr	%o4
	clr	%o5
	jmp	%o6			! reexecute save
	rett	%o7
	SET_SIZE(ross625_window_overflow)

#endif	/* lint */
