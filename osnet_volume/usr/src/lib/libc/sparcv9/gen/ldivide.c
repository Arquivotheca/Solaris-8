/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ldivide.c	1.7	96/12/20 SMI"

/*LINTLIBRARY*/

#pragma weak ldivide = _ldivide
#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/dl.h>
#include	"libc.h"

dl_t
ldivide(dl_t lop, dl_t rop)
{
	int		cnt;
	dl_t		ans;
	dl_t		tmp;
	dl_t		div;

	if (lsign(lop))
		lop = lsub(lzero, lop);
	if (lsign(rop))
		rop = lsub(lzero, rop);

	ans = lzero;
	div = lzero;

	for (cnt = 0; cnt < 63; cnt++) {
		div = lshiftl(div, 1);
		lop = lshiftl(lop, 1);
		if (lsign(lop))
			div.dl_lop |= 1;
		tmp = lsub(div, rop);
		ans = lshiftl(ans, 1);
		if (lsign(tmp) == 0) {
			ans.dl_lop |= 1;
			div = tmp;
		}
	}

	return (ans);
}
