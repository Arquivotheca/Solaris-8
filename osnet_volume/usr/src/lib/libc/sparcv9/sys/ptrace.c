/*
 * ptrace(2) interface built on top of proc(4).
 */

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#ident	"@(#)ptrace.c	1.7	99/06/10 SMI"

#ifdef __STDC__
#pragma weak ptrace = _ptrace
#endif

#include "synonyms.h"
#include "mtlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/signal.h>
#include <sys/siginfo.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <procfs.h>
#include <sys/stack.h>
#include <sys/psw.h>
#include <sys/user.h>
#include <thread.h>
#include <synch.h>
#include "libc.h"

static mutex_t pt_lock = DEFAULTMUTEX;

#undef TRUE
#define	TRUE	1
#undef FALSE
#define	FALSE	0

/*
 * All my children...
 */
typedef struct cstatus {
	struct cstatus	*next;		/* linked list			*/
	pid_t		pid;		/* process-id			*/
	int		asfd;		/* /proc/<pid>/as		*/
	int		ctlfd;		/* /proc/<pid>/ctl		*/
	int		statusfd;	/* /proc/<pid>/status		*/
	int		flags;		/* see below			*/
	pstatus_t	pstatus;	/* from /proc/<pid>/status	*/
	user_t		user;		/* manufactured u-block		*/
} cstatus_t;

/* flags */
#define	CS_SETREGS	0x01		/* set registers on run		*/
#define	CS_PSARGS	0x02		/* u_psargs[] has been fetched	*/
#define	CS_SIGNAL	0x04		/* u_signal[] has been fetched	*/

#define	NULLCP	((cstatus_t *)0)

static cstatus_t *childp = NULLCP;

/* fake u-block offsets */
#define	UP		((user_t *)NULL)
#define	U_REG		((long)(&UP->u_reg[0]))
#define	U_AR0		((long)(&UP->u_ar0))
#define	U_PSARGS	((long)(&UP->u_psargs[0]))
#define	U_SIGNAL	((long)(&UP->u_signal[0]))
#define	U_CODE		((long)(&UP->u_code))
#define	U_ADDR		((long)(&UP->u_addr))
#define	U_END		((long)sizeof (user_t))
/* arbitrary kernel address for u_ar0 */
#define	REGADDR		(u_long)0xffffffffffff0000

/* external routines defined in this module */
extern	long	ptrace(int, pid_t, long, long);
/* static routines defined in this module */
static	cstatus_t *FindProc(pid_t);
static	void	CheckAllProcs(void);
static	int	Dupfd(int, int);
static	void	MakeProcName(char *, pid_t);
static	int	OpenProc(cstatus_t *);
static	void	CloseProc(cstatus_t *);
static	cstatus_t *GrabProc(pid_t);
static	void	ReleaseProc(cstatus_t *);
static	int	ProcUpdate(cstatus_t *);
static	void	MakeUser(cstatus_t *);
static	void	GetPsargs(cstatus_t *);
static	void	GetSignal(cstatus_t *);

#ifdef PTRACE_DEBUG
/* for debugging */
static char *
map(int request)
{
	static char name[20];

	switch (request) {
	case 0:	return ("PTRACE_TRACEME");
	case 1:	return ("PTRACE_PEEKTEXT");
	case 2:	return ("PTRACE_PEEKDATA");
	case 3:	return ("PTRACE_PEEKUSER");
	case 4:	return ("PTRACE_POKETEXT");
	case 5:	return ("PTRACE_POKEDATA");
	case 6:	return ("PTRACE_POKEUSER");
	case 7:	return ("PTRACE_CONT");
	case 8:	return ("PTRACE_KILL");
	case 9:	return ("PTRACE_SINGLESTEP");
	}
	(void) sprintf(name, "%d", request);
	return (name);
}
#endif

long
ptrace(int request, pid_t pid, long addr, long data)
{
	pstatus_t *ps;
	cstatus_t *cp;
	int i;
	u_long xaddr;
	u_long stack;	/* %sp + STACK_BIAS */
	struct {
		long cmd;
		union {
			long flags;
			sigset_t signals;
			fltset_t faults;
			sysset_t syscalls;
			siginfo_t siginfo;
		} arg;
	} ctl;

#ifdef PTRACE_DEBUG
	fprintf(stderr, " ptrace(%s, %d, 0x%lX, 0x%lX)\n",
		map(request), pid, addr, data);
#endif

	mutex_lock(&pt_lock);

	if (request == 0) {	/* PTRACE_TRACEME, executed by traced process */
		/*
		 * Set stop-on-all-signals and nothing else.
		 * Turn off inherit-on-fork flag (grandchildren run away).
		 * Set ptrace-compatible flag.
		 */
		char procname[64];	/* /proc/<pid>/ctl */
		int fd;

		MakeProcName(procname, getpid());
		(void) strcat(procname, "/ctl");
		if ((fd = open(procname, O_WRONLY, 0)) < 0)
			exit(255);
		ctl.cmd = PCSTRACE;
		prfillset(&ctl.arg.signals);
		if (write(fd, (char *)&ctl, sizeof (long)+sizeof (sigset_t))
		    != sizeof (long)+sizeof (sigset_t))
			exit(255);
		ctl.cmd = PCSFAULT;
		premptyset(&ctl.arg.faults);
		if (write(fd, (char *)&ctl, sizeof (long)+sizeof (fltset_t))
		    != sizeof (long)+sizeof (fltset_t))
			exit(255);
		ctl.cmd = PCSENTRY;
		premptyset(&ctl.arg.syscalls);
		if (write(fd, (char *)&ctl, sizeof (long)+sizeof (sysset_t))
		    != sizeof (long)+sizeof (sysset_t))
			exit(255);
		ctl.cmd = PCSEXIT;
		premptyset(&ctl.arg.syscalls);
		if (write(fd, (char *)&ctl, sizeof (long)+sizeof (sysset_t))
		    != sizeof (long)+sizeof (sysset_t))
			exit(255);
		ctl.cmd = PCUNSET;
		ctl.arg.flags = PR_FORK;
		if (write(fd, (char *)&ctl, sizeof (long)+sizeof (long))
		    != sizeof (long)+sizeof (long))
			exit(255);
		ctl.cmd = PCSET;
		ctl.arg.flags = PR_PTRACE;
		if (write(fd, (char *)&ctl, sizeof (long)+sizeof (long))
		    != sizeof (long)+sizeof (long))
			exit(255);
		if (close(fd) != 0)
			exit(255);

		mutex_unlock(&pt_lock);
		return (0);
	}

again:
	errno = 0;

	/* find the cstatus structure corresponding to pid */
	if ((cp = GrabProc(pid)) == NULLCP)
		goto esrch;

	ps = &cp->pstatus;
	if (!(ps->pr_flags & PR_ISTOP)) {
		if (ProcUpdate(cp) != 0) {
			ReleaseProc(cp);
			goto esrch;
		}
		if (!(ps->pr_flags & PR_ISTOP))
			goto esrch;
	}

	/*
	 * Process the request.
	 */
	errno = 0;
	switch (request) {
	case 1:		/* PTRACE_PEEKTEXT */
	case 2:		/* PTRACE_PEEKDATA */
		if (addr & 07)
			goto eio;
		if (pread(cp->asfd, (char *)&data, sizeof (data), (off_t)addr)
		    == sizeof (data))
			goto out;
		goto eio;

	case 3:		/* PTRACE_PEEKUSER */
		if (addr & 07)
			goto eio;
		xaddr = addr;
		if (xaddr >= REGADDR && xaddr < REGADDR+sizeof (gregset_t))
			xaddr -= REGADDR-U_REG;
		if (xaddr >= U_PSARGS && xaddr < U_PSARGS+sizeof (UP->u_psargs))
			GetPsargs(cp);
		if (xaddr >= U_SIGNAL && xaddr < U_SIGNAL+sizeof (UP->u_signal))
			GetSignal(cp);
		if ((long)xaddr >= 0 && xaddr < U_END) {
			data = *((long *)((caddr_t)(&cp->user) + xaddr));
			goto out;
		}
		goto eio;

	case 4:		/* PTRACE_POKETEXT */
	case 5:		/* PTRACE_POKEDATA */
		if (addr & 07)
			goto eio;
		xaddr = addr;

		stack = (u_long)cp->user.u_reg[REG_SP];
		if (IS_V9STACK(stack)) {
			stack += STACK_BIAS;
			if (xaddr >= stack && xaddr < stack+16*sizeof (long))
				cp->flags |= CS_SETREGS;
		} else {
			if (xaddr >= stack && xaddr < stack+16*sizeof (int32_t))
				cp->flags |= CS_SETREGS;
		}

		if (pwrite(cp->asfd, (char *)&data, sizeof (data), (off_t)addr)
		    == sizeof (data))
			goto out;
		goto eio;

	case 6:		/* PTRACE_POKEUSER */
		if (addr & 07)
			goto eio;
		xaddr = addr;
		if (xaddr >= REGADDR && xaddr < REGADDR+sizeof (gregset_t))
			xaddr -= REGADDR-U_REG;
		if ((long)xaddr >= U_REG && xaddr < U_REG+sizeof (gregset_t)) {
			long rx = (xaddr-U_REG)/sizeof (greg_t);
			if (rx == REG_CCR)
				data = (cp->user.u_reg[REG_CCR] &
				    ~0xFF) | (data & 0xFF);
			cp->user.u_reg[rx] = data;
			cp->flags |= CS_SETREGS;
			goto out;
		}
		goto eio;

	case 7:		/* PTRACE_CONT */
	case 9:		/* PTRACE_SINGLESTEP */
	    {
		long runctl[3];

		if (cp->flags & CS_SETREGS) {
			long cmd;
			iovec_t iov[2];

			ps->pr_lwp.pr_reg[R_CCR] = cp->user.u_reg[REG_CCR];
			ps->pr_lwp.pr_reg[R_PC]  = cp->user.u_reg[REG_PC];
			ps->pr_lwp.pr_reg[R_nPC] = cp->user.u_reg[REG_nPC];
			ps->pr_lwp.pr_reg[R_Y]   = cp->user.u_reg[REG_Y];
			ps->pr_lwp.pr_reg[R_G1]  = cp->user.u_reg[REG_G1];
			ps->pr_lwp.pr_reg[R_G2]  = cp->user.u_reg[REG_G2];
			ps->pr_lwp.pr_reg[R_G3]  = cp->user.u_reg[REG_G3];
			ps->pr_lwp.pr_reg[R_G4]  = cp->user.u_reg[REG_G4];
			ps->pr_lwp.pr_reg[R_G5]  = cp->user.u_reg[REG_G5];
			ps->pr_lwp.pr_reg[R_G6]  = cp->user.u_reg[REG_G6];
			ps->pr_lwp.pr_reg[R_G7]  = cp->user.u_reg[REG_G7];
			ps->pr_lwp.pr_reg[R_O0]  = cp->user.u_reg[REG_O0];
			ps->pr_lwp.pr_reg[R_O1]  = cp->user.u_reg[REG_O1];
			ps->pr_lwp.pr_reg[R_O2]  = cp->user.u_reg[REG_O2];
			ps->pr_lwp.pr_reg[R_O3]  = cp->user.u_reg[REG_O3];
			ps->pr_lwp.pr_reg[R_O4]  = cp->user.u_reg[REG_O4];
			ps->pr_lwp.pr_reg[R_O5]  = cp->user.u_reg[REG_O5];
			ps->pr_lwp.pr_reg[R_O6]  = cp->user.u_reg[REG_O6];
			ps->pr_lwp.pr_reg[R_O7]  = cp->user.u_reg[REG_O7];
			ps->pr_lwp.pr_reg[R_ASI]  = cp->user.u_reg[REG_ASI];
			ps->pr_lwp.pr_reg[R_FPRS]  = cp->user.u_reg[REG_FPRS];

			stack = cp->user.u_reg[REG_SP];
			if (IS_V9STACK(stack)) {
				stack += STACK_BIAS;
				(void) pread(cp->asfd,
				    (char *)&ps->pr_lwp.pr_reg[R_L0],
				    16*sizeof (long), (off_t)stack);
			} else {
				uint32_t regs[16];
				int i;
				(void) pread(cp->asfd, (char *)regs,
				    16*sizeof (uint32_t), (off_t)stack);
				for (i = 0; i < 16; i++)
					ps->pr_lwp.pr_reg[R_L0+i] = regs[i];
			}

			cmd = PCSREG;
			iov[0].iov_base = (caddr_t)&cmd;
			iov[0].iov_len = sizeof (long);
			iov[1].iov_base = (caddr_t)&ps->pr_lwp.pr_reg[0];
			iov[1].iov_len = sizeof (ps->pr_lwp.pr_reg);
			if (writev(cp->ctlfd, iov, 2) < 0)
				goto tryagain;
		}
		if (addr != 1 &&	/* new virtual address */
		    (addr & ~03) != cp->user.u_reg[REG_PC]) {
			runctl[0] = PCSVADDR;
			runctl[1] = (addr & ~03);
			if (write(cp->ctlfd, (char *)runctl, 2*sizeof (long))
			    != 2*sizeof (long))
				goto tryagain;
		}
		/* make data the current signal */
		if (data != 0 && data != ps->pr_lwp.pr_cursig) {
			(void) memset((char *)&ctl.arg.siginfo, 0,
			    sizeof (siginfo_t));
			ctl.arg.siginfo.si_signo = data;
			ctl.cmd = PCSSIG;
			if (write(cp->ctlfd, (char *)&ctl,
			    sizeof (long)+sizeof (siginfo_t))
			    != sizeof (long)+sizeof (siginfo_t))
				goto tryagain;
		}
		if (data == 0)
			runctl[0] = PCCSIG;
		else
			runctl[0] = PCNULL;
		runctl[1] = PCRUN;
		runctl[2] = (request == 9)? PRSTEP : 0;
		if (write(cp->ctlfd, (char *)runctl, 3*sizeof (long))
		    != 3*sizeof (long)) {
			if (errno == ENOENT) {
				/* current signal must have killed it */
				ReleaseProc(cp);
				goto out;
			}
			goto tryagain;
		}
		(void) memset((char *)ps, 0, sizeof (pstatus_t));
		cp->flags = 0;
		goto out;
	    }

	case 8:		/* PTRACE_KILL */
		/* overkill? */
		(void) memset((char *)&ctl.arg.siginfo, 0, sizeof (siginfo_t));
		ctl.arg.siginfo.si_signo = SIGKILL;
		ctl.cmd = PCSSIG;
		(void) write(cp->ctlfd, (char *)&ctl,
		    sizeof (long)+sizeof (siginfo_t));
		(void) kill(pid, SIGKILL);
		ReleaseProc(cp);
		data = 0;
		goto out;

	default:
		goto eio;
	}

tryagain:
	if (errno == EAGAIN) {
		if (OpenProc(cp) == 0)
			goto again;
		ReleaseProc(cp);
	}
eio:
	errno = EIO;
	data = -1;
	goto out;
esrch:
	errno = ESRCH;
	data = -1;
out:
	mutex_unlock(&pt_lock);
	return (data);
}

/*
 * Find the cstatus structure corresponding to pid.
 */
static cstatus_t *
FindProc(pid_t pid)
{
	cstatus_t *cp;

	for (cp = childp; cp != NULLCP; cp = cp->next)
		if (cp->pid == pid)
			break;

	return (cp);
}

/*
 * Check every proc for existence, release those that are gone.
 * Be careful about the linked list; ReleaseProc() changes it.
 */
static void
CheckAllProcs()
{
	cstatus_t *cp = childp;

	while (cp != NULLCP) {
		cstatus_t *next = cp->next;

		if (ProcUpdate(cp) != 0)
			ReleaseProc(cp);
		cp = next;
	}
}

/*
 * Utility for OpenProc().
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
 * Open/reopen the /proc/<pid> files.
 */
static int
OpenProc(cstatus_t *cp)
{
	char procname[64];		/* /proc/nnnnn/fname */
	char *fname;
	int fd;
	int omode;

	MakeProcName(procname, cp->pid);
	fname = procname + strlen(procname);

	/*
	 * Use exclusive-open only if this is the first open.
	 */
	omode = (cp->asfd > 0)? O_RDWR : (O_RDWR|O_EXCL);
	(void) strcpy(fname, "/as");
	if ((fd = open(procname, omode, 0)) < 0 ||
	    (cp->asfd = Dupfd(fd, cp->asfd)) < 0)
		goto err;

	(void) strcpy(fname, "/ctl");
	if ((fd = open(procname, O_WRONLY, 0)) < 0 ||
	    (cp->ctlfd = Dupfd(fd, cp->ctlfd)) < 0)
		goto err;

	(void) strcpy(fname, "/status");
	if ((fd = open(procname, O_RDONLY, 0)) < 0 ||
	    (cp->statusfd = Dupfd(fd, cp->statusfd)) < 0)
		goto err;

	return (0);

err:
	CloseProc(cp);
	return (-1);
}

/*
 * Close the /proc/<pid> files.
 */
static void
CloseProc(cstatus_t *cp)
{
	if (cp->asfd > 0)
		(void) close(cp->asfd);
	if (cp->ctlfd > 0)
		(void) close(cp->ctlfd);
	if (cp->statusfd > 0)
		(void) close(cp->statusfd);
	cp->asfd = 0;
	cp->ctlfd = 0;
	cp->statusfd = 0;
}

/*
 * Take control of a child process.
 */
static cstatus_t *
GrabProc(pid_t pid)
{
	cstatus_t *cp;
	long ctl[2];
	pid_t ppid;
	pstatus_t pstatus;

	if (pid <= 0)
		return (NULLCP);

	if ((cp = FindProc(pid)) != NULLCP)	/* already grabbed */
		return (cp);

	CheckAllProcs();	/* clean up before grabbing new process */

	cp = (cstatus_t *)malloc(sizeof (cstatus_t));
	if (cp == NULLCP)
		return (NULLCP);
	(void) memset((char *)cp, 0, sizeof (cstatus_t));
	cp->pid = pid;

	ppid = getpid();
	while (OpenProc(cp) == 0) {
		ctl[0] = PCSET;
		ctl[1] = PR_RLC;
		errno = 0;

		if (pread(cp->statusfd, (char *)&cp->pstatus,
		    sizeof (cp->pstatus), (off_t)0) == sizeof (cp->pstatus) &&
		    cp->pstatus.pr_ppid == ppid &&
		    (cp->pstatus.pr_flags & PR_PTRACE) &&
		    write(cp->ctlfd, (char *)ctl, 2*sizeof (long))
		    == 2*sizeof (long)) {
			cp->next = childp;
			childp = cp;
			MakeUser(cp);
			return (cp);
		}

		if (errno != EAGAIN)
			break;
	}

	free((char *)cp);
	return (NULLCP);
}

/*
 * Close the /proc/<pid> file, if open.
 * Deallocate the memory used by the cstatus_t structure.
 */
static void
ReleaseProc(cstatus_t *cp)
{
	CloseProc(cp);

	if (childp == cp)
		childp = cp->next;
	else {
		cstatus_t *pcp;

		for (pcp = childp; pcp != NULLCP; pcp = pcp->next) {
			if (pcp->next == cp) {
				pcp->next = cp->next;
				break;
			}
		}
	}

	free((char *)cp);
}

/*
 * Update process information from /proc.
 * Return 0 on success, -1 on failure.
 */
static int
ProcUpdate(cstatus_t *cp)
{
	pstatus_t *ps = &cp->pstatus;
	u_long stack;

	if (cp->flags & CS_SETREGS) {
		long cmd;
		iovec_t iov[2];

		ps->pr_lwp.pr_reg[R_CCR] = cp->user.u_reg[REG_CCR];
		ps->pr_lwp.pr_reg[R_PC]  = cp->user.u_reg[REG_PC];
		ps->pr_lwp.pr_reg[R_nPC] = cp->user.u_reg[REG_nPC];
		ps->pr_lwp.pr_reg[R_Y]   = cp->user.u_reg[REG_Y];
		ps->pr_lwp.pr_reg[R_G1]  = cp->user.u_reg[REG_G1];
		ps->pr_lwp.pr_reg[R_G2]  = cp->user.u_reg[REG_G2];
		ps->pr_lwp.pr_reg[R_G3]  = cp->user.u_reg[REG_G3];
		ps->pr_lwp.pr_reg[R_G4]  = cp->user.u_reg[REG_G4];
		ps->pr_lwp.pr_reg[R_G5]  = cp->user.u_reg[REG_G5];
		ps->pr_lwp.pr_reg[R_G6]  = cp->user.u_reg[REG_G6];
		ps->pr_lwp.pr_reg[R_G7]  = cp->user.u_reg[REG_G7];
		ps->pr_lwp.pr_reg[R_O0]  = cp->user.u_reg[REG_O0];
		ps->pr_lwp.pr_reg[R_O1]  = cp->user.u_reg[REG_O1];
		ps->pr_lwp.pr_reg[R_O2]  = cp->user.u_reg[REG_O2];
		ps->pr_lwp.pr_reg[R_O3]  = cp->user.u_reg[REG_O3];
		ps->pr_lwp.pr_reg[R_O4]  = cp->user.u_reg[REG_O4];
		ps->pr_lwp.pr_reg[R_O5]  = cp->user.u_reg[REG_O5];
		ps->pr_lwp.pr_reg[R_O6]  = cp->user.u_reg[REG_O6];
		ps->pr_lwp.pr_reg[R_O7]  = cp->user.u_reg[REG_O7];
		ps->pr_lwp.pr_reg[R_ASI] = cp->user.u_reg[REG_ASI];
		ps->pr_lwp.pr_reg[R_FPRS] = cp->user.u_reg[REG_FPRS];

		stack = cp->user.u_reg[REG_SP];
		if (IS_V9STACK(stack)) {
			stack += STACK_BIAS;
			(void) pread(cp->asfd, (char *)&ps->pr_lwp.pr_reg[R_L0],
				16*sizeof (long), (off_t)stack);
		} else {
			uint32_t regs[16];
			int i;
			(void) pread(cp->asfd, (char *)regs,
				16*sizeof (uint32_t), (off_t)stack);
			for (i = 0; i < 16; i++)
				ps->pr_lwp.pr_reg[R_L0+i] = regs[i];
		}

		cmd = PCSREG;
		iov[0].iov_base = (caddr_t)&cmd;
		iov[0].iov_len = sizeof (long);
		iov[1].iov_base = (caddr_t)&ps->pr_lwp.pr_reg[0];
		iov[1].iov_len = sizeof (ps->pr_lwp.pr_reg);
		(void) writev(cp->ctlfd, iov, 2);
		cp->flags &= ~CS_SETREGS;
	}

	while (pread(cp->statusfd, (char *)ps, sizeof (*ps), (off_t)0) < 0) {
		/* attempt to regain control */
		if (errno != EINTR &&
		    !(errno == EAGAIN && OpenProc(cp) == 0))
			return (-1);
	}

	if (ps->pr_flags & PR_ISTOP)
		MakeUser(cp);
	else
		(void) memset((char *)ps, 0, sizeof (pstatus_t));

	return (0);
}

/*
 * Manufacture the contents of the fake u-block.
 */
static void
MakeUser(cstatus_t *cp)
{
	pstatus_t *ps = &cp->pstatus;

	cp->user.u_reg[REG_CCR] = ps->pr_lwp.pr_reg[R_CCR];
	cp->user.u_reg[REG_PC]  = ps->pr_lwp.pr_reg[R_PC];
	cp->user.u_reg[REG_nPC] = ps->pr_lwp.pr_reg[R_nPC];
	cp->user.u_reg[REG_Y]   = ps->pr_lwp.pr_reg[R_Y];
	cp->user.u_reg[REG_G1]  = ps->pr_lwp.pr_reg[R_G1];
	cp->user.u_reg[REG_G2]  = ps->pr_lwp.pr_reg[R_G2];
	cp->user.u_reg[REG_G3]  = ps->pr_lwp.pr_reg[R_G3];
	cp->user.u_reg[REG_G4]  = ps->pr_lwp.pr_reg[R_G4];
	cp->user.u_reg[REG_G5]  = ps->pr_lwp.pr_reg[R_G5];
	cp->user.u_reg[REG_G6]  = ps->pr_lwp.pr_reg[R_G6];
	cp->user.u_reg[REG_G7]  = ps->pr_lwp.pr_reg[R_G7];
	cp->user.u_reg[REG_O0]  = ps->pr_lwp.pr_reg[R_O0];
	cp->user.u_reg[REG_O1]  = ps->pr_lwp.pr_reg[R_O1];
	cp->user.u_reg[REG_O2]  = ps->pr_lwp.pr_reg[R_O2];
	cp->user.u_reg[REG_O3]  = ps->pr_lwp.pr_reg[R_O3];
	cp->user.u_reg[REG_O4]  = ps->pr_lwp.pr_reg[R_O4];
	cp->user.u_reg[REG_O5]  = ps->pr_lwp.pr_reg[R_O5];
	cp->user.u_reg[REG_O6]  = ps->pr_lwp.pr_reg[R_O6];
	cp->user.u_reg[REG_O7]  = ps->pr_lwp.pr_reg[R_O7];
	cp->user.u_reg[REG_ASI]  = ps->pr_lwp.pr_reg[R_ASI];
	cp->user.u_reg[REG_FPRS]  = ps->pr_lwp.pr_reg[R_FPRS];
	cp->user.u_ar0 = (greg_t *)REGADDR;
	cp->user.u_code = ps->pr_lwp.pr_info.si_code;
	cp->user.u_addr = ps->pr_lwp.pr_info.si_addr;
	cp->flags &= ~(CS_PSARGS|CS_SIGNAL);
}

/*
 * Fetch the contents of u_psargs[].
 */
static void
GetPsargs(cstatus_t *cp)
{
	char procname[64];	/* /proc/<pid>/psinfo */
	int fd;

	MakeProcName(procname, cp->pid);
	(void) strcat(procname, "/psinfo");
	if ((fd = open(procname, O_RDONLY, 0)) < 0) {
		(void) memset(cp->user.u_psargs, 0, PSARGSZ);
		return;
	}
	(void) pread(fd, cp->user.u_psargs, PSARGSZ,
	    (off_t)((psinfo_t *)0)->pr_psargs);
	(void) close(fd);

	cp->flags |= CS_PSARGS;
}

/*
 * Fetch the contents of u_signal[].
 */
static void
GetSignal(cstatus_t *cp)
{
	char procname[64];	/* /proc/<pid>/sigact */
	int fd;
	struct sigaction action[MAXSIG];
	int i;

	MakeProcName(procname, cp->pid);
	(void) strcat(procname, "/sigact");
	(void) memset((char *)action, 0, sizeof (action));
	if ((fd = open(procname, O_RDONLY, 0)) >= 0) {
		(void) read(fd, (char *)action, sizeof (action));
		(void) close(fd);
	}
	for (i = 0; i < MAXSIG; i++)
		cp->user.u_signal[i] = action[i].sa_handler;
	cp->flags |= CS_SIGNAL;
}
