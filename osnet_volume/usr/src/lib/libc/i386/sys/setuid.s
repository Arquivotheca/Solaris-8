	.ident	"@(#)setuid.s	1.8	98/07/08 SMI"

	.file	"setuid.s"

	.text

	.globl	__cerror

_fwdef_(`setuid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SETUID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	xorl	%eax,%eax
	ret
	_fw_setsize_(`setuid')
