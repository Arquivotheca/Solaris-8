	.ident	"@(#)getpmsg.s	1.10	98/07/08 SMI"

/ gid = getpmsg();
/ returns effective gid

	.file	"getpmsg.s"

	.text

	.globl  __cerror

_fwpdef_(`_getpmsg', `_libc_getpmsg'):
	MCOUNT
	movl	$GETPMSG,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	_getpmsg
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
	_fwp_setsize_(`_getpmsg', `_libc_getpmsg')
