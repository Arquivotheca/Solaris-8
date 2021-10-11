/
/ Copyright (c) 1995, by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)setreid.s	1.3	98/07/08 SMI"

	.file	"setreid.s"

	.text

	.globl	__cerror

_fwdef_(`setreuid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SETREUID,%eax
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
	_fw_setsize_(`setreuid')

_fwdef_(`setregid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SETREGID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
	_fw_setsize_(`setregid')
