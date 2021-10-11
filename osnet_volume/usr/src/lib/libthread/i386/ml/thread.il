/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)thread.il	1.7	94/11/03	SMI"

	.inline	_curthread, 0
	movl	%gs:0, %eax
	.end

	.inline _flush_store, 0
	lock
	xorl	$0, (%esp)
	.end
