/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)pthr_cond.c	1.12	97/08/21 SMI"

#ifdef __STDC__

#pragma weak	pthread_condattr_init = _pthread_condattr_init
#pragma weak	pthread_condattr_destroy = _pthread_condattr_destroy
#pragma weak	pthread_condattr_setpshared = _pthread_condattr_setpshared
#pragma weak	pthread_condattr_getpshared = _pthread_condattr_getpshared
#pragma weak	pthread_cond_init = _pthread_cond_init

#pragma weak	_ti_pthread_cond_init = _pthread_cond_init
#pragma weak	_ti_pthread_condattr_destroy = _pthread_condattr_destroy
#pragma weak	_ti_pthread_condattr_getpshared = _pthread_condattr_getpshared
#pragma weak	_ti_pthread_condattr_init = _pthread_condattr_init
#pragma weak	_ti_pthread_condattr_setpshared = _pthread_condattr_setpshared

#endif /* __STDC__ */

#include "libpthr.h"
#include "libthread.h"


/*
 * POSIX.1c
 * pthread_condattr_init: allocates the cond attribute object and
 * initializes it with the default values.
 */
int
_pthread_condattr_init(pthread_condattr_t *attr)
{
	cvattr_t	*ap;

	if ((ap = (cvattr_t *)_alloc_attr(sizeof (cvattr_t))) != NULL) {
		ap->pshared = DEFAULT_TYPE;
		attr->__pthread_condattrp = ap;
		return (0);
	} else
		return (ENOMEM);
}

/*
 * POSIX.1c
 * pthread_condattr_destroy: frees the cond attribute object and
 * invalidates it with NULL value.
 */
int
_pthread_condattr_destroy(pthread_condattr_t *attr)
{
	if (attr == NULL || attr->__pthread_condattrp == NULL ||
			_free_attr(attr->__pthread_condattrp) < 0)
		return (EINVAL);
	attr->__pthread_condattrp = NULL;
	return (0);
}

/*
 * POSIX.1c
 * pthread_condattr_setpshared: sets the shared attr to PRIVATE or
 * SHARED.
 * This is equivalent to setting USYNC_PROCESS/USYNC_THREAD flag in
 * cond_init().
 */
int
_pthread_condattr_setpshared(pthread_condattr_t *attr,
						int pshared)
{
	cvattr_t	*ap;


	if (attr != NULL && (ap = attr->__pthread_condattrp) != NULL &&
		(pshared == PTHREAD_PROCESS_PRIVATE ||
			pshared == PTHREAD_PROCESS_SHARED)) {
		ap->pshared = pshared;
		return (0);
	} else {
		return (EINVAL);
	}
}

/*
 * POSIX.1c
 * pthread_condattr_getpshared: gets the shared attr.
 */
int
_pthread_condattr_getpshared(const pthread_condattr_t *attr,
						int *pshared)
{
	cvattr_t	*ap;

	if (pshared != NULL && attr != NULL &&
			(ap = attr->__pthread_condattrp) != NULL) {
		*pshared = ap->pshared;
		return (0);
	} else {
		return (EINVAL);
	}
}

/*
 * POSIX.1c
 * pthread_cond_init: Initializes the cond object. It copies the
 * pshared attr into type argument and calls cond_init().
 */
int
_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
	int	type;

	if (attr != NULL) {
		if (attr->__pthread_condattrp == NULL)
			return (EINVAL);

		type = ((cvattr_t *)attr->__pthread_condattrp)->pshared;
	} else {
		type = DEFAULT_TYPE;
	}

	return (_cond_init((cond_t *) cond, type, NULL));
}
