/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)lwp.c	1.32	96/02/22	SMI"


#ifdef DEBUG

#ifdef __STDC__
#pragma weak _lwp_cond_wait = __lwp_cond_wait
#pragma weak _lwp_cond_timedwait = __lwp_cond_timedwait
#endif /* __STDC__ */

#include <errno.h>
#include "libthread.h"

int
__lwp_cond_wait(cond_t *cv, mutex_t *mp)
{
	int error;

	if (mp == &_schedlock)
		ASSERT(_sched_owner == curthread);
	error = ___lwp_cond_wait(cv, mp, NULL);
	__lwp_mutex_lock(mp);
	if (mp == &_schedlock) {
		_sched_owner = curthread;
		_sched_ownerpc = _getcaller();
	}
	return (error);
}

int
__lwp_cond_timedwait(cond_t *cv, mutex_t *mp, timestruc_t *absts)
{
	int error;
	struct timeval now1;
	timestruc_t now2, tslocal = *absts;

	if (mp == &_schedlock)
		ASSERT((_sched_owner == curthread));
	error = _gettimeofday(&now1, NULL);
	if (error == -1)
		return (errno);
	/* convert gettimeofday() value to a timestruc_t */
	now2.tv_sec  = now1.tv_sec;
	now2.tv_nsec = (now1.tv_usec)*1000;

	if (tslocal.tv_nsec >= now2.tv_nsec) {
		if (tslocal.tv_sec >= now2.tv_sec) {
			tslocal.tv_sec -= now2.tv_sec;
			tslocal.tv_nsec -= now2.tv_nsec;
		} else
			return (ETIME);
	} else {
		if (tslocal.tv_sec > now2.tv_sec) {
			tslocal.tv_sec  -= (now2.tv_sec + 1);
			tslocal.tv_nsec -= (now2.tv_nsec - 1000000000);
		} else
			return (ETIME);
	}
	error = ___lwp_cond_wait(cv, mp, &tslocal);
	__lwp_mutex_lock(mp);
	if (mp == &_schedlock) {
		_sched_owner = curthread;
		_sched_ownerpc = _getcaller();
	}
	return (error);
}

#endif /* DEBUG */
