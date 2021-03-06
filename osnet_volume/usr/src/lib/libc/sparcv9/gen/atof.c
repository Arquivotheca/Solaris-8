/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)atof.c	1.7	96/12/20 SMI"
/*LINTLIBRARY*/

#include	"synonyms.h"
#include	<stdio.h>
#include	<stdlib.h>

double
atof(const char *cp)
{
	return (strtod(cp, (char **) NULL));
}
