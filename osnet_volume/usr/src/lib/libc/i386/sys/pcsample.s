/ Copyright (c) 1998, by Sun Microsystems, Inc.
/ All rights reserved

	.ident	"@(#)pcsample.s	1.2	98/07/08 SMI"

	.file	"pcsample.s"

	.text

	.globl	__cerror

_fwdef_(`pcsample'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$PCSAMPLE,%eax
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
	_fw_setsize_(`pcsample')
