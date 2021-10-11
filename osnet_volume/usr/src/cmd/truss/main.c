/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)main.c	1.54	99/09/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <signal.h>
#include <wait.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/fstyp.h>
#include <sys/fsid.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <libproc.h>
#include "ramdata.h"
#include "proto.h"

/*
 * Function prototypes for static routines in this file.
 */
static	void	setup_basetime(const pstatus_t *, hrtime_t, struct timeval *);
static	int	xcreat(char *);
static	void	setoutput(int);
static	void	report(time_t);
static	void	prtim(timestruc_t *);
static	void	pids(char *);
static	void	psargs(struct ps_prochandle *);
static	int	control(struct ps_prochandle **, pid_t);
static	struct ps_prochandle *grabit(pid_t);
static	void	release(pid_t);
static	void	intr(int);
static	int	wait4all(void);
static	void	letgo(void);

static const lwp_mutex_t sharedmutex = SHAREDMUTEX;

/*
 * Test for empty set.
 * is_empty() should not be called directly.
 */
static	int	is_empty(const uint32_t *, size_t);
#define	isemptyset(sp) \
	is_empty((uint32_t *)(sp), sizeof (*(sp)) / sizeof (uint32_t))

/*
 * OR the second set into the first set.
 * or_set() should not be called directly.
 */
static	void	or_set(uint32_t *, const uint32_t *, size_t);
#define	prorset(sp1, sp2) \
	or_set((uint32_t *)(sp1), (uint32_t *)(sp2), \
	sizeof (*(sp1)) / sizeof (uint32_t))

main(int argc, char *argv[])
{
	struct tms tms;
	time_t starttime;
	struct timeval basedate;
	hrtime_t basehrtime;
	int retc;
	int ofd = -1;
	int opt;
	int i;
	int what;
	int first;
	int errflg = FALSE;
	int badname = FALSE;
	int req_flag = 0;
	int leave_hung = FALSE;
	int reset_traps = FALSE;
	struct ps_prochandle *Pr;
	pstatus_t *Psp;

	/*
	 * Make sure fd's 0, 1, and 2 are allocated,
	 * just in case truss was invoked from init.
	 */
	while ((i = open("/dev/null", O_RDWR)) >= 0 && i < 2)
		;
	if (i > 2)
		(void) close(i);

	starttime = times(&tms);	/* for elapsed timing */

	/* this should be per-traced-process */
	pagesize = sysconf(_SC_PAGESIZE);

	/* command name (e.g., "truss") */
	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	Euid = geteuid();
	Egid = getegid();
	Ruid = getuid();
	Rgid = getgid();
	ancestor = getpid();

	prfillset(&trace);	/* default: trace all system calls */
	premptyset(&verbose);	/* default: no syscall verbosity */
	premptyset(&rawout);	/* default: no raw syscall interpretation */

	prfillset(&signals);	/* default: trace all signals */

	prfillset(&faults);	/* default: trace all faults */
	prdelset(&faults, FLTPAGE);	/* except this one */

	premptyset(&readfd);	/* default: dump no buffers */
	premptyset(&writefd);

	premptyset(&syshang);	/* default: hang on no system calls */
	premptyset(&sighang);	/* default: hang on no signals */
	premptyset(&flthang);	/* default: hang on no faults */

#define	OPTIONS	"ZFpfcaeildDht:T:v:x:s:S:m:M:u:U:r:w:o:"
	while ((opt = getopt(argc, argv, OPTIONS)) != EOF) {
		switch (opt) {
		case 'Z':		/* slow mode */
			slowmode = TRUE;
			break;
		case 'F':		/* force grabbing (no O_EXCL) */
			Fflag = PGRAB_FORCE;
			break;
		case 'p':		/* grab processes */
			pflag = TRUE;
			break;
		case 'f':		/* follow children */
			fflag = TRUE;
			break;
		case 'c':		/* don't trace, just count */
			cflag = TRUE;
			iflag = TRUE;	/* implies no interruptable syscalls */
			break;
		case 'a':		/* display argument lists */
			aflag = TRUE;
			break;
		case 'e':		/* display environments */
			eflag = TRUE;
			break;
		case 'i':		/* don't show interruptable syscalls */
			iflag = TRUE;
			break;
		case 'l':		/* show lwp id for each syscall */
			lflag = TRUE;
			break;
		case 'h':		/* debugging: report hash stats */
			hflag = TRUE;
			break;
		case 'd':		/* show time stamps */
			dflag = TRUE;
			break;
		case 'D':		/* show time deltas */
			Dflag = TRUE;
			break;
		case 't':		/* system calls to trace */
			if (syslist(optarg, &trace, &tflag))
				badname = TRUE;
			break;
		case 'T':		/* system calls to hang process */
			if (syslist(optarg, &syshang, &Tflag))
				badname = TRUE;
			break;
		case 'v':		/* verbose interpretation of syscalls */
			if (syslist(optarg, &verbose, &vflag))
				badname = TRUE;
			break;
		case 'x':		/* raw interpretation of syscalls */
			if (syslist(optarg, &rawout, &xflag))
				badname = TRUE;
			break;
		case 's':		/* signals to trace */
			if (siglist(optarg, &signals, &sflag))
				badname = TRUE;
			break;
		case 'S':		/* signals to hang process */
			if (siglist(optarg, &sighang, &Sflag))
				badname = TRUE;
			break;
		case 'm':		/* machine faults to trace */
			if (fltlist(optarg, &faults, &mflag))
				badname = TRUE;
			break;
		case 'M':		/* machine faults to hang process */
			if (fltlist(optarg, &flthang, &Mflag))
				badname = TRUE;
			break;
		case 'u':		/* user library functions to trace */
			if (liblist(optarg, 0))
				badname = TRUE;
			break;
		case 'U':		/* user library functions to hang */
			if (liblist(optarg, 1))
				badname = TRUE;
			break;
		case 'r':		/* show contents of read(fd) */
			if (fdlist(optarg, &readfd))
				badname = TRUE;
			break;
		case 'w':		/* show contents of write(fd) */
			if (fdlist(optarg, &writefd))
				badname = TRUE;
			break;
		case 'o':		/* output file for trace */
			oflag = TRUE;
			if (ofd >= 0)
				(void) close(ofd);
			if ((ofd = xcreat(optarg)) < 0) {
				perror(optarg);
				badname = TRUE;
			}
			break;
		default:
			errflg = TRUE;
			break;
		}
	}

	if (badname)
		exit(2);

	/* if -a or -e was specified, force tracing of exec() */
	if (aflag || eflag) {
		praddset(&trace, SYS_exec);
		praddset(&trace, SYS_execve);
	}

	/*
	 * Make sure that all system calls, signals, and machine faults
	 * that hang the process are added to their trace sets.
	 */
	prorset(&trace, &syshang);
	prorset(&signals, &sighang);
	prorset(&faults, &flthang);

	argc -= optind;
	argv += optind;

	/* collect the specified process ids */
	if (pflag && argc > 0) {
		if ((grab = malloc(argc * sizeof (pid_t))) == NULL)
			abend("cannot allocate memory for process-ids", 0);
		while (argc-- > 0)
			pids(*argv++);
	}

	if (errflg || (argc <= 0 && ngrab <= 0)) {
		(void) fprintf(stderr,
	"usage:\t%s [-fcaeildD] [-[tTvx] [!]syscalls] [-[sS] [!]signals] \\\n",
			command);
		(void) fprintf(stderr,
	"\t[-[mM] [!]faults] [-[rw] [!]fds] [-[uU] [!]libs:[:][!]functs] \\\n");
		(void) fprintf(stderr,
	"\t[-o outfile]  command | -p pid ...\n");
		exit(2);
	}

	if ((sys_path = malloc(sys_psize = 16)) == NULL)
		abend("cannot allocate memory for syscall pathname", 0);
	if ((sys_string = malloc(sys_ssize = 32)) == NULL)
		abend("cannot allocate memory for syscall string", 0);

	if (argc > 0) {		/* create the controlled process */
		int err;
		char path[PATH_MAX];

		if ((Pr = Pcreate(argv[0], &argv[0], &err, path, sizeof (path)))
		    == NULL) {
			switch (err) {
			case C_PERM:
				(void) fprintf(stderr,
					"%s: cannot trace set-id or "
					"unreadable object file: %s\n",
					command, path);
				break;
			case C_LP64:
				(void) fprintf(stderr,
					"%s: cannot control _LP64 "
					"program: %s\n",
					command, path);
				break;
			case C_NOEXEC:
				(void) fprintf(stderr,
					"%s: cannot find/execute "
					"program: %s\n",
					command, argv[0]);
				break;
			case C_STRANGE:
				break;
			default:
				(void) fprintf(stderr, "%s: %s\n",
					command, Pcreate_error(err));
				break;
			}
			exit(2);
		}
		basehrtime = gethrtime();
		(void) gettimeofday(&basedate, NULL);
		if (fflag || Dynpat != NULL)
			(void) Psetflags(Pr, PR_FORK);
		else
			(void) Punsetflags(Pr, PR_FORK);
		Proc = Pr;
		/*
		 * Cast away const so we can cheat and modify Psp
		 */
		Psp = (pstatus_t *)Pstatus(Pr);
		data_model = Psp->pr_dmodel;
		created = Psp->pr_pid;
		make_pname(Pr, 0);
		(void) sysentry(Pr);
		length = 0;
		if (!cflag && prismember(&trace, SYS_execve)) {
			exec_string = realloc(exec_string,
				strlen(sys_string) + 1);
			(void) strcpy(exec_pname, pname);
			(void) strcpy(exec_string, sys_string);
			length += strlen(sys_string);
			exec_lwpid = Psp->pr_lwp.pr_lwpid;
			sys_leng = 0;
			*sys_string = '\0';
		}
		sysbegin = syslast = Psp->pr_stime;
		usrbegin = usrlast = Psp->pr_utime;
	}

	setoutput(ofd);		/* establish truss output */
	istty = isatty(1);

	if (setvbuf(stdout, (char *)NULL, _IOFBF, BUFSIZ) != 0) {
		abend("setvbuf() failure", 0);
		exit(2);
	}

	/*
	 * Set up signal dispositions.
	 */
	if (created && (oflag || !istty)) {	/* ignore interrupts */
		(void) sigset(SIGHUP, SIG_IGN);
		(void) sigset(SIGINT, SIG_IGN);
		(void) sigset(SIGQUIT, SIG_IGN);
	} else {				/* receive interrupts */
		if (sigset(SIGHUP, SIG_IGN) == SIG_DFL)
			(void) sigset(SIGHUP, intr);
		if (sigset(SIGINT, SIG_IGN) == SIG_DFL)
			(void) sigset(SIGINT, intr);
		if (sigset(SIGQUIT, SIG_IGN) == SIG_DFL)
			(void) sigset(SIGQUIT, intr);
	}
	(void) sigset(SIGTERM, intr);
	(void) sigset(SIGUSR1, intr);
	(void) sigset(SIGPIPE, intr);

	/* don't accumulate zombie children */
	(void) sigset(SIGCLD, SIG_IGN);

	if (fflag || Dynpat != NULL || ngrab > 1) { /* multiple processes */
		int zfd;
		void *p;	/* to suppress lint warning */

		if ((zfd = open("/dev/zero", O_RDWR)) < 0)
			abend("cannot open /dev/zero for shared memory", 0);
		p = mmap(NULL, sizeof (struct counts),
			PROT_READ|PROT_WRITE, MAP_SHARED, zfd, (off_t)0);
		Cp = (struct counts *)p;
		if (Cp == (struct counts *)(-1))
			abend("cannot mmap /dev/zero for shared memory", 0);
		(void) close(zfd);
	}
	if (Cp == (struct counts *)NULL)
		Cp = malloc(sizeof (struct counts));
	if (Cp == (struct counts *)NULL)
		abend("cannot allocate memory for counts", 0);

	(void) memset((char *)Cp, 0, sizeof (struct counts));
	(void) memcpy(&Cp->mutex[0], &sharedmutex, sizeof (lwp_mutex_t));
	(void) memcpy(&Cp->mutex[1], &sharedmutex, sizeof (lwp_mutex_t));

	if (created) {
		setup_basetime(Psp, basehrtime, &basedate);
		procadd(created);
		show_cred(Pr, TRUE);
	} else {		/* grab the specified processes */
		int gotone = FALSE;

		i = 0;
		while (i < ngrab) {		/* grab first process */
			if ((Pr = grabit(grab[i++])) != NULL) {
				gotone = TRUE;
				break;
			}
		}
		if (!gotone)
			abend(0, 0);
		basehrtime = gethrtime();
		(void) gettimeofday(&basedate, NULL);
		setup_basetime(Pstatus(Pr), basehrtime, &basedate);
		while (i < ngrab) {		/* grab the remainder */
			pid_t pid = grab[i++];

			switch (fork()) {
			case -1:
				(void) fprintf(stderr,
			"%s: cannot fork to control process, pid# %d\n",
					command, (int)pid);
			default:
				continue;	/* parent carries on */

			case 0:			/* child grabs process */
				Pfree(Pr);
				descendent = TRUE;
				if ((Pr = grabit(pid)) != NULL)
					break;
				exit(2);
			}
			break;
		}
		Proc = Pr;
		/*
		 * Cast away const so we can cheat and modify Psp
		 */
		Psp = (pstatus_t *)Pstatus(Pr);
	}

	/*
	 * If running setuid-root, become root for real to avoid
	 * affecting the per-user limitation on the maximum number
	 * of processes (one benefit of running setuid-root).
	 */
	if (Rgid != Egid)
		(void) setgid(Egid);
	if (Ruid != Euid)
		(void) setuid(Euid);

	if (!created && aflag && prismember(&trace, SYS_execve)) {
		psargs(Pr);
		Flush();
	}

	if (created && Pstate(Pr) != PS_STOP)	/* assertion */
		if (!(interrupt || sigusr1))
			abend("ASSERT error: process is not stopped", 0);

	traceeven = trace;		/* trace these system calls */

	/* trace these regardless, even if we don't report results */
	praddset(&traceeven, SYS_exit);
	praddset(&traceeven, SYS_lwp_exit);
	praddset(&traceeven, SYS_exec);
	praddset(&traceeven, SYS_execve);
	praddset(&traceeven, SYS_open);
	praddset(&traceeven, SYS_open64);
	praddset(&traceeven, SYS_fork);
	praddset(&traceeven, SYS_vfork);
	praddset(&traceeven, SYS_fork1);

	/* for I/O buffer dumps, force tracing of read()s and write()s */
	if (!isemptyset(&readfd)) {
		praddset(&traceeven, SYS_read);
		praddset(&traceeven, SYS_readv);
		praddset(&traceeven, SYS_pread);
		praddset(&traceeven, SYS_pread64);
		praddset(&traceeven, SYS_recv);
		praddset(&traceeven, SYS_recvfrom);
		praddset(&traceeven, SYS_recvmsg);
	}
	if (!isemptyset(&writefd)) {
		praddset(&traceeven, SYS_write);
		praddset(&traceeven, SYS_writev);
		praddset(&traceeven, SYS_pwrite);
		praddset(&traceeven, SYS_pwrite64);
		praddset(&traceeven, SYS_send);
		praddset(&traceeven, SYS_sendto);
		praddset(&traceeven, SYS_sendmsg);
	}

	Psetsysexit(Pr, &traceeven);
	if (slowmode) {
		/*
		 * Trace all system calls on entry, so we
		 * can print the entry information and
		 * have it seen even if the system panics
		 * before the syscall completes.
		 */
		Psetsysentry(Pr, &traceeven);
	}

	/* special case -- cannot trace sysexit because context is changed */
	if (prismember(&trace, SYS_context)) {
		(void) Psysentry(Pr, SYS_context, TRUE);
		(void) Psysexit(Pr, SYS_context, FALSE);
		prdelset(&traceeven, SYS_context);
	}

	/* special case -- sysexit not traced by OS */
	if (prismember(&trace, SYS_evtrapret)) {
		(void) Psysentry(Pr, SYS_evtrapret, TRUE);
		(void) Psysexit(Pr, SYS_evtrapret, FALSE);
		prdelset(&traceeven, SYS_evtrapret);
	}

	/* special case -- trace exec() on entry to get the args */
	(void) Psysentry(Pr, SYS_exec, TRUE);
	(void) Psysentry(Pr, SYS_execve, TRUE);

	/* special case -- sysexit never reached */
	(void) Psysentry(Pr, SYS_exit, TRUE);
	(void) Psysentry(Pr, SYS_lwp_exit, TRUE);
	(void) Psysexit(Pr, SYS_exit, FALSE);
	(void) Psysexit(Pr, SYS_lwp_exit, FALSE);

	Psetsignal(Pr, &signals);	/* trace these signals */
	Psetfault(Pr, &faults);		/* trace these faults */

	/* for function call tracing */
	if (Dynpat != NULL) {
		/* trace these regardless, to deal with function calls */
		(void) Pfault(Pr, FLTBPT, TRUE);
		(void) Pfault(Pr, FLTTRACE, TRUE);

		/* needed for x86 */
		(void) Psetflags(Pr, PR_BPTADJ);

		/*
		 * Find functions and set breakpoints on grabbed process.
		 * A process stopped on exec() gets its breakpoints set below.
		 */
		if ((Psp->pr_lwp.pr_why != PR_SYSENTRY &&
		    Psp->pr_lwp.pr_why != PR_SYSEXIT) ||
		    (Psp->pr_lwp.pr_what != SYS_exec &&
		    Psp->pr_lwp.pr_what != SYS_execve)) {
			establish_breakpoints(Pr);
			establish_stacks(Pr);
		}
	}

	/* flush out all tracing flags now. */
	Psync(Pr);

	first = created? FALSE : TRUE;
	if (!created &&
	    Pstate(Pr) == PS_STOP &&
	    Psp->pr_lwp.pr_why == PR_REQUESTED) {
		/*
		 * We grabbed a running process.
		 * Set it running again.
		 */
		first = FALSE;
		if (Psetrun(Pr, 0, 0) == -1)
			abend("cannot start subject process", " 1");
	}

	/*
	 * Run until termination.
	 */
	for (;;) {
		if (interrupt || sigusr1) {
			if (length)
				(void) fputc('\n', stdout);
			if (sigusr1)
				letgo();
			Flush();
			clear_breakpoints(Pr);
			Prelease(Pr, PRELEASE_CLEAR);
			break;
		}
		if (Pstate(Pr) == PS_RUN) {
			/* millisecond timeout is for sleeping syscalls */
			unsigned tout = (iflag||req_flag)? 0 : 1000;

			(void) Pwait(Pr, tout);
			if (Pstate(Pr) == PS_RUN && !interrupt && !sigusr1)
				req_flag = requested(Pr, 0);
			continue;
		}
		data_model = Psp->pr_dmodel;
		if (Pstate(Pr) == PS_UNDEAD) {
			if (!exit_called && !cflag) {
				if (length)
					(void) fputc('\n', stdout);
				(void) printf(
					"%s\t*** process killed ***\n",
					pname);
			}
			Flush();
			report_htable_stats();
			Prelease(Pr, PRELEASE_CLEAR);
			break;
		}
		if (Pstate(Pr) == PS_LOST) {	/* we lost control */
			if (Preopen(Pr) == 0)	/* we got it back */
				continue;

			/* we really lost it */
			if (exec_string && *exec_string) {
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
				(void) fputc('\n', stdout);
			} else if (length) {
				(void) fputc('\n', stdout);
			}
			if (sys_valid)
				(void) printf(
			"%s\t*** cannot trace across exec() of %s ***\n",
					pname, sys_path);
			else
				(void) printf(
				"%s\t*** lost control of process ***\n",
					pname);
			length = 0;
			Flush();
			report_htable_stats();
			Prelease(Pr, PRELEASE_CLEAR);
			break;
		}
		if (Pstate(Pr) != PS_STOP) {
			(void) fprintf(stderr,
				"%s: state = %d\n", command, Pstate(Pr));
			abend(pname, "uncaught status of subject process");
		}

		/*
		 * Special case for return from vfork() in the parent.
		 * We have to reestablish the breakpoint traps and
		 * all the return-from-function-call traps.
		 */
		if (reset_traps &&
		    Psp->pr_lwp.pr_why == PR_FAULTED &&
		    Psp->pr_lwp.pr_what == FLTTRACE) {
			reset_traps = FALSE;
			reestablish_traps(Pr);
			(void) Psetrun(Pr, 0, PRCFAULT);
			continue;
		}

		if (!cflag && (lflag || Dynpat != 0))
			make_pname(Pr, 0);

		what = Psp->pr_lwp.pr_what;
		switch (Psp->pr_lwp.pr_why) {
		case PR_REQUESTED:
			/* avoid multiple timeouts */
			req_flag = requested(Pr, req_flag);
			break;
		case PR_SIGNALLED:
			req_flag = signalled(Pr, req_flag);
			if (Sflag && !first && prismember(&sighang, what))
				leave_hung = TRUE;
			break;
		case PR_FAULTED:
			req_flag = 0;
			if (what == FLTBPT) {
				int rval = function_trace(Pr, first, 0);
				if (rval == 1)
					leave_hung = TRUE;
				if (rval >= 0)
					break;
			}
			if (faulted(Pr) &&
			    Mflag && !first && prismember(&flthang, what))
				leave_hung = TRUE;
			break;
		case PR_JOBCONTROL:	/* can't happen except first time */
			req_flag = jobcontrol(Pr);
			break;
		case PR_SYSENTRY:
			req_flag = 0;
			/* protect ourself from operating system error */
			if (what <= 0 || what > PRMAXSYS)
				what = PRMAXSYS;
			length = 0;

			/*
			 * Special cases.  Most syscalls are traced on exit.
			 */
			switch (what) {
			case SYS_exit:			/* exit() */
			case SYS_lwp_exit:		/* lwp_exit() */
			case SYS_context:		/* [get|set]context() */
			case SYS_evtrapret:		/* evtrapret() */
				if (!cflag && prismember(&trace, what)) {
					(void) sysentry(Pr);
					if (what == SYS_lwp_exit &&
					    Psp->pr_nlwp <= 1)
						outstring(
						"\t(last lwp is exiting ...)");
					putpname();
					timestamp(Psp);
					length += printf("%s\n", sys_string);
					Flush();
				}
				sys_leng = 0;
				*sys_string = '\0';
				if (prismember(&trace, what)) {
					Cp->syscount[what]++;
					accumulate(&Cp->systime[what],
						&Psp->pr_stime, &syslast);
				}
				syslast = Psp->pr_stime;
				usrlast = Psp->pr_utime;

				if (what == SYS_exit ||
				    (what == SYS_lwp_exit &&
				    Psp->pr_nlwp <= 1))
					exit_called = TRUE;
				break;
			case SYS_exec:
			case SYS_execve:
				(void) sysentry(Pr);
				if (!cflag && prismember(&trace, what)) {
					exec_string = realloc(exec_string,
						strlen(sys_string) + 1);
					(void) strcpy(exec_pname, pname);
					(void) strcpy(exec_string, sys_string);
					length += strlen(sys_string);
					exec_lwpid = Psp->pr_lwp.pr_lwpid;
				}
				sys_leng = 0;
				*sys_string = '\0';
				break;
			default:
				if (slowmode &&
				    !cflag && prismember(&trace, what)) {
					(void) sysentry(Pr);
					putpname();
					timestamp(Psp);
					length += printf("%s", sys_string);
					Flush();
					sys_leng = 0;
					*sys_string = '\0';
				}
				break;
			}
			if (Tflag && !first &&
			    (prismember(&syshang, what) ||
			    (exit_called && prismember(&syshang, SYS_exit))))
				leave_hung = TRUE;
			break;
		case PR_SYSEXIT:
			req_flag = 0;
			/* check for write open of a /proc file */
			if (what == SYS_open || what == SYS_open64) {
				(void) sysentry(Pr);
				Errno = Psp->pr_lwp.pr_errno;
				if ((Errno == 0 || Errno == EBUSY) &&
				    sys_valid &&
				    (sys_nargs > 1 &&
				    (sys_args[1]&0x3) != O_RDONLY)) {
					int rv = checkproc(Pr, sys_path);
					if (rv == 1 && Fflag != PGRAB_FORCE) {
						/*
						 * The process opened itself
						 * and no -F flag was specified.
						 * Just print the open() call
						 * and let go of the process.
						 */
						if (!cflag &&
						    prismember(&trace, what)) {
							putpname();
							timestamp(Psp);
							(void) printf("%s\n",
								sys_string);
						}

						letgo();
						Flush();
						clear_breakpoints(Pr);
						Prelease(Pr, PRELEASE_CLEAR);
						goto done;
					}
					if (rv == 2) {
						/*
						 * Process opened someone else.
						 * The open is being reissued.
						 * Don't report this one.
						 */
						sys_leng = 0;
						*sys_string = '\0';
						sys_nargs = -1;
						break;
					}
				}
			}
			if (sysexit(Pr))
				Flush();
			sys_nargs = -1;
			if (Tflag && !first && prismember(&syshang, what))
				leave_hung = TRUE;
			if ((what == SYS_exec || what == SYS_execve) &&
			    Errno == 0)
				reset_breakpoints(Pr);
			if (tstamp != NULL &&
			    (what == SYS_exec || what == SYS_execve ||
			    what == SYS_vfork || what == SYS_fork1) &&
			    Errno == 0) {
				/* only one lwp exists now */
				tstamp = realloc(tstamp,
					sizeof (struct tstamp));
				nstamps = 1;
			}
			break;
		default:
			req_flag = 0;
			abend("unknown reason for stopping", 0);
		}

		if (child) {		/* controlled process fork()ed */
			if (fflag || Dynpat != NULL)  {
				if (control(&Pr, child)) {
					/*
					 * We are now the child truss that
					 * follows the victim's child.
					 */
					Proc = Pr;
					/*
					 * Cast away const so we can cheat and
					 * modify Psp.
					 */
					Psp = (pstatus_t *)Pstatus(Pr);
					child = 0;
					if (!fflag) {
						/*
						 * If this is vfork(), then
						 * this clears the breakpoints
						 * in the parent's address space
						 * as well as in the child's.
						 */
						clear_breakpoints(Pr);
						Prelease(Pr, PRELEASE_CLEAR);
						break;
					}
					continue;
				}

				/*
				 * Here, we are still the parent truss.
				 * If the child messed with the breakpoints and
				 * this is vfork(), we have to set them again.
				 */
				if (Dynpat != NULL &&
				    Psp->pr_lwp.pr_syscall == SYS_vfork)
					reset_traps = TRUE;
			}
			child = 0;
		}

		if (leave_hung) {
			Flush();
			clear_breakpoints(Pr);
			Prelease(Pr, PRELEASE_CLEAR|PRELEASE_HANG);
			break;
		}

		if (Pstate(Pr) == PS_STOP) {
			int flags = 0;

			if (reset_traps) /* we must catch return from vfork() */
				flags = PRSTEP;
			if (Psetrun(Pr, 0, flags) != 0 &&
			    Pstate(Pr) != PS_LOST && Pstate(Pr) != PS_UNDEAD)
				abend("cannot start subject process", " 2");
		}
		first = FALSE;
	}
done:
	accumulate(&Cp->systotal, &syslast, &sysbegin);
	accumulate(&Cp->usrtotal, &usrlast, &usrbegin);
	procdel();
	retc = (leave_hung? 0 : wait4all());
	if (!descendent) {
		interrupt = FALSE; /* another interrupt kills the report */
		if (cflag)
			report(times(&tms) - starttime);
	}
	return (retc);	/* exit with exit status of created process, else 0 */
}

/*
 * Give a base date for time stamps, adjusted to the
 * stop time of the selected (first or created) process.
 */
static void
setup_basetime(const pstatus_t *Psp, hrtime_t basehrtime,
	struct timeval *basedate)
{
	Cp->basetime = Psp->pr_lwp.pr_tstamp;

	if ((dflag|Dflag) && !cflag) {
		const struct tm *ptm;
		const char *ptime;
		const char *pdst;
		hrtime_t delta = basehrtime -
			((hrtime_t)Cp->basetime.tv_sec * NANOSEC +
			Cp->basetime.tv_nsec);

		if (delta > 0) {
			basedate->tv_sec -= (time_t)(delta / NANOSEC);
			basedate->tv_usec -= (delta % NANOSEC) / 1000;
			if (basedate->tv_usec < 0) {
				basedate->tv_sec--;
				basedate->tv_usec += MICROSEC;
			}
		}
#ifdef DO_THIS_TO_ELIMINATE_FRACTIONAL_SECONDS_FROM_BASE_TIME_STAMP
		Cp->basetime.tv_nsec -= basedate->tv_usec * 1000;
		basedate->tv_usec = 0;
		if (Cp->basetime.tv_nsec < 0) {
			Cp->basetime.tv_sec--;
			Cp->basetime.tv_nsec += NANOSEC;
		}
#endif
		ptm = localtime(&basedate->tv_sec);
		ptime = asctime(ptm);
		if ((pdst = tzname[ptm->tm_isdst ? 1 : 0]) == NULL)
			pdst = "???";
		if (dflag) {
			(void) printf(
			    "Base time stamp:  %ld.%4.4ld  [ %.20s%s %.4s ]\n",
			    basedate->tv_sec, basedate->tv_usec / 100,
			    ptime, pdst, ptime + 20);
			Flush();
		}
	}
}

void
make_pname(struct ps_prochandle *Pr, id_t tid)
{
	const pstatus_t *Psp = Pstatus(Pr);

	pname[0] = '\0';
	if (!cflag) {
		char *s = pname;
		int ff = (fflag || ngrab > 1);
		int lf = (lflag || tid != 0);

		if (ff) {
			(void) sprintf(s, "%d", (int)Psp->pr_pid);
			s += strlen(s);
		}
		if (tid == 0)
			tid = Psp->pr_lwp.pr_lwpid;
		if (lf) {
			(void) sprintf(s, "/%d", (int)tid);
			s += strlen(s);
		}
		if (ff || lf) {
			(void) strcpy(s, ":\t");
			s += 2;
		}
		if (ff && lf && s < pname + 9)
			(void) strcpy(s, "\t");
	}
}

/*
 * Print the pname[] string, if any.
 */
void
putpname()
{
	if (pname[0])
		(void) fputs(pname, stdout);
}

/*
 * Print the timestamp, if requested (-d or -D).
 * Return the number of characters printed.
 */
void
timestamp(const pstatus_t *Psp)
{
	int sec;
	int fraction;

	if (!(dflag|Dflag) || !(Psp->pr_flags & PR_STOPPED))
		return;

	sec = Psp->pr_lwp.pr_tstamp.tv_sec - Cp->basetime.tv_sec;
	fraction = Psp->pr_lwp.pr_tstamp.tv_nsec - Cp->basetime.tv_nsec;
	if (fraction < 0) {
		sec--;
		fraction += NANOSEC;
	}
	/* fraction in 1/10 milliseconds, rounded up */
	fraction = (fraction + 50000) / 100000;
	if (fraction >= (MILLISEC * 10)) {
		sec++;
		fraction -= (MILLISEC * 10);
	}

	if (dflag)				/* time stamp */
		(void) printf("%2d.%4.4d\t", sec, fraction);

	if (Dflag && Psp->pr_nlwp > 0) {	/* time delta */
		id_t lwpid = Psp->pr_lwp.pr_lwpid;
		int i, n;

		if (tstamp == NULL) {
			tstamp = malloc(Psp->pr_nlwp * sizeof (struct tstamp));
			(void) memset(tstamp, 0,
				Psp->pr_nlwp * sizeof (struct tstamp));
			nstamps = Psp->pr_nlwp;
		}
		if (nstamps < Psp->pr_nlwp) {
			tstamp = realloc(tstamp,
				Psp->pr_nlwp * sizeof (struct tstamp));
			(void) memset(tstamp + nstamps, 0,
			    (Psp->pr_nlwp - nstamps) * sizeof (struct tstamp));
			nstamps = Psp->pr_nlwp;
		}
		n = -1;
		for (i = 0; i < nstamps; i++) {
			id_t id = tstamp[i].lwpid;
			if (id == 0)
				n = i;
			if (id == lwpid)
				break;
		}
		if (i < nstamps) {
			int osec = tstamp[i].sec;
			int ofraction = tstamp[i].fraction;

			tstamp[i].sec = sec;
			tstamp[i].fraction = fraction;
			sec -= osec;
			fraction -= ofraction;
			if (fraction < 0) {
				sec--;
				fraction += (MILLISEC * 10);
			}
		} else if (n >= 0) {
			tstamp[n].lwpid = lwpid;
			tstamp[n].sec = sec;
			tstamp[n].fraction = fraction;
			sec = fraction = 0;
		} else {
			/*
			 * not found, no space.
			 * There must be a vacated lwp slot.
			 * Find it.
			 */
			for (n = 0; n < nstamps; n++) {
				char lwpiddir[50];
				(void) sprintf(lwpiddir, "/proc/%d/lwp/%d",
					(int)Psp->pr_pid,
					(int)tstamp[n].lwpid);
				if (access(lwpiddir, F_OK) != 0)
					break;
			}
			if (n == nstamps) {
				tstamp = realloc(tstamp,
					(n + 1) * sizeof (struct tstamp));
				nstamps++;
			}
			tstamp[n].lwpid = lwpid;
			tstamp[n].sec = sec;
			tstamp[n].fraction = fraction;
			sec = fraction = 0;
		}
		(void) printf("%2d.%4.4d\t", sec, fraction);
	}
}

/*
 * Create output file, being careful about
 * suid/sgid and file descriptor 0, 1, 2 issues.
 */
static int
xcreat(char *path)
{
	int fd;
	int mode = 0666;

	if (Euid == Ruid && Egid == Rgid)	/* not set-id */
		fd = creat(path, mode);
	else if (access(path, F_OK) != 0) {	/* file doesn't exist */
		/* if directory permissions OK, create file & set ownership */

		char *dir;
		char *p;
		char dot[4];

		/* generate path for directory containing file */
		if ((p = strrchr(path, '/')) == NULL) {	/* no '/' */
			p = dir = dot;
			*p++ = '.';		/* current directory */
			*p = '\0';
		} else if (p == path) {			/* leading '/' */
			p = dir = dot;
			*p++ = '/';		/* root directory */
			*p = '\0';
		} else {				/* embedded '/' */
			dir = path;		/* directory path */
			*p = '\0';
		}

		if (access(dir, W_OK|X_OK) != 0) {
			/* not writeable/searchable */
			*p = '/';
			fd = -1;
		} else {	/* create file and set ownership correctly */
			*p = '/';
			if ((fd = creat(path, mode)) >= 0)
				(void) chown(path, (int)Ruid, (int)Rgid);
		}
	} else if (access(path, W_OK) != 0)	/* file not writeable */
		fd = -1;
	else
		fd = creat(path, mode);

	/*
	 * Make sure it's not one of 0, 1, or 2.
	 * This allows truss to work when spawned by init(1m).
	 */
	if (0 <= fd && fd <= 2) {
		int dfd = fcntl(fd, F_DUPFD, 3);
		(void) close(fd);
		fd = dfd;
	}

	/*
	 * Mark it close-on-exec so created processes don't inherit it.
	 */
	if (fd >= 0)
		(void) fcntl(fd, F_SETFD, FD_CLOEXEC);

	return (fd);
}

static void
setoutput(int ofd)
{
	if (ofd < 0) {
		(void) close(1);
		(void) fcntl(2, F_DUPFD, 1);
	} else if (ofd != 1) {
		(void) close(1);
		(void) fcntl(ofd, F_DUPFD, 1);
		(void) close(ofd);
		/* if no stderr, make it the same file */
		if ((ofd = dup(2)) < 0)
			(void) fcntl(1, F_DUPFD, 2);
		else
			(void) close(ofd);
	}
}

/*
 * Accumulate time differencies:  a += e - s;
 */
void
accumulate(timestruc_t *ap, timestruc_t *ep, timestruc_t *sp)
{
	ap->tv_sec += ep->tv_sec - sp->tv_sec;
	ap->tv_nsec += ep->tv_nsec - sp->tv_nsec;
	if (ap->tv_nsec >= 1000000000) {
		ap->tv_nsec -= 1000000000;
		ap->tv_sec++;
	} else if (ap->tv_nsec < 0) {
		ap->tv_nsec += 1000000000;
		ap->tv_sec--;
	}
}

static void
report(time_t lapse)	/* elapsed time, clock ticks */
{
	int i;
	long count;
	const char *name;
	long error;
	long total;
	long errtot;
	timestruc_t tickzero;
	timestruc_t ticks;
	timestruc_t ticktot;

	if (descendent)
		return;

	for (i = 0, total = 0; i <= PRMAXFAULT && !interrupt; i++) {
		if ((count = Cp->fltcount[i]) != 0) {
			if (total == 0)		/* produce header */
				(void) printf("faults -------------\n");
			name = fltname(i);
			(void) printf("%s%s\t%4ld\n", name,
				(((int)strlen(name) < 8)?
				    (const char *)"\t" : (const char *)""),
				count);
			total += count;
		}
	}
	if (total && !interrupt)
		(void) printf("total:\t\t%4ld\n\n", total);

	for (i = 0, total = 0; i <= PRMAXSIG && !interrupt; i++) {
		if ((count = Cp->sigcount[i]) != 0) {
			if (total == 0)		/* produce header */
				(void) printf("signals ------------\n");
			name = signame(i);
			(void) printf("%s%s\t%4ld\n", name,
				(((int)strlen(name) < 8)?
				    (const char *)"\t" : (const char *)""),
				count);
			total += count;
		}
	}
	if (total && !interrupt)
		(void) printf("total:\t\t%4ld\n\n", total);

	if (!interrupt)
		(void) printf("syscall              seconds   calls  errors\n");

	total = errtot = 0;
	tickzero.tv_sec = ticks.tv_sec = ticktot.tv_sec = 0;
	tickzero.tv_nsec = ticks.tv_nsec = ticktot.tv_nsec = 0;
	for (i = 0; i <= PRMAXSYS && !interrupt; i++) {
		if ((count = Cp->syscount[i]) != 0 || Cp->syserror[i]) {
			(void) printf("%-19.19s ", sysname(i, -1));

			ticks = Cp->systime[i];
			accumulate(&ticktot, &ticks, &tickzero);
			prtim(&ticks);

			(void) printf(" %7ld", count);
			if ((error = Cp->syserror[i]) != 0)
				(void) printf(" %6ld", error);
			(void) fputc('\n', stdout);
			total += count;
			errtot += error;
		}
	}

	if (!interrupt) {
		(void) printf("                     -------  ------   ----\n");
		(void) printf("sys totals:         ");
		prtim(&ticktot);
		(void) printf(" %7ld %6ld\n", total, errtot);
	}

	if (!interrupt) {
		(void) printf("usr time:           ");
		prtim(&Cp->usrtotal);
		(void) fputc('\n', stdout);
	}

	if (!interrupt) {
		int hz = (int)sysconf(_SC_CLK_TCK);

		ticks.tv_sec = lapse / hz;
		ticks.tv_nsec = (lapse % hz) * (1000000000 / hz);
		(void) printf("elapsed:            ");
		prtim(&ticks);
		(void) fputc('\n', stdout);
	}
}

static void
prtim(timestruc_t *tp)
{
	time_t sec;

	if ((sec = tp->tv_sec) != 0)			/* whole seconds */
		(void) printf("%5lu", sec);
	else
		(void) printf("     ");

	(void) printf(".%2.2ld", tp->tv_nsec/10000000);	/* fraction */
}

/*
 * Gather process id's.
 * Return 0 on success, != 0 on failure.
 */
static void
pids(char *arg)		/* arg is a number or /proc/nnn */
{
	pid_t pid;
	int i;

	if ((pid = proc_arg_psinfo(arg, PR_ARG_PIDS, NULL, &i)) < 0) {
		(void) fprintf(stderr, "%s: non-existent process ignored: %s\n",
			command, arg);
		return;
	}

	for (i = 0; i < ngrab; i++)
		if (grab[i] == pid)	/* duplicate */
			break;

	if (i == ngrab)
		grab[ngrab++] = pid;
	else
		(void) fprintf(stderr, "%s: duplicate process-id ignored: %d\n",
			command, (int)pid);
}

/*
 * Report psargs string.
 */
static void
psargs(struct ps_prochandle *Pr)
{
	pid_t pid = Pstatus(Pr)->pr_pid;
	psinfo_t psinfo;

	if (proc_get_psinfo(pid, &psinfo) == 0)
		(void) printf("%spsargs: %.64s\n", pname, psinfo.pr_psargs);
	else {
		perror("psargs()");
		(void) printf("%s\t*** Cannot read psinfo file for pid %d\n",
			pname, (int)pid);
	}
}

char *
fetchstring(long addr, int maxleng)
{
	struct ps_prochandle *Pr = Proc;
	int nbyte;
	int leng = 0;
	char string[41];

	string[40] = '\0';
	if (str_bsize == 0) {	/* initial allocation of string buffer */
		str_buffer = malloc(str_bsize = 16);
		if (str_buffer == NULL)
			abend("cannot allocate string buffer", 0);
	}
	*str_buffer = '\0';

	for (nbyte = 40; nbyte == 40 && leng < maxleng; addr += 40) {
		if ((nbyte = Pread(Pr, string, 40, addr)) < 0)
			return (leng? str_buffer : NULL);
		if (nbyte > 0 &&
		    (nbyte = strlen(string)) > 0) {
			while (leng+nbyte >= str_bsize) {
				str_buffer =
				    (char *)realloc(str_buffer, str_bsize *= 2);
				if (str_buffer == NULL)
					abend("cannot reallocate string buffer",
					    0);
			}
			(void) strcpy(str_buffer+leng, string);
			leng += nbyte;
		}
	}

	if (leng > maxleng)
		leng = maxleng;
	str_buffer[leng] = '\0';

	return (str_buffer);
}

void
show_cred(struct ps_prochandle *Pr, int new)
{
	prcred_t cred;

	if (proc_get_cred(Pstatus(Pr)->pr_pid, &cred, 0) < 0) {
		perror("show_cred()");
		(void) printf("%s\t*** Cannot get credentials\n", pname);
		return;
	}

	if (!cflag && prismember(&trace, SYS_exec)) {
		if (new)
			credentials = cred;
		if ((new && cred.pr_ruid != cred.pr_suid) ||
		    cred.pr_ruid != credentials.pr_ruid ||
		    cred.pr_suid != credentials.pr_suid)
			(void) printf(
		"%s    *** SUID: ruid/euid/suid = %d / %d / %d  ***\n",
			pname,
			(int)cred.pr_ruid,
			(int)cred.pr_euid,
			(int)cred.pr_suid);
		if ((new && cred.pr_rgid != cred.pr_sgid) ||
		    cred.pr_rgid != credentials.pr_rgid ||
		    cred.pr_sgid != credentials.pr_sgid)
			(void) printf(
		"%s    *** SGID: rgid/egid/sgid = %d / %d / %d  ***\n",
			pname,
			(int)cred.pr_rgid,
			(int)cred.pr_egid,
			(int)cred.pr_sgid);
	}

	credentials = cred;
}

/*
 * Take control of a child process.
 */
static int
control(struct ps_prochandle **Prp, pid_t pid)
{
	struct ps_prochandle *Pr = *Prp;
	const pstatus_t *Psp;
	pid_t mypid = 0;
	long flags;
	int rc;

	(void) sighold(SIGUSR1);
	if (interrupt || sigusr1 || (mypid = fork()) == -1) {
		if (mypid == -1)
			(void) printf(
			"%s\t*** Cannot fork() to control process #%d\n",
				pname, (int)pid);
		(void) sigrelse(SIGUSR1);
		release(pid);
		return (FALSE);
	}

	if (mypid != 0) {		/* parent carries on */
		while (!interrupt && !sigusr1) {
			(void) sigpause(SIGUSR1); /* after a brief pause */
			(void) sighold(SIGUSR1);
		}
		(void) sigrelse(SIGUSR1);
		sigusr1 = FALSE;
		return (FALSE);
	}

	(void) sigrelse(SIGUSR1);
	descendent = TRUE;
	exit_called = FALSE;
	Pfree(Pr);	/* forget old process */

	/*
	 * Child grabs the process and retains the tracing flags.
	 */
	if ((Pr = Pgrab(pid, PGRAB_RETAIN, &rc)) == NULL) {
		(void) fprintf(stderr,
			"%s: cannot control child process, pid# %d\n",
			command, (int)pid);
		(void) kill(getppid(), SIGUSR1);	/* wake up parent */
		exit(2);
	}
	/*
	 * We may have grabbed the child before it is fully stopped on exit
	 * from fork().  Wait one second (at most) for it to settle down.
	 */
	(void) Pwait(Pr, 1000);
	if (Rdb_agent != NULL)
	 	Rdb_agent = Prd_agent(Pr);

	*Prp = Pr;
	Psp = Pstatus(Pr);
	data_model = Psp->pr_dmodel;

	if (!cflag)
		make_pname(Pr, 0);

	if (Psp->pr_lwp.pr_why != PR_SYSEXIT ||
	    (Psp->pr_lwp.pr_what != SYS_fork &&
	    Psp->pr_lwp.pr_what != SYS_vfork &&
	    Psp->pr_lwp.pr_what != SYS_fork1))
		(void) printf("%s\t*** Expected SYSEXIT, SYS_[v]fork\n", pname);

	sysbegin = syslast = Psp->pr_stime;
	usrbegin = usrlast = Psp->pr_utime;

	flags = PR_FORK;
	if (Dynpat != NULL)
		flags |= PR_BPTADJ;	/* needed for x86 */
	(void) Psetflags(Pr, flags);
	procadd(pid);
	(void) kill(getppid(), SIGUSR1);	/* wake up parent */

	return (TRUE);
}

/*
 * Take control of an existing process.
 */
static struct ps_prochandle *
grabit(pid_t pid)
{
	struct ps_prochandle *Pr;
	const pstatus_t *Psp;
	int gcode;

	/*
	 * Don't force the takeover unless the -F option was specified.
	 */
	if ((Pr = Pgrab(pid, Fflag, &gcode)) == NULL) {
		(void) fprintf(stderr, "%s: %s: %d\n",
			command, Pgrab_error(gcode), (int)pid);
		return (NULL);
	}

	if (!cflag)
		make_pname(Pr, 0);

	Psp = Pstatus(Pr);
	data_model = Psp->pr_dmodel;
	sysbegin = syslast = Psp->pr_stime;
	usrbegin = usrlast = Psp->pr_utime;

	if (fflag || Dynpat != NULL)
		(void) Psetflags(Pr, PR_FORK);
	else
		(void) Punsetflags(Pr, PR_FORK);
	procadd(pid);
	show_cred(Pr, TRUE);
	return (Pr);
}

/*
 * Release process from control.
 */
static void
release(pid_t pid)
{
	/*
	 * The process in question is the child of a traced process.
	 * We are here to turn off the inherited tracing flags.
	 */

	int fd;
	char ctlname[100];
	long ctl[2];

	ctl[0] = PCSET;
	ctl[1] = PR_RLC;

	/* process is freshly forked, no need for exclusive open */
	(void) sprintf(ctlname, "/proc/%d/ctl", (int)pid);
	if ((fd = open(ctlname, O_WRONLY)) < 0 ||
	    write(fd, (char *)ctl, sizeof (ctl)) < 0) {
		perror("release()");
		(void) printf(
			"%s\t*** Cannot release child process, pid# %d\n",
			pname, (int)pid);
	}
	if (fd >= 0)	/* run-on-last-close sets the process running */
		(void) close(fd);
}

static void
intr(int sig)
{
	/*
	 * SIGUSR1 is special.  It is used by one truss process to tell
	 * another truss processes to release its controlled process.
	 */
	if (sig == SIGUSR1)
		sigusr1 = TRUE;
	else
		interrupt = TRUE;
}

void
errmsg(const char *s, const char *q)
{
	char msg[512];

	if (s || q) {
		msg[0] = '\0';
		if (command) {
			(void) strcpy(msg, command);
			(void) strcat(msg, ": ");
		}
		if (s)
			(void) strcat(msg, s);
		if (q)
			(void) strcat(msg, q);
		(void) strcat(msg, "\n");
		(void) write(2, msg, (size_t)strlen(msg));
	}
}

void
abend(const char *s, const char *q)
{
	if (Proc) {
		Flush();
		errmsg(s, q);
		clear_breakpoints(Proc);
		Prelease(Proc, created? PRELEASE_KILL : PRELEASE_CLEAR);
		procdel();
		(void) wait4all();
	} else {
		errmsg(s, q);
	}
	exit(2);
}

static int
wait4all()
{
	int i;
	pid_t pid;
	int rc = 0;
	int status;

	for (i = 0; i < 10; i++) {
		while ((pid = wait(&status)) != -1) {
			/* return exit() code of the created process */
			if (pid == created) {
				if ((rc = status&0xff) == 0)
					rc = (status>>8)&0xff;	/* exit()ed */
				else
					rc |= 0x80;		/* killed */
			}
		}
		if (errno != EINTR && errno != ERESTART)
			break;
	}

	if (i >= 10)	/* repeated interrupts */
		rc = 2;

	return (rc);
}

static void
letgo()
{
	(void) printf("%s\t*** process otherwise traced, releasing ...\n",
		pname);
}

/*
 * Test for empty set.
 * support routine used by isemptyset() macro.
 */
static int
is_empty(const uint32_t *sp,	/* pointer to set (array of int32's) */
	size_t n)		/* number of int32's in set */
{
	if (n) {
		do {
			if (*sp++)
				return (FALSE);
		} while (--n);
	}

	return (TRUE);
}

/*
 * OR the second set into the first.
 * The sets must be the same size.
 */
static void
or_set(uint32_t *sp1, const uint32_t *sp2, size_t n)
{
	if (n) {
		do {
			*sp1++ |= *sp2++;
		} while (--n);
	}
}
