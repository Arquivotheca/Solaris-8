/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)syncinit.c	1.2	98/04/28 SMI"

#include <string.h>
#include <synch.h>
#include <sys/types.h>
#include <pthread.h>

/*
 * lib[p]thread synch. object initlization interfaces.
 */
static mutex_t _mp = DEFAULTMUTEX;
static cond_t _cv = DEFAULTCV;
static sema_t _sp = DEFAULTSEMA;
static rwlock_t _rwl = DEFAULTRWLOCK;

static pthread_mutex_t _pmp = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t _pcv = PTHREAD_COND_INITIALIZER;
static pthread_rwlock_t _prwl = PTHREAD_RWLOCK_INITIALIZER;

int
_libc_mutex_init(mutex_t *mp, int type, void *arg)
{
	(void) memcpy(mp, &_mp, sizeof (mutex_t));
	return (0);
}

int
_libc_cond_init(cond_t *cvp, int type, void *arg)
{
	(void) memcpy(cvp, &_cv, sizeof (cond_t));
	return (0);
}

int
_libc_sema_init(sema_t *sp, unsigned int count, int type, void *arg)
{
	(void) memcpy(sp, &_sp, sizeof (sema_t));
	return (0);
}

int
_libc_rwlock_init(rwlock_t *rwlp, int type, void *arg)

{
	(void) memcpy(rwlp, &_rwl, sizeof (rwlock_t));
	return (0);
}

int
_libc_pthread_mutex_init(pthread_mutex_t *mutex,  pthread_mutexattr_t *attr)
{
	(void) memcpy(mutex, &_pmp, sizeof (pthread_mutex_t));
	return (0);
}

int
_libc_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
	(void) memcpy(cond, &_pcv, sizeof (pthread_cond_t));
	return (0);
}

int
_libc_pthread_rwlock_init(pthread_rwlock_t *rwlock, pthread_rwlockattr_t *attr)
{
	(void) memcpy(rwlock, &_prwl, sizeof (pthread_rwlock_t));
	return (0);
}
