/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* 	Portions Copyright(c) 1988, Sun Microsystems Inc.	*/
/*	All Rights Reserved					*/

#pragma ident	"@(#)index.c	1.3	95/03/01 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/
#include "synonyms.h"

#include <sys/types.h>
#include <string.h>
#include <strings.h>

/*
 * Return the ptr in sp at which the character c appears;
 * NULL if not found
 */
char *
index(const char *sp, int c)
{
	return (strchr(sp, c));
}
