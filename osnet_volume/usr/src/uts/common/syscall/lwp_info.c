/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)lwp_info.c	1.5	97/08/12 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/debug.h>
#include <sys/model.h>

/*
 * Get the time accounting information for the calling LWP.
 */
int
lwp_info(timestruc_t *tvp)
{
	timestruc_t tv[2];
	klwp_t *lwp = ttolwp(curthread);

	TICK_TO_TIMESTRUC(lwp->lwp_utime, &tv[0]);
	TICK_TO_TIMESTRUC(lwp->lwp_stime, &tv[1]);

	if (get_udatamodel() == DATAMODEL_NATIVE) {
		if (copyout(tv, tvp, sizeof (tv)))
			return (set_errno(EFAULT));
	} else {
		timestruc32_t tv32[2];

		if (TIMESPEC_OVERFLOW(&tv[0]) ||
		    TIMESPEC_OVERFLOW(&tv[1]))
			return (set_errno(EOVERFLOW));	/* unlikely */

		TIMESPEC_TO_TIMESPEC32(&tv32[0], &tv[0]);
		TIMESPEC_TO_TIMESPEC32(&tv32[1], &tv[1]);

		if (copyout(tv32, tvp, sizeof (tv32)))
			return (set_errno(EFAULT));
	}
	return (0);
}
