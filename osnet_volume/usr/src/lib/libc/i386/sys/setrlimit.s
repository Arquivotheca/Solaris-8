	.ident	"@(#)setrlimit.s	1.11	98/07/08 SMI"

/ Here's what's going on. The top m4 ifdef decides whether
/ to build the transitional 64bit api depending on whether
/ or not _LARGEFILE_INTERFACE is defined.
	
	.file	"setrlimit.s"
	
	.text

	.globl	__cerror

_m4_ifdef_(`_LARGEFILE_INTERFACE',
`_fwdef_(`setrlimit64'):
	MCOUNT
	movl	$SETRLIMIT64,%eax
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
	_fw_setsize_(`setrlimit64')',
`_fwdef_(`setrlimit'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SETRLIMIT,%eax
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
	_fw_setsize_(`setrlimit')'
)
