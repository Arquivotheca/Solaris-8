/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* 	Portions Copyright(c) 1988, Sun Microsystems Inc.	*/
/*	All Rights Reserved					*/

#pragma ident	"@(#)index.c	1.2	97/06/17 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

/*
 * Return the ptr in sp at which the character c appears;
 * NULL if not found
 */

#include <sys/types.h>
#include <strings.h>
#include <stddef.h>

char *
index(char *sp, char c)
{

	do {
		if (*sp == c)
			return (sp);
	} while (*sp++);
	return (NULL);
}
