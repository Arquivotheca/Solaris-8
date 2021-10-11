/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)adjtime.c	1.8	99/03/10 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/debug.h>
#include <sys/model.h>

int
adjtime(struct timeval *delta, struct timeval *olddelta)
{
	struct timeval atv, oatv;
	int64_t	ndelta;
	int64_t old_delta;
	int s;
	model_t datamodel = get_udatamodel();

	if (!suser(CRED()))
		return (set_errno(EPERM));

	if (datamodel == DATAMODEL_NATIVE) {
		if (copyin(delta, &atv, sizeof (atv)))
			return (set_errno(EFAULT));
	} else {
		struct timeval32 atv32;

		if (copyin(delta, &atv32, sizeof (atv32)))
			return (set_errno(EFAULT));
		TIMEVAL32_TO_TIMEVAL(&atv, &atv32);
	}

	if (atv.tv_usec <= -MICROSEC || atv.tv_usec >= MICROSEC)
		return (set_errno(EINVAL));

	/*
	 * The SVID specifies that if delta is 0, then there is
	 * no effect upon time correction, just return olddelta.
	 */
	ndelta = (int64_t)atv.tv_sec * NANOSEC + atv.tv_usec * 1000;
	mutex_enter(&tod_lock);
	s = hr_clock_lock();
	old_delta = timedelta;
	if (ndelta)
		timedelta = ndelta;
	/*
	 * Always set tod_needsync on all adjtime() calls, since it implies
	 * someone is watching over us and keeping the local clock in sync.
	 */
	tod_needsync = 1;
	hr_clock_unlock(s);
	mutex_exit(&tod_lock);

	if (olddelta) {
		oatv.tv_sec = old_delta / NANOSEC;
		oatv.tv_usec = (old_delta % NANOSEC) / 1000;
		if (datamodel == DATAMODEL_NATIVE) {
			if (copyout(&oatv, olddelta, sizeof (oatv)))
				return (set_errno(EFAULT));
		} else {
			struct timeval32 oatv32;

			if (TIMEVAL_OVERFLOW(&oatv))
				return (set_errno(EOVERFLOW));

			TIMEVAL_TO_TIMEVAL32(&oatv32, &oatv);

			if (copyout(&oatv32, olddelta, sizeof (oatv32)))
				return (set_errno(EFAULT));
		}
	}
	return (0);
}
