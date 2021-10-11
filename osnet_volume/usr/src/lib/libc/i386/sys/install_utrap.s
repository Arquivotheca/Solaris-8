/
/ Copyright (c) 1995, by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)install_utrap.s	1.3	98/07/08 SMI"

	.file	"install_utrap.s"

	.text

	.globl	__cerror

_fwdef_(`install_utrap'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$INSTALL_UTRAP,%eax
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
	_fw_setsize_(`install_utrap')
