	.ident	"@(#)memcntl.s	1.8	98/07/08 SMI"

/ gid = memcntl();
/ returns effective gid

	.file	"memcntl.s"

	.text

	.globl  __cerror

_fwdef_(`memcntl'):
	MCOUNT
	movl	$MEMCNTL,%eax
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
	_fw_setsize_(`memcntl')
