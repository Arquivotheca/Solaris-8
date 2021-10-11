/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)boot_elf.s	1.13	99/05/27 SMI"

#if	defined(lint)

#include	<sys/types.h>
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"

/* ARGSUSED0 */
int
elf_plt_trace()
{
	return (0);
}
#else

#include	<link.h>
#include	"_audit.h"

	.file	"boot_elf.s"
	.text

/*
 * On entry the 'glue code' has already initialized
 * %eax == dyndata.
 *
 * dyndata contains the following:
 *
 *	dyndata+0x0	uintptr_t	reflmp
 *	dyndata+0x4	uintptr_t	deflmp
 *	dyndata+0x8	ulong_t		symndx
 *	dyndata+0xc	ulont_t		sb_flags
 *	dyndata+0x10	Elf32_Sym	symdef
 */
	.globl	elf_plt_trace
	.type	elf_plt_trace,@function
	.align 16
elf_plt_trace:
	pushl	%ebp
	movl	%esp,%ebp
	subl	$88,%esp			/ create some local storage
	pushl	%ebx
	pushl	%edi
	pushl	%esi
	call	.L1				/ initialize %ebx to GOT
.L1:
	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-.L1], %ebx
	/*
	 * Local stack space storage is allocated as follows:
	 *
	 *	-4(%ebp)	store dyndata ptr
	 *	-8(%ebp)	store call destination
	 *	-84(%ebp)	space for gregset
	 *	-88(%ebp)	prev stack size
	 */

	movl	%eax,-4(%ebp)			/ -4(%ebp) == dyndata
	movl	-4(%ebp), %edi			/ %edi = dyndata
	testb	$LA_SYMB_NOPLTENTER, 0xc(%eax)	/ link.h
	je	.start_pltenter
	movl	0x14(%eax), %edi
	movl	%edi, -8(%ebp)			/ save destination address
	jmp	.end_pltenter

.start_pltenter:
	/*
	 * save all registers into gregset_t
	 */
	lea	4(%ebp), %edi
	movl	%edi, -84(%ebp)		/ %esp
	movl	0(%ebp), %edi
	movl	%edi, -80(%ebp)		/ %ebp
	/*
	 * trapno, err, eip, cs, efl, uesp, ss
	 */
	movl	-4(%ebp), %edi
	lea	12(%edi), %eax
	pushl	%eax				/ arg5 (&sb_flags)
	lea	-84(%ebp), %eax
	pushl	%eax				/ arg4 (regset)
	pushl	8(%edi)				/ arg3 (symndx)
	lea	16(%edi), %eax
	pushl	%eax				/ arg2 (&sym)
	pushl	4(%edi)				/ arg1 (dlmp)
	pushl	0(%edi)				/ arg0 (rlmp)
	call	audit_pltenter@PLT
	addl	$24, %esp			/ cleanup stack
	movl	%eax, -8(%ebp)			/ save calling address
.end_pltenter:

	/*
	 * If *no* la_pltexit() routines exist
	 * we do not need to keep the stack frame
	 * before we call the actual routine.  Instead we
	 * jump to it and remove our stack from the stack
	 * at the same time.
	 */
	movl	audit_flags@GOT(%ebx), %eax
	movl	(%eax), %eax
	andl	$AF_PLTEXIT, %eax		/ value of audit.h:AF_PLTEXIT
	cmpl	$0, %eax
	je	.bypass_pltexit
	/*
	 * Has the *nopltexit* flag been set for this entry point
	 */
	testb	$LA_SYMB_NOPLTEXIT, 12(%edi)
	je	.start_pltexit

.bypass_pltexit:
	/*
	 * No PLTEXIT processing required.
	 */
	movl	-8(%ebp), %eax			/ eax == calling destination
	popl	%esi				/
	popl	%edi				/    clean up stack
	popl	%ebx				/
	movl	%ebp, %esp			/
	popl	%ebp				/
	jmp	*%eax				/    jmp (*to)()

.start_pltexit:

	/*
	 * In order to call the destination procedure and then return
	 * to audit_pltexit() for post analysis we must first grow
	 * our stack frame and then duplicate the original callers
	 * stack state.  This duplicates all of the arguements
	 * that were to be passed to the distination procedure.
	 */
	movl	%ebp, %edi			/
	addl	$8, %edi			/    %edi = src
	movl	(%ebp), %edx			/
	subl	%edi, %edx			/    %edx == prev frame sz
	/*
	 * If audit_argcnt > 0 then we limit the number of
	 * arguements that will be duplicated to audit_argcnt.
	 *
	 * If (prev_stack_size > (audit_argcnt * 4))
	 *	prev_stack_size = audit_argcnt * 4;
	 */
	movl	audit_argcnt@GOT(%ebx),%eax
	movl	(%eax), %eax			/    %eax = audit_argcnt
	cmpl	$0, %eax
	jle	.grow_stack
	lea	(,%eax,4), %eax			/    %eax = %eax * 4
	cmpl	%eax,%edx
	jle	.grow_stack
	movl	%eax, %edx
	/*
	 * Grow the stack and duplicate the arguements of the
	 * original caller.
	 */
.grow_stack:
	subl	%edx, %esp			/    grow the stack 
	movl	%edx, -88(%ebp)			/    -88(%ebp) == prev frame sz
	movl	%esp, %ecx			/    %ecx = dest
	addl	%ecx, %edx			/    %edx == tail of dest
.while_base:
	cmpl	%edx, %ecx			/   while (base+size >= src++) {
	jge	.end_while				/
	movl	(%edi), %esi
	movl	%esi,(%ecx)			/        *dest = *src
	addl	$4, %edi			/	 src++
	addl	$4, %ecx			/        dest++
	jmp	.while_base			/    }

	/*
	 * The above stack is now an exact duplicate of
	 * the stack of the original calling procedure.
	 */
.end_while:
	movl	-8(%ebp), %eax
	call	*%eax				/  call dest_proc()
	addl	-88(%ebp), %esp			/  cleanup dupped stack

	movl	-4(%ebp), %edi
	pushl	8(%edi)				/ arg4 (symndx)
	lea	16(%edi), %ecx
	pushl	%ecx				/ arg3 (symp)
	pushl	4(%edi)				/ arg2 (dlmp)
	pushl	0(%edi)				/ arg1 (rlmp)
	pushl	%eax				/ arg0 (retval)
	call	audit_pltexit@PLT
	addl	$20, %esp
	
	/*
	 * Clean up after ourselves and return to the
	 * original calling procedure.
	 */
	popl	%esi				/
	popl	%edi				/ clean up stack
	popl	%ebx				/
	movl	%ebp, %esp			/
	popl	%ebp				/
	ret					/ return to caller
	.size	elf_plt_trace, .-elf_plt_trace
#endif

/*
 * We got here because a call to a function resolved to a procedure
 * linkage table entry.  That entry did a JMPL to the first PLT entry, which
 * in turn did a call to elf_rtbndr.
 *
 * the code sequence that got us here was:
 *
 * PLT entry for foo:
 *	jmp	*name1@GOT(%ebx)
 *	pushl	$rel.plt.foo
 *	jmp	PLT0
 *
 * 1st PLT entry (PLT0):
 *	pushl	4(%ebx)
 *	jmp	*8(%ebx)
 *	nop; nop; nop;nop;
 *
 */
#if defined(lint)

extern unsigned long	elf_bndr(Rt_map *, unsigned long, caddr_t);

void
elf_rtbndr(Rt_map * lmp, unsigned long reloc, caddr_t pc)
{
	(void) elf_bndr(lmp, reloc, pc);
}

#else
	.globl	elf_bndr
	.globl	elf_rtbndr
	.weak	_elf_rtbndr
	_elf_rtbndr = elf_rtbndr	/ Make dbx happy
	.type   elf_rtbndr,@function
	.align	4

elf_rtbndr:
	call	elf_bndr@PLT		/ call the C binder code
	addl	$8,%esp			/ pop args
	jmp	*%eax			/ invoke resolved function
	.size 	elf_rtbndr, .-elf_rtbndr
#endif
