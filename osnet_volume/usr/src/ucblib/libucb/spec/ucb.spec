#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)ucb.spec 1.1	99/09/21 SMI"
#

function	alphasort64
include		<sys/types.h>
include		<sys/dir.h>
declaration	int alphasort64(struct direct64 **d1, struct direct64 **d2)
arch		sparc i386
version		SUNW_1.1
end

function	fopen64
include		<stdio.h>
declaration	FILE *fopen64(const char *file, const char *mode)
arch		sparc i386
version		SUNW_1.1
end

function	freopen64
include		<stdio.h>
declaration	FILE *freopen64(const char *file, const char *mode, FILE *iop)
arch		sparc i386
version		SUNW_1.1
end

function	readdir64
include		<sys/types.h>
include		<sys/dir.h>
declaration	struct direct64 *readdir64(DIR *dirp)
arch		sparc i386
version		SUNW_1.1
end

function	scandir64
include		<sys/types.h>
include		<sys/dir.h>
declaration	int scandir64(char *dirname, struct direct64 *(*namelist[]), \
			int (*select)(struct direct64 *), \
			int (*dcomp)(struct direct64 **, struct direct64 **))
arch		sparc i386
version		SUNW_1.1
end

function	alphasort
include		<sys/types.h>
include		<sys/dir.h>
declaration	int alphasort(struct direct **d1, struct direct **d2)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	bcmp
include		<sys/types.h>
include		<strings.h>
declaration	int bcmp(const void *s1, const void *s2, size_t len)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	bcopy
include		<sys/types.h>
include		<strings.h>
declaration	void bcopy(const void *s1, void *s2, size_t len)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	bzero
include		<sys/types.h>
include		<strings.h>
declaration	void bzero(void *sp, size_t len)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	flock
include		<sys/types.h>
include		<sys/file.h>
include		<fcntl.h>
declaration	int flock(int fd, int operation)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	fopen
include		<stdio.h>
declaration	FILE *fopen(const char *file, const char *mode)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	fprintf
include		<stdio.h>
declaration	int fprintf(FILE *iop, const char *format, ...)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	freopen
include		<stdio.h>
declaration	FILE *freopen(const char *file, const char *mode, FILE *iop)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	fstatfs
include		<sys/types.h>
include		<sys/vfs.h>
declaration	int fstatfs(int fd, struct statfs *buf)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	ftime
include		<sys/types.h>
include		<sys/timeb.h>
declaration	int ftime(struct timeb *tp)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	getdtablesize
include		<sys/types.h>
include		<sys/time.h>
include		<sys/resource.h>
declaration	int getdtablesize(void)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	gethostid
include		<sys/types.h>
include		<sys/systeminfo.h>
declaration	long gethostid(void)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	gethostname
include		<sys/types.h>
include		<sys/utsname.h>
declaration	int gethostname(char *name, int namelen)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	getpagesize
include		<unistd.h>
declaration	int getpagesize(void)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	getpriority
include		<sys/types.h>
include		<sys/resource.h>
declaration	int getpriority(int which, int who)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	getrusage
include		<sys/types.h>
include		<sys/rusage.h>
declaration	int getrusage(int who, struct rusage *rusage)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	gettimeofday
include		<sys/types.h>
include		<sys/time.h>
declaration	int gettimeofday(struct timeval *tp, void *tzp)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	getwd
include		<stdlib.h>
declaration	char *getwd(char *pathname)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	index
include		<strings.h>
declaration	char *index(char *sp, char c)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	killpg
include		<sys/types.h>
include		<signal.h>
declaration	int killpg(int pgrp, int sig)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	longjmp
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	mctl
include		<sys/types.h>
include		<sys/mman.h>
declaration	int mctl(caddr_t addr, size_t len, int function, int arg)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	nice
include		<sys/resource.h>
declaration	int nice(int incr)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	nlist
include		<sys/types.h>
include		<nlist.h>
declaration	int nlist(const char *name, struct nlist *list)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	printf
include		<stdio.h>
declaration	int printf(const char *format, ...)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	psignal
include		<sys/types.h>
declaration	void psignal(unsigned int sig, char *s)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	rand
include		<stdlib.h>
declaration	int rand(void)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	re_comp
include		<sys/types.h>
include		<stdlib.h>
declaration	char *re_comp(char *sp)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	re_exec
include		<sys/types.h>
include		<stdlib.h>
declaration	int re_exec(char *p1)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	readdir
include		<sys/types.h>
include		<sys/dir.h>
declaration	struct direct *readdir(DIR *dirp)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	reboot
include		<sys/types.h>
declaration	int reboot(int howto, char *bootargs)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	rindex
include		<strings.h>
declaration	char *rindex(char *sp, char c)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	scandir
include		<sys/dir.h>
declaration	int scandir(char *dirname, struct direct *(*namelist[]), \
			int (*)(struct direct *), \
			int (*)(struct direct **, struct direct **))
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	setbuffer
declaration	void setbuffer(FILE *iop, char *abuf, int asize)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	sethostname
declaration	int sethostname(char *name, int namelen)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	setjmp
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	setlinebuf
include		<stdio.h>
declaration	int setlinebuf(FILE *iop)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	setpgrp
include		<sys/types.h>
include		<unistd.h>
declaration	int setpgrp(pid_t pid1, pid_t pid2)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	setpriority
declaration	int setpriority(int which, int who, int prio)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	settimeofday
declaration	int settimeofday(struct timeval *tp, void *tzp)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	sigblock
declaration	int sigblock(int mask)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	siginterrupt
declaration	int siginterrupt(int sig, int flag)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	signal
declaration	void (*signal(int s, void (*a)(int)))(int)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	sigpause
declaration	int sigpause(int mask)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	sigsetmask
declaration	int sigsetmask(int mask)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	sigstack
declaration	int sigstack(struct sigstack *nss, struct sigstack *oss)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	sigvec
declaration	int sigvec(int sig, struct sigvec *nvec, struct sigvec *ovec)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	sigvechandler
include		<sys/types.h>
include		<signal.h>
include		<sys/siginfo.h>
include		<sys/ucontext.h>
declaration	int sigvechandler(int sig, siginfo_t *sip, ucontext_t *ucp)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	sleep
declaration	int sleep(unsigned int n)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	sprintf
include		<stdio.h>
declaration	char *sprintf(const char *string, const char *format, ...)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	srand
include		<stdlib.h>
declaration	void srand(unsigned int x)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	statfs
include		<sys/vfs.h>
declaration	int statfs(char *path, struct statfs *buf)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

data		sys_siglist
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	times
include		<sys/time.h>
include		<sys/times.h>
declaration	clock_t times(struct tms *tmsp)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	ualarm
include		<sys/time.h>
declaration	unsigned int ualarm(unsigned int usecs, unsigned int reload)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	usignal
include		<signal.h>
declaration	void (*usignal(int s, void (*a)(int)))(int)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	usigpause
include		<signal.h>
declaration	int usigpause(int mask)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	usleep
include		<unistd.h>
declaration	int usleep(unsigned int n)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	vfprintf
include		<stdarg.h>
include		<stdio.h>
declaration	int vfprintf(FILE *iop, const char *format, va_list ap)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	vprintf
include		<stdio.h>
include		<stdarg.h>
declaration	int vprintf(const char *format, va_list ap)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	vsprintf
include		<stdio.h>
include		<stdarg.h>
declaration	char *vsprintf(char *string, char *format, va_list ap)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	wait3
include		<sys/types.h>
include		<sys/resource.h>
include		<sys/wait.h>
declaration	pid_t wait3(int *status, int options, struct rusage *rp)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	wait4
include		<sys/types.h>
include		<sys/wait.h>
include		<sys/resource.h>
declaration	pid_t wait4(pid_t pid, int *status, int options, \
			struct rusage *rp)
version		sparc=SUNW_0.7 i386=SUNW_0.7 sparcv9=SUNW_1.1
end

function	_doprnt
version		SUNWprivate_1.1
end

function	_getarg
version		SUNWprivate_1.1
end

function	_longjmp
version		SUNWprivate_1.1
end

function	_mkarglst
version		SUNWprivate_1.1
end

function	_setjmp
version		SUNWprivate_1.1
end

function	_sigblock
version		SUNWprivate_1.1
end

function	_siginterrupt
version		SUNWprivate_1.1
end

function	_sigsetmask
version		SUNWprivate_1.1
end

function	_sigstack
version		SUNWprivate_1.1
end

function	_sigvec
version		SUNWprivate_1.1
end

function	_sigvechandler
version		SUNWprivate_1.1
end

function	ucbsigblock
version		SUNWprivate_1.1
end

function	ucbsiginterrupt
version		SUNWprivate_1.1
end

function	ucbsigpause
version		SUNWprivate_1.1
end

function	ucbsigsetmask
version		SUNWprivate_1.1
end

function	ucbsigvec
version		SUNWprivate_1.1
end

function	__sigcleanup
arch		sparc sparcv9
version		SUNWprivate_1.1
end

function	syscall
arch		sparc
version		SUNWprivate_1.1
end

function	_syscall
arch		i386
version		SUNWprivate_1.1
end

function	_times
arch		i386
version		SUNWprivate_1.1
end
