/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)sysconfig.c	1.14	99/06/05 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/errno.h>
#include <sys/var.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/sysconfig.h>
#include <sys/resource.h>
#include <sys/ulimit.h>
#include <sys/unistd.h>
#include <sys/debug.h>
#include <sys/cpuvar.h>
#include <sys/mman.h>
#include <sys/timer.h>

long
sysconfig(int which)
{
	switch (which) {

	/*
	 * if it is not handled in mach_sysconfig either
	 * it must be EINVAL.
	 */
	default:
		return (mach_sysconfig(which)); /* `uname -i`/os */

	case _CONFIG_CLK_TCK:
		return ((long)hz);	/* clock frequency per second */

	case _CONFIG_PROF_TCK:
		return ((long)hz);	/* profiling clock freq per sec */

	case _CONFIG_NGROUPS:
		/*
		 * Maximum number of supplementary groups.
		 */
		return (ngroups_max);

	case _CONFIG_OPEN_FILES:
		/*
		 * Maximum number of open files (soft limit).
		 */
		return ((u_long)P_CURLIMIT(curproc, RLIMIT_NOFILE));

	case _CONFIG_CHILD_MAX:
		/*
		 * Maximum number of processes.
		 */
		return (v.v_maxup);

	case _CONFIG_POSIX_VER:
		return (_POSIX_VERSION); /* current POSIX version */

	case _CONFIG_PAGESIZE:
		return (PAGESIZE);

	case _CONFIG_XOPEN_VER:
		return (_XOPEN_VERSION); /* current XOPEN version */

	case _CONFIG_NPROC_CONF:
		return (ncpus);

	case _CONFIG_NPROC_ONLN:
		return (ncpus_online);

	case _CONFIG_STACK_PROT:
		return (curproc->p_stkprot & ~PROT_USER);

	case _CONFIG_AIO_LISTIO_MAX:
		return (_AIO_LISTIO_MAX);

	case _CONFIG_AIO_MAX:
		return (_AIO_MAX);

	case _CONFIG_AIO_PRIO_DELTA_MAX:
		return (0);

	case _CONFIG_DELAYTIMER_MAX:
		return (INT_MAX);

	case _CONFIG_MQ_OPEN_MAX:
		return (_MQ_OPEN_MAX);

	case _CONFIG_MQ_PRIO_MAX:
		return (_MQ_PRIO_MAX);

	case _CONFIG_RTSIG_MAX:
		return (_SIGRTMAX - _SIGRTMIN + 1);

	case _CONFIG_SEM_NSEMS_MAX:
		return (_SEM_NSEMS_MAX);

	case _CONFIG_SEM_VALUE_MAX:
		return (_SEM_VALUE_MAX);

	case _CONFIG_SIGQUEUE_MAX:
		return (_SIGQUEUE_MAX);

	case _CONFIG_SIGRT_MIN:
		return (_SIGRTMIN);

	case _CONFIG_SIGRT_MAX:
		return (_SIGRTMAX);

	case _CONFIG_TIMER_MAX:
		return (_TIMER_MAX);

	case _CONFIG_PHYS_PAGES:
		return (physinstalled);

	case _CONFIG_AVPHYS_PAGES:
		return (freemem);

	case _CONFIG_MAXPID:
		return (maxpid);
	}
}
