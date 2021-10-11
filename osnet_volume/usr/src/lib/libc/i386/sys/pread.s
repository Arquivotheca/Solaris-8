	.ident	"@(#)pread.s	1.13	98/07/08 SMI"

/ Here's what's going on. The top m4 ifdef decides whether
/ to build the transitional 64bit api depending on whether
/ or not _LARGEFILE_INTERFACE is defined.
	
	.file	"pread.s"

	.text

	.globl	__cerror

_m4_ifdef_(`_LARGEFILE_INTERFACE',
`_fwpdef_(`_pread64', `_libc_pread64'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$PREAD64,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror64
	cmpb	$ERESTART,%al
	je	_pread64
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror64:
	ret
	_fwp_setsize_(`_pread64', `_libc_pread64')',
`_fwpdef_(`_pread', `_libc_pread'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$PREAD,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je	_pread
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror:
	ret
	_fwp_setsize_(`_pread', `_libc_pread')'
)
