	.ident	"@(#)mmap.s	1.10	98/07/08 SMI"

/ gid = mmap();
/ returns effective gid

	.file	"mmap.s"

	.text

	.globl  __cerror

_m4_ifdef_(`_LARGEFILE_INTERFACE',
`_fwdef_(`mmap64'):
	MCOUNT
	movl	$MMAP64,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror64
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror64:
	ret
	_fw_setsize_(`mmap64')',
`_fwdef_(`mmap'):
	MCOUNT
	movl	$MMAP,%eax
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
	_fw_setsize_(`mmap')'
)
