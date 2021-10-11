	.ident	"@(#)ntp_adjtime.s	1.2	98/07/08 SMI"


	.file	"ntp_adjtime.s"

	.text

	.globl	__cerror

_fwdef_(`ntp_adjtime'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$NTP_ADJTIME,%eax
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
	_fw_setsize_(`ntp_adjtime')
