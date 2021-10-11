/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)swtch.s	1.43	99/04/13 SMI"

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
#include <sys/mmu.h>
#include <sys/pcb.h>
#include <sys/machthread.h>
#include <sys/privregs.h>
#include <sys/vtrace.h>
#include <sys/spitregs.h>

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

	membar	#Sync				! flush writebuffers
	flushw					! flushes all but this window

	stn	%i7, [THREAD_REG + T_PC]	! save return address
	stn	%fp, [THREAD_REG + T_SP]	! save sp

	!
	! Save GSR (Graphics Status Register).
	!
	! Read fprs, call fp_save if FPRS_FEF set.
	! This handles floating-point state saving.
	! The fprs could be turned on by hw bcopy software,
	! *or* by fp_disabled. Handle it either way.
	!
	ldn	[THREAD_REG + T_LWP], %o4	! get lwp pointer
	rd	%fprs, %g4			! read fprs
	brnz,pt	%o4, 0f				! if user thread skip
	  ldn	[THREAD_REG + T_CPU], %i1	! get CPU pointer

	!
	! kernel thread
	!	
	! we save fprs at the beginning the stack so we know
	! where to check at resume time
	ldn	[THREAD_REG + T_STACK], %i2
	ldn	[THREAD_REG + T_CTX], %g3	! get ctx pointer
	andcc	%g4, FPRS_FEF, %g0		! is FPRS_FEF set
	bz,pt	%icc, 1f			! nope, skip
	  st	%g4, [%i2 + SA(MINFRAME) + FPU_FPRS]	! save fprs
	  
	! save kernel fp state in stack
	add	%i2, SA(MINFRAME), %o0		! o0 = v9_fpu ptr
	rd	%gsr, %g5
	call	fp_save
	stx	%g5, [%o0 + FPU_GSR]		! store GSR
	ba,a,pt	%icc, 1f
	  nop

0:
	! user thread
	! o4 = lwp ptr
	! g4 = fprs
	! i1 = CPU ptr
	ldn	[%o4 + LWP_FPU], %o0		! fp pointer
	stn	%fp, [THREAD_REG + T_SP]	! save sp
	andcc	%g4, FPRS_FEF, %g0		! is FPRS_FEF set
	st	%g4, [%o0 + FPU_FPRS]		! store FPRS
#ifdef DEBUG
	sethi	%hi(fpu_exists), %g5
	ld	[%g5 + %lo(fpu_exists)], %g5
	brz,pn	%g5, 1f
	  ldn	[THREAD_REG + T_CTX], %g3	! get ctx pointer
#endif
	bz,pt	%icc, 1f			! most apps don't use fp
	  ldn	[THREAD_REG + T_CTX], %g3	! get ctx pointer
	ldn	[%o4 + LWP_FPU], %o0		! fp pointer
	rd	%gsr, %g5
	call	fp_save				! doesn't touch globals
	stx	%g5, [%o0 + FPU_GSR]		! store GSR
1:
	!
	! Perform context switch callback if set.
	! This handles coprocessor state saving.
	! i1 = cpu ptr
	! g3 = ctx pointer
	!
	wr	%g0, %g0, %fprs			! disable fpu and clear fprs
	brz,pt	%g3, 2f				! skip call when zero
	ldn	[%i0 + T_PROCP], %i3		! delay slot - get proc pointer
	call	savectx
	mov	THREAD_REG, %o0			! delay - arg = thread pointer
2:
	ldn	[THREAD_REG + T_PROCP], %i2	! load old curproc - for mmu

	!
	! Temporarily switch to idle thread's stack
	!
	ldn	[%i1 + CPU_IDLE_THREAD], %o0	! idle thread pointer
	ldn	[%o0 + T_SP], %o1		! get onto idle thread stack
	sub	%o1, SA(MINFRAME), %sp		! save room for ins and locals
	clr	%fp

	!
	! Set the idle thread as the current thread
	!
	mov	THREAD_REG, %l3			! save %g7 (current thread)
	mov	%o0, THREAD_REG			! set %g7 to idle
	stn	%o0, [%i1 + CPU_THREAD]		! set CPU's thread to idle

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
	ldn	[%i3 + P_AS], %o0	! delay, load p->p_as

	!
	! Check to see if we already have context. If so then set up the
	! context. Otherwise we leave the proc in the kernels context which
	! will cause it to fault if it ever gets back to userland.
	!
	ldn	[%o0 + A_HAT], %o3	! load (p->p_as->a_hat)
	ldsh	[%o3 + SFMMU_CNUM], %o0
	brlz,a,pn %o0, 4f		! if ctx is invalid use kernel ctx
	mov	%g0, %o0
	!
	! update cpusran field
	!
	ld	[%i1 + CPU_ID], %o4
	add	%o3, SFMMU_CPUSRAN, %o5
	CPU_INDEXTOSET(%o5, %o4, %g1)
	ldn	[%o5], %o2		! o2 = cpusran field
	mov	1, %g2
	slln	%g2, %o4, %o4		! o4 = bit for this cpu
	andcc	%o4, %o2, %g0
	bnz	%icc, 4f
	  nop
3:
	or	%o2, %o4, %o1		! or in this cpu's bit mask
	casn	[%o5], %o2, %o1
	cmp	%o2, %o1
	bne,a,pn %icc, 3b
	  ldn	[%o5], %o2		! o2 = cpusran field
	membar	#StoreLoad
	!
	! Switch to different address space.
	!
4:
	rdpr	%pstate, %o5
	wrpr	%o5, PSTATE_IE, %pstate		! disable interrupts

	! read ctxnum from sfmmu struct
	ldsh	[%o3 + SFMMU_CNUM], %o0

	call	sfmmu_setctx_sec		! switch to other ctx (maybe 0)
	nop

	wrpr	%g0, %o5, %pstate		! enable interrupts
	
5:
	!
	! spin until dispatched thread's mutex has
	! been unlocked. this mutex is unlocked when
	! it becomes safe for the thread to run.
	! 
	ldstub	[%i0 + T_LOCK], %o0	! lock curthread's t_lock
6:
	brnz,pn	%o0, 7f			! lock failed
	ldn	[%i0 + T_PC], %i7	! delay - restore resuming thread's pc

	!
	! Fix CPU structure to indicate new running thread.
	! Set pointer in new thread to the CPU structure.
	! XXX - Move migration statistic out of here
	!
        ldn	[%i0 + T_CPU], %g2	! last CPU to run the new thread
        cmp     %g2, %i1		! test for migration
        be      4f			! no migration
	ldn	[%i0 + T_LWP], %o1	! delay - get associated lwp (if any)
        ld	[%i1 + CPU_SYSINFO_CPUMIGRATE], %g2 ! cpu_sysinfo.cpumigrate++
        inc     %g2
        st	%g2, [%i1 + CPU_SYSINFO_CPUMIGRATE]
	stn	%i1, [%i0 + T_CPU]	! set new thread's CPU pointer
4:
	stn	%i0, [%i1 + CPU_THREAD]	! set CPU's thread pointer
	membar	#StoreLoad		! synchronize with mutex_exit()
	mov	%i0, THREAD_REG		! update global thread register
	tst	%o1			! does new thread have an lwp?
	stn	%o1, [%i1 + CPU_LWP]	! set CPU's lwp ptr
	bz,a	1f			! if no lwp, branch and clr mpcb
	stn	%g0, [%i1 + CPU_MPCB]
	!
	! user thread
	! o1 = lwp
	! i0 = new thread
	!
	ldn	[%i0 + T_STACK], %o0
	stn	%o0, [%i1 + CPU_MPCB]	! set CPU's mpcb pointer
	! Switch to new thread's stack
	ldn	[%i0 + T_SP], %o0	! restore resuming thread's sp
	sub	%o0, SA(MINFRAME), %sp	! in case of intr or trap before restore
	mov	%o0, %fp
	!
	! Restore resuming thread's GSR reg and floating-point regs
	! Note that the ld to the gsr register ensures that the loading of
	! the floating point saved state has completed without necessity
	! of a membar #Sync.
	!
#ifdef DEBUG
	sethi	%hi(fpu_exists), %g3
	ld	[%g3 + %lo(fpu_exists)], %g3
	brz,pn	%g3, 2f
	  ldn	[%i0 + T_CTX], %i5	! should resumed thread restorectx?
#endif
	ldn	[%o1 + LWP_FPU], %o0		! fp pointer
	ld	[%o0 + FPU_FPRS], %g5		! get fpu_fprs
	andcc	%g5, FPRS_FEF, %g0		! is FPRS_FEF set?
	bz,a,pt	%icc, 9f			! no, skip fp_restore
	wr	%g0, FPRS_FEF, %fprs		! enable fprs so fp_zero works

	ldn	[THREAD_REG + T_CPU], %o4	! cpu pointer
	call	fp_restore
	wr	%g5, %g0, %fprs			! enable fpu and restore fprs

	ldx	[%o0 + FPU_GSR], %g5		! load saved GSR data
	wr	%g5, %g0, %gsr			! restore %gsr data
	ba,pt	%icc,2f
	ldn	[%i0 + T_CTX], %i5	! should resumed thread restorectx?

9:
	!
	! Zero resuming thread's fp registers, for *all* non-fp program
	! Remove all possibility of using the fp regs as a "covert channel".
	!
	call	fp_zero
	ldn	[%i0 + T_CTX], %i5	! should resumed thread restorectx?
	ba,pt	%icc, 2f
	wr	%g0, %g0, %fprs			! disable fprs

1:
	!
	! kernel thread
	! i0 = new thread
	!
	! Switch to new thread's stack
	!
	ldn	[%i0 + T_SP], %o0	! restore resuming thread's sp
	sub	%o0, SA(MINFRAME), %sp	! in case of intr or trap before restore
	mov	%o0, %fp
	!
	! Restore resuming thread's GSR reg and floating-point regs
	! Note that the ld to the gsr register ensures that the loading of
	! the floating point saved state has completed without necessity
	! of a membar #Sync.
	!
	ldn	[%i0 + T_STACK], %o0
	ld	[%o0 + SA(MINFRAME) + FPU_FPRS], %g5	! load fprs
	ldn	[%i0 + T_CTX], %i5		! should thread restorectx?
	andcc	%g5, FPRS_FEF, %g0		! did we save fp in stack?
	bz,a,pt	%icc, 2f
	  wr	%g0, %g0, %fprs			! clr fprs

	wr	%g5, %g0, %fprs			! enable fpu and restore fprs
	call	fp_restore
	add	%o0, SA(MINFRAME), %o0		! o0 = v9_fpu ptr
	ldx	[%o0 + FPU_GSR], %g5		! load saved GSR data
	wr	%g5, %g0, %gsr			! restore %gsr data

2:
	!
	! Restore resuming thread's context
	! i5 = ctx ptr
	!
	brz,a,pt %i5, 8f		! skip restorectx() when zero
	ld	[%i1 + CPU_BASE_SPL], %o0
	call	restorectx		! thread can not sleep on temp stack
	mov	THREAD_REG, %o0		! delay slot - arg = thread pointer
	!
	! Set priority as low as possible, blocking all interrupt threads
	! that may be active.
	!
	ld	[%i1 + CPU_BASE_SPL], %o0
8:
	wrpr	%o0, 0, %pil
	wrpr	%g0, WSTATE_KERN, %wstate
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
	brnz,a,pt %o0, 7b		! spin while locked
	  ldub	[%i0 + T_LOCK], %o0
	ba	%xcc, 6b
	  ldstub  [%i0 + T_LOCK], %o0	! delay - lock curthread's mutex
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
	ldn	[THREAD_REG + T_CPU], %i1	! cpu pointer
					
	flushw					! flushes all but this window
	ldn	[THREAD_REG + T_PROCP], %i2	! old procp for mmu ctx

	!
	! Temporarily switch to the idle thread's stack so that
	! the zombie thread's stack can be reclaimed by the reaper.
	!
	ldn	[%i1 + CPU_IDLE_THREAD], %o2	! idle thread pointer
	ldn	[%o2 + T_SP], %o1		! get onto idle thread stack
	sub	%o1, SA(MINFRAME), %sp		! save room for ins and locals
	clr	%fp
	!
	! Set the idle thread as the current thread.
	! Put the zombie on death-row.
	! 	
	mov	THREAD_REG, %o0			! save %g7 = curthread for arg
	mov	%o2, THREAD_REG			! set %g7 to idle
	stn	%g0, [%i1 + CPU_MPCB]		! clear mpcb
	call	reapq_add			! reapq_add(old_thread);
	stn	%o2, [%i1 + CPU_THREAD]		! delay - CPU's thread = idle

	!
	! resume_from_idle args:
	!	%i0 = new thread
	!	%i1 = cpu
	!	%i2 = old proc
	!	%i3 = new proc
	!	
	b	_resume_from_idle		! finish job of resume
	ldn	[%i0 + T_PROCP], %i3		! new process
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
					
	flushw					! flushes all but this window
	stn	%fp, [THREAD_REG + T_SP]	! delay - save sp
	stn	%i7, [THREAD_REG + T_PC]	! save return address

	ldn	[%i0 + T_PC], %i7		! restore resuming thread's pc
	ldn	[THREAD_REG + T_CPU], %i1	! cpu pointer

	!
	! Fix CPU structure to indicate new running thread.
	! The pinned thread we're resuming already has the CPU pointer set.
	!
	mov	THREAD_REG, %l3		! save old thread
	stn	%i0, [%i1 + CPU_THREAD]	! set CPU's thread pointer
	membar	#StoreLoad		! synchronize with mutex_exit()
	mov	%i0, THREAD_REG		! update global thread register
	!
	! Switch to new thread's stack
	!
	ldn	[%i0 + T_SP], %o0	! restore resuming thread's sp
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
