/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)expound.c	1.46	99/10/25 SMI"

#define	_SYSCALL32

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <libproc.h>
#include <string.h>
#include <limits.h>
#include <sys/statfs.h>
#include <sys/times.h>
#include <sys/utssys.h>
#include <sys/utsname.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/dirent.h>
#include <sys/utime.h>
#include <ustat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/termios.h>
#include <sys/termiox.h>
#include <sys/termio.h>
#include <sys/ttold.h>
#include <sys/jioctl.h>
#include <sys/filio.h>
#include <stropts.h>
#include <poll.h>
#include <sys/uio.h>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/aio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/byteorder.h>
#include <arpa/inet.h>
#include <sys/audioio.h>
#include <sys/cladm.h>
#include <sys/synch.h>
#include <sys/synch32.h>

#include "ramdata.h"
#include "systable.h"
#include "proto.h"

/*
 * Function prototypes for static routines in this module.
 */
static	void	show_utime(struct ps_prochandle *);
static	void	show_utimes(struct ps_prochandle *);
static	void	show_timeofday(struct ps_prochandle *);
static	void	show_itimerval(struct ps_prochandle *, long, const char *);
static	void	show_timeval(struct ps_prochandle *, long, const char *);
static	void	show_timestruc(struct ps_prochandle *, long, const char *);
static	void	show_stime(void);
static	void	show_times(struct ps_prochandle *);
static	void	show_utssys(struct ps_prochandle *, long);
static	void	show_uname(struct ps_prochandle *, long);
static	void	show_nuname(struct ps_prochandle *, long);
static	void	show_ustat(struct ps_prochandle *, long);
static	void	show_fusers(struct ps_prochandle *, long, long);
static	void	show_ioctl(struct ps_prochandle *, int, long);
static	void	show_termio(struct ps_prochandle *, long);
static	void	show_termios(struct ps_prochandle *, long);
static	void	show_termiox(struct ps_prochandle *, long);
static	void	show_sgttyb(struct ps_prochandle *, long);
static	void	show_tchars(struct ps_prochandle *, long);
static	void	show_ltchars(struct ps_prochandle *, long);
static	char	*show_char(char *, int);
static	void	show_termcb(struct ps_prochandle *, long);
static	void	show_strint(struct ps_prochandle *, int, long);
static	void	show_strioctl(struct ps_prochandle *, long);
static	void	show_strpeek(struct ps_prochandle *, long);
static	void	show_strfdinsert(struct ps_prochandle *, long);
static	const char *strflags(int);
static	void	show_strrecvfd(struct ps_prochandle *, long);
static	void	show_strlist(struct ps_prochandle *, long);
static	void	show_jwinsize(struct ps_prochandle *, long);
static	void	show_winsize(struct ps_prochandle *, long);
static	void	show_statvfs(struct ps_prochandle *);
static	void	show_statvfs64(struct ps_prochandle *);
static	void	show_statfs(struct ps_prochandle *);
static	void	show_fcntl(struct ps_prochandle *);
static	void	show_flock32(struct ps_prochandle *, long);
static	void	show_flock64(struct ps_prochandle *, long);
static	void	show_share(struct ps_prochandle *, long);
static	void	show_gp_msg(struct ps_prochandle *, int);
static	void	show_strbuf(struct ps_prochandle *, long, const char *, int);
static	void	print_strbuf(struct ps_prochandle *,
			struct strbuf *, const char *, int);
static	void	show_int(struct ps_prochandle *, long, const char *);
static	void	show_hhex_int(struct ps_prochandle *, long, const char *);
static	void	show_poll(struct ps_prochandle *);
static	void	show_pollfd(struct ps_prochandle *, long);
static	const char *pollevent(int);
static	void	show_msgsys(struct ps_prochandle *, long);
static	void	show_msgctl(struct ps_prochandle *, long);
static	void	show_msgbuf(struct ps_prochandle *, long, long);
static	void	show_semsys(struct ps_prochandle *);
static	void	show_semctl(struct ps_prochandle *, long);
static	void	show_semop(struct ps_prochandle *, long, long);
static	void	show_shmsys(struct ps_prochandle *);
static	void	show_shmctl(struct ps_prochandle *, long);
static	void	show_groups(struct ps_prochandle *, long, long);
static	void	show_sigset(struct ps_prochandle *, long, const char *);
static	char	*sigset_string(sigset_t *);
static	void	show_sigaltstack(struct ps_prochandle *, long, const char *);
static	void	show_sigaction(struct ps_prochandle *,
			long, const char *, long);
static	void	show_siginfo(struct ps_prochandle *, long);
static	void	show_bool(struct ps_prochandle *, long, int);
static	void	show_iovec(struct ps_prochandle *, long, long, int, long);
static	void	show_dents32(struct ps_prochandle *, long, long);
static	void	show_dents64(struct ps_prochandle *, long, long);
static	void	show_rlimit32(struct ps_prochandle *, long);
static	void	show_rlimit64(struct ps_prochandle *, long);
static	void	show_adjtime(struct ps_prochandle *, long, long);
static	void	show_sockaddr(struct ps_prochandle *,
			const char *, long, long, long);
static	void	show_msghdr(struct ps_prochandle *, long);
static	void	prtime(const char *, time_t);
static	void	show_audio_info(struct ps_prochandle *, long);
static	void	show_audio_prinfo(const char *, struct audio_prinfo *);
static	void	show_audio_ports(const char *, const char *, uint_t);
static	void	show_cladm(struct ps_prochandle *, int, int, long);
static	void	show_mutex(struct ps_prochandle *, long);
static	void	show_condvar(struct ps_prochandle *, long);
static	void	show_sema(struct ps_prochandle *, long);

#ifdef _LP64
static	void	show_utssys32(struct ps_prochandle *, long);
static	void	show_ustat32(struct ps_prochandle *, long);
static	void	show_strioctl32(struct ps_prochandle *, long);
static	void	show_strpeek32(struct ps_prochandle *, long);
static	void	show_strfdinsert32(struct ps_prochandle *, long);
static	void	show_strlist32(struct ps_prochandle *, long);
static	void	show_statfs32(struct ps_prochandle *);
static	void	show_strbuf32(struct ps_prochandle *, long, const char *, int);
static	void print_strbuf32(struct ps_prochandle *,
			struct strbuf32 *, const char *, int);
static	void	show_msgctl32(struct ps_prochandle *, long);
static	void	show_msgbuf32(struct ps_prochandle *, long, long msgsz);
static	void	show_semctl32(struct ps_prochandle *, long);
static	void	show_shmctl32(struct ps_prochandle *, long);
static	void	show_sigaltstack32(struct ps_prochandle *, long, const char *);
static	void	show_sigaction32(struct ps_prochandle *,
			long, const char *, long);
static	void	show_siginfo32(struct ps_prochandle *, long);
static	void	print_siginfo32(const siginfo32_t *);
static	void	show_iovec32(struct ps_prochandle *, long, int, int, long);
static	void	show_msghdr32(struct ps_prochandle *, long);
static	void	show_statvfs32(struct ps_prochandle *);
#endif	/* _LP64 */

/* expound verbosely upon syscall arguments */
/*ARGSUSED*/
void
expound(struct ps_prochandle *Pr, long r0, int raw)
{
	int lp64 = (data_model == PR_MODEL_LP64);
	int what = Pstatus(Pr)->pr_lwp.pr_what;
	int err = Errno;		/* don't display output parameters */
					/* for a failed system call */
#ifndef _LP64
	/* We are a 32-bit truss; we can't grok a 64-bit process */
	if (lp64)
		return;
#endif
	switch (what) {
	case SYS_utime:
		show_utime(Pr);
		break;
	case SYS_utimes:
		show_utimes(Pr);
		break;
	case SYS_gettimeofday:
		if (!err)
			show_timeofday(Pr);
		break;
	case SYS_getitimer:
		if (!err && sys_nargs > 1)
			show_itimerval(Pr, (long)sys_args[1], " value");
		break;
	case SYS_setitimer:
		if (sys_nargs > 1)
			show_itimerval(Pr, (long)sys_args[1], " value");
		if (!err && sys_nargs > 2)
			show_itimerval(Pr, (long)sys_args[2], "ovalue");
		break;
	case SYS_stime:
		show_stime();
		break;
	case SYS_times:
		if (!err)
			show_times(Pr);
		break;
	case SYS_utssys:
		if (err)
			break;
#ifdef _LP64
		if (lp64)
			show_utssys(Pr, r0);
		else
			show_utssys32(Pr, r0);
#else
		show_utssys(Pr, r0);
#endif
		break;
	case SYS_ioctl:
		if (sys_nargs >= 3)	/* each case must decide for itself */
			show_ioctl(Pr, sys_args[1], (long)sys_args[2]);
		break;
	case SYS_stat:
	case SYS_fstat:
	case SYS_lstat:
		if (!err && sys_nargs >= 2)
			show_stat(Pr, (long)sys_args[1]);
		break;
	case SYS_stat64:
	case SYS_fstat64:
	case SYS_lstat64:
		if (!err && sys_nargs >= 2)
			show_stat64_32(Pr, (long)sys_args[1]);
		break;
	case SYS_xstat:
	case SYS_fxstat:
	case SYS_lxstat:
		if (!err && sys_nargs >= 3)
			show_xstat(Pr, (int)sys_args[0], (long)sys_args[2]);
		break;
	case SYS_statvfs:
	case SYS_fstatvfs:
		if (err)
			break;
#ifdef _LP64
		if (!lp64) {
			show_statvfs32(Pr);
			break;
		}
#endif
		show_statvfs(Pr);
		break;
	case SYS_statvfs64:
	case SYS_fstatvfs64:
		if (err)
			break;
		show_statvfs64(Pr);
		break;
	case SYS_statfs:
	case SYS_fstatfs:
		if (err)
			break;
#ifdef _LP64
		if (lp64)
			show_statfs(Pr);
		else
			show_statfs32(Pr);
#else
		show_statfs(Pr);
#endif
		break;
	case SYS_fcntl:
		show_fcntl(Pr);
		break;
	case SYS_msgsys:
		show_msgsys(Pr, r0);	/* each case must decide for itself */
		break;
	case SYS_semsys:
		show_semsys(Pr);	/* each case must decide for itself */
		break;
	case SYS_shmsys:
		show_shmsys(Pr);	/* each case must decide for itself */
		break;
	case SYS_getdents:
		if (err || sys_nargs <= 1 || r0 <= 0)
			break;
#ifdef _LP64
		if (!lp64) {
			show_dents32(Pr, (long)sys_args[1], r0);
			break;
		}
		/* FALLTHROUGH */
#else
		show_dents32(Pr, (long)sys_args[1], r0);
		break;
#endif
	case SYS_getdents64:
		if (err || sys_nargs <= 1 || r0 <= 0)
			break;
		show_dents64(Pr, (long)sys_args[1], r0);
		break;
	case SYS_getmsg:
		show_gp_msg(Pr, what);
		if (sys_nargs > 3)
			show_hhex_int(Pr, (long)sys_args[3], "flags");
		break;
	case SYS_getpmsg:
		show_gp_msg(Pr, what);
		if (sys_nargs > 3)
			show_hhex_int(Pr, (long)sys_args[3], "band");
		if (sys_nargs > 4)
			show_hhex_int(Pr, (long)sys_args[4], "flags");
		break;
	case SYS_putmsg:
	case SYS_putpmsg:
		show_gp_msg(Pr, what);
		break;
	case SYS_poll:
		show_poll(Pr);
		break;
	case SYS_setgroups:
		if (sys_nargs > 1 && (r0 = sys_args[0]) > 0)
			show_groups(Pr, (long)sys_args[1], r0);
		break;
	case SYS_getgroups:
		if (!err && sys_nargs > 1 && sys_args[0] > 0)
			show_groups(Pr, (long)sys_args[1], r0);
		break;
	case SYS_sigprocmask:
		if (sys_nargs > 1)
			show_sigset(Pr, (long)sys_args[1], " set");
		if (!err && sys_nargs > 2)
			show_sigset(Pr, (long)sys_args[2], "oset");
		break;
	case SYS_sigsuspend:
	case SYS_sigtimedwait:
		if (sys_nargs > 0)
			show_sigset(Pr, (long)sys_args[0], "sigmask");
		if (!err && sys_nargs > 1)
			show_siginfo(Pr, (long)sys_args[1]);
		if (sys_nargs > 2)
			show_timestruc(Pr, (long)sys_args[2], "timeout");
		break;
	case SYS_sigaltstack:
		if (sys_nargs > 0)
			show_sigaltstack(Pr, (long)sys_args[0], "new");
		if (!err && sys_nargs > 1)
			show_sigaltstack(Pr, (long)sys_args[1], "old");
		break;
	case SYS_sigaction:
		if (sys_nargs > 1)
			show_sigaction(Pr, (long)sys_args[1], "new", NULL);
		if (!err && sys_nargs > 2)
			show_sigaction(Pr, (long)sys_args[2], "old", r0);
		break;
	case SYS_sigpending:
		if (!err && sys_nargs > 1)
			show_sigset(Pr, (long)sys_args[1], "sigmask");
		break;
	case SYS_waitsys:
		if (!err && sys_nargs > 2)
			show_siginfo(Pr, (long)sys_args[2]);
		break;
	case SYS_sigsendsys:
		if (sys_nargs > 0)
			show_procset(Pr, (long)sys_args[0]);
		break;
	case SYS_priocntlsys:
		if (sys_nargs > 1)
			show_procset(Pr, (long)sys_args[1]);
		break;
	case SYS_mincore:
		if (!err && sys_nargs > 2)
			show_bool(Pr, (long)sys_args[2],
				(sys_args[1]+pagesize-1)/pagesize);
		break;
	case SYS_readv:
	case SYS_writev:
		if (sys_nargs > 2) {
			int i = sys_args[0]+1;
			int showbuf = FALSE;
			long nb = (what == SYS_readv)? r0 : 32*1024;

			if ((what == SYS_readv && !err &&
			    prismember(&readfd, i)) ||
			    (what == SYS_writev &&
			    prismember(&writefd, i)))
				showbuf = TRUE;
			show_iovec(Pr, (long)sys_args[1], sys_args[2],
				showbuf, nb);
		}
		break;
	case SYS_getrlimit:
		if (err)
			break;
		/*FALLTHROUGH*/
	case SYS_setrlimit:
		if (sys_nargs <= 1)
			break;
#ifdef _LP64
		if (lp64)
			show_rlimit64(Pr, (long)sys_args[1]);
		else
			show_rlimit32(Pr, (long)sys_args[1]);
#else
		show_rlimit32(Pr, (long)sys_args[1]);
#endif
		break;
	case SYS_getrlimit64:
		if (err)
			break;
		/*FALLTHROUGH*/
	case SYS_setrlimit64:
		if (sys_nargs <= 1)
			break;
		show_rlimit64(Pr, (long)sys_args[1]);
		break;
	case SYS_uname:
		if (!err && sys_nargs > 0)
			show_nuname(Pr, (long)sys_args[0]);
		break;
	case SYS_adjtime:
		if (!err && sys_nargs > 1)
			show_adjtime(Pr, (long)sys_args[0], (long)sys_args[1]);
		break;
	case SYS_lwp_info:
		if (!err && sys_nargs > 0)
			show_timestruc(Pr, (long)sys_args[0], "cpu time");
		break;
	case SYS_lwp_wait:
		if (!err && sys_nargs > 1)
			show_int(Pr, (long)sys_args[1], "lwpid");
		break;
	case SYS_lwp_mutex_wakeup:
	case SYS_lwp_mutex_lock:
	case SYS_lwp_mutex_unlock:
	case SYS_lwp_mutex_trylock:
	case SYS_lwp_mutex_init:
		if (sys_nargs > 0)
			show_mutex(Pr, (long)sys_args[0]);
		break;
	case SYS_lwp_cond_wait:
		if (sys_nargs > 0)
			show_condvar(Pr, (long)sys_args[0]);
		if (sys_nargs > 1)
			show_mutex(Pr, (long)sys_args[1]);
		if (sys_nargs > 2)
			show_timestruc(Pr, (long)sys_args[2], "timeout");
		break;
	case SYS_lwp_cond_signal:
	case SYS_lwp_cond_broadcast:
		if (sys_nargs > 0)
			show_condvar(Pr, (long)sys_args[0]);
		break;
	case SYS_lwp_sema_wait:
	case SYS_lwp_sema_trywait:
	case SYS_lwp_sema_post:
		if (sys_nargs > 0)
			show_sema(Pr, (long)sys_args[0]);
		break;
	case SYS_lwp_sigredirect:
		if (!err && sys_nargs > 2)
			show_int(Pr, (long)sys_args[2], "queued");
		break;
	case SYS_lwp_sigtimedwait:
		if (sys_nargs > 0)
			show_sigset(Pr, (long)sys_args[0], "sigmask");
		if (!err && sys_nargs > 1)
			show_siginfo(Pr, (long)sys_args[1]);
		if (sys_nargs > 2)
			show_timestruc(Pr, (long)sys_args[2], "timeout");
		if (!err && sys_nargs > 3)
			show_int(Pr, (long)sys_args[3], "queued");
		break;
	case SYS_lwp_create:
		/* XXX print some values in ucontext ??? */
		if (!err && sys_nargs > 2)
			show_int(Pr, (long)sys_args[2], "lwpid");
		break;
	case SYS_kaio:
		if (sys_args[0] == AIOWAIT && !err && sys_nargs > 1)
			show_timeval(Pr, (long)sys_args[1], "timeout");
		break;
	case SYS_bind:
	case SYS_connect:
		if (sys_nargs > 2)
			show_sockaddr(Pr, "name", (long)sys_args[1], 0,
					(long)sys_args[2]);
		break;
	case SYS_sendto:
		if (sys_nargs > 5)
			show_sockaddr(Pr, "to", (long)sys_args[4], 0,
					sys_args[5]);
		break;
	case SYS_accept:
		if (!err && sys_nargs > 2)
			show_sockaddr(Pr, "name", (long)sys_args[1],
				(long)sys_args[2], 0);
		break;
	case SYS_getsockname:
	case SYS_getpeername:
		if (!err && sys_nargs > 2)
			show_sockaddr(Pr, "name", (long)sys_args[1],
				(long)sys_args[2], 0);
		break;
	case SYS_cladm:
		if (!err && sys_nargs > 2)
			show_cladm(Pr, sys_args[0], sys_args[1],
			    (long)sys_args[2]);
		break;
	case SYS_recvfrom:
		if (!err && sys_nargs > 5)
			show_sockaddr(Pr, "from", (long)sys_args[4],
				(long)sys_args[5], 0);
		break;
	case SYS_recvmsg:
		if (err)
			break;
		/* FALLTHROUGH */
	case SYS_sendmsg:
		if (sys_nargs <= 1)
			break;
#ifdef _LP64
		if (lp64)
			show_msghdr(Pr, sys_args[1]);
		else
			show_msghdr32(Pr, sys_args[1]);
#else
		show_msghdr(Pr, sys_args[1]);
#endif
		break;
	}
}

static void
show_utime(struct ps_prochandle *Pr)
{
	long offset;
	struct utimbuf utimbuf;

	if (sys_nargs < 2 || (offset = sys_args[1]) == NULL)
		return;

	if (data_model == PR_MODEL_NATIVE) {
		if (Pread(Pr, &utimbuf, sizeof (utimbuf), offset)
		    != sizeof (utimbuf))
			return;
	} else {
		struct utimbuf32 utimbuf32;

		if (Pread(Pr, &utimbuf32, sizeof (utimbuf32), offset)
		    != sizeof (utimbuf32))
			return;

		utimbuf.actime = (time_t)utimbuf32.actime;
		utimbuf.modtime = (time_t)utimbuf32.modtime;
	}

	/* print access and modification times */
	prtime("atime: ", utimbuf.actime);
	prtime("mtime: ", utimbuf.modtime);
}

static void
show_utimes(struct ps_prochandle *Pr)
{
	long offset;
	struct {
		struct timeval	atime;
		struct timeval	mtime;
	} utimbuf;

	if (sys_nargs < 2 || (offset = sys_args[1]) == NULL)
		return;

	if (data_model == PR_MODEL_NATIVE) {
		if (Pread(Pr, &utimbuf, sizeof (utimbuf), offset)
		    != sizeof (utimbuf))
			return;
	} else {
		struct {
			struct timeval32 atime;
			struct timeval32 mtime;
		} utimbuf32;

		if (Pread(Pr, &utimbuf32, sizeof (utimbuf32), offset)
		    != sizeof (utimbuf32))
			return;

		TIMEVAL32_TO_TIMEVAL(&utimbuf.atime, &utimbuf32.atime);
		TIMEVAL32_TO_TIMEVAL(&utimbuf.mtime, &utimbuf32.mtime);
	}

	/* print access and modification times */
	prtime("atime: ", utimbuf.atime.tv_sec);
	prtime("mtime: ", utimbuf.mtime.tv_sec);
}

static void
show_timeofday(struct ps_prochandle *Pr)
{
	struct timeval tod;
	long offset;

	if (sys_nargs < 1 || (offset = sys_args[0]) == NULL)
		return;

	if (data_model == PR_MODEL_NATIVE) {
		if (Pread(Pr, &tod, sizeof (tod), offset)
		    != sizeof (tod))
			return;
	} else {
		struct timeval32 tod32;

		if (Pread(Pr, &tod32, sizeof (tod32), offset)
		    != sizeof (tod32))
			return;

		TIMEVAL32_TO_TIMEVAL(&tod, &tod32);
	}

	prtime("time: ", tod.tv_sec);
}

static void
show_itimerval(struct ps_prochandle *Pr, long offset, const char *name)
{
	struct itimerval itimerval;

	if (offset == NULL)
		return;

	if (data_model == PR_MODEL_NATIVE) {
		if (Pread(Pr, &itimerval, sizeof (itimerval), offset)
		    != sizeof (itimerval))
			return;
	} else {
		struct itimerval32 itimerval32;

		if (Pread(Pr, &itimerval32, sizeof (itimerval32), offset)
		    != sizeof (itimerval32))
			return;

		ITIMERVAL32_TO_ITIMERVAL(&itimerval, &itimerval32);
	}

	(void) printf(
	    "%s\t%s:  interval: %4ld.%6.6ld sec  value: %4ld.%6.6ld sec\n",
	    pname,
	    name,
	    itimerval.it_interval.tv_sec,
	    itimerval.it_interval.tv_usec,
	    itimerval.it_value.tv_sec,
	    itimerval.it_value.tv_usec);
}

static void
show_timeval(struct ps_prochandle *Pr, long offset, const char *name)
{
	struct timeval timeval;

	if (offset == NULL)
		return;

	if (data_model == PR_MODEL_NATIVE) {
		if (Pread(Pr, &timeval, sizeof (timeval), offset)
		    != sizeof (timeval))
			return;
	} else {
		struct timeval32 timeval32;

		if (Pread(Pr, &timeval32, sizeof (timeval32), offset)
		    != sizeof (timeval32))
			return;

		TIMEVAL32_TO_TIMEVAL(&timeval, &timeval32);
	}

	(void) printf(
	    "%s\t%s: %ld.%6.6ld sec\n",
	    pname,
	    name,
	    timeval.tv_sec,
	    timeval.tv_usec);
}

static void
show_timestruc(struct ps_prochandle *Pr, long offset, const char *name)
{
	timestruc_t timestruc;

	if (offset == NULL)
		return;

	if (data_model == PR_MODEL_NATIVE) {
		if (Pread(Pr, &timestruc, sizeof (timestruc), offset)
		    != sizeof (timestruc))
			return;
	} else {
		timestruc32_t timestruc32;

		if (Pread(Pr, &timestruc32, sizeof (timestruc32), offset)
		    != sizeof (timestruc32))
			return;

		TIMESPEC32_TO_TIMESPEC(&timestruc, &timestruc32);
	}

	(void) printf(
	    "%s\t%s: %ld.%9.9ld sec\n",
	    pname,
	    name,
	    timestruc.tv_sec,
	    timestruc.tv_nsec);
}

static void
show_stime()
{
	if (sys_nargs >= 1) {
		/* print new system time */
		prtime("systime = ", (time_t)sys_args[0]);
	}
}

static void
show_times(struct ps_prochandle *Pr)
{
	long hz = sysconf(_SC_CLK_TCK);
	long offset;
	struct tms tms;

	if (sys_nargs < 1 || (offset = sys_args[0]) == NULL)
		return;

	if (data_model == PR_MODEL_NATIVE) {
		if (Pread(Pr, &tms, sizeof (tms), offset)
		    != sizeof (tms))
			return;
	} else {
		struct tms32 tms32;

		if (Pread(Pr, &tms32, sizeof (tms32), offset)
		    != sizeof (tms32))
			return;

		/*
		 * This looks a bit odd (since the values are actually
		 * signed), but we need to suppress sign extension to
		 * preserve compatibility (we've always printed these
		 * numbers as unsigned quantities).
		 */
		tms.tms_utime = (unsigned)tms32.tms_utime;
		tms.tms_stime = (unsigned)tms32.tms_stime;
		tms.tms_cutime = (unsigned)tms32.tms_cutime;
		tms.tms_cstime = (unsigned)tms32.tms_cstime;
	}

	(void) printf(
	    "%s\tutim=%-6lu stim=%-6lu cutim=%-6lu cstim=%-6lu (HZ=%ld)\n",
	    pname,
	    tms.tms_utime,
	    tms.tms_stime,
	    tms.tms_cutime,
	    tms.tms_cstime,
	    hz);
}

static void
show_utssys(struct ps_prochandle *Pr, long r0)
{
	if (sys_nargs >= 3) {
		switch (sys_args[2]) {
		case UTS_UNAME:
			show_uname(Pr, (long)sys_args[0]);
			break;
		case UTS_USTAT:
			show_ustat(Pr, (long)sys_args[0]);
			break;
		case UTS_FUSERS:
			show_fusers(Pr, (long)sys_args[3], r0);
			break;
		}
	}
}

#ifdef _LP64
static void
show_utssys32(struct ps_prochandle *Pr, long r0)
{
	if (sys_nargs >= 3) {
		switch (sys_args[2]) {
		case UTS_UNAME:
			show_uname(Pr, (long)sys_args[0]);
			break;
		case UTS_USTAT:
			show_ustat32(Pr, (long)sys_args[0]);
			break;
		case UTS_FUSERS:
			show_fusers(Pr, (long)sys_args[3], r0);
			break;
		}
	}
}
#endif	/* _LP64 */

static void
show_uname(struct ps_prochandle *Pr, long offset)
{
	/*
	 * Old utsname buffer (no longer accessible in <sys/utsname.h>).
	 */
	struct {
		char	sysname[9];
		char	nodename[9];
		char	release[9];
		char	version[9];
		char	machine[9];
	} ubuf;

	if (offset != NULL &&
	    Pread(Pr, &ubuf, sizeof (ubuf), offset) == sizeof (ubuf)) {
		(void) printf(
		"%s\tsys=%-9.9snod=%-9.9srel=%-9.9sver=%-9.9smch=%.9s\n",
			pname,
			ubuf.sysname,
			ubuf.nodename,
			ubuf.release,
			ubuf.version,
			ubuf.machine);
	}
}

/* XX64 -- definition of 'struct ustat' is strange -- check out the defn */
static void
show_ustat(struct ps_prochandle *Pr, long offset)
{
	struct ustat ubuf;

	if (offset != NULL &&
	    Pread(Pr, &ubuf, sizeof (ubuf), offset) == sizeof (ubuf)) {
		(void) printf(
		"%s\ttfree=%-6ld tinode=%-5lu fname=%-6.6s fpack=%-.6s\n",
			pname,
			ubuf.f_tfree,
			ubuf.f_tinode,
			ubuf.f_fname,
			ubuf.f_fpack);
	}
}

#ifdef _LP64
static void
show_ustat32(struct ps_prochandle *Pr, long offset)
{
	struct ustat32 ubuf;

	if (offset != NULL &&
	    Pread(Pr, &ubuf, sizeof (ubuf), offset) == sizeof (ubuf)) {
		(void) printf(
		"%s\ttfree=%-6d tinode=%-5u fname=%-6.6s fpack=%-.6s\n",
			pname,
			ubuf.f_tfree,
			ubuf.f_tinode,
			ubuf.f_fname,
			ubuf.f_fpack);
	}
}
#endif	/* _LP64 */

static void
show_fusers(struct ps_prochandle *Pr, long offset, long nproc)
{
	f_user_t fubuf;
	int serial = (nproc > 4);

	if (offset == NULL)
		return;

	/* enter region of lengthy output */
	if (serial)
		Eserialize();

	while (nproc > 0 &&
	    Pread(Pr, &fubuf, sizeof (fubuf), offset) == sizeof (fubuf)) {
		(void) printf("%s\tpid=%-5d uid=%-5d flags=%s\n",
		    pname,
		    (int)fubuf.fu_pid,
		    (int)fubuf.fu_uid,
		    fuflags(fubuf.fu_flags));
		nproc--;
		offset += sizeof (fubuf);
	}

	/* exit region of lengthy output */
	if (serial)
		Xserialize();
}

static void
show_cladm(struct ps_prochandle *Pr, int code, int function, long offset)
{
	int	arg;

	switch (code) {
	case CL_INITIALIZE:
		switch (function) {
		case CL_GET_BOOTFLAG:
			if (Pread(Pr, &arg, sizeof (arg), offset)
			    == sizeof (arg)) {
				if (arg & CLUSTER_CONFIGURED)
					(void) printf("%s\tbootflags="
					    "CLUSTER_CONFIGURED", pname);
				if (arg & CLUSTER_BOOTED)
					(void) printf("|CLUSTER_BOOTED\n");
			}
			break;
		}
		break;
	case CL_CONFIG:
		switch (function) {
		case CL_NODEID:
		case CL_HIGHEST_NODEID:
			if (Pread(Pr, &arg, sizeof (arg), offset)
			    == sizeof (arg)) {
				(void) printf("%s\tnodeid=%d\n", pname, arg);
			}
		}
		break;
	}
}

#define	ALL_LOCK_TYPES	\
	(USYNC_PROCESS|LOCK_ERRORCHECK|LOCK_RECURSIVE|USYNC_PROCESS_ROBUST|\
	    LOCK_PRIO_INHERIT|LOCK_PRIO_PROTECT|LOCK_ROBUST_NP)

/* return cv and mutex types */
static const char *
synch_type(uint_t type)
{
	char *str = code_buf;

	if (type & USYNC_PROCESS)
		(void) strcpy(str, "USYNC_PROCESS");
	else
		(void) strcpy(str, "USYNC_THREAD");

	if (type & LOCK_ERRORCHECK)
		(void) strcat(str, "|LOCK_ERRORCHECK");
	if (type & LOCK_RECURSIVE)
		(void) strcat(str, "|LOCK_RECURSIVE");
	if (type & USYNC_PROCESS_ROBUST)
		(void) strcat(str, "|USYNC_PROCESS_ROBUST");
	if (type & LOCK_PRIO_INHERIT)
		(void) strcat(str, "|LOCK_PRIO_INHERIT");
	if (type & LOCK_PRIO_PROTECT)
		(void) strcat(str, "|LOCK_PRIO_PROTECT");
	if (type & LOCK_ROBUST_NP)
		(void) strcat(str, "|LOCK_ROBUST_NP");

	if ((type &= ~ALL_LOCK_TYPES) != 0)
		(void) sprintf(str + strlen(str), "|0x%.4X", type);

	return ((const char *)str);
}

static void
show_mutex(struct ps_prochandle *Pr, long offset)
{
	lwp_mutex_t mutex;

	if (Pread(Pr, &mutex, sizeof (mutex), offset) == sizeof (mutex)) {
		(void) printf("%s\tmutex type: %s\n",
			pname,
			synch_type(mutex.mutex_type));
	}
}

static void
show_condvar(struct ps_prochandle *Pr, long offset)
{
	lwp_cond_t condvar;

	if (Pread(Pr, &condvar, sizeof (condvar), offset) == sizeof (condvar)) {
		(void) printf("%s\tcondvar type: %s\n",
			pname,
			synch_type(condvar.cond_type));
	}
}

static void
show_sema(struct ps_prochandle *Pr, long offset)
{
	lwp_sema_t sema;

	if (Pread(Pr, &sema, sizeof (sema), offset) == sizeof (sema)) {
		(void) printf("%s\tsema type: %s  count = %u\n",
			pname,
			synch_type(sema.sema_type),
			sema.sema_count);
	}
}

static void
show_ioctl(struct ps_prochandle *Pr, int code, long offset)
{
	int lp64 = (data_model == PR_MODEL_LP64);
	int err = Errno;	/* don't display output parameters */
				/* for a failed system call */
#ifndef _LP64
	if (lp64)
		return;
#endif
	if (offset == NULL)
		return;

	switch (code) {
	case TCGETA:
		if (err)
			break;
		/*FALLTHROUGH*/
	case TCSETA:
	case TCSETAW:
	case TCSETAF:
		show_termio(Pr, offset);
		break;
	case TCGETS:
		if (err)
			break;
		/*FALLTHROUGH*/
	case TCSETS:
	case TCSETSW:
	case TCSETSF:
		show_termios(Pr, offset);
		break;
	case TCGETX:
		if (err)
			break;
		/*FALLTHROUGH*/
	case TCSETX:
	case TCSETXW:
	case TCSETXF:
		show_termiox(Pr, offset);
		break;
	case TIOCGETP:
		if (err)
			break;
		/*FALLTHROUGH*/
	case TIOCSETN:
	case TIOCSETP:
		show_sgttyb(Pr, offset);
		break;
	case TIOCGLTC:
		if (err)
			break;
		/*FALLTHROUGH*/
	case TIOCSLTC:
		show_ltchars(Pr, offset);
		break;
	case TIOCGETC:
		if (err)
			break;
		/*FALLTHROUGH*/
	case TIOCSETC:
		show_tchars(Pr, offset);
		break;
	case LDGETT:
		if (err)
			break;
		/*FALLTHROUGH*/
	case LDSETT:
		show_termcb(Pr, offset);
		break;
	/* streams ioctl()s */
#if 0
		/* these are displayed as strings in the arg list */
		/* by prt_ioa().  don't display them again here */
	case I_PUSH:
	case I_LOOK:
	case I_FIND:
		/* these are displayed as decimal in the arg list */
		/* by prt_ioa().  don't display them again here */
	case I_LINK:
	case I_UNLINK:
	case I_SENDFD:
		/* these are displayed symbolically in the arg list */
		/* by prt_ioa().  don't display them again here */
	case I_SRDOPT:
	case I_SETSIG:
	case I_FLUSH:
		break;
		/* this one just ignores the argument */
	case I_POP:
		break;
#endif
		/* these return something in an int pointed to by arg */
	case I_NREAD:
	case I_GRDOPT:
	case I_GETSIG:
	case TIOCGSID:
	case TIOCGPGRP:
	case TIOCLGET:
	case FIONREAD:
	case FIORDCHK:
		if (err)
			break;
		/*FALLTHROUGH*/
		/* these pass something in an int pointed to by arg */
	case TIOCSPGRP:
	case TIOCFLUSH:
	case TIOCLBIS:
	case TIOCLBIC:
	case TIOCLSET:
		show_strint(Pr, code, offset);
		break;
		/* these all point to structures */
	case I_STR:
#ifdef _LP64
		if (lp64)
			show_strioctl(Pr, offset);
		else
			show_strioctl32(Pr, offset);
#else
		show_strioctl(Pr, offset);
#endif
		break;
	case I_PEEK:
#ifdef _LP64
		if (lp64)
			show_strpeek(Pr, offset);
		else
			show_strpeek32(Pr, offset);
#else
		show_strpeek(Pr, offset);
#endif
		break;
	case I_FDINSERT:
#ifdef _LP64
		if (lp64)
			show_strfdinsert(Pr, offset);
		else
			show_strfdinsert32(Pr, offset);
#else
		show_strfdinsert(Pr, offset);
#endif
		break;
	case I_RECVFD:
		if (err)
			break;
		show_strrecvfd(Pr, offset);
		break;
	case I_LIST:
		if (err)
			break;
#ifdef _LP64
		if (lp64)
			show_strlist(Pr, offset);
		else
			show_strlist32(Pr, offset);
#else
		show_strlist(Pr, offset);
#endif
		break;
	case JWINSIZE:
		if (err)
			break;
		show_jwinsize(Pr, offset);
		break;
	case TIOCGWINSZ:
		if (err)
			break;
		/*FALLTHROUGH*/
	case TIOCSWINSZ:
		show_winsize(Pr, offset);
		break;
	case AUDIO_GETINFO:
	case (int)AUDIO_SETINFO:
		show_audio_info(Pr, offset);
		break;
	}
}

static void
show_termio(struct ps_prochandle *Pr, long offset)
{
	struct termio termio;
	char cbuf[8];
	int i;

	if (Pread(Pr, &termio, sizeof (termio), offset) == sizeof (termio)) {
		(void) printf(
		"%s\tiflag=0%.6o oflag=0%.6o cflag=0%.6o lflag=0%.6o line=%d\n",
			pname,
			termio.c_iflag,
			termio.c_oflag,
			termio.c_cflag,
			termio.c_lflag,
			termio.c_line);
		(void) printf("%s\t    cc: ", pname);
		for (i = 0; i < NCC; i++)
			(void) printf(" %s",
				show_char(cbuf, (int)termio.c_cc[i]));
		(void) fputc('\n', stdout);
	}
}

static void
show_termios(struct ps_prochandle *Pr, long offset)
{
	struct termios termios;
	char cbuf[8];
	int i;

	if (Pread(Pr, &termios, sizeof (termios), offset) == sizeof (termios)) {
		(void) printf(
		"%s\tiflag=0%.6o oflag=0%.6o cflag=0%.6o lflag=0%.6o\n",
			pname,
			termios.c_iflag,
			termios.c_oflag,
			termios.c_cflag,
			termios.c_lflag);
		(void) printf("%s\t    cc: ", pname);
		for (i = 0; i < NCCS; i++) {
			if (i == NCC)	/* show new chars on new line */
				(void) printf("\n%s\t\t", pname);
			(void) printf(" %s",
				show_char(cbuf, (int)termios.c_cc[i]));
		}
		(void) fputc('\n', stdout);
	}
}

static void
show_termiox(struct ps_prochandle *Pr, long offset)
{
	struct termiox termiox;
	int i;

	if (Pread(Pr, &termiox, sizeof (termiox), offset) == sizeof (termiox)) {
		(void) printf("%s\thflag=0%.3o cflag=0%.3o rflag=0%.3o",
			pname,
			termiox.x_hflag,
			termiox.x_cflag,
			termiox.x_rflag[0]);
		for (i = 1; i < NFF; i++)
			(void) printf(",0%.3o", termiox.x_rflag[i]);
		(void) printf(" sflag=0%.3o\n",
			termiox.x_sflag);
	}
}

static void
show_sgttyb(struct ps_prochandle *Pr, long offset)
{
	struct sgttyb sgttyb;

	if (Pread(Pr, &sgttyb, sizeof (sgttyb), offset) == sizeof (sgttyb)) {
		char erase[8];
		char kill[8];

		(void) printf(
		"%s\tispeed=%-2d ospeed=%-2d erase=%s kill=%s flags=0x%.8x\n",
			pname,
			sgttyb.sg_ispeed&0xff,
			sgttyb.sg_ospeed&0xff,
			show_char(erase, sgttyb.sg_erase),
			show_char(kill, sgttyb.sg_kill),
			sgttyb.sg_flags);
	}
}

static void
show_ltchars(struct ps_prochandle *Pr, long offset)
{
	struct ltchars ltchars;
	char *p;
	char cbuf[8];
	int i;

	if (Pread(Pr, &ltchars, sizeof (ltchars), offset) == sizeof (ltchars)) {
		(void) printf("%s\t    cc: ", pname);
		for (p = (char *)&ltchars, i = 0; i < sizeof (ltchars); i++)
			(void) printf(" %s", show_char(cbuf, (int)*p++));
		(void) fputc('\n', stdout);
	}
}

static void
show_tchars(struct ps_prochandle *Pr, long offset)
{
	struct tchars tchars;
	char *p;
	char cbuf[8];
	int i;

	if (Pread(Pr, &tchars, sizeof (tchars), offset) == sizeof (tchars)) {
		(void) printf("%s\t    cc: ", pname);
		for (p = (char *)&tchars, i = 0; i < sizeof (tchars); i++)
			(void) printf(" %s", show_char(cbuf, (int)*p++));
		(void) fputc('\n', stdout);
	}
}

/* represent character as itself ('c') or octal (012) */
static char *
show_char(char *buf, int c)
{
	const char *fmt;

	if (c >= ' ' && c < 0177)
		fmt = "'%c'";
	else
		fmt = "%.3o";

	(void) sprintf(buf, fmt, c&0xff);
	return (buf);
}

static void
show_termcb(struct ps_prochandle *Pr, long offset)
{
	struct termcb termcb;

	if (Pread(Pr, &termcb, sizeof (termcb), offset) == sizeof (termcb)) {
		(void) printf(
		"%s\tflgs=0%.2o termt=%d crow=%d ccol=%d vrow=%d lrow=%d\n",
			pname,
			termcb.st_flgs&0xff,
			termcb.st_termt&0xff,
			termcb.st_crow&0xff,
			termcb.st_ccol&0xff,
			termcb.st_vrow&0xff,
			termcb.st_lrow&0xff);
	}
}

/* integer value pointed to by ioctl() arg */
static void
show_strint(struct ps_prochandle *Pr, int code, long offset)
{
	int val;

	if (Pread(Pr, &val, sizeof (val), offset) == sizeof (val)) {
		const char *s = NULL;

		switch (code) {		/* interpret these symbolically */
		case I_GRDOPT:
			s = strrdopt(val);
			break;
		case I_GETSIG:
			s = strevents(val);
			break;
		case TIOCFLUSH:
			s = tiocflush(val);
			break;
		}

		if (s == NULL)
			(void) printf("%s\t0x%.8lX: %d\n", pname, offset, val);
		else
			(void) printf("%s\t0x%.8lX: %s\n", pname, offset, s);
	}
}

static void
show_strioctl(struct ps_prochandle *Pr, long offset)
{
	struct strioctl strioctl;

	if (Pread(Pr, &strioctl, sizeof (strioctl), offset) ==
	    sizeof (strioctl)) {
		(void) printf(
			"%s\tcmd=%s timout=%d len=%d dp=0x%.8lX\n",
			pname,
			ioctlname(strioctl.ic_cmd),
			strioctl.ic_timout,
			strioctl.ic_len,
			(long)strioctl.ic_dp);

		if (!recur++)	/* avoid indefinite recursion */
			show_ioctl(Pr, strioctl.ic_cmd, (long)strioctl.ic_dp);
		--recur;
	}
}

#ifdef _LP64
static void
show_strioctl32(struct ps_prochandle *Pr, long offset)
{
	struct strioctl32 strioctl;

	if (Pread(Pr, &strioctl, sizeof (strioctl), offset) ==
	    sizeof (strioctl)) {
		(void) printf(
			"%s\tcmd=%s timout=%d len=%d dp=0x%.8lX\n",
			pname,
			ioctlname(strioctl.ic_cmd),
			strioctl.ic_timout,
			strioctl.ic_len,
			(long)strioctl.ic_dp);

		if (!recur++)	/* avoid indefinite recursion */
			show_ioctl(Pr, strioctl.ic_cmd, (long)strioctl.ic_dp);
		--recur;
	}
}
#endif	/* _LP64 */

static void
show_strpeek(struct ps_prochandle *Pr, long offset)
{
	struct strpeek strpeek;

	if (Pread(Pr, &strpeek, sizeof (strpeek), offset) == sizeof (strpeek)) {

		print_strbuf(Pr, &strpeek.ctlbuf, "ctl", FALSE);
		print_strbuf(Pr, &strpeek.databuf, "dat", FALSE);

		(void) printf("%s\tflags=%s\n",
			pname,
			strflags(strpeek.flags));
	}
}

#ifdef _LP64
static void
show_strpeek32(struct ps_prochandle *Pr, long offset)
{
	struct strpeek32 strpeek;

	if (Pread(Pr, &strpeek, sizeof (strpeek), offset) == sizeof (strpeek)) {

		print_strbuf32(Pr, &strpeek.ctlbuf, "ctl", FALSE);
		print_strbuf32(Pr, &strpeek.databuf, "dat", FALSE);

		(void) printf("%s\tflags=%s\n",
			pname,
			strflags(strpeek.flags));
	}
}
#endif	/* _LP64 */

static void
show_strfdinsert(struct ps_prochandle *Pr, long offset)
{
	struct strfdinsert strfdinsert;

	if (Pread(Pr, &strfdinsert, sizeof (strfdinsert), offset) ==
	    sizeof (strfdinsert)) {

		print_strbuf(Pr, &strfdinsert.ctlbuf, "ctl", FALSE);
		print_strbuf(Pr, &strfdinsert.databuf, "dat", FALSE);

		(void) printf("%s\tflags=%s fildes=%d offset=%d\n",
			pname,
			strflags(strfdinsert.flags),
			strfdinsert.fildes,
			strfdinsert.offset);
	}
}

#ifdef _LP64
static void
show_strfdinsert32(struct ps_prochandle *Pr, long offset)
{
	struct strfdinsert32 strfdinsert;

	if (Pread(Pr, &strfdinsert, sizeof (strfdinsert), offset) ==
	    sizeof (strfdinsert)) {

		print_strbuf32(Pr, &strfdinsert.ctlbuf, "ctl", FALSE);
		print_strbuf32(Pr, &strfdinsert.databuf, "dat", FALSE);

		(void) printf("%s\tflags=%s fildes=%d offset=%d\n",
			pname,
			strflags(strfdinsert.flags),
			strfdinsert.fildes,
			strfdinsert.offset);
	}
}
#endif	/* _LP64 */

static const char *
strflags(int flags)		/* strpeek and strfdinsert flags word */
{
	const char *s;

	switch (flags) {
	case 0:
		s = "0";
		break;
	case RS_HIPRI:
		s = "RS_HIPRI";
		break;
	default:
		(void) sprintf(code_buf, "0x%.4X", flags);
		s = code_buf;
	}

	return (s);
}

static void
show_strrecvfd(struct ps_prochandle *Pr, long offset)
{
	struct strrecvfd strrecvfd;

	if (Pread(Pr, &strrecvfd, sizeof (strrecvfd), offset) ==
	    sizeof (strrecvfd)) {
		(void) printf(
			"%s\tfd=%-5d uid=%-5d gid=%d\n",
			pname,
			strrecvfd.fd,
			(int)strrecvfd.uid,
			(int)strrecvfd.gid);
	}
}

static void
show_strlist(struct ps_prochandle *Pr, long offset)
{
	struct str_list strlist;
	struct str_mlist list;
	int count;

	if (Pread(Pr, &strlist, sizeof (strlist), offset) ==
	    sizeof (strlist)) {
		(void) printf("%s\tnmods=%d  modlist=0x%.8lX\n",
			pname,
			strlist.sl_nmods,
			(long)strlist.sl_modlist);

		count = strlist.sl_nmods;
		offset = (long)strlist.sl_modlist;
		while (!interrupt && --count >= 0) {
			if (Pread(Pr, &list, sizeof (list), offset) !=
			    sizeof (list))
				break;
			(void) printf("%s\t\t\"%.*s\"\n",
				pname,
				sizeof (list.l_name),
				list.l_name);
			offset += sizeof (struct str_mlist);
		}
	}
}

#ifdef _LP64
static void
show_strlist32(struct ps_prochandle *Pr, long offset)
{
	struct str_list32 strlist;
	struct str_mlist list;
	int count;

	if (Pread(Pr, &strlist, sizeof (strlist), offset) ==
	    sizeof (strlist)) {
		(void) printf("%s\tnmods=%d  modlist=0x%.8lX\n",
			pname,
			strlist.sl_nmods,
			(long)strlist.sl_modlist);

		count = strlist.sl_nmods;
		offset = (long)strlist.sl_modlist;
		while (!interrupt && --count >= 0) {
			if (Pread(Pr, &list, sizeof (list), offset) !=
			    sizeof (list))
				break;
			(void) printf("%s\t\t\"%.*s\"\n",
				pname,
				sizeof (list.l_name),
				list.l_name);
			offset += sizeof (struct str_mlist);
		}
	}
}
#endif	/* _LP64 */

static void
show_jwinsize(struct ps_prochandle *Pr, long offset)
{
	struct jwinsize jwinsize;

	if (Pread(Pr, &jwinsize, sizeof (jwinsize), offset) ==
	    sizeof (jwinsize)) {
		(void) printf(
			"%s\tbytesx=%-3u bytesy=%-3u bitsx=%-3u bitsy=%-3u\n",
			pname,
			(unsigned)jwinsize.bytesx,
			(unsigned)jwinsize.bytesy,
			(unsigned)jwinsize.bitsx,
			(unsigned)jwinsize.bitsy);
	}
}

static void
show_winsize(struct ps_prochandle *Pr, long offset)
{
	struct winsize winsize;

	if (Pread(Pr, &winsize, sizeof (winsize), offset) == sizeof (winsize)) {
		(void) printf(
			"%s\trow=%-3d col=%-3d xpixel=%-3d ypixel=%-3d\n",
			pname,
			winsize.ws_row,
			winsize.ws_col,
			winsize.ws_xpixel,
			winsize.ws_ypixel);
	}
}

static void
show_audio_info(struct ps_prochandle *Pr, long offset)
{
	struct audio_info au;

	if (Pread(Pr, &au, sizeof (au), offset) == sizeof (au)) {
		show_audio_prinfo("play", &au.play);
		show_audio_prinfo("record", &au.record);
		(void) printf("%s\tmonitor_gain=%u output_muted=%u\n",
			pname, au.monitor_gain, au.output_muted);
	}
}

static void
show_audio_prinfo(const char *mode, struct audio_prinfo *au_pr)
{
	const char *s;

	/*
	 * The following values describe the audio data encoding.
	 */

	(void) printf("%s\t%s\tsample_rate=%u channels=%u precision=%u\n",
		pname, mode,
		au_pr->sample_rate,
		au_pr->channels,
		au_pr->precision);

	s = NULL;
	switch (au_pr->encoding) {
	case AUDIO_ENCODING_NONE:	s = "NONE";	break;
	case AUDIO_ENCODING_ULAW:	s = "ULAW";	break;
	case AUDIO_ENCODING_ALAW:	s = "ALAW";	break;
	case AUDIO_ENCODING_LINEAR:	s = "LINEAR";	break;
	case AUDIO_ENCODING_DVI:	s = "DVI";	break;
	case AUDIO_ENCODING_LINEAR8:	s = "LINEAR8";	break;
	}
	if (s)
		(void) printf("%s\t%s\tencoding=%s\n", pname, mode, s);
	else {
		(void) printf("%s\t%s\tencoding=%u\n",
			pname, mode, au_pr->encoding);
	}

	/*
	 * The following values control audio device configuration
	 */

	(void) printf(
	"%s\t%s\tgain=%u buffer_size=%u\n",
		pname, mode,
		au_pr->gain,
		au_pr->buffer_size);
	show_audio_ports(mode, "port", au_pr->port);
	show_audio_ports(mode, "avail_ports", au_pr->avail_ports);

	/*
	 * The following values describe driver state
	 */

	(void) printf("%s\t%s\tsamples=%u eof=%u pause=%u error=%u\n",
		pname, mode,
		au_pr->samples,
		au_pr->eof,
		au_pr->pause,
		au_pr->error);
	(void) printf("%s\t%s\twaiting=%u balance=%u minordev=%u\n",
		pname, mode,
		au_pr->waiting,
		au_pr->balance,
		au_pr->minordev);

	/*
	 * The following values are read-only state flags
	 */
	(void) printf("%s\t%s\topen=%u active=%u\n",
		pname, mode,
		au_pr->open,
		au_pr->active);
}

struct audio_stuff {
	uint_t	bit;
	const char *str;
};

static const struct audio_stuff audio_output_ports[] = {
	{ AUDIO_SPEAKER, "SPEAKER" },
	{ AUDIO_HEADPHONE, "HEADPHONE" },
	{ AUDIO_LINE_OUT, "LINE_OUT" },
	{ 0, NULL }
};

static const struct audio_stuff audio_input_ports[] = {
	{ AUDIO_MICROPHONE, "MICROPHONE" },
	{ AUDIO_LINE_IN, "LINE_IN" },
	{ AUDIO_CD, "CD" },
	{ 0, NULL }
};

static void
show_audio_ports(const char *mode, const char *field, uint_t ports)
{
	const struct audio_stuff *audio_porttab;

	(void) printf("%s\t%s\t%s=", pname, mode, field);
	if (ports == 0) {
		(void) printf("0\n");
		return;
	}
	if (*mode == 'p')
		audio_porttab = audio_output_ports;
	else
		audio_porttab = audio_input_ports;
	for (; audio_porttab->bit != 0; ++audio_porttab) {
		if (ports & audio_porttab->bit) {
			(void) printf(audio_porttab->str);
			ports &= ~audio_porttab->bit;
			if (ports)
				(void) putchar('|');
		}
	}
	if (ports)
		(void) printf("0x%x", ports);
	(void) putchar('\n');
}

static void
show_statvfs(struct ps_prochandle *Pr)
{
	long offset;
	struct statvfs statvfs;
	char *cp;

	if (sys_nargs > 1 && (offset = sys_args[1]) != NULL &&
	    Pread(Pr, &statvfs, sizeof (statvfs), offset) == sizeof (statvfs)) {
		(void) printf(
		"%s\tbsize=%-10lu frsize=%-9lu blocks=%-8llu bfree=%-9llu\n",
			pname,
			statvfs.f_bsize,
			statvfs.f_frsize,
			(u_longlong_t)statvfs.f_blocks,
			(u_longlong_t)statvfs.f_bfree);
		(void) printf(
		"%s\tbavail=%-9llu files=%-10llu ffree=%-9llu favail=%-9llu\n",
			pname,
			(u_longlong_t)statvfs.f_bavail,
			(u_longlong_t)statvfs.f_files,
			(u_longlong_t)statvfs.f_ffree,
			(u_longlong_t)statvfs.f_favail);
		(void) printf(
		"%s\tfsid=0x%-9.4lX basetype=%-7.16s namemax=%ld\n",
			pname,
			statvfs.f_fsid,
			statvfs.f_basetype,
			(long)statvfs.f_namemax);
		(void) printf(
		"%s\tflag=%s\n",
			pname,
			svfsflags((ulong_t)statvfs.f_flag));
		cp = statvfs.f_fstr + strlen(statvfs.f_fstr);
		if (cp < statvfs.f_fstr + sizeof (statvfs.f_fstr) - 1 &&
		    *(cp+1) != '\0')
			*cp = ' ';
		(void) printf("%s\tfstr=\"%.*s\"\n",
			pname,
			(int)sizeof (statvfs.f_fstr),
			statvfs.f_fstr);
	}
}

#ifdef _LP64
static void
show_statvfs32(struct ps_prochandle *Pr)
{
	long offset;
	struct statvfs32 statvfs;
	char *cp;

	if (sys_nargs > 1 && (offset = sys_args[1]) != NULL &&
	    Pread(Pr, &statvfs, sizeof (statvfs), offset) == sizeof (statvfs)) {
		(void) printf(
		"%s\tbsize=%-10u frsize=%-9u blocks=%-8u bfree=%-9u\n",
			pname,
			statvfs.f_bsize,
			statvfs.f_frsize,
			statvfs.f_blocks,
			statvfs.f_bfree);
		(void) printf(
		"%s\tbavail=%-9u files=%-10u ffree=%-9u favail=%-9u\n",
			pname,
			statvfs.f_bavail,
			statvfs.f_files,
			statvfs.f_ffree,
			statvfs.f_favail);
		(void) printf(
		"%s\tfsid=0x%-9.4X basetype=%-7.16s namemax=%d\n",
			pname,
			statvfs.f_fsid,
			statvfs.f_basetype,
			(int)statvfs.f_namemax);
		(void) printf(
		"%s\tflag=%s\n",
			pname,
			svfsflags((ulong_t)statvfs.f_flag));
		cp = statvfs.f_fstr + strlen(statvfs.f_fstr);
		if (cp < statvfs.f_fstr + sizeof (statvfs.f_fstr) - 1 &&
		    *(cp+1) != '\0')
			*cp = ' ';
		(void) printf("%s\tfstr=\"%.*s\"\n",
			pname,
			sizeof (statvfs.f_fstr),
			statvfs.f_fstr);
	}
}
#endif	/* _LP64 */

static void
show_statvfs64(struct ps_prochandle *Pr)
{
	long offset;
	struct statvfs64_32 statvfs;
	char *cp;

	if (sys_nargs > 1 && (offset = sys_args[1]) != NULL &&
	    Pread(Pr, &statvfs, sizeof (statvfs), offset) == sizeof (statvfs)) {
		(void) printf(
		"%s\tbsize=%-10u frsize=%-9u blocks=%-8llu bfree=%-9llu\n",
			pname,
			statvfs.f_bsize,
			statvfs.f_frsize,
			(u_longlong_t)statvfs.f_blocks,
			(u_longlong_t)statvfs.f_bfree);
		(void) printf(
		"%s\tbavail=%-9llu files=%-10llu ffree=%-9llu favail=%-9llu\n",
			pname,
			(u_longlong_t)statvfs.f_bavail,
			(u_longlong_t)statvfs.f_files,
			(u_longlong_t)statvfs.f_ffree,
			(u_longlong_t)statvfs.f_favail);
		(void) printf(
		"%s\tfsid=0x%-9.4X basetype=%-7.16s namemax=%d\n",
			pname,
			statvfs.f_fsid,
			statvfs.f_basetype,
			(int)statvfs.f_namemax);
		(void) printf(
		"%s\tflag=%s\n",
			pname,
			svfsflags((ulong_t)statvfs.f_flag));
		cp = statvfs.f_fstr + strlen(statvfs.f_fstr);
		if (cp < statvfs.f_fstr + sizeof (statvfs.f_fstr) - 1 &&
		    *(cp+1) != '\0')
			*cp = ' ';
		(void) printf("%s\tfstr=\"%.*s\"\n",
			pname,
			sizeof (statvfs.f_fstr),
			statvfs.f_fstr);
	}
}

static void
show_statfs(struct ps_prochandle *Pr)
{
	long offset;
	struct statfs statfs;

	if (sys_nargs >= 2 && (offset = sys_args[1]) != NULL &&
	    Pread(Pr, &statfs, sizeof (statfs), offset) == sizeof (statfs)) {
		(void) printf(
		"%s\tfty=%d bsz=%ld fsz=%ld blk=%ld bfr=%ld fil=%lu ffr=%lu\n",
			pname,
			statfs.f_fstyp,
			statfs.f_bsize,
			statfs.f_frsize,
			statfs.f_blocks,
			statfs.f_bfree,
			statfs.f_files,
			statfs.f_ffree);
		(void) printf("%s\t    fname=%.6s fpack=%.6s\n",
			pname,
			statfs.f_fname,
			statfs.f_fpack);
	}
}

#ifdef _LP64
static void
show_statfs32(struct ps_prochandle *Pr)
{
	long offset;
	struct statfs32 statfs;

	if (sys_nargs >= 2 && (offset = sys_args[1]) != NULL &&
	    Pread(Pr, &statfs, sizeof (statfs), offset) == sizeof (statfs)) {
		(void) printf(
		"%s\tfty=%d bsz=%d fsz=%d blk=%d bfr=%d fil=%u ffr=%u\n",
			pname,
			statfs.f_fstyp,
			statfs.f_bsize,
			statfs.f_frsize,
			statfs.f_blocks,
			statfs.f_bfree,
			statfs.f_files,
			statfs.f_ffree);
		(void) printf("%s\t    fname=%.6s fpack=%.6s\n",
			pname,
			statfs.f_fname,
			statfs.f_fpack);
	}
}
#endif	/* _LP64 */

/* print values in fcntl() pointed-to structure */
static void
show_fcntl(struct ps_prochandle *Pr)
{
	long offset;

	if (sys_nargs < 3 || (offset = sys_args[2]) == NULL)
		return;

	switch (sys_args[1]) {
#ifdef _LP64
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
	case F_FREESP:
	case F_ALLOCSP:
		if (data_model == PR_MODEL_LP64)
			show_flock64(Pr, offset);
		else
			show_flock32(Pr, offset);
		break;
	case 33:	/* F_GETLK64 */
	case 34:	/* F_SETLK64 */
	case 35:	/* F_SETLKW64 */
	case 27:	/* F_FREESP64 */
		show_flock64(Pr, offset);
		break;
#else	/* _LP64 */
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
	case F_FREESP:
	case F_ALLOCSP:
		show_flock32(Pr, offset);
		break;
	case F_GETLK64:
	case F_SETLK64:
	case F_SETLKW64:
	case F_FREESP64:
		show_flock64(Pr, offset);
		break;
#endif	/* _LP64 */
	case F_SHARE:
	case F_UNSHARE:
		show_share(Pr, offset);
		break;
	}
}

static void
show_flock32(struct ps_prochandle *Pr, long offset)
{
	struct flock32 flock;

	if (Pread(Pr, &flock, sizeof (flock), offset) == sizeof (flock)) {
		const char *str = NULL;

		(void) printf("%s\ttyp=", pname);

		switch (flock.l_type) {
		case F_RDLCK:
			str = "F_RDLCK";
			break;
		case F_WRLCK:
			str = "F_WRLCK";
			break;
		case F_UNLCK:
			str = "F_UNLCK";
			break;
		}
		if (str != NULL)
			(void) printf("%s", str);
		else
			(void) printf("%-7d", flock.l_type);

		str = whencearg(flock.l_whence);
		if (str != NULL)
			(void) printf("  whence=%s", str);
		else
			(void) printf("  whence=%-8u", flock.l_whence);

		(void) printf(
			" start=%-5d len=%-5d sys=%-2u pid=%d\n",
			flock.l_start,
			flock.l_len,
			flock.l_sysid,
			flock.l_pid);
	}
}

static void
show_flock64(struct ps_prochandle *Pr, long offset)
{
	struct flock64 flock;

	if (Pread(Pr, &flock, sizeof (flock), offset) == sizeof (flock)) {
		const char *str = NULL;

		(void) printf("%s\ttyp=", pname);

		switch (flock.l_type) {
		case F_RDLCK:
			str = "F_RDLCK";
			break;
		case F_WRLCK:
			str = "F_WRLCK";
			break;
		case F_UNLCK:
			str = "F_UNLCK";
			break;
		}
		if (str != NULL)
			(void) printf("%s", str);
		else
			(void) printf("%-7d", flock.l_type);

		str = whencearg(flock.l_whence);
		if (str != NULL)
			(void) printf("  whence=%s", str);
		else
			(void) printf("  whence=%-8u", flock.l_whence);

		(void) printf(
			" start=%-5lld len=%-5lld sys=%-2u pid=%d\n",
			(long long)flock.l_start,
			(long long)flock.l_len,
			flock.l_sysid,
			(int)flock.l_pid);
	}
}

static void
show_share(struct ps_prochandle *Pr, long offset)
{
	struct fshare fshare;

	if (Pread(Pr, &fshare, sizeof (fshare), offset) == sizeof (fshare)) {
		const char *str = NULL;

		(void) printf("%s\taccess=", pname);

		switch (fshare.f_access) {
		case F_RDACC:
			str = "F_RDACC";
			break;
		case F_WRACC:
			str = "F_WRACC";
			break;
		case F_RWACC:
			str = "F_RWACC";
			break;
		}
		if (str != NULL)
			(void) printf("%s", str);
		else
			(void) printf("%-7d", fshare.f_access);

		str = NULL;
		switch (fshare.f_deny) {
		case F_NODNY:
			str = "F_NODNY";
			break;
		case F_RDDNY:
			str = "F_RDDNY";
			break;
		case F_WRDNY:
			str = "F_WRDNY";
			break;
		case F_RWDNY:
			str = "F_RWDNY";
			break;
		case F_COMPAT:
			str = "F_COMPAT";
			break;
		}
		if (str != NULL)
			(void) printf("  deny=%s", str);
		else
			(void) printf("  deny=%-7d", fshare.f_deny);

		(void) printf("  id=%x\n", fshare.f_id);
	}
}

static void
show_gp_msg(struct ps_prochandle *Pr, int what)
{
	long offset;
	int dump = FALSE;
	int fdp1 = sys_args[0] + 1;

	switch (what) {
	case SYS_getmsg:
	case SYS_getpmsg:
		if (Errno == 0 && prismember(&readfd, fdp1))
			dump = TRUE;
		break;
	case SYS_putmsg:
	case SYS_putpmsg:
		if (prismember(&writefd, fdp1))
			dump = TRUE;
		break;
	}

	/* enter region of lengthy output */
	if (dump)
		Eserialize();

#ifdef _LP64
	if (sys_nargs >= 2 && (offset = sys_args[1]) != NULL) {
		if (data_model == PR_MODEL_LP64)
			show_strbuf(Pr, offset, "ctl", dump);
		else
			show_strbuf32(Pr, offset, "ctl", dump);
	}
	if (sys_nargs >= 3 && (offset = sys_args[2]) != NULL) {
		if (data_model == PR_MODEL_LP64)
			show_strbuf(Pr, offset, "dat", dump);
		else
			show_strbuf32(Pr, offset, "dat", dump);
	}
#else	/* _LP64 */
	if (sys_nargs >= 2 && (offset = sys_args[1]) != NULL)
		show_strbuf(Pr, offset, "ctl", dump);
	if (sys_nargs >= 3 && (offset = sys_args[2]) != NULL)
		show_strbuf(Pr, offset, "dat", dump);
#endif	/* _LP64 */

	/* exit region of lengthy output */
	if (dump)
		Xserialize();
}

static void
show_strbuf(struct ps_prochandle *Pr, long offset, const char *name, int dump)
{
	struct strbuf strbuf;

	if (Pread(Pr, &strbuf, sizeof (strbuf), offset) == sizeof (strbuf))
		print_strbuf(Pr, &strbuf, name, dump);
}

#ifdef _LP64
static void
show_strbuf32(struct ps_prochandle *Pr, long offset, const char *name, int dump)
{
	struct strbuf32 strbuf;

	if (Pread(Pr, &strbuf, sizeof (strbuf), offset) == sizeof (strbuf))
		print_strbuf32(Pr, &strbuf, name, dump);
}
#endif	/* _LP64 */

static void
print_strbuf(struct ps_prochandle *Pr,
	struct strbuf *sp, const char *name, int dump)
{
	(void) printf(
		"%s\t%s:  maxlen=%-4d len=%-4d buf=0x%.8lX",
		pname,
		name,
		sp->maxlen,
		sp->len,
		(long)sp->buf);
	/*
	 * Should we show the buffer contents?
	 * Keyed to the '-r fds' and '-w fds' options?
	 */
	if (sp->buf == NULL || sp->len <= 0)
		(void) fputc('\n', stdout);
	else {
		int nb = (sp->len > 8)? 8 : sp->len;
		char buffer[8];
		char obuf[40];

		if (Pread(Pr, buffer, (size_t)nb, (long)sp->buf) == nb) {
			(void) strcpy(obuf, ": \"");
			showbytes(buffer, nb, obuf+3);
			(void) strcat(obuf,
				(nb == sp->len)?
				    (const char *)"\"" : (const char *)"\"..");
			(void) fputs(obuf, stdout);
		}
		(void) fputc('\n', stdout);
		if (dump && sp->len > 8)
			showbuffer(Pr, (long)sp->buf, (long)sp->len);
	}
}

#ifdef _LP64
static void
print_strbuf32(struct ps_prochandle *Pr,
	struct strbuf32 *sp, const char *name, int dump)
{
	(void) printf(
		"%s\t%s:  maxlen=%-4d len=%-4d buf=0x%.8lX",
		pname,
		name,
		sp->maxlen,
		sp->len,
		(long)sp->buf);
	/*
	 * Should we show the buffer contents?
	 * Keyed to the '-r fds' and '-w fds' options?
	 */
	if (sp->buf == NULL || sp->len <= 0)
		(void) fputc('\n', stdout);
	else {
		int nb = (sp->len > 8)? 8 : sp->len;
		char buffer[8];
		char obuf[40];

		if (Pread(Pr, buffer, (size_t)nb, (long)sp->buf) == nb) {
			(void) strcpy(obuf, ": \"");
			showbytes(buffer, nb, obuf+3);
			(void) strcat(obuf,
				(nb == sp->len)?
				    (const char *)"\"" : (const char *)"\"..");
			(void) fputs(obuf, stdout);
		}
		(void) fputc('\n', stdout);
		if (dump && sp->len > 8)
			showbuffer(Pr, (long)sp->buf, (long)sp->len);
	}
}
#endif	/* _LP64 */

static void
show_int(struct ps_prochandle *Pr, long offset, const char *name)
{
	int value;

	if (offset != 0 &&
	    Pread(Pr, &value, sizeof (value), offset) == sizeof (value))
		(void) printf("%s\t%s:\t%d\n",
			pname,
			name,
			value);
}

static void
show_hhex_int(struct ps_prochandle *Pr, long offset, const char *name)
{
	int value;

	if (Pread(Pr, &value, sizeof (value), offset) == sizeof (value))
		(void) printf("%s\t%s:\t0x%.4X\n",
			pname,
			name,
			value);
}

static void
show_poll(struct ps_prochandle *Pr)
{
	long offset;
	int nfds, skip = 0;

	if (sys_nargs >= 2 &&
	    (offset = sys_args[0]) != NULL &&
	    (nfds = sys_args[1]) > 0) {
		if (nfds > 32) {	/* let's not be ridiculous */
			skip = nfds - 32;
			nfds = 32;
		}
		for (; nfds && !interrupt;
		    nfds--, offset += sizeof (struct pollfd))
			show_pollfd(Pr, offset);
		if (skip && !interrupt)
			(void) printf(
				"%s\t...skipping %d file descriptors...\n",
				pname,
				skip);
	}
}

static void
show_pollfd(struct ps_prochandle *Pr, long offset)
{
	struct pollfd pollfd;

	if (Pread(Pr, &pollfd, sizeof (pollfd), offset) == sizeof (pollfd)) {
		/* can't print both events and revents in same printf */
		/* pollevent() returns a pointer to a static location */
		(void) printf("%s\tfd=%-2d ev=%s",
			pname,
			pollfd.fd,
			pollevent(pollfd.events));
		(void) printf(" rev=%s\n",
			pollevent(pollfd.revents));
	}
}

#define	ALL_POLL_FLAGS	(POLLIN|POLLPRI|POLLOUT| \
	POLLRDNORM|POLLRDBAND|POLLWRBAND|POLLERR|POLLHUP|POLLNVAL)

static const char *
pollevent(int arg)
{
	char *str = code_buf;

	if (arg == 0)
		return ("0");
	if (arg & ~ALL_POLL_FLAGS) {
		(void) sprintf(str, "0x%-5X", arg);
		return ((const char *)str);
	}

	*str = '\0';
	if (arg & POLLIN)
		(void) strcat(str, "|POLLIN");
	if (arg & POLLPRI)
		(void) strcat(str, "|POLLPRI");
	if (arg & POLLOUT)
		(void) strcat(str, "|POLLOUT");
	if (arg & POLLRDNORM)
		(void) strcat(str, "|POLLRDNORM");
	if (arg & POLLRDBAND)
		(void) strcat(str, "|POLLRDBAND");
	if (arg & POLLWRBAND)
		(void) strcat(str, "|POLLWRBAND");
	if (arg & POLLERR)
		(void) strcat(str, "|POLLERR");
	if (arg & POLLHUP)
		(void) strcat(str, "|POLLHUP");
	if (arg & POLLNVAL)
		(void) strcat(str, "|POLLNVAL");

	return ((const char *)(str+1));
}

static void
show_perm(struct ipc_perm *ip)
{
	(void) printf(
	"%s\tu=%-5u g=%-5u cu=%-5u cg=%-5u m=0%.6o seq=%u key=%d\n",
		pname,
		(int)ip->uid,
		(int)ip->gid,
		(int)ip->cuid,
		(int)ip->cgid,
		(int)ip->mode,
		ip->seq,
		ip->key);
}

#ifdef _LP64
static void
show_perm32(struct ipc_perm32 *ip)
{
	(void) printf(
	"%s\tu=%-5u g=%-5u cu=%-5u cg=%-5u m=0%.6o seq=%u key=%d\n",
		pname,
		ip->uid,
		ip->gid,
		ip->cuid,
		ip->cgid,
		ip->mode,
		ip->seq,
		ip->key);
}
#endif	/* _LP64 */

#ifdef _LP64
static void
show_msgsys(struct ps_prochandle *Pr, long msgsz)
{
	switch (sys_args[0]) {
	case 0:			/* msgget() */
		break;
	case 1:			/* msgctl() */
		if (sys_nargs > 3) {
			switch (sys_args[2]) {
			case IPC_STAT:
				if (Errno)
					break;
				/*FALLTHROUGH*/
			case IPC_SET:
				if (data_model == PR_MODEL_LP64)
					show_msgctl(Pr, (long)sys_args[3]);
				else
					show_msgctl32(Pr, (long)sys_args[3]);
				break;
			}
		}
		break;
	case 2:			/* msgrcv() */
		if (!Errno && sys_nargs > 2) {
			if (data_model == PR_MODEL_LP64)
				show_msgbuf(Pr, sys_args[2], msgsz);
			else
				show_msgbuf32(Pr, sys_args[2], msgsz);
		}
		break;
	case 3:			/* msgsnd() */
		if (sys_nargs > 3) {
			if (data_model == PR_MODEL_LP64)
				show_msgbuf(Pr, sys_args[2], sys_args[3]);
			else
				show_msgbuf32(Pr, sys_args[2], sys_args[3]);
		}
		break;
	default:		/* unexpected subcode */
		break;
	}
}
#else	/* _LP64 */
static void
show_msgsys(struct ps_prochandle *Pr, long msgsz)
{
	switch (sys_args[0]) {
	case 0:			/* msgget() */
		break;
	case 1:			/* msgctl() */
		if (sys_nargs > 3) {
			switch (sys_args[2]) {
			case IPC_STAT:
				if (Errno)
					break;
				/*FALLTHROUGH*/
			case IPC_SET:
				show_msgctl(Pr, (long)sys_args[3]);
				break;
			}
		}
		break;
	case 2:			/* msgrcv() */
		if (!Errno && sys_nargs > 2)
			show_msgbuf(Pr, sys_args[2], msgsz);
		break;
	case 3:			/* msgsnd() */
		if (sys_nargs > 3)
			show_msgbuf(Pr, sys_args[2], sys_args[3]);
		break;
	default:		/* unexpected subcode */
		break;
	}
}
#endif	/* _LP64 */

static void
show_msgctl(struct ps_prochandle *Pr, long offset)
{
	struct msqid_ds msgq;

	if (offset != NULL &&
	    Pread(Pr, &msgq, sizeof (msgq), offset) == sizeof (msgq)) {
		show_perm(&msgq.msg_perm);

		(void) printf(
	"%s\tbytes=%-5lu msgs=%-5lu maxby=%-5lu lspid=%-5u lrpid=%-5u\n",
			pname,
			msgq.msg_cbytes,
			msgq.msg_qnum,
			msgq.msg_qbytes,
			(int)msgq.msg_lspid,
			(int)msgq.msg_lrpid);

		prtime("    st = ", msgq.msg_stime);
		prtime("    rt = ", msgq.msg_rtime);
		prtime("    ct = ", msgq.msg_ctime);
	}
}

#ifdef _LP64
static void
show_msgctl32(struct ps_prochandle *Pr, long offset)
{
	struct msqid_ds32 msgq;

	if (offset != NULL &&
	    Pread(Pr, &msgq, sizeof (msgq), offset) == sizeof (msgq)) {
		show_perm32(&msgq.msg_perm);

		(void) printf(
	"%s\tbytes=%-5u msgs=%-5u maxby=%-5u lspid=%-5u lrpid=%-5u\n",
			pname,
			msgq.msg_cbytes,
			msgq.msg_qnum,
			msgq.msg_qbytes,
			msgq.msg_lspid,
			msgq.msg_lrpid);

		prtime("    st = ", msgq.msg_stime);
		prtime("    rt = ", msgq.msg_rtime);
		prtime("    ct = ", msgq.msg_ctime);
	}
}
#endif	/* _LP64 */

static void
show_msgbuf(struct ps_prochandle *Pr, long offset, long msgsz)
{
	struct msgbuf msgb;

	if (offset != NULL &&
	    Pread(Pr, &msgb, sizeof (msgb.mtype), offset) ==
	    sizeof (msgb.mtype)) {
		/* enter region of lengthy output */
		if (msgsz > BUFSIZ/4)
			Eserialize();

		(void) printf("%s\tmtype=%lu  mtext[]=\n",
			pname,
			msgb.mtype);
		showbuffer(Pr, (long)(offset + sizeof (msgb.mtype)), msgsz);

		/* exit region of lengthy output */
		if (msgsz > BUFSIZ/4)
			Xserialize();
	}
}

#ifdef _LP64
static void
show_msgbuf32(struct ps_prochandle *Pr, long offset, long msgsz)
{
	struct ipcmsgbuf32 msgb;

	if (offset != NULL &&
	    Pread(Pr, &msgb, sizeof (msgb.mtype), offset) ==
	    sizeof (msgb.mtype)) {
		/* enter region of lengthy output */
		if (msgsz > BUFSIZ/4)
			Eserialize();

		(void) printf("%s\tmtype=%u  mtext[]=\n",
			pname,
			msgb.mtype);
		showbuffer(Pr, (long)(offset + sizeof (msgb.mtype)), msgsz);

		/* exit region of lengthy output */
		if (msgsz > BUFSIZ/4)
			Xserialize();
	}
}
#endif	/* _LP64 */

static void
show_semsys(struct ps_prochandle *Pr)
{
	switch (sys_args[0]) {
	case 0:			/* semctl() */
		if (sys_nargs > 4) {
			switch (sys_args[3]) {
			case IPC_STAT:
				if (Errno)
					break;
				/*FALLTHROUGH*/
			case IPC_SET:
#ifdef _LP64
				if (data_model == PR_MODEL_LP64)
					show_semctl(Pr, (long)sys_args[4]);
				else
					show_semctl32(Pr, (long)sys_args[4]);
#else
				show_semctl(Pr, (long)sys_args[4]);
#endif
				break;
			}
		}
		break;
	case 1:			/* semget() */
		break;
	case 2:			/* semop() */
		if (sys_nargs > 3)
			show_semop(Pr, (long)sys_args[2], sys_args[3]);
		break;
	default:		/* unexpected subcode */
		break;
	}
}

static void
show_semctl(struct ps_prochandle *Pr, long offset)
{
	struct semid_ds semds;

	if (offset != NULL &&
	    Pread(Pr, &semds, sizeof (semds), offset) == sizeof (semds)) {
		show_perm(&semds.sem_perm);

		(void) printf("%s\tnsems=%u\n",
			pname,
			semds.sem_nsems);

		prtime("    ot = ", semds.sem_otime);
		prtime("    ct = ", semds.sem_ctime);
	}
}

#ifdef _LP64
static void
show_semctl32(struct ps_prochandle *Pr, long offset)
{
	struct semid_ds32 semds;

	if (offset != NULL &&
	    Pread(Pr, &semds, sizeof (semds), offset) == sizeof (semds)) {
		show_perm32(&semds.sem_perm);

		(void) printf("%s\tnsems=%u\n",
			pname,
			semds.sem_nsems);

		prtime("    ot = ", semds.sem_otime);
		prtime("    ct = ", semds.sem_ctime);
	}
}
#endif	/* _LP64 */

static void
show_semop(struct ps_prochandle *Pr, long offset, long nsops)
{
	struct sembuf sembuf;
	const char *str;

	if (offset == NULL)
		return;

	if (nsops > 40)		/* let's not be ridiculous */
		nsops = 40;

	for (; nsops > 0 && !interrupt; --nsops, offset += sizeof (sembuf)) {
		if (Pread(Pr, &sembuf, sizeof (sembuf), offset) !=
		    sizeof (sembuf))
			break;

		(void) printf("%s\tsemnum=%-5u semop=%-5d semflg=",
			pname,
			sembuf.sem_num,
			sembuf.sem_op);

		if (sembuf.sem_flg == 0)
			(void) printf("0\n");
		else if ((str = semflags(sembuf.sem_flg)) != NULL)
			(void) printf("%s\n", str);
		else
			(void) printf("0%.6o\n", sembuf.sem_flg);
	}
}

static void
show_shmsys(struct ps_prochandle *Pr)
{
	switch (sys_args[0]) {
	case 0:			/* shmat() */
		break;
	case 1:			/* shmctl() */
		if (sys_nargs > 3) {
			switch (sys_args[2]) {
			case IPC_STAT:
				if (Errno)
					break;
				/*FALLTHROUGH*/
			case IPC_SET:
#ifdef _LP64
				if (data_model == PR_MODEL_LP64)
					show_shmctl(Pr, (long)sys_args[3]);
				else
					show_shmctl32(Pr, (long)sys_args[3]);
#else
				show_shmctl(Pr, (long)sys_args[3]);
#endif
				break;
			}
		}
		break;
	case 2:			/* shmdt() */
		break;
	case 3:			/* shmget() */
		break;
	default:		/* unexpected subcode */
		break;
	}
}

static void
show_shmctl(struct ps_prochandle *Pr, long offset)
{
	struct shmid_ds shmds;

	if (offset != NULL &&
	    Pread(Pr, &shmds, sizeof (shmds), offset) == sizeof (shmds)) {
		show_perm(&shmds.shm_perm);

		(void) printf(
		"%s\tsize=%-6lu lpid=%-5u cpid=%-5u na=%-5lu cna=%lu\n",
			pname,
			(ulong_t)shmds.shm_segsz,
			(int)shmds.shm_lpid,
			(int)shmds.shm_cpid,
			shmds.shm_nattch,
			shmds.shm_cnattch);

		prtime("    at = ", shmds.shm_atime);
		prtime("    dt = ", shmds.shm_dtime);
		prtime("    ct = ", shmds.shm_ctime);
	}
}

#ifdef _LP64
static void
show_shmctl32(struct ps_prochandle *Pr, long offset)
{
	struct shmid_ds32 shmds;

	if (offset != NULL &&
	    Pread(Pr, &shmds, sizeof (shmds), offset) == sizeof (shmds)) {
		show_perm32(&shmds.shm_perm);

		(void) printf(
		"%s\tsize=%-6u lpid=%-5u cpid=%-5u na=%-5u cna=%u\n",
			pname,
			shmds.shm_segsz,
			shmds.shm_lpid,
			shmds.shm_cpid,
			shmds.shm_nattch,
			shmds.shm_cnattch);

		prtime("    at = ", shmds.shm_atime);
		prtime("    dt = ", shmds.shm_dtime);
		prtime("    ct = ", shmds.shm_ctime);
	}
}
#endif	/* _LP64 */

static void
show_groups(struct ps_prochandle *Pr, long offset, long count)
{
	int groups[100];

	if (count > 100)
		count = 100;

	if (count > 0 && offset != NULL &&
	    Pread(Pr, &groups[0], count*sizeof (int), offset) ==
	    count*sizeof (int)) {
		int n;

		(void) printf("%s\t", pname);
		for (n = 0; !interrupt && n < count; n++) {
			if (n != 0 && n%10 == 0)
				(void) printf("\n%s\t", pname);
			(void) printf(" %5d", groups[n]);
		}
		(void) fputc('\n', stdout);
	}
}

static void
show_sigset(struct ps_prochandle *Pr, long offset, const char *name)
{
	sigset_t sigset;

	if (offset != NULL &&
	    Pread(Pr, &sigset, sizeof (sigset), offset) == sizeof (sigset)) {
		(void) printf("%s\t%s =%s\n",
			pname, name, sigset_string(&sigset));
	}
}

/*
 * This assumes that a sigset_t is simply an array of ints.
 */
static char *
sigset_string(sigset_t *sp)
{
	char *s = code_buf;
	int n = sizeof (*sp) / sizeof (int32_t);
	int32_t *lp = (int32_t *)sp;

	while (--n >= 0) {
		int32_t val = *lp++;

		if (val == 0)
			s += sprintf(s, " 0");
		else
			s += sprintf(s, " 0x%.8X", val);
	}

	return (code_buf);
}

static void
show_sigaltstack(struct ps_prochandle *Pr, long offset, const char *name)
{
	struct sigaltstack altstack;

#ifdef _LP64
	if (data_model != PR_MODEL_LP64) {
		show_sigaltstack32(Pr, offset, name);
		return;
	}
#endif
	if (offset != NULL &&
	    Pread(Pr, &altstack, sizeof (altstack), offset) ==
	    sizeof (altstack)) {
		(void) printf("%s\t%s: sp=0x%.8lX size=%lu flags=0x%.4X\n",
			pname,
			name,
			(ulong_t)altstack.ss_sp,
			(ulong_t)altstack.ss_size,
			altstack.ss_flags);
	}
}

#ifdef _LP64
static void
show_sigaltstack32(struct ps_prochandle *Pr, long offset, const char *name)
{
	struct sigaltstack32 altstack;

	if (offset != NULL &&
	    Pread(Pr, &altstack, sizeof (altstack), offset) ==
	    sizeof (altstack)) {
		(void) printf("%s\t%s: sp=0x%.8X size=%u flags=0x%.4X\n",
			pname,
			name,
			altstack.ss_sp,
			altstack.ss_size,
			altstack.ss_flags);
	}
}
#endif	/* _LP64 */

static void
show_sigaction(struct ps_prochandle *Pr,
	long offset, const char *name, long odisp)
{
	struct sigaction sigaction;

#ifdef _LP64
	if (data_model != PR_MODEL_LP64) {
		show_sigaction32(Pr, offset, name, odisp);
		return;
	}
#endif
	if (offset != NULL &&
	    Pread(Pr, &sigaction, sizeof (sigaction), offset) ==
	    sizeof (sigaction)) {
		/* This is stupid, we shouldn't have to do this */
		if (odisp != NULL)
			sigaction.sa_handler = (void (*)())odisp;
		(void) printf(
			"%s    %s: hand = 0x%.8lX mask =%s flags = 0x%.4X\n",
			pname,
			name,
			(long)sigaction.sa_handler,
			sigset_string(&sigaction.sa_mask),
			sigaction.sa_flags);
	}
}

#ifdef _LP64
static void
show_sigaction32(struct ps_prochandle *Pr,
	long offset, const char *name, long odisp)
{
	struct sigaction32 sigaction;

	if (offset != NULL &&
	    Pread(Pr, &sigaction, sizeof (sigaction), offset) ==
	    sizeof (sigaction)) {
		/* This is stupid, we shouldn't have to do this */
		if (odisp != NULL)
			sigaction.sa_handler = (caddr32_t)odisp;
		(void) printf(
			"%s    %s: hand = 0x%.8X mask =%s flags = 0x%.4X\n",
			pname,
			name,
			sigaction.sa_handler,
			sigset_string((sigset_t *)&sigaction.sa_mask),
			sigaction.sa_flags);
	}
}
#endif	/* _LP64 */

static void
show_siginfo(struct ps_prochandle *Pr, long offset)
{
	struct siginfo siginfo;

#ifdef _LP64
	if (data_model != PR_MODEL_LP64) {
		show_siginfo32(Pr, offset);
		return;
	}
#endif
	if (offset != NULL &&
	    Pread(Pr, &siginfo, sizeof (siginfo), offset) == sizeof (siginfo))
		print_siginfo(&siginfo);
}

#ifdef _LP64
static void
show_siginfo32(struct ps_prochandle *Pr, long offset)
{
	struct siginfo32 siginfo;

	if (offset != NULL &&
	    Pread(Pr, &siginfo, sizeof (siginfo), offset) == sizeof (siginfo))
		print_siginfo32(&siginfo);
}
#endif	/* _LP64 */

void
print_siginfo(const siginfo_t *sip)
{
	const char *code = NULL;

	(void) printf("%s      siginfo: %s", pname, signame(sip->si_signo));

	if (sip->si_signo != 0 && SI_FROMUSER(sip) && sip->si_pid != 0) {
		(void) printf(" pid=%d uid=%d",
		    (int)sip->si_pid,
		    (int)sip->si_uid);
		if (sip->si_code != 0)
			(void) printf(" code=%d", sip->si_code);
		(void) fputc('\n', stdout);
		return;
	}

	switch (sip->si_signo) {
	default:
		(void) fputc('\n', stdout);
		return;
	case SIGILL:
	case SIGTRAP:
	case SIGFPE:
	case SIGSEGV:
	case SIGBUS:
	case SIGEMT:
	case SIGCLD:
	case SIGPOLL:
	case SIGXFSZ:
		break;
	}

	switch (sip->si_signo) {
	case SIGILL:
		switch (sip->si_code) {
		case ILL_ILLOPC:	code = "ILL_ILLOPC";	break;
		case ILL_ILLOPN:	code = "ILL_ILLOPN";	break;
		case ILL_ILLADR:	code = "ILL_ILLADR";	break;
		case ILL_ILLTRP:	code = "ILL_ILLTRP";	break;
		case ILL_PRVOPC:	code = "ILL_PRVOPC";	break;
		case ILL_PRVREG:	code = "ILL_PRVREG";	break;
		case ILL_COPROC:	code = "ILL_COPROC";	break;
		case ILL_BADSTK:	code = "ILL_BADSTK";	break;
		}
		break;
	case SIGTRAP:
		switch (sip->si_code) {
		case TRAP_BRKPT:	code = "TRAP_BRKPT";	break;
		case TRAP_TRACE:	code = "TRAP_TRACE";	break;
		}
		break;
	case SIGFPE:
		switch (sip->si_code) {
		case FPE_INTDIV:	code = "FPE_INTDIV";	break;
		case FPE_INTOVF:	code = "FPE_INTOVF";	break;
		case FPE_FLTDIV:	code = "FPE_FLTDIV";	break;
		case FPE_FLTOVF:	code = "FPE_FLTOVF";	break;
		case FPE_FLTUND:	code = "FPE_FLTUND";	break;
		case FPE_FLTRES:	code = "FPE_FLTRES";	break;
		case FPE_FLTINV:	code = "FPE_FLTINV";	break;
		case FPE_FLTSUB:	code = "FPE_FLTSUB";	break;
		}
		break;
	case SIGSEGV:
		switch (sip->si_code) {
		case SEGV_MAPERR:	code = "SEGV_MAPERR";	break;
		case SEGV_ACCERR:	code = "SEGV_ACCERR";	break;
		}
		break;
	case SIGEMT:
		switch (sip->si_code) {
#ifdef EMT_TAGOVF
		case EMT_TAGOVF:	code = "EMT_TAGOVF";	break;
#endif
		case EMT_CPCOVF:	code = "EMT_CPCOVF";	break;
		}
		break;
	case SIGBUS:
		switch (sip->si_code) {
		case BUS_ADRALN:	code = "BUS_ADRALN";	break;
		case BUS_ADRERR:	code = "BUS_ADRERR";	break;
		case BUS_OBJERR:	code = "BUS_OBJERR";	break;
		}
		break;
	case SIGCLD:
		switch (sip->si_code) {
		case CLD_EXITED:	code = "CLD_EXITED";	break;
		case CLD_KILLED:	code = "CLD_KILLED";	break;
		case CLD_DUMPED:	code = "CLD_DUMPED";	break;
		case CLD_TRAPPED:	code = "CLD_TRAPPED";	break;
		case CLD_STOPPED:	code = "CLD_STOPPED";	break;
		case CLD_CONTINUED:	code = "CLD_CONTINUED";	break;
		}
		break;
	case SIGPOLL:
		switch (sip->si_code) {
		case POLL_IN:		code = "POLL_IN";	break;
		case POLL_OUT:		code = "POLL_OUT";	break;
		case POLL_MSG:		code = "POLL_MSG";	break;
		case POLL_ERR:		code = "POLL_ERR";	break;
		case POLL_PRI:		code = "POLL_PRI";	break;
		case POLL_HUP:		code = "POLL_HUP";	break;
		}
		break;
	}

	if (code == NULL) {
		(void) sprintf(code_buf, "code=%d", sip->si_code);
		code = (const char *)code_buf;
	}

	switch (sip->si_signo) {
	case SIGILL:
	case SIGTRAP:
	case SIGFPE:
	case SIGSEGV:
	case SIGBUS:
	case SIGEMT:
		(void) printf(" %s addr=0x%.8lX",
			code,
			(long)sip->si_addr);
		break;
	case SIGCLD:
		(void) printf(" %s pid=%d status=0x%.4X",
			code,
			(int)sip->si_pid,
			sip->si_status);
		break;
	case SIGPOLL:
	case SIGXFSZ:
		(void) printf(" %s fd=%d band=%ld",
			code,
			sip->si_fd,
			sip->si_band);
		break;
	}

	if (sip->si_errno != 0) {
		const char *ename = errname(sip->si_errno);

		(void) printf(" errno=%d", sip->si_errno);
		if (ename != NULL)
			(void) printf("(%s)", ename);
	}

	(void) fputc('\n', stdout);
}

#ifdef _LP64
static void
print_siginfo32(const siginfo32_t *sip)
{
	const char *code = NULL;

	(void) printf("%s      siginfo: %s", pname, signame(sip->si_signo));

	if (sip->si_signo != 0 && SI_FROMUSER(sip) && sip->si_pid != 0) {
		(void) printf(" pid=%d uid=%d", sip->si_pid, sip->si_uid);
		if (sip->si_code != 0)
			(void) printf(" code=%d", sip->si_code);
		(void) fputc('\n', stdout);
		return;
	}

	switch (sip->si_signo) {
	default:
		(void) fputc('\n', stdout);
		return;
	case SIGILL:
	case SIGTRAP:
	case SIGFPE:
	case SIGSEGV:
	case SIGBUS:
	case SIGEMT:
	case SIGCLD:
	case SIGPOLL:
	case SIGXFSZ:
		break;
	}

	switch (sip->si_signo) {
	case SIGILL:
		switch (sip->si_code) {
		case ILL_ILLOPC:	code = "ILL_ILLOPC";	break;
		case ILL_ILLOPN:	code = "ILL_ILLOPN";	break;
		case ILL_ILLADR:	code = "ILL_ILLADR";	break;
		case ILL_ILLTRP:	code = "ILL_ILLTRP";	break;
		case ILL_PRVOPC:	code = "ILL_PRVOPC";	break;
		case ILL_PRVREG:	code = "ILL_PRVREG";	break;
		case ILL_COPROC:	code = "ILL_COPROC";	break;
		case ILL_BADSTK:	code = "ILL_BADSTK";	break;
		}
		break;
	case SIGTRAP:
		switch (sip->si_code) {
		case TRAP_BRKPT:	code = "TRAP_BRKPT";	break;
		case TRAP_TRACE:	code = "TRAP_TRACE";	break;
		}
		break;
	case SIGFPE:
		switch (sip->si_code) {
		case FPE_INTDIV:	code = "FPE_INTDIV";	break;
		case FPE_INTOVF:	code = "FPE_INTOVF";	break;
		case FPE_FLTDIV:	code = "FPE_FLTDIV";	break;
		case FPE_FLTOVF:	code = "FPE_FLTOVF";	break;
		case FPE_FLTUND:	code = "FPE_FLTUND";	break;
		case FPE_FLTRES:	code = "FPE_FLTRES";	break;
		case FPE_FLTINV:	code = "FPE_FLTINV";	break;
		case FPE_FLTSUB:	code = "FPE_FLTSUB";	break;
		}
		break;
	case SIGSEGV:
		switch (sip->si_code) {
		case SEGV_MAPERR:	code = "SEGV_MAPERR";	break;
		case SEGV_ACCERR:	code = "SEGV_ACCERR";	break;
		}
		break;
	case SIGEMT:
		switch (sip->si_code) {
#ifdef EMT_TAGOVF
		case EMT_TAGOVF:	code = "EMT_TAGOVF";	break;
#endif
		case EMT_CPCOVF:	code = "EMT_CPCOVF";	break;
		}
		break;
	case SIGBUS:
		switch (sip->si_code) {
		case BUS_ADRALN:	code = "BUS_ADRALN";	break;
		case BUS_ADRERR:	code = "BUS_ADRERR";	break;
		case BUS_OBJERR:	code = "BUS_OBJERR";	break;
		}
		break;
	case SIGCLD:
		switch (sip->si_code) {
		case CLD_EXITED:	code = "CLD_EXITED";	break;
		case CLD_KILLED:	code = "CLD_KILLED";	break;
		case CLD_DUMPED:	code = "CLD_DUMPED";	break;
		case CLD_TRAPPED:	code = "CLD_TRAPPED";	break;
		case CLD_STOPPED:	code = "CLD_STOPPED";	break;
		case CLD_CONTINUED:	code = "CLD_CONTINUED";	break;
		}
		break;
	case SIGPOLL:
		switch (sip->si_code) {
		case POLL_IN:		code = "POLL_IN";	break;
		case POLL_OUT:		code = "POLL_OUT";	break;
		case POLL_MSG:		code = "POLL_MSG";	break;
		case POLL_ERR:		code = "POLL_ERR";	break;
		case POLL_PRI:		code = "POLL_PRI";	break;
		case POLL_HUP:		code = "POLL_HUP";	break;
		}
		break;
	}

	if (code == NULL) {
		(void) sprintf(code_buf, "code=%d", sip->si_code);
		code = (const char *)code_buf;
	}

	switch (sip->si_signo) {
	case SIGILL:
	case SIGTRAP:
	case SIGFPE:
	case SIGSEGV:
	case SIGBUS:
	case SIGEMT:
		(void) printf(" %s addr=0x%.8X",
			code,
			sip->si_addr);
		break;
	case SIGCLD:
		(void) printf(" %s pid=%d status=0x%.4X",
			code,
			sip->si_pid,
			sip->si_status);
		break;
	case SIGPOLL:
	case SIGXFSZ:
		(void) printf(" %s fd=%d band=%d",
			code,
			sip->si_fd,
			sip->si_band);
		break;
	}

	if (sip->si_errno != 0) {
		const char *ename = errname(sip->si_errno);

		(void) printf(" errno=%d", sip->si_errno);
		if (ename != NULL)
			(void) printf("(%s)", ename);
	}

	(void) fputc('\n', stdout);
}
#endif	/* _LP64 */

static void
show_bool(struct ps_prochandle *Pr, long offset, int count)
{
	int serial = (count > BUFSIZ/4);

	/* enter region of lengthy output */
	if (serial)
		Eserialize();

	while (count > 0) {
		char buf[32];
		int nb = (count < 32)? count : 32;
		int i;

		if (Pread(Pr, buf, (size_t)nb, offset) != nb)
			break;

		(void) printf("%s   ", pname);
		for (i = 0; i < nb; i++)
			(void) printf(" %d", buf[i]);
		(void) fputc('\n', stdout);

		count -= nb;
		offset += nb;
	}

	/* exit region of lengthy output */
	if (serial)
		Xserialize();
}

static void
show_iovec(struct ps_prochandle *Pr,
	long offset, long niov, int showbuf, long count)
{
	iovec_t iovec[16];
	iovec_t *ip;
	long nb;
	int serial = (count > BUFSIZ/4 && showbuf);

#ifdef _LP64
	if (data_model != PR_MODEL_LP64) {
		show_iovec32(Pr, offset, niov, showbuf, count);
		return;
	}
#endif
	if (niov > 16)		/* is this the real limit? */
		niov = 16;

	if (offset != NULL && niov > 0 &&
	    Pread(Pr, &iovec[0], niov*sizeof (iovec_t), offset)
	    == niov*sizeof (iovec_t)) {
		/* enter region of lengthy output */
		if (serial)
			Eserialize();

		for (ip = &iovec[0]; niov-- && !interrupt; ip++) {
			(void) printf("%s\tiov_base = 0x%.8lX  iov_len = %lu\n",
				pname,
				(long)ip->iov_base,
				ip->iov_len);
			if ((nb = count) > 0) {
				if (nb > ip->iov_len)
					nb = ip->iov_len;
				if (nb > 0)
					count -= nb;
			}
			if (showbuf && nb > 0)
				showbuffer(Pr, (long)ip->iov_base, nb);
		}

		/* exit region of lengthy output */
		if (serial)
			Xserialize();
	}
}

#ifdef _LP64
static void
show_iovec32(struct ps_prochandle *Pr,
	long offset, int niov, int showbuf, long count)
{
	iovec32_t iovec[16];
	iovec32_t *ip;
	long nb;
	int serial = (count > BUFSIZ/4 && showbuf);

	if (niov > 16)		/* is this the real limit? */
		niov = 16;

	if (offset != NULL && niov > 0 &&
	    Pread(Pr, &iovec[0], niov*sizeof (iovec32_t), offset)
	    == niov*sizeof (iovec32_t)) {
		/* enter region of lengthy output */
		if (serial)
			Eserialize();

		for (ip = &iovec[0]; niov-- && !interrupt; ip++) {
			(void) printf("%s\tiov_base = 0x%.8X  iov_len = %d\n",
				pname,
				ip->iov_base,
				ip->iov_len);
			if ((nb = count) > 0) {
				if (nb > ip->iov_len)
					nb = ip->iov_len;
				if (nb > 0)
					count -= nb;
			}
			if (showbuf && nb > 0)
				showbuffer(Pr, (long)ip->iov_base, nb);
		}

		/* exit region of lengthy output */
		if (serial)
			Xserialize();
	}
}
#endif	/* _LP64 */

static void
show_dents32(struct ps_prochandle *Pr, long offset, long count)
{
	long buf[BUFSIZ/sizeof (long)];
	struct dirent32 *dp;
	int serial = (count > 100);

	if (offset == NULL)
		return;

	/* enter region of lengthy output */
	if (serial)
		Eserialize();

	while (count > 0 && !interrupt) {
		int nb = count < BUFSIZ? count : BUFSIZ;

		if ((nb = Pread(Pr, &buf[0], (size_t)nb, offset)) <= 0)
			break;

		dp = (struct dirent32 *)&buf[0];
		if (nb < (int)(dp->d_name - (char *)dp))
			break;
		if ((unsigned)nb < dp->d_reclen) {
			/* getdents() error? */
			(void) printf(
			"%s    ino=%-5u off=%-4d rlen=%-3d\n",
				pname,
				dp->d_ino,
				dp->d_off,
				dp->d_reclen);
			break;
		}

		while (!interrupt &&
		    nb >= (int)(dp->d_name - (char *)dp) &&
		    (unsigned)nb >= dp->d_reclen) {
			(void) printf(
			"%s    ino=%-5u off=%-4d rlen=%-3d \"%.*s\"\n",
				pname,
				dp->d_ino,
				dp->d_off,
				dp->d_reclen,
				dp->d_reclen - (int)(dp->d_name - (char *)dp),
				dp->d_name);
			nb -= dp->d_reclen;
			count -= dp->d_reclen;
			offset += dp->d_reclen;
			/* LINTED improper alignment */
			dp = (struct dirent32 *)((char *)dp + dp->d_reclen);
		}
	}

	/* exit region of lengthy output */
	if (serial)
		Xserialize();
}

static void
show_dents64(struct ps_prochandle *Pr, long offset, long count)
{
	long long buf[BUFSIZ/sizeof (long long)];
	struct dirent64 *dp;
	int serial = (count > 100);

	if (offset == NULL)
		return;

	/* enter region of lengthy output */
	if (serial)
		Eserialize();

	while (count > 0 && !interrupt) {
		int nb = count < BUFSIZ? count : BUFSIZ;

		if ((nb = Pread(Pr, &buf[0], (size_t)nb, offset)) <= 0)
			break;

		dp = (struct dirent64 *)&buf[0];
		if (nb < (int)(dp->d_name - (char *)dp))
			break;
		if ((unsigned)nb < dp->d_reclen) {
			/* getdents() error? */
			(void) printf(
			"%s    ino=%-5llu off=%-4lld rlen=%-3d\n",
				pname,
				(long long)dp->d_ino,
				(long long)dp->d_off,
				dp->d_reclen);
			break;
		}

		while (!interrupt &&
		    nb >= (int)(dp->d_name - (char *)dp) &&
		    (unsigned)nb >= dp->d_reclen) {
			(void) printf(
			"%s    ino=%-5llu off=%-4lld rlen=%-3d \"%.*s\"\n",
				pname,
				(long long)dp->d_ino,
				(long long)dp->d_off,
				dp->d_reclen,
				dp->d_reclen - (int)(dp->d_name - (char *)dp),
				dp->d_name);
			nb -= dp->d_reclen;
			count -= dp->d_reclen;
			offset += dp->d_reclen;
			/* LINTED improper alignment */
			dp = (struct dirent64 *)((char *)dp + dp->d_reclen);
		}
	}

	/* exit region of lengthy output */
	if (serial)
		Xserialize();
}

static void
show_rlimit32(struct ps_prochandle *Pr, long offset)
{
	struct rlimit32 rlimit;

	if (offset != NULL &&
	    Pread(Pr, &rlimit, sizeof (rlimit), offset) == sizeof (rlimit)) {
		(void) printf("%s\t", pname);
		switch (rlimit.rlim_cur) {
		case RLIM32_INFINITY:
			(void) fputs("cur = RLIM_INFINITY", stdout);
			break;
		case RLIM32_SAVED_MAX:
			(void) fputs("cur = RLIM_SAVED_MAX", stdout);
			break;
		case RLIM32_SAVED_CUR:
			(void) fputs("cur = RLIM_SAVED_CUR", stdout);
			break;
		default:
			(void) printf("cur = %lu", (long)rlimit.rlim_cur);
			break;
		}
		switch (rlimit.rlim_max) {
		case RLIM32_INFINITY:
			(void) fputs("  max = RLIM_INFINITY\n", stdout);
			break;
		case RLIM32_SAVED_MAX:
			(void) fputs("  max = RLIM_SAVED_MAX\n", stdout);
			break;
		case RLIM32_SAVED_CUR:
			(void) fputs("  max = RLIM_SAVED_CUR\n", stdout);
			break;
		default:
			(void) printf("  max = %lu\n", (long)rlimit.rlim_max);
			break;
		}
	}
}

static void
show_rlimit64(struct ps_prochandle *Pr, long offset)
{
	struct rlimit64 rlimit;

	if (offset != NULL &&
	    Pread(Pr, &rlimit, sizeof (rlimit), offset) == sizeof (rlimit)) {
		(void) printf("%s\t", pname);
		switch (rlimit.rlim_cur) {
		case RLIM64_INFINITY:
			(void) fputs("cur = RLIM64_INFINITY", stdout);
			break;
		case RLIM64_SAVED_MAX:
			(void) fputs("cur = RLIM64_SAVED_MAX", stdout);
			break;
		case RLIM64_SAVED_CUR:
			(void) fputs("cur = RLIM64_SAVED_CUR", stdout);
			break;
		default:
			(void) printf("cur = %llu", rlimit.rlim_cur);
			break;
		}
		switch (rlimit.rlim_max) {
		case RLIM64_INFINITY:
			(void) fputs("  max = RLIM64_INFINITY\n", stdout);
			break;
		case RLIM64_SAVED_MAX:
			(void) fputs("  max = RLIM64_SAVED_MAX\n", stdout);
			break;
		case RLIM64_SAVED_CUR:
			(void) fputs("  max = RLIM64_SAVED_CUR\n", stdout);
			break;
		default:
			(void) printf("  max = %llu\n", rlimit.rlim_max);
			break;
		}
	}
}

static void
show_nuname(struct ps_prochandle *Pr, long offset)
{
	struct utsname ubuf;

	if (offset != NULL &&
	    Pread(Pr, &ubuf, sizeof (ubuf), offset) == sizeof (ubuf)) {
		(void) printf(
		"%s\tsys=%s nod=%s rel=%s ver=%s mch=%s\n",
			pname,
			ubuf.sysname,
			ubuf.nodename,
			ubuf.release,
			ubuf.version,
			ubuf.machine);
	}
}

static void
show_adjtime(struct ps_prochandle *Pr, long off1, long off2)
{
	show_timeval(Pr, off1, "   delta");
	show_timeval(Pr, off2, "olddelta");
}

static void
show_sockaddr(struct ps_prochandle *Pr,
	const char *str, long addroff, long lenoff, long len)
{
	/*
	 * A buffer large enough for PATH_MAX size AF_UNIX address, which is
	 * also large enough to store a sockaddr_in or a sockaddr_in6.
	 */
	long buf[(sizeof (short) + PATH_MAX + sizeof (long) - 1)
		/ sizeof (long)];
	struct sockaddr *sa = (struct sockaddr *)buf;
	struct sockaddr_in *sin = (struct sockaddr_in *)buf;
	struct sockaddr_un *soun = (struct sockaddr_un *)buf;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)buf;
	char addrbuf[INET6_ADDRSTRLEN];
	struct in_addr unmapped;

	if (lenoff != 0) {
		uint_t ilen;
		if (Pread(Pr, &ilen, sizeof (ilen), lenoff) != sizeof (ilen))
			return;
		len = ilen;
	}

	if (len >= sizeof (buf))	/* protect against ridiculous length */
		len = sizeof (buf) - 1;
	if (Pread(Pr, buf, len, addroff) != len)
		return;

	switch (sa->sa_family) {
	case AF_INET6:
		if (!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
			(void) printf("%s\tAF_INET6  %s = %s  port = %u\n",
			    pname, str,
			    inet_ntop(AF_INET6, &sin6->sin6_addr, addrbuf,
				sizeof (addrbuf)),
			    ntohs(sin6->sin6_port));
			break;
		}
		IN6_V4MAPPED_TO_INADDR(&sin6->sin6_addr, &unmapped);
		(void) printf("%s\tAF_INET  %s = %s  port = %u\n", pname, str,
		    inet_ntop(AF_INET, &unmapped, addrbuf, sizeof (addrbuf)),
		    ntohs(sin6->sin6_port));
		break;
	case AF_INET:
		(void) printf("%s\tAF_INET  %s = %s  port = %u\n", pname, str,
		    inet_ntop(AF_INET, &sin->sin_addr, addrbuf,
			sizeof (addrbuf)),
		    ntohs(sin->sin_port));
		break;
	case AF_UNIX:
		len -= sizeof (soun->sun_family);
		if (len >= 0) {
			/* Null terminate */
			soun->sun_path[len] = NULL;
			(void) printf("%s\tAF_UNIX  %s = %s\n", pname,
				str, soun->sun_path);
		}
		break;
	}
}

static void
show_msghdr(struct ps_prochandle *Pr, long offset)
{
	struct msghdr msg;

	if (Pread(Pr, &msg, sizeof (msg), offset) != sizeof (msg))
		return;
	if (msg.msg_name != NULL && msg.msg_namelen != 0)
		show_sockaddr(Pr, "msg_name",
			(long)msg.msg_name, 0, (long)msg.msg_namelen);
	/* XXX add msg_iov printing */
}

#ifdef _LP64
static void
show_msghdr32(struct ps_prochandle *Pr, long offset)
{
	struct msghdr32 {
		caddr32_t	msg_name;
		int32_t		msg_namelen;
	} msg;

	if (Pread(Pr, &msg, sizeof (msg), offset) != sizeof (msg))
		return;
	if (msg.msg_name != NULL && msg.msg_namelen != 0)
		show_sockaddr(Pr, "msg_name",
			(long)msg.msg_name, 0, (long)msg.msg_namelen);
	/* XXX add msg_iov printing */
}
#endif	/* _LP64 */

static void
prtime(const char *name, time_t value)
{
	char str[80];

	(void) strftime(str, sizeof (str), "%b %e %H:%M:%S %Z %Y",
		localtime(&value));
	(void) printf("%s\t%s%s  [ %llu ]\n",
	    pname,
	    name,
	    str,
	    (longlong_t)value);
}

void
prtimestruc(const char *name, timestruc_t *value)
{
	prtime(name, value->tv_sec);
}
