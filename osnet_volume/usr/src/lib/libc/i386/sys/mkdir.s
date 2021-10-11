	.ident	"@(#)mkdir.s	1.8	98/07/08 SMI"

	.file	"mkdir.s"

	.text

	.globl	__cerror

_fwdef_(`mkdir'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$MKDIR,%eax
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
	_fw_setsize_(`mkdir')
