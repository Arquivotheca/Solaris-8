/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)rand.c	1.8	96/10/15 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/
#include "synonyms.h"
#include <sys/types.h>
#include <stdlib.h>

static int randx = 1;

void
srand(unsigned x)
{
	randx = x;
}

int
rand(void)
{
	return (((randx = randx * 1103515245 + 12345)>>16) & 0x7fff);
}
