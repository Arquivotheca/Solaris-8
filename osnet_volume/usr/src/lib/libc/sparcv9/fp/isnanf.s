/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)isnanf.s	1.9	97/08/29 SMI"	/* SVr4.0 1.4	*/

/*	int isnanf(srcF)
 *	float srcF;
 *
 *	This routine returns 1 if the argument is a NaN
 *		     returns 0 otherwise.
 */

	.file	"isnanf.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(isnanf,function)

#include "synonyms.h"

	ENTRY(isnanf)
	fabss	%f1,%f1
	st	%f1,[%sp+STACK_BIAS+ARGPUSH]
	ld	[%sp+STACK_BIAS+ARGPUSH],%o0
	sethi	%hi(0x7f800000),%o1
	sub	%o1,%o0,%o0
	retl
	srl	%o0,31,%o0

	SET_SIZE(isnanf)
