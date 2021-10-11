/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)sparcsubr.s	1.16	99/01/28 SMI"

#include <sys/asm_linkage.h>
#include <sys/trap.h>
#include <assym.h>
#include "utrace.h"
#include <sys/stack.h>

#ifdef PIC
#include "PIC.h"
#endif



#define	TLS_REG	%g7

#ifdef TLS
#define	THREAD_REG	%l6
#else
#define	THREAD_REG	%g7
#endif /* TLS */

/* PROBE_SUPPORT begin */
/*
 * _thread_probe_getfunc()
 *
 * return the address of the current thread's tpdp
 * CAUTION: This code does not work if TLS is defined.
 */
	ENTRY(_thread_probe_getfunc)
#ifdef TLS
	retl
	mov 	%g0, %o0		! disable if compiled with TLS
#else
	retl
	ldn	[%g7 + T_TPDP], %o0	! return thread probe data
#endif
	SET_SIZE(_thread_probe_getfunc)
/* PROBE_SUPPORT end */

/*
 * _manifest_thread_state()
 *
 * guarantees that the thread has consistent state saved in
 * its thread struct.
 */
	ENTRY(_manifest_thread_state)
	flushw				! save all regs on stack
	st	%g2, [THREAD_REG + T_G2]
	st	%g3, [THREAD_REG + T_G3]
	st	%i7, [THREAD_REG + T_PC]
	retl
	mov	%fp, %o0
	SET_SIZE(_manifest_thread_state)

/*
 * getsp()
 *
 * return the current sp (for debugging)
 */
	ENTRY(_getsp)
	retl
	mov	%sp, %o0
	SET_SIZE(_getsp)

	ENTRY(_getfp)
	retl
	mov	%fp, %o0
	SET_SIZE(_getfp)
/*
 * getpc()
 *
 * return the current pc of getpc()'s caller
 */
	ENTRY(_getpc)
	retl
	mov	%o7, %o0
	SET_SIZE(_getpc)
/*
 * getcaller()
 *
 * return the pc of the calling point in the routine which called the routine
 * which called getcaller()
 */
	ENTRY(_getcaller)
	retl
	mov	%i7, %o0
	SET_SIZE(_getcaller)

/*
 * caller()
 *
 * return the address of our caller's caller.
 */
	ENTRY(caller)
	retl
	mov	%i7, %o0
	SET_SIZE(caller)

/*
 * whereami()
 *
 * Store the address at which execution will resume following this
 * routine at the address pointed at by %o0.
 */
	ENTRY(_whereami)
	add	%o7, 8, %o1
	retl
	stn	%o1, [%o0]
	SET_SIZE(_whereami)

/*
 * _tlsbase()
 *
 * returns the base address for a thread's TLS segment.
 */
	ENTRY(_tlsbase)
	retl
	mov %g7, %o0
	SET_SIZE(_tlsbase)

/*
 * _curthread()
 *
 * return the value of the currently active thread.
 */
	ENTRY(_curthread)
	retl
	mov %g7, %o0
	SET_SIZE(_curthread)

/*
 * _flush_store
 */
	ENTRY(_flush_store)
	retl
	membar #StoreStore|#StoreLoad	! XX64 Is this right?
	SET_SIZE(_flush_store)

/*
 * _save_thread(uthread_t *t)
 */
	ENTRY(_save_thread)
	stn	%i7, [%o0 + T_PC]
	retl
	stn	%fp, [%o0 + T_SP]
	SET_SIZE(_save_thread)

/*
 * _switch_stack(caddr_t stk, uthread_t *ot, uthread_t *t)
 *
 *  flush regiser windows to stack, release curthread's t_lock,
 *  and switch to new stack, "stk".
 */
	ENTRY(_switch_stack)
	flushw
	save	%sp, -SA(MINFRAME), %sp
	sub	%i0, SA(MINFRAME), %fp
	stn	%i0, [%fp + 14*CLONGSIZE]
	call	_lwp_mutex_unlock	!_lwp_mutex_unlock(&t->t_lock)
	add	%i1, T_LOCK, %o0
	restore
	mov	%o0, %fp
	retl
	mov	%o2, %o0
	SET_SIZE(_switch_stack)

/*
 * _init_cpu(t)
 *
 * set the initial cpu to point at the initial thread.
 */
	ENTRY(_init_cpu)
	retl
	mov	%o0, TLS_REG
	SET_SIZE(_init_cpu)

/*
 * _threadjmp(uthread_t *t, uthread_t *ot)
 *
 * jump to thread "t" and do some clean up if old thread "ot"
 * is a zombie.
 */
	ENTRY(_threadjmp)
	flushw
#ifdef TLS
	ldn	[%o0 + T_TLS], TLS_REG
#else
	mov	%o0, %g7
#endif
	ldn	[%o0 + T_PC], %i7		! return pc
	ldn	[%o0 + T_SP], %fp		! return sp
	mov	%o1, %i0
	call	_resume_ret
	restore
	SET_SIZE(_threadjmp)

/*
 * _flush_user_windows()
 *
 * flush user's register windows to the stack.
 */
	ENTRY(_flush_user_windows)
	retl
	flushw
	SET_SIZE(_flush_user_windows)

/*
 * _stack_switch(size_t stk)
 *
 * force the current thread to start running on the stack
 * specified by "stk".
 */
	ENTRY(_stack_switch)
	clr	%fp
	retl
	mov	%o0, %sp
	SET_SIZE(_stack_switch)

/*
 * _getpsr()
 *
 * returns the current value of the psr.
 */
	ENTRY(_getpsr)
	retl
	ta	ST_GETPSR

/*
 * _setpsr(int v)
 *
 * sets the user accessibles fields in the psr.
 */
	ENTRY(_setpsr)
	retl
	ta	ST_SETPSR

/*
 * _getfprs()
 *
 * returns the current value of the floating point registers state register
 */
	ENTRY(_getfprs)
	retl
	mov	%fprs, %o0

/*
 * _copyfsr(t)
 *
 * copy the current FSR.
 */
	ENTRY(_copyfsr)
	retl
	stn	%fsr, [%o0]
	SET_SIZE(_copyfsr)

/*
 * _savefsr(t)
 *
 * save the current FSR to thread "t".
 */
	ENTRY(_savefsr)
	retl
	stn	%fsr, [%o0 + T_FSR]
	SET_SIZE(_savefsr)

/*
 * _restorefsr(t)
 *
 * restore thread "t" FSR into the current FSR.
 *
 * The reason for the nops - on page 92 of the SPARC V8 Architecture Manual:
 *	If any of the three instructions that follow (in time)
 *	a LDFSR is an FBfcc, the value of the fcc field of the
 *	FSR which is seen by the FBfcc is undefined.
 */
	ENTRY(_restorefsr)
	ldn	[%o0 + T_FSR], %fsr
	nop
	retl
	nop
	SET_SIZE(_restorefsr)

/*
 * _thread_start()
 *
 * the current register window was crafted by thread_load() to contain
 * an address of a function in register %i7 and its arg in register
 * %i0. thr_exit() is called if the procedure returns.
 */
	ENTRY(_thread_start)
#ifdef TLS
#ifdef PIC
	PIC_SETUP(l7);
	sethi	%hi(_thread), THREAD_REG
	ld	[THREAD_REG+%lo(_thread)], THREAD_REG
	ldn	[%l7+THREAD_REG], THREAD_REG
#else
	setn	_thread, %l1, THREAD_REG
#endif	/* PIC */
	call	_lwp_self
	add	TLS_REG, THREAD_REG, THREAD_REG
	st	%o0, [THREAD_REG + T_LWPID]
#else
	call	_lwp_self
	nop
	st	%o0, [THREAD_REG + T_LWPID]
#endif	/* TLS */

#if defined(ITRACE) || defined(UTRACE)
	TRACE_ASM_0(%o3, UTR_FAC_TRACE, UTR_THR_LWP_MAP, UTR_thr_lwp_map);
	.seg ".data"
	.align 4
	.global UTR_thr_lwp_map;
UTR_thr_lwp_map:
	.asciz "thr_lwp_map";
	.align 4;
	.seg ".text"
#endif
	ld	[THREAD_REG + T_USROPTS], %l1
	set	THR_BOUND, %l2
	andcc	%l1, %l2, %g0
	bz	1f			! branch if unbound
	nop
	or	%g0, -1, %o0
	call	_sc_setup		! else if bound call _sc_setup(-1, 0)
	or	%g0, 0, %o1
	! Determine if bound thread has FSR saved state to inherit.
	ldub    [THREAD_REG + T_FPU_EN], %o2	! get T_FPU_EN bit from
	tst	%o2				!    new thread
	bz	2f
	nop
	ldn	[THREAD_REG + T_FSR], %fsr
2:
#ifdef ITRACE
	ld	[THREAD_REG + T_LWPID], %o2
	/* temporary trace point for lwp create */
	ITRACE_ASM_1(%o3, TR_FAC_SYS_LWP, TR_SYS_LWP_CREATE_END2,
	    UTR_lwp_create_end2, %o2);
1:	ITRACE_ASM_1(%o3, UTR_FAC_THREAD, UTR_THR_CREATE_END2,
	    UTR_thr_create_end2, THREAD_REG);

	.seg ".data"
	.align 4
	.global UTR_lwp_create_end2;
UTR_lwp_create_end2:
	.asciz "lwp_create end2:lwpid 0x%x";
	.align 4;

	.global UTR_thr_create_end2;
UTR_thr_create_end2:
	.asciz "thr_create end2:tid 0x%x";
	.align 4;
	.seg ".text"
#else
1:
#endif

	jmpl	%i7, %o7
	mov	%i0, %o0	!call func(arg)

	call	_thr_exit	!destroy thread if it returns
	nop
	unimp 0
	SET_SIZE(_thread_start)
/*
 * lwp_terminate(thread)
 *
 * This is called when a bound thread does a thr_exit(). The
 * exiting thread is placed on deathrow and switches to a dummy
 * stack that contains one register window. This stack frame can
 * be shared because the register window is not used when the SP
 * points to this frame. Note that the register window is not used
 * only when you avoid using the locals (%ls) and ins (%is) as
 * scratch registers after the switch. The only registers safe
 * to use after the switch are the outs (%os).
 * Registers %o2 and %o3 are used below after switching to the
 * _SHAREDFRAME stack. This stack is global and shared between all
 * lwps which are terminating. Hence the saves and restores of the
 * %l's and %i's to the same stack could result in one lwp smashing
 * the locals/ins of another lwp making these unusable after the
 * stack switch
 */
	.seg ".data"
	.global _SHAREDFRAME;
	.align 16
	.skip 64*CLONGSIZE
_SHAREDFRAME:
	.skip MINFRAME
	.seg ".text"

	ENTRY(_lwp_terminate)
	flushw
	call	_reapq_add		! _reapq_add(thread)
	nop
#ifdef PIC
	PIC_SETUP(o2);			! set up %o2, do not touch after this!
	sethi	%hi(_SHAREDFRAME), %o0
	or	%o0, %lo(_SHAREDFRAME), %o0
	ldn	[%o2+%o0], %o0
	sub	%o0, STACK_BIAS, %o0
	mov	%o0, %sp		! use SHAREDFRAME as stack
					! AS soon as the above stack switch
					! occurs, all locals and ins are
					! unusable since the stack is a global
					! which is shared by all terminating
					! lwps after this point. Use only
					! %o2 and %o3 here. The calls to
					! _lwp_mutex_unlock() and _lwp_exit()
					! below do not do a save/restore,
					! (see libc wrapper) so this is safe.
					! We also do not care about preserving
					! the values in %o2 and %o3 ACROSS the
					! call to _lwp_mutex_unlock() since
					! they are written to right after the
					! return from this call.
	sethi	%hi(_reaplockp), %o0
	or	%o0, %lo(_reaplockp), %o0
	ldn	[%o2+%o0], %o0		! %o0 points to _reaplockp
#else
	setn	_SHAREDFRAME, %o2, %o0	! stop running on thread's stack
	sub	%o0, STACK_BIAS, %o0
	mov	%o0, %sp		! use SHAREDFRAME as stack
					! NOTE: AFTER THIS POINT, USE ONLY THE
					! %o REGISTERS: %o0, %o2, %o3, %o4, %o5
					! AS SCRATCH REGISTERS
	setn	_reaplockp, %o2, %o0	! here, %o0 is the address of _reaplockp
#endif
					! IMPORTANT NOTE:
					! after switching to this stack, no
					! subsequent calls should be such that
					! they use the stack - specifically
					! remove the possibility that the
					! dynamic linker is called after the
					! stack switch, to resolve symbols
					! like _lwp_mutex_unlock, _lwp_exit
					! and _reaplock which are referenced
					! below.
					! This is ensured by resolving them in
					! _t0init() and storing the addresses of
					! these symbols into libthread pointers
					! and de-referencing these pointers
					! here.
					! Make sure that, in the future, if
					! any other symbols are added in the
					! code below besides these, they should
					! also have the matching libthread
					! pointers, initialized in _t0init().
					! Also, ensure that the 2 routines
					! called from here: _lwp_mutex_unlock()
					! and _lwp_exit():
					!	- do not execute a save/restore
					!   AND - do not use globals or calls
					!	  requiring run-time linker
					!	  resolution.

	ldn	[%o0], %o0		! read _reaplockp for _reaplock address
	clr	%g7			! make g7 invalid by setting it to zero
					! so that a debugger knows that the
					! thread has disappeared.
#ifdef PIC
	sethi	%hi(_lwp_mutex_unlockp), %o3
	or	%o3, %lo(_lwp_mutex_unlockp), %o3
	ldn	[%o2+%o3], %o3
#else
	setn	_lwp_mutex_unlockp, %o2, %o3
#endif
	ldn 	[%o3], %o3		!
	call	%o3			! call _lwp_mutex_unlock()
	nop				!
					! NOTE: The implementation of
					! _lwp_mutex_unlock() should NOT do a
					! save/restore. This is called out in
					! libc.
					! Note that _lwp_exit() or
					! _lwp_mutex_unlock(), etc. should never
					! result in calls to any functions that
					! are unbound, otherwise we have a
					! deadlock, e.g. tracing, etc. If
					! _lwp_exit needs to be traced, trace it
					! in the kernel - not at user level.
#ifdef PIC
	PIC_SETUP(o2);			! could have been trashed by
					! _lwp_mutex_unlock(); set-up again.
	sethi	%hi(_lwp_exitp), %o3
	or	%o3, %lo(_lwp_exitp), %o3
	ldn	[%o2+%o3], %o3
#else
	setn	_lwp_exitp, %o2, %o3
#endif
	ldn 	[%o3], %o3		! Read _lwp_exitp for addr. of _lwp_exit
	call	%o3			! call _lwp_exit()
	nop
	unimp	0
	SET_SIZE(_lwp_terminate)

/*
 * The following .init section gets called by crt1.s through _init(). It takes
 * over control from crt1.s by making sure that the return from _init() goes
 * to the _tcrt routine above.
 */
	.section  ".init", #alloc, #execinstr
	.align  4
	call	_t0init		! setup the primordial thread
	nop

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
	flushw				! save all regs on stack
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
