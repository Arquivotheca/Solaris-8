	.ident	"@(#)nuname.s	1.8	98/07/08 SMI"

/ gid = nuname();
/ returns effective gid

	.file	"nuname.s"

	.text

	.globl  __cerror

_fwdef_(`nuname'):
	MCOUNT
	movl	$NUNAME,%eax
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
	_fw_setsize_(`nuname')
