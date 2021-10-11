/*	Copyright (c) 1998 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident   "@(#)i386subr.s 1.31     99/01/27     SMI"


#include <sys/asm_linkage.h>
#include <sys/trap.h>
#include <assym.s>
#include <pic.h>
#include <thread.h>

/* PROBE_SUPPORT begin */
/*
 * _thread_probe_getfunc()
 *
 * return the address of the current thread's tpdp
 */
	ENTRY(_thread_probe_getfunc)
	movl	%gs:0, %eax
	movl	T_TPDP(%eax), %eax	/ return thread probe data
	ret
	SET_SIZE(_thread_probe_getfunc)
/* PROBE_SUPPORT end */

/*
 * getgs()
 *
 * return the what is in the caller %gs register
 */
	ENTRY(_getgs)
	pushl	%gs
	popl	%eax
	ret
	SET_SIZE(_getgs)

/*
 * _manifest_thread_state()
 *
 * guarantee that a consistent thread state is saved
 * to the caller's thread struct, and return a consistent
 * frame pointer. the frame pointer is the frame pointer
 * for the caller's caller.
 */
	ENTRY(_manifest_thread_state)
	movl	%gs:0, %eax
	movl	%esi, T_ESI(%eax)
	movl	%edi, T_EDI(%eax)
	movl	%ebx, T_EBX(%eax)
	movl	4(%ebp), %ecx
	movl	%ecx, T_PC(%eax)
	movl	(%ebp), %eax
	ret
	SET_SIZE(_manifest_thread_state)

/*
 * getsp()/getfp()
 *
 * return the current sp/fp (for debugging)
 */

	ALTENTRY(_getsp)
	ENTRY(_getfp)
	movl	%ebp, %eax
	ret
	SET_SIZE(_getfp)
/*
 * getpc()
 *
 * return the current pc of getpc()'s caller
 */
	ENTRY(_getpc)
	movl	(%esp), %eax
	ret
	SET_SIZE(_getpc)
/*
 * _copy_fctrl_fstat(fctrl, fstat)
 *
 * copy FPU environment.
 */
	ENTRY(_copy_fctrl_fstat)
	fwait
	movl	4(%esp),%eax
	fnstcw	(%eax)
	movl	8(%esp),%eax
	fnstsw	(%eax)
	ret
	SET_SIZE(_copy_fctrl_fstat)

/*
 * getcaller()
 *
 * return the pc of the calling point in the routine which called the routine
 * which called getcaller()
 */
	ENTRY(_getcaller)
	ALTENTRY(caller)
	movl	4(%ebp), %eax
	ret
	SET_SIZE(_getcaller)

/*
 * _whereami
 * Like getpc, but return value is passed by reference
 */
	ENTRY(_whereami)
	movl	4(%esp), %eax
	movl	(%esp), %ecx
	movl	%ecx, (%eax)
	ret
	SET_SIZE(_whereami)

/*
 * _curthread()
 *
 * return the value of the currently active thread.
 */
	ENTRY(_curthread)
	movl	%gs:0, %eax
	ret
	SET_SIZE(_curthread)

/*
 * _cleanup_ldt()
 *
 * call __free_all_selectors with %gs (so it'll be preserved)
 */
	ENTRY(_cleanup_ldt)
	pic_prolog(.L7)
	xorl	%eax, %eax
	movw	%gs, %ax
	pushl	%eax
	call	fcnref(__free_all_selectors)
	addl	$4, %esp
	pic_epilog
	ret
	SET_SIZE(_cleanup_ldt)

/*
 * _init_cpu(t)
 *
 * set the initial cpu to point at the initial thread.
 */
	.comm	_t0ldt, 8, 4

	ENTRY(_init_cpu)
	pic_prolog(.L6)
	pushl	$8
	pushl	dataaddr(_t0ldt)
	call	fcnref(__alloc_selector) / allocate an ldt slot
	addl	$8, %esp
	pushl	%eax
	popl	%gs			/ get selector into %gs
	movl	dataaddr(_t0ldt), %eax
	movl	%eax, %gs:4		/ set pointer to _t0ldt as in __setupgs
	movl	0x8(%esp), %eax
	movl	%eax, %gs:0		/ set pointer to thread
	pic_epilog
	ret
	SET_SIZE(_init_cpu)

/*
 * _thread_start(thread, arg, func)
 *
 * Sets curthread = thread, then
 * sets curthread->t_lwpid to _lwp_self(), then calls func(arg).
 * _thr_exit() is called if the procedure returns.
 * Since we never return, we can use non-volatiles w/o saving them.
 */
	ENTRY(_thread_start)
	unsafe_pic_prolog(.L3)		/ doesn't push %ebx
	call	fcnref(_lwp_self)
	popl	%ecx			/ get thread into %ecx
	movl	%eax, T_LWPID(%ecx)	/ curthread->t_lwpid = _lwp_self();
	movl	T_USROPTS(%ecx), %eax
	andl	$THR_BOUND, %eax	/ is this a bound thread?
	jz	L1			/ if not, skip
	/ Load the FPU env. for bound threads
	leal    T_FPENV(%ecx), %eax
	fldenv	(%eax)
	pushl	$0
	pushl	$-1
	call	fcnref(_sc_setup)	/ if it is, call _sc_setup(-1, 0)
	addl	$8, %esp
L1:	
	call	*0x4(%esp)		/ call func (arg is in place)
	addl	$8, %esp
	pushl   %eax
	call	fcnref(_thr_exit)	/ .. and call _thr_exit
	SET_SIZE(_thread_start)

/*
 * lwp_terminate(thread)
 *
 * This is called when a bound thread does a _thr_exit(). The
 * exiting thread is placed on deathrow and switches to a dummy stack.
 * XXX The use of _SHAREDFRAME seems suspect, except that the
 * XXX data that goes on the stack is the same in all instances.
 *
 * We take great care to avoid the dynamic linker after switching
 * to the small stack...
 */
	.bss	_SHAREDFRAME, 512, 4

	.text
	ENTRY(_lwp_terminate)
	popl	%eax			/ discard return address

	pic_prolog(.L4)			/ pushs %ebx

	/ order is extremely important here. the terminating thread
	/ should always be freed before its LWP's private data pointer
	/ as represented by the "gs" segment is freed. reversing the
	/ order can result in SIGSEGV's if the freed "gs" segment is
	/ referenced. Any reference through "curthread" is through the
	/ "gs" segment. So, it's not really safe to call any c routine,
	/ or use any global symbol once the "gs" segment is freed. the
	/ terminating thread's stack can not be re-used until reaplock
	/ is released which happens below, after safely switching to
	/ a temporary stack.

	pushl	0x4(%esp)
	call	fcnref(_reapq_add)	/ _reapq_add(thread)
	addl	$4, %esp

	xorl	%eax, %eax
	movw	%gs, %ax
	pushl	%eax
	call	fcnref(__freegs)
	addl	$4, %esp

	xorw	%ax, %ax		/ clear GS segment so that debugger
	movw	%ax, %gs		/ knows that "curthread" is invalid

	/ Create a small temporary stack which is used to contain the return
	/ address from the system calls. Don't have worry about multiple
	/ lwp's using the same stack area because the return address stored
	/ will be exactly the same.
	movl	dataaddr(_SHAREDFRAME), %esp / switch to tiny stack for exit
	addl	$256, %esp

	pushl	dataaddr(_reaplock)
	movl	dataaddr(_lwp_mutex_unlockp), %edx
	movl	(%edx), %edx
	call	*%edx
	addl	$4, %esp			/ discard argument
						/ NOTE that after this point,
						/ since the stack is shared, no
						/ pushes should be made to the
						/ stack. Otherwise, the call
						/ to _lwp_mutex_unlock() will
						/ have a trashed argument, or
						/ worse. So, the following call
						/ to _lwp_exit(), should be
						/ a "jmp", instead of a "call"
						/ so that the return address
						/ is not pushed on the stack.
						/ Since the call to _lwp_exit()
						/ is not supposed to returm,
						/ this is OK.

	movl	dataaddr(_lwp_exitp), %edx
	movl	(%edx), %edx
	jmp	*%edx				/ 

	SET_SIZE(_lwp_terminate)

/*
 * __getxregsize()
 *
 * return the size of the extra register state.
 */
	ENTRY(__getxregsize)
	subl	%eax, %eax
	ret
	SET_SIZE(__getxregsize)

/*
 * The following .init section gets called by crt1.s through _init(). It takes
 * over control from crt1.s by making sure that the return from _init() goes
 * to the _tcrt routine above.
 */
	.section .init
	call	fcnref(_init386)	/ do 386-specific initialization
	call	fcnref(_t0init)		/ setup the primordial thread


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
	movl	(%ebp),	%edx		/ pop first frame [ goo() ]
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
	unsafe_pic_prolog(.L8)
	movl	%ebp, 4(%esp)
	jmp	fcnref(_t_cancel)
	SET_SIZE(_tcancel_all)

/*
 * __sighndlr(int sig, siginfo_t *si, ucontext_t *uc, void (*hndlr)())
 */
	ENTRY(__sighndlr)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	16(%ebp)
	pushl	12(%ebp)
	pushl	8(%ebp)
	call	*20(%ebp)
	leave
	.globl	__sighndlrend
__sighndlrend:
	ret
	SET_SIZE(__sighndlr)
