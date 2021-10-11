/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)asm_subr.s	1.1	99/10/14 SMI"

	.file	"asm_subr.s"

#include <SYS.h>

	/ This is where execution resumes when a thread created
	/ with thr_create() or ptread_create() returns.
	/ (See _lwp_makecontext() in libc)
	/ We pass the (void *) return value to _thr_terminate()
	ENTRY(_lwp_start)
	addl	$4, %esp
	UNSAFE_PIC_SETUP(.L1)
	pushl	%eax
	call	fcnref(_thr_terminate)
	addl	$4, %esp	/ actually, never returns
	SET_SIZE(_lwp_start)

	/ All we need to do now is (carefully) call _lwp_exit().
	ENTRY(_lwp_terminate)
	movl	$SYS_lwp_exit, %eax
	lcall	$SYSCALL_TRAPNUM, $0
	ret		/ if we return, it is very bad
	SET_SIZE(_lwp_terminate)

	ENTRY(set_gs)
	movl	4(%esp), %eax
	movw	%ax, %gs
	ret
	SET_SIZE(set_gs)

	ENTRY(lwp_setprivate)
	movl	4(%esp), %eax
	movl	%eax, %gs:0
	ret
	SET_SIZE(lwp_setprivate)

	ENTRY(__fork1)
	SYSTRAP(fork1)
	jae	1f
	jmp	__Cerror
1:
	testl	%edx, %edx	/test for child
	jz	1f		/if 0, then parent
	xorl	%eax, %eax	/child, return (0)
1:
	ret
	SET_SIZE(__fork1)

	ENTRY(__fork)
	SYSTRAP(fork)
	jae	1f
	jmp	__Cerror
1:
	testl	%edx, %edx	/test for child
	jz	1f		/if 0, then parent
	xorl	%eax, %eax	/child, return (0)
1:
	ret
	SET_SIZE(__fork)

/ C return sequence which sets errno, returns -1.

	ENTRY(__Cerror)
	cmpl	$ERESTART, %eax
	jne	1f
	movl	$EINTR, %eax
1:
	PIC_SETUP(.L4)
	pushl	%eax
	call	fcnref(___errno)
	popl	%ecx
	movl	%ecx, (%eax)
	PIC_EPILOG
	movl	$-1, %eax
	ret
	SET_SIZE(__Cerror)

	/ _save_nv_regs(savedregs_t *)
	ENTRY(_save_nv_regs)
	movl	4(%esp), %eax		/ ptr to savedreg struct
	movl	0(%esp), %edx		/ return pc
	movl	%edx, 0(%eax)
	movl	%ebp, 4(%eax)
	movl	%edi, 8(%eax)
	movl	%esi, 12(%eax)
	movl	%ebx, 16(%eax)
	movl	%esp, %edx
	addl	$8, %edx		/ return stack pointer
	movl	%edx, 20(%eax)
	ret
	SET_SIZE(_save_nv_regs)

	ENTRY(_getsp)
	movl	%esp, %eax
	ret
	SET_SIZE(_getsp)

	ENTRY(_getfp)
	movl	%ebp, %eax
	ret
	SET_SIZE(_getfp)

	ENTRY(__alarm)
	SYSTRAP(alarm)
	ret
	SET_SIZE(__alarm)

	ENTRY(__lwp_alarm)
	SYSTRAP(lwp_alarm)
	ret
	SET_SIZE(__lwp_alarm)

/
/ The rest of this file was blatently stolen from libc
/
	ENTRY(__sigsuspend_trap)
	SYSTRAP(sigsuspend)
	jae	1f
	jmp	__Cerror
1:
	ret
	SET_SIZE(__sigsuspend_trap)

	PRAGMA_WEAK(sigprocmask, _sigprocmask)
	PRAGMA_WEAK(_liblwp_sigprocmask, _sigprocmask)
	ENTRY(_sigprocmask)
	SYSTRAP(sigprocmask)
	jae	1f
	jmp	__Cerror
1:
	ret
	SET_SIZE(_sigprocmask)

	PRAGMA_WEAK(setitimer, _setitimer)
	PRAGMA_WEAK(_liblwp_setitimer, _setitimer)
	ENTRY(_setitimer)
	SYSTRAP(setitimer)
	jae	1f
	jmp	__Cerror
1:
	ret
	SET_SIZE(_setitimer)

/* Cancellation stuff */

/*
 * _ex_unwind_local(void (*func)(void *), void *arg)
 *
 * unwinds two frames and invoke "func" with "arg"
 * supposed to be in libC and libc.
 *
 * Before this call stack is - f4 f3 f2 f1 f0
 * After this call stack is -  f4 f3 f2 func (as if "call f1" is replaced
 *                                            by the"call func" in f2)
 * The usage in practice is to call _ex_wind from f0 with argument as
 * _ex_unwind(f0, 0) ( assuming f0 does not take an argument )
 * So, after this call to  _ex_unwind from f0 the satck will look like
 *
 * Note: f2 has no knowledge of "arg" which has been pushed by this
 * function before jumping to func. So there will always be an extra
 * argment on the stack if ever func returns. In our case func never
 * returns, it either calls _ex_unwind() itself or call _thr_exit().
 *
 *      f4 f3 f2 f0 - as if f2 directly called f0 (f1 is removed)
 */

	ENTRY(_ex_unwind_local)
	movl	(%ebp), %edx		/ pop first frame [ goo() ]
	movl	(%edx), %ebp		/ pop second frame [ bar() ]
	movl	4(%edx), %eax		/ save return pc
	movl	%eax, (%edx)		/ mov it up for one arg
	movl	8(%esp), %eax		/ save arg from current stk
	movl	%eax, 4(%edx)		/ put it below ret addr
	movl	4(%esp), %eax		/ get the f() pointer
	movl	%edx, %esp		/ stack pointer at ret addr
	jmp	*%eax			/ [ jump to f() ]
	SET_SIZE(_ex_unwind_local)

/*
 * _ex_clnup_handler(void *arg, void  (*clnup)(void *),
 *                                        void (*tcancel)(void))
 *
 * This function goes one frame down, execute the cleanup handler with
 * argument arg and invokes func.
 */

	ENTRY(_ex_clnup_handler)
	pushl	%ebp			/ save current frame pointer
	pushl	8(%esp)			/ first argument [arg]
	movl	(%ebp), %ebp		/ pop out one frame
	call	*16(%esp)		/ call handler [clnup]
	popl	%eax			/ throw the arg of cleanup handler
	popl	%eax			/ old frame pointer
	movl	12(%esp), %ecx		/ save _tcancel_all addr
	lea	4(%eax), %esp		/ stk points to old frame
	jmp	*%ecx			/ jump to _tcancel_all
					/ it looks like frame below caller
					/ called [tcancel]
	SET_SIZE(_ex_clnup_handler)
/*
 * _tcancel_all()
 * It jumps to _t_cancel with caller's fp
 */
	ENTRY(_tcancel_all)
	UNSAFE_PIC_SETUP(.L5)
	movl	%ebp, 4(%esp)
	jmp	fcnref(_t_cancel)
	SET_SIZE(_tcancel_all)

/*
 * __sighndlr(int sig, siginfo_t *si, ucontext_t *uc, void (*hndlr)())
 *
 * This is called from sigacthandler() for the entire purpose of
 * comunicating the ucontext to java's stack tracing functions.
 */
	ENTRY(__sighndlr)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	16(%ebp)
	pushl	12(%ebp)
	pushl	8(%ebp)
	call	*20(%ebp)
	leave
	ret
	.globl	__sighndlrend
__sighndlrend:
	SET_SIZE(__sighndlr)

/*
 * uint32_t swap32(uint32_t *target, uint32_t new);
 * Store a new value into a 32-bit cell, and return the old value.
 */
	ENTRY(swap32)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	xchgl	(%edx), %eax
	ret
	SET_SIZE(swap32)

/*
 * void **getpriptr(int sel);
 * Return the pointer to the private data for the given selector.
 * It follows the private data itself in the segment.
 */
	ENTRY(getpriptr)
	movw	%fs, %dx
	movl	4(%esp), %eax
	movw	%ax, %fs
	movl	%fs:4, %eax
	movw	%dx, %fs
	ret
	SET_SIZE(getpriptr)
