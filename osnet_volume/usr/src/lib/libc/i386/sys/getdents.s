	.ident	"@(#)getdents.s	1.10	98/07/08 SMI"

	.file	"getdents.s"

	.text

	.globl	__cerror

_m4_ifdef_(`_LARGEFILE_INTERFACE',
`_fwdef_(`getdents64'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETDENTS64,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror64
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror64:
	ret
	_fw_setsize_(`getdents64')',
`_fwdef_(`getdents'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETDENTS,%eax
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
	_fw_setsize_(`getdents')'
)

