/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)putenv.c	1.12	98/01/16 SMI"	/* SVr4.0 1.11	*/

/*LINTLIBRARY*/

#pragma weak putenv = _putenv

#include "synonyms.h"
#include "shlib.h"
#include <mtlib.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <thread.h>
#include <synch.h>

extern const char **environ;	/* pointer to enviroment */

#ifdef _REENTRANT
extern mutex_t	__environ_lock;
#endif

/*
 * s1 is either name, or name=value
 * s2 is name=value
 * if names match, return value of 1,
 * else return 0
 */
static
match(const char *s1, const char *s2)
{
	while (*s1 == *s2++) {
		if (*s1 == '=')
			return (1);
		s1++;
	}
	return (0);
}

/*
 * find - find where s2 is in environ
 *
 * input - str = string of form name=value
 *
 * output - index of name in environ that matches "name"
 *	 -size of table, if none exists
 */
static long
find(const char *str)
{
	long ct = 0;	/* index into environ */

	while (environ[ct] != 0)   {
		if (match(environ[ct], str) != 0)
			return (ct);
		ct++;
	}
	return (-(++ct));
}

/*
 * putenv - change environment variables
 *
 * input - const char *change = a pointer to a string of the form
 *				"name=value"
 * output - 0, if successful
 *	    1, otherwise
 */
int
putenv(char *change)
{
	char **newenv;	/* points to new environment */
	long	which;	/* index of variable to replace */
	static int reall = 0;	/* realloc (first call to putenv does alloc) */

	(void) _mutex_lock(&__environ_lock);
	if ((which = find(change)) < 0) {
		/*
		 * if a new variable
		 * which is negative of table size, so invert and
		 * count new element
		 */
		which = (-which) + 1;
		if (reall)  {
			/*
			 * we have expanded environ before
			 */
			newenv = realloc(environ, which * sizeof (char *));
			if (newenv == 0)  {
				(void) _mutex_unlock(&__environ_lock);
				return (-1);
			}
		} else {
			/*
			 * environ points to the original space
			 */
			reall++;
			newenv = malloc(which * sizeof (char *));
			if (newenv == 0) {
				(void) _mutex_unlock(&__environ_lock);
				return (-1);
			}
			(void) memcpy(newenv, environ,
			    which * sizeof (char *));
		}

		/*
		 * now that we have space, change environ
		 */
		environ = (const char **)newenv;
		environ[which-2] = change;
		environ[which-1] = 0;
	} else {
		/* we are replacing an old variable */
		environ[which] = change;
	}
	(void) _mutex_unlock(&__environ_lock);
	return (0);
}
