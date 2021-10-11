/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)rwlock.c	1.49	99/07/27 SMI"

/*
 * synchronization primitives for threads.
 */

#ifdef __STDC__
#pragma weak rwlock_init = _rwlock_init
#pragma weak rwlock_destroy = _rwlock_destroy
#pragma weak rw_rdlock = _rw_rdlock
#pragma weak rw_wrlock = _rw_wrlock
#pragma weak rw_unlock = _rw_unlock
#pragma weak rw_tryrdlock = _rw_tryrdlock
#pragma weak rw_trywrlock = _rw_trywrlock

#pragma	weak _ti_rw_read_held = _rw_read_held
#pragma	weak _ti_rw_rdlock = _rw_rdlock
#pragma	weak _ti_rw_wrlock = _rw_wrlock
#pragma weak _ti_rw_unlock = _rw_unlock
#pragma weak _ti_rw_tryrdlock = _rw_tryrdlock
#pragma	weak _ti_rw_trywrlock = _rw_trywrlock
#pragma	weak _ti_rw_write_held = _rw_write_held
#pragma	weak _ti_rwlock_init = _rwlock_init
#pragma	weak _ti_rwlock_destroy = _rwlock_destroy

#pragma weak pthread_rwlock_destroy = _rwlock_destroy
#pragma weak pthread_rwlock_rdlock = _rw_rdlock
#pragma weak pthread_rwlock_wrlock = _rw_wrlock
#pragma weak pthread_rwlock_unlock = _rw_unlock
#pragma weak pthread_rwlock_tryrdlock = _rw_tryrdlock
#pragma weak pthread_rwlock_trywrlock = _rw_trywrlock

#pragma weak _pthread_rwlock_destroy = _rwlock_destroy
#pragma weak _pthread_rwlock_rdlock = _rw_rdlock
#pragma weak _pthread_rwlock_wrlock = _rw_wrlock
#pragma weak _pthread_rwlock_unlock = _rw_unlock
#pragma weak _pthread_rwlock_tryrdlock = _rw_tryrdlock
#pragma weak _pthread_rwlock_trywrlock = _rw_trywrlock

#pragma weak _ti_pthread_rwlock_destroy = _rwlock_destroy
#pragma weak _ti_pthread_rwlock_rdlock = _rw_rdlock
#pragma weak _ti_pthread_rwlock_wrlock = _rw_wrlock
#pragma weak _ti_pthread_rwlock_unlock = _rw_unlock
#pragma weak _ti_pthread_rwlock_tryrdlock = _rw_tryrdlock
#pragma weak _ti_pthread_rwlock_trywrlock = _rw_trywrlock

#endif /* __STDC__ */


#include "libthread.h"
#include "tdb_agent.h"

/*
 * The following functions are used to make the rwlocks fork1 safe.
 * The functions  are called from fork1() (see sys/common/fork1.c)
 */
void _rwlsub_lock(void);
void _rwlsub_unlock(void);

/*
 * All rwlock table size (509).  This number can
 * change to increase the number of buckets and increase the
 * concurrency through the rwlock interface.
 */
#define	ALLRWL_TBLSIZ 509

/* The rwlock HASH bucket (static to init the locks! and hide it) */
static mutex_t _allrwlocks[ALLRWL_TBLSIZ];

#define	HASH_RWL(rwlp) ((uintptr_t)(rwlp) % ALLRWL_TBLSIZ)

#if	defined(UTRACE) || defined(ITRACE)
#define	TRACE_RW_NAME(x) (((x)->rcv.type & TRACE_TYPE) ? (x)->name : "<noname>")
#include <string.h>
#endif

/*
 * Check if a reader version of the lock is held.
 */
int
_rw_read_held(rwlock_t *rwlp)
{
	return (rwlp->readers > 0);
}

/*
 * Check if a writer version of the lock is held.
 */
int
_rw_write_held(rwlock_t *rwlp)
{
	return (rwlp->readers == -1);
}

int
_rwlock_init(rwlock_t *rwlp, int type, void *arg)
{
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	cond_t *readers = (cond_t *)(rwlp->pad2);
	cond_t *writers = (cond_t *)(rwlp->pad3);

	if (type != USYNC_THREAD && type != USYNC_PROCESS)
		return (EINVAL);
	_mutex_init(rwlock, type, arg);
	_cond_init(readers, type, arg);
	_cond_init(writers, type, arg);
	rwlp->type = type;
	rwlp->readers = 0;
	rwlp->magic = RWL_MAGIC;
	if (__tdb_attach_stat != TDB_NOT_ATTACHED)
		_tdb_sync_obj_register((caddr_t)rwlp, RWL_MAGIC);
	return (0);
}

int
_rwlock_destroy(rwlock_t *rwlp)
{
	rwlp->magic = 0;
	_tdb_sync_obj_deregister((caddr_t)rwlp);
	return (0);
}

int
_rw_rdlock(rwlock_t *rwlp)
{
	cond_t *readers = (cond_t *)(rwlp->pad2);
	cond_t *writers = (cond_t *)(rwlp->pad3);
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);

	/*
	 * for fork1() safety, for process local rw locks, acquire a lock from
	 * a static table, instead of a lock embedded in the rwlock structure.
	 */
	if (rwlp->type == USYNC_THREAD)
		rwlock = &(_allrwlocks[HASH_RWL(rwlp)]);
	_mutex_lock(rwlock);
	while ((rwlp->readers == -1) || writers->cond_waiters) {
		rwlp->magic = RWL_MAGIC;
		if (__tdb_attach_stat != TDB_NOT_ATTACHED)
			_tdb_sync_obj_register((caddr_t)rwlp, RWL_MAGIC);
		_cond_wait(readers, rwlock);
	}
	rwlp->readers++;
	ASSERT(rwlp->readers > 0);
	_mutex_unlock(rwlock);
	return (0);
}

int
_lrw_rdlock(rwlock_t *rwlp)
{
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	cond_t *readers = (cond_t *)(rwlp->pad2);
	cond_t *writers = (cond_t *)(rwlp->pad3);

	_sigoff();
	_lwp_mutex_lock(rwlock);
	while ((rwlp->readers == -1) || writers->cond_waiters) {
		__lwp_cond_wait(readers, rwlock);
	}
	rwlp->readers++;
	ASSERT(rwlp->readers > 0);
	_lwp_mutex_unlock(rwlock);
	return (0);
}

int
_rw_wrlock(rwlock_t *rwlp)
{
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	cond_t *readers = (cond_t *)(rwlp->pad2);
	cond_t *writers = (cond_t *)(rwlp->pad3);

	if (rwlp->type == USYNC_THREAD)
		rwlock = &(_allrwlocks[HASH_RWL(rwlp)]);
	_mutex_lock(rwlock);
	if (writers->cond_waiters) {
		rwlp->magic = RWL_MAGIC;
		if (__tdb_attach_stat != TDB_NOT_ATTACHED)
			_tdb_sync_obj_register((caddr_t)rwlp, RWL_MAGIC);
		/* This ensures FIFO scheduling of write requests.  */
		_cond_wait(writers, rwlock);
		/* Fall through to the while loop below */
	}
	while (rwlp->readers > 0 || rwlp->readers == -1)
		_cond_wait(writers, rwlock);
	rwlp->readers = -1;
	_mutex_unlock(rwlock);
	return (0);
}

int
_lrw_wrlock(rwlock_t *rwlp)
{
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	cond_t *readers = (cond_t *)(rwlp->pad2);
	cond_t *writers = (cond_t *)(rwlp->pad3);

	_sigoff();
	_lwp_mutex_lock(rwlock);
	if (writers->cond_waiters) {
		/* This ensures FIFO scheduling of write requests.  */
		__lwp_cond_wait(writers, rwlock);
		/* Fall through to the while loop below */
	}
	while (rwlp->readers > 0 || rwlp->readers == -1)
		__lwp_cond_wait(writers, rwlock);
	rwlp->readers = -1;
	return (0);
}

int
_rw_unlock(rwlock_t *rwlp)
{
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	cond_t *readers = (cond_t *)(rwlp->pad2);
	cond_t *writers = (cond_t *)(rwlp->pad3);

	if (rwlp->type == USYNC_THREAD)
		rwlock = &(_allrwlocks[HASH_RWL(rwlp)]);
	_mutex_lock(rwlock);
	ASSERT(rwlp->readers == -1 || rwlp->readers > 0);
	if (rwlp->readers == -1) { /* if this is a write lock */
		if (writers->cond_waiters) {
			_cond_signal(writers);
		} else if (readers->cond_waiters)
			_cond_broadcast(readers);
		rwlp->readers = 0;
	} else {
		rwlp->readers--;
		ASSERT(rwlp->readers >= 0);
		if (!rwlp->readers && writers->cond_waiters) {
			/* signal a blocked writer */
			_cond_signal(writers);
		}
	}
	_mutex_unlock(rwlock);
	return (0);
}

int
_lrw_unlock(rwlock_t *rwlp)
{
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	cond_t *readers = (cond_t *)(rwlp->pad2);
	cond_t *writers = (cond_t *)(rwlp->pad3);

	if (rwlp->readers != -1)
		_lwp_mutex_lock(rwlock);
	ASSERT(rwlp->readers == -1 || rwlp->readers > 0);
	if (rwlp->readers == -1) { /* if this is a write lock */
		if (writers->cond_waiters) {
			_lwp_cond_signal(writers);
		} else if (readers->cond_waiters)
			_lwp_cond_broadcast(readers);
		rwlp->readers = 0;
	} else {
		rwlp->readers--;
		ASSERT(rwlp->readers >= 0);
		if (!rwlp->readers && writers->cond_waiters) {
			/* signal a blocked writer */
			_lwp_cond_signal(writers);
		}
	}
	_lwp_mutex_unlock(rwlock);
	_sigon();
	return (0);
}

int
_rw_tryrdlock(rwlock_t *rwlp)
{
	int retval = 0;
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	cond_t *readers = (cond_t *)(rwlp->pad2);
	cond_t *writers = (cond_t *)(rwlp->pad3);

	if (rwlp->type == USYNC_THREAD)
		rwlock = &(_allrwlocks[HASH_RWL(rwlp)]);
	_mutex_lock(rwlock);
	if (rwlp->readers == -1 || writers->cond_waiters)
		retval = EBUSY;
	else {
		rwlp->readers++;
		retval = 0;
	}
	_mutex_unlock(rwlock);
	return (retval);
}

int
_rw_trywrlock(rwlock_t *rwlp)
{
	int retval = 0;
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	cond_t *readers = (cond_t *)(rwlp->pad2);
	cond_t *writers = (cond_t *)(rwlp->pad3);

	if (rwlp->type == USYNC_THREAD)
		rwlock = &(_allrwlocks[HASH_RWL(rwlp)]);
	_mutex_lock(rwlock);
	if (rwlp->readers > 0 || rwlp->readers == -1) {
		retval = EBUSY;
	} else {
		rwlp->readers = -1;
		retval = 0;
	}
	_mutex_unlock(rwlock);
	return (retval);
}

/*
 * The _rwlsub_[un]lock() functions are called by the thread calling
 * fork1(). These functions acquire/release all the static locks which
 * correspond to the USYNC_THREAD rwlocks. Needs to be as fast as
 * possible - should not heavily penalize programs which do not use
 * rwlocks.
 *
 */
void
_rwlsub_lock(void)
{
	int i;

	for (i = 0; i < ALLRWL_TBLSIZ; i++)
		_mutex_lock(&(_allrwlocks[i]));
}

/*
 * Unlock all the static locks that collectively make rwlocks fork1 safe
 */

void
_rwlsub_unlock(void)
{
	int i;

	for (i = 0; i < ALLRWL_TBLSIZ; i++) {
		ASSERT(MUTEX_HELD(&_allrwlocks[i]));
		_mutex_unlock(&(_allrwlocks[i]));
	}
}

#if	defined(ITRACE) || defined(UTRACE)

int
trace_rw_rdlock(rwlock_t *rwlp)
{
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	cond_t *readers = (cond_t *)(rwlp->pad2);
	cond_t *writers = (cond_t *)(rwlp->pad3);

	trace_mutex_lock(rwlock);
	while (rwlp->readers == -1 || writers->cond_waiters)
		trace_cond_wait(readers, rwlock);
	rwlp->readers++;
	trace_mutex_unlock(rwlock);
	return (0);
}

int
trace_rw_wrlock(rwlock_t *rwlp)
{
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	cond_t *readers = (cond_t *)(rwlp->pad2);
	cond_t *writers = (cond_t *)(rwlp->pad3);

	trace_mutex_lock(rwlock);
	if (writers->cond_waiters) {
		/* This ensures FIFO scheduling of write requests.  */
		trace_cond_wait(writers, rwlock);
		/* Fall through to the while loop below */
	}
	while (rwlp->readers > 0 || rwlp->readers == -1)
		trace_cond_wait(writers, rwlock);
	rwlp->readers = -1;
	trace_mutex_unlock(rwlock);
	return (0);
}

int
trace_rw_unlock(rwlock_t *rwlp)
{
	mutex_t *rwlock = (mutex_t *)(rwlp->pad1);
	cond_t *readers = (cond_t *)(rwlp->pad2);
	cond_t *writers = (cond_t *)(rwlp->pad3);

	trace_mutex_lock(rwlock);
	ASSERT(rwlp->readers == -1 || rwlp->readers > 0);
	if (rwlp->readers == -1) { /* if this is a write lock */
		if (writers->cond_waiters) {
			trace_cond_signal(&rwlp->wcv);
		} else if (readers->cond_waiters)
			trace_cond_broadcast(readers);
		rwlp->writer = 0;
	} else {
		rwlp->readers--;
		if (!(rwlp->readers) && writers->cond_waiters) {
			/* signal a blocked writer */
			trace_cond_signal(writers);
		}
	}
	trace_mutex_unlock(rwlock);
	return (0);
}

#endif
