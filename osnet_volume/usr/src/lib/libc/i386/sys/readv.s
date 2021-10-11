	.ident	"@(#)readv.s	1.10	98/07/08 SMI"

/ gid = readv();
/ returns effective gid

	.file	"readv.s"

	.text

	.globl  __cerror

_fwpdef_(`_readv', `_libc_readv'):
	MCOUNT
	movl	$READV,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	_readv
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
	_fwp_setsize_(`_readv', `_libc_readv')
