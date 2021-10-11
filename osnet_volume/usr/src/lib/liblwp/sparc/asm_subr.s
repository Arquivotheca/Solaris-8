/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)asm_subr.s	1.1	99/10/14 SMI"

	.file	"asm_subr.s"

#include <sys/asm_linkage.h>
#include <sys/trap.h>
#include <SYS.h>

	! This is where execution resumes when a thread created
	! with thr_create() or ptread_create() returns.
	! (See _lwp_makecontext() in libc)
	! We pass the (void *) return value to _thr_terminate()
	ENTRY(_lwp_start)
	nop	! this is the location from which the func() was "called"
	nop
	call	_thr_terminate	! %o0 contains the return value
	nop
	SET_SIZE(_lwp_start)

	ENTRY(_lwp_terminate)
	! Flush the register windows so the stack can be reused.
	ta	ST_FLUSH_WINDOWS
	! All we need to do now is (carefully) call _lwp_exit().
	mov	SYS_lwp_exit, %g1
	ta	SYSCALL_TRAPNUM
	RET		! if we return, it is very bad
	SET_SIZE(_lwp_terminate)

	ENTRY(lwp_setprivate)
	retl
	mov	%o0, %g7
	SET_SIZE(lwp_setprivate)

	ENTRY(__fork1)
	SYSTRAP(fork1)
	SYSCERROR
	tst	%o1	!test for child
	bnz,a	1f	!if !0, then child - jump
	clr	%o0	!child, return (0)
1:			!parent, return (%o0 = child pid)
	RET
	SET_SIZE(__fork1)

	ENTRY(__fork)
	SYSTRAP(fork)
	SYSCERROR
	tst	%o1	!test for child
	bnz,a	1f	!if !0, then child - jump
	clr	%o0	!child, return (0)
1:			!parent, return (%o0 = child pid)
	RET
	SET_SIZE(__fork)

	! _save_nv_regs(savedregs_t *)
	ENTRY(_save_nv_regs)
	add	%o7, 8, %o1
	stn	%o1, [%o0 + 0*GREGSIZE]
	stn	%o6, [%o0 + 1*GREGSIZE]
	stn	%o7, [%o0 + 2*GREGSIZE]
	stn	%g1, [%o0 + 3*GREGSIZE]
	stn	%g2, [%o0 + 4*GREGSIZE]
	stn	%g3, [%o0 + 5*GREGSIZE]
	retl
	stn	%g4, [%o0 + 6*GREGSIZE]
	SET_SIZE(_save_nv_regs)

	ENTRY(_flush_windows)
	retl
	ta	ST_FLUSH_WINDOWS
	SET_SIZE(_flush_windows)

	ENTRY(_getsp)
	retl
	mov	%sp, %o0
	SET_SIZE(_getsp)

	ENTRY(_getfp)
	retl
	mov	%fp, %o0
	SET_SIZE(_getfp)

	ENTRY(__alarm)
	SYSTRAP(alarm)
	RET
	SET_SIZE(__alarm)

	ENTRY(__lwp_alarm)
	SYSTRAP(lwp_alarm)
	RET
	SET_SIZE(__lwp_alarm)

!
! The remainder of this file is blatently stolen from libc
!

	.weak	_liblwp_setitimer
	.type	_liblwp_setitimer, #function
	_liblwp_setitimer = _setitimer
	.weak	setitimer
	.type	setitimer, #function
	setitimer = _setitimer
	ENTRY(_setitimer)
	SYSTRAP(setitimer)
	SYSCERROR
	RET
	SET_SIZE(_setitimer)

	.weak	_liblwp_sigprocmask
	.type	_liblwp_sigprocmask, #function
	_liblwp_sigprocmask = _sigprocmask
	.weak	sigprocmask
	.type	sigprocmask, #function
	sigprocmask = _sigprocmask
	ENTRY(_sigprocmask)
	SYSTRAP(sigprocmask)
	SYSCERROR
	RET
	SET_SIZE(_sigprocmask)

	ENTRY(__sigsuspend_trap)
	SYSTRAP(sigsuspend)
	SYSCERROR
	RET
	SET_SIZE(__sigsuspend_trap)

/* Cancellation stuff */

/*
 * _ex_unwind(void (*func)(void *), void *arg)
 *
 * unwinds two frames and invoke "func" with "arg"
 * supposed to be in libC and libc.
 *
 * Before this call stack is - f4 f3 f2 f1 f0
 * After this call stack is -  f4 f3 f2 func (as if "call f1" is replaced
 *					      by the"call func" in f2)
 * The usage in practice is to call _ex_wind from f0 with argument as
 * _ex_unwind(f0, 0) ( assuming f0 does not take an argument )
 * So, after this call to  _ex_unwind from f0 the stack will look like
 *
 * 	f4 f3 f2 f0 - as if f2 directly called f0 (f1 is removed)
 */
FPTR	= (14*CLONGSIZE)	! offset of frame ptr in frame
PC	= (15*CLONGSIZE)	! offset of PC in frame
WSIZE	= MINFRAME		! size of frame

	ENTRY(_ex_unwind_local)
	ta	ST_FLUSH_WINDOWS	! save all regs on stack
	sub	%sp, WSIZE, %sp		! create MINFRAME word frame for traps
	ldn	[%fp+PC+STACK_BIAS], %i7! load ret addr of second frame below
	ldn	[%fp+FPTR+STACK_BIAS], %fp	! load fp second frame below
	jmpl	%o0, %g0		! invoke func
	restore	%o1, %g0, %o0		! 	with loaded arg
	SET_SIZE(_ex_unwind_local)

/*
 * _ex_clnup_handler(void *arg, void  (*clnup)(void *),
 *					void (*tcancel)(void))
 *
 * This function goes one frame down, execute the cleanup handler with
 * argument arg and invokes func.
 */
	ENTRY(_ex_clnup_handler)
	mov 	%o2, %i2		! save ret func addr before restore
	mov	%o1, %i1		! save cleanup handler address
	restore	%o0, %g0, %o0		! restore with 	with loaded arg
	mov 	%o7, %i3		! load ret addr of caller of t_cancel
	jmpl	%o1, %o7		! invoke func
	mov	%o2, %i2		! save ret func address before call
	jmpl	%i2, %g0		! call ret func with return address
	mov	%i3, %o7		! of caller so it looks like caller
	SET_SIZE(_ex_clnup_handler)	! t_cancel id calling this func

/*
 * _tcancel_all()
 * It jumps to _t_cancel with caller's fp
 */
	ENTRY(_tcancel_all)
	mov	%o7, %o3		! save return address
	mov	%fp, %o0		! fp as first argument
	call	_t_cancel		! call t_tcancel(fp)
	mov	%o3, %o7		! restore return address
	SET_SIZE(_tcancel_all)

/*
 * __sighndlr(int sig, siginfo_t *si, ucontex_t *uc, void (*hndlr)())
 *
 * This is called from sigacthandler() for the entire purpose of
 * comunicating the ucontext to java's stack tracing functions.
 */
	ENTRY(__sighndlr)
	.globl	__sighndlrend
	save	%sp, -SA(MINFRAME), %sp
	mov	%i0, %o0
	mov	%i1, %o1
	jmpl	%i3, %o7
	mov	%i2, %o2
	ret
	restore
__sighndlrend:
	SET_SIZE(__sighndlr)

/*
 * uint32_t swap32(uint32_t *target, uint32_t new);
 * Store a new value into a 32-bit cell, and return the old value.
 */
	ENTRY(swap32)
	swap	[%o0], %o1
	retl
	mov	%o1, %o0
	SET_SIZE(swap32)
