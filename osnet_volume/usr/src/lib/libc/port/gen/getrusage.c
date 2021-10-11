/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)getrusage.c	1.6	99/10/05 SMI"

/*
 * ******************************************************************
 *
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		  All rights reserved.
 * ********************************************************************
 */

/*LINTLIBRARY*/
#include "synonyms.h"

/*
 * Compatibility lib for BSD's getrusgae(). Only the
 * CPU time usage is supported for RUSAGE_CHILDREN, and hence does not
 * fully support BSD's rusage semantics.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <stdio.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <sys/fcntl.h>
#include <procfs.h>
#include <string.h>
#include <unistd.h>

static int readprocusage(pid_t, struct rusage *);

int
getrusage(int who, struct rusage *rusage)
{
	struct tms tms;
	clock_t hz;

	(void) memset(rusage, 0, sizeof (struct rusage));

	switch (who) {
	case RUSAGE_SELF:
		return (readprocusage(getpid(), rusage));

	case RUSAGE_CHILDREN:
		if (times(&tms) == (clock_t) -1)
			return (-1);		/* errno set by times() */
		hz = CLK_TCK;
		rusage->ru_utime.tv_sec = tms.tms_cutime / hz;
		rusage->ru_utime.tv_usec = (tms.tms_cutime % hz) *
			(1000000 / hz);
		rusage->ru_stime.tv_sec = tms.tms_cstime / hz;
		rusage->ru_stime.tv_usec = (tms.tms_cstime % hz) *
			(1000000 / hz);
		return (0);

	default:
		errno = EINVAL;
		return (-1);
	}
}


/*
 * result = a + b;
 */
#define	ADDTIMES(result, a, b) { \
	(result).tv_sec = (a).tv_sec + (b).tv_sec; \
	if (((result).tv_usec = ((a).tv_nsec + (b).tv_nsec)/1000) >= 1000000) {\
		(result).tv_sec++; \
		(result).tv_usec -= 1000000; \
	} \
}

static int
readprocusage(pid_t pid, struct rusage *rup)
{
	int	fd;
	ssize_t	ret;
	char	proc[32];
	struct prusage	pr;

	(void) sprintf(proc, "/proc/%d/usage", pid);
	if ((fd = open(proc, O_RDONLY)) < 0)
		return (-1);
	ret = read(fd, (char *)&pr, sizeof (pr));
	(void) close(fd);
	if (ret != sizeof (pr))
		return (-1);

	rup->ru_utime.tv_sec = pr.pr_utime.tv_sec;
	rup->ru_utime.tv_usec = pr.pr_utime.tv_nsec / 1000;
	ADDTIMES(rup->ru_stime, pr.pr_stime, pr.pr_ttime);
	rup->ru_minflt = pr.pr_minf;
	rup->ru_majflt = pr.pr_majf;
	rup->ru_nswap = pr.pr_nswap;	/* swaps */
	rup->ru_inblock = pr.pr_inblk;	/* block input operations */
	rup->ru_oublock = pr.pr_oublk;	/* block output operations */
	rup->ru_msgsnd = pr.pr_msnd;	/* messages sent */
	rup->ru_msgrcv = pr.pr_mrcv;	/* messages received */
	rup->ru_nsignals = pr.pr_sigs;	/* signals received */
	rup->ru_nvcsw = pr.pr_vctx;	/* voluntary context switches */
	rup->ru_nivcsw = pr.pr_ictx;

	return (0);
}
