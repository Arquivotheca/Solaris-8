/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sysconf.c	1.34	98/10/28 SMI"	/* SVr4.0 1.6 */

/*LINTLIBRARY*/

/* sysconf(3C) - returns system configuration information */

#pragma weak sysconf = _sysconf

#include "synonyms.h"
#include <mtlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysconfig.h>
#include <sys/errno.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <nss_dbdefs.h>
#include <thread.h>
#include <xti.h>
#include "libc.h"

long
sysconf(int name)
{
	static int _pagesize = 0;
	static int _hz = 0;
	static pid_t _maxpid = 0;
	static int _stackprot = 0;
	extern int __xpg4;

	switch (name) {
		default:
			errno = EINVAL;
			return (-1L);

		case _SC_ARG_MAX:
			return ((long)ARG_MAX);

		case _SC_CLK_TCK:
			if (_hz <= 0)
				_hz = _sysconfig(_CONFIG_CLK_TCK);
			return (_hz);

		case _SC_JOB_CONTROL:
			return ((long)_POSIX_JOB_CONTROL);

		case _SC_SAVED_IDS:
			return ((long)_POSIX_SAVED_IDS);

		case _SC_CHILD_MAX:
			return (_sysconfig(_CONFIG_CHILD_MAX));

		case _SC_NGROUPS_MAX:
			return (_sysconfig(_CONFIG_NGROUPS));

		case _SC_OPEN_MAX:
			return (_sysconfig(_CONFIG_OPEN_FILES));

		case _SC_VERSION:
			return (_sysconfig(_CONFIG_POSIX_VER));

		case _SC_PAGESIZE:
			if (_pagesize <= 0)
				_pagesize = _sysconfig(_CONFIG_PAGESIZE);
			return (_pagesize);

		case _SC_XOPEN_VERSION:
			if (__xpg4 == 0)
				return (3L);
			else
				return (4L);

		case _SC_XOPEN_XCU_VERSION:
			return ((long)_XOPEN_XCU_VERSION);

		case _SC_PASS_MAX:
			return ((long)PASS_MAX);

		case _SC_LOGNAME_MAX:
			return ((long)LOGNAME_MAX);

		case _SC_STREAM_MAX:
			return (_sysconfig(_CONFIG_OPEN_FILES));

		case _SC_TZNAME_MAX:
			return (-1L);

		case _SC_NPROCESSORS_CONF:
			return (_sysconfig(_CONFIG_NPROC_CONF));

		case _SC_NPROCESSORS_ONLN:
			return (_sysconfig(_CONFIG_NPROC_ONLN));

		case _SC_STACK_PROT:
			if (_stackprot == 0)
				_stackprot = _sysconfig(_CONFIG_STACK_PROT);
			return (_stackprot);

		/* POSIX.4 names */
		case _SC_ASYNCHRONOUS_IO:
#ifdef _POSIX_ASYNCHRONOUS_IO
			return (1L);
#else
			return (-1L);
#endif

		case _SC_FSYNC:
#ifdef _POSIX_FSYNC
			return (1L);
#else
			return (-1L);
#endif

		case _SC_MAPPED_FILES:
#ifdef _POSIX_MAPPED_FILES
			return (1L);
#else
			return (-1L);
#endif

		case _SC_MEMLOCK:
#ifdef _POSIX_MEMLOCK
			return (1L);
#else
			return (-1L);
#endif

		case _SC_MEMLOCK_RANGE:
#ifdef _POSIX_MEMLOCK_RANGE
			return (1L);
#else
			return (-1L);
#endif

		case _SC_MEMORY_PROTECTION:
#ifdef _POSIX_MEMORY_PROTECTION
			return (1L);
#else
			return (-1L);
#endif

		case _SC_MESSAGE_PASSING:
#ifdef _POSIX_MESSAGE_PASSING
			return (1L);
#else
			return (-1L);
#endif

		case _SC_PRIORITIZED_IO:
#ifdef _POSIX_PRIORITIZED_IO
			return (1L);
#else
			return (-1L);
#endif

		case _SC_PRIORITY_SCHEDULING:
#ifdef _POSIX_PRIORITY_SCHEDULING
			return (1L);
#else
			return (-1L);
#endif

		case _SC_REALTIME_SIGNALS:
#ifdef _POSIX_REALTIME_SIGNALS
			return (1L);
#else
			return (-1L);
#endif

		case _SC_SEMAPHORES:
#ifdef _POSIX_SEMAPHORES
			return (1L);
#else
			return (-1L);
#endif

		case _SC_SHARED_MEMORY_OBJECTS:
#ifdef _POSIX_SHARED_MEMORY_OBJECTS
			return (1L);
#else
			return (-1L);
#endif

		case _SC_SYNCHRONIZED_IO:
#ifdef _POSIX_SYNCHRONIZED_IO
			return (1L);
#else
			return (-1L);
#endif

		case _SC_TIMERS:
#ifdef _POSIX_TIMERS
			return (1L);
#else
			return (-1L);
#endif

		case _SC_AIO_LISTIO_MAX:
			return (_sysconfig(_CONFIG_AIO_LISTIO_MAX));

		case _SC_AIO_MAX:
			return (_sysconfig(_CONFIG_AIO_MAX));

		case _SC_AIO_PRIO_DELTA_MAX:
			return (_sysconfig(_CONFIG_AIO_PRIO_DELTA_MAX));

		case _SC_DELAYTIMER_MAX:
			return (_sysconfig(_CONFIG_DELAYTIMER_MAX));

		case _SC_MQ_OPEN_MAX:
			return (_sysconfig(_CONFIG_MQ_OPEN_MAX));

		case _SC_MQ_PRIO_MAX:
			return (_sysconfig(_CONFIG_MQ_PRIO_MAX));

		case _SC_RTSIG_MAX:
			return (_sysconfig(_CONFIG_RTSIG_MAX));

		case _SC_SEM_NSEMS_MAX:
			return (_sysconfig(_CONFIG_SEM_NSEMS_MAX));

		case _SC_SEM_VALUE_MAX:
			return (_sysconfig(_CONFIG_SEM_VALUE_MAX));

		case _SC_SIGQUEUE_MAX:
			return (_sysconfig(_CONFIG_SIGQUEUE_MAX));

		case _SC_SIGRT_MAX:
			return (_sysconfig(_CONFIG_SIGRT_MAX));

		case _SC_SIGRT_MIN:
			return (_sysconfig(_CONFIG_SIGRT_MIN));

		case _SC_TIMER_MAX:
			return (_sysconfig(_CONFIG_TIMER_MAX));

		case _SC_PHYS_PAGES:
			return (_sysconfig(_CONFIG_PHYS_PAGES));

		case _SC_AVPHYS_PAGES:
			return (_sysconfig(_CONFIG_AVPHYS_PAGES));

		case _SC_2_C_BIND:
			return ((long)_POSIX2_C_BIND);

		case _SC_2_CHAR_TERM:
			return ((long)_POSIX2_CHAR_TERM);

		case _SC_2_C_DEV:
			return ((long)_POSIX2_C_DEV);

		case _SC_2_C_VERSION:
			return (_POSIX2_C_VERSION);

		case _SC_2_FORT_DEV:
		case _SC_2_FORT_RUN:
			return (-1L);

		case _SC_2_LOCALEDEF:
			return ((long)_POSIX2_LOCALEDEF);

		case _SC_2_SW_DEV:
			return ((long)_POSIX2_SW_DEV);

		case _SC_2_UPE:
			return ((long)_POSIX2_UPE);

		case _SC_2_VERSION:
			return (_POSIX2_VERSION);

		case _SC_BC_BASE_MAX:
			return ((long)BC_BASE_MAX);

		case _SC_BC_DIM_MAX:
			return ((long)BC_DIM_MAX);

		case _SC_BC_SCALE_MAX:
			return ((long)BC_SCALE_MAX);

		case _SC_BC_STRING_MAX:
			return ((long)BC_STRING_MAX);

		case _SC_COLL_WEIGHTS_MAX:
			return ((long)COLL_WEIGHTS_MAX);

		case _SC_EXPR_NEST_MAX:
			return ((long)EXPR_NEST_MAX);

		case _SC_LINE_MAX:
			return ((long)LINE_MAX);

		case _SC_RE_DUP_MAX:
			return ((long)RE_DUP_MAX);

		case _SC_XOPEN_CRYPT:
			return (1L);

		case _SC_XOPEN_ENH_I18N:
			return ((long)_XOPEN_ENH_I18N);

		case _SC_XOPEN_SHM:
			return ((long)_XOPEN_SHM);

		case _SC_XOPEN_UNIX:
			return (1L);

		case _SC_XOPEN_LEGACY:
			return (1L);

		case _SC_XOPEN_REALTIME:
			return (1L);

		case _SC_XOPEN_REALTIME_THREADS:
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && \
	defined(_POSIX_THREAD_PRIO_INHERIT) && \
	defined(_POSIX_THREAD_PRIO_PROTECT)
			return (1L);
#else
			return (-1L);
#endif

		case _SC_XBS5_ILP32_OFF32:
			return (1L);

		case _SC_XBS5_ILP32_OFFBIG:
			return (1L);

		case _SC_XBS5_LP64_OFF64:
#ifdef __sparc
			return (1L);
#else
			return (-1L);
#endif

		case _SC_XBS5_LPBIG_OFFBIG:
#ifdef __sparc
			return (1L);
#else
			return (-1L);
#endif

		case _SC_ATEXIT_MAX:
			return ((long)ATEXIT_MAX);

		case _SC_IOV_MAX:
			return ((long)IOV_MAX);

		case _SC_T_IOV_MAX:
			return ((long)T_IOV_MAX);

		case _SC_THREAD_DESTRUCTOR_ITERATIONS:
			return (-1L);

		case _SC_GETGR_R_SIZE_MAX:
			return ((long)NSS_BUFLEN_GROUP);

		case _SC_GETPW_R_SIZE_MAX:
			return ((long)NSS_BUFLEN_PASSWD);

		case _SC_LOGIN_NAME_MAX:
			return ((long)(LOGNAME_MAX + 1));

		case _SC_THREAD_KEYS_MAX:
			return (-1L);

		case _SC_THREAD_STACK_MIN:
			return ((long)_thr_min_stack());

		case _SC_THREAD_THREADS_MAX:
			return (-1L);

		case _SC_TTY_NAME_MAX:
			return ((long)TTYNAME_MAX);

		case _SC_THREADS:
#ifdef _POSIX_THREADS
			return (1L);
#else
			return (-1L);
#endif

		case _SC_THREAD_ATTR_STACKADDR:
#ifdef _POSIX_THREAD_ATTR_STACKADDR
			return (1L);
#else
			return (-1L);
#endif

		case _SC_THREAD_ATTR_STACKSIZE:
#ifdef _POSIX_THREAD_ATTR_STACKSIZE
			return (1L);
#else
			return (-1L);
#endif

		case _SC_THREAD_PRIORITY_SCHEDULING:
#ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
			return (1L);
#else
			return (-1L);
#endif

		case _SC_THREAD_PRIO_INHERIT:
#ifdef _POSIX_THREAD_PRIO_INHERIT
			return (1L);
#else
			return (-1L);
#endif

		case _SC_THREAD_PRIO_PROTECT:
#ifdef _POSIX_THREAD_PRIO_PROTECT
			return (1L);
#else
			return (-1L);
#endif

		case _SC_THREAD_PROCESS_SHARED:
#ifdef _POSIX_THREAD_PROCESS_SHARED
			return (1L);
#else
			return (-1L);
#endif

		case _SC_THREAD_SAFE_FUNCTIONS:
#ifdef _POSIX_THREAD_SAFE_FUNCTIONS
			return (1L);
#else
			return (-1L);
#endif

		case _SC_COHER_BLKSZ:
			return (_sysconfig(_CONFIG_COHERENCY));

		case _SC_SPLIT_CACHE:
			return (_sysconfig(_CONFIG_SPLIT_CACHE));

		case _SC_ICACHE_SZ:
			return (_sysconfig(_CONFIG_ICACHESZ));

		case _SC_DCACHE_SZ:
			return (_sysconfig(_CONFIG_DCACHESZ));

		case _SC_ICACHE_LINESZ:
			return (_sysconfig(_CONFIG_ICACHELINESZ));

		case _SC_DCACHE_LINESZ:
			return (_sysconfig(_CONFIG_DCACHELINESZ));

		case _SC_ICACHE_BLKSZ:
			return (_sysconfig(_CONFIG_ICACHEBLKSZ));

		case _SC_DCACHE_BLKSZ:
			return (_sysconfig(_CONFIG_DCACHEBLKSZ));

		case _SC_DCACHE_TBLKSZ:
			return (_sysconfig(_CONFIG_DCACHETBLKSZ));

		case _SC_ICACHE_ASSOC:
			return (_sysconfig(_CONFIG_ICACHE_ASSOC));

		case _SC_DCACHE_ASSOC:
			return (_sysconfig(_CONFIG_DCACHE_ASSOC));

		case _SC_MAXPID:
			if (_maxpid <= 0)
				_maxpid = _sysconfig(_CONFIG_MAXPID);
			return (_maxpid);
	}
}

/*
 * UNIX 98 version of sysconf needed in order to set _XOPEN_VERSION to 500.
 */

long
__sysconf_xpg5(int name)
{
	if (name == _SC_XOPEN_VERSION)
		return (500L);
	else
		return (sysconf(name));
}
