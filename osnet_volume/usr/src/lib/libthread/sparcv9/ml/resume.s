/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)resume.s	1.6	97/08/28 SMI"

#include <sys/asm_linkage.h>
#include <sys/fsr.h>
#include <sys/trap.h>
#include "assym.h"
#include "utrace.h"

#ifdef PIC
#include "PIC.h"
#endif

#ifdef TLS
#define	TLS_REG		%g7
#define	THREAD_REG	%l6
#else
#define	TLS_REG	%g7
#define	THREAD_REG TLS_REG
#endif

/*
 * _resume(thread, stk, dontsave)
 *	struct thread *thread;
 *	int dontsave;		don't save current thread's state
 */
	ENTRY(_resume)
	save	%sp, -SA(MINFRAME), %sp
	flushw

	ITRACE_ASM_0(%o1, UTR_FAC_TLIB_SWTCH, UTR_RESUME_FLUSH, TR_rf);
#ifdef TLS
#ifdef PIC
	PIC_SETUP(l7);
	sethi	%hi(_thread), THREAD_REG
	ld	[THREAD_REG+%lo(_thread)], THREAD_REG
	ldn	[%l7+THREAD_REG], THREAD_REG
#else
	setn	_thread, THREAD_REG
#endif
	add	TLS_REG, THREAD_REG, THREAD_REG
#endif
	clr	%i4				! %i4 is 0 when %i5 is junk
	mov	%g0, %l0
	!
	! Determine if current thread is an idlethread
	!
	lduh	[THREAD_REG + T_FLAG], %o0
	set	T_IDLETHREAD, %o1
	andcc	%o0, %o1, %g0
	bnz,a	3f				! if T_IDLETHREAD is set, jmp 3f
	mov	THREAD_REG, %l0			! and store the idle thrd in %l0
	!
	! Determine if current thread is a zombie.
	!
	ldub	[THREAD_REG + T_STATE], %o0
	cmp	%o0, TS_ZOMB
	bne,a	1f				! if !zombie, goto 1:
	cmp	%i2, %g0			! "if (dontsave)" in delay slot
	ba 	4f				! if zombie, goto 4:
	mov	THREAD_REG, %l0			! %l0 is set to zombie
1:
	bne	3f				! if (dontsave) goto 3f
	nop

	mov	%fprs, %o0
	btst	FPRS_FEF, %o0			! is curthread using fpu?
	bz	2f
	stb	%g0, [THREAD_REG + T_FPU_EN]	! curthread not using fpu

	!
	! Save FSR register to curthread
	!
	stn	%fsr, [THREAD_REG + T_FSR]
	mov	1, %o2
	stb	%o2, [THREAD_REG + T_FPU_EN]	! curthread using fpu
2:
	stn	%i7, [THREAD_REG + T_PC]	! save caller's pc
	stn	%fp, [THREAD_REG + T_SP]	! save caller's sp
3:
	stn	%g2, [THREAD_REG + T_G2]	! save caller's %g2
	stn	%g3, [THREAD_REG + T_G3]	! save caller's %g3
	mov	%g0, %fp
	mov	%i1, %sp			! switch to temporary stack
						! switch to temporary stack not
						! done if curthread is a zombie
						! since a zombie's stack is not
						! unlocked
	!
	! curthread's resumed state is now saved. it is
	! ok to unlock curthread's t_lock and acquire the
	! resumed thread's t_lock.
	!
	call	_lwp_mutex_unlock		! unlock curthread's t_lock
	add	THREAD_REG, T_LOCK, %o0

4:
	call	_lwp_mutex_lock			! lock resumed thread's t_lock
	add	%i0, T_LOCK, %o0

	!
	! Determine if resume thread has FSR saved state
	!
	ldub	[%i0 + T_FPU_EN], %o2		! get T_FPU_EN bit from
	tst	%o2				!    new thread
	bz	5f				! is T_FPU_EN bit set?
	stb	%g0, [%i0 + T_FPU_EN]

	!
	! Set enable bit in FPRS and restore FSR
	!
	mov	%fprs, %o0
	or	%o0, FPRS_FEF, %o0		! enable FPU
	mov	%o0, %fprs
	ba	8f
	ldn	[%i0 + T_FSR], %fsr

5:
	!
	! Resume thread is not using FPU
	!
	mov	%fprs, %o0
	andn	%o0, FPRS_FEF, %o0		! disable FPU
	mov	%o0, %fprs

8:
	!
	! Now switch to the resumed thread.
	!
#ifdef TLS
	ldn	[%i0 + T_TLS], TLS_REG		! setup TLS
#else
	mov	%i0, TLS_REG			! setup curthread
#endif
	ldn	[%i0 + T_PC], %i7		! return pc
	ldn	[%i0 + T_SP], %fp		! return sp
	ldn	[%i0 + T_G2], %g2		! %g2
	ldn	[%i0 + T_G3], %g3		! %g3
	mov	%l0, %i0			! _resume_ret(oldthread)
	call	_resume_ret			! re-enable signals
	restore					! return to caller
	SET_SIZE(_resume)

	.global TR_rf;
TR_rf:
	.asciz "in_resume (after flush)";
	.align 4;

#ifdef TLS
/*
 * t_tls()
 *
 * return the offset to "thread" in TLS.
 */
	ENTRY(t_tls)
	setn	_thread, %o0
	retl
	nop
	SET_SIZE(t_tls)
#endif
