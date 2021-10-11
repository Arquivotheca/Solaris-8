	.ident	"@(#)setsid.s	1.8	98/07/08 SMI"

/ C library -- setsid, setpgid, getsid, getpgid

	.file	"setsid.s"

	.text

	.globl  __cerror

_fwdef_(`getsid'):
	popl	%edx
	pushl	$2
	pushl	%edx
	jmp	pgrp

_fwdef_(`setsid'):
	popl	%edx
	pushl	$3
	pushl	%edx
	jmp	pgrp

_fwdef_(`getpgid'):
	popl	%edx
	pushl	$4
	pushl	%edx
	jmp	pgrp

	
_fwdef_(`setpgid'):
	popl	%edx
	pushl	$5
	pushl	%edx
	jmp	pgrp

pgrp:
	movl	$SETSID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	popl	%edx
	movl	%edx,0(%esp)	/ Remove extra word
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
	_fw_setsize_(`getsid')
	_fw_setsize_(`setsid')
	_fw_setsize_(`getpgid')
	_fw_setsize_(`setpgid')
