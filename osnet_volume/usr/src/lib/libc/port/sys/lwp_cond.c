/*
 *      Copyright (c) (1992-1996) Sun Microsystems Inc
 *      All Rights Reserved.
*/

#pragma ident	"@(#)lwp_cond.c 1.11	99/10/25	SMI"

#pragma weak _lwp_cond_wait = __lwp_cond_wait
#pragma weak _lwp_cond_timedwait = __lwp_cond_timedwait

/*LINTLIBRARY*/
#include "synonyms.h"
#include <sys/types.h>

#include <sys/time.h>
#include <errno.h>
#include <synch.h>
#include <sys/synch32.h>
#include <pthread.h>

extern int ___lwp_cond_wait(lwp_cond_t *, lwp_mutex_t *, timestruc_t *);
extern int ___lwp_mutex_lock(lwp_mutex_t *);

int
_lwp_cond_wait(cond_t *cv, mutex_t *mp)
{
	int error;

	error = ___lwp_cond_wait(cv, mp, (timestruc_t *)NULL);
	if (mp->mutex_type & (PTHREAD_PRIO_INHERIT|PTHREAD_PRIO_PROTECT))
		(void) ___lwp_mutex_lock(mp);
	else
		(void) _lwp_mutex_lock(mp);
	return (error);
}

int
_lwp_cond_timedwait(cond_t *cv, mutex_t *mp, timestruc_t *absts)
{
	int error;
	struct timeval now1;
	timestruc_t now2, tslocal = *absts;

	(void) gettimeofday(&now1, NULL);
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
	if (mp->mutex_type & (PTHREAD_PRIO_INHERIT|PTHREAD_PRIO_PROTECT))
		(void) ___lwp_mutex_lock(mp);
	else
		(void) _lwp_mutex_lock(mp);
	return (error);
}
