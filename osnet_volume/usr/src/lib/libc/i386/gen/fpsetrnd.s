/ fpsetrnd( new_rounding_mode )
/ sets the 80[23]87 coprocessor's rounding mode to new_rounding_mode
/ and returns the old rounding mode.

	.file	"fpsetrnd.s"
	.ident	"@(#)fpsetrnd.s	1.2	98/07/08 SMI"

	.text
        .set    FPRC,  0x00000c00 / FPRC (rounding control) from <sys/fp.h>
	.set	CFPRC, 0xfffff3ff / complement of FPRC
	.globl	fpsetrnd
	.align	4
_fgdef_(fpsetrnd):
	pushl	%ecx
	fstcw	0(%esp)		/ fstcw m
	movl	0(%esp),%eax
	movl	%eax,%ecx	/ save m
	andl	$CFPRC,%eax	/ m = (m & (~FPRC)) | (rnd & FPRC)
	movl	8(%esp),%edx
	andl	$FPRC,%edx
	orl	%edx,%eax
	movl	%eax,0(%esp)
	fldcw	0(%esp)
	movl	%ecx,%eax	/ restore old value of m
	andl	$FPRC,%eax	/ return m & FPRC
	popl	%ecx
	ret	
	.align	4
	_fg_setsize_(`fpsetrnd')
