	.ident	"@(#)mount.s	1.8	98/07/08 SMI"

	.file	"mount.s"

	.globl	__cerror

_fwdef_(`mount'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$MOUNT,%eax
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
	_fw_setsize_(`mount')
