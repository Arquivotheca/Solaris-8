	.file	"abs.s"

	.ident	"@(#)abs.s	1.2	98/07/08 SMI"

/	/* Assembler program to implement the following C program */
/	int
/	abs(arg)
/	int	arg;
/	{
/		return((arg < 0)? -arg: arg);
/	}


	.text
	.globl	abs
	.globl	labs
	.align	4

_fgdef_(abs):
_fgdef_(labs):
	MCOUNT			/ subroutine entry counter if profiling

	movl	4(%esp),%eax	/ arg < 0?
	testl	%eax,%eax
	jns	.posit
	negl	%eax		/ yes, return -arg
.posit:
	ret
	_fg_setsize_(`abs')
	_fg_setsize_(`labs')
