/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 *
 * The POSIX async. I/O functionality is
 * implemented in libaio/common/posix_aio.c
 */

#pragma ident   "@(#)aio.c 1.14     98/12/22     SMI"

/*LINTLIBRARY*/

#include <aio.h>
#include <sys/types.h>
#include <errno.h>
#include "pos4.h"

#pragma weak close = __posix_aio_close
#pragma weak fork = __posix_aio_fork

extern int _libaio_close(int fd);
extern pid_t _libaio_fork(void);

int
__posix_aio_close(int fd)
{
	return (_libaio_close(fd));
}

pid_t
__posix_aio_fork(void)
{
	return (_libaio_fork());
}

int
aio_cancel(int fildes, struct aiocb *aiocbp)
{
	return (__aio_cancel(fildes, aiocbp));
}

#if defined(_LARGEFILE64_SOURCE) && !defined(_LP64)

int
aio_cancel64(int fildes, struct aiocb64 *aiocbp)
{
	return (__aio_cancel64(fildes, aiocbp));
}

#endif

int
aio_error(const struct aiocb *aiocbp)
{
	return (__aio_error(aiocbp));
}

#if defined(_LARGEFILE64_SOURCE) && !defined(_LP64)

int
aio_error64(const struct aiocb64 *aiocbp)
{
	return (__aio_error64(aiocbp));
}

#endif

int
aio_fsync(int op, struct aiocb *aiocbp)
{
	return (__aio_fsync(op, aiocbp, 0));
}

#if defined(_LARGEFILE64_SOURCE) && !defined(_LP64)

int
aio_fsync64(int op, struct aiocb64 *aiocbp)
{
	return (__aio_fsync64(op, aiocbp, 0));
}

#endif

int
aio_read(struct aiocb *aiocbp)
{
	return (__aio_read(aiocbp));
}

#if defined(_LARGEFILE64_SOURCE) && !defined(_LP64)

int
aio_read64(struct aiocb64 *aiocbp)
{
	return (__aio_read64(aiocbp));
}

#endif

ssize_t
aio_return(struct aiocb *aiocbp)
{
	return (__aio_return(aiocbp));
}

#if defined(_LARGEFILE64_SOURCE) && !defined(_LP64)

ssize_t
aio_return64(struct aiocb64 *aiocbp)
{
	return (__aio_return64(aiocbp));
}

#endif

int
aio_suspend(const struct aiocb * const list[], int nent,
    const struct timespec *timeout)
{
	return (__aio_suspend(list, nent, timeout));
}

#if defined(_LARGEFILE64_SOURCE) && !defined(_LP64)

int
aio_suspend64(const struct aiocb64 * const list[], int nent,
    const struct timespec *timeout)
{
	return (__aio_suspend64(list, nent, timeout));
}

#endif

int
aio_write(struct aiocb *aiocbp)
{
	return (__aio_write(aiocbp));
}

#if defined(_LARGEFILE64_SOURCE) && !defined(_LP64)

int
aio_write64(struct aiocb64 *aiocbp)
{
	return (__aio_write64(aiocbp));
}

#endif

int
lio_listio(int mode, struct aiocb * const list[], int nent,
	struct sigevent *sig)
{
	return (__lio_listio(mode, list, nent, sig));
}

#if defined(_LARGEFILE64_SOURCE) && !defined(_LP64)

int
lio_listio64(int mode, struct aiocb64 * const list[], int nent,
	struct sigevent *sig)
{
	return (__lio_listio64(mode, list, nent, sig));
}

#endif
