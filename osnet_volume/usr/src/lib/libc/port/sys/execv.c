/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1996 Sun Microsystems Inc
 *      All Rights Reserved.
*/
#pragma ident	"@(#)execv.c	1.11	96/11/13 SMI"	/* SVr4.0 1.6.1.5	*/

/*
 *	execv(file, argv)
 *
 *	where argv is a vector argv[0] ... argv[x], NULL
 *	last vector element must be NULL
 *	environment passed automatically
 */

/*LINTLIBRARY*/
#include "synonyms.h"
#include <sys/types.h>
#include <unistd.h>

#pragma weak execv = _execv

int
execv(const char *file, char *const argv[])
{
	extern  char    **environ;
	return (execve(file, argv, environ));
}
