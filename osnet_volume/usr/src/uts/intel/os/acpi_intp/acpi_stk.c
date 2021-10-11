/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_stk.c	1.2	99/06/29 SMI"


/* stacks */
#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>

#ifdef ACPI_BOOT
#include <sys/salib.h>
#endif

#ifdef ACPI_USER
#include <stdlib.h>
#endif

#include "acpi_exc.h"
#include "acpi_stk.h"


acpi_stk_t *
stack_new(int size, int len)
{
	acpi_stk_t *new;

	if ((new = kmem_alloc(sizeof (acpi_stk_t) + size * len, KM_SLEEP)) ==
	    NULL)
		return (exc_null(ACPI_ERES));
	new->ptr = (char *)new + sizeof (acpi_stk_t);
	new->base = new->ptr;
	new->index = 0;
	new->size = size;
	new->max = len - 1;
	return (new);
}

void
stack_free(acpi_stk_t *sp)
{
	if (sp)
		kmem_free(sp, sizeof (acpi_stk_t) + sp->size * (sp->max + 1));
}

void *
stack_push(acpi_stk_t *sp)
{
	if (sp->index >= sp->max)
		return (exc_null(ACPI_ELIMIT));
	sp->index++;
	return (sp->ptr += sp->size);
}

int
stack_pop(acpi_stk_t *sp)
{
	if (sp->index <= 0)
		return (exc_code(ACPI_ELIMIT));
	sp->index--;
	sp->ptr -= sp->size;
	return (ACPI_OK);
}

int
stack_popn(acpi_stk_t *sp, int n)
{
	int i;

	i = sp->index - n;
	if (i < 0 || i > sp->max)
		return (exc_code(ACPI_EINTERNAL));
	sp->index = i;
	sp->ptr -= n * sp->size;
	return (ACPI_OK);
}

int
stack_seek(acpi_stk_t *sp, int n)
{
	int diff;

	if (n < 0 || n > sp->max)
		return (exc_code(ACPI_EINTERNAL));
	diff = n - sp->index;
	sp->index = n;
	sp->ptr += diff * sp->size;
	return (ACPI_OK);
}


/* eof */
