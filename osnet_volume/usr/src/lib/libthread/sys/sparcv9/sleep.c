/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)sleep.c	1.3	98/11/09	SMI"

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

/*
 * Suspend the thread for `sleep_tm' seconds - using cond_timedwait() /
 * nanosleep().  If cond_timedwait()/nanosleep)_ returns prematurely, due to a
 * signal, return to the caller the (unsigned) quantity of (requested) seconds
 * unslept. No interaction with alarm().
 */
unsigned
_sleep(unsigned sleep_tm)
{
	cond_t sleep_cv = DEFAULTCV; /* dummy cv and mutex */
	mutex_t   sleep_mx = DEFAULTMUTEX;
	struct timeval wake_tv, current_tv;
	timestruc_t alarm_ts;
	struct timespec tm, rtm;
	int ret = 0;

	if (sleep_tm == 0)
		return (0);

	if (ISBOUND(curthread)) {
		tm.tv_sec = sleep_tm;
		tm.tv_nsec = 0;
		if (((ret = __nanosleep(&tm, &rtm)) < 0) && (errno != EINTR)) {
					_panic("sleep: __nanosleep fails");
		}
		if ((ret < 0) && (rtm.tv_sec > 0))
			return (rtm.tv_sec);
		return (0);
	} else {
		/*
		 * Call cond_timedwait() here  with absolute time.
		 */
		struct timeval sleep_tv;
		int overflow = 0;
		unsigned int osleep_tm = sleep_tm;

		_gettimeofday(&sleep_tv, NULL);
more_sleep:
		if (sleep_tm > COND_REL_EOT) {
			overflow = sleep_tm - COND_REL_EOT;
			sleep_tm = COND_REL_EOT;
		} else {
			overflow = 0;
		}
		_gettimeofday(&current_tv, NULL);
		alarm_ts.tv_sec = current_tv.tv_sec + sleep_tm;
		alarm_ts.tv_nsec = current_tv.tv_usec * 1000;
		if (ret = _cond_timedwait_cancel(&sleep_cv, &sleep_mx,
			&alarm_ts)) {
			if (ret == EINVAL) {
				_panic("sleep: cond_timedwait fails");
			} else if (ret == ETIME) {
				if (overflow > 0) {
					sleep_tm = overflow;
					goto more_sleep;
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
