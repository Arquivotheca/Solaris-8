/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ptrace.c	1.25	99/03/22 SMI"

/*
 * ptrace(2)/wait(2) interface built on top of proc(4).
 * proc_wait(pid, wait_loc), defined here, must be used
 * instead of wait(wait_loc).
 */

#include <stdio.h>
#include <stdlib.h>
#include "adb.h"
#include "ptrace.h"
#include <memory.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/auxv.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <procfs.h>
#include <sys/uio.h>
#include <sys/regset.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/psw.h>
#include <sys/stat.h>

/* condition code bit indicating syscall failure */
#if __sparcv9
#define	R_CCREG	R_CCR
#define	ERRBIT	0x1
#elif sparc
#define	R_CCREG	R_PSR
#define	ERRBIT	PSR_C
#elif i386
#define	R_CCREG	EFL
#define	ERRBIT	PS_C
#endif

#define	TRUE	1
#define	FALSE	0

/* external routines defined in this module */
extern	long	ptrace(int, pid_t, long, long, caddr_t);
extern	int	proc_wait(pid_t, int *);
extern	auxv_t	*FetchAuxv(void);
extern	char	*map(int);
/* static routines defined in this module */
static	int	Dupfd(int, int);
static	void	MakeProcName(char *, pid_t);
static	void	MakeLwpName(char *, pid_t, id_t);
static	int	OpenProc(pid_t);
static	void	CloseProc(void);
static	int	OpenLwp(pid_t, id_t);
static	void	CloseLwp(void);
static	int	GrabProc(pid_t);
static	int	FirstOpen(pid_t);

static	pid_t	process_id = 0;	/* pid of process under control */
static	char	stopped = FALSE;
static	char	setregs = FALSE;
static	char	setfpregs = FALSE;
static	int	asfd = -1;		/* /proc/<pid>/as */
static	int	ctlfd = -1;		/* /proc/<pid>/ctl */
static	int	statusfd = -1;		/* /proc/<pid>/status */
static	id_t	lwp_id = 0;
static	int	lwpctlfd = -1;		/* /proc/<pid>/lwp/<lwpid>/lwpctl */
static	int	lwpstatusfd = -1;	/* /proc/<pid>/lwp/<lwpid>/lwpstatus */

long
ptrace(int request, pid_t pid, long addr, long data, caddr_t addr2)
{
	pstatus_t *ps = &Prstatus;
#if sparc
	extern int v9flag;
	static long size;
#endif
	int status;
	iovec_t iov[2];
	struct {
		long cmd;
		union {
			long flags;
			sysset_t syscalls;
			siginfo_t siginfo;
			prwatch_t prwatch;
		} arg;
	} ctl;

	db_printf(5, "ptrace: request=%s, pid=%D, addr=%lX, data=%lX, addr2=%X",
	    map(request), (long)pid, addr, data, addr2);

	if (request == PTRACE_TRACEME) {	/* executed by traced process */
		/*
		 * Set stop-on-exit-from-exec flags.
		 */
		char procname[64];	/* /proc/<pid>/ctl */
		int fd;

		MakeProcName(procname, getpid());
		(void) strcat(procname, "/ctl");
		if ((fd = open(procname, O_WRONLY, 0)) < 0)
			_exit(255);
		premptyset(&ctl.arg.syscalls);
		praddset(&ctl.arg.syscalls, SYS_exec);
		praddset(&ctl.arg.syscalls, SYS_execve);
		ctl.cmd = PCSENTRY;
		if (write(fd, (char *)&ctl, sizeof (long)+sizeof (sysset_t))
		    != sizeof (long)+sizeof (sysset_t))
			_exit(255);
		ctl.cmd = PCSEXIT;
		if (write(fd, (char *)&ctl, sizeof (long)+sizeof (sysset_t))
		    != sizeof (long)+sizeof (sysset_t))
			_exit(255);
		if (close(fd) != 0)
			_exit(255);

		return (0);
	}

	/*
	 * adb doesn't control more than one process at the same time.
	 */
	if (pid != process_id && process_id != 0) {
		errno = ESRCH;
		return (-1);
	}

	if (request == PTRACE_ATTACH) {
		int retry = 2;

		while (GrabProc(pid) < 0) {
			CloseProc();
			if (--retry < 0 || errno != EAGAIN) {
				if (errno == ENOENT)
					errno = ESRCH;
				else if (errno == EBUSY)
					errno = EPERM;
				return (-1);
			}
			(void) sleep(1);
		}
		if (SetTracingFlags() != 0) {
			CloseProc();
			return (-1);
		}
		return (0);
	}

again:
	errno = 0;

	switch (request) {
	case PTRACE_PEEKUSER:
		break;

	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA:
#if !i386	/* no alignment restriction on x86 */
		if (addr & 03)
			goto eio;
#endif
		errno = 0;
		if (pread(asfd, (char *)&data, sizeof (data), (off_t)addr)
		    == sizeof (data))
			return (data);
		goto tryagain;

	case PTRACE_POKEUSER:
		break;

	case PTRACE_POKETEXT:
	case PTRACE_POKEDATA:
#if !i386	/* no alignment restriction on x86 */
		if (addr & 03)
			goto eio;
#endif
#if sparc
		if ((caddr_t)ps->pr_lwp.pr_reg[R_SP] <
		    (caddr_t)addr + sizeof (data) &&
		    (caddr_t)ps->pr_lwp.pr_reg[R_SP] + 16*sizeof (int) >
		    (caddr_t)addr)
			setregs = TRUE;
#endif
		if (pwrite(asfd, (char *)&data, sizeof (data), (off_t)addr)
		    == sizeof (data))
			return (0);
		goto tryagain;

	case PTRACE_SYSCALL:
		break;

	case PTRACE_SINGLESTEP:
	case PTRACE_CONT:
	    {
		long runctl[3];

		CloseLwp();

		if (request == PTRACE_SINGLESTEP && ps->pr_lwp.pr_lwpid) {
			/*
			 * If stepping, some LWP must be stopped.
			 * Open that LWP, and step only that one.
			 */
			if (OpenLwp(pid, ps->pr_lwp.pr_lwpid) == -1)
				fprintf(stderr,
				    "Unable to open LWP for step.\n");
		}
		if (addr != 1 &&	/* new virtual address */
		    addr != ps->pr_lwp.pr_reg[R_PC]) {
			ps->pr_lwp.pr_reg[R_PC] = (prgreg_t)addr;
#if sparc
			ps->pr_lwp.pr_reg[R_nPC] = (prgreg_t)addr + 4;
#endif
			setregs = TRUE;
		}
		if (setregs) {
#if sparc
			(void) pread(asfd, (char *)&ps->pr_lwp.pr_reg[R_L0],
				16*sizeof (int),
				(off_t)ps->pr_lwp.pr_reg[R_SP]);
#endif
			ctl.cmd = PCSREG;
			iov[0].iov_base = (caddr_t)&ctl.cmd;
			iov[0].iov_len = sizeof (long);
			iov[1].iov_base = (caddr_t)&ps->pr_lwp.pr_reg[0];
			iov[1].iov_len = sizeof (ps->pr_lwp.pr_reg);
			if (writev(ctlfd, iov, 2) < 0)
				goto tryagain;
			setregs = FALSE;
		}
		if (setfpregs) {
			ctl.cmd = PCSFPREG;
			iov[0].iov_base = (caddr_t)&ctl.cmd;
			iov[0].iov_len = sizeof (long);
			iov[1].iov_base = (caddr_t)&ps->pr_lwp.pr_fpreg;
			iov[1].iov_len = sizeof (ps->pr_lwp.pr_fpreg);
			if (writev(ctlfd, iov, 2) < 0)
				goto tryagain;
			setfpregs = FALSE;
		}
		/* make data the current signal */
		if (data != 0 && data != ps->pr_lwp.pr_cursig) {
			(void) memset((char *)&ctl.arg.siginfo, 0,
			    sizeof (ctl.arg.siginfo));
			ctl.arg.siginfo.si_signo = data;
			ctl.cmd = PCSSIG;
			if (write(ctlfd, (char *)&ctl,
			    sizeof (long)+sizeof (siginfo_t)) < 0)
				goto tryagain;
		}
		if (data == 0)
			runctl[0] = PCCSIG;
		else
			runctl[0] = PCNULL;
		runctl[1] = PCRUN;
		runctl[2] = (request == PTRACE_SINGLESTEP)? PRSTEP : 0;
		if (write((lwpctlfd >= 0)? lwpctlfd : ctlfd,
		    (char *)runctl, 3*sizeof (long)) < 0)
			goto tryagain;
		stopped = FALSE;
		return (0);
	    }

	case PTRACE_KILL:
		/* overkill? */
		(void) memset((char *)&ctl.arg.siginfo, 0,
		    sizeof (ctl.arg.siginfo));
		ctl.arg.siginfo.si_signo = SIGKILL;
		ctl.cmd = PCSSIG;
		(void) write(ctlfd, (char *)&ctl,
		    sizeof (long)+sizeof (siginfo_t));
		(void) kill(pid, SIGKILL);
		ctl.cmd = PCRUN;
		ctl.arg.flags = 0;
		(void) write(ctlfd, (char *)&ctl, 2*sizeof (long));
		/* Put a stake through the heart of this zombie. */
		(void) proc_wait(pid, &status);
		CloseProc();
		return (0);

	case PTRACE_DETACH:
		if (addr != 1 &&	/* new virtual address */
		    addr != ps->pr_lwp.pr_reg[R_PC]) {
			ps->pr_lwp.pr_reg[R_PC] = (prgreg_t)addr;
#if sparc
			ps->pr_lwp.pr_reg[R_nPC] = (prgreg_t)addr + 4;
#endif
			setregs = TRUE;
		}
		if (setregs) {
#if sparc
			(void) pread(asfd, (char *)&ps->pr_lwp.pr_reg[R_L0],
				16*sizeof (int),
				(off_t)ps->pr_lwp.pr_reg[R_SP]);
#endif
			ctl.cmd = PCSREG;
			iov[0].iov_base = (caddr_t)&ctl.cmd;
			iov[0].iov_len = sizeof (long);
			iov[1].iov_base = (caddr_t)&ps->pr_lwp.pr_reg[0];
			iov[1].iov_len = sizeof (ps->pr_lwp.pr_reg);
			if (writev(ctlfd, iov, 2) < 0)
				goto tryagain;
			setregs = FALSE;
		}
		if (setfpregs) {
			ctl.cmd = PCSFPREG;
			iov[0].iov_base = (caddr_t)&ctl.cmd;
			iov[0].iov_len = sizeof (long);
			iov[1].iov_base = (caddr_t)&ps->pr_lwp.pr_fpreg;
			iov[1].iov_len = sizeof (ps->pr_lwp.pr_fpreg);
			if (writev(ctlfd, iov, 2) < 0)
				goto tryagain;
			setfpregs = FALSE;
		}
		if (data != ps->pr_lwp.pr_cursig) {
			(void) memset((char *)&ctl.arg.siginfo, 0,
			    sizeof (ctl.arg.siginfo));
			ctl.arg.siginfo.si_signo = data;
			ctl.cmd = PCSSIG;
			if (write(ctlfd, (char *)&ctl,
			    sizeof (long)+sizeof (siginfo_t)) < 0)
				goto tryagain;
		}
		CloseProc();	/* this sets the process running */
		return (0);

	case PTRACE_GETREGS:
	    {
		/* Nothing to do; they really live in the global struct. */
		return (0);
	    }

	case PTRACE_SETREGS:
	    {
		/* Just record that there has been a change. */
		setregs = TRUE;
		return (0);
	    }

	case PTRACE_GETFPAREGS:
		break;

	case PTRACE_GETFPREGS:
	    {
		(void) memcpy((char *)addr, (char *)&ps->pr_lwp.pr_fpreg,
		    sizeof (ps->pr_lwp.pr_fpreg));
#if sparc
		if (v9flag) {
			char xregname[64];
			int xfd;

			(void) sprintf(xregname, "/proc/%ld/lwp/%ld/xregs",
				process_id, ps->pr_lwp.pr_lwpid);
			if ((xfd = open(xregname, O_RDONLY)) >= 0) {
				(void) read(xfd, (char *)&xregs,
					sizeof (xregs));
				(void) close(xfd);
			}
		}
#endif
		return (0);
	    }

	case PTRACE_SETFPAREGS:
		break;

	case PTRACE_SETFPREGS:
	    {
		(void) memcpy((char *)&ps->pr_lwp.pr_fpreg, (char *)addr,
		    sizeof (ps->pr_lwp.pr_fpreg));
		setfpregs = TRUE;
#if sparc
		if (v9flag) {
			ctl.cmd = PCSXREG;
			iov[0].iov_base = (caddr_t)&ctl.cmd;
			iov[0].iov_len = sizeof (long);
			iov[1].iov_base = (caddr_t)&xregs;
			iov[1].iov_len = sizeof (xregs);
			if (writev(ctlfd, iov, 2) < 0)
				goto tryagain;
		}
#endif
		return (0);
	    }

#if sparc
	case PTRACE_GETWINDOW:
	case PTRACE_SETWINDOW:
		break;
#endif

	case PTRACE_READDATA:
	case PTRACE_READTEXT:
		if (data <= 0)
			goto eio;
		errno = 0;
		if (pread(asfd, (char *)addr2, (unsigned)data, (off_t)addr)
		    == data)
			return (0);
		goto tryagain;

	case PTRACE_WRITEDATA:
	case PTRACE_WRITETEXT:
		if (data <= 0)
			goto eio;
#if sparc
		if ((caddr_t)ps->pr_lwp.pr_reg[R_SP] <
		    (caddr_t)addr+(unsigned)data &&
		    (caddr_t)ps->pr_lwp.pr_reg[R_SP]+16*sizeof (int) >
		    (caddr_t)addr)
			setregs = TRUE;
#endif
		if (pwrite(asfd, (char *)addr2, (unsigned)data, (off_t)addr)
		    == data)
			return (0);
		goto tryagain;

	case PTRACE_DUMPCORE:
	case PTRACE_TRAPCODE:
		break;

	/*
	 * Set a watched area.  There are no restrictions on the alignment
	 * or length of the area or on the the number of watched areas.
	 * The only restriction is that no two areas may overlap.
	 * If the size (data) argument passed to ptrace() is <= 0,
	 * then the watched area is cancelled rather than set.
	 */
	case PTRACE_SETWRBKPT:
	case PTRACE_SETACBKPT:
	case PTRACE_SETBPP:
	    {
		extern int trapafter;	/* global to adb */

		ctl.cmd = PCWATCH;
		switch (request) {
		case PTRACE_SETWRBKPT:
			ctl.arg.prwatch.pr_wflags = WA_WRITE;
			break;
		case PTRACE_SETACBKPT:
			ctl.arg.prwatch.pr_wflags = WA_READ|WA_WRITE;
			break;
		case PTRACE_SETBPP:
			ctl.arg.prwatch.pr_wflags = WA_EXEC;
			break;
		}
		ctl.arg.prwatch.pr_vaddr = (uintptr_t)addr;
		if (data > 0) {
			if (trapafter)
				ctl.arg.prwatch.pr_wflags |= WA_TRAPAFTER;
			ctl.arg.prwatch.pr_size = data;
		} else {
			ctl.arg.prwatch.pr_wflags = 0;
			ctl.arg.prwatch.pr_size = 0;
		}
		if (write(ctlfd, (char *)&ctl,
		    sizeof (long)+sizeof (prwatch_t)) < 0)
			return (-1);
		return (0);
	    }

	case PTRACE_CLRDR7:	/* clear all watchpoints */
	    {
		char watchfile[64];
		int wfd;
		prwatch_t wbuf[100];
		int nwa;
		int i;

		(void) sprintf(watchfile, "/proc/%ld/watch", process_id);
		if ((wfd = open(watchfile, O_RDONLY)) < 0)
			return (0);
		for (;;) {
			nwa = pread(wfd, (char *)&wbuf[0], sizeof (wbuf), 0);
			if (nwa <= 0 || (nwa /= sizeof (prwatch_t)) == 0)
				break;
			for (i = 0; i < nwa; i++) {
				ctl.cmd = PCWATCH;
				iov[0].iov_base = (caddr_t)&ctl.cmd;
				iov[0].iov_len = sizeof (long);
				iov[1].iov_base = (caddr_t)&wbuf[i];
				iov[1].iov_len = sizeof (prwatch_t);
				wbuf[i].pr_wflags = 0;
				if (writev(ctlfd, iov, 2) < 0) {
					(void) close(wfd);
					goto tryagain;
				}
			}
			if (nwa < (sizeof (wbuf) / sizeof (prwatch_t)))
				break;
		}
		(void) close(wfd);
		return (0);
	    }
	}

	/* unimplemented request */
	db_printf(1, "ptrace: cannot handle %s!", map(request));
	errno = EIO;
	return (-1);

tryagain:
	if (asfd == -1 || errno == ENOENT) {
		errno = ESRCH;
		return (-1);
	}
	if (errno == EAGAIN && GrabProc(pid) >= 0)
		goto again;
eio:
	errno = EIO;
	return (-1);
}

/*
 * Utility for OpenProc()/OpenLwp().
 */
static int
Dupfd(int fd, int dfd)
{
	/*
	 * Make sure fd not one of 0, 1, or 2 to avoid stdio interference.
	 * Also, if dfd is greater than 2, dup fd to be exactly dfd.
	 */
	if (dfd > 2 || (0 <= fd && fd <= 2)) {
		if (dfd > 2 && fd != dfd)
			(void) close(dfd);
		else
			dfd = 3;
		if (fd != dfd) {
			dfd = fcntl(fd, F_DUPFD, dfd);
			(void) close(fd);
			fd = dfd;
		}
	}
	/*
	 * Mark filedescriptor close-on-exec.
	 * Should also be close-on-return-from-fork-in-child.
	 */
	(void) fcntl(fd, F_SETFD, 1);
	return (fd);
}

/*
 * Construct the /proc directory name:  "/proc/<pid>"
 * The name buffer passed by the caller must be large enough.
 */
static void
MakeProcName(char *procname, pid_t pid)
{
	(void) sprintf(procname, "/proc/%d", pid);
}

/*
 * Construct the /proc directory name:  "/proc/<pid>/lwp/<lwpid>"
 * The name buffer passed by the caller must be large enough.
 */
static void
MakeLwpName(char *procname, pid_t pid, id_t lwpid)
{
	(void) sprintf(procname, "/proc/%d/lwp/%d", pid, lwpid);
}

/*
 * Open/reopen the /proc/<pid> files.
 */
static int
OpenProc(pid_t pid)
{
	char procname[64];		/* /proc/nnnnn/fname */
	char *fname;
	int fd;
	int omode;

	MakeProcName(procname, pid);
	fname = procname + strlen(procname);

	/*
	 * Use exclusive-open only if this is the first open.
	 * Don't use O_EXCL is this is a forced takeover (-F flag).
	 */
	omode = (asfd > 0 || Fflag)? O_RDWR : (O_RDWR|O_EXCL);
	(void) strcpy(fname, "/as");
	if ((fd = open(procname, omode, 0)) < 0 ||
	    (asfd = Dupfd(fd, asfd)) < 0)
		goto err;

	(void) strcpy(fname, "/ctl");
	if ((fd = open(procname, O_WRONLY, 0)) < 0 ||
	    (ctlfd = Dupfd(fd, ctlfd)) < 0)
		goto err;

	(void) strcpy(fname, "/status");
	if ((fd = open(procname, O_RDONLY, 0)) < 0 ||
	    (statusfd = Dupfd(fd, statusfd)) < 0)
		goto err;

	process_id = pid;
	stopped = FALSE;
	setregs = FALSE;
	setfpregs = FALSE;
	return (0);

err:
	CloseProc();
	return (-1);
}

/*
 * Close the /proc/<pid> files.
 */
static void
CloseProc()
{
	CloseLwp();

	if (asfd > 0)
		(void) close(asfd);
	if (ctlfd > 0)
		(void) close(ctlfd);
	if (statusfd > 0)
		(void) close(statusfd);

	memset(&Prstatus, 0, sizeof (Prstatus));
	process_id = 0;
	stopped = FALSE;
	setregs = FALSE;
	setfpregs = FALSE;
	asfd = -1;
	ctlfd = -1;
	statusfd = -1;
}

/*
 * Open/reopen the /proc/<pid>/lwp/<lwpid> files.
 */
static int
OpenLwp(pid_t pid, id_t lwpid)
{
	char procname[64];		/* /proc/nnnnn/lwp/mmm/fname */
	char *fname;
	int fd;

	MakeLwpName(procname, pid, lwpid);
	fname = procname + strlen(procname);

	(void) strcpy(fname, "/lwpctl");
	if ((fd = open(procname, O_WRONLY, 0)) < 0 ||
	    (lwpctlfd = Dupfd(fd, lwpctlfd)) < 0)
		goto err;

	(void) strcpy(fname, "/lwpstatus");
	if ((fd = open(procname, O_RDONLY, 0)) < 0 ||
	    (lwpstatusfd = Dupfd(fd, lwpstatusfd)) < 0)
		goto err;

	lwp_id = lwpid;
	return (0);

err:
	CloseLwp();
	return (-1);
}

/*
 * Close the /proc/<pid>/lwp/<lwpid> files.
 */
static void
CloseLwp()
{
	if (lwpctlfd > 0)
		(void) close(lwpctlfd);
	if (lwpstatusfd > 0)
		(void) close(lwpstatusfd);

	lwp_id = 0;
	lwpctlfd = -1;
	lwpstatusfd = -1;
}

static int
SetTracingFlags()
{
	struct {
		long cmd;
		union {
			sigset_t signals;
			fltset_t faults;
			sysset_t syscalls;
		} arg;
	} ctl;

	/*
	 * Process is stopped; these will "certainly" not fail.
	 */
	ctl.cmd = PCSTRACE;
	prfillset(&ctl.arg.signals);
	if (write(ctlfd, (char *)&ctl, sizeof (long)+sizeof (sigset_t))
	    != sizeof (long)+sizeof (sigset_t))
		return (-1);
	ctl.cmd = PCSFAULT;
	premptyset(&ctl.arg.faults);
	if (write(ctlfd, (char *)&ctl, sizeof (long)+sizeof (fltset_t))
	    != sizeof (long)+sizeof (fltset_t))
		return (-1);
	premptyset(&ctl.arg.syscalls);
	praddset(&ctl.arg.syscalls, SYS_exec);
	praddset(&ctl.arg.syscalls, SYS_execve);
	ctl.cmd = PCSENTRY;
	if (write(ctlfd, (char *)&ctl, sizeof (long)+sizeof (sysset_t))
	    != sizeof (long)+sizeof (sysset_t))
		return (-1);
	ctl.cmd = PCSEXIT;
	if (write(ctlfd, (char *)&ctl, sizeof (long)+sizeof (sysset_t))
	    != sizeof (long)+sizeof (sysset_t))
		return (-1);
	return (0);
}

/*
 * Take control of a child process.
 */
static int
GrabProc(pid_t pid)
{
	long ctl[5];
	pstatus_t *ps = &Prstatus;

	if (pid <= 0)
		return (-1);

reopen:
	while (OpenProc(pid) == 0) {
		/*
		 * Set Run-on-last-close comes before PCSTOP, in case we
		 * get interrupted while waiting for the process to stop.
		 */
		ctl[0] = PCSET;
		ctl[1] = PR_RLC;
		ctl[2] = PCUNSET;
		ctl[3] = PR_FORK;
		ctl[4] = PCSTOP;
		errno = 0;

		if (write(ctlfd, (char *)ctl, 5*sizeof (long))
		    == 5*sizeof (long) &&
		    pread(statusfd, (char *)ps, sizeof (*ps),
		    (off_t)0) == sizeof (*ps)) {
			ctl[0] = PCRUN;
			ctl[1] = PRSTOP;
			ctl[2] = PCWSTOP;
			while (ps->pr_lwp.pr_why == PR_SYSENTRY ||
			    ps->pr_lwp.pr_why == PR_SYSEXIT) {
				if (write(ctlfd, (char *)ctl, 3*sizeof (long))
				    != 3*sizeof (long) ||
				    pread(statusfd, (char *)ps, sizeof (*ps),
				    (off_t)0) != sizeof (*ps)) {
					if (errno == EAGAIN)
						goto reopen;
					CloseProc();
					return (-1);
				}
			}
			stopped = TRUE;
			return (0);
		}

		if (errno != EAGAIN)
			break;
	}

	CloseProc();
	return (-1);
}

/*
 * The first open() of the /proc file by the parent.
 */
static int
FirstOpen(pid_t pid)
{
	char procname[64];		/* /proc/nnnnn/fname */
	char *fname;
	int fd;
	long ctl[1];

	MakeProcName(procname, pid);
	fname = procname + strlen(procname);

	/*
	 * See if child has finished its ptrace(0,0,0,0)
	 * and has come to a stop.
	 */
	for (;;) {
		(void) strcpy(fname, "/ctl");
		if ((fd = open(procname, O_WRONLY, 0)) < 0 ||
		    (fd = Dupfd(fd, 0)) < 0) {
			errno = ESRCH;
			return (-1);
		}
		ctl[0] = PCWSTOP;
		if (write(fd, (char *)ctl, sizeof (long)) == sizeof (long))
			break;
		(void) close(fd);
		if (errno != EAGAIN) {
			if (errno == ENOENT)
				errno = ESRCH;
			else if (errno == EBUSY)
				errno = EPERM;
			return (-1);
		}
	}
	(void) close(fd);

	/* now open the process for real */
	if (GrabProc(pid) < 0) {
		errno = ESRCH;
		return (-1);
	}
	if (SetTracingFlags() != 0) {
		CloseProc();
		return (-1);
	}
	return (0);
}

auxv_t *
FetchAuxv()
{
	char procname[64];	/* /proc/<pid>/auxv */
	struct stat statbuf;
	static auxv_t *auxv = NULL;
	int fd;

	MakeProcName(procname, process_id);
	(void) strcat(procname, "/auxv");
	if ((fd = open(procname, O_RDONLY, 0)) < 0)
		return (NULL);
	if (fstat(fd, &statbuf) < 0)
		return (NULL);
	if (auxv)
		free(auxv);
	if ((auxv = (auxv_t *)malloc(statbuf.st_size)) == NULL)
		return (NULL);
	(void) read(fd, (char *)auxv, statbuf.st_size);
	(void) close(fd);
	return (auxv);
}

static int
lwp_wait(int pid, int *stat_loc)
{
	pstatus_t *ps = &Prstatus;
	long ctl[4];
	int sig;

startover:
	db_printf(2, "lwp_wait: waiting for lwp %D...", ps->pr_lwp.pr_lwpid);
	/* Wait for the lwp to stop. */
	ctl[0] = PCTWSTOP;
	ctl[1] = 2000;		/* millisecond timeout */
	if (write(lwpctlfd, (char *)&ctl[0], 2*sizeof (long)) < 0) {
		if (errno != EINTR) {
			if (errno != ENOENT)
				(void) printf(
				    "errno #%d waiting for LWP to stop", errno);
			return (-1);
		}
	}
	(void) pread(lwpstatusfd, (char *)&ps->pr_lwp, sizeof (ps->pr_lwp),
		(off_t)0);
	ps->pr_flags = ps->pr_lwp.pr_flags;
	if (!(ps->pr_flags & PR_ISTOP)) {
		/*
		 * We got the timeout.  This is *probably*
		 * because the lwp is blocked.
		 */
		db_printf(2, "lwp_wait: lwp %D is not stopped",
			ps->pr_lwp.pr_lwpid);
		ctl[0] = PCSTOP;
		if (write(lwpctlfd, (char *)&ctl[0], sizeof (long)) < 0 ||
		    pread(lwpstatusfd, (char *)&ps->pr_lwp, sizeof (ps->pr_lwp),
		    (off_t)0) < 0) {
			if (errno != EINTR)
				error("Unable to get pstatus for blocked lwp");
		}
		ps->pr_flags = ps->pr_lwp.pr_flags;
		ps->pr_lwp.pr_why = PR_SIGNALLED;
		ps->pr_lwp.pr_what = SIGTRAP;
		db_printf(2, "lwp_wait: pr_flags = 0x%X in blocked lwp",
			ps->pr_flags);
		if (ps->pr_flags & PR_ASLEEP) {
			printf("lwp %d is asleep in syscall %d\n",
				ps->pr_lwp.pr_lwpid, ps->pr_lwp.pr_syscall);
		} else {
			/* Hard to say why, but we need to say something. */
			printf(" lwp %d appears to be hanged\n",
				ps->pr_lwp.pr_lwpid);
		}
	}

	if (ps->pr_lwp.pr_why == PR_SIGNALLED) {
		sig = ps->pr_lwp.pr_what;
	}
	if (ps->pr_lwp.pr_why == PR_SYSENTRY &&
	    (ps->pr_lwp.pr_what == SYS_exec ||
	    ps->pr_lwp.pr_what == SYS_execve)) {
		ctl[0] = PCRUN;
		ctl[1] = 0;
		(void) write(lwpctlfd, (char *)&ctl[0], 2*sizeof (long));
		goto startover;
	}
	if (ps->pr_lwp.pr_why == PR_SYSEXIT &&
	    (ps->pr_lwp.pr_what == SYS_exec ||
	    ps->pr_lwp.pr_what == SYS_execve) &&
	    !(ps->pr_lwp.pr_reg[R_CCREG] & ERRBIT)) {
		sig = SIGTRAP;
	}
	/* simulate normal return from wait(2) */
	if (stat_loc != 0)
		*stat_loc = (sig << 8) | WSTOPFLG;

	/*
	 * This little weirdness is supposed to move the state of this lwp
	 * from PR_SIGNALLED to PR_REQUESTED.  Leaving the lwp in the former
	 * state confuses /proc when next we run the whole process.  BUT
	 * /proc gets even more confused when a run doesn't have a
	 * complimentary stop, so throw in one of those, too!
	 */
	ctl[0] = PCCSIG;
	ctl[1] = PCRUN;
	ctl[2] = PRSTOP;
	ctl[3] = PCWSTOP;
	(void) write(lwpctlfd, (char *)&ctl[0], 4*sizeof (long));

	CloseLwp();		/* can't afford to hang on to an fd */
	stopped = TRUE;
	setregs = FALSE;
	setfpregs = FALSE;
	return (0);
}

int
proc_wait(pid_t pid, int *stat_loc)
{
	pstatus_t *ps = &Prstatus;
	int fd;
	int sig;
	int status;

	/*
	 * lwpctlfd != -1 iff we're single stepping.  In that case, we
	 * are running the lwp and want to wait for that instead of
	 * for the process.
	 */
	if (lwpctlfd != -1 && lwp_wait(pid, stat_loc) == 0)
		return (pid);

	/*
	 * If this is the first time for this pid,
	 * open the /proc/pid file.  If all else fails,
	 * just give back whatever waitpid() has to offer.
	 */
	if ((process_id == 0 && FirstOpen(pid) < 0) ||
	    process_id != pid)
		return (waitpid(pid, stat_loc, WUNTRACED));

	db_printf(2, "proc_wait: waiting for process %D", pid);

	for (;;) {	/* loop on unsuccessful exec()s */
		long ctl[2];
		int had_eagain = 0;

		ctl[0] = PCWSTOP;
		while (write(ctlfd, (char *)&ctl[0], sizeof (long)) < 0 ||
		    pread(statusfd, (char *)ps, sizeof (*ps), (off_t)0) < 0) {
			if (errno == EINTR) {
				long stopit = PCDSTOP;
				(void) write(ctlfd, (char *)&stopit,
				    sizeof (long));
			} else if (errno == EAGAIN) {
				if (had_eagain == 0)
					had_eagain = 1;
				else {
					CloseProc();
					return (-1);
				}
				if (GrabProc(pid) < 0)
					return (-1);
			} else {
				CloseProc();
				/*
				 * If we are not the process's parent,
				 * waitpid() will fail.  Manufacture
				 * a normal exit code in this case.
				 */
				if (waitpid(pid, stat_loc, WUNTRACED) != pid &&
				    stat_loc != 0)
					*stat_loc = 0;
				return (pid);
			}
		}
		if (ps->pr_lwp.pr_why == PR_SIGNALLED) {
			sig = ps->pr_lwp.pr_what;
			if (sig != SIGCLD) break; /* ignore SIGCLD for child */
		}
		if (ps->pr_lwp.pr_why == PR_REQUESTED) {	/* ATTACH */
			sig = SIGTRAP;
			break;
		}
		if (ps->pr_lwp.pr_why == PR_SYSENTRY &&
		    (ps->pr_lwp.pr_what == SYS_exec ||
		    ps->pr_lwp.pr_what == SYS_execve)) {
			had_eagain = 0;
		} else if (ps->pr_lwp.pr_why == PR_SYSEXIT &&
		    (ps->pr_lwp.pr_what == SYS_exec ||
		    ps->pr_lwp.pr_what == SYS_execve) &&
		    !(ps->pr_lwp.pr_reg[R_CCREG] & ERRBIT)) {
			sig = SIGTRAP;
			break;
		}
		ctl[0] = PCRUN;
		ctl[1] = 0;
		(void) write(ctlfd, (char *)ctl, 2*sizeof (long));
	}

	stopped = TRUE;
	setregs = FALSE;
	setfpregs = FALSE;

	/* simulate normal return from wait(2) */
	status = (sig << 8) | WSTOPFLG;
	db_printf(2, "proc_wait: returning status 0x%04x for process %D",
		status, pid);
	if (stat_loc != 0)
		*stat_loc = status;
	return (pid);
}

void
enumerate_lwps(pid_t pid)
{
	DIR *dirp;
	struct dirent *dentp;
	char lwpname[64];	/* /proc/<pid>/lwp */
	int i;

	/* Special-case the mundane since it's so much simpler. */
	if (Prstatus.pr_nlwp == 1) {
		printf("lwpid %d is the only lwp in process %d.\n",
			Prstatus.pr_lwp.pr_lwpid, pid);
		return;
	}

	MakeProcName(lwpname, pid);
	(void) strcat(lwpname, "/lwp");

	if ((dirp = opendir(lwpname)) == NULL) {
		/* error message */
		return;
	}
	printf("lwpids ");
	i = 0;
	while ((dentp = readdir(dirp)) != NULL) {
		if (dentp->d_name[0] == '.')
			continue;
		if (i == 0)
			printf("%s", dentp->d_name);
		else if (i != Prstatus.pr_nlwp - 1)
			printf(", %s", dentp->d_name);
		else
			printf(" and %s", dentp->d_name);
		i++;
	}
	printf(" are in process %d\n", pid);
	(void) closedir(dirp);
}

void
set_lwp(id_t lwpid, pid_t pid)
{
	int lfd;
	int i;
	long ctl[1];
	char lwpname[64];	/* /proc/<pid>/lwp/<lwpid>lwpstatus */
	pstatus_t *ps = &Prstatus;
	extern addr_t usernpc;

	if (lwpid == (int)ps->pr_lwp.pr_lwpid)
		return;		/* We've all ready got this one. */

	/* Stop the process.  Too much can go wrong otherwise. */
	ctl[0] = PCSTOP;
	if (write(ctlfd, (char *)ctl, sizeof (long)) < 0)
		error("Unable to stop process");

	MakeLwpName(lwpname, pid, lwpid);
	(void) strcat(lwpname, "/lwpstatus");
	if ((lfd = open(lwpname, O_RDONLY)) < 0) {
		printf("lwpid %d was not found in proc %d\n", lwpid, pid);
		return;
	}

	i = read(lfd, (char *)&ps->pr_lwp, sizeof (ps->pr_lwp));
	ps->pr_flags = ps->pr_lwp.pr_flags;
	(void) close(lfd);
	if (i == -1)
		error("Can't get lwp's status");

	/* Pick up this lwp's registers. */
	(void) core_to_regs();
	userpc = (addr_t)readreg(Reg_PC);
#if sparc
	usernpc = (addr_t)readreg(Reg_NPC);
#endif
	printf("lwp %d: ", ps->pr_lwp.pr_lwpid);
	(void) print_dis(Reg_PC, 0);
}
