/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)time.c	1.1	99/08/15 SMI"

#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <libcpc.h>

#include "cpucmds.h"

static hrtime_t timebase;

void
zerotime(void)
{
	timebase = gethrtime();
}

#define	NSECPERSEC	1000000000.0

float
mstimestamp(cpc_event_t *event)
{
	hrtime_t hrt;

	if (event == NULL || event->ce_hrt == 0)
		hrt = gethrtime();
	else
		hrt = event->ce_hrt;
	return ((float)(hrt - timebase) / NSECPERSEC);
}
