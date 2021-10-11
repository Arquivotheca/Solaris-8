#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)unix.spec	1.2	99/05/26 SMI"
#
# lib/libpthread/spec/unix.spec

function	alarm extends libc/spec/sys.spec alarm
include		<unistd.h>
version		SUNW_0.9
end		

function	close extends libc/spec/sys.spec close
include		<unistd.h>
version		SUNW_0.9
end		

function	cond_broadcast extends libc/spec/sys.spec cond_broadcast
include		<synch.h>
version		SUNW_0.9
end		

function	cond_destroy extends libc/spec/sys.spec cond_destroy
include		<synch.h>
version		SUNW_0.9
end		

function	cond_init extends libc/spec/sys.spec cond_init
include		<synch.h>
version		SUNW_0.9
end		

function	cond_signal extends libc/spec/sys.spec cond_signal
include		<synch.h>
version		SUNW_0.9
end		

function	cond_timedwait extends libc/spec/sys.spec cond_timedwait
include		<synch.h>, <thread.h>
version		SUNW_0.9
end		

function	cond_wait extends libc/spec/sys.spec cond_wait
include		<synch.h>
version		SUNW_0.9
end		

function	creat extends libc/spec/sys.spec creat
include		<sys/types.h>, <sys/stat.h>, <fcntl.h>
version		SUNW_0.9
end		

function	creat64 extends libc/spec/interface64.spec creat64
arch		i386 sparc
version		SUNW_1.1
end		

function	fcntl extends libc/spec/sys.spec fcntl
version		SUNW_0.9
end		

function	fork extends libc/spec/sys.spec fork
include		<sys/types.h>, <unistd.h>
version		SUNW_0.9
end		

function	fork1 extends libc/spec/sys.spec fork1
include		<sys/types.h>, <unistd.h>
version		SUNW_0.9
end		

function	fsync extends libc/spec/sys.spec fsync
include		<unistd.h>
version		SUNW_0.9
end		

function	msync extends libc/spec/gen.spec msync
include		<sys/mman.h>
version		SUNW_0.9
end		

function	mutex_destroy extends libc/spec/gen.spec mutex_destroy
include		<synch.h>
version		SUNW_0.9
end		

function	mutex_init extends libc/spec/gen.spec mutex_init
include		<synch.h>
version		SUNW_0.9
end		

function	mutex_lock extends libc/spec/gen.spec mutex_lock
include		<synch.h>
version		SUNW_0.9
end		

function	mutex_trylock extends	libc/spec/gen.spec mutex_trylock
include		<synch.h>
version		SUNW_0.9
end		

function	mutex_unlock extends libc/spec/gen.spec mutex_unlock
include		<synch.h>
version		SUNW_0.9
end		

function	open extends libc/spec/sys.spec open
include		<sys/types.h>, <fcntl.h>
version		SUNW_0.9
end		

function	open64 extends libc/spec/interface64.spec open64
include		<sys/types.h>, <fcntl.h>
arch		i386 sparc
version		SUNW_1.1
end		

function	pause extends libc/spec/sys.spec pause
include		<unistd.h>
version		SUNW_0.9
end		

function	pread extends libc/spec/sys.spec pread
include		<unistd.h>, <sys/uio.h>, <limits.h>
version		SUNW_1.2
end		

function	pread64 extends libc/spec/interface64.spec pread64
arch		i386 sparc
version		SUNW_1.2
end		

function	pwrite extends libc/spec/sys.spec pwrite
include		<unistd.h>, <sys/uio.h>
version		SUNW_1.2
end		

function	pwrite64 extends libc/spec/interface64.spec pwrite64
arch		i386 sparc
version		SUNW_1.2
end		

function	read extends libc/spec/sys.spec read
include		<unistd.h>, <sys/uio.h>, <limits.h>
version		SUNW_0.9
end		

function	readv extends libc/spec/sys.spec readv
include		<unistd.h>, <sys/uio.h>, <limits.h>
version		SUNW_1.2
end		

function	rw_rdlock extends libc/spec/gen.spec rw_rdlock
include		<synch.h>
version		SUNW_0.9
end		

function	rw_tryrdlock extends libc/spec/gen.spec rw_tryrdlock
include		<synch.h>
version		SUNW_0.9
end		

function	rw_trywrlock extends libc/spec/gen.spec rw_trywrlock
include		<synch.h>
version		SUNW_0.9
end		

function	rw_unlock extends libc/spec/gen.spec rw_unlock
include		<synch.h>
version		SUNW_0.9
end		

function	rw_wrlock extends libc/spec/gen.spec rw_wrlock
include		<synch.h>
version		SUNW_0.9
end		

function	rwlock_init extends libc/spec/gen.spec rwlock_init
include		<synch.h>
version		SUNW_0.9
end		

function	sema_destroy extends libc/spec/gen.spec sema_destroy
include		<synch.h>
version		SUNW_0.9
end		

function	sema_init extends libc/spec/gen.spec sema_init
include		<synch.h>
version		SUNW_0.9
end		

function	sema_post extends libc/spec/gen.spec sema_post
include		<synch.h>
version		SUNW_0.9
end		

function	sema_trywait extends libc/spec/gen.spec sema_trywait
include		<synch.h>
version		SUNW_0.9
end		

function	sema_wait extends libc/spec/gen.spec sema_wait
include		<synch.h>
version		SUNW_0.9
end		

function	_getfp
version		SUNW_0.9
end		

function	__sigtimedwait
version		SUNWprivate_1.1
end		

function	setitimer
version		SUNW_0.9
end		

function	siglongjmp
version		SUNW_0.9
end		

function	sigsetjmp
version		SUNW_0.9
end		

function	sigaction extends libaio/spec/aio.spec sigaction
include		<signal.h>
version		SUNW_0.9
end		

function	sigprocmask extends libc/spec/sys.spec sigprocmask
include		<signal.h>
version		SUNW_0.9
end		

function	sigsuspend extends libc/spec/sys.spec sigsuspend
include		<signal.h>
version		SUNW_0.9
end		

function	sigwait extends libc/spec/sys.spec sigwait
include		<signal.h>
version		SUNW_0.9
end		

function	sleep extends libc/spec/gen.spec sleep
include		<unistd.h>
version		SUNW_0.9
end		

function	tcdrain extends libc/spec/gen.spec tcdrain
include		<termios.h>
version		SUNW_0.9
end		

function	thr_continue extends libc/spec/stubs.spec thr_continue
include		<thread.h>
version		SUNW_0.9
end		

function	thr_create extends libc/spec/stubs.spec thr_create
include		<thread.h>
version		SUNW_0.9
end		

function	thr_exit extends libc/spec/stubs.spec thr_exit
include		<thread.h>
version		SUNW_0.9
end		

function	thr_getconcurrency extends libc/spec/stubs.spec thr_getconcurrency
include		<thread.h>
version		SUNW_0.9
end		

function	thr_getprio extends libc/spec/stubs.spec thr_getprio
include		<thread.h>
version		SUNW_0.9
end		

function	thr_getspecific extends libc/spec/stubs.spec thr_getspecific
include		<thread.h>
version		SUNW_0.9
end		

function	thr_join extends libc/spec/stubs.spec thr_join
include		<thread.h>
version		SUNW_0.9
end		

function	thr_keycreate extends libc/spec/stubs.spec thr_keycreate
include		<thread.h>
version		SUNW_0.9
end		

function	thr_kill extends libc/spec/stubs.spec thr_kill
include		<thread.h>, <signal.h>
version		SUNW_0.9
end		

function	thr_main extends libc/spec/stubs.spec thr_main
include		<thread.h>
version		SUNW_0.9
end		

function	thr_min_stack extends libc/spec/stubs.spec thr_min_stack
include		<thread.h>
version		SUNW_0.9
end		

function	thr_self extends libc/spec/stubs.spec thr_self
include		<thread.h>
version		SUNW_0.9
end		

function	thr_setconcurrency extends libc/spec/stubs.spec thr_setconcurrency
include		<thread.h>
version		SUNW_0.9
end		

function	thr_setprio extends libc/spec/stubs.spec thr_setprio
include		<thread.h>
version		SUNW_0.9
end		

function	thr_setspecific extends libc/spec/stubs.spec thr_setspecific
include		<thread.h>
version		SUNW_0.9
end		

function	thr_sigsetmask extends libc/spec/stubs.spec thr_sigsetmask
include		<thread.h>, <signal.h>
version		SUNW_0.9
end		

function	thr_stksegment extends libc/spec/stubs.spec thr_stksegment
include		<thread.h>, <sys/signal.h>
version		SUNW_0.9
end		

function	thr_suspend extends libc/spec/stubs.spec thr_suspend
include		<thread.h>
version		SUNW_0.9
end		

function	thr_yield extends libc/spec/stubs.spec thr_yield
include		<thread.h>
version		SUNW_0.9
end		

function	thr_probe_getfunc_addr
version		SUNWprivate_1.1
end		

function	thr_probe_setup
version		SUNWprivate_1.1
end		

function	wait extends libc/spec/sys.spec wait
include		<sys/types.h>, <sys/wait.h>
version		SUNW_0.9
end		

function	waitpid extends libc/spec/gen.spec waitpid
include		<sys/types.h>, <sys/wait.h>
version		SUNW_0.9
end		

function	write extends libc/spec/sys.spec write
include		<unistd.h>, <sys/uio.h>
version		SUNW_0.9
end		

function	writev extends libc/spec/sys.spec writev
include		<unistd.h>, <sys/uio.h>
version		SUNW_1.2
end		

