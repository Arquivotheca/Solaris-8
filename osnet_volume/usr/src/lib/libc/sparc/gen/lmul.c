/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)lmul.c	1.8	96/11/20 SMI"	/* SVr4.0 1.2 */

/*LINTLIBRARY*/

#pragma weak lmul = _lmul

#include	"synonyms.h"
#include	"sys/types.h"
#include	"sys/dl.h"

dl_t
lmul(dl_t lop, dl_t rop)
{
	dl_t		ans;
	dl_t		tmp;
	int	jj;

	ans = lzero;

	for (jj = 0; jj <= 63; jj++) {
		if ((lshiftl(rop, -jj).dl_lop & 1) == 0)
			continue;
		tmp = lshiftl(lop, jj);
		tmp.dl_hop &= 0x7fffffff;
		ans = ladd(ans, tmp);
	};

	return (ans);
}
