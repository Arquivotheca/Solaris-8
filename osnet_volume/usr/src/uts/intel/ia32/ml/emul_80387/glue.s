/*
 * Copyright (c) 1993, 1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)glue.s	1.9	99/05/04 SMI"

#if !defined(lint)

#include <sys/psw.h>
#include <sys/pcb.h>
#include "assym.h"
#include "e80387.h"

	.bss
	.comm	fsysflag, 4, 4
	.globl	kernelbase

	.text
/
/	Floating point emulator preambles for standard page fault handler.
/	Emulator fast system calls set fsysflag to a return address before
/	accessing locations that could cause page faults.  These preambles
/	jump to the standard handlers if fsysflag is not set.  If fsysflag
/	is set, they remove the error code from the stack, replace the
/	return address by the value of fsysflag, and return from the trap.
/	e80387_pgflt1 is the preamble for the standard idt and e80387_pgflt2
/	is for the VP/ix idt2.
/

#endif	/* lint */

#if defined(lint)
#include <sys/reg.h>		/* contains definition of fpchip_state */

void
e80387_pgflt1(void)
{
	extern int e80387_saved_pfdest1;

	e80387_saved_pfdest1 = 0;
}

#else	/* lint */

	.globl	e80387_pgflt1
e80387_pgflt1:
	cmpl	$0, %cs:fsysflag
	jne	catchit
	jmp	*%cs:e80387_saved_pfdest1
catchit:
	addl	$8, %esp		/ pop error code and return address
	pushl	%cs:fsysflag		/ push new return address
	iret

/	Emulator fast system call routine for getting FP context.
/	Caller supplies stack-relative context destination address in %ebx.
/	Clears carry and zero flags for success, sets zero flag for page
/	fault, sets carry flag for other errors.

	.globl	e80387_getfp

#endif	/* lint */

#if defined(lint)

void
e80387_getfp(void)
{
	extern struct fpchip_state fpinit_result;

	fpinit_result.state[0] = 0;
}

#else	/* lint */

e80387_getfp:
	push	%ds
	push	%es
	push	%fs
	push	%gs
#define	G_CS_OFFSET 20
#define	G_FL_OFFSET 24
#define	G_SS_OFFSET 32
	movw	$KDSSEL, %ax
	movw	%ax, %ds
	movw	%ax, %es

/	We do not use %fs here, but kernel interrupt entry code assumes
/	that %fs is set up if %gs contains KGSSEL.  This routine runs
/	with interrupts disabled, so normal use does not involve kernel
/	entry but we set up %fs to allow using the kernel debugger
/	while testing.
/
	movw	$KFSSEL, %ax
	movw	%ax, %fs
	movw	$KGSSEL, %ax
	movw	%ax, %gs
	movl	%gs:CPU_LWP, %eax		/ lwp = CPU->cpu_lwp

/	Clear carry and zero bits in pushed flags
/
	andw	$0xFFFF - PS_C - PS_Z, G_FL_OFFSET(%esp)

/	If there is no current LWP, just copy fpinit_result
/	directly to the emulator stack.  This situation can
/	occur when cleaning up the FPU while exiting a thread.
/	The kernel executes an fninit instruction to clear any
/	pending errors so that they cannot affect other threads.
/	It would ought to be safe just to do nothing in this case,
/	but returning the fpinit_result is more flexible if the
/	kernel is changed later.
/
	movl	$fpinit_result, %esi
	cmpl	$0, %eax
	jz	no_lwp

/	Initialize FPU image from stored copy if not already done.
/
	testl	$FPU_EN, LWP_FPU_FLAGS(%eax)
	jnz	initialized
	leal	LWP_FPU_CHIP_STATE(%eax), %edi
	movl	$FPCTXSIZE, %ecx
	repz
	smovl
	movl	$FPU_EN+FPU_VALID, LWP_FPU_FLAGS(%eax)
initialized:
	leal	LWP_FPU_CHIP_STATE(%eax), %esi
no_lwp:

/	If we entered from level 0, caller is the emulator trying
/	to emulate a kernel FP instruction.  We know that the stack
/	segment has 0 base and that the argument is valid.
/
	testb	$3, G_CS_OFFSET(%esp)	/ %cs pushed during interrupt
	jz	get_kern

/	If we entered with %ss as USER_DS, we know that the stack segment
/	has 0 base.  We can skip the base calculation but must still
/	check the validity of the argument.
	cmpw	$USER_DS, G_SS_OFFSET(%esp) / %ss pushed during interrupt
	je	get_comp

/	Reject the call if the stack segment was not in the LDT or was
/	not a valid segment (saves checking the LDT limit explicitly).
/
	testw	$4, G_SS_OFFSET(%esp)
	jz	get_error
	lar	G_SS_OFFSET(%esp), %eax		/ validate user %ss
	jnz	get_error

/	Find the process-specific LDT for the current LWP.  Error if
/	none.
/
	movl	%gs:CPU_LWP, %edi		/ edi = current LWP
	movl	LWP_PROCP(%edi), %edi		/ edi = proc for LWP
	movl	P_LDT(%edi), %edi		/ edi = LDT for proc
	orl	%edi, %edi			/ make sure non-zero
	jz	get_error

/	Extract the base address from the LDT descriptor and add it
/	to the user argument.
/
	movw	G_SS_OFFSET(%esp), %ax		/ get user %ss
	andl	$0xFFF8, %eax			/ convert to LDT offset
	addl	%eax, %edi			/ edi = > LDT entry
	movl	4(%edi), %eax			/ get base high byte
	andl	$0xFF000000, %eax		/ isolate it
	addl	%eax, %ebx			/ add to user arg
	movl	2(%edi), %eax			/ get base remainder
	andl	$0xFFFFFF, %eax			/ isolate it
	addl	%eax, %ebx			/ add to user arg

/	Make sure the argument is within the user portion of virtual
/	address space.
get_comp:
	cmpl	kernelbase, %ebx
	jae	get_error

/	Protect against page faults.  Do the copy. Indicate success.
/	Clean up page fault flag.  Return to caller.
get_kern:
	movl	%ebx, %edi
	movl	$FPCTXSIZE, %ecx
	movl	$get_fault, fsysflag		/ catch page faults
	repz
	smovl					/ page fault possible here
get_uncatch:
	movl	$0, fsysflag			/ stop catching page faults
get_done:
	pop	%gs
	pop	%fs
	pop	%es
	pop	%ds
	iret

/	Page fault during context copy causes jump to here.  Indicate
/	failure due to page fault and join common return path.
get_fault:
	orw	$PS_Z, G_FL_OFFSET(%esp)
	jmp	get_uncatch

get_error:
	orw	$PS_C, G_FL_OFFSET(%esp)
	jmp	get_done


/	Emulator fast system call routine for setting FP context.
/	Caller supplies stack-relative context source address in %ebx.
/	Clears carry and zero flags for success, sets zero flag for page
/	fault, sets carry flag for other errors.
/
/	Need to check that given address is below kernelbase if caller
/	is user mode to prevent someone using this feature to copy pieces
/	of kernel data into their FP context and then retrieve them via
/	e80387_getfp.
/
	.globl	e80387_setfp

#endif	/* lint */

#if defined(lint)

void
e80387_setfp(void)
{}

#else	/* lint */

e80387_setfp:
	push	%ds
	push	%es
	push	%fs
	push	%gs
#define	S_CS_OFFSET 20
#define	S_FL_OFFSET 24
#define	S_SS_OFFSET 32
	movw	$KDSSEL, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	$KFSSEL, %ax
	movw	%ax, %fs
	movw	$KGSSEL, %ax
	movw	%ax, %gs
	movl	%gs:CPU_LWP, %eax		/ lwp = CPU->cpu_lwp

/	Clear carry and zero bits in pushed flags
/
	andw	$0xFFFF - PS_C - PS_Z, S_FL_OFFSET(%esp)

/	If there is no current LWP, just discard the FPU context
/	because there is nowhere to put it.  See equivalent comments
/	in e80387_getfp above.
/
	cmpl	$0, %eax
	jz	set_done

	leal	LWP_FPU_CHIP_STATE(%eax), %edi

/	If we entered from level 0, caller is the emulator trying
/	to emulate a kernel FP instruction.  We know that the stack
/	segment has 0 base and that the argument is valid.
/
	testb	$3, S_CS_OFFSET(%esp)	/ %cs pushed during interrupt
	jz	set_kern

/	If we entered with %ss as USER_DS, we know that the stack segment
/	has 0 base.  We can skip the base calculation but must still
/	check the validity of the argument.
	cmpw	$USER_DS, S_SS_OFFSET(%esp) / %ss pushed during interrupt
	je	set_comp

/	Reject the call if the stack segment was not in the LDT or was
/	not a valid segment (saves checking the LDT limit explicitly).
/
	testw	$4, S_SS_OFFSET(%esp)
	jz	set_error
	lar	S_SS_OFFSET(%esp), %eax		/ validate user %ss
	jnz	set_error

/	Find the process-specific LDT for the current LWP.  Error if
/	none.
/
	movl	%gs:CPU_LWP, %esi		/ esi = current LWP
	movl	LWP_PROCP(%esi), %esi		/ esi = proc for LWP
	movl	P_LDT(%esi), %esi		/ esi = LDT for proc
	orl	%esi, %esi			/ make sure non-zero
	jz	set_error

/	Extract the base address from the LDT descriptor and add it
/	to the user argument.
/
	movw	S_SS_OFFSET(%esp), %ax		/ get user %ss
	andl	$0xFFF8, %eax			/ convert to LDT offset
	addl	%eax, %esi			/ esi = > LDT entry
	movl	4(%esi), %eax			/ get base high byte
	andl	$0xFF000000, %eax		/ isolate it
	addl	%eax, %ebx			/ add to user arg
	movl	2(%esi), %eax			/ get base remainder
	andl	$0xFFFFFF, %eax			/ isolate it
	addl	%eax, %ebx			/ add to user arg
set_comp:
	cmpl	kernelbase, %ebx		/ prevent copy from bad address
	jae	set_error
set_kern:
	movl	%ebx, %esi
	movl	$FPCTXSIZE, %ecx
	movl	$set_fault, fsysflag		/ catch page faults
	repz
	smovl					/ page fault possible here
set_uncatch:
	movl	$0, fsysflag			/ stop catching page faults
set_done:
	pop	%gs
	popl	%fs
	pop	%es
	pop	%ds
	iret

set_fault:
	orw	$PS_Z, S_FL_OFFSET(%esp)
	jmp	set_uncatch

set_error:
	orw	$PS_C, S_FL_OFFSET(%esp)
	jmp	set_done

#endif	/* lint */
