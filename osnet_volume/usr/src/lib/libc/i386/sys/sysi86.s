	.ident	"@(#)sysi86.s	1.9	98/07/08 SMI"

	.file	"sysi86.s"
	
	.text

	.globl	__cerror

_fwdef_(`sysi86'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$SYSI86,%eax
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
	_fw_setsize_(`sysi86')
