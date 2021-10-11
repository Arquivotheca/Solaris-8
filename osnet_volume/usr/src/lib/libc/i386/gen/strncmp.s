/
/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)strncmp.s	1.3	98/10/09 SMI"

	.file	"strncmp.s"

	.globl	strncmp
	.align	4

_fgdef_(strncmp):
	MCOUNT
	pushl	%esi		/ save register variables
	movl	8(%esp),%esi	/ %esi = first string
	movl	%edi,%edx
	movl	12(%esp),%edi	/ %edi = second string
	cmpl	%esi,%edi	/ same string?
	je	.equal
	movl	16(%esp),%ecx	/ %ecx = length
	incl	%ecx		/ will later predecrement this uint
.loop:
	decl	%ecx
	je	.equal		/ Used all n chars?
	movb	(%esi),%al	/ slodb ; scab
	cmpb	(%edi),%al
	jne	.notequal_0	/ Are the bytes equal?
	testb	%al,%al
	je	.equal		/ End of string?

	decl	%ecx
	je	.equal		/ Used all n chars?
	movb	1(%esi),%al	/ slodb ; scab
	cmpb	1(%edi),%al
	jne	.notequal_1	/ Are the bytes equal?
	testb	%al,%al
	je	.equal		/ End of string?

	decl	%ecx
	je	.equal		/ Used all n chars?
	movb	2(%esi),%al	/ slodb ; scab
	cmpb	2(%edi),%al
	jne	.notequal_2	/ Are the bytes equal?
	testb	%al,%al
	je	.equal		/ End of string?

	decl	%ecx
	je	.equal		/ Used all n chars?
	movb	3(%esi),%al	/ slodb ; scab
	cmpb	3(%edi),%al
	jne	.notequal_3	/ Are the bytes equal?
	addl	$4,%esi
	addl	$4,%edi
	testb	%al,%al
	jne	.loop		/ End of string?

.equal:
	popl	%esi		/ restore registers
	xorl	%eax,%eax	/ return 0
	movl	%edx,%edi
	ret

	.align	4
.notequal_3:
	incl	%edi
.notequal_2:
	incl	%edi
.notequal_1:
	incl	%edi
.notequal_0:
	popl	%esi		/ restore registers
	clc			/ clear carry bit
	subb	(%edi),%al	
	movl	%edx,%edi
	movl	$-1, %eax	/ possibly wasted instr
	jc	.neg		/ did we overflow in the sub
	movl	$1, %eax	/ if not a > b
.neg:
	ret
	_fg_setsize_(`strncmp')
