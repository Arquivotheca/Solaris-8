/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getloadavg.c	1.1	97/12/22 SMI"

/*LINTLIBRARY*/

#ifndef DSHLIB
#pragma weak getloadavg = _getloadavg
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/syscall.h>
#include <sys/loadavg.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * getloadavg -- get the time averaged run queues from the system
 */
int
getloadavg(double loadavg[], int nelem)
{
	int i, buf[LOADAVG_NSTATS];

	if (nelem > LOADAVG_NSTATS)
		nelem = LOADAVG_NSTATS;

	if ((nelem = __getloadavg(buf, nelem)) == -1)
		return (-1);

	for (i = 0; i < nelem; i++)
		loadavg[i] = (double)buf[i] / FSCALE;

	return (nelem);
}
