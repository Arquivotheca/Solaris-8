/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)eventset.c	1.1	99/08/15 SMI"

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libcpc.h>
#include "cpucmds.h"

/*
 * These routines were originally created in an attempt to
 * multiplex several events onto one performance counter in
 * the library.  While we may one day do this, the added complexity
 * of implementing this kind of interface in the kernel doesn't
 * yet seem worth holding up the rest of the effort.
 *
 * So for now, these routines are solely used to manage a list of
 * events, without any multiplexing effect at all.
 *
 * Note that although this is an array-based implementation, the
 * interfaces deliberately allow a list-based implementation.
 * Perhaps cpc_eset_reset() is a bit clunky in that regard.
 */

struct __cpc_eventset {
	struct eset_elem {
		cpc_event_t event;
		char *name;
	} *events;		/* array of events and names */
	int nelem;		/* size of array */
	int cpuver;		/* cpu version */
	int current;		/* currently bound event in eventset */
};

cpc_eventset_t *
cpc_eset_new(int cpuver)
{
	cpc_eventset_t *eset;

	if ((eset = calloc(1, sizeof (*eset))) != NULL) {
		eset->nelem = 0;
		eset->cpuver = cpuver;
		eset->current = -1;
	}
	return (eset);
}

cpc_eventset_t *
cpc_eset_newevent(cpc_eventset_t *eset, const char *spec, int *errcnt)
{
	cpc_event_t event;
	struct eset_elem *new;

	if ((*errcnt = cpc_strtoevent(eset->cpuver, spec, &event)) != 0)
		return (NULL);
	new = realloc(eset->events, (1 + eset->nelem) * sizeof (*new));
	if (new == NULL) {
		*errcnt += 1;
		return (NULL);
	}
	eset->events = new;
	eset->events[eset->nelem].event = event;
	eset->events[eset->nelem].name = cpc_eventtostr(&event);
	if (eset->current < 0)
		eset->current = 0;
	eset->nelem++;
	return (eset);
}

cpc_eventset_t *
cpc_eset_clone(cpc_eventset_t *old)
{
	int i;
	cpc_eventset_t *new;
	struct eset_elem *newa;

	if ((new = calloc(1, sizeof (*new))) == NULL)
		return (NULL);
	if ((newa = calloc(1, old->nelem * sizeof (*newa))) == NULL) {
		free(new);
		return (NULL);
	}

	new->nelem = old->nelem;
	new->cpuver = old->cpuver;
	new->current = old->current;
	new->events = newa;
	for (i = 0; i < old->nelem; i++, newa++) {
		newa->event = old->events[i].event;
		newa->name = strdup(old->events[i].name);
	}
	return (new);
}	       

static void
cpc_eset_delevent(cpc_eventset_t *eset)
{
	int l;

	if ((uint_t)eset->current >= eset->nelem)
		eset->current = eset->nelem - 1;
	if (eset->current < 0)
		return;
	free(eset->events[eset->current].name);
	for (l = eset->current; l < eset->nelem - 1; l++)
		eset->events[l] = eset->events[l + 1];
	eset->nelem--;
}

void
cpc_eset_free(cpc_eventset_t *eset)
{
	if (eset->events) {
		while (eset->nelem)
			cpc_eset_delevent(eset);
		free(eset->events);
	}
	free(eset);
}

cpc_event_t *
cpc_eset_getevent(cpc_eventset_t *eset)
{
	if ((uint_t)eset->current >= eset->nelem)
		return (NULL);
	return (&(eset->events[eset->current].event));
}

const char *
cpc_eset_getname(cpc_eventset_t *eset)
{
	if ((uint_t)eset->current >= eset->nelem)
		return (NULL);
	return (eset->events[eset->current].name);
}

int
cpc_eset_numevents(cpc_eventset_t *eset)
{
	return (eset->nelem);
}

cpc_event_t *
cpc_eset_nextevent(cpc_eventset_t *eset)
{
	if (eset->current < 0)
		return (NULL);
	if (++eset->current >= eset->nelem)
		eset->current = 0;
	return (cpc_eset_getevent(eset));
}

/*
 * Put the eventset pointer back to the beginning of the set
 */
void
cpc_eset_reset(cpc_eventset_t *eset)
{
	if (eset->current > 0)
		eset->current = 0;
}

void
cpc_eset_accum(cpc_eventset_t *accum, cpc_eventset_t *eset)
{
	cpc_event_t *avent, *event, *start;

	cpc_eset_reset(accum);
	cpc_eset_reset(eset);
	if (accum->nelem != eset->nelem)
		return;

	avent = cpc_eset_getevent(accum);
	start = event = cpc_eset_getevent(eset);
	do {
		cpc_event_accum(avent, event);
		avent = cpc_eset_nextevent(accum);
	} while ((event = cpc_eset_nextevent(eset)) != start);
}
