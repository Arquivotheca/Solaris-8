/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)squander.c	1.1	98/12/04 SMI"

#include <unistd.h>
#include <math.h>
#include "stabs.h"

void squander_do_sou(struct tdesc *tdp, struct node *np);
void squander_do_enum(struct tdesc *tdp, struct node *np);
void squander_do_intrinsic(struct tdesc *tdp, struct node *np);

void
squander_do_intrinsic(struct tdesc *tdp, struct node *np)
{
}

void
squander_do_sou(struct tdesc *tdp, struct node *np)
{
	struct mlist *mlp;
	size_t msize = 0;
	unsigned long offset;

	if (np->name == NULL)
		return;
	if (tdp->type == UNION)
		return;

	offset = 0;
	for (mlp = tdp->data.members; mlp != NULL; mlp = mlp->next) {
		if (offset != (mlp->offset / 8)) {
			printf("%lu wasted bytes before %s.%s (%lu, %lu)\n",
			    (mlp->offset / 8) - offset,
			    np->name,
			    mlp->name == NULL ? "(null)" : mlp->name,
			    offset, mlp->offset / 8);
		}
		msize += (mlp->size / 8);
		offset = (mlp->offset / 8) + (mlp->size / 8);
	}

	printf("%s: sizeof: %lu  total: %lu  wasted: %lu\n", np->name,
	    tdp->size, msize, tdp->size - msize);
}

void
squander_do_enum(struct tdesc *tdp, struct node *np)
{
}
