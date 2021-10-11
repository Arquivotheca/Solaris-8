	.ident	"@(#)umount2.s	1.1	99/03/08 SMI"


	.file	"umount2.s"

	.text

	.globl	__cerror

_fwdef_(`umount2'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$UMOUNT2,%eax
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
	_fw_setsize_(`umount2')
