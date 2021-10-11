.ident "@(#)_times.s	1.6	96/03/28 SMI"


	.file	"times.s"

	.text

	.globl	__ucbcerror

_fwdef_(`times'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$TIMES,%eax
	lcall	$0x7,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__ucbcerror)
noerror:
	ret
