/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)sleep.c	1.27	98/11/09	SMI"


#ifndef ABI
#ifdef __STDC__
#pragma weak sleep = _sleep

#pragma	weak _ti_sleep = _sleep
#endif
#endif
#include "libthread.h"
#include "sys/time.h"
#include <limits.h>

extern int __nanosleep(const struct timespec *rqtp,  struct  timespec *rmtp);
static unsigned int sleep_forever(cond_t *, mutex_t *, unsigned);

/*
 * Suspend the thread for `sleep_tm' seconds - using cond_timedwait()/
 * __nanosleep(). If cond_timedwait() returns prematurely, due to a signal,
 * return to the caller the (unsigned) quantity of (requested) seconds unslept.
 * No interaction with alarm().
 */
unsigned
_sleep(unsigned sleep_tm)
{
	cond_t sleep_cv = DEFAULTCV; /* dummy cv and mutex */
	mutex_t   sleep_mx = DEFAULTMUTEX;
	struct timeval wake_tv, sleep_tv, current_tv;
	timestruc_t alarm_ts;
	int ret = 0;
	unsigned int overflow = 0;
	struct timespec tm, rtm = {0, 0};
	unsigned int osleep_tm = sleep_tm;

	if (sleep_tm == 0)
		return (0);

	if (sleep_tm > INT_MAX)
		overflow = sleep_tm - INT_MAX;
	if (ISBOUND(curthread)) {
		if (overflow > 0)
			tm.tv_sec = INT_MAX;
		else
			tm.tv_sec = sleep_tm;
		tm.tv_nsec = 0;
		if (tm.tv_sec >= LONG_MAX)
			return (sleep_forever(&sleep_cv, &sleep_mx, sleep_tm));
		_gettimeofday(&sleep_tv, NULL);

bound_more_sleep:
		/* remove check for EOVERFLOW when __nanosleep() fixed */
		if (((ret = __nanosleep(&tm, &rtm)) < 0) && (errno != EINTR) &&
			(errno != EOVERFLOW)) {
			_panic("sleep: __nanosleep fails");
		}
		if ((ret < 0) && (errno == EOVERFLOW)) {
			_gettimeofday(&wake_tv, NULL);
			if (wake_tv.tv_sec - sleep_tv.tv_sec >= osleep_tm)
				return (0);
			else
				/* if premature wakeup */
				return (osleep_tm - wake_tv.tv_sec +
					sleep_tv.tv_sec);
		}
		if ((ret < 0) && (rtm.tv_sec > 0))
			return (rtm.tv_sec);
		if (overflow > 0) {
			tm.tv_sec = overflow;
			overflow = 0;
			goto bound_more_sleep;
		}
		return (0);
	} else {
		if (overflow > 0)
			alarm_ts.tv_sec = time(NULL) + INT_MAX;
		else
			alarm_ts.tv_sec = time(NULL) + sleep_tm;
		alarm_ts.tv_nsec = 0;
		if (alarm_ts.tv_sec >= LONG_MAX)
			return (sleep_forever(&sleep_cv, &sleep_mx, sleep_tm));
		_gettimeofday(&sleep_tv, NULL);
ubound_more_sleep:
		if (sleep_tm > COND_REL_EOT) {
			overflow = sleep_tm - COND_REL_EOT;
			sleep_tm = COND_REL_EOT;
		} else {
			overflow = 0;
		}
		_gettimeofday(&current_tv, NULL);
		alarm_ts.tv_sec = current_tv.tv_sec + sleep_tm;
		alarm_ts.tv_nsec = current_tv.tv_usec * 1000;
		/*
		 * Call cond_timedwait() here  with absolute time.
		 */
		if (ret = _cond_timedwait_cancel(&sleep_cv, &sleep_mx,
			&alarm_ts)) {
			if (ret == EINVAL) {
				_panic("sleep: cond_timedwait fails");
			} else if (ret == ETIME) {
				if (overflow > 0) {
					sleep_tm = overflow;
					goto ubound_more_sleep;
				}
				return (0); /* valid sleep */
			}
		}
		_gettimeofday(&wake_tv, NULL);
		if (wake_tv.tv_sec - sleep_tv.tv_sec >= osleep_tm)
			return (0);
		else
			/* if premature wakeup */
			return (osleep_tm - wake_tv.tv_sec + sleep_tv.tv_sec);
	}
}


static unsigned int
sleep_forever(cond_t *sleep_cv, mutex_t *sleep_mx, unsigned sleep_tm)
{
	struct timeval sleep_tv, wake_tv;
	int ret;

	_gettimeofday(&sleep_tv, NULL);
	ret = ___lwp_cond_wait(sleep_cv, sleep_mx, 0);
	if (ret == EINTR) {
		_gettimeofday(&wake_tv, NULL);
		if (wake_tv.tv_sec - sleep_tv.tv_sec >= sleep_tm)
			return (0);
		else
			/* if premature wakeup */
			return (sleep_tm -  wake_tv.tv_sec + sleep_tv.tv_sec);
	} else {
		_panic("sleep: ___lwp_cond_timedwait fails");
	}
}
