/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ident	"@(#)scalls.c	1.5	99/11/18 SMI"

#ifdef __STDC__
#pragma weak close = _libaio_close
#pragma weak fork = _libaio_fork
#pragma weak aio_close = _libaio_close	/* See bugid 4292483 */
#pragma weak aio_fork = _libaio_fork	/* See bugid 4292483 */
#pragma weak fsync = _libaio_fsync
#if	defined(_LARGEFILE64_SOURCE) && !defined(_LP64)
#pragma weak fsync64 = _libaio_fsync64
#endif /* (_LARGEFILE64_SOURCE) && !defined(_LP64) */
#endif

#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include "libaio.h"

#pragma weak	_cancelon
#pragma weak	_canceloff

extern int __uaio_ok;
extern void _cancelon(void);
extern void _canceloff(void);

int
_libaio_close(int fd)
{
	int	rc;

	if (__uaio_ok)
		aiocancel_all(fd);

	if (_cancelon != NULL)
		_cancelon();

	rc = _close(fd);

	if (_cancelon != NULL)
		_canceloff();

	/*
	 * If the file is successfully closed, clear the
	 * bit for this file, as the next open may re-use this
	 * file descriptor, and the new file may have
	 * different kaio() behaviour
	 */
	if (rc == 0)
		CLEAR_KAIO_SUPPORTED(fd);

	return (rc);

}

pid_t
_libaio_fork(void)
{
	pid_t pid;

	if (__uaio_ok || _kaio_ok) {
		pid = fork1();
		if (pid == 0)
			_aio_forkinit();
		return (pid);
	}
	return (_fork());
}

int
_libaio_fsync(int fd)
{
	aiocb_t cb;

	memset(&cb, 0, sizeof (cb));
	cb.aio_fildes = fd;
	return (__aio_fsync(O_SYNC, &cb, 1));
}

#if	defined(_LARGEFILE64_SOURCE) && !defined(_LP64)

int
_libaio_fsync64(int fd)
{
	aiocb64_t cb;

	memset(&cb, 0, sizeof (cb));
	cb.aio_fildes = fd;
	return (__aio_fsync64(O_SYNC, &cb, 1));
}

#endif /* (_LARGEFILE64_SOURCE) && !defined(_LP64) */
