/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)getenv.c	1.13	96/11/27 SMI"	/* SVr4.0 1.8	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include <mtlib.h>
#include <sys/types.h>
#include <thread.h>
#include <synch.h>
#include <stdlib.h>

extern const char **environ;

#ifdef _REENTRANT
extern mutex_t	__environ_lock;
#endif

/*
 * s1 is either name, or name=value
 * s2 is name=value
 * if names match, return value of s2, else NULL
 * used for environment searching: see getenv
 */
static const char *
nvmatch(const char *s1, const char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == '=')
			return (s2);
	if (*s1 == '\0' && *(s2-1) == '=')
		return (s2);
	return (NULL);
}

/*
 * getenv(name)
 * returns ptr to value associated with name, if any, else NULL
 */
char *
getenv(const char *name)
{
	char *v = 0;
	const char **p;

	(void) _mutex_lock(&__environ_lock);
	p = environ;
	if (p == 0) {
		(void) _mutex_unlock(&__environ_lock);
		return (NULL);
	}
	while (*p != NULL)
		if ((v = (char *)nvmatch(name, *p++)) != 0)
			break;
	(void) _mutex_unlock(&__environ_lock);
	return (v);
}
