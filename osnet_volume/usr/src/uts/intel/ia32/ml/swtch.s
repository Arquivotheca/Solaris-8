/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)swtch.s	1.43	99/08/02 SMI"

/*
 * Process switching routines.
 */

#include <sys/asm_linkage.h>
#include <sys/asm_misc.h>
#include <sys/regset.h>
#include <sys/stack.h>

#if defined(lint) || defined(__lint)
#include <sys/thread.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/segment.h>

/*
 * resume(thread_id_t t);
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

/* Save non volatile registers (ebp, esi, edi and ebx) on the stack */
#define SAVE_REGS()\
	pushl	%ebp;\
	movl	%esp, %ebp;\
	pushl	%esi;\
	pushl	%edi;\
	pushl	%ebx;

/* Restore non volatile registers (ebp, esi, edi and ebx) from the stack */
#define RESTORE_REGS()\
	popl	%ebx;\
	popl	%edi;\
	popl	%esi;\
	popl	%ebp;


#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
resume(kthread_id_t t)
{}

#else	/* lint */

	ENTRY(resume)
	SAVE_REGS()			/ Save non volatile registers
	LOADCPU(%ebx)
	movl	8(%ebp), %edi		/ %edi = t (new thread)
	movl	CPU_THREAD(%ebx), %esi	/ %esi = curthread
	movl	%esp, T_SP(%esi)	/ Save SP for the current thread
	movl	$resume_exit, T_PC(%esi)/ Set return address as resume_exit()
					/ for the current thread
	/*
	 * Call savectx if thread has installed context ops.
	 */
	movl	T_CTX(%esi), %eax	/ should current thread savectx?
	cmpl	$0, %eax
	je	.nosavectx		/ skip call when zero
	pushl	%esi			/ arg = thread pointer
	call	savectx			/ call ctx ops
	addl	$4, %esp		/ restore stack pointer
.nosavectx:

	/ save context of floating point  if needed
	movl	T_LWP(%esi), %ecx
	movl	$FPU_EN, %edx		/ filling a slot for use later
#if	LWP_PCB_FPU != 0
	addl	$LWP_PCB_FPU, %ecx 	/ &lwp->lwp_pcb.pcb_fpu
#endif
	pushl	%ecx			/ save address for later check
	movl 	T_PROCP(%edi), %eax	/ load new proc 
	cmpl	$LWP_PCB_FPU, %ecx
	je	.disabled_fpu
	cmpl	%edx, PCB_FPU_FLAGS(%ecx)
	je	.save_fpu	/ if FPU is enabled & valid save it
.disabled_fpu:

/ We will have to fix the code above to call save ops from a linked list of
/ Contexts. In that case, we either need to ensure that the fp operations
/ Do not link in a context or that we skip the fp context here.
	/*
	 * Setup LDT register
	 */
	movl	%gs:CPU_GDT, %ecx	/ make sure gdt contains the right
					/ ldt desc
	movl	P_LDT_DESC(%eax), %edx
	movl	[P_LDT_DESC+4](%eax), %eax
	movl	%edx,LDTSEL(%ecx)
	movl	%eax,[LDTSEL+4](%ecx)
	movl	$LDTSEL, %edx
	lldt	%edx
.L2:
	/* 
	 * Temporarily switch to idle thread's stack
	 */
	movl	CPU_IDLE_THREAD(%ebx), %eax 	/ idle thread pointer
	popl	%ecx			/ restore pointer to fp structure.
	/* 
	 * Set the idle thread as the current thread
	 */
	movl	T_SP(%eax), %esp	/ It is safe to set esp
	movl	%eax, CPU_THREAD(%ebx)
	movl	%ecx, %ebx		/ save pcb_fpu pointer

	/* switch in the hat context for this thread */
	pushl	%edi
	call	hat_setup4thread
	addl	$4, %esp
	
	movl	$FPU_EN, %edx
	xorl	%ecx,%ecx
	cmpl	$LWP_PCB_FPU, %ebx
	je	.disabled_fpu2
	cmpl	%edx, PCB_FPU_FLAGS(%ebx)
	je	.wait_for_fpusave
.disabled_fpu2:
	/* 
	 * Clear and unlock previous thread's t_lock
	 * to allow it to be dispatched by another processor.
	 */
	movb	%cl, T_LOCK(%esi)

	/*
	 * IMPORTANT: Registers at this point must be:
	 *       %edi = new thread
	 *
	 * Here we are in the idle thread, have dropped the old thread.
	 */
	ALTENTRY(_resume_from_idle)
	/*
	 * spin until dispatched thread's mutex has
	 * been unlocked. this mutex is unlocked when
	 * it becomes safe for the thread to run.
	 */
.L4:
	lock
	btsl	$0, T_LOCK(%edi) / lock new thread's mutex
	jc	.L4_2			/ lock did not succeed

	/*
	 * Fix CPU structure to indicate new running thread.
	 * Set pointer in new thread to the CPU structure.
	 */
.L5:
	movl	%fs:0, %esi		/ current CPU pointer
	movl	T_STACK(%edi), %eax	/ here to use v pipeline of
					/ Pentium. Used few lines below
	cmpl	%esi, T_CPU(%edi)
	jne	.L5_2
.L5_1:
	/*
	 * Setup esp0 (kernel stack) in TSS to curthread's stack.
	 * (Note: Since we don't have saved 'regs' structure for all
	 *	  the threads we can't easily determine if we need to
	 *	  change esp0. So, we simply change the esp0 to bottom 
	 *	  of the thread stack and it will work for all cases.)
	 */
	movl	CPU_TSS(%esi), %ecx
	addl	$REGSIZE+MINFRAME, %eax	/ to the bottom of thread stack
	movl	%eax, TSS_ESP0(%ecx)

	movl	%edi, CPU_THREAD(%esi)	/ set CPU's thread pointer
	movl	$0, %ebp		/ make $<threadlist behave better
	movl	T_LWP(%edi), %eax 	/ set associated lwp to 
	movl	%eax, CPU_LWP(%esi) 	/ CPU's lwp ptr

	movl	T_SP(%edi), %esp	/ switch to outgoing thread's stack

	movl	T_PC(%edi), %esi	/ saved return address

	/*
	 * Call restorectx if context ops have been installed.
	 */
	movl	T_CTX(%edi), %eax	/ should resumed thread restorectx?
	cmpl	$0, %eax
	je	.norestorectx		/ skip call when zero
	pushl	%edi			/ arg = thread pointer
	call	restorectx		/ call ctx ops
	addl	$4, %esp		/ restore stack pointer
.norestorectx:

	/*
	 * Set priority as low as possible, blocking all interrupt
	 * threads that may be active.
	 */
	call	spl0
	jmp	*%esi			/ transfer to the resuming thread's PC

.save_fpu:
#if	PCB_FPU_REGS != 0
	addl	$PCB_FPU_REGS, %ecx
#endif
        fnsave  (%ecx)			/ save state
	jmp	.disabled_fpu

.wait_for_fpusave:
	movl	$[FPU_VALID|FPU_EN], %edx
	mov	%cr0, %eax
	movl	%edx, PCB_FPU_FLAGS(%ebx)	/ mark copy in pcb as valid
	orl	$CR0_TS, %eax			/ set to trap on next switch
	fwait			/ ensure save is done before we unlock
	movl	%eax, %cr0
	jmp	.disabled_fpu2

.L4_2:
	cmpb	$0, T_LOCK(%edi)
	je	.L4
	jmp	.L4_2

.L5_2:
	incl	CPU_SYSINFO_CPUMIGRATE(%esi) / cpu_sysinfo.cpumigrate++
	movl	%esi, T_CPU(%edi)	/ set new thread's CPU pointer
	jmp	.L5_1

	SET_SIZE(_resume_from_idle)
	SET_SIZE(resume)
#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
resume_from_zombie(kthread_id_t t)
{}

#else	/* lint */

	ENTRY(resume_from_zombie)
	SAVE_REGS()			/ Save non volatile registers
	movl	8(%ebp), %edi		/ %edi = t (new thread)
	movl	%gs:CPU_THREAD, %esi	/ %esi = curthread
	movl	%esp, T_SP(%esi)	/ Save SP for the current thread
	movl	$resume_exit, T_PC(%esi)/ Set return address as resume_exit()
					/ for the current thread
	/*
	 * Setup LDT register
	 */
	movl 	T_PROCP(%edi), %ecx	/ load new proc 
	movl	%gs:CPU_GDT, %eax	/ make sure gdt contains the right
					/ ldt desc
	movl	P_LDT_DESC(%ecx), %edx
	movl	[P_LDT_DESC+4](%ecx), %ecx
	movl	%edx,LDTSEL(%eax)
	movl	%ecx,[LDTSEL+4](%eax)
	movl	$LDTSEL, %edx
	lldt	%edx

	/clean up the fp unit. It might be left enabled
	movl	%cr0, %eax
	testl	$CR0_TS, %eax
	jnz	.zfpu_disabled		/if TS already set, nothing to do
	fninit				/initialize fpu & discard pending error
	orl	$CR0_TS, %eax
	movl	%eax, %cr0
.zfpu_disabled:
	/* 
	 * Temporarily switch to idle thread's stack so that the zombie
	 * thread's stack can be reclaimed by the reaper.
	 */
	movl	%gs:CPU_IDLE_THREAD, %eax / idle thread pointer
	movl	T_SP(%eax), %esp	/ get onto idle thread stack
	/* 
	 * Set the idle thread as the current thread.
	 */
	movl	%eax, %gs:CPU_THREAD

	/* switch in the hat context for this thread */
	pushl	%edi
	call	hat_setup4thread
	addl	$4, %esp
	/* 
	 * Put the zombie on death-row.
	 */
	pushl	%esi
	call	reapq_add
	addl	$4, %esp
	jmp	_resume_from_idle	/ finish job of resume

	SET_SIZE(resume_from_zombie)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
resume_from_intr(kthread_id_t t)
{}

#else	/* lint */

	ENTRY(resume_from_intr)
	SAVE_REGS()			/ Save non volatile registers
	movl	8(%ebp), %edi		/ %edi = t (new thread)
	movl	%gs:CPU_THREAD, %esi	/ %esi = curthread
	movl	%esp, T_SP(%esi)	/ Save SP for the current thread
	movl	$resume_exit, T_PC(%esi)/ Set return address as resume_exit()
					/ for the current thread

	movl	%edi, %gs:CPU_THREAD	/ set CPU's thread pointer
	movl	T_SP(%edi), %esp	/ restore resuming thread's sp
	movl	$0, %ebp		/ make $<threadlist behave better

	/* 
	 * unlock outgoing thread's mutex dispatched by another processor.
	 */
	xorl	%eax,%eax
	xchgb	%al, T_LOCK(%esi)

	/* switch in the hat context if the blocked thread had a special one */
	cmpl	$0, T_MMUCTX(%esi)	/ should be safe to check here after
	je	.nommuctx		/ dropping lock as ITHR cannot migrate.
	pushl	%edi
	call	hat_load_mmuctx
	addl	$4, %esp
.nommuctx:
	/*
	 * Set priority as low as possible, blocking all interrupt
	 * threads that may be active.
	 */
	movl	T_PC(%edi), %esi	/ saved return address
	call	spl0
	jmp	*%esi			/ transfer to the resuming thread's PC

	SET_SIZE(resume_from_intr)

#endif /* lint */

/*
 * resume_exit():
 *	Restore the stack frame (saved by resume()) to resume the execution
 *	of the thread and to return to the caller of the resume() (typically
 *	swtch()).
 *	The top of the system stack at entry to this routine would look like
 *		0(%esp) - 12(%esp)	Saved non volatile registers
 *		16(%esp)		Return PC from resume()
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
resume_exit()
{}

#else	/* lint */

	ENTRY(resume_exit)
	RESTORE_REGS()			/ restore non-volatile registers
	ret
	SET_SIZE(resume_exit)

#endif	/* lint */

