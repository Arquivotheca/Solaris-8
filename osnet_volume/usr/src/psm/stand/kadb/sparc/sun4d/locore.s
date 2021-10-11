/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)locore.s	1.22	99/04/13 SMI" /* From SunOS 4.1.1 */

#include "assym.s"
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/asm_linkage.h>
#include <sys/mmu.h>
#include <sys/privregs.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/eeprom.h>
#include <sys/debug/debug.h>

/*
 * The debug stack. This must be the first thing in the data
 * segment (other than an sccs string) so that we don't stomp
 * on anything important. We get a red zone below this stack
 * for free when the text is write protected.
 */
#define	STACK_SIZE	0x8000

#if !defined(lint)

	.seg	".data"
	.global estack
	.align	8
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
 * lock used by kadb
 */
	.global kadblock

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
	BAD_TRAP;				! 02 unimp instruction
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
	BAD_TRAP;				! FC 
	TRAP(trap);				! FD enter debugger
	TRAP(trap);				! FE breakpoint
	BAD_TRAP;				! PROM breakpoint trap

/*
 * Debugger vector table.
 * Must follow trap table.
 */
dvec:
	b,a	.enter			! dv_entry
	.word	trap			! dv_trap
	.word	pagesused		! dv_pages
	.word	scbsync			! dv_scbsync
	.word	DEBUGVEC_VERSION_0	! dv_version

/*
 * defines for figuring out the cpu id
 */
#define	ASI_BB			0x2f
#define	BB_BASE			0xf0000000
#define	XOFF_BOOTBUS_STATUS3	0x14
#define	BB_CPU_MASK		0xf8
#define	BB_CPU_SHIFT		3

/*
 * Debugger entry point.
 * Check the "first" flag to see if kadb has had a chance to initialize
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
	sethi	%hi(first), %o5
	ld	[%o5 + %lo(first)], %o4	! read "first" flag
	tst	%o4			! been through here before?
	bne,a	init			! non-zero --> no
	st	%g0, [%o5 + %lo(first)] ! clear flag

	!
	! Enter debugger by doing a software trap.
	! We ASSUME that the software trap we have set up is still there.
	!
	t	ST_KADB_TRAP
	nop
	retl
	nop

	!
	! We have not been relocated yet.
	!
init:
	mov	%psr, %g1
	bclr	PSR_PIL, %g1		! PIL = 15
	bset	(15 << 8), %g1
	mov	%g1, %psr
	nop; nop; nop

	!
	! Save romp and bootops.
	!
	call	early_startup
	nop

	sethi	%hi(kadblock), %o0	! clear kadblock
	stb	%g0, [%o0 + %lo(kadblock)]

	!
	! Save monitor's level14 clock interrupt vector code and trap #0.
	!
	mov     %tbr, %l4               ! save monitor's tbr
	bclr    0xfff, %l4              ! remove tt

	or      %l4, TT(T_INT_LEVEL_14), %l4
	set     scb, %l5
	or      %l5, TT(T_INT_LEVEL_14), %l5
	ldd	[%l4], %o0
	std	%o0, [%l5]
	ldd	[%l4+8], %o0
	std	%o0, [%l5+8]

	bclr    0xfff, %l4              ! remove tt
	or      %l4, TT(T_OSYSCALL), %l4
	bclr    0xfff, %l5
	or      %l5, TT(T_OSYSCALL), %l5
	ldd	[%l4], %o0
	std	%o0, [%l5]
	ldd	[%l4+8], %o0
	std	%o0, [%l5+8]

	bclr    0xfff, %l4              ! remove tt
	or      %l4, TT(ST_MON_BREAKPOINT + T_SOFTWARE_TRAP), %l4
	bclr    0xfff, %l5
	or      %l5, TT(ST_MON_BREAKPOINT + T_SOFTWARE_TRAP), %l5
	ldd	[%l4], %o0
	std	%o0, [%l5]
	ldd	[%l4+8], %o0
	std	%o0, [%l5+8]
	!
	! Take over the world. Save the current stack pointer, as
	! we're going to need it for a little while longer.
	!
	mov	%sp, %g2
	set	scb, %g1		! setup kadb tbr
	mov 	%g1, %tbr
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
	! Fix the number of windows in the trap vectors.
	! The last byte of the window handler trap vectors must be equal to
	! the number of windows in the implementation minus one.
	!
	dec	%g1			! last byte of trap vectors get NW-1
	set	scb, %g2
	add	%g2, (5 << 4), %g2	! window overflow trap handler offset
	stb	%g1, [%g2 + 15]		! write last byte of trap vector
	add	%g2, 16, %g2		! now get window underflow trap
	stb	%g1, [%g2 + 15]		! write last byte of trap vector
	!
	! Get module id info.
	!
	set	BB_BASE + (XOFF_BOOTBUS_STATUS3 << 16), %l5
	lduba	[%l5]ASI_BB, %l5
	and	%l5, BB_CPU_MASK, %l5
	srl	%l5, BB_CPU_SHIFT, %l5
	sethi	%hi(cur_cpuid), %l3
	st	%l5, [%l3 + %lo(cur_cpuid)]
	!
	! Call startup to do the rest of the startup work.
	!
	call	startup
	nop
	!
	! call main to enter the debugger
	!
	call	main
	nop
	mov	%psr, %g1
	bclr	PSR_PIL, %g1		! PIL = 14
	bset	(14 << 8), %g1
	mov	%g1, %psr
	nop;nop;nop
	t	ST_KADB_TRAP
	!
	! In the unlikely event we get here, return to the monitor.
	!
	call	prom_exit_to_mon
	clr	%o0

/*
 * exitto(addr)
 * int *addr;
 */

	ENTRY(_exitto)
	save	%sp, -SA(MINFRAME), %sp
	sethi	%hi(mon_tbr), %l3	! re-install prom's %tbr
	ld	[%l3 + %lo(mon_tbr)], %l3
	mov	%l3, %tbr
	nop				! tbr delay
	set	bootops, %o0		! pass bootops to callee
	ld	[%o0], %o2
	set	dvec, %o1		! pass dvec address to callee
	sethi	%hi(elfbootvec), %o3	! pass elf bootstrap vector
	ld	[%o3 + %lo(elfbootvec)], %o3
	clr	%o4			! 1210381 - no 1275 cif
	set	romp, %o0		! pass the romp to callee
	jmpl	%i0, %o7		! register-indirect call
	ld	[%o0], %o0
	ret
	restore

/*
 * This is where breakpoint traps go.
 * We assume we are in the normal condition after a trap.
 */
	ENTRY(trap)
	!
	! Get module id info.
	!
	set	BB_BASE + (XOFF_BOOTBUS_STATUS3 << 16), %l5
	lduba	[%l5]ASI_BB, %l5
	and	%l5, BB_CPU_MASK, %l5
	srl	%l5, BB_CPU_SHIFT, %l5

	!
	! Try to acquire the mutex.
	!
	set	kadblock, %l3
	ldstub	[%l3], %l4
1:
	tst	%l4			! Is the lock held?
	bnz	2f			! lock already held - go spin
	nop

	!
	! The lock is held by another cpu so spin until it is
	! released.
	!
	b,a	4f
2:
	ldub	[%l3], %l4
3:
	tst	%l4			! Is the lock held?
	bz,a	1b			! lock appears to be free, try again
	ldstub	[%l3], %l4		! delay slot - try to set lock
	ba	3b			! otherwise, spin
	ldub	[%l3], %l4		! delay - reload
4:

	sethi	%hi(cur_cpuid), %l3
	st	%l5, [%l3 + %lo(cur_cpuid)]
	!
	! dump the whole cpu state (all windows) on the stack.
	!
	set	regsave, %l3
	st	%l0, [%l3 + R_PSR]
	st	%l1, [%l3 + R_PC]
	st	%l2, [%l3 + R_NPC]
	mov	%wim, %l4
	st	%l4, [%l3 + R_WIM]
	mov	%g0, %wim		! zero wim so that we can move around
	mov	%tbr, %l4
	st	%l4, [%l3 + R_TBR]
	mov	%y, %l4
	st	%l4, [%l3 + R_Y]
	st	%g1, [%l3 + R_G1]
	st	%g2, [%l3 + R_G2]
	st	%g3, [%l3 + R_G3]
	st	%g4, [%l3 + R_G4]
	st	%g5, [%l3 + R_G5]
	st	%g6, [%l3 + R_G6]
	st	%g7, [%l3 + R_G7]
	add	%l3, R_WINDOW, %g7
	sethi	%hi(nwindows), %g6
	ld	[%g6 + %lo(nwindows)], %g6
	bclr	PSR_CWP, %l0		! go to window 0
	mov	%l0, %psr
	nop; nop; nop;			! psr delay
1:
	st	%l0, [%g7 + 0*4]	! save locals
	st	%l1, [%g7 + 1*4]
	st	%l2, [%g7 + 2*4]
	st	%l3, [%g7 + 3*4]
	st	%l4, [%g7 + 4*4]
	st	%l5, [%g7 + 5*4]
	st	%l6, [%g7 + 6*4]
	st	%l7, [%g7 + 7*4]
	st	%i0, [%g7 + 8*4]	! save ins
	st	%i1, [%g7 + 9*4]
	st	%i2, [%g7 + 10*4]
	st	%i3, [%g7 + 11*4]
	st	%i4, [%g7 + 12*4]
	st	%i5, [%g7 + 13*4]
	st	%i6, [%g7 + 14*4]
	st	%i7, [%g7 + 15*4]
	add	%g7, WINDOWSIZE, %g7
	subcc	%g6, 1, %g6		! all windows done?
	bnz	1b
	restore				! delay slot, increment CWP
	!
	! Back in window 0.
	!
	set	regsave, %g2		! need to get back to state of entering
	ld	[%g2 + R_WIM], %g1
	mov	%g1, %wim
	ld	[%g2 + R_PSR], %g1
	bclr	PSR_PIL, %g1		! PIL = 14
	bset	(14 << 8), %g1
	mov     %g1, %psr		! go back to orig window
	nop;nop;nop
	!
	! Now we must make sure all the window stuff goes to memory.
	! Flush all register windows to the stack.
	! But wait! If we trapped into the invalid window, we can't just
	! save because we'll slip under the %wim tripwire.
	! Do one restore first, so subsequent saves are guaranteed
	! to flush the registers. (Don't worry about the restore
	! triggering a window underflow, since if we're in a trap
	! window the next window up has to be OK.)
	! Do all this while still using the kernel %tbr: let it worry
	! about user windows and user stack faults.
	!
	restore
	mov	%psr, %g1		! get new CWP
	wr	%g1, PSR_ET, %psr	! enable traps
	nop;nop;nop
	save	%sp, -SA(MINFRAME), %sp	! now we're back in the trap window
	mov	%sp, %g3
	mov	%fp, %g4
	sethi	%hi(nwindows), %g7
	ld	[%g7 + %lo(nwindows)], %g7
	sub	%g7, 2, %g6		! %g6 = NWINDOW - 2
1:
	deccc	%g6			! all windows done?
	bnz	1b
	save	%sp, -WINDOWSIZE, %sp
	sub	%g7, 2, %g6		! %g6 = NWINDOW - 2
2:
	deccc	%g6			! all windows done?
	bnz	2b
	restore				! delay slot, increment CWP


	! kadb has been using the kernel window overflow handlers
	! to push the windows onto the stack.  When kadb returns
	! to the kernel, the %wim registers must reflect this
	! usage.  The CWP in the %psr must also reflect the usage.
	! Make sure that there is one frame available for
	! the return from trap when this %wim is restored.
	! Save the %wim register here after all the saves
	! and restores so that the side effects of the saves
	! and restores will be seem by the kernel.
	! The CWP in the saved %psr should also reflect the usage
	! but here the CWP has not changed from that in the %psr
	! originally saved (equal saves and restores done).
	! Also, we don't want to save save a current
	! %psr that has had the ET and PIL modified as above.

	restore
	save	%sp, -SA(MINFRAME), %sp
	set	regsave, %g1
	mov	%wim, %g5
	st	%g5, [%g1 + R_WIM]


	mov	%psr, %g1
	wr	%g1, PSR_ET, %psr	! disable traps while working
	mov	2, %wim			! setup wim
	bclr	PSR_CWP, %g1		! go to window 0
	bclr	PSR_PIL, %g1		! PIL = 14
	bset	(14 << 8), %g1
	wr	%g1, PSR_ET, %psr	! rewrite %psr, but keep traps disabled
	nop; nop; nop
	mov	%g3, %sp		! put back %sp and %fp
	mov	%g4, %fp
	mov	%g1, %psr		! now enable traps
	nop; nop; nop

	! save fpu
	sethi	%hi(_fpu_exists), %g1
	ld	[%g1 + %lo(_fpu_exists)], %g1
	tst	%g1			! don't bother if no fpu
	bz	1f
	nop
	set	regsave, %l3		! was it on?
	ld	[%l3 + R_PSR], %l0
	set	PSR_EF, %l5		! FPU enable bit
	btst	%l5, %l0		! was the FPU enabled?
	bz	1f
	nop
	!
	! Save floating point registers and status.
	! All floating point operations must be complete.
	! Storing the fsr first will accomplish this.
	!
	set	fpuregs, %g7
	st	%fsr, [%g7+(32*4)]
	std	%f0, [%g7+(0*4)]
	std	%f2, [%g7+(2*4)]
	std	%f4, [%g7+(4*4)]
	std	%f6, [%g7+(6*4)]
	std	%f8, [%g7+(8*4)]
	std	%f10, [%g7+(10*4)]
	std	%f12, [%g7+(12*4)]
	std	%f14, [%g7+(14*4)]
	std	%f16, [%g7+(16*4)]
	std	%f18, [%g7+(18*4)]
	std	%f20, [%g7+(20*4)]
	std	%f22, [%g7+(22*4)]
	std	%f24, [%g7+(24*4)]
	std	%f26, [%g7+(26*4)]
	std	%f28, [%g7+(28*4)]
	std	%f30, [%g7+(30*4)]
1:
	set	scb, %g1		! setup kadb tbr
	mov	%g1, %tbr
	nop				! tbr delay

	!
	! Entering kadb, so idle the other CPUs.  For now
	! this is accomplished by turning off the Arbiter Enable
	! register of the appropriate CPU.
	!
	call	idle_other_cpus
	nop

	call	cmd
	nop				! tbr delay

	!
	! Returning from kadb, so let the other CPUs go.
	!
	call	resume_other_cpus
	nop

	mov	%psr, %g1		! disable traps, goto window 0
	bclr	PSR_CWP, %g1
	mov	%g1, %psr
	nop; nop; nop;			! psr delay
	wr	%g1, PSR_ET, %psr
	nop; nop; nop;			! psr delay
	mov	%g0, %wim		! zero wim so that we can move around
	!
	! Restore fpu
	!
	! If there is not an fpu, we are emulating and the
	! registers are already in the right place, the u area.
	! If we have not modified the u area there is no problem.
	! If we have it is not the debuggers problem.
	!
	sethi	%hi(_fpu_exists), %g1
	ld	[%g1 + %lo(_fpu_exists)], %g1
	tst	%g1			! don't bother if no fpu
	bz	2f			! its already in the u area
	nop
	set	regsave, %l3		! was it on?
	ld	[%l3 + R_PSR], %l0
	set	PSR_EF, %l5		! FPU enable bit
	btst	%l5, %l0		! was the FPU enabled?
	bz	2f
	nop
	set	fpuregs, %g7
	ldd	[%g7+(0*4)], %f0	! restore registers
	ldd	[%g7+(2*4)], %f2
	ldd	[%g7+(4*4)], %f4
	ldd	[%g7+(6*4)], %f6
	ldd	[%g7+(8*4)], %f8
	ldd	[%g7+(10*4)], %f10
	ldd	[%g7+(12*4)], %f12
	ldd	[%g7+(14*4)], %f14
	ldd	[%g7+(16*4)], %f16
	ldd	[%g7+(18*4)], %f18
	ldd	[%g7+(20*4)], %f20
	ldd	[%g7+(22*4)], %f22
	ldd	[%g7+(24*4)], %f24
	ldd	[%g7+(26*4)], %f26
	ldd	[%g7+(28*4)], %f28
	ldd	[%g7+(30*4)], %f30
	ld	[%g7+(32*4)], %fsr	! restore fsr
2:
	set	regsave, %g7
	add	%g7, R_WINDOW, %g7
	sethi	%hi(nwindows), %g6
	ld	[%g6 + %lo(nwindows)], %g6
1:
	ld	[%g7 + 0*4], %l0	! restore locals
	ld	[%g7 + 1*4], %l1
	ld	[%g7 + 2*4], %l2
	ld	[%g7 + 3*4], %l3
	ld	[%g7 + 4*4], %l4
	ld	[%g7 + 5*4], %l5
	ld	[%g7 + 6*4], %l6
	ld	[%g7 + 7*4], %l7
	ld	[%g7 + 8*4], %i0	! restore ins
	ld	[%g7 + 9*4], %i1
	ld	[%g7 + 10*4], %i2
	ld	[%g7 + 11*4], %i3
	ld	[%g7 + 12*4], %i4
	ld	[%g7 + 13*4], %i5
	ld	[%g7 + 14*4], %i6
	ld	[%g7 + 15*4], %i7
	add	%g7, WINDOWSIZE, %g7
	subcc	%g6, 1, %g6		! all windows done?
	bnz	1b
	restore				! delay slot, increment CWP
	!
	! Should be back in window 0.
	!
	set	regsave, %g7
	ld	[%g7 + R_WIM], %g1
	mov	%g1, %wim
	ld	[%g7 + R_TBR], %g1
	srl	%g1, 4, %g1		! realign ...
	and	%g1, 0xff, %g1		!... and mask to get trap number
	set	(ST_KADB_BREAKPOINT | T_SOFTWARE_TRAP), %g2
	cmp	%g1, %g2		! compare to see this is a breakpoint
	bne	debugtrap		! a debugger trap return PC+1
	nop
	!
	! return from breakpoint trap
	!
	ld	[%g7 + R_PSR], %g1	! restore psr
	mov	%g1, %psr
	nop; nop; nop;			! psr delay
	ld	[%g7 + R_TBR], %g1
	mov	%g1, %tbr
	ld	[%g7 + R_Y], %g1
	mov	%g1, %y
	ld	[%g7 + R_PC], %l1	! restore pc
	ld	[%g7 + R_NPC], %l2	! restore npc
	mov	%g7, %l3		! put state ptr in local
	ld	[%l3 + R_G1], %g1	! restore globals
	ld	[%l3 + R_G2], %g2
	ld	[%l3 + R_G3], %g3
	ld	[%l3 + R_G4], %g4
	ld	[%l3 + R_G5], %g5
	ld	[%l3 + R_G6], %g6
	ld	[%l3 + R_G7], %g7

	sethi	%hi(kadblock), %l3
	clrb	[%l3 + %lo(kadblock)]	! clear lock

	jmp	%l1			! return from trap
	rett	%l2
	!
	! return from debugger trap
	!
debugtrap:
	ld	[%g7 + R_PSR], %g1	! restore psr
	mov	%g1, %psr
	nop; nop; nop;			! psr delay
	ld	[%g7 + R_TBR], %g1
	mov	%g1, %tbr
	ld	[%g7 + R_Y], %g1
	mov	%g1, %y
	ld	[%g7 + R_PC], %l1	! restore pc
	ld	[%g7 + R_NPC], %l2	! restore npc
	mov	%g7, %l3		! put state ptr in local
	ld	[%l3 + R_G1], %g1	! restore globals
	ld	[%l3 + R_G2], %g2
	ld	[%l3 + R_G3], %g3
	ld	[%l3 + R_G4], %g4
	ld	[%l3 + R_G5], %g5
	ld	[%l3 + R_G6], %g6
	ld	[%l3 + R_G7], %g7

	sethi	%hi(kadblock), %l3
	clrb	[%l3 + %lo(kadblock)]	! clear lock

	jmp	%l2			! return from trap
	rett	%l2 + 4

/*
 * tcode handler for scbsync()
 */
	.align  4
	.global tcode
tcode:
	sethi	%hi(trap), %l3
	or	%l3, %lo(trap), %l3
	jmp	%l3
	mov	%psr, %l0

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
 * Misc subroutines.
 */

/*
 * Get trap base register.
 *
 * char *
 * gettbr()
 */
	ENTRY(gettbr)
	mov	%tbr, %o0
	srl	%o0, 12, %o0
	retl
	sll	%o0, 12, %o0

/*
 * Set trap base register.
 *
 * void
 * settbr(t)
 *	struct scb *t;
 */
	ENTRY(settbr)
	srl	%o0, 12, %o0
	sll	%o0, 12, %o0
	retl
	mov	%o0, %tbr

/*
 * Return current sp value to caller
 *
 * char *
 * getsp()
 */
	ENTRY(getsp)
	retl
	mov	%sp, %o0

/*
 * Set priority level hi.
 *
 * splhi()
 */
	ENTRY(splhi)
	mov	%psr, %o0
	or	%o0, PSR_PIL, %g1
	mov	%g1, %psr
	nop				! psr delay
	retl
	nop

/*
 * Set priority level 13.
 *
 * spl13()
 */
	ENTRY(spl13)
	mov     %psr, %o0
	bclr    PSR_PIL, %o0            ! PIL = 13
	mov     %g0, %g1
	bset    (13 << 8), %g1
	or      %g1, %o0, %g1
	mov     %g1, %psr
	nop                             ! psr delay
	retl
	nop 

	.seg	".data"
	.align	4
	.global	_fpu_exists
_fpu_exists:
	.word	1			! assume FPU exists

	.seg	".text"
	.align	4
/*
 * FPU probe - try a floating point instruction to see if there
 * really is an FPU in the current configuration.
 */
	ENTRY(fpu_probe)
	mov	%psr, %g1		! read psr
	set	PSR_EF, %g2		! enable floating-point
	bset	%g2, %g1
	mov	%g1, %psr		! write psr
	nop				! psr delay, three cycles
	sethi	%hi(zero), %g2		! wait till the fpu bit is fixed
	or	%g2, %lo(zero), %g2
	ld	[%g2], %fsr		! This causes less trouble with SAS.
	retl				! if no FPU, we get fp_disabled trap
	nop				! which will clear the fpu_exists flag

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


	ENTRY(flush_windows)
	sethi	%hi(nwindows), %g7
	ld	[%g7 + %lo(nwindows)], %g7
	sub	%g7, 2, %g6
1:
	deccc	%g6			! all windows done?
	bnz	1b
	save	%sp, -WINDOWSIZE, %sp
	sub	%g7, 2, %g6
2:
	deccc	%g6			! all windows done?
	bnz	2b
	restore				! delay slot, increment CWP
	retl
	nop

	/*
	 * asm_trap(x)
	 * int x;
	 *
	 * Do a "t x" instruction
	 */
	ENTRY(asm_trap)
	retl
	t	%o0

#endif	/* !defined(lint) */

/*
 * kernel agent version that doesn't depend on thread structure existing
 * ka_splx - set PIL back to that indicated by the old %PSR passed as an
 * argument, or to the CPU's base priority, whichever is higher.
 * sys_rtt (in locore.s) relies on this not to use %g1 or %g2.
 */
 
#if defined(lint)
 
/* ARGSUSED */
int
ka_splx(int level)
{ return (0); }
 
#else   /* lint */
 
        ENTRY(ka_splx)
        rd      %psr, %o4               ! get old PSR
        andn    %o4, PSR_PIL, %o3       ! clear PIL from old PSR
        and     %o0, PSR_PIL, %o2       ! mask off argument
        wr      %o3, %o2, %psr          ! write the new psr
        nop                             ! psr delay
        retl                            ! psr delay
	and     %o4, PSR_PIL, %o0	! psr delay - return old PSR
        SET_SIZE(ka_splx)
 
#endif  /* lint */

#if defined(lint)

/* ARGSUSED */
int
ka_splr(int level)
{ return (0); }

#else   /* lint */
 
/*
 * ka_splr(psr_pri_field)
 * kernel agent version of splr.  We needed it because the kernel version uses
 * the thread pointer in %g7 which is not appropriate for kernel agent.
 * splr is like splx but will only raise the priority and never drop it
 */
        ENTRY(ka_splr)
        and     %o0, PSR_PIL, %o3       ! mask proposed new value
        rd      %psr, %o0               ! find current priority
        and     %o0, PSR_PIL, %o2
	andn	%o0, PSR_PIL, %o1
        cmp     %o2, %o3                ! if cur pri < new pri, set new pri
        bg,a    1f                      ! psr was higher than new value
        mov     %o2, %o3                ! delay - move chosen value to %o3
1:
	wr      %o1, %o3, %psr          ! use chosen value
        nop                             ! psr delay
        retl                            ! psr delay return old PSR
        nop                             ! psr delay
	SET_SIZE(ka_splr)
 
#endif  /* lint */
