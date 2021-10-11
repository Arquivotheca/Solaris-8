/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)lmath.c	1.1	97/05/25 SMI"

/*
 * This set of routines used to be implemented mostly in assembler.
 *
 * Since the purpose they have is now rather vestigial, and V9
 * can do operations on 64-bit quantities pretty efficiently, a
 * C implementation seems quite adequate and much more maintainable.
 */

#pragma	weak	ladd	= _ladd
#pragma	weak	lsub	= _lsub
#pragma	weak	lshiftl	= _lshiftl
#pragma	weak	lsign	= _lsign
#pragma	weak	lmul	= _lmul

#include "synonyms.h"

#include <sys/types.h>
#include <sys/dl.h>

typedef union {
	long	xword;
	dl_t	dl;
} dlx_t;

dl_t
ladd(dl_t lop, dl_t rop)
{
	dlx_t r;
	r.xword = *(long *)&lop + *(long *)&rop;
	return (r.dl);
}

dl_t
lshiftl(dl_t op, int cnt)
{
	dlx_t r;
	if (cnt < 0)
		r.xword = (long)(*(u_long *)&op >> (-cnt));
	else
		r.xword = *(long *)&op << cnt;
	return (r.dl);
}

int
lsign(dl_t op)
{
	return ((*(long *)&op) >> 63);
}

dl_t
lsub(dl_t lop, dl_t rop)
{
	dlx_t r;
	r.xword = *(long *)&lop - *(long *)&rop;
	return (r.dl);
}

dl_t
lmul(dl_t lop, dl_t rop)
{
	dlx_t r;
	r.xword = *(long *)&lop * *(long *)&rop;
	return (r.dl);
}
