/
/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.
/

.ident	"@(#)fpcw.s	1.4	98/07/08 SMI"

	.file	"fpcw.s"
	.version "01.01"
	.text
	.align	4

_fwdef_(`_getcw'):
	movl	4(%esp), %eax
	fstcw	(%eax)
	ret
	_fw_setsize_(`_getcw')

_fwdef_(`_putcw'):
	fldcw	4(%esp)
	ret
	_fw_setsize_(`_putcw')
