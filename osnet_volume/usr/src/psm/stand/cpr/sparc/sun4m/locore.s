/*
 * Copyright (c) 1991 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)locore.s 1.10	96/04/16 SMI"

#include <sys/errno.h>
#include <sys/param.h>
#include <sys/asm_linkage.h>
#include <v7/sys/privregs.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/enable.h>
#include <sys/cpu.h>
#include <sys/trap.h>
#include <sys/eeprom.h>

#if defined(lint)

 
#else   /* lint */

/*
 * The debug stack. This must be the first thing in the data
 * segment (other than an sccs string) so that we don't stomp
 * on anything important. We get a red zone below this stack
 * for free when the text is write protected.
 */
#define	STACK_SIZE	0xc000
	.seg	".data"
	.align	16			! needs to be on double word boundry

	.global estack
	.skip	STACK_SIZE
estack:					! end (top) of debugger stack
fpuregs:
	.skip	33*4			! %f0 - %f31 plus %fsr

first:
	.word	1			! flags fist entry into kadb

/*
 * Current cpuid info
 */
	.global	cur_cpuid
cur_cpuid:
	.word	0

/*
 * The number of windows, set by fiximp when machine is booted.
 */
	.global nwindows
nwindows:
	.word	8

/*
 * The prom's trap base register.
 */
	.global mon_tbr

/*
 * The parent callback routine.
 */
        .global release_parent
release_parent:
        .word   0

	.seg	".bss"
	.align	16
	.globl	bss_start
bss_start:

	.seg	".text"

/*
 * Trap vector macros.
 */
#define TRAP(H) \
	b (H); mov %psr,%l0; nop; nop;

/*
 * The constant in the last expression must be (nwindows-1).
 * XXX - See fiximp() below.
 */
#define WIN_TRAP(H) \
	mov %psr,%l0;  mov %wim,%l3;  b (H);  mov 7,%l6;

#define SYS_TRAP(T) \
	mov %psr,%l0;  sethi %hi(T),%l3;  b sys_trap;  or %l3,%lo(T),%l3;

#define BAD_TRAP	SYS_TRAP(fault);

/*
 * Trap vector table.
 * This must be the first text in the boot image.
 *
 * When a trap is taken, we vector to DEBUGSTART+(TT*16) and we have
 * the following state:
 *	2) traps are disabled
 *	3) the previous state of PSR_S is in PSR_PS
 *	4) the CWP has been decremented into the trap window
 *	5) the previous pc and npc is in %l1 and %l2 respectively.
 *
 * Registers:
 *	%l0 - %psr immediately after trap
 *	%l1 - trapped pc
 *	%l2 - trapped npc
 *	%l3 - trap handler pointer (sys_trap only)
 *		or current %wim (win_trap only)
 *	%l6 - NW-1, for wim calculations (win_trap only)
 *
 * Note: DEBUGGER receives control at vector 0 (trap).
 */
	.seg	".text"
	.align	16

	.global start, scb
start:
scb:
	TRAP(.enter);				! 00
	BAD_TRAP;				! 01 text fault
	WIN_TRAP(unimpl_instr);			! 02 unimp instruction
	BAD_TRAP;				! 03 priv instruction
	TRAP(fp_disabled);			! 04 fp disabled
	WIN_TRAP(window_overflow);		! 05
	WIN_TRAP(window_underflow);		! 06
	BAD_TRAP;				! 07 alignment
	BAD_TRAP;				! 08 fp exception
	SYS_TRAP(fault);			! 09 data fault
	BAD_TRAP;				! 0A tag_overflow
	BAD_TRAP; BAD_TRAP;			! 0B - 0C
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 0D - 10
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 11 - 14 int 1-4
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 15 - 18 int 5-8
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 19 - 1C int 9-12
	BAD_TRAP; BAD_TRAP;			! 1D - 1E int 13-14
	SYS_TRAP(level15);			! 1F int 15
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 20 - 23
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 24 - 27
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 28 - 2B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 2C - 2F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 30 - 34
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 34 - 37
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 38 - 3B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 3C - 3F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 40 - 44
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 44 - 47
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 48 - 4B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 4C - 4F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 50 - 53
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 54 - 57
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 58 - 5B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 5C - 5F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 60 - 64
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 64 - 67
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 68 - 6B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 6C - 6F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 70 - 74
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 74 - 77
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP;	! 78 - 7B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 7C - 7F
	!
	! software traps
	!
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 80 - 83
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 84 - 87
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 88 - 8B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 8C - 8F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 90 - 93
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 94 - 97
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 98 - 9B
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! 9C - 9F
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! A0 - A3
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! A4 - A7
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! A8 - AB
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! AC - AF
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! B0 - B3
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! B4 - B7
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! B8 - BB
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! BC - BF
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! C0 - C3
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! C4 - C7
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! C8 - CB
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! CC - CF
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! D0 - D3
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! D4 - D7
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! D8 - DB
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! DC - DF
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! E0 - E3
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! E4 - E7
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! E8 - EB
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! EC - EF
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! F0 - F3
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! F4 - F7
	BAD_TRAP; BAD_TRAP; BAD_TRAP; BAD_TRAP; ! F8 - FB
	BAD_TRAP; BAD_TRAP; BAD_TRAP; 		! FC - FE
	BAD_TRAP;				! PROM breakpoint trap


LPSR = 0*4
LPC = 1*4
LNPC = 2*4
LSP = 3*4
LG1 = 4*4
LG2 = 5*4
LG3 = 6*4
LG4 = 7*4
LG5 = 8*4
LG6 = 9*4
LG7 = 10*4
REGSIZE = 11*4

/*
 * cprboot entry point.
 * Check the "fist" flag to see if cprboot has had a chance to initialize
 * itself yet.  If it has all ready, trap into kadb.  Otherwise, take
 * care of the initialization.
 *
 * NOTE:  All that old-postion-independent code that dealt with relocation
 * is gone.  We don't need to do that anymore.
 *
 *	%o0:	romp
 *	%o1:	shim
 *	%o2:	bootops
 */

.enter:
init:
	mov	%psr, %g1
	bclr	PSR_PIL, %g1		! PIL = 15
	bset	(13 << 8), %g1
	mov	%g1, %psr
	nop; nop; nop

	!
	! Save romp and bootops.
	!
	call	early_startup
	nop

	!
	! Using the boot prom's %tbr.
	!
	call	module_setup		! setup correct module routines
	lda	[%g0]ASI_MOD, %o0	! find module type

	!
	! Take over the world. Save the current stack pointer, as
	! we're going to need it for a little while longer.
	!
	mov	%sp, %g2
	set	scb, %g1		! setup kadb tbr
!	mov 	%g1, %tbr
	nop; nop; nop
	mov	0x2, %wim
	mov 	%psr, %g1
	bclr 	PSR_CWP, %g1
	mov 	%g1, %psr
	nop; nop; nop;			! psr delay
	sub	%g2, SA(MINFRAME), %sp

	!
	! Fix the implementation dependent parameters.
	!
.fiximp:
	!
	! Find out how many windows we have and set the global variable.
	! We must be in window 0 for this to work.
	!
	set	nwindows, %g2
	save				! CWP is now (nwindows - 1)
	mov	%psr, %g1
	and	%g1, PSR_CWP, %g1
	inc	%g1
	st	%g1, [%g2]
	restore
	!
	!
	! switch over to our new stack
	!
	! XXX: ideally we want to setup a return stack back
	!      to boot, but let's do that later
	!
	set	estack, %g2
	sub	%g2, SA(MINFRAME), %g2	! setup top (null) window

	sub	%g2, SA(MINFRAME+REGSIZE), %sp	! the 1st working frame
	st	%g2, [%sp + MINFRAME + LSP]	! link to last frame

	!
	! call main to enter the debugger
	!
	call	main
	nop

	!
	! In the unlikely event we get here, return to the monitor.
	!
	call	prom_exit_to_mon
	clr	%o0


/*
 * General debugger trap handler.
 * This is only used by traps that happen while the debugger is running.
 * It is not used for any debuggee traps.
 * Does overflow checking then vectors to trap handler.
 */
sys_trap:
	!
	! Prepare to go to C (batten down the hatches).
	! Save volatile regs.
	!
	sub	%fp, SA(MINFRAME+REGSIZE), %l7 ! make room for reg save area
	st	%g1, [%l7 + MINFRAME + LG1]
	st	%g2, [%l7 + MINFRAME + LG2]
	st	%g3, [%l7 + MINFRAME + LG3]
	st	%g4, [%l7 + MINFRAME + LG4]
	st	%g5, [%l7 + MINFRAME + LG5]
	st	%g6, [%l7 + MINFRAME + LG6]
	st	%g7, [%l7 + MINFRAME + LG7]
	st	%fp, [%l7 + MINFRAME + LSP]
	st	%l0, [%l7 + MINFRAME + LPSR]
	st	%l1, [%l7 + MINFRAME + LPC]
	st	%l2, [%l7 + MINFRAME + LNPC]
	!
	! Check for window overflow.
	!
	mov	0x01, %l5		! CWM = 0x01 << CWP
	sll	%l5, %l0, %l5
	mov	%wim, %l4		! get WIM
	btst	%l5, %l4		! compare WIM and CWM
	bz	st_have_window
	nop
	!
	! The next window is not empty. Save it.
	!
	sethi	%hi(nwindows), %l6
	ld	[%l6 + %lo(nwindows)], %l6
	dec	%l6			
	srl	%l4, 1, %g1		! WIM = %g1 = ror(WIM, 1, NWINDOW)
	sll	%l4, %l6, %l4		! %l6 = NWINDOW - 1 
	or	%l4, %g1, %g1
	save				! get into window to be saved
	mov	%g1, %wim		! install new WIM
	SAVE_WINDOW(%sp)
	restore				! get back to original window
	!
	! The next window is available.
	!
st_have_window:
	mov	%l7, %sp		! install new sp
	mov	%tbr, %o0		! get trap number
	bclr	PSR_PIL, %l0		! PIL = 15
	bset	(15 << 8), %l0
	wr	%l0, PSR_ET, %psr	! enable traps
	srl	%o0, 4, %o0		! psr delay
	and	%o0, 0xff, %o0
	ld	[%l7 + MINFRAME + LPC], %o1
	ld	[%l7 + MINFRAME + LNPC], %o2
	mov	%l3, %g1
	jmpl	%g1, %o7		! call handler
	nop
	!
	! Return from trap.
	!
	ld	[%l7 + MINFRAME + LPSR], %l0 ! get saved psr
	bclr	PSR_PIL, %l0		! PIL = 14
	bset	(14 << 8), %l0
	mov	%psr, %g1		! use current CWP
	mov	%g1,%g3
	and	%g1, PSR_CWP, %g1
	andn	%l0, PSR_CWP, %l0
	or	%l0, %g1, %l0
	mov	%l0, %g1
	mov	%l0, %psr		! install old psr, disable traps
	nop;nop;nop
	!
	! Make sure that there is a window to return to.
	!
	mov	0x2, %g1		! compute mask for CWP + 1
	sll	%g1, %l0, %g1
	sethi	%hi(nwindows), %g3
	ld	[%g3 + %lo(nwindows)], %g3
	srl	%g1, %g3, %g2
	or	%g1, %g2, %g1
	mov	%wim, %g2		! cmp with wim to check for underflow
	btst	%g1, %g2
	bz	sr_out
	nop
	!
	! No window to return to. Restore it.
	!
	sll	%g2, 1, %g1		! compute new WIM = rol(WIM, 1, NWINDOW)
	dec	%g3			! %g3 is now NWINDOW-1
	srl	%g2, %g3, %g2
	or	%g1, %g2, %g1
	mov	%g1, %wim		! install it
	nop; nop; nop;			! wim delay
	restore				! get into window to be restored
	RESTORE_WINDOW(%sp)
	save				! get back to original window
	!
	! There is a window to return to.
	! Restore the volatile regs and return.
	!
sr_out:
	mov	%l0, %psr		! install old PSR_CC
	ld	[%l7 + MINFRAME + LG1], %g1
	ld	[%l7 + MINFRAME + LG2], %g2
	ld	[%l7 + MINFRAME + LG3], %g3
	ld	[%l7 + MINFRAME + LG4], %g4
	ld	[%l7 + MINFRAME + LG5], %g5
	ld	[%l7 + MINFRAME + LG6], %g6
	ld	[%l7 + MINFRAME + LG7], %g7
	ld	[%l7 + MINFRAME + LSP], %fp
	ld	[%l7 + MINFRAME + LPC], %l1
	ld	[%l7 + MINFRAME + LNPC], %l2
	jmp	%l1
	rett	%l2
	.empty

/*
 * Trap handlers.
 */

/*
 * Window overflow trap handler.
 * On entry, %l3 = %wim, %l6 = nwindows-1
 */
	ENTRY(window_overflow)
	!
	! Compute new WIM.
	!
	mov	%g1, %l7		! save %g1
	srl	%l3, 1, %g1		! next WIM = %g1 = ror(WIM, 1, NWINDOW)
	sll	%l3, %l6, %l4		! %l6 = NWINDOW-1
	or	%l4, %g1, %g1
	save				! get into window to be saved
	mov	%g1, %wim		! install new wim
	nop; nop; nop;			! wim delay
	!
	! Put it on the stack.
	!
	SAVE_WINDOW(%sp)
	restore				! go back to trap window
	mov	%l7, %g1		! restore g1
	jmp	%l1			! reexecute save
	rett	%l2

/*
 * Window underflow trap handler.
 * On entry, %l3 = %wim, %l6 = nwindows-1
 */
	ENTRY(window_underflow)
	sll	%l3, 1, %l4		! next WIM = rol(WIM, 1, NWINDOW)
	srl	%l3, %l6, %l5		! %l6 = NWINDOW-1
	or	%l5, %l4, %l5
	mov	%l5, %wim		! install it
	nop; nop; nop;			! wim delay
	restore				! (wim delay 3) get into last window
	restore				! get into window to be restored
	RESTORE_WINDOW(%sp)
	save				! get back to original window
	save
	jmp	%l1			! reexecute restore
	rett	%l2

/*
 * Unimplemented instruction trap.
 * Need to check for iflush here for implementations that lack it.
 */
	ENTRY(unimpl_instr)
	ld	[%l1], %l4		! get illegal instruction
	set	0xc1f80000, %l6		! mask for the iflush instruction
	and	%l4, %l6, %l4
	set	0x81d80000, %l6		! and check the opcode
	cmp	%l4, %l6		! iflush -> nop
	bne	1f
	nop
	jmp	%l2			! skip illegal iflush instruction
	rett	%l2 + 4
1:
	BAD_TRAP;			! else it's a bap trap

/*
 * Misc subroutines.
 */


	.seg	".data"
	.align	4
	.global	_fpu_exists
_fpu_exists:
	.word	1			! assume FPU exists

	.seg	".text"
	.align	4

zero:	.word	0

/*
 * floating point disabled trap.
 * if FPU does not exist, emulate instruction
 * otherwise, enable floating point
 */
	.global fp_disabled
fp_disabled:
	!
	! if we get an fp_disabled trap and the FPU is enabled
	! then there is not an FPU in the configuration
	!
	set	PSR_EF, %l5		! FPU enable bit
	btst	%l5, %l0		! was the FPU enabled?
	sethi	%hi(_fpu_exists), %l6
	bz,a	1f			! fp was disabled, fix up state
	ld	[%l6 + %lo(_fpu_exists)], %l5	! else clear fpu_exists
	!
	! fp_disable trap when the FPU is enabled; should only happen
	! once from autoconf when there is not an FPU in the board
	!
	clr	[%l6 + %lo(_fpu_exists)] ! FPU does not exist in configuration
	set 	PSR_EF, %l4
	bclr	%l4, %l0
	mov	%l0, %psr
	nop;nop;nop
1:
	jmp	%l2			! return from trap skip inst
	rett	%l2 + 4


#endif  /* lint */
