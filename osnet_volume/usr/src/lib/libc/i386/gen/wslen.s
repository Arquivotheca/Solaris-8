/
/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.
/
	.ident	"@(#)wslen.s	1.1	98/09/09 SMI"
	.file	"wslen.s"
/
/ Wide character wcslen() implementation
/
/ Algorithm based on SUN gen/strlen.s implementation
/
/	.ident	"@(#)strlen.s	1.1	92/04/17 SMI"
/
/

	.align	4
_fwdef_(wcslen):
_m4_ifdef_(`DSHLIB',
`',
`_fwdef_(wslen):'
)
	MCOUNT

	movl	4(%esp),%edx	/ string address
	xorl	%eax,%eax	/ %al = 0

.top:
	cmpl	$0, (%edx)
	je	.out
	incl	%eax
	addl	$4, %edx
	jmp	.top
.out:
	ret

_m4_ifdef_(`DSHLIB',
`
	.align	4
_fwdef_(wslen):
	_prologue_
	movl	_esp_(8),%eax
	movl	_esp_(4),%edx
	pushl	%eax
	pushl	%edx
	call	_fref_(_wcslen)
	addl	$8,%esp
	_epilogue_
	ret
	_fg_setsize_(`wslen')
'
)
