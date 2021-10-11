	.ident	"@(#)lstat.s	1.11	98/07/08 SMI"

	.file	"lstat.s"

	.text

	.globl  __cerror

	.align 4

_m4_ifdef_(`_LARGEFILE_INTERFACE',
`_fwdef_(`lstat64'):
	MCOUNT
	movl	$LSTAT64,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror64
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror64:
	xorl	%eax,%eax
	ret
	_fw_setsize_(`lstat64')',
`_fwdef_(`lstat'):
	MCOUNT
	movl	$LSTAT,%eax
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
	_fw_setsize_(`lstat')'
)
