/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)tempnam.c	1.10	97/02/05 SMI"	/* SVr4.0 1.7.1.9 */

/*LINTLIBRARY*/
#pragma weak tempnam = _tempnam

#include "synonyms.h"
#include "shlib.h"
#include <mtlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <thread.h>
#include <synch.h>
#include <unistd.h>
#include <sys/stat.h>

#define	max(A, B) (((A) < (B))?(B):(A))

static char *pcopy(char *, const char *);

static char *seed = "AAA";

#ifdef _REENTRANT
static mutex_t seed_lk = DEFAULTMUTEX;
#endif _REENTRANT

char *
tempnam(const char *dir,	/* use this directory please (if non-NULL) */
	const char *pfx)	/* use this (if non-NULL) as filename prefix */
{
	char *p, *q, *tdir;
	size_t x = 0, y = 0, z;
	struct stat64 statbuf;

	z = sizeof (P_tmpdir) - 1;
	if ((tdir = getenv("TMPDIR")) != NULL) {
		x = strlen(tdir);
	}
	if (dir != NULL) {
		if (stat64(dir, &statbuf) == 0 && S_ISDIR(statbuf.st_mode))
			y = strlen(dir);
	}
	if ((p = malloc(max(max(x, y), z)+16)) == NULL)
		return (NULL);
	if (x > 0 && access(pcopy(p, tdir), (W_OK | X_OK)) == 0)
		goto OK;
	if (y > 0 && access(pcopy(p, dir), (W_OK | X_OK)) == 0)
		goto OK;
	if (access(pcopy(p, P_tmpdir), (W_OK | X_OK)) == 0)
		goto OK;
	if (access(pcopy(p, "/tmp"), (W_OK | X_OK)) != 0) {
		free(p);
		return (NULL);
	}
OK:
	(void) strcat(p, "/");
	if (pfx) {
		*(p+strlen(p)+5) = '\0';
		(void) strncat(p, pfx, 5);
	}
	(void) _mutex_lock(&seed_lk);
	(void) strcat(p, seed);
	(void) strcat(p, "XXXXXX");
	q = seed;
	while (*q == 'Z')
		*q++ = 'A';
	if (*q != '\0')
		++*q;
	(void) _mutex_unlock(&seed_lk);
	if (*mktemp(p) == '\0') {
		free(p);
		return (NULL);
	}
	return (p);
}

static char *
pcopy(char *space, const char *arg)
{
	char *p;

	if (arg) {
		(void) strcpy(space, arg);
		p = space-1+strlen(space);
		while ((p >= space) && (*p == '/'))
			*p-- = '\0';
	}
	return (space);
}
