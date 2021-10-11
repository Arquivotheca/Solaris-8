	.ident	"@(#)ptrace.s	1.8	98/07/08 SMI"

	.file	"ptrace.s"

	.text

	.globl	__cerror
	.globl	errno

_fwdef_(`ptrace'):
	MCOUNT			/ subroutine entry counter if profiling
	_prologue_
_m4_ifdef_(`DSHLIB',
`	movl	_daref_(errno),%ecx
	movl	$0,(%ecx)
',
`	movl	$0,errno
')
	_epilogue_
	movl	$PTRACE,%eax
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
	_fw_setsize_(`ptrace')
