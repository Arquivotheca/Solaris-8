/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)clock_timer.c 1.7     97/07/30 SMI"

#pragma	weak clock_getres = _clock_getres
#pragma	weak clock_gettime = _clock_gettime
#pragma	weak clock_settime = _clock_settime
#pragma	weak timer_create = _timer_create
#pragma	weak timer_delete = _timer_delete
#pragma	weak timer_getoverrun = _timer_getoverrun
#pragma	weak timer_gettime = _timer_gettime
#pragma	weak timer_settime = _timer_settime
#pragma	weak nanosleep = _nanosleep

/*LINTLIBRARY*/

#include <time.h>
#include <sys/types.h>
#include "pos4.h"

int
_clock_getres(clockid_t clock_id, struct timespec *res)
{
	return (__clock_getres(clock_id, res));
}

int
_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	return (__clock_gettime(clock_id, tp));
}

int
_clock_settime(clockid_t clock_id, const struct timespec *tp)
{
	return (__clock_settime(clock_id, tp));
}

int
_timer_create(clockid_t clock_id, struct sigevent *evp, timer_t *timerid)
{
	return (__timer_create(clock_id, evp, timerid));
}

int
_timer_delete(timer_t timerid)
{
	return (__timer_delete(timerid));
}

int
_timer_getoverrun(timer_t timerid)
{
	return (__timer_getoverrun(timerid));
}

int
_timer_gettime(timer_t timerid, struct itimerspec *value)
{
	return (__timer_gettime(timerid, value));
}

int
_timer_settime(timer_t timerid, int flags, const struct itimerspec *value,
    struct itimerspec *ovalue)
{
	return (__timer_settime(timerid, flags, value, ovalue));
}

int
_nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	return (__nanosleep(rqtp, rmtp));
}
