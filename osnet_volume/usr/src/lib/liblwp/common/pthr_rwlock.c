/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pthr_rwlock.c	1.1	99/10/14 SMI"

#include "liblwp.h"

/*
 * UNIX98
 * pthread_rwlockattr_init: allocates the mutex attribute object and
 * initializes it with the default values.
 */
#pragma weak pthread_rwlockattr_init = _pthread_rwlockattr_init
#pragma weak _liblwp_pthread_rwlockattr_init = _pthread_rwlockattr_init
int
_pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
{
	rwlattr_t *ap;

	if ((ap = malloc(sizeof (rwlattr_t))) == NULL)
		return (ENOMEM);
	ap->pshared = DEFAULT_TYPE;
	attr->__pthread_rwlockattrp = ap;
	return (0);
}

/*
 * UNIX98
 * pthread_rwlockattr_destroy: frees the rwlock attribute object and
 * invalidates it with NULL value.
 */
#pragma weak pthread_rwlockattr_destroy =  _pthread_rwlockattr_destroy
#pragma weak _liblwp_pthread_rwlockattr_destroy = _pthread_rwlockattr_destroy
int
_pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
	if (attr == NULL || attr->__pthread_rwlockattrp == NULL)
		return (EINVAL);
	liblwp_free(attr->__pthread_rwlockattrp);
	attr->__pthread_rwlockattrp = NULL;
	return (0);
}

/*
 * UNIX98
 * pthread_rwlockattr_setpshared: sets the shared attr to PRIVATE or SHARED.
 */
#pragma weak pthread_rwlockattr_setpshared =  _pthread_rwlockattr_setpshared
#pragma weak _liblwp_pthread_rwlockattr_setpshared = \
				_pthread_rwlockattr_setpshared
int
_pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int pshared)
{
	rwlattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_rwlockattrp) != NULL &&
	    (pshared == PTHREAD_PROCESS_PRIVATE ||
	    pshared == PTHREAD_PROCESS_SHARED)) {
		ap->pshared = pshared;
		return (0);
	}
	return (EINVAL);
}

/*
 * UNIX98
 * pthread_rwlockattr_getpshared: gets the shared attr.
 */
#pragma weak pthread_rwlockattr_getpshared =  _pthread_rwlockattr_getpshared
#pragma weak _liblwp_pthread_rwlockattr_getpshared = \
				_pthread_rwlockattr_getpshared
int
_pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr, int *pshared)
{
	rwlattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_rwlockattrp) != NULL &&
	    pshared != NULL) {
		*pshared = ap->pshared;
		return (0);
	}
	return (EINVAL);
}

/*
 * UNIX98
 * pthread_rwlock_init: Initializes the rwlock object. It copies the
 * pshared attr into type argument and calls rwlock_init().
 */
#pragma weak pthread_rwlock_init = _pthread_rwlock_init
#pragma weak _liblwp_pthread_rwlock_init = _pthread_rwlock_init
int
_pthread_rwlock_init(pthread_rwlock_t *rwlock, pthread_rwlockattr_t *attr)
{
	rwlattr_t *ap;
	int type;

	if (attr == NULL)
		type = DEFAULT_TYPE;
	else if ((ap = attr->__pthread_rwlockattrp) != NULL)
		type = ap->pshared;
	else
		return (EINVAL);

	return (_rwlock_init((rwlock_t *)rwlock, type, NULL));
}
