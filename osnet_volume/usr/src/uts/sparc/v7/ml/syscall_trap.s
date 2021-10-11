/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)syscall_trap.s	1.15	99/04/13 SMI"

/*
 * System call trap handler.
 */
#if defined(lint)
#include <sys/types.h>
#include <sys/thread.h>
#endif

#include <sys/asm_linkage.h>
#include <sys/machpcb.h>
#include <sys/machthread.h>
#include <sys/syscall.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/pcb.h>
#include <sys/archsystm.h>

#if !defined(lint) && !defined(__lint)
#include "assym.h"
#endif

#ifdef TRAPTRACE
#include <sys/traptrace.h>
#endif /* TRAPTRACE */

#if defined(lint) || defined(__lint)

void
syscall_trap(void)	/* for tags only - trap handler - not called from C */
{}

#else /* lint */

/*
 * System call trap handler.
 *
 * This code is entered from the trap vector for system calls.
 *
 *	Entry:
 *	%l0 = %psr with PS == 0 (from user), ET == 0 (traps off).
 *	%l1 = PC of trap instruction.
 *	%l2 = nPC of trap.
 *	%g1 = system call number
 *	%i0-%i5 = system call args
 *
 *	Register usage in trap window:
 *	%l6 = CPU pointer
 *	%l5 = thread pointer
 *	%l7 = kernel stack pointer
 *
 *	Register usage in shared user/kernel window:
 *	%sp = kernel stack pointer
 *	%o0-%o6 = system call args / %o0-%o1 = return values.
 */
	ENTRY_NP(syscall_trap)
	!
	! Find the kernel stack for this thread.
	! Do statistics while the CPU pointer is handy.
	!
	CPU_ADDR(%l6, %l7)			! load CPU ptr to %l6 using %l7
	ld	[%l6 + CPU_THREAD], %l5		! load thread pointer

#ifdef TRAPTRACE
	!
	! make trap trace entry - helps in debugging watchdogs
	!
	TRACE_PTR(%l4, %l3)			! get trace pointer
	mov	%tbr, %l3
	st	%l3, [%l4 + TRAP_ENT_TBR]
	st	%l0, [%l4 + TRAP_ENT_PSR]
	st	%l1, [%l4 + TRAP_ENT_PC]
	st	%fp, [%l4 + TRAP_ENT_SP]
	st	%l5, [%l4 + TRAP_ENT_G7]
	mov	%wim, %l3
	st	%l3, [%l4 + 0x14]		! save wim
	st	%g1, [%l4 + 0x18]		! save syscall number
	st	%i0, [%l4 + 0x1c]		! save first arg (indir code)
	TRACE_NEXT(%l4, %l3, %l7)		! set new trace pointer
#endif /* TRAPTRACE */

	ld	[%l6 + CPU_SYSINFO_SYSCALL], %l7 ! pesky stats 
	inc	%l7				! stats
	st	%l7, [%l6 + CPU_SYSINFO_SYSCALL] ! pesky stats
	ld	[%l5 + T_STACK], %l7		! %l7 is LWP's kernel stack
	!
	! Save PC, nPC, PSR, and global registers
	!
	st	%l0, [%l7 + MINFRAME + PSR*4]	! save caller's %psr
	st	%l1, [%l7 + MINFRAME + PC*4]	! save caller's %pc
	st	%l2, [%l7 + MINFRAME + nPC*4]	! save caller's %npc
	st	%g1, [%l7 + MINFRAME + G1*4]	! save globals - preserve %g1
	sth	%g1, [%l5 + T_SYSNUM]		! save syscall code
	std	%g2, [%l7 + MINFRAME + G2*4]	! save %g2-%g3
	std	%g4, [%l7 + MINFRAME + G4*4]	! save %g4-%g5
	mov	%y, %g4
	std	%g6, [%l7 + MINFRAME + G6*4]	! save %g6-%g7
	st	%g4, [%l7 + MINFRAME + Y*4]	! save %y register
	std	%fp, [%l7 + MINFRAME + O6*4]	! save callers %o6-%o7

#ifdef TRACE
	!
	! Make vtrace entry.  Note that this macro uses label 5: internally.
	! %l6 has the CPU pointer
	!
	mov	T_SYSCALL, %g7
	TRACE_SYS_TRAP_START(%g7, %g2, %g3, %g4, %g5, %g6);
	VT_ASM_TEST_FT(TR_FAC_SYSCALL, TR_SYSCALL_START, %l6, %g2, %g3);
	bz	1f
	sll	%g2, 16, %g2			! delay - shift event number
	or	%g2, %g3, %g2			! form event + info
	orcc	%g1, %g0, %g4			! test for indirect syscall
	bz,a	0f
	mov	%i0, %g4			! delay - put code in trace
0:
	TRACE_DUMP_1(%g2, %g4, %l6, %g5, %g6, %g7);
1:
	! %l6 still has CPU pointer
	! trace hasn't disturbed code in %g1
#endif	/* TRACE */
	!
	! Use restore to move to the register window before the trap.
	! This is a known-valid window, and the trap window may be invalid.
	! This avoids checking and potentially expensive overflow checks.
	! It also has the advantage of providing all args in registers for
	! the system call.
	!
	mov	%l5, THREAD_REG			! setup kernel's %g7
	ld	[%l5 + T_LWP], %g5		! load LWP pointer
	restore %l7, 0, %sp			! move to known valid window

	!
	! Compute the user window mask (mpcb->mpcb_uwm), which is a mask of
	! windows which contain user data.  It is all the windows "between"
	! CWM and WIM, plus the current window, which is going to be borrowed
	! by the kernel.
	!
	mov	%psr, %g4			! for CWP, and enabling
	mov	1, %g3
	sll	%g3, %g4, %g3			! CWM = 1 << CWP
	st	%g3, [%sp + MPCB_SWM]		! set trap shared window mask
	mov	%wim, %g2			! get window invalid mask
	subcc	%g2, %g3, %g2			! if (WIM > CWM)
	bge	1f				!     mpcb_uwm = (WIM-CWM)
	sethi	%hi(winmask), %g6		! delay - load the window mask
	sub	%g2, 1, %g2			! else
						!     mpcb_uwm = (WIM-CWM-1)
	ld	[%g6 + %lo(winmask)], %g6
	andn	%g2, %g6, %g2
1:
	st	%g2, [%sp + MPCB_UWM]		! set mpcb->mpcb_uwm
	!
	! Set new state for LWP, then enable traps.
	!
	mov	LWP_SYS, %g2
	stb	%g2, [%g5 + LWP_STATE]		! set LWP state before ET
	ldd	[%g5 + LWP_RU_SYSC], %g2	! pesky statistics
	addcc	%g3, 1, %g3
	addx	%g2, 0, %g2

	wr	%g4, PSR_ET, %psr		! enable traps
	std	%g2, [%g5 + LWP_RU_SYSC] 	! psr delay 

	std	%o0, [%sp + MINFRAME + O0*4]	! save o0-o1 for /proc, restart
	!
	! If floating-point is enabled, handle possible exceptions here.
	!
	set	PSR_EF, %g2			! psr delay temp FP check
	btst	%g4, %g2			! check PSR_EF
	bz	2f				! floating point off
	std	%o2, [%sp + MINFRAME + O2*4]	! delay - save %o2-%o3
	ld	[%sp + MPCB_FLAGS], %g3		! clear FP_TRAPPED
	andn	%g3, FP_TRAPPED, %g3
	st	%g3, [%sp + MPCB_FLAGS]
	st	%fsr, [%sp + 64]		! take exception, if one pending
	ld	[%sp + MPCB_FLAGS], %g3		! test FP_TRAPPED
	btst	FP_TRAPPED, %g3
	bnz	_syscall_fp			! trap occurred
	.empty					! label OK in delay slot
2:
	std	%o4, [%sp + MINFRAME + O4*4]	! delay - save %o4-%o5

	!
	! Test for pre-system-call handling
	!
	ldub	[THREAD_REG + T_PRE_SYS], %g2	! pre-syscall proc?
#ifdef SYSCALLTRACE
	sethi	%hi(syscalltrace), %g4
	ld	[%g4 + %lo(syscalltrace)], %g4
	orcc	%g2, %g4, %g0			! pre_syscall OR syscalltrace?
#else
	tst	%g2				! is pre_syscall flag set?
#endif /* SYSCALLTRACE */

	bnz	_syscall_pre			! yes - pre_syscall needed
	cmp	%g1, NSYSCALL			! delay - range check sysnum

	!
	! Call the handler.  The args are still in %o0-%o5 from the trap.
	!
3:
	set	(sysent + SY_CALLC), %g3	! load address of vector table
	bgeu	_syscall_ill			! syscall out of range
#if SYSENT_SIZE == 16
	sll	%g1, 4, %g4			! delay - get index 
#else
	.error	"sysent size change requires change in shift amount"
#endif
	ld	[%g3 + %g4], %g3		! load system call handler
	call	%g3				! call system call handler
	nop					! delay

	!
	! Return from system call.
	! Load users's PSR, but with current window and with carry cleared.
	!
_syscall_rtt:
	mov	%psr, %g3			! read current %psr

	!
	! Check for post-syscall processing.
	! This tests all members of the union containing t_astflag, t_post_sys,
	! and t_sig_check with one test.
	!
	wr	%g3, PSR_ET, %psr		! disable traps
	nop; nop				! psr delay
	ld	[THREAD_REG + T_POST_SYS_AST], %g1	! includes T_ASTFLAG
#ifdef SYSCALLTRACE
	sethi	%hi(syscalltrace), %g4
	ld	[%g4 + %lo(syscalltrace)], %g4
	orcc	%g4, %g1, %g0			! OR in syscalltrace
#else
	tst	%g1				! need post-processing?
#endif /* SYSCALLTRACE */
	bnz	_syscall_post			! yes - post_syscall or AST set
	mov	%wim, %g2			! delay - read %wim

	!
	! Restore necessary registers.
	! Since this is a normal return, it should not be necessary to restore
	! registers %g1-%g3, %y, and %o2-%o5.  However, libc has been
	! relying on some of these not changing so for now, restore all.
	!
	! Do a save to get into the window above this to allow the rett to
	! get back to this window.  First clear the WIM to make it safe
	! to do a save with traps off, since we may be moving into the
	! invalid window temporarily.
	!
	ld	[THREAD_REG + T_LWP], %g5
	mov	%g0, %wim			! clear %wim
	set	LWP_USER, %g1			! wim delay 
	stb	%g1, [%g5 + LWP_STATE]		! wim delay - set lwp_state
	clr	[%sp + MPCB_SWM]		! wim delay - clear shared mask
	clr	[%sp + MPCB_RSP]		! clear user window return sp
	clr	[%sp + MPCB_RSP + 4]		! clear user window return sp
	save	%sp, 0, %l7			! move up a window
	!
	! The current window may be invalid window now, since it's the one we
	! trapped into and the routine we called may not have done a save,
	! but we now know the window we were in is valid, so no underflow
	! processing is needed for the rett.
	!
	mov	%g2, %wim			! restore %wim
	ldd	[%l7 + MINFRAME + O6*4], %i6	! wim delay - callers %sp, %o7
	ld	[%l7 + MINFRAME + nPC*4], %l2	! wim delay - callers %npc
	clrh	[THREAD_REG + T_SYSNUM]		! clear syscall code
	!
	! Form user's PSR, especially for the condition codes.  Keep CWP.
	! Traps are off, so using %g6 and %g7 won't bother anyone.
	! This also keeps the user from seeing these results, since %g6-g7 are
	! restored after this.
	!
	ld	[THREAD_REG + T_CPU], %l1	! get CPU pointer
	mov	%psr, %l0			! current %psr for CWP
	ld	[%l7 + MINFRAME + PSR*4], %g6	! load user's saved %psr
	ld	[%l1 + CPU_BASE_SPL], %l1	! get base PIL
	and	%l0, PSR_CWP, %l0		! keep current window
	andn	%g6, PSR_CWP | PSR_PIL, %g6	! clear CWP and PIL in saved PSR
	or	%g6, %l0, %g6			! OR in current window pointer
	or	%g6, %l1, %g6			! OR in base PIL
	set	PSR_C, %l1
	andn	%g6, %l1, %g6			! clear carry bit for no error

#ifdef TRAPTRACE
	!
	! make trap trace entry for return - helps in debugging watchdogs
	!
	TRACE_PTR(%l4, %l3)			! get trace pointer
	mov	TT_SC_RET, %l3			! system call return code
	st	%l3, [%l4 + TRAP_ENT_TBR]
	st	%g6, [%l4 + TRAP_ENT_PSR]
	st	%l2, [%l4 + TRAP_ENT_PC]
	st	%fp, [%l4 + TRAP_ENT_SP]
	st	THREAD_REG, [%l4 + TRAP_ENT_G7]
	st	%g2, [%l4 + 0x14]		! put %wim in trace
	std	%i0, [%l4 + 0x18]		! put return values in trace
	TRACE_NEXT(%l4, %l3, %l5)		! set new trace pointer
#endif /* TRAPTRACE */

#ifdef TRACE
	!
	! Make vtrace entry.  Note that this macro uses label 5: internally.
	!
	ld	[THREAD_REG + T_CPU], %g5	! get CPU pointer
	VT_ASM_TEST_FT(TR_FAC_SYSCALL, TR_SYSCALL_END, %g5, %g1, %g2);
	bz	6f
	sll	%g1, 16, %g1			! delay - shift event number
	or	%g1, %g2, %g1			! form event + info
	TRACE_DUMP_0(%g1, %g5, %g2, %g3, %g4);
6:
	TRACE_SYS_TRAP_END(%g1, %g2, %g3, %g4, %g5);
#endif	/* TRACE */

	mov	%g6, %psr			! user's PSR - traps off
	ldd	[%l7 + MINFRAME + O2*4], %i2	! psr delay - restore %o2-%o3
	ldd	[%l7 + MINFRAME + O4*4], %i4	! psr delay - restore %o4-%o5
	RESTORE_GLOBALS(%l7 + MINFRAME)		! psr delay - restore globals
	!
	! PC, nPC alignment and clean windows functions must be tested
	! in post_syscall if necessary.
	!
#ifdef VIKING_BUG_16
	stbar
#endif /* VIKING_BUG_16 */
	jmp	%l2				! wim delay - return after trap
	rett	%l2 + 4				! return from trap

_syscall_pre:
	call	pre_syscall			! abort_flg = pre_syscall(args);
	nop					! delay

	tst	%o0				! did it abort?
	bnz	1f				! yes, syscall is to be aborted
	ldd	[%sp + MINFRAME + O0*4], %o0	! delay - reload args/rvals

	ldd	[%sp + MINFRAME + O2*4], %o2
	ldd	[%sp + MINFRAME + O4*4], %o4
	ld	[%sp + MINFRAME + G1*4], %g1	! reload syscall code
	b	3b
	cmp	%g1, NSYSCALL

	!
	! Floating-point trap was pending at start of system call.
	! Here with:
	!	%g5 = LWP pointer
	!	%g3 = mpcb_flags
	!
_syscall_fp:
	andn	%g3, FP_TRAPPED, %g3		! turn flag off again
	b	_syscall_post_rtt		! return from syscall
	st	%g3, [%sp + MPCB_FLAGS]		! delay - clear FP_TRAPPED
	!
	! Post-syscall special processing needed.
	! Here with traps disabled, and enabling %psr in %g3
	!
_syscall_post:
	ld	[THREAD_REG + T_CPU], %g2	! get CPU pointer
	andn	%g3, PSR_PIL, %g3		! clear PIL from %psr
	ld	[%g2 + CPU_BASE_SPL], %g2	! load base PIL
	wr	%g3, %g2, %psr			! enable traps
	nop					! psr delay
1:
	call	post_syscall			! psr delay post_syscall(rvals);
	nop					! psr delay

	!
	! Disable traps and check for deferred window overflow.
	!
_syscall_post_rtt:
	mov	%psr, %g1
	wr	%g1, PSR_ET, %psr		! disable traps
	ld	[THREAD_REG + T_LWP], %g5	! psr delay 
	clrh	[THREAD_REG + T_SYSNUM]		! psr delay - clear syscall code
	ldub	[THREAD_REG + T_ASTFLAG], %g2	! psr delay - test t_astflag
	tst	%g2
	bnz	_syscall_ast			! handle AST
	ld	[%sp + MPCB_WBCNT], %g2		! delay - check overflow
	tst	%g2				! check for window overflow trap
	bnz	_syscall_overflow		! handle overflow trap
	ld	[%sp + MPCB_FLAGS], %g2		! delay - load mpcb_flags
	btst	GOTO_SYS_RTT, %g2		! test for return to sys_rtt
	bnz	_goto_sys_rtt
	mov	%wim, %g2			! delay - save wim
	!
	! Clear the WIM to make it safe to do a save with traps disabled,
	! then do a save to get to the window above this for the rett.
	!
	mov	%g0, %wim			! clear %wim 
	ld	[THREAD_REG + T_CPU], %g4	! wim delay - load CPU pointer
	clr	[%sp + MPCB_SWM]		! wim delay - clear shared mask
	clr	[%sp + MPCB_RSP]		! wim delay - clear return sp
	clr	[%sp + MPCB_RSP + 4]		! clear return sp
	save	%sp, 0, %l7			! move up a window
#ifdef TRACE
	!	
	! Make end-of-syscall trace entry. 
	! Watch this: it modifies the %psr CC and has a label 5: inside it.
	! %g4 has the CPU pointer which is preserved through the trace macros
	! %g2 has the old wim which must be preserved
	! %g5 has the LWP pointer which must be preserved
	! %l1-%l4 are scratch
	!
	VT_ASM_TEST_FT(TR_FAC_SYSCALL, TR_SYSCALL_END, %g4, %l3, %l4);
	bz	6f
	sll	%l3, 16, %l3			! delay - shift event number
	or	%l3, %l4, %l3			! form event + info
	TRACE_DUMP_0(%l3, %g4, %l1, %l2, %l4);
6:
	TRACE_SYS_TRAP_END(%g4, %l1, %l2, %l3, %l4);
#endif	/* TRACE */
	ld	[%l7 + MINFRAME + PC*4], %l1	! load saved %pc
	ld	[%l7 + MINFRAME + nPC*4], %l2	! load saved %npc
	ld	[%l7 + MINFRAME + PSR*4], %g3	! load user's saved %psr

	!
	! Merge users's PSR, with current window.
	! Condition code must be set by post_syscall to indicate error.
	! Don't modify the condition codes after this.
	! Traps are already disabled.
	! 
	mov	%psr, %g1
	and	%g1, PSR_CWP, %g1		! keep current window
	andn	%g3, PSR_CWP | PSR_PIL, %g3	! clear CWP and PIL in saved PSR
	or	%g1, %g3, %g1			! or in CWP
	ld	[%g4 + CPU_BASE_SPL], %g3
	ld	[%l7 + MPCB_FLAGS], %l6		! load mpcb_flags
#ifdef TRAPTRACE
	/*
	 * make trap trace entry for return - helps in debugging watchdogs
	 * TRACE_NEXT modifies CC, so do this before loading user's PSR
	 */
	TRACE_PTR(%l4, %l3)			! get trace pointer
	mov	TT_SC_POST, %l3			! post system call return code
	st	%l3, [%l4 + TRAP_ENT_TBR]
	or	%g1, %g3, %l3			! OR in base PIL
	st	%l3, [%l4 + TRAP_ENT_PSR]
	st	%l1, [%l4 + TRAP_ENT_PC]
	st	%fp, [%l4 + TRAP_ENT_SP]
	st	%g2, [%l4 + 0x14]		! put WIM in trace
	ldd	[%l7 + MINFRAME + O0*4], %g4	! get RVALs for trace (uses %g5)
	std	%g4, [%l4 + 0x18]		! put return values in trace
	st	THREAD_REG, [%l4 + TRAP_ENT_G7]	! put THREAD_REG in trace
	TRACE_NEXT(%l4, %l3, %l5)		! set new trace pointer
#endif	/* TRAPTRACE */

	btst	CLEAN_WINDOWS, %l6		! test for clean windows
	bnz	_syscall_clean			! %g2 still has saved %wim
	or	%g1, %g3, %g1			! delay - form user's PSR
	mov	%g1, %psr			! load user's PSR with base PIL
	!
	! The current window may be invalid window now, since it could be
	! the one we trapped into and the routine we called may not have
	! done a save, but we now know the window we were just in is valid,
	! so no underflow processing is needed for the rett.
	!
	mov	%g2, %wim			! psr delay - restore %wim
	RESTORE_GLOBALS(%l7 + MINFRAME)		! psr, wim delay/restore globals
	RESTORE_OUTS(%l7 + MINFRAME)		! restore outs (into ins)
#ifdef VIKING_BUG_16
	stbar
#endif /* VIKING_BUG_16 */
	jmp	%l1				! return to user
	rett	%l2

	!
	! Clean-windows return from syscall (entered at _syscall_clean)
	! In the window above the trap window, which could be invalid.
	!	%wim = 0
	!	%g1 = user PSR with CWP == current window
	!	%g2 = new %wim
	!	%l7 = stack pointer
	!	%g7 = current thread
1:
	clr	%l1
	clr	%l2
	clr	%l3
	clr	%l4
	clr	%l5
	clr	%l6
	clr	%l7
	clr	%i0
	clr	%i1
	clr	%i2
	clr	%i3
	clr	%i4
	clr	%i5
	clr	%i6
	clr	%i7
_syscall_clean:
	mov	%psr, %g3		! get current window pointer (CWP)
	srl	%g2, %g3, %g4		! test WIM bit for new window
	btst	1, %g4
	bz,a	1b			! not invalid window
	save	%g0, 0, %l0		! delay - save and clear %l0

	mov	%g1, %psr		! back to trap window, user CC
	mov	%g2, %wim		! psr delay - restore %wim
	!
	! Set t->t_post_sys non-zero so that next syscall will 
	! also maintain clean windows.
	!
	mov	1, %g2
	stb	%g2, [THREAD_REG + T_POST_SYS]
	RESTORE_GLOBALS(%l7 + MINFRAME)	! psr, wim delay/restore globals
	RESTORE_OUTS(%l7 + MINFRAME)	! restore outs (into ins)
	!
	! Clean up the trap window.  Note that the outs belong to the next
	! window on the stack, which has either been cleaned above, or this
	! is the invalid window.  Don't clean the outs.
	! This leaves %l1 and %l2 dirty, but it can't be helped, and this
	! is allowed by the ABI, since these are valid user PC values.
	!
	clr	%l3
	clr	%l4
	clr	%l5
	clr	%l6
	clr	%l7
#ifdef VIKING_BUG_16
	stbar
#endif /* VIKING_BUG_16 */
	jmp	%l1				! return to user
	rett	%l2

	!
	! Deferred window-overflow trap.
	! Here in shared window, with traps disabled, and enabling %psr in %g1.
	! Post-syscall handling has already been complete.
	!
_syscall_overflow:
	b	_syscall_exit_trap		! call trap with T_FLUSH_PCB
	mov	T_FLUSH_PCB, %o0		! psr delay

	!
	! AST flag on after post-syscall. 
	!
_syscall_ast:
	mov	T_AST, %o0
	!
	! Enable, then call trap with type in %o0, then resume syscall_post_rtt.
	!
_syscall_exit_trap:
#ifdef TRAPTRACE
	/*
	 * make trap trace entry for trap rtn - helps in debugging watchdogs
	 * TRACE_NEXT modifies CC, so do this before loading user's PSR
	 */
	TRACE_PTR(%g4, %g3)			! get trace pointer
	mov	TT_SC_TRAP, %g2			! post system call return code
	st	%g2, [%g4 + TRAP_ENT_TBR]
	st	%g1, [%g4 + TRAP_ENT_PSR]
	ld	[%sp + MINFRAME + PC*4], %g2	! load saved %pc
	st	%g2, [%g4 + TRAP_ENT_PC]
	st	%sp, [%g4 + TRAP_ENT_SP]
	st	%g7, [%g4 + TRAP_ENT_G7]	! put THREAD_REG in trace
	mov	%wim, %g2
	st	%g2, [%g4 + 0x14]		! put WIM in trace
	ld	[%sp + MPCB_WBCNT], %g2
	st	%g2, [%g4 + 0x18]		! put MPCB_WBCNT into trace
	st	%o0, [%g4 + 0x1c]		! put trap code into trace
	TRACE_NEXT(%g4, %g3, %g2)		! set new trace pointer
#endif	/* TRAPTRACE */
	ld	[THREAD_REG + T_CPU], %g2	! get CPU pointer
	andn	%g1, PSR_PIL, %g1		! clear PIL from %psr
	ld	[%g2 + CPU_BASE_SPL], %g2	! load base PIL
	wr	%g1, %g2, %psr			! enable traps
	nop					! %psr delay
	call	trap				! trap(type, rp);
	add	%sp, MINFRAME, %o1		! delay - register pointer arg 
	b,a	_syscall_post_rtt		! return from trap

	!
	! Return via _sys_rtt (restorecontext() makes us do this)
	!
_goto_sys_rtt:
	ld	[%sp + MPCB_FLAGS], %g2		! clear GOTO_SYS_RTT
	andn	%g2, GOTO_SYS_RTT, %g2
	st	%g2, [%sp + MPCB_FLAGS]
	clr	[%sp + MPCB_SWM]		! clear shared mask
	clr	[%sp + MPCB_RSP]		! clear user window return sp
	clr	[%sp + MPCB_RSP + 4]		! clear user window return sp

	mov	0x02, %wim			! setup wim
	ld	[THREAD_REG + T_CPU], %g2	! get CPU pointer
	andn	%g1, PSR_CWP|PSR_PIL|PSR_ET, %g1 ! CWP=0, PIL=0, traps off
	ld	[%g2 + CPU_BASE_SPL], %g2	! load base PIL
	or	%g1, %g2, %g2			! new %psr, traps disabled
	mov	%g2, %psr
	nop; nop ; nop				! psr delay
	ld	[THREAD_REG + T_STACK], %sp	! load lwp's kernel stack
	wr	%g2, PSR_ET, %psr		! enable traps
	nop; nop				! psr delay
	b,a	_sys_rtt			! psr delay

	!
	! illegal system call - syscall number out of range
	!
_syscall_ill:
	call	nosys
	nop
	b,a	_syscall_rtt

	SET_SIZE(syscall_trap)


/*
 * Entry for old 4.x trap (trap 0).
 */
	ENTRY_NP(syscall_trap_4x)
	CPU_ADDR(%l5, %l7)		! load CPU struct addr to %l5 using %l7
	ld	[%l5 + CPU_THREAD], %l5	! load thread pointer
	ld	[%l5 + T_LWP], %l5	! load klwp pointer
	ld	[%l5 + PCB_TRAP0], %l5	! lwp->lwp_pcb.pcb_trap0addr
	tst	%l5			! has it been set?
	bz	1f			! not set, call syscall_trap
	mov	%l2, %g7		! pass npc to user code in %g7
#ifdef VIKING_BUG_16
	stbar
#endif /* VIKING_BUG_16 */
	jmp	%l5			! return
	rett	%l5 + 4

1:
	!
	! check for old syscall mmap which is the only different one which
	! must be the same.  Others are handled in the compatibility library.
	!
	cmp	%g1, OSYS_mmap		! compare to old 4.x mmap
	bne	syscall_trap
	nop
	b	syscall_trap
	mov	SYS_mmap, %g1		! delay - set new mmap
	SET_SIZE(syscall_trap_4x)


/*
 * Handler for software trap 9.
 * Set trap0 emulation address for old 4.x system call trap.
 * XXX - this should be a system call.
 */
	ENTRY_NP(set_trap0_addr)
	CPU_ADDR(%l5, %l7)		! load CPU struct addr to %l5 using %l7
	ld	[%l5 + CPU_THREAD], %l5	! load thread pointer
	ld	[%l5 + T_LWP], %l5	! load klwp pointer
	andn	%g1, 3, %g1		! force alignment
	st	%g1, [%l5 + PCB_TRAP0]	! lwp->lwp_pcb.pcb_trap0addr
#ifdef VIKING_BUG_16
	stbar
#endif /* VIKING_BUG_16 */
	jmp	%l2			! return
	rett	%l2 + 4
	SET_SIZE(set_trap0_addr)

#endif /* lint */


/*
 * lwp_rtt - start execution in newly created LWP.
 *	Here with t_post_sys set by lwp_create, and lwp_eosys == JUSTRETURN,
 *	so that post_syscall() will run and the registers will
 *	simply be restored.
 *	This must go out through sys_rtt instead of syscall_rtt.
 */
#if defined(lint) || defined(__lint)

void
lwp_rtt()
{}

#else	/* lint */

	ENTRY_NP(lwp_rtt)
	ld      [THREAD_REG + T_STACK], %sp	! set stack at base
	call	post_syscall
	ldd	[%sp + MINFRAME + O0*4], %o0	! delay - return values in outs
	ld	[%sp + MPCB_FLAGS], %g2		! clear GOTO_SYS_RTT
	andn	%g2, GOTO_SYS_RTT, %g2
	st	%g2, [%sp + MPCB_FLAGS]
	b,a	_sys_rtt
	SET_SIZE(lwp_rtt)

#endif	/* lint */
