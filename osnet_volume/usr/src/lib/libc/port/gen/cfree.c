/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cfree.c	1.7	96/10/15 SMI"	/* SVr4.0 1.1	*/

/* LINTLIBRARY */
/*
 * cfree - clear memory block
 */
#define	NULL 0

#pragma weak cfree = _cfree
#include "synonyms.h"
#include <sys/types.h>
#include <stdlib.h>

void cfree(void *p, unsigned num, unsigned size);

/*ARGSUSED*/
void
cfree(void *p, unsigned num, unsigned size)
{
	free(p);
}
