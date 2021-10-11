/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)lsign.s	1.6	92/07/14 SMI"	/* SVr4.0 1.1	*/

/*
 * Determine the sign of a double-long number.
 * Ported from m32 version to sparc.
 *
 *	int
 *	lsign (op)
 *		dl_t	op;
 */

	.file	"lsign.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lsign,function)

#include "synonyms.h"

	ENTRY(lsign)

	ld	[%o0],%o0		! fetch op (high word only)
	jmp	%o7+8			! return
	srl	%o0,31,%o0		! shift letf logical to isolate sign

	SET_SIZE(lsign)
