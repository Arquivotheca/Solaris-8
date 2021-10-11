/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma ident  "@(#)new.c 1.7 94/08/25 SMI"

/*
 * Includes
 */

#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>
#include "new.h"


/*
 * new_alloc() - allocate and bail if neccessary
 */

void		   *
new_alloc(size_t size)
{
	void		   *ptr;

	ptr = malloc(size);

	if (!ptr) {
		(void) fprintf(stderr,
			gettext("new; out of memory, aborting\n"));
		abort();

	}
	return (ptr);

}				/* new_alloc */
