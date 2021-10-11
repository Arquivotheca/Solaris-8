	.ident	"@(#)getrlimit.s	1.10	98/07/08 SMI"

	
/ Here's what's going on. The top m4 ifdef decides whether
/ to build the transitional 64bit api depending on whether
/ or not _LARGEFILE_INTERFACE is defined.
	
	.file	"getrlimit.s"

	.text

	.globl  __cerror


_m4_ifdef_(`_LARGEFILE_INTERFACE',
`_fwdef_(`getrlimit64'):
	MCOUNT
	movl	$GETRLIMIT64,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror64
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror64:
	xorl	%eax, %eax
	ret
	_fw_setsize_(`getrlimit64')',
`_fwdef_(`getrlimit'):
	MCOUNT
	movl	$GETRLIMIT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	xorl	%eax, %eax
	ret
	_fw_setsize_(`getrlimit')'
)
