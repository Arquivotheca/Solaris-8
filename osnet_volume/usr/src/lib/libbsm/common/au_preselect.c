#ifndef lint
static char sccsid[] = "@(#)au_preselect.c 1.8 97/10/29 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

/*
 * au_preselect.c
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <bsm/audit.h>
#include <bsm/libbsm.h>
#include <synch.h>

#define	ALLOC_INIT (600)	/* initially allocate ALLOC_INIT map entries */
#define	ALLOC_INCR (100)	/* if more map entries are needed, realloc */
				/* in ALLOC_INCR increments */

static int alloc_map();
static int load_map();
static int realloc_map();

typedef struct event_map {
	au_event_t event;	/* audit event number */
	au_class_t class;	/* audit event class mask */
} event_map_t;

static event_map_t *event_map;	/* the map */
static u_int alloc_count;	/* number of entries currently allocated */
static u_int event_count;	/* number of entries in map */
static mutex_t mutex_au_preselect = DEFAULTMUTEX;

extern int _mutex_lock(mutex_t *);
extern int _mutex_unlock(mutex_t *);

/*
 * au_preselect:
 *
 * Keep a dynamic array of event<-->class mappings.
 * Refresh the map when the value of flag is non-zero.
 * Return:
 *	1: The event is preselected.
 *	0: The event is not preselected.
 *	-1: There was an error:
 *		Couldn't allocate memory.
 *		Couldn't find event.
 */
int
#ifdef __STDC__
au_preselect(au_event_t au_event, au_mask_t *au_mask_p, int sorf, int flag)
#else
au_preselect(au_event, au_mask_p, sorf, flag)
	au_event_t au_event;	/* event */
	au_mask_t *au_mask_p;	/* preselection mask */
	int sorf;		/* success or failure */
	int flag;		/* re-read flag */
#endif /* __STDC__ */
{
	static char been_here_before;  /* we cache the map */
	register int i;
	register au_class_t comp_class;

	_mutex_lock(&mutex_au_preselect);
	if (!been_here_before) {
		if (alloc_map() == -1) {
			_mutex_unlock(&mutex_au_preselect);
			return (-1);
		}

		if (load_map() == -1) {
			_mutex_unlock(&mutex_au_preselect);
			return (-1);
		}

		been_here_before = 1;
	}

	/*
	 * Don't use the cache. Re-read the audit_event(5) db every time
	 */
	if (flag == AU_PRS_REREAD) {
		if (load_map() == -1) {
			_mutex_unlock(&mutex_au_preselect);
			return (-1);
		}
	}

	/* Determine what portion of the preselection mask to check. */
	if (sorf == AU_PRS_SUCCESS)
		comp_class = au_mask_p->am_success;
	else if (sorf == AU_PRS_FAILURE)
		comp_class = au_mask_p->am_failure;
	else
		comp_class = au_mask_p->am_success | au_mask_p->am_failure;

	for (i = 0; i < event_count; i++) {
		if (event_map[i].event == au_event) {
			if (event_map[i].class & comp_class) {
				_mutex_unlock(&mutex_au_preselect);
				return (1);
			} else {
				_mutex_unlock(&mutex_au_preselect);
				return (0);
			}
		}
	}

	_mutex_unlock(&mutex_au_preselect);
	return (-1);	/* could not find event in the table */
}

/*
 * Initially allocate about as many map entries as are there
 * are audit events shipped with the system. For sites
 * that don't add audit events, this should be enough.
 */
static int
alloc_map()
{
	if ((event_map = (event_map_t *)
		calloc(ALLOC_INIT, (size_t)sizeof (event_map_t))) ==
		(event_map_t *)NULL)
		return (-1);
	else
		alloc_count = ALLOC_INIT;

	return (0);
}

/*
 * load the event<->class map into memory
 */
static int
load_map()
{
	register au_event_ent_t *evp;

	event_count = 0;
	setauevent();
	while ((evp = getauevent()) != (au_event_ent_t *)NULL){
		if (event_count > alloc_count)
			if (realloc_map() == -1) {
				endauevent();
				return (-1);
			}
		event_map[event_count].event = evp->ae_number;
		event_map[event_count].class = evp->ae_class;
		++event_count;
	}
	endauevent();

	return (0);
}

/*
 * realloc the event map in ALLOC_INCR increments
 */
static int
realloc_map()
{
	register size_t rsize;
	rsize = sizeof (event_map_t) * (alloc_count + ALLOC_INCR);

	if ((event_map = (event_map_t *)
		realloc(event_map, rsize)) == (event_map_t *)NULL)
		return (-1);

	return (0);
}
