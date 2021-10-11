	.ident	"@(#)lseek.s	1.13	98/07/08 SMI"

	.file	"lseek.s"

	.text

	.globl  __cerror

	.globl	__cerror64

_m4_ifdef_(`_LARGEFILE_INTERFACE',
`_fwdef_(`lseek64'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$LLSEEK,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror64
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror64)
noerror64:
	ret
	_fw_setsize_(`lseek64')',
`_fwdef_(`lseek'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$LSEEK,%eax
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
	_fw_setsize_(`lseek')'
)
