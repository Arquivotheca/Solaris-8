/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)llog10.c	1.1	96/11/20 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

#pragma weak llog10 = _llog10

#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/dl.h>

dl_t
llog10(dl_t val)
{
	dl_t	result;

	result = lzero;
	val    = ldivide(val, lten);

	while (val.dl_hop != 0 || val.dl_lop != 0) {
		val = ldivide(val, lten);
		result = ladd(result, lone);
	}

	return (result);
}
