/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)i386.il	1.1	99/10/14 SMI"

	.inline	_curthread,0
	movl	%gs:0, %eax
	.end

	.inline	gethrtime,0
	movl	$3, %eax
	int	$0xd2
	.end
