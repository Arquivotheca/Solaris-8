/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pthr_attr.c	1.2	99/11/02 SMI"

#define	_POSIX_C_SOURCE	199506L
#include <limits.h>
#undef	_POSIX_C_SOURCE
#include "liblwp.h"
#include <sched.h>

/*
 * pthread_attr_init: allocates the attribute object and initializes it
 * with the default values.
 */
#pragma weak pthread_attr_init = _pthread_attr_init
#pragma weak _liblwp_pthread_attr_init = _pthread_attr_init
int
_pthread_attr_init(pthread_attr_t *attr)
{
	thrattr_t *ap;

	if ((ap = malloc(sizeof (thrattr_t))) != NULL) {
		ap->stksize = 0;
		ap->stkaddr = NULL;
		ap->prio = 0;
		ap->policy = SCHED_OTHER;
		ap->inherit = PTHREAD_EXPLICIT_SCHED;
		ap->detachstate = PTHREAD_CREATE_JOINABLE;
		ap->scope = PTHREAD_SCOPE_PROCESS;
		ap->guardsize = _lpagesize;
		attr->__pthread_attrp = ap;
		return (0);
	}
	return (ENOMEM);
}

/*
 * pthread_attr_destroy: frees the attribute object and invalidates it
 * with NULL value.
 */
#pragma weak pthread_attr_destroy = _pthread_attr_destroy
#pragma weak _liblwp_pthread_attr_destroy = _pthread_attr_destroy
int
_pthread_attr_destroy(pthread_attr_t *attr)
{
	if (attr == NULL || attr->__pthread_attrp == NULL)
		return (EINVAL);
	liblwp_free(attr->__pthread_attrp);
	attr->__pthread_attrp = NULL;
	return (0);
}

/*
 * pthread_attr_setstacksize: sets the user stack size, minimum should
 * be PTHREAD_STACK_MIN.
 * This is equivalent to stksize argument in thr_create().
 */
#pragma weak pthread_attr_setstacksize = _pthread_attr_setstacksize
#pragma weak _liblwp_pthread_attr_setstacksize = _pthread_attr_setstacksize
int
_pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    stacksize >= PTHREAD_STACK_MIN) {
		ap->stksize = stacksize;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getstacksize: gets the user stack size.
 */
#pragma weak pthread_attr_getstacksize = _pthread_attr_getstacksize
#pragma weak _liblwp_pthread_attr_getstacksize = _pthread_attr_getstacksize
int
_pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    stacksize != NULL) {
		*stacksize = ap->stksize;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setstackaddr: sets the user stack addr.
 * This is equivalent to stkaddr argument in thr_create().
 */
#pragma weak pthread_attr_setstackaddr = _pthread_attr_setstackaddr
#pragma weak _liblwp_pthread_attr_setstackaddr = _pthread_attr_setstackaddr
int
_pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL) {
		ap->stkaddr = stackaddr;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getstackaddr: gets the user stack addr.
 */
#pragma weak pthread_attr_getstackaddr = _pthread_attr_getstackaddr
#pragma weak _liblwp_pthread_attr_getstackaddr = _pthread_attr_getstackaddr
int
_pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    stackaddr != NULL) {
		*stackaddr = ap->stkaddr;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setdetachstate: sets the detach state to JOINABLE or
 * DETACHED.
 * This is equivalent to setting THR_DETACHED flag in thr_create().
 */
#pragma weak pthread_attr_setdetachstate = _pthread_attr_setdetachstate
#pragma weak _liblwp_pthread_attr_setdetachstate = _pthread_attr_setdetachstate
int
_pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    (detachstate == PTHREAD_CREATE_DETACHED ||
	    detachstate == PTHREAD_CREATE_JOINABLE)) {
		ap->detachstate = detachstate;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getdetachstate: gets the detach state.
 */
#pragma weak pthread_attr_getdetachstate = _pthread_attr_getdetachstate
#pragma weak _liblwp_pthread_attr_getdetachstate = _pthread_attr_getdetachstate
int
_pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    detachstate != NULL) {
		*detachstate = ap->detachstate;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setscope: sets the scope to SYSTEM or PROCESS.
 * This is equivalent to setting THR_BOUND flag in thr_create().
 */
#pragma weak pthread_attr_setscope = _pthread_attr_setscope
#pragma weak _liblwp_pthread_attr_setscope = _pthread_attr_setscope
int
_pthread_attr_setscope(pthread_attr_t *attr, int scope)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    (scope == PTHREAD_SCOPE_SYSTEM ||
	    scope == PTHREAD_SCOPE_PROCESS)) {
		ap->scope = scope;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getscope: gets the scheduling scope.
 */
#pragma weak pthread_attr_getscope = _pthread_attr_getscope
#pragma weak _liblwp_pthread_attr_getscope = _pthread_attr_getscope
int
_pthread_attr_getscope(const pthread_attr_t *attr, int *scope)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    scope != NULL) {
		*scope = ap->scope;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setinheritsched: sets the scheduling parameters to be
 * EXPLICIT or INHERITED from parent thread.
 */
#pragma weak pthread_attr_setinheritsched = _pthread_attr_setinheritsched
#pragma weak _liblwp_pthread_attr_setinheritsched = \
				_pthread_attr_setinheritsched
int
_pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    (inherit == PTHREAD_EXPLICIT_SCHED ||
	    inherit == PTHREAD_INHERIT_SCHED)) {
		ap->inherit = inherit;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getinheritsched: gets the scheduling inheritance.
 */
#pragma weak pthread_attr_getinheritsched = _pthread_attr_getinheritsched
#pragma weak _liblwp_pthread_attr_getinheritsched = \
				_pthread_attr_getinheritsched
int
_pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inherit)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    inherit != NULL) {
		*inherit = ap->inherit;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setschedpolicy: sets the scheduling policy to SCHED_RR,
 * SCHED_FIFO or SCHED_OTHER.
 */
#pragma weak pthread_attr_setschedpolicy = _pthread_attr_setschedpolicy
#pragma weak _liblwp_pthread_attr_setschedpolicy = _pthread_attr_setschedpolicy
int
_pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    (policy == SCHED_OTHER ||
	    policy == SCHED_FIFO ||
	    policy == SCHED_RR)) {
		ap->policy = policy;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_getpolicy: gets the scheduling policy.
 */
#pragma weak pthread_attr_getschedpolicy = _pthread_attr_getschedpolicy
#pragma weak _liblwp_pthread_attr_getschedpolicy = _pthread_attr_getschedpolicy
int
_pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    policy != NULL) {
		*policy = ap->policy;
		return (0);
	}
	return (EINVAL);
}

/*
 * pthread_attr_setschedparam: sets the scheduling parameters.
 * Currently, we support priority only.
 */
#pragma weak pthread_attr_setschedparam = _pthread_attr_setschedparam
#pragma weak _liblwp_pthread_attr_setschedparam = _pthread_attr_setschedparam
int
_pthread_attr_setschedparam(pthread_attr_t *attr,
	const struct sched_param *param)
{
	thrattr_t *ap;
	int	policy;
	int	pri;

	if (attr == NULL || (ap = attr->__pthread_attrp) == NULL)
		return (EINVAL);

	policy = ap->policy;
	pri = param->sched_priority;
	if (policy == SCHED_OTHER) {
		if ((pri < THREAD_MIN_PRIORITY || pri > THREAD_MAX_PRIORITY) &&
		    _validate_rt_prio(policy, pri))
			return (EINVAL);
	} else if (_validate_rt_prio(policy, pri)) {
		return (EINVAL);
	}
	ap->prio = pri;
	return (0);
}

/*
 * pthread_attr_getschedparam: gets the scheduling parameters.
 * Currently, only priority is defined as sched parameter.
 */
#pragma weak pthread_attr_getschedparam = _pthread_attr_getschedparam
#pragma weak _liblwp_pthread_attr_getschedparam = _pthread_attr_getschedparam
int
_pthread_attr_getschedparam(const pthread_attr_t *attr,
					struct sched_param *param)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    param != NULL) {
		param->sched_priority = ap->prio;
		return (0);
	}
	return (EINVAL);
}

/*
 * UNIX98
 * pthread_attr_setguardsize: sets the guardsize
 */
#pragma weak pthread_attr_setguardsize = _pthread_attr_setguardsize
#pragma weak _liblwp_pthread_attr_setguardsize = _pthread_attr_setguardsize
int
_pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL) {
		ap->guardsize = guardsize;
		return (0);
	}
	return (EINVAL);
}

/*
 * UNIX98
 * pthread_attr_getguardsize: gets the guardsize
 */
#pragma weak pthread_attr_getguardsize = _pthread_attr_getguardsize
#pragma weak _liblwp_pthread_attr_getguardsize = _pthread_attr_getguardsize
int
_pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize)
{
	thrattr_t *ap;

	if (attr != NULL && (ap = attr->__pthread_attrp) != NULL &&
	    guardsize != NULL) {
		*guardsize = ap->guardsize;
		return (0);
	}
	return (EINVAL);
}
