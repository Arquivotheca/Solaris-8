/*
 * Copyright (c) 1994-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)syscall_trap.s	1.17	99/04/12 SMI"

/*
 * System call trap handler.
 */
#include <sys/asm_linkage.h>
#include <sys/machpcb.h>
#include <sys/machthread.h>
#include <sys/syscall.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/pcb.h>

#if !defined(lint) && !defined(__lint)
#include "assym.h"
#endif

#ifdef TRAPTRACE
#include <sys/traptrace.h>
#endif /* TRAPTRACE */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
syscall_trap(struct regs *rp)	/* for tags only; not called from C */
{}

#else /* lint */

#if (1 << SYSENT_SHIFT) != SYSENT_SIZE
#error	"SYSENT_SHIFT does not correspond to size of sysent structure"
#endif
	
/*
 * Native System call trap handler.
 *
 * We branch here from sys_trap when a 32-bit system call occurs on
 * a ILP32 kernel or a 64-bit system call occurs on a LP64 kernel.
 *
 * Entry:
 *	%o0 = regs
 *
 * Usage:
 *	%l0 = saved return address
 *	%l1 = saved regs
 *	%l2 = lwp
 */
	ENTRY_NP(syscall_trap)
	ldn	[THREAD_REG + T_CPU], %g1	! get cpu pointer
	mov	%o7, %l0			! save return addr
#ifdef __sparcv9
	!
	! If the trapping thread has the address mask bit set, then it's
	!   a 32-bit process, and has no business calling 64-bit syscalls.
	!
	ldx	[%o0 + TSTATE_OFF], %l1		! saved %tstate.am is that
	andcc	%l1, TSTATE_AM, %l1		!   of the trapping proc
	bne,pn	%xcc, _syscall_ill		!
#endif
	  ld	[%g1 + CPU_SYSINFO_SYSCALL], %g2
	mov	%o0, %l1			! save reg pointer
	inc	%g2				! pesky stats
	mov	%i0, %o0			! copy 1st arg
	st	%g2, [%g1 + CPU_SYSINFO_SYSCALL]
	mov	%i1, %o1			! copy 2nd arg

	!
	! Set new state for LWP
	!
	ldn	[THREAD_REG + T_LWP], %l2
	mov	LWP_SYS, %g3
	mov	%i2, %o2			! copy 3rd arg
	stb	%g3, [%l2 + LWP_STATE]
	mov	%i3, %o3			! copy 4th arg
	ldx	[%l2 + LWP_RU_SYSC], %g2	! pesky statistics
	mov	%i4, %o4			! copy 5th arg
	addx	%g2, 1, %g2
	stx	%g2, [%l2 + LWP_RU_SYSC]
	mov	%i5, %o5			! copy 6th arg
	! args for direct syscalls now set up

#ifdef TRAPTRACE
	!
	! make trap trace entry - helps in debugging
	!
	rdpr	%pstate, %l3
	andn	%l3, PSTATE_IE | PSTATE_AM, %g3
	wrpr	%g0, %g3, %pstate		! disable interrupt
	TRACE_PTR(%g3, %g2)			! get trace pointer
	GET_TRACE_TICK(%g1)
	stxa	%g1, [%g3 + TRAP_ENT_TICK]%asi
	ldx	[%l1 + G1_OFF], %g1		! get syscall code
	stha	%g1, [%g3 + TRAP_ENT_TL]%asi
	set	TT_SC_ENTR, %g2
	stha	%g2, [%g3 + TRAP_ENT_TT]%asi
	stxa	%g7, [%g3 + TRAP_ENT_TSTATE]%asi ! save thread in tstate space
	stna	%sp, [%g3 + TRAP_ENT_SP]%asi
	stna	%o0, [%g3 + TRAP_ENT_F1]%asi
	stna	%o1, [%g3 + TRAP_ENT_F2]%asi
	stna	%o2, [%g3 + TRAP_ENT_F3]%asi
	stna	%o3, [%g3 + TRAP_ENT_F4]%asi
	stna	%o4, [%g3 + TRAP_ENT_TPC]%asi
	stna	%o5, [%g3 + TRAP_ENT_TR]%asi
	TRACE_NEXT(%g3, %g2, %g1)		! set new trace pointer
	wrpr	%g0, %l3, %pstate		! enable interrupt
#endif /* TRAPTRACE */

#ifndef __sparcv9
	st	%o0, [%l2 + LWP_ARG + (0*LWP_ARG_SZ)]	! save 1st arg
	st	%o1, [%l2 + LWP_ARG + (1*LWP_ARG_SZ)]	! save 2nd arg
	st	%o2, [%l2 + LWP_ARG + (2*LWP_ARG_SZ)]	! save 3rd arg
	st	%o3, [%l2 + LWP_ARG + (3*LWP_ARG_SZ)]	! save 4th arg
	st	%o4, [%l2 + LWP_ARG + (4*LWP_ARG_SZ)]	! save 5th arg
#endif

	!
	! Test for pre-system-call handling
	!
	ldub	[THREAD_REG + T_PRE_SYS], %g3	! pre-syscall proc?
#ifdef SYSCALLTRACE
	sethi	%hi(syscalltrace), %g4
	ld	[%g4 + %lo(syscalltrace)], %g4
	orcc	%g3, %g4, %g0			! pre_syscall OR syscalltrace?
#else
	tst	%g3				! is pre_syscall flag set?
#endif /* SYSCALLTRACE */

#ifndef __sparcv9
	bnz,pn	%icc, _syscall_pre		! yes - pre_syscall needed
	st	%o5, [%l2 + LWP_ARG + (5*LWP_ARG_SZ)]	! save 6th arg
#else
	bnz,pn	%icc, _syscall_pre
	  nop
#endif /* __sparcv9 */

	! lwp_arg now set up
3:
	!
	! Call the handler.  The %o's and lwp_arg have been set up.
	!
	ldx	[%l1 + G1_OFF], %g1		! get code
	set	sysent, %g3			! load address of vector table
	cmp	%g1, NSYSCALL			! check range
	sth	%g1, [THREAD_REG + T_SYSNUM]	! save syscall code
	bgeu,pn	%ncc, _syscall_ill
	  sll	%g1, SYSENT_SHIFT, %g4			! delay - get index 
	add	%g3, %g4, %l4
	ldn	[%l4 + SY_CALLC], %g3		! load system call handler

	call	%g3				! call system call handler
	  nop
#ifdef __sparcv9
	!
	! If handler returns two ints, then we need to split the 64-bit
	! return value in %o0 into %o0 and %o1
	!
	lduh	[%l4 + SY_FLAGS], %l4		! load sy_flags
	andcc	%l4, SE_32RVAL2, %g0		! check for 2 x 32-bit
	bz,pt	%xcc, 5f
	  nop
	srl	%o0, 0, %o1			! lower 32-bits into %o1
	srlx	%o0, 32, %o0			! upper 32-bits into %o0
5:
#endif

#ifdef TRAPTRACE
	!
	! make trap trace entry for return - helps in debugging
	!
	rdpr	%pstate, %g5
	andn	%g5, PSTATE_IE | PSTATE_AM, %g4
	wrpr	%g0, %g4, %pstate		! disable interrupt
	TRACE_PTR(%g4, %g2)			! get trace pointer
	GET_TRACE_TICK(%g2)
	stxa	%g2, [%g4 + TRAP_ENT_TICK]%asi
	lduh	[THREAD_REG + T_SYSNUM], %g2
	stha	%g2, [%g4 + TRAP_ENT_TL]%asi
	mov	TT_SC_RET, %g2			! system call return code
	stha	%g2, [%g4 + TRAP_ENT_TT]%asi
	ldn	[%l1 + nPC_OFF], %g2		! get saved npc (new pc)
	stna	%g2, [%g4 + TRAP_ENT_TPC]%asi
	ldx	[%l1 + TSTATE_OFF], %g2		! get saved tstate
	stxa	%g2, [%g4 + TRAP_ENT_TSTATE]%asi
	stna	%sp, [%g4 + TRAP_ENT_SP]%asi
	stna	THREAD_REG, [%g4 + TRAP_ENT_TR]%asi
	stna	%o0, [%g4 + TRAP_ENT_F1]%asi
	stna	%o1, [%g4 + TRAP_ENT_F2]%asi
	stna	%g0, [%g4 + TRAP_ENT_F3]%asi
	stna	%g0, [%g4 + TRAP_ENT_F4]%asi
	TRACE_NEXT(%g4, %g2, %g3)		! set new trace pointer
	wrpr	%g0, %g5, %pstate		! enable interrupt
#endif /* TRAPTRACE */
	!
	! Check for post-syscall processing.
	! This tests all members of the union containing t_astflag, t_post_sys,
	! and t_sig_check with one test.
	!
	ld	[THREAD_REG + T_POST_SYS_AST], %g1
#ifdef SYSCALLTRACE
	sethi	%hi(syscalltrace), %g4
	ld	[%g4 + %lo(syscalltrace)], %g4
	orcc	%g4, %g1, %g0			! OR in syscalltrace
#else
	tst	%g1				! need post-processing?
#endif /* SYSCALLTRACE */
	bnz,pn	%icc, _syscall_post		! yes - post_syscall or AST set
	mov	LWP_USER, %g1
	stb	%g1, [%l2 + LWP_STATE]		! set lwp_state
	stx	%o0, [%l1 + O0_OFF]		! set rp->r_o0
	stx	%o1, [%l1 + O1_OFF]		! set rp->r_o1
	clrh	[THREAD_REG + T_SYSNUM]		! clear syscall code
	ldx	[%l1 + TSTATE_OFF], %g1		! get saved tstate
	ldn	[%l1 + nPC_OFF], %g2		! get saved npc (new pc)
	mov	CCR_IC, %g3
	sllx	%g3, TSTATE_CCR_SHIFT, %g3
	add	%g2, 4, %g4			! calc new npc
	andn	%g1, %g3, %g1			! clear carry bit for no error
	stn	%g2, [%l1 + PC_OFF]
	stn	%g4, [%l1 + nPC_OFF]
	jmp	%l0 + 8
	stx	%g1, [%l1 + TSTATE_OFF]

_syscall_pre:
	ldx	[%l1 + G1_OFF], %g1
	call	pre_syscall			! abort = pre_syscall(arg0)
	sth	%g1, [THREAD_REG + T_SYSNUM]

	brnz,pn	%o0, _syscall_post		! did it abort?
	nop
	ldx	[%l1 + O0_OFF], %o0		! reload args
	ldx	[%l1 + O1_OFF], %o1
	ldx	[%l1 + O2_OFF], %o2
	ldx	[%l1 + O3_OFF], %o3
	ldx	[%l1 + O4_OFF], %o4
	ba,pt	%xcc, 3b
	ldx	[%l1 + O5_OFF], %o5

	!
	! Floating-point trap was pending at start of system call.
	! Here with:
	!	%l3 = mpcb_flags
	!
_syscall_fp:
	andn	%l3, FP_TRAPPED, %l3
	st	%l3, [%sp + STACK_BIAS + MPCB_FLAGS] 	! clear FP_TRAPPED
	jmp	%l0 + 8				! return to user_rtt
	clrh	[THREAD_REG + T_SYSNUM]		! clear syscall code

	!
	! illegal system call - syscall number out of range
	!
_syscall_ill:
	call	nosys
	nop
	!
	! Post-syscall with special processing needed.
	!
_syscall_post:
	call	post_syscall			! post_syscall(rvals)
	nop
	jmp	%l0 + 8				! return to user_rtt
	nop
	SET_SIZE(syscall_trap)
#endif	/* lint */

#if defined(__sparcv9)

#if defined(lint) || defined(__lint)

void
syscall_trap32(void)	/* for tags only - trap handler - not called from C */
{}

#else /* lint */

/*
 * System call trap handler for ILP32 processes
 * on a LP64 kernel.
 *
 * We branch here from sys_trap when a system call occurs.
 *
 * Entry:
 *	%o0 = regs
 *
 * Usage:
 *	%l0 = saved return address
 *	%l1 = saved regs
 *	%l2 = lwp
 */
	ENTRY_NP(syscall_trap32)
	ldx	[THREAD_REG + T_CPU], %g1	! get cpu pointer
	mov	%o7, %l0			! save return addr

	!
	! If the trapping thread has the address mask bit clear, then it's
	!   a 64-bit process, and has no business calling 32-bit syscalls.
	!
	ldx	[%o0 + TSTATE_OFF], %l1		! saved %tstate.am is that
	andcc	%l1, TSTATE_AM, %l1		!   of the trapping proc
	be,pn	%xcc, _syscall_ill32		!

	  ld	[%g1 + CPU_SYSINFO_SYSCALL], %g2
	mov	%o0, %l1			! save reg pointer
	inc	%g2				! pesky stats
	srl	%i0, 0, %o0			! copy 1st arg, clear high bits
	st	%g2, [%g1 + CPU_SYSINFO_SYSCALL]
	srl	%i1, 0, %o1			! copy 2nd arg, clear high bits

	!
	! Set new state for LWP
	!
	ldx	[THREAD_REG + T_LWP], %l2
	mov	LWP_SYS, %g3
	srl	%i2, 0, %o2			! copy 3rd arg, clear high bits
	stb	%g3, [%l2 + LWP_STATE]
	srl	%i3, 0, %o3			! copy 4th arg, clear high bits
	ldx	[%l2 + LWP_RU_SYSC], %g2	! pesky statistics
	srl	%i4, 0, %o4			! copy 5th arg, clear high bits
	addx	%g2, 1, %g2
	stx	%g2, [%l2 + LWP_RU_SYSC]
	srl	%i5, 0, %o5			! copy 6th arg, clear high bits
	! args for direct syscalls now set up

#ifdef TRAPTRACE
	!
	! make trap trace entry - helps in debugging
	!
	rdpr	%pstate, %l3
	andn	%l3, PSTATE_IE | PSTATE_AM, %g3
	wrpr	%g0, %g3, %pstate		! disable interrupt
	TRACE_PTR(%g3, %g2)			! get trace pointer
	GET_TRACE_TICK(%g1)
	stxa	%g1, [%g3 + TRAP_ENT_TICK]%asi
	ldx	[%l1 + G1_OFF], %g1		! get syscall code
	stha	%g1, [%g3 + TRAP_ENT_TL]%asi
	set	TT_SC_ENTR, %g2
	stha	%g2, [%g3 + TRAP_ENT_TT]%asi
	stxa	%g7, [%g3 + TRAP_ENT_TSTATE]%asi ! save thread in tstate space
	stxa	%sp, [%g3 + TRAP_ENT_SP]%asi
	stxa	%o0, [%g3 + TRAP_ENT_F1]%asi
	stxa	%o1, [%g3 + TRAP_ENT_F2]%asi
	stxa	%o2, [%g3 + TRAP_ENT_F3]%asi
	stxa	%o3, [%g3 + TRAP_ENT_F4]%asi
	stxa	%o4, [%g3 + TRAP_ENT_TPC]%asi
	stxa	%o5, [%g3 + TRAP_ENT_TR]%asi
	TRACE_NEXT(%g3, %g2, %g1)		! set new trace pointer
	wrpr	%g0, %l3, %pstate		! enable interrupt
#endif /* TRAPTRACE */

	!
	! Test for pre-system-call handling
	!
	ldub	[THREAD_REG + T_PRE_SYS], %g3	! pre-syscall proc?
#ifdef SYSCALLTRACE
	sethi	%hi(syscalltrace), %g4
	ld	[%g4 + %lo(syscalltrace)], %g4
	orcc	%g3, %g4, %g0			! pre_syscall OR syscalltrace?
#else
	tst	%g3				! is pre_syscall flag set?
#endif /* SYSCALLTRACE */
	bnz,pn	%icc, _syscall_pre32		! yes - pre_syscall needed
	  nop

	! lwp_arg now set up
3:
	!
	! Call the handler.  The %o's have been set up.
	!
	lduw	[%l1 + G1_OFF + 4], %g1		! get 32-bit code
	set	sysent32, %g3			! load address of vector table
	cmp	%g1, NSYSCALL			! check range
	sth	%g1, [THREAD_REG + T_SYSNUM]	! save syscall code
	bgeu,pn	%ncc, _syscall_ill32
	  sll	%g1, SYSENT_SHIFT, %g4		! delay - get index 
	add	%g3, %g4, %g5			! g5 = addr of sysentry
	ldx	[%g5 + SY_CALLC], %g3		! load system call handler

	brnz,a,pt %g1, 4f			! check for indir()
	mov	%g5, %l4			! save addr of sysentry
	!
	! Yuck.  If %g1 is zero, that means we're doing a syscall() via the
	! indirect system call.  That means we have to check the
	! flags of the targetted system call, not the indirect system call
	! itself.  See return value handling code below.
	!
	set	sysent32, %l4			! load address of vector table
	cmp	%o0, NSYSCALL			! check range
	bgeu,pn	%ncc, 4f			! out of range, let C handle it
	  sll	%o0, SYSENT_SHIFT, %g4		! delay - get index
	add	%g4, %l4, %l4			! compute & save addr of sysent
4:
	call	%g3				! call system call handler
	nop

	!
	! If handler returns long long then we need to split the 64 bit
	! return value in %o0 into %o0 and %o1 for ILP32 clients.
	!	
	lduh    [%l4 + SY_FLAGS], %g4           ! load sy_flags
	andcc	%g4, SE_64RVAL | SE_32RVAL2, %g0 ! check for 64-bit return
	bz,a,pt	%xcc, 5f
	  srl	%o0, 0, %o0			! 32-bit only
	srl	%o0, 0, %o1			! lower 32 bits into %o1
	srlx	%o0, 32, %o0			! upper 32 bits into %o0
5:

#ifdef TRAPTRACE
	!
	! make trap trace entry for return - helps in debugging
	!
	rdpr	%pstate, %g5
	andn	%g5, PSTATE_IE | PSTATE_AM, %g4
	wrpr	%g0, %g4, %pstate		! disable interrupt
	TRACE_PTR(%g4, %g2)			! get trace pointer
	GET_TRACE_TICK(%g2)
	stxa	%g2, [%g4 + TRAP_ENT_TICK]%asi
	lduh	[THREAD_REG + T_SYSNUM], %g2
	stha	%g2, [%g4 + TRAP_ENT_TL]%asi
	mov	TT_SC_RET, %g2			! system call return code
	stha	%g2, [%g4 + TRAP_ENT_TT]%asi
	ldx	[%l1 + nPC_OFF], %g2		! get saved npc (new pc)
	stxa	%g2, [%g4 + TRAP_ENT_TPC]%asi
	ldx	[%l1 + TSTATE_OFF], %g2		! get saved tstate
	stxa	%g2, [%g4 + TRAP_ENT_TSTATE]%asi
	stxa	%sp, [%g4 + TRAP_ENT_SP]%asi
	stxa	THREAD_REG, [%g4 + TRAP_ENT_TR]%asi
	stxa	%o0, [%g4 + TRAP_ENT_F1]%asi
	stxa	%o1, [%g4 + TRAP_ENT_F2]%asi
	stxa	%g0, [%g4 + TRAP_ENT_F3]%asi
	stxa	%g0, [%g4 + TRAP_ENT_F4]%asi
	TRACE_NEXT(%g4, %g2, %g3)		! set new trace pointer
	wrpr	%g0, %g5, %pstate		! enable interrupt
#endif /* TRAPTRACE */
	!
	! Check for post-syscall processing.
	! This tests all members of the union containing t_astflag, t_post_sys,
	! and t_sig_check with one test.
	!
	ld	[THREAD_REG + T_POST_SYS_AST], %g1
#ifdef SYSCALLTRACE
	sethi	%hi(syscalltrace), %g4
	ld	[%g4 + %lo(syscalltrace)], %g4
	orcc	%g4, %g1, %g0			! OR in syscalltrace
#else
	tst	%g1				! need post-processing?
#endif /* SYSCALLTRACE */
	bnz,pn	%icc, _syscall_post32		! yes - post_syscall or AST set
	mov	LWP_USER, %g1
	stb	%g1, [%l2 + LWP_STATE]		! set lwp_state
	stx	%o0, [%l1 + O0_OFF]		! set rp->r_o0
	stx	%o1, [%l1 + O1_OFF]		! set rp->r_o1
	clrh	[THREAD_REG + T_SYSNUM]		! clear syscall code
	ldx	[%l1 + TSTATE_OFF], %g1		! get saved tstate
	ldx	[%l1 + nPC_OFF], %g2		! get saved npc (new pc)
	mov	CCR_IC, %g3
	sllx	%g3, TSTATE_CCR_SHIFT, %g3
	add	%g2, 4, %g4			! calc new npc
	andn	%g1, %g3, %g1			! clear carry bit for no error
	stx	%g2, [%l1 + PC_OFF]
	stx	%g4, [%l1 + nPC_OFF]
	jmp	%l0 + 8
	stx	%g1, [%l1 + TSTATE_OFF]

_syscall_pre32:
	ldx	[%l1 + G1_OFF], %g1
	call	pre_syscall			! abort = pre_syscall(arg0)
	sth	%g1, [THREAD_REG + T_SYSNUM]

	brnz,pn	%o0, _syscall_post32		! did it abort?
	nop
	lduw	[%l1 + O0_OFF + 4], %o0		! reload 32-bit args
	lduw	[%l1 + O1_OFF + 4], %o1
	lduw	[%l1 + O2_OFF + 4], %o2
	lduw	[%l1 + O3_OFF + 4], %o3
	lduw	[%l1 + O4_OFF + 4], %o4
	ba,pt	%xcc, 3b
	lduw	[%l1 + O5_OFF + 4], %o5

	!
	! Floating-point trap was pending at start of system call.
	! Here with:
	!	%l3 = mpcb_flags
	!
_syscall_fp32:
	andn	%l3, FP_TRAPPED, %l3
	st	%l3, [%sp + STACK_BIAS + MPCB_FLAGS] 	! clear FP_TRAPPED
	jmp	%l0 + 8				! return to user_rtt
	clrh	[THREAD_REG + T_SYSNUM]		! clear syscall code

	!
	! illegal system call - syscall number out of range
	!
_syscall_ill32:
	call	nosys
	nop
	!
	! Post-syscall with special processing needed.
	!
_syscall_post32:
	call	post_syscall			! post_syscall(rvals)
	nop
	jmp	%l0 + 8				! return to user_rtt
	nop
	SET_SIZE(syscall_trap32)

#endif /* __sparcv9 */
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
lwp_rtt(void)
{}

#else	/* lint */

	ENTRY_NP(lwp_rtt)
#if (STACK_BIAS != 0)
	ldn	[THREAD_REG + T_STACK], %l7
	sub	%l7, STACK_BIAS, %sp
#else
	ldn	[THREAD_REG + T_STACK], %sp	! set stack at base
#endif
	add	%sp, REGOFF + STACK_BIAS, %l7
	ldx	[%l7 + O0_OFF], %o0
	call	post_syscall
	ldx	[%l7 + O1_OFF], %o1
	ba,a,pt	%xcc, user_rtt
	SET_SIZE(lwp_rtt)

#endif	/* lint */
