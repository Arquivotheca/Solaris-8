/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)getloadavg.c	1.1	97/12/22 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/loadavg.h>

/*
 * Extract elements of the raw avenrun array from the kernel for the
 * implementation of getloadavg(3c)
 */
int
getloadavg(int *buf, int nelem)
{
	if (nelem < 0)
		return (set_errno(EINVAL));
	if (nelem > LOADAVG_NSTATS)
		nelem = LOADAVG_NSTATS;
	if (copyout(avenrun, buf, nelem * sizeof (avenrun[0])) != 0)
		return (set_errno(EFAULT));
	return (nelem);
}
