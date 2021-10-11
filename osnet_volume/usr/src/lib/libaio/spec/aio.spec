#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)aio.spec	1.4	99/11/18 SMI"
#
# lib/libaio/spec/aio.spec

function	aiocancel
include		<sys/asynch.h>, <aio.h>
declaration	int aiocancel(aio_result_t *resultp)
version		sparc=SISCD_2.3 sparcv9=SUNW_0.7 i386=SUNW_0.7 ia64=SUNW_0.7
errno		EACCES EFAULT EINVAL  
exception	$return == -1
end

function	aioread
include		<sys/types.h>, <sys/asynch.h>, <aio.h>
declaration	int aioread(int fildes, char *bufp, int bufs, \
			off_t offset, int whence, aio_result_t *resultp)
version		sparc=SISCD_2.3 sparcv9=SUNW_0.7 i386=SUNW_0.7 ia64=SUNW_0.7
errno		EAGAIN EBADF EFAULT EINVAL ENOMEM  
exception	$return == -1
end

function	aioread64
declaration	int aioread64(int fd, caddr_t buf, int bufsz, off64_t offset, \
			int whence, aio_result_t *resultp)
version		i386=SUNW_1.0 sparc=SUNW_1.0
end

function	aiowait
include		<sys/asynch.h>, <aio.h>, <sys/time.h>
declaration	aio_result_t *aiowait(struct timeval *timeout)
version		sparc=SISCD_2.3 sparcv9=SUNW_0.7 i386=SUNW_0.7 ia64=SUNW_0.7
errno		EFAULT EINTR EINVAL  
exception	$return == (aio_result_t *)-1
end

function	aiowrite
include		<sys/types.h>, <sys/asynch.h>, <aio.h>
declaration	int aiowrite(int fildes, char *bufp, int bufs, \
			off_t offset, int whence, aio_result_t *resultp)
version		sparc=SISCD_2.3 sparcv9=SUNW_0.7 i386=SUNW_0.7 ia64=SUNW_0.7
errno		EAGAIN EBADF EFAULT EINVAL ENOMEM
exception	$return == -1
end

function	aiowrite64
include		<sys/types.h>, <sys/asynch.h>, <aio.h>
declaration	int aiowrite64(int fildes, char *bufp, int bufs, \
			off64_t offset, int whence, aio_result_t *resultp)
version		sparc=SUNW_1.0 i386=SUNW_1.0
errno		EAGAIN EBADF EFAULT EINVAL ENOMEM
exception	$return == -1
end

function	assfail
declaration	int assfail(char *a, char *f, int l)
version		SUNW_1.1
end

function	close
include		<unistd.h>
declaration	int close(int fildes)
version		SUNW_0.7
errno		EBADF EINTR ENOLINK EIO
exception	$return == -1
end

function	fork
declaration	pid_t fork(void)
version		SUNW_0.7
exception	$return == -1
end

function	sigaction
include		<signal.h>
declaration	int sigaction(int sig, const struct sigaction *act,\
			struct sigaction *oact)
version		SUNW_0.7
errno		EINVAL EFAULT
exception	$return == -1
end

function	signal extends  libc/spec/gen.spec
include		<signal.h>
version		SUNW_0.7
end

function	sigset extends  libc/spec/gen.spec
include		<signal.h>
version		SUNW_0.7
end

function	sigignore extends  libc/spec/gen.spec
include		<signal.h>
version		SUNW_0.7
end

data		_pagesize
version		SUNWprivate_1.1
end

function	_aiosigaction
declaration	int _aiosigaction(int sig, const struct sigaction *act, \
			struct sigaction *oact)
version		SUNWprivate_1.1
end

function	__lio_listio
declaration	int __lio_listio(int mode, aiocb_t * const list[], int nent, \
			struct sigevent *sig)
version		SUNWprivate_1.1
end

function	__aio_suspend
declaration	int __aio_suspend(aiocb_t *list[], int nent, \
			const timespec_t *timo)
version		SUNWprivate_1.1
end

function	__aio_error
declaration	int __aio_error(aiocb_t *cb)
version		SUNWprivate_1.1
end

function	__aio_return
declaration	ssize_t __aio_return(aiocb_t *cb)
version		SUNWprivate_1.1
end

function	__aio_read
declaration	int __aio_read(aiocb_t *cb)
version		SUNWprivate_1.1
end

function	__aio_write
declaration	int __aio_write(aiocb_t *cb)
version		SUNWprivate_1.1
end

function	__aio_fsync
declaration	int __aio_fsync(int op, aiocb_t *aiocbp)
version		SUNWprivate_1.1
end

function	__aio_cancel
declaration	int __aio_cancel(int fd, aiocb_t *aiocbp)
version		SUNWprivate_1.1
end

function	__lio_listio64
declaration	int __lio_listio64(int mode, aiocb64_t * const list[], \
			int nent, struct sigevent *sig)
version		sparc=SUNWprivate_1.1 i386=SUNWprivate_1.1
end

function	__aio_suspend64
declaration	int __aio_suspend64(aiocb64_t *list[], int nent, const \
			timespec_t *timo)
version		sparc=SUNWprivate_1.1 i386=SUNWprivate_1.1
end

function	__aio_error64
declaration	int __aio_error64(aiocb64_t *cb)
version		sparc=SUNWprivate_1.1 i386=SUNWprivate_1.1
end

function	__aio_return64
declaration	ssize_t __aio_return64(aiocb64_t *cb)
version		sparc=SUNWprivate_1.1 i386=SUNWprivate_1.1
end

function	__aio_read64
declaration	int __aio_read64(aiocb64_t *cb)
version		sparc=SUNWprivate_1.1 i386=SUNWprivate_1.1
end

function	__aio_write64
declaration	int __aio_write64(aiocb64_t *cb)
version		sparc=SUNWprivate_1.1 i386=SUNWprivate_1.1
end

function	__aio_fsync64
declaration	int __aio_fsync64(int op, aiocb64_t *aiocbp)
version		sparc=SUNWprivate_1.1 i386=SUNWprivate_1.1
end

function	__aio_cancel64
declaration	int __aio_cancel64(int fd, aiocb64_t *aiocbp)
version		sparc=SUNWprivate_1.1 i386=SUNWprivate_1.1
end

function	__libaio_fdsync
declaration	int __libaio_fdsync(int, int, int *)
arch		sparc i386
version		SUNWprivate_1.1
end

function	__libaio_pread
declaration	ssize_t __libaio_pread(int, void *, size_t, offset_t, int *)
version		SUNWprivate_1.1
end

function	__libaio_pwrite
declaration	ssize_t __libaio_pwrite(int, void *, size_t, offset_t, int *)
version		SUNWprivate_1.1
end

function	__libaio_read
declaration	ssize_t __libaio_read(int, void *, size_t, int *)
arch		sparc i386
version		SUNWprivate_1.1
end

function	__libaio_write
declaration	ssize_t __libaio_write(int, void *, size_t, int *)
arch		sparc i386
version		SUNWprivate_1.1
end

function	_libaio_close
version		SUNWprivate_1.1
end

function	_libaio_fork
version		SUNWprivate_1.1
end

function	_libaio_fsync
version		SUNWprivate_1.1
end

#
# Weak interfaces, see bugid 4292483 concerning aio_close() & aio_fork()
#
function	aio_close
weak		_libaio_close
version		SUNWprivate_1.1
end

function	aio_fork
weak		_libaio_fork
version		SUNWprivate_1.1
end
