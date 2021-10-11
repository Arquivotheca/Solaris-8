/
/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)wsncmp.s	1.1	98/09/09 SMI"

	.file	"wsncmp.s"

/
/ Wide character wcsncpy() implementation
/
/ Algorithm based on Solaris 2.6 gen/strncpy.s implementation
/
/	.ident	"@(#)strncpy.s	1.1	92/04/17 SMI"
/
/

_fwdef_(wcsncmp):
_m4_ifdef_(`DSHLIB',
`',
`_fwdef_(wsncmp):'
)
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
	movl	(%esi),%eax	/ slodb ; scab
	cmpl	(%edi),%eax
	jne	.notequal_0	/ Are the bytes equal?
	testl	%eax,%eax
	je	.equal		/ End of string?

	decl	%ecx
	je	.equal		/ Used all n chars?
	movl	4(%esi),%eax	/ slodb ; scab
	cmpl	4(%edi),%eax
	jne	.notequal_1	/ Are the bytes equal?
	testl	%eax,%eax
	je	.equal		/ End of string?

	decl	%ecx
	je	.equal		/ Used all n chars?
	movl	8(%esi),%eax	/ slodb ; scab
	cmpl	8(%edi),%eax
	jne	.notequal_2	/ Are the bytes equal?
	testl	%eax,%eax
	je	.equal		/ End of string?

	decl	%ecx
	je	.equal		/ Used all n chars?
	movl	12(%esi),%eax	/ slodb ; scab
	cmpl	12(%edi),%eax
	jne	.notequal_3	/ Are the bytes equal?
	addl	$16,%esi
	addl	$16,%edi
	testl	%eax,%eax
	jne	.loop		/ End of string?

.equal:
	popl	%esi		/ restore registers
	xorl	%eax,%eax	/ return 0
	movl	%edx,%edi
	ret

	.align	4
.notequal_3:
	addl	$4,%edi
.notequal_2:
	addl	$4,%edi
.notequal_1:
	addl	$4,%edi
.notequal_0:
	popl	%esi		/ restore registers
	subl	(%edi),%eax	/ return value is (*s1 - *--s2)
	movl	%edx,%edi
	ret

_m4_ifdef_(`DSHLIB',
`
	.align	4
_fwdef_(wsncmp):
	_prologue_
	movl	_esp_(12),%ecx
	movl	_esp_(8),%eax
	movl	_esp_(4),%edx
	pushl	%ecx
	pushl	%eax
	pushl	%edx
	call	_fref_(_wcsncmp)
	addl	$12,%esp
	_epilogue_
	ret
	_fg_setsize_(`wsncmp')
'
)
