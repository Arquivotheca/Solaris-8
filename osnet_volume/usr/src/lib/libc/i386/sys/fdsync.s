	.ident	"@(#)fdsync.s	1.4 0 SMI"

/ error = __fdsync(fd, how);


	.file	"fdsync.s"

	.text

	.globl  __cerror

_fwdef_(`__fdsync'):
	MCOUNT
	movl	$FDSYNC,%eax
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
	_fw_setsize_(`__fdsync')
