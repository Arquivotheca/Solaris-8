/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)interceptlib.c	1.1	99/05/14 SMI"

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <apptrace.h>
#include "abienv.h"

/*
 * This file is meant to contain support functions
 * for interceptors.  They are built into the auditing
 * object making the namespace available to "children"
 * objects.
 */

size_t
strnlen(const char *s, size_t n)
{
	char *ptr;

	if (s == NULL)
		return (n);
	ptr = memchr(s, 0, n);
	if (ptr == NULL)
		return (n);

	return ((ptr - s) + 1);
}

/* Return true on empty */
int
is_empty_string(char const *s)
{
	if (s != NULL && *s != '\0')
		return (0);

	return (1);
}

void
abilock(sigset_t *mask)
{
	(*abi_sigsetmask)(SIG_BLOCK, &abisigset, mask);
	(*abi_mutex_lock)(&abi_stdio_mutex);
}

void
abiunlock(sigset_t *mask)
{
	(*abi_mutex_unlock)(&abi_stdio_mutex);
	(*abi_sigsetmask)(SIG_SETMASK, mask, NULL);
}
