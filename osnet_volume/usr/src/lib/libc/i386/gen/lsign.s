
/ Determine the sign of a double-long number.

	.ident	"@(#)lsign.s	1.2	98/07/08 SMI"
	.file	"lsign.s"
	.text

_fwdef_(`lsign'):

	MCOUNT

	movl	8(%esp),%eax
	roll	%eax
	andl	$1,%eax

	ret
	_fw_setsize_(`lsign')
