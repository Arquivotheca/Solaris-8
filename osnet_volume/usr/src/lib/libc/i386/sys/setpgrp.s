	.ident	"@(#)setpgrp.s	1.8	98/07/08 SMI"

	.file	"setpgrp.s"

	.text

	.globl	__cerror

_fwdef_(`setpgrp'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	%esp,%ecx
	pushl	$1
	pushl	%eax		/ slot for ret addr.
	movl	$SETPGRP,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	movl	%ecx,%esp
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
	_fw_setsize_(`setpgrp')

_fwdef_(`getpgrp'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	%esp,%ecx
	pushl	$0
	pushl	%eax		/ slot for ret addr.
	movl	$SETPGRP,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	movl	%ecx,%esp
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
	_fw_setsize_(`getpgrp')
