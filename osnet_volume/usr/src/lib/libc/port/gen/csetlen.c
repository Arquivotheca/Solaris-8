/*
 * Copyright (c) 1988, 1996  by Sun Microsystems, Inc.
 * All rights reserved.
 * Copyright (c) 1988 by Nihon Sun Microsystems K.K.
 */

#pragma ident	"@(#)csetlen.c	1.6	96/10/15 SMI"   /* Nihon Sun Micro JLE */

/* LINTLIBRARY */

#include	"synonyms.h"
#include	<sys/types.h>
#include	<ctype.h>
#include	<euc.h>

int
csetlen(int cset)
{
	switch (cset) {
	case 0:
		return (1);
	case 1:
		return (eucw1);
	case 2:
		return (eucw2);
	case 3:
		return (eucw3);
	default:
		return (0);
	}
}


int
csetcol(int cset)
{
	switch (cset) {
	case 0:
		return (1);
	case 1:
		return (scrw1);
	case 2:
		return (scrw2);
	case 3:
		return (scrw3);
	default:
		return (0);
	}
}
