/
/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.
/

.ident	"@(#)_xtoll.s	1.3	98/07/08 SMI"

/ These functions truncate the top of the 387 stack into signed or
/ unsigned long, or a signed long-long.

	.file	"_xtoll.s"
	.set	cw,0
	.set	cw_old,2
	.set    four_words,4
	.set    two_words,4
	.text
	.globl __xtoll
	.globl __xtol
	.globl __xtoul
_fgdef_(__xtoll):		// 387-stack to signed long long
_fgdef_(__xtol):			// 387-stack to signed long
_fgdef_(__xtoul):		// 387-stack to unsigned long
	subl		$12,%esp
	fstcw		cw_old(%esp)
	movw		cw_old(%esp), %ax
	andw		$0x0f3ff, %ax
	orw		$0x0c00, %ax
	movw		%ax, cw(%esp)
	fldcw		cw(%esp)
	fistpll		four_words(%esp)
 	fldcw		cw_old (%esp)
 	movl		two_words(%esp), %eax
 	movl		two_words+4(%esp), %edx
	addl		$12,%esp
 	ret
	_fg_setsize_(`__xtoll')
	_fg_setsize_(`__xtol')
	_fg_setsize_(`__xtoul')
