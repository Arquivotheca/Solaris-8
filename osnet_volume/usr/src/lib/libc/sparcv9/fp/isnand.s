/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)isnand.s	1.8	97/08/29 SMI"	/* SVr4.0 1.7	*/

/*	int isnand(srcD)
 *	double srcD;
 *
 *	This routine returns 1 if the argument is a NaN
 *		     returns 0 otherwise.
 *
 *	int isnan(srcD)
 *	double srcD;
 *	-- functionality is same as isnand().
 */

	.file	"isnand.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(isnan,function)
	ANSI_PRAGMA_WEAK(isnand,function)

#include "synonyms.h"

	ENTRY2(isnan,isnand)
	fabsd	%f0,%f0
	std	%f0,[%sp+STACK_BIAS+ARGPUSH]
	ldx	[%sp+STACK_BIAS+ARGPUSH],%o0
	sethi	%hi(0x7ff00000),%o1
	sllx	%o1,32,%o1
	sub	%o1,%o0,%o0
	retl
	srlx	%o0,63,%o0

	SET_SIZE(isnan)
	SET_SIZE(isnand)
