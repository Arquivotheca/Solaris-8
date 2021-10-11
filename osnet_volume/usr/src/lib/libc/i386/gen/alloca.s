	.file	"alloca.s"

	.ident	"@(#)alloca.s	1.5	98/07/08 SMI"

	.text
	.globl	__builtin_alloca
	.align	4

_fgdef_(__builtin_alloca):
	MCOUNT
	popl	%ecx			/ grab our return address
	movl	(%esp),%eax		/ get argument
	addl	$3,%eax
	andl	$0xfffffffc,%eax	/ round up to multiple of 4
	subl	%eax,%esp		/ leave requested space on stack
	leal	4(%esp),%eax		/ adjust, accounting for the "size" arg
	pushl	%ecx			/ put back return address
	ret
	_fg_setsize_(`__builtin_alloca')
