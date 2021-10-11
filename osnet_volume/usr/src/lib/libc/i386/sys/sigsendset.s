	.ident	"@(#)sigsendset.s	1.8	98/07/08 SMI"

	.file	"sigsendset.s"

	.globl	__cerror

_fwdef_(`sigsendset'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SIGSENDSET,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
	_fw_setsize_(`sigsendset')
