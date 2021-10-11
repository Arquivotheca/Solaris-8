.ident	"@(#)adjtime.s	1.8	98/07/08 SMI"

/ adjtime


	.file	"adjtime.s"

	.text

	.globl  __cerror

_fwdef_(`adjtime'):
	MCOUNT
	movl	$ADJTIME,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp 	_fref_(__cerror)
noerror:
	xorl	%eax,%eax
	ret
	_fw_setsize_(`adjtime')
