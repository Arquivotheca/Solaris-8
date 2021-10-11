/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sparc.il	1.1	99/10/14 SMI"

	.inline	_curthread, 0
	.register %g7, #scratch
	mov	%g7, %o0
	.end

	.inline	gethrtime, 0
	ta	0x24
	.end
