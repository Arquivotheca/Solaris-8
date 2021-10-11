/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)sema.c	1.49	99/12/10 SMI"

#ifdef __STDC__
#pragma weak sema_init = _sema_init
#pragma weak sema_destroy = _sema_destroy
#pragma weak sema_wait = _sema_wait_cancel
#pragma weak sema_trywait = _sema_trywait
#pragma weak sema_post = _sema_post

#pragma	weak _ti_sema_held = _sema_held
#pragma	weak _ti_sema_init = _sema_init
#pragma	weak _ti_sema_post = _sema_post
#pragma	weak _ti_sema_trywait = _sema_trywait
#pragma	weak _ti_sema_wait = _sema_wait
#pragma	weak _ti_sema_destroy = _sema_destroy
#endif /* __STDC__ */

#include "libthread.h"
#include "tdb_agent.h"

/*
 * Check to see if anyone is waiting for this semaphore.
 */
int
_sema_held(sema_t *sp)
{
	return (sp->count <= 0);
}

int
_sema_init(sema_t *sp, unsigned int count, int type, void *arg)
{
	mutex_t *semalock = (mutex_t *)&sp->pad1[0];
	cond_t	*semacv	= (cond_t *)&sp->pad2[0];

	if ((type != USYNC_THREAD && type != USYNC_PROCESS) ||
		(count > _lsemvaluemax))
			return (EINVAL);
	_mutex_init(semalock, type, NULL);
	_cond_init(semacv, type, NULL);

	sp->count = count;
	sp->type = type;
	sp->magic = SEMA_MAGIC;
	if (__tdb_attach_stat != TDB_NOT_ATTACHED)
		_tdb_sync_obj_register((caddr_t)sp, SEMA_MAGIC);
	return (0);
}

int
_sema_destroy(sema_t *sp)
{
	sp->magic = 0;
	_tdb_sync_obj_deregister((caddr_t)sp);
	return (0);
}
/*
 * sema_wait():
 * This function is a cancellation point. If libthread needs to use
 * _sema_wait in future and calling function is NOT a cancellation
 * point then you should use _sema_wait which does  not
 * have _cancelon/_canceloff code in it.
 */

int
_sema_wait_cancel(sema_t *sp)
{
	mutex_t *semalock = (mutex_t *)&sp->pad1[0];
	cond_t *semacv = (cond_t *)&sp->pad2[0];
	int err;

	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_SEMA_WAIT_START,
	    "sema_wait start:name %s, addr 0x%x, count %d",
	    TRACE_SEM_NAME(sp), sp, (u_long)sp->count);
	sp->magic = SEMA_MAGIC;
	if (sp->type == USYNC_PROCESS)
		return (_lwp_sema_wait((lwp_sema_t *)sp));
	else {
		err = 0;
		_lmutex_lock(semalock);
		while (sp->count == 0) {
			_sched_lock();
			sp->magic = SEMA_MAGIC;
			if (__tdb_attach_stat != TDB_NOT_ATTACHED)
				_tdb_sync_obj_register((caddr_t)sp,
				    SEMA_MAGIC);
			curthread->t_flag &= ~T_INTR;
			curthread->t_flag |= T_WAITCV;
			_t_block((caddr_t)semacv);
			semacv->cond_waiters = 1;
			_sched_unlock_nosig();
			_lmutex_unlock(semalock);
			_cancelon();
			_swtch(0);
			_canceloff();
			_sigon();
			if (curthread->t_flag & T_INTR) {
				curthread->t_flag &= ~T_INTR;
				return (EINTR);
			}
			_lmutex_lock(semalock);
		}
		sp->count--;
		_lmutex_unlock(semalock);
	}
	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_SEMA_WAIT_END,
	    "sema_wait end:name %s, addr 0x%x, count %d",
	    TRACE_SEM_NAME(sp), sp, (u_long)sp->count);
	return (err);
}

int
_sema_wait(sema_t *sp)
{
	mutex_t *semalock = (mutex_t *)&sp->pad1[0];
	cond_t *semacv = (cond_t *)&sp->pad2[0];
	int err;

	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_SEMA_WAIT_START,
	    "sema_wait start:name %s, addr 0x%x, count %d",
	    TRACE_SEM_NAME(sp), sp, (u_long)sp->count);
	sp->magic = SEMA_MAGIC;
	if (sp->type == USYNC_PROCESS)
		return (_lwp_sema_wait((lwp_sema_t *)sp));
	else {
		err = 0;
		_lmutex_lock(semalock);
		while (sp->count == 0) {
			_sched_lock();
			curthread->t_flag &= ~T_INTR;
			curthread->t_flag |= T_WAITCV;
			if (__tdb_attach_stat != TDB_NOT_ATTACHED)
				_tdb_sync_obj_register((caddr_t)sp,
				    SEMA_MAGIC);
			_t_block((caddr_t)semacv);
			semacv->cond_waiters = 1;
			_sched_unlock_nosig();
			_lmutex_unlock(semalock);
			_swtch(0);
			_sigon();
			if (curthread->t_flag & T_INTR) {
				curthread->t_flag &= ~T_INTR;
				return (EINTR);
			}
			_lmutex_lock(semalock);
		}
		sp->count--;
		_lmutex_unlock(semalock);
	}
	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_SEMA_WAIT_END,
	    "sema_wait end:name %s, addr 0x%x, count %d",
	    TRACE_SEM_NAME(sp), sp, (u_long)sp->count);
	return (err);
}

int
_sema_trywait(sema_t *sp)
{
	mutex_t *semalock = (mutex_t *)&sp->pad1[0];
	cond_t *semacv = (cond_t *)&sp->pad2[0];
	int retval = 0;

	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_SEMA_TRYWAIT_START,
	    "sema_trywait start:name %s, addr 0x%x, count %d",
	    TRACE_SEM_NAME(sp), sp, (u_long)sp->count);
	sp->magic = SEMA_MAGIC;

	if (sp->type == USYNC_PROCESS)
		return (_lwp_sema_trywait((lwp_sema_t *)sp));

	_lmutex_lock(semalock);
	if (sp->count > 0)
		sp->count--;
	else {
		retval = EBUSY;
		if (__td_event_report(curthread, TD_LOCK_TRY)) {
			curthread->t_td_evbuf.eventnum = TD_LOCK_TRY;
			tdb_event_lock_try();
		}
	}
	_lmutex_unlock(semalock);
	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_SEMA_TRYWAIT_END,
	    "sema_trywait end:name %s, addr 0x%x, count %d",
	    TRACE_SEM_NAME(sp), sp, (u_long)sp->count);
	return (retval);
}

int
_sema_post(sema_t *sp)
{
	mutex_t *semalock = (mutex_t *)&sp->pad1[0];
	cond_t *semacv = (cond_t *)&sp->pad2[0];
	u_char waiters;
	thread_t *t;

	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_SEMA_POST_START,
	    "sema_post start:name %s, addr 0x%x, count %d",
	    TRACE_SEM_NAME(sp), sp, (u_long)sp->count);
	sp->magic = SEMA_MAGIC;
	if (sp->type == USYNC_PROCESS)
		return (_lwp_sema_post((lwp_sema_t *)sp));
	else {
		_lmutex_lock(semalock);
		if (sp->count == _lsemvaluemax) {
			_lmutex_unlock(semalock);
			return (EOVERFLOW);
		} else {
			sp->count++;
		}
		if (semacv->cond_waiters) {
			_sched_lock();
			_t_release((caddr_t)semacv, &waiters, T_WAITCV);
			semacv->cond_waiters = waiters;
			_mutex_sema_unlock(semalock);
			_sched_unlock();
		} else
			_lmutex_unlock(semalock);
	}
	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_SEMA_POST_END,
	    "sema_post end:name %s, addr 0x%x, count %d",
	    TRACE_SEM_NAME(sp), sp, (u_long)sp->count);
	return (0);
}

/*
 * The function _libthread_sema_wait() is called by librt. It is to
 * temp-bind unbound threads, so that they are woken up in thread
 * priority order, instead of LWP priority order. See bugid 4126344.
 */
int
_libthread_sema_wait(sema_t *sem)
{

	int	err = 0;
	int	ret;

	/*
	 * If the thread is unbound then temp-bind the thread to the LWP.
	 * The temp binding for an unbound thread in the SCHED_FIFO/RR class
	 * when executed successfully (as root), will push the priority in the
	 * thread down to the LWP, resulting in a correct prioritized wait
	 * for the semaphore, for USYNC_PROCESS type semaphores. If the thread
	 * has the SCHED_OTHER scheduling policy, and/or the type of the
	 * semaphore is USYNC_THREAD, the temp binding is not really necessary
	 * or helpful.
	 */
	if (!ISBOUND(curthread))
		err = _pthread_temp_rt_bind();

	/*
	 * Failing with EPERM means that we do not have permission to
	 * to do the temp-bind. We can still call _sema_wait(),
	 * however, threads will NOT be woken up in thread priority
	 * order, but in the order of the prioirty of the LWP that they
	 * are running on. See bugid 4126344.
	 * If the temp bind failed for some other reason then just
	 * inform the caller.
	 */

	if (err != 0 && err != EPERM)
		return (err);

	ret = sema_wait((sema_t *)sem);

	/*
	 * If the earlier temp-bind was sucessful, then unbind the thread
	 */
	if (!ISBOUND(curthread) && err == 0)
		_pthread_temp_rt_unbind();

	return (ret);
}

#if defined(ITRACE) || defined(UTRACE)
/*
 * Note that the following works if sp is of THREAD_SYNC_SHARED type. If
 * not, and e.g. trace_sema_post() is called by trace() at an internal libthread
 * tracepoint with _schedlock held, then there will be a deadlock if the
 * semlock is not available and the thread has to block.
 */
int
trace_sema_init(sema_t *sp, unsigned int count, char type, void *arg)
{
	trace_mutex_init(&sp->semlock, type, 0);
	trace_cond_init(&sp->semcv, type, 0);
	sp->count = count;
	sp->wakecnt = 0;
	sp->magic = SEMA_MAGIC;
}

int
trace_sema_post(sema_t *sp)
{
	mutex_t *semalock = (mutex_t *)&sp->pad1[0];
	cond_t *semacv = (cond_t *)&sp->pad2[0];

	sp->magic = SEMA_MAGIC;
	if (sp->type == USYNC_PROCESS)
		return (_lwp_sema_post(sp));
	else {
		trace_mutex_lock(semalock);
		if (semacv->cond_waiters) {
			_sched_lock();
			_t_release((caddr_t)semacv, &waiters, T_WAITCV);
			semacv->cond_waiters = waiters;
			sp->count++;
			_mutex_sema_unlock(semalock);
			_sched_unlock();
		} else {
			sp->count++;
			trace_mutex_unlock(semalock);
		}
	}
	return (0);
}

int
trace_sema_wait(sema_t *sp)
{
	mutex_t *semalock = (mutex_t *)&sp->pad1[0];
	cond_t *semacv = (cond_t *)&sp->pad2[0];

	sp->magic = SEMA_MAGIC;
	if (sp->type == USYNC_PROCESS)
		return (_lwp_sema_wait(sp));
	else {
		trace_mutex_lock(semalock);
		while (sp->count == 0) {
			if (err = _cond_wait(semacv, semalock)) {
				trace_mutex_unlock(semalock);
				return (err);
			}
		}
		sp->count++;
		trace_mutex_unlock(semalock);
	}
}

#endif
