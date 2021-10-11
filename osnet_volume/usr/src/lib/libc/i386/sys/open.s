	.ident	"@(#)open.s	1.15	98/07/08 SMI"

/ Here's what's going on. The top m4 ifdef decides whether
/ to build the transitional 64bit api depending on whether
/ or not _LARGEFILE_INTERFACE is defined.
	
	.file	"open.s"
	
	.text

	.globl	__cerror
	.globl	__open
	.globl	__open64


_m4_ifdef_(`_LARGEFILE_INTERFACE',	
`_fgdef_(`__open64'):
	MCOUNT
	movl	$OPEN64,%eax
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
	_fg_setsize_(`__open64')',

`_fgdef_(`__open'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$OPEN,%eax
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
	_fg_setsize_(`__open')'
)
