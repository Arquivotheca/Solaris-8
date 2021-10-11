/*	Copyright (c) 1997, by Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)pthr_rwlock.c	1.1	97/12/01 SMI"

#ifdef __STDC__

#pragma weak	pthread_rwlockattr_init = _pthread_rwlockattr_init
#pragma weak	pthread_rwlockattr_destroy =  _pthread_rwlockattr_destroy
#pragma weak	pthread_rwlockattr_setpshared =  _pthread_rwlockattr_setpshared
#pragma weak	pthread_rwlockattr_getpshared =  _pthread_rwlockattr_getpshared
#pragma weak	pthread_rwlock_init = _pthread_rwlock_init


#pragma	weak	_ti_pthread_rwlock_init = _pthread_rwlock_init
#pragma weak	_ti_pthread_rwlockattr_destroy =  _pthread_rwlockattr_destroy
#pragma weak	_ti_pthread_rwlockattr_getpshared = \
					_pthread_rwlockattr_getpshared
#pragma weak	_ti_pthread_rwlockattr_init = _pthread_rwlockattr_init
#pragma weak	_ti_pthread_rwlockattr_setpshared = \
			_pthread_rwlockattr_setpshared
#endif /* __STDC__ */

#include "libpthr.h"
#include "libthread.h"

/*
 * UNIX98
 * pthread_rwlockattr_init: allocates the mutex attribute object and
 * initializes it with the default values.
 */
int
_pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
{
	rwlattr_t	*ap;

	if ((ap = (rwlattr_t *)_alloc_attr(sizeof (rwlattr_t))) != NULL) {
		ap->pshared = DEFAULT_TYPE;
		attr->__pthread_rwlockattrp = ap;
		return (0);
	} else
		return (ENOMEM);
}

/*
 * UNIX98
 * pthread_rwlockattr_destroy: frees the rwlock attribute object and
 * invalidates it with NULL value.
 */
int
_pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
	if (attr == NULL || attr->__pthread_rwlockattrp == NULL ||
			_free_attr(attr->__pthread_rwlockattrp) < 0)
		return (EINVAL);
	attr->__pthread_rwlockattrp = NULL;
	return (0);
}

/*
 * UNIX98
 * pthread_rwlockattr_setpshared: sets the shared attr to PRIVATE or
 * SHARED.
 */
int
_pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr,
						int pshared)
{
	rwlattr_t	*ap;


	if (attr != NULL && (ap = attr->__pthread_rwlockattrp) != NULL &&
		(pshared == PTHREAD_PROCESS_PRIVATE ||
			pshared == PTHREAD_PROCESS_SHARED)) {
		ap->pshared = pshared;
		return (0);
	} else {
		return (EINVAL);
	}
}

/*
 * UNIX98
 * pthread_rwlockattr_getpshared: gets the shared attr.
 */
int
_pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr,
						int *pshared)
{
	rwlattr_t	*ap;

	if (pshared != NULL && attr != NULL &&
			(ap = attr->__pthread_rwlockattrp) != NULL) {
		*pshared = ap->pshared;
		return (0);
	} else {
		return (EINVAL);
	}
}

/*
 * UNIX98
 * pthread_rwlock_init: Initializes the rwlock object. It copies the
 * pshared attr into type argument and calls rwlock_init().
 */
int
_pthread_rwlock_init(pthread_rwlock_t *rwlock, pthread_rwlockattr_t *attr)
{
	int	type;

	if (attr != NULL) {
		if (attr->__pthread_rwlockattrp == NULL)
			return (EINVAL);

		type = ((rwlattr_t *)attr->__pthread_rwlockattrp)->pshared;
	} else {
		type = DEFAULT_TYPE;
	}

	return (_rwlock_init((rwlock_t *) rwlock, type, NULL));
}
