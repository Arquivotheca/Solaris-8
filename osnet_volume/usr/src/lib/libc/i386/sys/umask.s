	.ident	"@(#)umask.s	1.8	98/07/08 SMI"


	.file	"umask.s"

	.text

	.globl	__cerror

_fwdef_(`umask'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$UMASK,%eax
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
	_fw_setsize_(`umask')
