/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)atof.c	1.1	92/04/17 SMI"	/* SVr4.0 1.13	*/
/*LINTLIBRARY*/

#include	"synonyms.h"
#include	<stdio.h>

extern	double
strtod();

double
atof(char *cp)
{
	return strtod(cp, (char **) NULL);
}
