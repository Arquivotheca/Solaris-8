/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)actions.c	1.38	99/05/04 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stack.h>
#include <signal.h>
#include <sys/isa_defs.h>
#include <libproc.h>
#include "ramdata.h"
#include "systable.h"
#include "print.h"
#include "proto.h"

/*
 * Actions to take when process stops.
 */

/*
 * Function prototypes for static routines in this module.
 */
static	int	stopsig(struct ps_prochandle *);
static	void	showpaths(const struct systable *);
static	void	showargs(struct ps_prochandle *, int);
static	void	dumpargs(struct ps_prochandle *, long, const char *);
static	int	getsubcode(struct ps_prochandle *);

/* determine the subcode for this syscall, if any */
static int
getsubcode(struct ps_prochandle *Pr)
{
	const pstatus_t *Psp = Pstatus(Pr);
	int syscall = Psp->pr_lwp.pr_syscall;
	int nsysarg = Psp->pr_lwp.pr_nsysarg;
	int subcode = -1;

	if (syscall > 0 && nsysarg > 0) {
		subcode = Psp->pr_lwp.pr_sysarg[0];
		switch (syscall) {
		case SYS_utssys:
			if (nsysarg > 2)
				subcode = Psp->pr_lwp.pr_sysarg[2];
			break;
		case SYS_open:
		case SYS_open64:
			if (nsysarg > 1)
				subcode = Psp->pr_lwp.pr_sysarg[1];
			break;
		case SYS_door:
			if (nsysarg > 5)
				subcode = Psp->pr_lwp.pr_sysarg[5];
			break;
		case SYS_lwp_create:
			subcode =	/* 0 for parent, 1 for child */
				(Psp->pr_lwp.pr_why == PR_SYSEXIT &&
				    Psp->pr_lwp.pr_errno == 0 &&
				    Psp->pr_lwp.pr_rval1 == 0);
		}
	}
	return (subcode);
}

/*
 * Report an lwp to be sleeping (if true).
 */
static int
report_sleeping(struct ps_prochandle *Pr)
{
	/*
	 * Here we cast away the const-ness of Pstatus so we can cheat below.
	 */
	pstatus_t *Psp = (pstatus_t *)Pstatus(Pr);
	int sys = Psp->pr_lwp.pr_syscall;

	if (!prismember(&trace, sys) ||
	    !(Psp->pr_lwp.pr_flags & (PR_ASLEEP|PR_VFORKP))) {
		/* Make sure we catch sysexit even if we're not tracing it. */
		(void) Psysexit(Pr, sys, TRUE);
		return (0);
	}

	length = 0;
	Psp->pr_lwp.pr_what = sys;	/* cheating a little */
	Errno = 0;
	Rval1 = Rval2 = 0;
	(void) sysentry(Pr);
	if (lflag || Dynpat != NULL)
		make_pname(Pr, 0);
	putpname();
	timestamp(Psp);
	length += printf("%s", sys_string);
	sys_leng = 0;
	*sys_string = '\0';
	length >>= 3;
	if (Psp->pr_lwp.pr_flags & PR_VFORKP)
		length += 2;
	if (length >= 4)
		(void) fputc(' ', stdout);
	for (; length < 4; length++)
		(void) fputc('\t', stdout);
	if (Psp->pr_lwp.pr_flags & PR_VFORKP)
		(void) fputs("(waiting for child to exit()/exec()...)\n",
			stdout);
	else
		(void) fputs("(sleeping...)\n", stdout);
	length = 0;
	if (prismember(&verbose, sys)) {
		int raw = prismember(&rawout, sys);
		Errno = 1;
		expound(Pr, 0, raw);
		Errno = 0;
	}
	return (1);
}

/*
 * Report all the lwps in the process to be sleeping (if true).
 */
static void
report_all_sleeping(struct ps_prochandle *Pr)
{
	/*
	 * Here we cast away the const-ness of Pstatus so we can cheat below.
	 */
	pstatus_t *Psp = (pstatus_t *)Pstatus(Pr);
	prheader_t *Lhp;
	lwpstatus_t *Lsp;
	lwpstatus_t save;
	char lstatus[64];
	struct stat statb;
	int fd;
	long nlwp;
	int reportable;

	if (Psp->pr_nlwp <= 1 || (Psp->pr_lwp.pr_flags & PR_VFORKP)) {
		/* only one lwp to report */
		if (report_sleeping(Pr))
			Flush();
		return;
	}

	(void) sprintf(lstatus, "/proc/%d/lstatus", (int)Psp->pr_pid);
	if ((fd = open(lstatus, O_RDONLY)) < 0 ||
	    fstat(fd, &statb) != 0) {
		if (fd >= 0)
			(void) close(fd);
		return;
	}

	Lhp = malloc(statb.st_size);
	if (read(fd, Lhp, statb.st_size) <
	    sizeof (prheader_t) + sizeof (lwpstatus_t)) {
		(void) close(fd);
		free(Lhp);
		return;
	}
	(void) close(fd);

	/*
	 * First make sure every lwp is sleeping and some are reportable.
	 */
	reportable = 0;
	/* LINTED improper alignment */
	for (nlwp = Lhp->pr_nent, Lsp = (lwpstatus_t *)(Lhp + 1); nlwp > 0;
	    /* LINTED improper alignment */
	    nlwp--, Lsp = (lwpstatus_t *)((char *)Lsp + Lhp->pr_entsize)) {
		int sys = Lsp->pr_syscall;

		if (!(Lsp->pr_flags & PR_ASLEEP)) {
			reportable = 0;
			break;
		}
		if (prismember(&trace, sys))
			reportable++;
	}
	if (reportable == 0) {
		free(Lhp);
		return;
	}

	/*
	 * They are all sleeping and some are reportable.
	 */
	Eserialize();	/* enter region of lengthy output */
	save = Psp->pr_lwp;
	/* LINTED improper alignment */
	for (nlwp = Lhp->pr_nent, Lsp = (lwpstatus_t *)(Lhp + 1); nlwp > 0;
	    /* LINTED improper alignment */
	    nlwp--, Lsp = (lwpstatus_t *)((char *)Lsp + Lhp->pr_entsize)) {
		Psp->pr_lwp = *Lsp;		/* cheating a lot */
		(void) report_sleeping(Pr);
	}
	Psp->pr_lwp = save;
	Xserialize();
	free(Lhp);
}

/*
 * requested() gets called for these reasons:
 *	flag == JOBSIG:		report nothing; change state to JOBSTOP
 *	flag == JOBSTOP:	report "Continued ..."
 *	default:		report sleeping system call
 *
 * It returns a new flag:  JOBSTOP or SLEEPING or 0.
 */
int
requested(struct ps_prochandle *Pr, int flag)
{
	const pstatus_t *Psp = Pstatus(Pr);
	int sig = Psp->pr_lwp.pr_cursig;
	int newflag = 0;

	switch (flag) {
	case JOBSIG:
		return (JOBSTOP);

	case JOBSTOP:
		if (!cflag && prismember(&signals, sig)) {
			length = 0;
			putpname();
			timestamp(Psp);
			(void) printf("    Continued with signal #%d, %s",
				sig, signame(sig));
			if (Psp->pr_lwp.pr_action.sa_handler == SIG_DFL)
				(void) printf(" [default]");
			else if (Psp->pr_lwp.pr_action.sa_handler == SIG_IGN)
				(void) printf(" [ignored]");
			else
				(void) printf(" [caught]");
			(void) fputc('\n', stdout);
			Flush();
		}
		newflag = 0;
		break;

	default:
		newflag = SLEEPING;
		if (!cflag)
			report_all_sleeping(Pr);
		break;
	}

	return (newflag);
}

int
jobcontrol(struct ps_prochandle *Pr)
{
	const pstatus_t *Psp = Pstatus(Pr);
	int sig = stopsig(Pr);

	if (sig == 0)
		return (0);

	if (!cflag &&				/* not just counting */
	    prismember(&signals, sig)) {	/* tracing this signal */
		int sys;

		length = 0;
		putpname();
		timestamp(Psp);
		(void) printf("    Stopped by signal #%d, %s",
			sig, signame(sig));
		if ((Psp->pr_lwp.pr_flags & PR_ASLEEP) &&
		    (sys = Psp->pr_lwp.pr_syscall) > 0 && sys <= PRMAXSYS)
			(void) printf(", in %s()",
				sysname(sys, getsubcode(Pr)));
		(void) fputc('\n', stdout);
		Flush();
	}

	return (JOBSTOP);
}

/*
 * Return the signal the process stopped on iff process is already stopped on
 * PR_JOBCONTROL or is stopped on PR_SIGNALLED or PR_REQUESTED with a current
 * signal that will cause a JOBCONTROL stop when the process is set running.
 */
static int
stopsig(struct ps_prochandle *Pr)
{
	const pstatus_t *Psp = Pstatus(Pr);
	int sig = 0;

	if (Pstate(Pr) == PS_STOP) {
		switch (Psp->pr_lwp.pr_why) {
		case PR_JOBCONTROL:
			sig = Psp->pr_lwp.pr_what;
			if (sig < 0 || sig > PRMAXSIG)
				sig = 0;
			break;
		case PR_SIGNALLED:
		case PR_REQUESTED:
			if (Psp->pr_lwp.pr_action.sa_handler == SIG_DFL) {
				switch (Psp->pr_lwp.pr_cursig) {
				case SIGSTOP:
					sig = SIGSTOP;
					break;
				case SIGTSTP:
				case SIGTTIN:
				case SIGTTOU:
					if (!(Psp->pr_flags & PR_ORPHAN))
						sig = Psp->pr_lwp.pr_cursig;
					break;
				}
			}
			break;
		}
	}

	return (sig);
}

int
signalled(struct ps_prochandle *Pr, int flag)
{
	const pstatus_t *Psp = Pstatus(Pr);
	int sig = Psp->pr_lwp.pr_what;

	if (sig <= 0 || sig > PRMAXSIG)	/* check bounds */
		return (0);

	if (cflag)			/* just counting */
		Cp->sigcount[sig]++;

	if (sig == SIGCONT && (flag == JOBSIG || flag == JOBSTOP))
		flag = requested(Pr, JOBSTOP);
	else if ((flag = jobcontrol(Pr)) == 0 &&
	    !cflag &&
	    prismember(&signals, sig)) {
		int sys;

		length = 0;
		putpname();
		timestamp(Psp);
		(void) printf("    Received signal #%d, %s",
			sig, signame(sig));
		if ((Psp->pr_lwp.pr_flags & PR_ASLEEP) &&
		    (sys = Psp->pr_lwp.pr_syscall) > 0 && sys <= PRMAXSYS)
			(void) printf(", in %s()",
				sysname(sys, getsubcode(Pr)));
		if (Psp->pr_lwp.pr_action.sa_handler == SIG_DFL)
			(void) printf(" [default]");
		else if (Psp->pr_lwp.pr_action.sa_handler == SIG_IGN)
			(void) printf(" [ignored]");
		else
			(void) printf(" [caught]");
		(void) fputc('\n', stdout);
		if (Psp->pr_lwp.pr_info.si_code != 0 ||
		    Psp->pr_lwp.pr_info.si_pid != 0)
			print_siginfo(&Psp->pr_lwp.pr_info);
		Flush();
	}

	if (flag == JOBSTOP)
		flag = JOBSIG;
	return (flag);
}

int
faulted(struct ps_prochandle *Pr)
{
	const pstatus_t *Psp = Pstatus(Pr);
	int flt = Psp->pr_lwp.pr_what;

	if ((uint_t)flt > PRMAXFAULT || !prismember(&faults, flt))
		return (0);

	Cp->fltcount[flt]++;

	if (cflag)		/* just counting */
		return (1);

	length = 0;
	putpname();
	timestamp(Psp);
	(void) printf("    Incurred fault #%d, %s  %%pc = 0x%.8lX",
		flt, fltname(flt), (long)Psp->pr_lwp.pr_reg[R_PC]);
	if (flt == FLTPAGE)
		(void) printf("  addr = 0x%.8lX",
			(long)Psp->pr_lwp.pr_info.si_addr);
	(void) fputc('\n', stdout);
	if (Psp->pr_lwp.pr_info.si_signo != 0)
		print_siginfo(&Psp->pr_lwp.pr_info);
	Flush();
	return (1);
}

/*
 * Set up sys_nargs and sys_args[] (syscall args).
 */
static void
setupsysargs(struct ps_prochandle *Pr)
{
	const pstatus_t *Psp = Pstatus(Pr);
	int nargs;
	int what = Psp->pr_lwp.pr_what;
	int i;

	/* protect ourself from operating system error */
	if (what <= 0 || what > PRMAXSYS)
		what = 0;

#if sparc
	/* determine whether syscall is indirect */
	sys_indirect = (Psp->pr_lwp.pr_reg[R_G1] == SYS_syscall)? 1 : 0;
#else
	sys_indirect = 0;
#endif

	(void) memset(sys_args, 0, sizeof (sys_args));
	if (what != Psp->pr_lwp.pr_syscall) {	/* assertion */
		(void) printf("%s\t*** Inconsistent syscall: %d vs %d ***\n",
			pname, what, Psp->pr_lwp.pr_syscall);
	}
	nargs = Psp->pr_lwp.pr_nsysarg;
	for (i = 0;
	    i < nargs && i < sizeof (sys_args) / sizeof (sys_args[0]); i++)
		sys_args[i] = Psp->pr_lwp.pr_sysarg[i];
	sys_nargs = nargs;
}

#define	ISREAD(code) \
	((code) == SYS_read || (code) == SYS_pread || \
	(code) == SYS_pread64 || (code) == SYS_readv || \
	(code) == SYS_recv || (code) == SYS_recvfrom)
#define	ISWRITE(code) \
	((code) == SYS_write || (code) == SYS_pwrite || \
	(code) == SYS_pwrite64 || (code) == SYS_writev || \
	(code) == SYS_send || (code) == SYS_sendto)

/*
 * Return TRUE iff syscall is being traced.
 */
int
sysentry(struct ps_prochandle *Pr)
{
	const pstatus_t *Psp = Pstatus(Pr);
	long arg;
	int nargs;
	int i;
	int x;
	int len;
	char *s;
	const struct systable *stp;
	int what = Psp->pr_lwp.pr_what;
	int subcode;
	int istraced;
	int raw;

	/* protect ourself from operating system error */
	if (what <= 0 || what > PRMAXSYS)
		what = 0;

	/* set up the system call arguments (sys_nargs & sys_args[]) */
	setupsysargs(Pr);
	nargs = sys_nargs;

	/* get systable entry for this syscall */
	subcode = getsubcode(Pr);
	stp = subsys(what, subcode);

	if (nargs > stp->nargs)
		nargs = stp->nargs;
	sys_nargs = nargs;

	/* fetch and remember first argument if it's a string */
	sys_valid = FALSE;
	if (nargs > 0 && stp->arg[0] == STG) {
		long offset;
		uint32_t offset32;

		/*
		 * Special case for exit from exec().
		 * The address in sys_args[0] refers to the old process image.
		 * We must fetch the string from the new image.
		 */
		if (Psp->pr_lwp.pr_why == PR_SYSEXIT &&
		    (Psp->pr_lwp.pr_what == SYS_execve ||
		    Psp->pr_lwp.pr_what == SYS_exec)) {
			psinfo_t psinfo;
			long argv;
			auxv_t auxv[32];
			int naux;

			offset = 0;
			naux = proc_get_auxv(Psp->pr_pid, auxv, 32);
			for (i = 0; i < naux; i++) {
				if (auxv[i].a_type == AT_SUN_EXECNAME) {
					offset = (long)auxv[i].a_un.a_ptr;
					break;
				}
			}
			if (offset == 0 &&
			    proc_get_psinfo(Psp->pr_pid, &psinfo) == 0) {
				argv = (long)psinfo.pr_argv;
				if (data_model == PR_MODEL_LP64)
					(void) Pread(Pr, &offset,
						sizeof (offset), argv);
				else {
					offset32 = 0;
					(void) Pread(Pr, &offset32,
						sizeof (offset32), argv);
					offset = offset32;
				}
			}
		} else {
			offset = (long)sys_args[0];
		}
		if ((s = fetchstring(offset, 400)) != NULL) {
			sys_valid = TRUE;
			len = strlen(s);
			while (len >= sys_psize) { /* reallocate if necessary */
				free(sys_path);
				sys_path = malloc(sys_psize *= 2);
				if (sys_path == NULL)
					abend("cannot allocate pathname buffer",
						0);
			}
			(void) strcpy(sys_path, s);	/* remember pathname */
		}
	}

	istraced = prismember(&trace, what);
	raw = prismember(&rawout, what);

	/* force tracing of read/write buffer dump syscalls */
	if (!istraced && nargs > 2) {
		int fdp1 = sys_args[0] + 1;

		if (ISREAD(what)) {
			if (prismember(&readfd, fdp1))
				istraced = TRUE;
		} else if (ISWRITE(what)) {
			if (prismember(&writefd, fdp1))
				istraced = TRUE;
		}
	}

	sys_leng = 0;
	if (cflag || !istraced)		/* just counting */
		*sys_string = 0;
	else {
		int argprinted = FALSE;

		sys_leng = sprintf(sys_string, "%s(",
			sysname(what, raw? -1 : subcode));
		for (i = 0; i < nargs; i++) {
			arg = sys_args[i];
			x = stp->arg[i];

			if (x == STG && !raw &&
			    i == 0 && sys_valid) {	/* already fetched */
				outstring("\"");
				outstring(sys_path);
				outstring("\"");
				argprinted = TRUE;
			} else if (x != HID || raw) {
				if (argprinted)
					outstring(", ");
				if (x == LLO)
					(*Print[x])(raw, arg, sys_args[++i]);
				else
					(*Print[x])(raw, arg);
				argprinted = TRUE;
			}
		}
		outstring(")");
	}

	return (istraced);
}
#undef	ISREAD
#undef	ISWRITE

/*
 * sysexit() returns non-zero if anything was printed.
 */
int
sysexit(struct ps_prochandle *Pr)
{
	/*
	 * Here we cast away the const-ness of Pstatus so we can cheat below.
	 */
	pstatus_t *Psp = (pstatus_t *)Pstatus(Pr);
	int what = Psp->pr_lwp.pr_what;
	const struct systable *stp;
	int arg0;
	int istraced;
	int raw;

	/* protect ourself from operating system error */
	if (what <= 0 || what > PRMAXSYS)
		return (0);

	/*
	 * If we aren't supposed to be tracing this one, then
	 * delete it from the traced signal set.  We got here
	 * because the process was sleeping in an untraced syscall.
	 */
	if (!prismember(&traceeven, what)) {
		(void) Psysexit(Pr, what, FALSE);
		return (0);
	}

	/* get systable entry for this syscall */
	stp = subsys(what, -1);

	/* pick up registers & set Errno before anything else */
	Errno = Psp->pr_lwp.pr_errno;
	Rval1 = Psp->pr_lwp.pr_rval1;
	Rval2 = Psp->pr_lwp.pr_rval2;

	switch (what) {
	case SYS_exit:		/* these are traced on entry */
	case SYS_lwp_exit:
	case SYS_evtrapret:
	case SYS_context:
		istraced = prismember(&trace, what);
		break;
	case SYS_exec:		/* these are normally traced on entry */
	case SYS_execve:
		istraced = prismember(&trace, what);
		if (exec_string && *exec_string) {
			if (!cflag && istraced) { /* print exec() string now */
				id_t lwpid;
				if (exec_pname[0] != '\0')
					(void) fputs(exec_pname, stdout);
				/*
				 * timestamp() expects to see the
				 * original lwpid, before the exec.
				 */
				lwpid = Psp->pr_lwp.pr_lwpid;
				Psp->pr_lwp.pr_lwpid = exec_lwpid;
				timestamp(Psp);
				Psp->pr_lwp.pr_lwpid = lwpid;
				(void) fputs(exec_string, stdout);
			}
			exec_pname[0] = '\0';
			exec_string[0] = '\0';
			break;
		}
		/* FALLTHROUGH */
	default:
		if (slowmode) {		/* everything traced on entry */
			istraced = prismember(&trace, what);
			break;
		}
		/* we called sysentry() in main() for these */
		if (what == SYS_open || what == SYS_open64)
			istraced = prismember(&trace, what);
		else
			istraced = sysentry(Pr);
		length = 0;
		if (!cflag && istraced) {
			putpname();
			timestamp(Psp);
			length += printf("%s", sys_string);
		}
		sys_leng = 0;
		*sys_string = '\0';
		break;
	}

	if (istraced) {
		Cp->syscount[what]++;
		accumulate(&Cp->systime[what], &Psp->pr_stime, &syslast);
	}
	syslast = Psp->pr_stime;
	usrlast = Psp->pr_utime;

	arg0 = sys_args[0];

	if (!cflag && istraced) {
		if ((what == SYS_fork ||
		    what == SYS_vfork ||
		    what == SYS_fork1) &&
		    Errno == 0 && Rval2 != 0) {
			length &= ~07;
			length += 14 + printf("\t\t(returning as child ...)");
		}
		if (what == SYS_lwp_create &&
		    Errno == 0 && Rval1 == 0) {
			length &= ~07;
			length += 7 + printf("\t(returning as new lwp ...)");
		}
		if (Errno != 0 || (what != SYS_exec && what != SYS_execve)) {
			/* prepare to print the return code */
			length >>= 3;
			if (length >= 6)
				(void) fputc(' ', stdout);
			for (; length < 6; length++)
				(void) fputc('\t', stdout);
		}
	}
	length = 0;

	/* interpret syscalls with sub-codes */
	if (sys_nargs > 0 && what != SYS_open)
		stp = subsys(what, arg0);

	raw = prismember(&rawout, what);

	if (Errno != 0) {		/* error in syscall */
		if (istraced) {
			Cp->syserror[what]++;
			if (!cflag) {
				const char *ename = errname(Errno);

				(void) printf("Err#%d", Errno);
				if (ename != NULL) {
					(void) fputc(' ', stdout);
					(void) fputs(ename, stdout);
				}
				(void) fputc('\n', stdout);
			}
		}
	} else {
		/* show arguments on successful exec */
		if (what == SYS_exec || what == SYS_execve) {
			if (!cflag && istraced)
				showargs(Pr, raw);
		} else if (!cflag && istraced) {
			const char *fmt = NULL;
			long rv1 = Rval1;
			long rv2 = Rval2;

			switch (what) {
			case SYS_llseek:
				rv1 &= 0xffffffff;
				rv2 &= 0xffffffff;
#ifdef _LONG_LONG_LTOH	/* first long of a longlong is the low order */
				if (rv2 != 0) {
					long temp = rv1;
					fmt = "= 0x%lX%.8lX";
					rv1 = rv2;
					rv2 = temp;
					break;
				}
#else	/* the other way around */
				if (rv1 != 0) {
					fmt = "= 0x%lX%.8lX";
					break;
				}
				rv1 = rv2;	/* ugly */
#endif
				/* FALLTHROUGH */
			case SYS_lseek:
			case SYS_ulimit:
				if (rv1 & 0xff000000)
					fmt = "= 0x%.8lX";
				break;
			case SYS_signal:
				if (raw)
					/* EMPTY */;
				else if (rv1 == (int)SIG_DFL)
					fmt = "= SIG_DFL";
				else if (rv1 == (int)SIG_IGN)
					fmt = "= SIG_IGN";
				else if (rv1 == (int)SIG_HOLD)
					fmt = "= SIG_HOLD";
				break;
			case SYS_sigtimedwait:
				if (raw)
					/* EMPTY */;
				else if ((fmt = rawsigname(rv1)) != NULL) {
					rv1 = (long)fmt;	/* filthy */
					fmt = "= %s";
				}
				break;
			}

			if (fmt == NULL) {
				switch (stp->rval[0]) {
				case HEX:
					fmt = "= 0x%.8lX";
					break;
				case HHX:
					fmt = "= 0x%.4lX";
					break;
				case OCT:
					fmt = "= %#lo";
					break;
				default:
					fmt = "= %ld";
					break;
				}
			}

			(void) printf(fmt, rv1, rv2);

			switch (stp->rval[1]) {
			case NOV:
				fmt = NULL;
				break;
			case HEX:
				fmt = " [0x%.8lX]";
				break;
			case HHX:
				fmt = " [0x%.4lX]";
				break;
			case OCT:
				fmt = " [%#lo]";
				break;
			default:
				fmt = " [%ld]";
				break;
			}

			if (fmt != NULL)
				(void) printf(fmt, rv2);
			(void) fputc('\n', stdout);
		}

		if (what == SYS_fork ||
		    what == SYS_vfork ||
		    what == SYS_fork1) {
			if (Rval2 == 0)		/* child was created */
				child = Rval1;
			else if (istraced)	/* this is the child */
				Cp->syscount[what]--;
		}
	}

#define	ISREAD(code) \
	((code) == SYS_read || (code) == SYS_pread || (code) == SYS_pread64 || \
	(code) == SYS_recv || (code) == SYS_recvfrom)
#define	ISWRITE(code) \
	((code) == SYS_write || (code) == SYS_pwrite || \
	(code) == SYS_pwrite64 || (code) == SYS_send || (code) == SYS_sendto)

	if (!cflag && istraced) {
		int fdp1 = arg0+1;	/* read()/write() filedescriptor + 1 */

		if (raw) {
			if (what != SYS_exec && what != SYS_execve)
				showpaths(stp);
			if (ISREAD(what) || ISWRITE(what)) {
				if (iob_buf[0] != '\0')
					(void) printf("%s     0x%.8lX: %s\n",
						pname, sys_args[1], iob_buf);
			}
		}

		/*
		 * Show buffer contents for read()/pread() or write()/pwrite().
		 * IOBSIZE bytes have already been shown;
		 * don't show them again unless there's more.
		 */
		if ((ISREAD(what) && Errno == 0 &&
		    prismember(&readfd, fdp1)) ||
		    (ISWRITE(what) && prismember(&writefd, fdp1))) {
			long nb = ISWRITE(what) ? sys_args[2] : Rval1;

			if (nb > IOBSIZE) {
				/* enter region of lengthy output */
				if (nb > BUFSIZ/4)
					Eserialize();

				showbuffer(Pr, (long)sys_args[1], nb);

				/* exit region of lengthy output */
				if (nb > BUFSIZ/4)
					Xserialize();
			}
		}
#undef	ISREAD
#undef	ISWRITE
		/*
		 * Do verbose interpretation if requested.
		 * If buffer contents for read or write have been requested and
		 * this is a readv() or writev(), force verbose interpretation.
		 */
/* XXX add SYS_sendmsg and SYS_recvmsg */
		if (prismember(&verbose, what) ||
		    (what == SYS_readv && Errno == 0 &&
		    prismember(&readfd, fdp1)) ||
		    (what == SYS_writev && prismember(&writefd, fdp1)))
			expound(Pr, Rval1, raw);
	}

	return (!cflag && istraced);
}

static void
showpaths(const struct systable *stp)
{
	int i;

	for (i = 0; i < sys_nargs; i++) {
		if ((stp->arg[i] == STG) ||
		    (stp->arg[i] == RST && !Errno) ||
		    (stp->arg[i] == RLK && !Errno && Rval1 > 0)) {
			long addr = (long)sys_args[i];
			int maxleng =
			    (stp->arg[i] == RLK)? Rval1 : 400;
			char *s;

			if (i == 0 && sys_valid)	/* already fetched */
				s = sys_path;
			else
				s = fetchstring(addr,
				    maxleng > 400 ? 400 : maxleng);

			if (s != (char *)NULL)
				(void) printf("%s     0x%.8lX: \"%s\"\n",
					pname, addr, s);
		}
	}
}

/*
 * Display arguments to successful exec().
 */
static void
showargs(struct ps_prochandle *Pr, int raw)
{
	const pstatus_t *Psp = Pstatus(Pr);
	int nargs;
	long ap;
	int ptrsize;
	int fail;

	length = 0;
	ptrsize = (data_model == PR_MODEL_LP64)? 8 : 4;

#if defined(__i386) || defined(__ia64)	/* XXX Merced */
	ap = (long)Psp->pr_lwp.pr_reg[R_SP];
	fail = (Pread(Pr, &nargs, sizeof (nargs), ap) != sizeof (nargs));
	ap += ptrsize;
#endif /* i386 */

#if sparc
	if (data_model == PR_MODEL_LP64) {
		int64_t xnargs;
		ap = (long)(Psp->pr_lwp.pr_reg[R_SP]) + 16 * sizeof (int64_t)
			+ STACK_BIAS;
		fail = (Pread(Pr, &xnargs, sizeof (xnargs), ap) !=
			sizeof (xnargs));
		nargs = (int)xnargs;
	} else {
		ap = (long)(Psp->pr_lwp.pr_reg[R_SP]) + 16 * sizeof (int32_t);
		fail = (Pread(Pr, &nargs, sizeof (nargs), ap) !=
			sizeof (nargs));
	}
	ap += ptrsize;
#endif /* sparc */

	if (fail) {
		(void) printf("\n%s\t*** Bad argument list? ***\n", pname);
		return;
	}

	(void) printf("  argc = %d\n", nargs);
	if (raw)
		showpaths(&systable[SYS_exec]);

	show_cred(Pr, FALSE);

	if (aflag || eflag) {		/* dump args or environment */

		/* enter region of (potentially) lengthy output */
		Eserialize();

		if (aflag)		/* dump the argument list */
			dumpargs(Pr, ap, "argv:");
		ap += (nargs+1) * ptrsize;
		if (eflag)		/* dump the environment */
			dumpargs(Pr, ap, "envp:");

		/* exit region of lengthy output */
		Xserialize();
	}
}

static void
dumpargs(struct ps_prochandle *Pr, long ap, const char *str)
{
	char *string;
	unsigned int leng = 0;
	int ptrsize;
	long arg = 0;
	char *argaddr;
	char badaddr[32];

	if (interrupt)
		return;

#ifdef _LP64
	if (data_model == PR_MODEL_LP64) {
		argaddr = (char *)&arg;
		ptrsize = 8;
	} else {
#if defined(_LITTLE_ENDIAN)
		argaddr = (char *)&arg;
#else
		argaddr = (char *)&arg + 4;
#endif
		ptrsize = 4;
	}
#else
	argaddr = (char *)&arg;
	ptrsize = 4;
#endif
	putpname();
	(void) fputc(' ', stdout);
	(void) fputs(str, stdout);
	leng += 1 + strlen(str);

	while (!interrupt) {
		if (Pread(Pr, argaddr, ptrsize, ap) != ptrsize) {
			(void) printf("\n%s\t*** Bad argument list? ***\n",
				pname);
			return;
		}
		ap += ptrsize;

		if (arg == 0)
			break;
		string = fetchstring(arg, 400);
		if (string == NULL) {
			(void) sprintf(badaddr, "BadAddress:0x%.8lX", arg);
			string = badaddr;
		}
		if ((leng += strlen(string)) < 63) {
			(void) fputc(' ', stdout);
			leng++;
		} else {
			(void) fputc('\n', stdout);
			leng = 0;
			putpname();
			(void) fputs("  ", stdout);
			leng += 2 + strlen(string);
		}
		(void) fputs(string, stdout);
	}
	(void) fputc('\n', stdout);
}

/*
 * Display contents of read() or write() buffer.
 */
void
showbuffer(struct ps_prochandle *Pr, long offset, long count)
{
	char buffer[320];
	int nbytes;
	char *buf;
	int n;

	while (count > 0 && !interrupt) {
		nbytes = (count < sizeof (buffer))? count : sizeof (buffer);
		if ((nbytes = Pread(Pr, buffer, nbytes, offset)) <= 0)
			break;
		count -= nbytes;
		offset += nbytes;
		buf = buffer;
		while (nbytes > 0 && !interrupt) {
			char obuf[65];

			n = (nbytes < 32)? nbytes : 32;
			showbytes(buf, n, obuf);

			putpname();
			(void) fputs("  ", stdout);
			(void) fputs(obuf, stdout);
			(void) fputc('\n', stdout);
			nbytes -= n;
			buf += n;
		}
	}
}

void
showbytes(const char *buf, int n, char *obuf)
{
	int c;

	while (--n >= 0) {
		int c1 = '\\';
		int c2;

		switch (c = (*buf++ & 0xff)) {
		case '\0':
			c2 = '0';
			break;
		case '\b':
			c2 = 'b';
			break;
		case '\t':
			c2 = 't';
			break;
		case '\n':
			c2 = 'n';
			break;
		case '\v':
			c2 = 'v';
			break;
		case '\f':
			c2 = 'f';
			break;
		case '\r':
			c2 = 'r';
			break;
		default:
			if (isprint(c)) {
				c1 = ' ';
				c2 = c;
			} else {
				c1 = c>>4;
				c1 += (c1 < 10)? '0' : 'A'-10;
				c2 = c&0xf;
				c2 += (c2 < 10)? '0' : 'A'-10;
			}
			break;
		}
		*obuf++ = (char)c1;
		*obuf++ = (char)c2;
	}

	*obuf = '\0';
}
