	.ident	"@(#)pwrite.s	1.13	98/07/08 SMI"

/ Here's what's going on. The top m4 ifdef decides whether
/ to build the transitional 64bit api depending on whether
/ or not _LARGEFILE_INTERFACE is defined.

	.file	"pwrite.s"

	.text

_m4_ifdef_(`_LARGEFILE_INTERFACE',
`_fwpdef_(`_pwrite64', `_libc_pwrite64'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$PWRITE64,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror64
	cmpb	$ERESTART,%al
	je	_pwrite64
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror64:
	ret
	_fwp_setsize_(`_pwrite64', `_libc_pwrite64')',
`_fwpdef_(`_pwrite', `_libc_pwrite'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$PWRITE,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	_pwrite
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror:
	ret
	_fwp_setsize_(`_pwrite', `_libc_pwrite')'
)
