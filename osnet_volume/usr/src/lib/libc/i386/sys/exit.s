.ident	"@(#)exit.s	1.4	98/07/08 SMI"

	.file	"exit.s"

	.text

	.globl	_exit

_fgdef_(_exit):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$EXIT,%eax
	lcall	$0x7,$0
	_fg_setsize_(`_exit')
