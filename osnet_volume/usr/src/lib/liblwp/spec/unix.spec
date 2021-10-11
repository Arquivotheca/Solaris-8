#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)unix.spec	1.1	99/10/14 SMI"
#
# lib/liblwp/spec/unix.spec

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
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	cond_destroy extends libc/spec/sys.spec cond_destroy
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	cond_init extends libc/spec/sys.spec cond_init
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	cond_signal extends libc/spec/sys.spec cond_signal
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	cond_timedwait extends libc/spec/sys.spec cond_timedwait
include		<synch.h>, <thread.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	cond_wait extends libc/spec/sys.spec cond_wait
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
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
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
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
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	mutex_init extends libc/spec/gen.spec mutex_init
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	mutex_lock extends libc/spec/gen.spec mutex_lock
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	mutex_trylock extends libc/spec/gen.spec mutex_trylock
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	mutex_unlock extends libc/spec/gen.spec mutex_unlock
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	open extends libc/spec/sys.spec open
include		<sys/types.h>, <sys/stat.h>, <fcntl.h>, <sys/file.h>
version		SUNW_0.9
end		

function	open64 extends libc/spec/interface64.spec open64
include		<sys/types.h>, <sys/stat.h>, <fcntl.h>, <sys/file.h>
arch		i386 sparc
version		SUNW_1.1
end		

function	pause extends libc/spec/sys.spec pause
include		<unistd.h>
version		SUNW_0.9
end		

function	pread extends libc/spec/sys.spec pread
include		<unistd.h>, <sys/uio.h>, <limits.h>
version		SUNW_1.4
end		

function	pread64 extends libc/spec/interface64.spec pread64
arch		i386 sparc
version		SUNW_1.4
end		

function	pwrite extends libc/spec/sys.spec pwrite
include		<unistd.h>, <sys/uio.h>
version		SUNW_1.4
end		

function	pwrite64 extends libc/spec/interface64.spec pwrite64
arch		i386 sparc
version		SUNW_1.4
end		

function	read extends libc/spec/sys.spec read
include		<unistd.h>, <sys/uio.h>, <limits.h>
version		SUNW_0.9
end		

function	readv extends libc/spec/sys.spec readv
include		<unistd.h>, <sys/uio.h>, <limits.h>
version		SUNW_1.4
end		

function	rw_rdlock extends libc/spec/gen.spec rw_rdlock
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	rw_tryrdlock extends libc/spec/gen.spec rw_tryrdlock
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	rw_trywrlock extends libc/spec/gen.spec rw_trywrlock
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	rw_unlock extends libc/spec/gen.spec rw_unlock
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	rw_wrlock extends libc/spec/gen.spec rw_wrlock
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	rwlock_destroy extends libc/spec/gen.spec rwlock_destroy
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	rwlock_init extends libc/spec/gen.spec rwlock_init
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	sema_destroy extends libc/spec/gen.spec sema_destroy
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	sema_init extends libc/spec/gen.spec sema_init
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	sema_post extends libc/spec/gen.spec sema_post
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	sema_trywait extends libc/spec/gen.spec sema_trywait
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	sema_wait extends libc/spec/gen.spec sema_wait
include		<synch.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	_getfp
version		SUNW_0.7
end		

function	__sigtimedwait
version		SUNWprivate_1.1
end		

function	setitimer
version		SUNW_0.9
end		

function	sigaction extends libaio/spec/aio.spec sigaction
include		<signal.h>
version		SUNW_0.7
end		

function	sigprocmask extends libc/spec/sys.spec sigprocmask
include		<signal.h>
version		SUNW_0.7
end		

function	sigsuspend extends libc/spec/sys.spec sigsuspend
include		<signal.h>
version		SUNW_0.9
end		

function	sigwait extends libc/spec/sys.spec sigwait
include		<signal.h>
version		i386=SUNW_0.7 sparc=SISCD_2.3a sparcv9=SUNW_0.7
end		

function	sleep extends libc/spec/gen.spec sleep
include		<unistd.h>
version		SUNW_0.7
end		

function	tcdrain extends libc/spec/gen.spec tcdrain
include		<termios.h>
version		SUNW_0.9
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
version		SUNW_1.4
end		

function	kill extends libc/spec/sys.spec kill
version		SUNW_1.1
end		

function	sigpending extends libc/spec/sys.spec sigpending
version		SUNW_0.9
end		

function	setcontext extends libc/spec/sys.spec setcontext
version		SUNW_0.7
end		

function	_mutex_held extends libc/spec/sys.spec	_mutex_held
version		SUNW_0.7
end		

function	_getsp extends libc/spec/private.spec _getsp
arch		sparc sparcv9
version		SUNWprivate_1.1
end		

function	_rw_read_held extends libc/spec/sys.spec _rw_read_held
version		SUNW_0.7
end		

function	_rw_write_held extends libc/spec/sys.spec _rw_write_held
version		SUNW_0.7
end		

function	_rwlock_destroy extends libc/spec/sys.spec _rwlock_destroy
version		SUNWprivate_1.1
end		

function	_sema_held extends libc/spec/sys.spec	_sema_held
version		SUNW_0.7
end		

function	__gettsp
version		SUNWprivate_1.1
end		

function	_assfail
version		SUNWprivate_1.1
end		

function	_cond_timedwait_cancel
version		SUNWprivate_1.1
end		

function	_cond_wait_cancel
version		SUNWprivate_1.1
end		

function	_sema_wait_cancel
version		SUNWprivate_1.1
end		

function	_sigoff
version		SUNWprivate_1.1
end		

function	_sigon
version		SUNWprivate_1.1
end		

#
# Weak interfaces
#
function	_fork
weak		fork
version		SUNWprivate_1.1
end		

function	_fork1
weak		fork1
version		SUNWprivate_1.1
end		

function	_sigaction
weak		sigaction
version		SUNWprivate_1.1
end		

function	_sigprocmask
weak		sigprocmask
version		SUNWprivate_1.1
end		

function	_sigwait
weak		sigwait
version		SUNWprivate_1.1
end		

function	_sigsuspend
weak		sigsuspend
version		SUNWprivate_1.1
end		

function	_sleep
weak		sleep
version		SUNWprivate_1.1
end		

function	_alarm
weak		alarm
version		SUNWprivate_1.1
end		

function	_setitimer
weak		setitimer
version		SUNWprivate_1.1
end		
