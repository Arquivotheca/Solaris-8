/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)isnand.s	1.6	92/07/14 SMI"	/* SVr4.0 1.7	*/

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

#define	DMAX_EXP	0x7ff

	ENTRY2(isnan,isnand)
	srl	%o0, 20, %o2
	and	%o2, 0x7ff, %o2		! exponent
	cmp	%o2, DMAX_EXP		! if ( exp != 0x7ff )
	bne,a	.false			!	its not a NaN
	set	0, %o0
	set	0xfffff, %o2
	and	%o2, %o0, %o2		! get fraction
	or	%o1, %o2, %o2		! or lsw or fraction
	tst	%o2			! if ( fraction == 0 )
	be,a	.false			!	its an infinity
	set	0, %o0
	set	1, %o0
.false:
	retl
	nop

	SET_SIZE(isnan)
	SET_SIZE(isnand)
