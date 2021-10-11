/*
 * Copyright (c) 1990-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)printlist.c	1.4	97/07/28 SMI"

#include "gprof.h"

/*
 *	these are the lists of names:
 *	there is the list head and then the listname
 *	is a pointer to the list head
 *	(for ease of passing to stringlist functions).
 */
struct stringlist	fhead = { 0, 0 };
struct stringlist	*flist = &fhead;
struct stringlist	Fhead = { 0, 0 };
struct stringlist	*Flist = &Fhead;
struct stringlist	ehead = { 0, 0 };
struct stringlist	*elist = &ehead;
struct stringlist	Ehead = { 0, 0 };
struct stringlist	*Elist = &Ehead;

void
addlist(struct stringlist *listp, char *funcname)
{
	struct stringlist	*slp;

	slp = (struct stringlist *)malloc(sizeof (struct stringlist));

	if (slp == NULL) {
		fprintf(stderr, "gprof: ran out room for printlist\n");
		exit(1);
	}

	slp->next = listp->next;
	slp->string = funcname;
	listp->next = slp;
}

bool
onlist(struct stringlist *listp, char *funcname)
{
	struct stringlist	*slp;

	for (slp = listp->next; slp; slp = slp->next) {
		if (strcmp(slp->string, funcname) == 0)
			return (TRUE);

		if (funcname[0] == '_' &&
		    strcmp(slp->string, &funcname[1]) == 0)
			return (TRUE);
	}
	return (FALSE);
}
