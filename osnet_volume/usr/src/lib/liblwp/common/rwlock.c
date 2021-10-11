/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rwlock.c	1.3	99/11/02 SMI"

#include "liblwp.h"

/*
 * Check if a reader version of the lock is held.
 */
#pragma weak rw_read_held = _rw_read_held
#pragma weak _liblwp_rw_read_held = _rw_read_held
int
_rw_read_held(rwlock_t *rwlp)
{
	return (rwlp->readers > 0);
}

/*
 * Check if a writer version of the lock is held.
 */
#pragma weak rw_write_held = _rw_write_held
#pragma weak _liblwp_rw_write_held = _rw_write_held
int
_rw_write_held(rwlock_t *rwlp)
{
	/* LINTED pointer cast */
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);

	return (rwlp->readers == -1 && _mutex_held(rwlock));
}

#pragma weak rwlock_init = _rwlock_init
#pragma weak _liblwp_rwlock_init = _rwlock_init
/* ARGSUSED2 */
int
_rwlock_init(rwlock_t *rwlp, int type, void *arg)
{
	/* LINTED pointer cast */
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	/* LINTED pointer cast */
	cond_t *readers = (cond_t *)(rwlp->pad2);
	/* LINTED pointer cast */
	cond_t *writers = (cond_t *)(rwlp->pad3);

	if (type != USYNC_THREAD && type != USYNC_PROCESS)
		return (EINVAL);
	(void) _memset(rwlp, 0, sizeof (*rwlp));
	rwlp->type = (uint16_t)type;
	rwlp->readers = 0;
	rwlock->mutex_type = (uint8_t)type;
	rwlock->mutex_flag = LOCK_INITED;
	readers->cond_type = (uint16_t)type;
	writers->cond_type = (uint16_t)type;
	(void) RWLOCK_STATS(rwlp);
	return (0);
}

#pragma weak rwlock_destroy = _rwlock_destroy
#pragma weak _liblwp_rwlock_destroy = _rwlock_destroy
#pragma weak pthread_rwlock_destroy = _rwlock_destroy
#pragma weak _pthread_rwlock_destroy = _rwlock_destroy
#pragma weak _liblwp_pthread_rwlock_destroy = _rwlock_destroy
int
_rwlock_destroy(rwlock_t *rwlp)
{
	rwlp->magic = 0;
	tdb_sync_obj_deregister(rwlp);
	return (0);
}

#pragma weak rw_rdlock = _rw_rdlock
#pragma weak _liblwp_rw_rdlock = _rw_rdlock
#pragma weak pthread_rwlock_rdlock = _rw_rdlock
#pragma weak _pthread_rwlock_rdlock = _rw_rdlock
#pragma weak _liblwp_pthread_rwlock_rdlock = _rw_rdlock
int
_rw_rdlock(rwlock_t *rwlp)
{
	/* LINTED pointer cast */
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	/* LINTED pointer cast */
	cond_t *readers = (cond_t *)(rwlp->pad2);
	/* LINTED pointer cast */
	cond_t *writers = (cond_t *)(rwlp->pad3);
	tdb_rwlock_stats_t *rwsp;

	(void) mutex_lock_internal(rwlock, rwlp, MUTEX_LOCK);
	while (rwlp->readers < 0 || CVWAITERS(writers))
		(void) cond_wait_internal(readers, rwlock, rwlp);
	rwlp->readers++;
	ASSERT(rwlp->readers > 0);
	if ((rwsp = RWLOCK_STATS(rwlp)) != NULL)
		tdb_incr(rwsp->rw_rdlock);
	(void) _mutex_unlock(rwlock);
	curthread->ul_rdlock++;
	return (0);
}

#pragma weak rw_wrlock = _rw_wrlock
#pragma weak _liblwp_rw_wrlock = _rw_wrlock
#pragma weak pthread_rwlock_wrlock = _rw_wrlock
#pragma weak _pthread_rwlock_wrlock = _rw_wrlock
#pragma weak _liblwp_pthread_rwlock_wrlock = _rw_wrlock
int
_rw_wrlock(rwlock_t *rwlp)
{
	/* LINTED pointer cast */
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	/* LINTED pointer cast */
	cond_t *writers = (cond_t *)(rwlp->pad3);
	tdb_rwlock_stats_t *rwsp;

	(void) mutex_lock_internal(rwlock, rwlp, MUTEX_LOCK);
	while (rwlp->readers != 0)
		(void) cond_wait_internal(writers, rwlock, rwlp);
	rwlp->readers = -1;
	if ((rwsp = RWLOCK_STATS(rwlp)) != NULL) {
		tdb_incr(rwsp->rw_wrlock);
		rwsp->rw_wrlock_begin_hold = gethrtime();
	}
	curthread->ul_wrlock++;
	/*
	 * We return with the mutex held.
	 * This is ensures fork1() safety, among other things.
	 */
	return (0);
}

#pragma weak rw_unlock = _rw_unlock
#pragma weak _liblwp_rw_unlock = _rw_unlock
#pragma weak pthread_rwlock_unlock = _rw_unlock
#pragma weak _pthread_rwlock_unlock = _rw_unlock
#pragma weak _liblwp_pthread_rwlock_unlock = _rw_unlock
int
_rw_unlock(rwlock_t *rwlp)
{
	/* LINTED pointer cast */
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	/* LINTED pointer cast */
	cond_t *readers = (cond_t *)(rwlp->pad2);
	/* LINTED pointer cast */
	cond_t *writers = (cond_t *)(rwlp->pad3);
	tdb_rwlock_stats_t *rwsp;

	ASSERT(rwlp->readers != 0);
	if (rwlp->readers < 0) {
		/*
		 * Since the writer lock is held, we are holding
		 * it, else we could not legitimately be here.
		 * This means we are also holding the embedded mutex.
		 */
		ASSERT(_mutex_held(rwlock));
		if (CVWAITERS(writers))
			(void) cond_signal_internal(writers, rwlp);
		else if (CVWAITERS(readers))
			(void) cond_broadcast_internal(readers, rwlp);
		if ((rwsp = RWLOCK_STATS(rwlp)) != NULL) {
			if (rwsp->rw_wrlock_begin_hold)
				rwsp->rw_wrlock_hold_time +=
				    gethrtime() - rwsp->rw_wrlock_begin_hold;
			rwsp->rw_wrlock_begin_hold = 0;
		}
		rwlp->readers = 0;
		curthread->ul_wrlock--;
	} else if (rwlp->readers > 0) {
		(void) mutex_lock_internal(rwlock, rwlp, MUTEX_LOCK);
		if (--rwlp->readers == 0 && CVWAITERS(writers))
			(void) cond_signal_internal(writers, rwlp);
		curthread->ul_rdlock--;
	} else {
		/*
		 * This is a usage error.
		 * No thread should release an unowned lock.
		 */
		return (EPERM);
	}
	(void) _mutex_unlock(rwlock);
	return (0);
}

#pragma weak rw_tryrdlock = _rw_tryrdlock
#pragma weak _liblwp_rw_tryrdlock = _rw_tryrdlock
#pragma weak pthread_rwlock_tryrdlock = _rw_tryrdlock
#pragma weak _pthread_rwlock_tryrdlock = _rw_tryrdlock
#pragma weak _liblwp_pthread_rwlock_tryrdlock = _rw_tryrdlock
int
_rw_tryrdlock(rwlock_t *rwlp)
{
	/* LINTED pointer cast */
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	/* LINTED pointer cast */
	cond_t *writers = (cond_t *)(rwlp->pad3);
	tdb_rwlock_stats_t *rwsp = RWLOCK_STATS(rwlp);
	int error = 0;

	if (rwsp)
		tdb_incr(rwsp->rw_rdlock_try);

	/*
	 * Spin on trying to grab the mutex so long as
	 * there is no writer and no writers are waiting.
	 * We don't want to fail just because some other
	 * reader happens to own the mutex right now.
	 */
	while (_mutex_trylock(rwlock) != 0) {
		if (rwlp->readers < 0 || CVWAITERS(writers)) {
			if (rwsp)
				tdb_incr(rwsp->rw_rdlock_try_fail);
			return (EBUSY);
		}
		_yield();
	}
	if (rwlp->readers < 0 || CVWAITERS(writers)) {
		if (rwsp)
			tdb_incr(rwsp->rw_rdlock_try_fail);
		error = EBUSY;
	} else {
		rwlp->readers++;
		curthread->ul_rdlock++;
	}
	(void) _mutex_unlock(rwlock);
	return (error);
}

#pragma weak rw_trywrlock = _rw_trywrlock
#pragma weak _liblwp_rw_trywrlock = _rw_trywrlock
#pragma weak pthread_rwlock_trywrlock = _rw_trywrlock
#pragma weak _pthread_rwlock_trywrlock = _rw_trywrlock
#pragma weak _liblwp_pthread_rwlock_trywrlock = _rw_trywrlock
int
_rw_trywrlock(rwlock_t *rwlp)
{
	/* LINTED pointer cast */
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	tdb_rwlock_stats_t *rwsp = RWLOCK_STATS(rwlp);

	if (rwsp)
		tdb_incr(rwsp->rw_wrlock_try);

	/*
	 * One failure to grab the mutex is sufficient
	 * to know that the lock is currently busy or
	 * at least that it was busy just a moment ago.
	 */
	if (_mutex_trylock(rwlock) != 0) {
		if (rwsp)
			tdb_incr(rwsp->rw_wrlock_try_fail);
		return (EBUSY);
	}
	if (rwlp->readers != 0) {
		if (rwsp)
			tdb_incr(rwsp->rw_wrlock_try_fail);
		(void) _mutex_unlock(rwlock);
		return (EBUSY);
	}
	rwlp->readers = -1;
	curthread->ul_wrlock++;
	/*
	 * We return with the mutex held.
	 * This is done to ensure fork1() safety.
	 */
	return (0);
}
