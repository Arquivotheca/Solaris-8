/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tsd.c	1.5	99/08/04 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <thread.h>

/*
 * The following things need to be accessible to libthread so that
 * it can transfer the values to its multi-threaded implementation
 * of the TSD interfaces if/when libthread is dlopen()ed.
 *
 * _libc_tsd_common is a global symbol known to libthread,
 * It is exported as a private interface from libc.
 * Do not change this structure without also changing the code
 * in libthread that knows all about it.
 */

typedef void (*PFrV)(void *);
#define	DELETED	((PFrV)1)

struct _libc_tsd_common {
	uint_t	_keys_used;		/* number of keys in use */
	uint_t	_keys_allocated;	/* number of keys allocated */
	PFrV	*_destructors;		/* array of destructor functions */
	void	**_values;		/* array of key values */
} _libc_tsd_common;

#define	keys_used	_libc_tsd_common._keys_used
#define	keys_allocated	_libc_tsd_common._keys_allocated
#define	destructors	_libc_tsd_common._destructors
#define	values		_libc_tsd_common._values

static void
keys_destruct(void)
{
	int i;

	for (i = 0; i < keys_used; i++) {
		if (values[i] != NULL && destructors[i] != NULL) {
			if (destructors[i] != DELETED)
				(destructors[i])(values[i]);
			values[i] = NULL;
		}
	}
}

/* public interfaces */
int
_libc_thr_keycreate(thread_key_t *pkey, PFrV destruct)
{
	PFrV *dp;
	void **vp;
	uint_t nkeys;

	/*
	 * If this is the first call to thr_keycreate(),
	 * then the destructor routine has not been registered yet.
	 */
	if (destructors == NULL)
		(void) atexit(keys_destruct);

	if (keys_used >= keys_allocated) {
		if (keys_allocated == 0) {
			nkeys = 1;
		} else {
			/*
			 * Reallocate, doubling size.
			 */
			nkeys = keys_allocated * 2;
		}
		dp = realloc(destructors, nkeys * sizeof (PFrV));
		vp = realloc(values, nkeys * sizeof (void *));
		if (dp == NULL || vp == NULL)	/* the world is ending */
			return (ENOMEM);
		keys_allocated = nkeys;
		destructors = dp;
		values = vp;
	}

	/* key index is the key value minus one, since 0 is an invalid key */
	destructors[keys_used] = destruct;
	values[keys_used] = NULL;
	*pkey = ++keys_used;
	return (0);
}


int
_libc_thr_key_delete(thread_key_t key)
{
	/* check for out-of-range key */
	if (key == 0 || key > keys_used)
		return (EINVAL);

	/* check for already deleted key */
	if (destructors[key-1] == DELETED)
		return (EINVAL);

	destructors[key-1] = DELETED;

	return (0);
}


int
_libc_thr_getspecific(thread_key_t key, void **valuep)
{
	/* check for out-of-range key */
	if (key == 0 || key > keys_used)
		return (EINVAL);

	/* check for deleted key */
	if (destructors[key-1] == DELETED)
		return (EINVAL);

	*valuep = values[key-1];
	return (0);
}


int
_libc_thr_setspecific(thread_key_t key, void *value)
{
	/* check for out-of-range key */
	if (key == 0 || key > keys_used)
		return (EINVAL);

	/* check for deleted key */
	if (destructors[key-1] == DELETED)
		return (EINVAL);

	values[key-1] = value;
	return (0);
}

void *
_libc_pthread_getspecific(thread_key_t key)
{
	void *value;

	if (_libc_thr_getspecific(key, &value) != 0)
		return (NULL);
	else
		return (value);
}
