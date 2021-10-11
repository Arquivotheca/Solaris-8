/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma  ident	"@(#)tsdalloc.c 1.14     99/03/10 SMI"

#include <sys/types.h>
#include <thread.h>
#include <synch.h>
#include <errno.h>
#include <stdlib.h>
#include "mtlib.h"
#include "libc.h"
#include "tsd.h"

static void
_free_tsdbuf(_tsdbuf_t *loc)
{
	_tsdbuf_t	*p;
	_tsdbuf_t	*q;

	if (loc) {
		p = loc;
		while (p) {
			q = p->next;
			if (p->buf) {
				free(p->buf);
			}
			free(p);
			p = q;
		}
	}
}

void *
_tsdbufalloc(__tsd_item n, size_t nelem, size_t elsize)
{
	static int		once_key = 0;
	static mutex_t		key_lock = DEFAULTMUTEX;
	static thread_key_t	key;
	_tsdbuf_t		*loc = NULL;
	_tsdbuf_t		*p;
	_tsdbuf_t		*q;

	if (!once_key) {
		(void) _mutex_lock(&key_lock);
		if (!once_key) {
			if (_thr_keycreate(&key,
				(void (*)(void *))_free_tsdbuf) != 0) {
					(void) _mutex_unlock(&key_lock);
					return (NULL);
			}
			once_key++;
		}
		_mutex_unlock(&key_lock);
	}
	if (_thr_getspecific(key, (void **)&loc) != 0) {
		return (NULL);
	}

	if (!loc) {
		/* no memory has been allocated for loc yet */
		loc = (_tsdbuf_t *)calloc((size_t)1, sizeof (_tsdbuf_t));
		if (loc == NULL)
			return (NULL);
		if (_thr_setspecific(key, loc) != 0) {
			free(loc);
			return (NULL);
		}
		q = loc;
	} else {
		q = loc;
		p = loc->next;
		while (p != NULL) {
			if (p->item == n) {
				/* found */
				return (p->buf);
			}
			q = p;
			p = p->next;
		}
	}
	/* create the node for the new TSD */
	p = (_tsdbuf_t *)calloc((size_t)1, sizeof (_tsdbuf_t));
	if (p == NULL)
		return (NULL);

	/*
	 * A switch statement is needed for the near future. To make libc
	 * MT-safe, there more functions need to be calling this function.
	 * Some of the functions require different method of memory allocation.
	 */

	/* no memory has been allocated for this item n yet */
	p->buf = calloc(nelem, elsize);
	if (p->buf == NULL) {
		free(p);
		return (NULL);
	}
	p->item = n;
	q->next = p;
	return (p->buf);
}
