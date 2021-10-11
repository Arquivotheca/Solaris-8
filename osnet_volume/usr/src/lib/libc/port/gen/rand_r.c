/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)rand_r.c	1.5	96/10/15 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/
#pragma weak rand_r = _rand_r

#include "synonyms.h"
#include <sys/types.h>
#include <stdlib.h>

int
_rand_r(unsigned int *randx)
{
	return (((*randx = *randx * 1103515245 + 12345)>>16) & 0x7fff);
}
