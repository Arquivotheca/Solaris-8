/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)isnanf.s	1.7	92/07/14 SMI"	/* SVr4.0 1.4	*/

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

#define	FMAX_EXP	0xff

	ENTRY(isnanf)
	srl	%o0, 23, %o1
	and	%o1, 0xff, %o1		! exponent
	cmp	%o1, FMAX_EXP		! if ( exp != 0xff )
	bne,a	.false			!	its not a NaN
	set	0, %o0
	set	0x7fffff, %o1
	and	%o1, %o0, %o1		! get fraction
	tst	%o1			! if ( fraction == 0 )
	be,a	.false			!	its an infinity
	set	0, %o0
	set	1, %o0
.false:
	retl
	nop

	SET_SIZE(isnanf)
