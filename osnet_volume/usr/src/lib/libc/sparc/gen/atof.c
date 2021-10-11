/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)atof.c	1.8	96/12/03 SMI"	/* SVr4.0 1.13	*/
/*LINTLIBRARY*/

#include	"synonyms.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<sys/types.h>

double
atof(const char *cp)
{
	return (strtod(cp, (char **) NULL));
}
