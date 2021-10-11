.ident	"@(#)getegid.s	1.3	98/07/08 SMI"

	.file	"getegid.s"

	.text

_fwdef_(`getegid'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$GETGID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	movl	%edx,%eax
	ret
	_fw_setsize_(`getegid')
