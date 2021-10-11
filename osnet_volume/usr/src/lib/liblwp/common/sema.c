/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sema.c	1.3	99/11/02 SMI"

#include "liblwp.h"

/*
 * Declare the strong versions of the _lwp_sema_*() functions.
 */
extern	int	__lwp_sema_wait(lwp_sema_t *);
extern	int	__lwp_sema_trywait(lwp_sema_t *);
extern	int	__lwp_sema_post(lwp_sema_t *);

/*
 * Check to see if anyone is waiting for this semaphore.
 */
#pragma weak sema_held = _sema_held
#pragma weak _liblwp_sema_held = _sema_held
int
_sema_held(sema_t *sp)
{
	return (sp->count == 0);
}

#pragma weak sema_init = _sema_init
#pragma weak _liblwp_sema_init = _sema_init
/* ARGSUSED2 */
int
_sema_init(sema_t *sp, unsigned int count, int type, void *arg)
{
	tdb_sema_stats_t *ssp;

	if ((type != USYNC_THREAD && type != USYNC_PROCESS) ||
	    (count > _lsemvaluemax))
		return (EINVAL);
	(void) _memset(sp, 0, sizeof (*sp));
	sp->count = count;
	sp->type = (uint16_t)type;
	if ((ssp = SEMA_STATS(sp)) != NULL) {
		ssp->sema_max_count = count;
		ssp->sema_min_count = count;
	}
	return (0);
}

#pragma weak sema_destroy = _sema_destroy
#pragma weak _liblwp_sema_destroy = _sema_destroy
int
_sema_destroy(sema_t *sp)
{
	sp->magic = 0;
	tdb_sync_obj_deregister(sp);
	return (0);
}

/*
 * sema_wait() is a cancellation point but _sema_wait() is not.
 * System libraries call the non-cancellation version.
 * It is expected that only applications call the cancellation version.
 */
#pragma weak _liblwp_sema_wait = _sema_wait
int
_sema_wait(sema_t *sp)
{
	ulwp_t *self = curthread;
	tdb_sema_stats_t *ssp = SEMA_STATS(sp);
	hrtime_t begin_sleep = 0;
	uint_t count;
	int error;

	if (ssp)
		tdb_incr(ssp->sema_wait);

	_save_nv_regs(&self->ul_savedregs);
	self->ul_validregs = 1;
	self->ul_wchan = (uintptr_t)sp;
	if (__td_event_report(self, TD_SLEEP)) {
		self->ul_td_evbuf.eventnum = TD_SLEEP;
		self->ul_td_evbuf.eventdata = sp;
		tdb_event_sleep();
	}
	/* just a guess, but it looks like we will sleep */
	if (ssp && sp->count == 0) {
		begin_sleep = gethrtime();
		if (sp->count == 0)	/* still looks like sleep */
			tdb_incr(ssp->sema_wait_sleep);
		else			/* we changed our mind */
			begin_sleep = 0;
	}
	error = __lwp_sema_wait((lwp_sema_t *)sp);
	self->ul_wchan = 0;
	self->ul_validregs = 0;
	if (ssp) {
		if (error == 0) {
			/* we just decremented the count */
			count = sp->count;
			if (ssp->sema_min_count > count)
				ssp->sema_min_count = count;
		}
		if (begin_sleep)
			ssp->sema_wait_sleep_time += gethrtime() - begin_sleep;
	}
	return (error);
}

#pragma weak sema_wait = _sema_wait_cancel
int
_sema_wait_cancel(sema_t *sp)
{
	int error;

	_cancelon();
	error = _sema_wait(sp);
	if (error == EINTR)
		_canceloff();
	else
		_canceloff_nocancel();
	return (error);
}

#pragma weak sema_trywait = _sema_trywait
#pragma weak _liblwp_sema_trywait = _sema_trywait
int
_sema_trywait(sema_t *sp)
{
	ulwp_t *self = curthread;
	tdb_sema_stats_t *ssp = SEMA_STATS(sp);
	uint_t count;
	int error;

	if (ssp)
		tdb_incr(ssp->sema_trywait);

	error = __lwp_sema_trywait((lwp_sema_t *)sp);
	if (error == 0) {
		if (ssp) {
			/* we just decremented the count */
			count = sp->count;
			if (ssp->sema_min_count > count)
				ssp->sema_min_count = count;
		}
	} else {
		if (ssp)
			tdb_incr(ssp->sema_trywait_fail);
		if (__td_event_report(self, TD_LOCK_TRY)) {
			self->ul_td_evbuf.eventnum = TD_LOCK_TRY;
			tdb_event_lock_try();
		}
	}
	return (error);
}

#pragma weak sema_post = _sema_post
#pragma weak _liblwp_sema_post = _sema_post
int
_sema_post(sema_t *sp)
{
	tdb_sema_stats_t *ssp = SEMA_STATS(sp);
	uint_t count;
	int error;

	if (ssp)
		tdb_incr(ssp->sema_post);

	error = __lwp_sema_post((lwp_sema_t *)sp);
	if (error == 0) {
		if (ssp) {
			/* we just incremented the count */
			count = sp->count;
			if (ssp->sema_max_count < count)
				ssp->sema_max_count = count;
		}
	}
	return (error);
}
