/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)zmalloc.c	6.3	94/11/04 SMI"

/*
 *	malloc(3C) with error checking
 */

#include <stdio.h>
#include "errmsg.h"
#ifdef __STDC__
#include <stdlib.h>
#else
extern char *malloc();
#endif

char *
zmalloc(severity, n)
int	severity;
unsigned	n;
{
	char	*p;

	if ((p = (char *) malloc(n)) == NULL)
		_errmsg("UXzmalloc1", severity,
			"Cannot allocate a block of %d bytes.",
			n);
	return (p);
}
