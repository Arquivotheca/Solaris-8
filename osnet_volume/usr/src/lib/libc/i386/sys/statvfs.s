	.ident	"@(#)statvfs.s	1.10	98/07/08 SMI"


/ Here's what's going on. The top m4 ifdef decides whether
/ to build the transitional 64bit api depending on whether
/ or not _LARGEFILE_INTERFACE is defined.
	
	.file	"statvfs.s"

	.text

	.globl	__cerror


_m4_ifdef_(`_LARGEFILE_INTERFACE',
`_fwdef_(`statvfs64'):
	MCOUNT
	movl	$STATVFS64,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror64
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror64:
	xorl	%eax,%eax
	ret
	_fw_setsize_(`statvfs64')',
`_fwdef_(`statvfs'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$STATVFS,%eax
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
	_fw_setsize_(`statvfs')'
)
