/
/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.
/
	.ident	"@(#)wschr.s	1.1	98/08/20 SMI"

	.file	"wschr.s"
/
/ Wide character wcschr() implementation
/
/ Algorithm based on Solaris 2.6 gen/strchr.s implementation
/
/	.ident	"@(#)strchr.s	1.1	92/04/17 SMI"
/

	.align	8		/ accounts for .loop alignment and prolog

_fwdef_(wcschr):
_m4_ifdef_(`DSHLIB',
`',
`_fwdef_(wschr):'
)
	MCOUNT			/ profiling
	movl	4(%esp),%eax	/ %eax = string address
	movl	8(%esp),%ecx	/ %ecx = wchar sought
.loop:
	movl	(%eax),%edx	/ %edx = wchar of string
	cmpl	%ecx,%edx	/ find it?
	je	.found		/ yes
	testl	%edx,%edx	/ is it null?
	je	.notfound

	movl	4(%eax),%edx	/ %edx = wchar of string
	cmpl	%ecx,%edx	/ find it?
	je	.found1		/ yes
	testl	%edx,%edx	/ is it null?
	je	.notfound

	movl	8(%eax),%edx	/ %edx = wchar of string
	cmpl	%ecx,%edx	/ find it?
	je	.found2		/ yes
	testl	%edx,%edx	/ is it null?
	je	.notfound

	movl	12(%eax),%edx	/ %edx = wchar of string
	cmpl	%ecx,%edx	/ find it?
	je	.found3		/ yes
	addl	$16,%eax
	testl	%edx,%edx	/ is it null?
	jne	.loop

.notfound:
	xorl	%eax,%eax	/ %eax = NULL
	ret

.found3:
	addl	$12,%eax
	ret
.found2:
	addl	$8,%eax
	ret
.found1:
	addl	$4,%eax
.found:
	ret

_m4_ifdef_(`DSHLIB',
`
	.align	4
_fwdef_(wschr):
	_prologue_
	movl	_esp_(8),%eax
	movl	_esp_(4),%edx
	pushl	%eax
	pushl	%edx
	call	_fref_(_wcschr)
	addl	$8,%esp
	_epilogue_
	ret
	_fg_setsize_(`wschr')
'
)
