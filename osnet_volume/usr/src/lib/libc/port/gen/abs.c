/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)abs.c	1.8	96/10/15 SMI"	/* SVr4.0 1.9	*/

/*LINTLIBRARY*/
#include "synonyms.h"
#include "shlib.h"
#include <stdlib.h>
#include <sys/types.h>

int
abs(int arg)
{
	return (arg >= 0 ? arg : -arg);
}

long
labs(long int arg)
{
	return (arg >= 0 ? arg : -arg);
}
