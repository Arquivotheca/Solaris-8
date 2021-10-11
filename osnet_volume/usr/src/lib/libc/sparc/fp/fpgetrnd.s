/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)fpgetrnd.s	1.7	92/07/14 SMI"	/* SVr4.0 1.11	*/

	.file	"fpgetrnd.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpgetround,function)

#include "synonyms.h"

	ENTRY(fpgetround)
	add	%sp, -SA(MINFRAME), %sp	! get an additional word of storage
	st	%fsr, [%sp+ARGPUSH]	! get fsr value
	ld	[%sp+ARGPUSH], %o0	! load into register
	srl	%o0, 30, %o0		! return round control value
	retl
	add	%sp, SA(MINFRAME), %sp	! reclaim stack space

	SET_SIZE(fpgetround)
