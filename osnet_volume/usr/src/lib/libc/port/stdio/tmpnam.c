/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tmpnam.c	1.11	96/11/19 SMI"	/* SVr4.0 1.11	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include "mtlib.h"
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <thread.h>
#include <synch.h>

static char seed[] = { 'a', 'a', 'a', '\0' };

#ifdef _REENTRANT
static mutex_t seed_lk = DEFAULTMUTEX;
#endif _REENTRANT

/*
 * Comment on _REENTRANT change :
 * For s == NULL, it is better to malloc than to use TLS (to minimize
 * TLS usage). Note that the caller should call free() on the pointer returned
 * by tmpnam - should be documented in the man page for tmpnam.
 *
 */

char *
tmpnam(char *s)
{
	static char buf[L_tmpnam];
	char *p = (s ? s : buf), *q;

	(void) strcpy(p, P_tmpdir);
	(void) _mutex_lock(&seed_lk);
	(void) strcat(p, seed);
	(void) strcat(p, "XXXXXX");

	q = seed;
	while (*q == 'z')
		*q++ = 'a';
	if (*q != '\0')
		++*q;
	(void) _mutex_unlock(&seed_lk);
	(void) mktemp(p);
	return (p);
}
