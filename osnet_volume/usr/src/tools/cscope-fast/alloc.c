/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)alloc.c	1.1	99/01/11 SMI"

/* memory allocation functions */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern	char	*argv0;	/* command name (must be set in main function) */

char	*stralloc(char *s);
void	*mymalloc(size_t size);
void	*mycalloc(size_t nelem, size_t size);
void	*myrealloc(void *p, size_t size);
static void *alloctest(void *p);

/* allocate a string */

char *
stralloc(char *s)
{
	return (strcpy(mymalloc(strlen(s) + 1), s));
}

/* version of malloc that only returns if successful */

void *
mymalloc(size_t size)
{
	return (alloctest(malloc(size)));
}

/* version of calloc that only returns if successful */

void *
mycalloc(size_t nelem, size_t size)
{
	return (alloctest(calloc(nelem, size)));
}

/* version of realloc that only returns if successful */

void *
myrealloc(void *p, size_t size)
{
	return (alloctest(realloc(p, size)));
}

/* check for memory allocation failure */

static void *
alloctest(void *p)
{
	if (p == NULL) {
		(void) fprintf(stderr, "\n%s: out of storage\n", argv0);
		exit(1);
	}
	return (p);
}
