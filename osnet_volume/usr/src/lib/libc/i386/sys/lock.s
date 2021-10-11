/
/ Copyright (c) 1993-1999 by Sun Microsystems, Inc.
/ All rights reserved.
/

	.ident	"@(#)lock.s	1.4	99/09/13 SMI"

	.file	"lock.s"

	.text

/
/ lock_try(lp)
/	- returns non-zero on success.
/
	.weak	_private_lock_try
	_private_lock_try = __lock_try
_fwdef_(`_lock_try'):
	movl	$1,%eax
	movl	4(%esp),%ecx
	xchgb	%al, (%ecx)
	xorb	$1, %al
	ret
	_fw_setsize_(`_lock_try')

/
/ lock_clear(lp)
/	- clear lock and force it to appear unlocked in memory.
/
_fwdef_(`_lock_clear'):
	movl	4(%esp),%eax
	movb	$0, (%eax)
	ret
	_fw_setsize_(`_lock_clear')

