/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)swtch.s	1.49	99/07/01 SMI"

/*
 * Process switching routines.
 */

#if !defined(lint)
#include "assym.h"
#else	/* lint */
#include <sys/thread.h>
#endif	/* lint */

#include <sys/param.h>
#include <sys/asm_linkage.h>
#include <sys/machtrap.h>
#include <sys/mmu.h>
#include <sys/psr.h>
#include <sys/pcb.h>
#include <sys/machthread.h>
#include <sys/vtrace.h>

/*
 * resume(kthread_id_t)
 *
 * a thread can only run on one processor at a time. there
 * exists a window on MPs where the current thread on one
 * processor is capable of being dispatched by another processor.
 * some overlap between outgoing and incoming threads can happen
 * when they are the same thread. in this case where the threads
 * are the same, resume() on one processor will spin on the incoming 
 * thread until resume() on the other processor has finished with
 * the outgoing thread.
 *
 * The MMU context changes when the resuming thread resides in a different
 * process.  Kernel threads are known by resume to reside in process 0.
 * The MMU context, therefore, only changes when resuming a thread in
 * a process different from curproc.
 *
 * resume_from_intr() is called when the thread being resumed was not 
 * passivated by resume (e.g. was interrupted).  This means that the
 * resume lock is already held and that a restore context is not needed.
 * Also, the MMU context is not changed on the resume in this case.
 *
 * resume_from_zombie() is the same as resume except the calling thread
 * is a zombie and must be put on the deathrow list after the CPU is
 * off the stack.
 */

#if defined(lint)

/* ARGSUSED */
void
resume(kthread_id_t t)
{}

#else	/* lint */

#ifdef	TRACE

TR_resume_end:
	.asciz "resume_end";
	.align 4;
TR_swtch_end:
	.asciz "swtch_end";
	.align 4;

#endif
	ENTRY(resume)
	save	%sp, -SA(MINFRAME), %sp		! save ins and locals

	call	flush_writebuffers		! flush processors write buffers
	nop
					
	call	flush_windows			! flushes all but this window
	st	%fp, [THREAD_REG + T_SP]	! delay - save sp

	st	%i7, [THREAD_REG + T_PC]	! save return address
	ld	[THREAD_REG + T_CPU], %i1	! get CPU pointer

	!
	! Perform context switch callback if set.
	! This handles floating-point and/or coprocessor state saving.
	!
	ld	[THREAD_REG + T_CTX], %g5	! should current thread savectx?
	tst	%g5			
	bz	1f				! skip call when zero
	ld	[%i0 + T_PROCP], %i3		! delay slot - get proc pointer
	call	savectx
	mov	THREAD_REG, %o0			! delay - arg = thread pointer
1:
	ld	[THREAD_REG + T_PROCP], %i2	! load old curproc - for mmu

	!
	! Temporarily switch to idle thread's stack
	!
	ld	[%i1 + CPU_IDLE_THREAD], %o0	! idle thread pointer
	ld	[%o0 + T_SP], %o1		! get onto idle thread stack
	sub	%o1, SA(MINFRAME), %sp		! save room for ins and locals
	clr	%fp

	!
	! Set the idle thread as the current thread
	!
	mov	THREAD_REG, %l3			! save %g7 (current thread)
	mov	%o0, THREAD_REG			! set %g7 to idle
	st	%o0, [%i1 + CPU_THREAD]		! set CPU's thread to idle

	!
	! Clear and unlock previous thread's t_lock
	! to allow it to be dispatched by another processor.
	!
	clrb	[%l3 + T_LOCK]			! clear tp->t_lock

	!
	! IMPORTANT: Registers at this point must be:
	!	%i0 = new thread
	!	%i1 = flag (non-zero if unpinning from an interrupt thread)
	!	%i1 = cpu pointer
	!	%i2 = old proc pointer
	!	%i3 = new proc pointer
	!	
	! Here we are in the idle thread, have dropped the old thread.
	! 
	ALTENTRY(_resume_from_idle)
	cmp 	%i2, %i3		! resuming the same process?
	be	5f			! yes.
	ld	[%i3 + P_AS], %o0	! delay, load p->p_as

	!
	! Check to see if we already have context. If so then set up the
	! context. Otherwise we leave the proc in the kernels context which
	! will cause it to fault if it ever gets back to userland.
	!
	ld	[%o0 + A_HAT], %o0	! load (p->p_as->a_hat)
	ld	[%o0 + SRMMU_HAT], %o0	! check (a_hat->hat_data)

	ldsh	[%o0 + SRMMU_CTX], %o0	! check (a_hat->hat_data->srmmu_ctx)
	mov	-1, %o1
	cmp	%o0, %o1		! ctx == -1 means no context.
	bz,a	4f			! 
	clr	%o0      		! no context, use kas.

	!
	! Switch to different address space.
	!
4:
	call	mmu_setctxreg		! switch to other context (maybe 0)
	nop
5:
	!
	! spin until dispatched thread's mutex has
	! been unlocked. this mutex is unlocked when
	! it becomes safe for the thread to run.
	! 
	ldstub	[%i0 + T_LOCK], %o0	! lock curthread's t_lock
6:
	tst	%o0
	bnz	7f			! lock failed
	ld	[%i0 + T_PC], %i7	! delay - restore resuming thread's pc

	!
	! Fix CPU structure to indicate new running thread.
	! Set pointer in new thread to the CPU structure.
	! XXX - Move migration statistic out of here
	!
        ld      [%i0 + T_CPU], %g2	! last CPU to run the new thread
        cmp     %g2, %i1		! test for migration
        be      4f			! no migration
	ld	[%i0 + T_LWP], %o1	! delay - get associated lwp (if any)
        ld      [%i1 + CPU_SYSINFO_CPUMIGRATE], %g2 ! cpu_sysinfo.cpumigrate++
        inc     %g2
        st      %g2, [%i1 + CPU_SYSINFO_CPUMIGRATE]
	st	%i1, [%i0 + T_CPU]	! set new thread's CPU pointer
4:
	st	%i0, [%i1 + CPU_THREAD]	! set CPU's thread pointer
	mov	%i0, THREAD_REG		! update global thread register
	ldstub	[%i0 + T_LOCK_FLUSH], %g0 ! synchronize with mutex_exit()
	tst	%o1			! does new thread have an lwp?
	st	%o1, [%i1 + CPU_LWP]	! set CPU's lwp ptr
	bz,a	1f			! if no lwp, branch and clr mpcb
	st	%g0, [%i1 + CPU_MPCB]
	ld	[%i0 + T_STACK], %o0
	st	%o0, [%i1 + CPU_MPCB]	! set CPU's mpcb pointer
1:
	!
	! Switch to new thread's stack
	!
	ld	[%i0 + T_SP], %o0	! restore resuming thread's sp
	sub	%o0, SA(MINFRAME), %sp	! in case of intr or trap before restore
	mov	%o0, %fp
	!
	! Restore resuming thread's context
	!
	ld	[%i0 + T_CTX], %g5 	! should resumed thread restorectx?
	tst	%g5			
	bz	8f			! skip restorectx() when zero
	mov	%psr, %l0		! delay - get old %psr
	call	restorectx		! thread can not sleep on temp stack
	mov	THREAD_REG, %o0		! delay slot - arg = thread pointer
	mov	%psr, %l0		! get %psr again - may have EF on now
	!
	! Set priority as low as possible, blocking all interrupt threads
	! that may be active.
	!
8:
	ld	[%i1 + CPU_BASE_SPL], %o1
	andn	%l0, PSR_PIL, %l0	! clear out old PIL
	wr	%l0, %o1, %psr		! XOR in new PIL
	nop; nop			! psr delay
#ifdef TRACE
	sethi	%hi(tracing_state), %o0
	ld	[%o0 + %lo(tracing_state)], %o0
	cmp	%o0, VTR_STATE_PERPROC
	bne	1f
	nop
	call	trace_check_process
	nop
1:
#endif	/* TRACE */
	TRACE_ASM_0 (%o1, TR_FAC_DISP, TR_RESUME_END, TR_resume_end)
	TRACE_ASM_0 (%o1, TR_FAC_DISP, TR_SWTCH_END, TR_swtch_end)
	ret				! resume curthread
	restore

	!
	! lock failed - spin with regular load to avoid cache-thrashing.
	!
7:
	ldub	[%i0 + T_LOCK], %o0
1:
	tst	%o0
	bz,a	6b			! was unlocked, try to lock it again
	ldstub	[%i0 + T_LOCK], %o0	! delay - lock curthread's mutex
	b	1b			! spin until it is unlocked
	ldub	[%i0 + T_LOCK], %o0	! delay - reload mutex
	SET_SIZE(_resume_from_idle)
	SET_SIZE(resume)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
resume_from_zombie(kthread_id_t t)
{}

#else	/* lint */

	ENTRY(resume_from_zombie)
	save	%sp, -SA(MINFRAME), %sp		! save ins and locals
	ld	[THREAD_REG + T_CPU], %i1	! cpu pointer
					
	call	flush_windows			! flushes all but this window
	ld	[THREAD_REG + T_PROCP], %i2	! delay - old procp for mmu ctx

	!
	! Temporarily switch to the idle thread's stack so that
	! the zombie thread's stack can be reclaimed by the reaper.
	!
	ld	[%i1 + CPU_IDLE_THREAD], %o2	! idle thread pointer
	ld	[%o2 + T_SP], %o1		! get onto idle thread stack
	sub	%o1, SA(MINFRAME), %sp		! save room for ins and locals
	clr	%fp
	!
	! Set the idle thread as the current thread.
	! Put the zombie on death-row.
	! 	
	mov	THREAD_REG, %o0			! save %g7 = curthread for arg
	mov	%o2, THREAD_REG			! set %g7 to idle
	st	%g0, [%i1 + CPU_MPCB]		! clear mpcb
	call	reapq_add			! reapq_add(old_thread);
	st	%o2, [%i1 + CPU_THREAD]		! delay - CPU's thread = idle

	!
	! resume_from_idle args:
	!	%i0 = new thread
	!	%i1 = cpu
	!	%i2 = old proc
	!	%i3 = new proc
	!	
	b	_resume_from_idle		! finish job of resume
	ld	[%i0 + T_PROCP], %i3		! new process
	
	SET_SIZE(resume_from_zombie)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
resume_from_intr(kthread_id_t t)
{}

#else	/* lint */

	ENTRY(resume_from_intr)
	save	%sp, -SA(MINFRAME), %sp		! save ins and locals
					
	call	flush_windows			! flushes all but this window
	st	%fp, [THREAD_REG + T_SP]	! delay - save sp
	st	%i7, [THREAD_REG + T_PC]	! save return address

	ld	[%i0 + T_PC], %i7		! restore resuming thread's pc
	ld	[THREAD_REG + T_CPU], %i1	! cpu pointer

	!
	! Fix CPU structure to indicate new running thread.
	! The pinned thread we're resuming already has the CPU pointer set.
	!
	mov	THREAD_REG, %l3		! save old thread
	st	%i0, [%i1 + CPU_THREAD]	! set CPU's thread pointer
	mov	%i0, THREAD_REG		! update global thread register
	ldstub	[%i0 + T_LOCK_FLUSH], %g0 ! synchronize with mutex_exit()
	!
	! Switch to new thread's stack
	!
	ld	[%i0 + T_SP], %o0	! restore resuming thread's sp
	sub	%o0, SA(MINFRAME), %sp	! in case of intr or trap before restore
	mov	%o0, %fp
	clrb	[%l3 + T_LOCK]		! clear intr thread's tp->t_lock

#ifdef TRACE
	sethi	%hi(tracing_state), %o0
	ld	[%o0 + %lo(tracing_state)], %o0
	cmp	%o0, VTR_STATE_PERPROC
	bne	1f
	nop
	call	trace_check_process
	nop
1:
#endif	/* TRACE */
	TRACE_ASM_0 (%o1, TR_FAC_DISP, TR_RESUME_END, TR_resume_end)
	TRACE_ASM_0 (%o1, TR_FAC_DISP, TR_SWTCH_END, TR_swtch_end)
	ret				! resume curthread
	restore
	SET_SIZE(resume_from_intr)
#endif /* lint */


/*
 * thread_start()
 *
 * the current register window was crafted by thread_run() to contain
 * an address of a procedure (in register %i7), and its args in registers
 * %i0 through %i5. a stack trace of this thread will show the procedure
 * that thread_start() invoked at the bottom of the stack. an exit routine
 * is stored in %l0 and called when started thread returns from its called
 * procedure.
 */

#if defined(lint)

void
thread_start(void)
{}

#else	/* lint */

	ENTRY(thread_start)
	mov	%i0, %o0
	jmpl 	%i7, %o7	! call thread_run()'s start() procedure.
	mov	%i1, %o1

	call	thread_exit	! destroy thread if it returns.
	nop
	unimp 0
	SET_SIZE(thread_start)

#endif	/* lint */
