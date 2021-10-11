	.ident	"@(#)_xtoull.s 1.6 93/04/01 SMI"

/       Copyright (c) 1992 by Sun Microsystems, Inc.

 	.file "_xtoull.s"

	.set	cw,0
	.set	cw_old,2
	.set    four_words,8
	.set    two_words,4

	.text
	.align  4
two_to_63: .long	0x5f000000

	.globl  __xtoull
_fgdef_(__xtoull):
	subl		$12, %esp

	fstcw		cw_old(%esp)
	movw		cw_old(%esp), %ax
	andw		$0xf3ff, %ax
	orw		$0x0c00, %ax
	movw		%ax, cw(%esp)
	fldcw		cw(%esp)
	_prologue_
	fcoms           _sref_(two_to_63)       // compare st to 2**63
	_epilogue_
	fnstsw          %ax                     // store status in %ax
	sahf                                    // load AH into flags
	jbe             .donotsub               // jump if st >= 2**63
	_prologue_
 	fsubs		_sref_(two_to_63)       // Subtract 2**63
	_epilogue_
.donotsub:
	fistpll		two_words(%esp)
 	fldcw		cw_old(%esp)
  	movl		two_words(%esp), %eax
  	movl		four_words(%esp), %edx
	jbe             .donotadd               // flags did not change
 	add		$0x80000000, %edx	// Add back 2**63
.donotadd:
	addl		$12, %esp
 	ret
	.align  4
	.size  __xtoull,.-__xtoull
