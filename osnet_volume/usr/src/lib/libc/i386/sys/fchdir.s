
.ident	"@(#)fchdir.s	1.8	98/07/08 SMI"

/ error = fchdir(fd)

	.file	"fchdir.s"

	.text

	.globl  __cerror

_fwdef_(`fchdir'):
	MCOUNT
	movl	$FCHDIR,%eax
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
	_fw_setsize_(`fchdir')
