/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987,	1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS	UNPUBLISHED PROPRIETARY	SOURCE CODE OF AT&T	*/
/*	The copyright notice above does	not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Portions Copyright(c) 1988, Sun	Microsystems Inc.	*/
/*	All Rights Reserved					*/

#pragma	ident	"@(#)wait4.c	1.9	99/10/05 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include "synonyms.h"

/*
 * Compatibility lib for SunOS's wait4().
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <wait.h>
#include <sys/siginfo.h>
#include <sys/procset.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <string.h>
#include <errno.h>

/*
 * Since sysV does not support rusage as in BSD, an approximate	approach
 * is:
 *	...
 *	call times
 *	call waitid
 *	if ( a child is	found )
 *		call times again
 *		rusage ~= diff in the 2	times call
 *	...
 */

static int wstat(int, int);

pid_t
wait4(pid_t pid, int *status, int options, struct rusage *rp)
{
	struct	tms	before_tms;
	struct	tms	after_tms;
	siginfo_t	info;
	int		error;
	int		noptions;
	idtype_t	idtype;

	if (rp)
		(void) memset(rp, 0, sizeof (struct rusage));
	(void) memset(&info, 0, sizeof	(siginfo_t));

	if (times(&before_tms) == (clock_t) -1)
		return (-1);		/* errno is set	by times() */

	/*
	 * SunOS's wait4() previously supported only WNOHANG &
	 * WUNTRACED.  XPG4v2 mandates that wait3() (which calls
	 * wait4()) also support WCONTINUED.
	 */
	if (options & ~(WNOHANG|WUNTRACED|WCONTINUED)) {
		errno = EINVAL;
		return (-1);
	}
	noptions = options | WEXITED | WTRAPPED;

	/*
	 * Emulate undocumented	4.x semantics for 1186845
	 */
	if (pid	< 0) {
		pid = -pid;
		idtype = P_PGID;
	} else if (pid == 0)
		idtype = P_ALL;
	else
		idtype = P_PID;

	error =	waitid(idtype, pid, &info, noptions);
	if (error == 0)	{
		clock_t	diffu;	/* difference in usertime (ticks) */
		clock_t	diffs;	/* difference in systemtime (ticks) */
		clock_t hz;

		if ((options & WNOHANG)	&& (info.si_pid	== 0))
			return (0);	/* no child found */

		if (rp)	{

			if (times(&after_tms) == (clock_t) -1)
				return (-1);	/* errno set by	times()	*/
			/*
			 * The system/user time	is an approximation only !!!
			 */
			diffu =	after_tms.tms_cutime - before_tms.tms_cutime;
			diffs =	after_tms.tms_cstime - before_tms.tms_cstime;
			hz = CLK_TCK;
			rp->ru_utime.tv_sec = diffu / hz;
			rp->ru_utime.tv_usec = (diffu %	hz) * (1000000 / hz);
			rp->ru_stime.tv_sec = diffs / hz;
			rp->ru_stime.tv_usec = (diffs %	hz) * (1000000 / hz);
		}
		if (status)
			*status	= wstat(info.si_code, info.si_status);
		return (info.si_pid);
	} else {
		return (-1);		/* error number	is set by waitid() */
	}
}

/*
 * Convert the status code to old style	wait status
 */
static int
wstat(int code,	int status)
{
	int stat = (status & 0377);

	switch (code) {
	case CLD_EXITED:
		stat <<= 8;
		break;
	case CLD_DUMPED:
		stat |=	WCOREFLG;
		break;
	case CLD_KILLED:
		break;
	case CLD_TRAPPED:
	case CLD_STOPPED:
		stat <<= 8;
		stat |=	WSTOPFLG;
		break;
	case CLD_CONTINUED:
		stat = WCONTFLG;
		break;
	}
	return (stat);
}
