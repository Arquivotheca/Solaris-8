/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)private.c	1.1	99/08/15 SMI"

#include <sys/types.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <libintl.h>

#include "libcpc.h"
#include "libcpc_impl.h"

int
cpc_bind_event(cpc_event_t *this, int flags)
{
	if (this == NULL) {
		(void) cpc_rele();
		return (0);
	}
	return (syscall(SYS_cpc, CPC_BIND_EVENT, -1, this, flags));
}

int
cpc_take_sample(cpc_event_t *this)
{
	return (syscall(SYS_cpc, CPC_TAKE_SAMPLE, -1, this, 0));
}

int
cpc_count_usr_events(int enable)
{
	return (syscall(SYS_cpc, CPC_USR_EVENTS, -1, (void *)enable, 0));
}

int
cpc_count_sys_events(int enable)
{
	return (syscall(SYS_cpc, CPC_SYS_EVENTS, -1, (void *)enable, 0));
}

int
cpc_rele(void)
{
	return (syscall(SYS_cpc, CPC_RELE, -1, NULL, 0));
}

/*
 * See if the system call is working and installed.
 *
 * We invoke the system call with nonsense arguments - if it's
 * there and working correctly, it will return EINVAL.
 *
 * (This avoids the user getting a SIGSYS core dump when they attempt
 * to bind on older hardware)
 */
int
cpc_access(void)
{
	void (*handler)(int);
	int error = 0;
	const char fn[] = "access";

	handler = signal(SIGSYS, SIG_IGN);
	if (syscall(SYS_cpc, -1, -1, NULL, 0) == -1 &&
	    errno != EINVAL)
		error = errno;
	(void) signal(SIGSYS, handler);

	switch (error) {
	case EAGAIN:
		__cpc_error(fn, gettext("Another process may be "
		    "sampling system-wide CPU statistics\n"));
		break;
	case ENOSYS:
		__cpc_error(fn, gettext("CPU performance counters "
		    "are inaccessible on this machine\n"));
		break;
	default:
		__cpc_error(fn, "%s\n", strerror(errno));
		break;
	case 0:
		return (0);
	}

	errno = error;
	return (-1);
}
