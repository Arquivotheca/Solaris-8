/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pthr_cond.c	1.2	99/11/02 SMI"

#include "liblwp.h"

/*
 * pthread_condattr_init: allocates the cond attribute object and
 * initializes it with the default values.
 */
#pragma weak pthread_condattr_init = _pthread_condattr_init
#pragma weak _liblwp_pthread_condattr_init = _pthread_condattr_init
int
_pthread_condattr_init(pthread_condattr_t *attr)
{
	cvattr_t *ap;

	if ((ap = malloc(sizeof (cvattr_t))) == NULL)
		return (ENOMEM);
	ap->pshared = DEFAULT_TYPE;
	attr->__pthread_condattrp = ap;
	return (0);
}

/*
 * pthread_condattr_destroy: frees the cond attribute object and
 * invalidates it with NULL value.
 */
#pragma weak pthread_condattr_destroy = _pthread_condattr_destroy
#pragma weak _liblwp_pthread_condattr_destroy = _pthread_condattr_destroy
int
_pthread_condattr_destroy(pthread_condattr_t *attr)
{
	if (attr == NULL || attr->__pthread_condattrp == NULL)
		return (EINVAL);
	liblwp_free(attr->__pthread_condattrp);
	attr->__pthread_condattrp = NULL;
	return (0);
}

/*
 * pthread_condattr_setpshared: sets the shared attr to PRIVATE or SHARED.
 * This is equivalent to setting USYNC_PROCESS/USYNC_THREAD flag in cond_init().
 */
#pragma weak pthread_condattr_setpshared = _pthread_condattr_setpshared
#pragma weak _liblwp_pthread_condattr_setpshared = _pthread_condattr_setpshared
int
_pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared)
{
	cvattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_condattrp) != NULL &&
	    (pshared == PTHREAD_PROCESS_PRIVATE ||
	    pshared == PTHREAD_PROCESS_SHARED)) {
		ap->pshared = pshared;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_condattr_getpshared: gets the shared attr.
 */
#pragma weak pthread_condattr_getpshared = _pthread_condattr_getpshared
#pragma weak _liblwp_pthread_condattr_getpshared = _pthread_condattr_getpshared
int
_pthread_condattr_getpshared(const pthread_condattr_t *attr, int *pshared)
{
	cvattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_condattrp) != NULL &&
	    pshared != NULL) {
		*pshared = ap->pshared;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_cond_init: Initializes the cond object. It copies the
 * pshared attr into type argument and calls cond_init().
 */
#pragma weak pthread_cond_init = _pthread_cond_init
#pragma weak _liblwp_pthread_cond_init = _pthread_cond_init
int
_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
	cvattr_t *ap;
	int type;

	if (attr == NULL)
		type = DEFAULT_TYPE;
	else if ((ap = attr->__pthread_condattrp) != NULL)
		type = ap->pshared;
	else
		return (EINVAL);

	return (_cond_init((cond_t *)cond, type, NULL));
}
