/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)cerror.s	1.3 94/11/03	SMI" /* SVr4.0 1.9 */

	.file	"cerror.s"

#include <sys/asm_linkage.h>
#include "i386/SYS.h"


/ C return sequence which sets errno, returns -1.
/ This code should only be called by system calls which have done the prologue

	fwdef(_Cerror)
#ifdef PIC
	cmpl	$ERESTART, (%esp)
	jne	1f
	movl	$EINTR, (%esp)
1:
#else
	cmpl	$ERESTART, %eax
	jne	1f
	movl	$EINTR, %eax
1:
	pushl	%eax
#endif
	call	fcnref(___errno)
	popl	%ecx
	movl	%ecx, (%eax)
	movl	$-1, %eax
	PIC_EPILOG
	ret
