/*
 * Copyright (c) 1993-1994 by Sun Microsystems Inc.
 */

#ident	"@(#)common.c	1.4	97/08/22 SMI"	/* SVr4.0 1.3	*/

#include <stdlib.h>
#include <sys/types.h>
#include <thread.h>
#include <synch.h>

extern int _thr_getspecific();
extern int _thr_setspecific();

int *
_t_tsdalloc(thread_key_t *key, int size)
{
	int *loc = 0;
	extern mutex_t tsd_lock;

	if (*key == 0) {
		mutex_lock(&tsd_lock);
		if (*key == 0)
			(void) thr_keycreate(key, free);
		mutex_unlock(&tsd_lock);
	}

	if (*key == 0)
		return (0);

	(void) _thr_getspecific(*key, &loc);
	if (!loc) {
		if (_thr_setspecific(*key,
		    (void *)(loc = (int *)malloc(size))) != 0) {
			if (loc)
				(void) free(loc);
			return (0);
		}
		*loc = 0;		/* initialize to zero */
	}
	return (loc);
}
