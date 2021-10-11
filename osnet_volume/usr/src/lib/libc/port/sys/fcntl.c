/*
 * Copyright (c) 1994-1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fcntl.c	1.14	99/09/11 SMI"

/*LINTLIBRARY*/

#pragma weak _fcntl = _libc_fcntl

#include "synonyms.h"
#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/filio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/stropts.h>
#include <sys/socket.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/socketvar.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "libc.h"

extern int __fcntl(int fd, int cmd, intptr_t arg);

#if !defined(_LP64)
/*
 * XXX these hacks are needed for X.25 which assumes that s_fcntl and
 * s_ioctl exist in the socket library.
 * There is no need for a _s_ioctl for other purposes.
 */
#pragma weak s_fcntl = _libc_fcntl
#pragma weak _s_fcntl = _libc_fcntl
#pragma weak s_ioctl = _s_ioctl

int
_s_ioctl(int fd, int cmd, intptr_t arg)
{
	return (_ioctl(fd, cmd, arg));
}
/* End XXX */
#endif	/* _LP64 */

static int
issock(int fd)
{
	struct stat64 stats;

	if (fstat64(fd, &stats) == -1)
		return (0);
	return (S_ISSOCK(stats.st_mode));
}


int
_libc_fcntl(int fd, int cmd, intptr_t arg)
{
	int	res;
	int	pid;

	switch (cmd) {
	case F_SETOWN:
		pid = (int)arg;
		return (_ioctl(fd, FIOSETOWN, &pid));

	case F_GETOWN:
		if (_ioctl(fd, FIOGETOWN, &res) < 0)
			return (-1);
		return (res);

	case F_SETFL:
		if (issock(fd)) {
			int len = sizeof (res);

			if (_so_getsockopt(fd, SOL_SOCKET, SO_STATE,
			    (char *)&res, &len) < 0)
				return (-1);

			if (arg & FASYNC)
				res |= SS_ASYNC;
			else
				res &= ~SS_ASYNC;
			if (_so_setsockopt(fd, SOL_SOCKET, SO_STATE,
			    (const char *)&res, sizeof (res)) < 0)
				return (-1);
		}
		return (__fcntl(fd, cmd, arg));

	case F_GETFL: {
		int flags;

		if ((flags = __fcntl(fd, cmd, arg)) < 0)
			return (-1);

		if (issock(fd)) {
			/*
			 * See if FASYNC is on.
			 */
			int len = sizeof (res);

			if (_so_getsockopt(fd, SOL_SOCKET, SO_STATE,
			    (char *)&res, &len) < 0)
				return (-1);

			if (res & SS_ASYNC)
				flags |= FASYNC;
		}
		return (flags);
	}

	default:
		return (__fcntl(fd, cmd, arg));
	}
}
