/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)lexp10.c	1.1	96/11/20 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

#pragma weak lexp10 = _lexp10

#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/dl.h>

dl_t
lexp10(dl_t exp)
{
	dl_t	result;

	result = lone;

	while (exp.dl_hop != 0 || exp.dl_lop != 0) {
		result = lmul(result, lten);
		exp    = lsub(exp, lone);
	}

	return (result);
}
