/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 *	Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)execle.c	1.18	99/05/04 SMI"
					/* SVr4.0 1.6.1.5	*/

/*
 *	execle(file, arg1, arg2, ..., 0, envp)
 */

/*LINTLIBRARY*/
#include "synonyms.h"
#include <sys/types.h>
#include <alloca.h>
#include <malloc.h>
#include <stdarg.h>
#include <unistd.h>

#pragma weak execle = _execle

/*VARARGS1*/
int
execle(const char *file, const char *arg0, ...)
{
	char **argp;
	va_list args;
	char **argvec;
	register  char  **environmentp;
	int err;
	int nargs = 0;
	char *nextarg;

	/*
	 * count the number of arguments in the variable argument list
	 * and allocate an argument vector for them on the stack,
	 * adding space at the end for a terminating null pointer
	 */

	va_start(args, arg0);
	while (va_arg(args, char *) != (char *)0) {
		nargs++;
	}

	/*
	 * save the environment pointer, which is at the end of the
	 * variable argument list
	 */

	environmentp = va_arg(args, char **);
	va_end(args);

	/*
	 * load the arguments in the variable argument list
	 * into the argument vector, and add the terminating null pointer
	 */

	va_start(args, arg0);
	/* workaround for bugid 1242839 */
	argvec = (char **)alloca((size_t)((nargs + 2) * sizeof (char *)));
	argp = argvec;
	*argp++ = (char *)arg0;
	while ((nextarg = va_arg(args, char *)) != (char *)0) {
		*argp++ = nextarg;
	}
	va_end(args);
	*argp = (char *)0;

	/*
	 * call execve()
	 */

	err = execve(file, argvec, environmentp);
	return (err);
}
