	.ident	"@(#)ioctl.s	1.9	98/07/08 SMI"

	.file	"ioctl.s"

	.text

	.globl	__cerror

_fwdef_(`ioctl'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$IOCTL,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je 	ioctl	
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror:
	ret
	_fw_setsize_(`ioctl')
