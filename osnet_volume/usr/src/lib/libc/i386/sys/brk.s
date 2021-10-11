/
/ Copyright (c) 1992-1997 by Sun Microsystems, Inc.
/ All rights reserved.
/
	.ident	"@(#)brk.s	1.2	98/07/08 SMI"
	.file	"brk.s"

	.text

	.globl	_nd
	.globl	__cerror

/
/  _brk_unlocked() simply traps into the kernel to set the brk.  It
/  returns 0 if the break was successfully set, or -1 otherwise.
/  It doesn't enforce any alignment and it doesn't perform any locking.
/  _brk_unlocked() is only called from brk() and _sbrk_unlocked().
/
_fwdef_(`_brk_unlocked'):
	movl	$BRK,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	_prologue_
	movl	_esp_(4),%edx
_m4_ifdef_(`DSHLIB',
`	movl	_daref_(_nd),%ecx
	movl	%edx,(%ecx)
',
`	movl    %edx,_nd
')
	xorl	%eax,%eax
	_epilogue_
	ret
	_fw_setsize_(`_brk_unlocked')

	.data
_nd:
	.long	end
