/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)idtab.c	1.1	97/12/08 SMI"

#include <libintl.h>
#include <string.h>
#include <stdlib.h>

#include "utils.h"
#include "idtab.h"

#define	IDTAB_GROW	2	/* Table size multiplier on grow */
#define	IDTAB_DEFSIZE	16	/* Starting table size */

void
idtab_create(idtab_t *idt)
{
	(void) memset(idt, 0, sizeof (idtab_t));
}

void
idtab_destroy(idtab_t *idt)
{
	if (idt->id_data) {
		free(idt->id_data);
		idt->id_data = NULL;
		idt->id_nelems = idt->id_size = 0;
	}
}

void
idtab_append(idtab_t *idt, idkey_t id)
{
	size_t size;
	void *data;

	if (idt->id_nelems >= idt->id_size) {
		size = idt->id_size ? idt->id_size * IDTAB_GROW : IDTAB_DEFSIZE;

		if (data = realloc(idt->id_data, sizeof (idkey_t) * size)) {
			idt->id_data = data;
			idt->id_size = size;
		} else {
			die(gettext("Failed to grow table"));
		}
	}

	idt->id_data[idt->id_nelems++] = id;
}

static int
idtab_compare(const void *lhsp, const void *rhsp)
{
	idkey_t lhs = *((idkey_t *)lhsp);
	idkey_t rhs = *((idkey_t *)rhsp);

	if (lhs == rhs)
		return (0);

	return (lhs > rhs ? 1 : -1);
}

void
idtab_sort(idtab_t *idt)
{
	if (idt->id_data) {
		qsort(idt->id_data, idt->id_nelems,
		    sizeof (idkey_t), idtab_compare);
	}
}

int
idtab_search(idtab_t *idt, idkey_t id)
{
	return (bsearch(&id, idt->id_data, idt->id_nelems,
	    sizeof (idkey_t), idtab_compare) != NULL);
}
