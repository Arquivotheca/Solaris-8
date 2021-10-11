/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 *	Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */


#pragma ident	"@(#)execl.c	1.18	99/05/04 SMI"
					/* SVr4.0 1.5.1.6	*/

/*
 *	execl(name, arg0, arg1, ..., argn, 0)
 *	environment automatically passed.
 */
/*LINTLIBRARY*/

#include "synonyms.h"
#include <alloca.h>
#include <malloc.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#pragma weak execl = _execl


/*VARARGS1*/
int
execl(const char *name, const char *arg0, ...)
{
	char **argp;
	va_list args;
	char **argvec;
	extern char **environ;
	int err;
	int nargs = 0;
	char *nextarg;

	/*
	 * count the number of arguments in the variable argument list
	 * and allocate an argument vector for them on the stack,
	 * adding an extra element for a terminating null pointer
	 */

	va_start(args, arg0);
	while (va_arg(args, char *) != (char *)0) {
		nargs++;
	}
	va_end(args);

	/*
	 * load the arguments in the variable argument list
	 * into the argument vector and add the terminating null pointer
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

	err = execve(name, argvec, environ);
	return (err);
}
