/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)shared.c	1.1	99/08/15 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <libintl.h>

#include "libcpc.h"
#include "libcpc_impl.h"

/*
 * To look at the system-wide counters, we have to open the
 * 'shared' device.  Once that device is open, no further contexts
 * can be installed (though one open is needed per CPU)
 */
int
cpc_shared_open(void)
{
	const char driver[] = CPUDRV_SHARED;

	return (open(driver, O_RDWR));
}

void
cpc_shared_close(int fd)
{
	(void) cpc_shared_rele(fd);
	(void) close(fd);
}

int
cpc_shared_bind_event(int fd, cpc_event_t *this, int flags)
{
	if (this == NULL) {
		(void) cpc_shared_rele(fd);
		return (0);
	} else if (flags != 0) {
		errno = EINVAL;
		return (-1);
	} else
		return (ioctl(fd, CPCIO_BIND_EVENT, (intptr_t)this));
}

int
cpc_shared_take_sample(int fd, cpc_event_t *this)
{
	return (ioctl(fd, CPCIO_TAKE_SAMPLE, (intptr_t)this));
}

int
cpc_shared_rele(int fd)
{
	return (ioctl(fd, CPCIO_RELE, 0));
}
