/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)memutils.c	1.5	99/01/22 SMI"

#include <stdlib.h>
#include <errno.h>
#include <libintl.h>
#include <string.h>
#include "memutils.h"

extern void msg(const char *, ...);
extern void dumpabort(void);

void *
xmalloc(bytes)
	size_t bytes;
{
	void *cp;

	cp = malloc(bytes);
	if (cp == NULL) {
		int saverr = errno;
		msg(gettext("Cannot allocate memory: %s\n"), strerror(saverr));
		dumpabort();
	}
	return (cp);
}

void *
xcalloc(nelem, size)
	size_t nelem;
	size_t size;
{
	void *cp;

	cp = calloc(nelem, size);
	if (cp == NULL) {
		int saverr = errno;
		msg(gettext("Cannot allocate memory: %s\n"), strerror(saverr));
		dumpabort();
	}
	return (cp);
}

void *
xrealloc(allocated, newsize)
	void *allocated;
	size_t newsize;
{
	void *cp;

	/* LINTED realloc knows what to do with a NULL pointer */
	cp = realloc(allocated, newsize);
	if (cp == NULL) {
		int saverr = errno;
		msg(gettext("Cannot allocate memory: %s\n"), strerror(saverr));
		dumpabort();
	}
	return (cp);
}
