/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)spray_subr.c	1.5	94/08/25 SMI"

#include <rpc/rpc.h>
#include <rpcsvc/spray.h>

static spraycumul cumul;
static spraytimeval start_time;

void *
sprayproc_spray_1(argp, clnt)
	sprayarr *argp;
	CLIENT *clnt;
{
	cumul.counter++;
	return ((void *)0);
}

spraycumul *
sprayproc_get_1(argp, clnt)
	void *argp;
	CLIENT *clnt;
{
	gettimeofday((struct timeval *)&cumul.clock, 0);
	if (cumul.clock.usec < start_time.usec) {
		cumul.clock.usec += 1000000;
		cumul.clock.sec -= 1;
	}
	cumul.clock.sec -= start_time.sec;
	cumul.clock.usec -= start_time.usec;
	return (&cumul);
}

void *
sprayproc_clear_1(argp, clnt)
	void *argp;
	CLIENT *clnt;
{
	static char res;

	cumul.counter = 0;
	gettimeofday((struct timeval *)&start_time, 0);
	(void) memset((char *)&res, 0, sizeof(res));
	return ((void *)&res);
}
