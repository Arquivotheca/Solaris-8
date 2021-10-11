/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)resume.s	1.8	97/01/15	SMI"

#include <sys/asm_linkage.h>
#include <sys/trap.h>
#include <assym.s>
#include <pic.h>


/*
 *	_resume(t, tmpstack, dontsave)
 */
	ENTRY(_resume)
	movl	%gs:0, %eax
	cmpb	$TS_ZOMB, T_STATE(%eax)
	je	.dontswitch
	movl	T_FLAG(%eax), %ecx
	andl	$T_IDLETHREAD, %ecx
	jne	.dontsave
	cmpl	$0, 0xc(%esp)
	jne	.dontsave

	/
	/ Save caller's non-volatile state
	/
	movl	%ebp, T_BP(%eax)		/ save caller's %ebp
	movl	%esp, T_SP(%eax)
	leal	4(%esp), %ecx
	movl	%ecx, T_SP(%eax)
	movl	(%esp), %ecx
	movl	%ecx, T_PC(%eax)
	movl	%esi, T_ESI(%eax)
	movl	%edi, T_EDI(%eax)
	movl	%ebx, T_EBX(%eax)

	/ 
	/ Save the FPU state
	/
	fwait
	leal	T_FPENV(%eax), %ecx
	fnstenv  (%ecx)

.dontsave:

	/
	/ Change to temporary stack...
	/
	movl	8(%esp), %edi		/ temp stack
	subl	$8, %edi		/ alloc stack space
	movl	4(%esp), %ecx		/ copy arg1
	movl	%ecx, 4(%edi)
	movl	$0, (%edi)		/ fake ret address

	movl	%edi, %esp		/ On new stack!
	leal	T_LOCK(%eax), %ecx
	pushl	%ecx
	call	fcnref(_lwp_mutex_unlock) / Cheat, use _swtch's %ebx
	addl	$4, %esp
	movl	$0, %esi			/ arg to _resume_ret below
	jmp	.cont
.dontswitch:
	movl	%eax, %esi		/ curthread is arg
.cont:
	movl	4(%esp), %eax		/ %eax is new thread...
	leal	T_LOCK(%eax), %ecx
	pushl	%ecx
	call	fcnref(_lwp_mutex_lock)	/ lock new thread
	movl	8(%esp), %eax		/ don't need this stack
					/ anymore so just leave arg...

	/ 
	/ Restore the FPU state
	/
	leal    T_FPENV(%eax), %ecx
	fldenv	(%ecx)

	/
	/ Jump to new thread!
	/
	movl	%eax, %gs:0		/ make thread curthread
	movl	T_SP(%eax), %esp	/ switch to thread's stack
	movl	T_BP(%eax), %ebp

	pushl	T_PC(%eax)		/ push return PC on stack
	pushl	%esi			/ arg to _resume_ret

	movl	T_ESI(%eax), %esi	/ restore regs
	movl	T_EDI(%eax), %edi
	movl	T_EBX(%eax), %ebx	/ pointless...

	unsafe_pic_prolog(.res1)	/ need %ebx when we're
					/ resuming to _thread_start
	call	fcnref(_resume_ret)
	addl	$4, %esp			/ dump arg
	ret				/ returns to thread...
	SET_SIZE(_resume)
