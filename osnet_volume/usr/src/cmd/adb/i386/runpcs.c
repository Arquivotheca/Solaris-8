/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

/*
 * adb - subprocess running routines
 */

#ident "@(#)runpcs.c	1.25	98/01/20 SMI"

#define BPT_INIT
#include <stdio.h>
#include "adb.h"
#include <fcntl.h>
#include <procfs.h>
#include "ptrace.h"
#include <sys/wait.h>
#include <sys/errno.h>
#include "fpascii.h"
#include <link.h>
#include <sys/param.h>
#include <sys/auxv.h>
#include <sys/termios.h>
addr_t rtld_state_addr;
addr_t exec_entry;

/*
 * On a sparc, we must simulate single-stepping.  The only ptrace
 * calls that might try to single-step are in this file.  I've replaced
 * those calls with calls to adb_ptrace.  The following #define allows
 * ptrace to be called directly if not on a sparc.
 *
 * NOTE: The above is true now only for kadb.  adb single-steps using
 * the PTRACE_SINGLESTEP option of the bogus ptrace() interface to /proc.
 */
#if !defined(KADB)
#define adb_ptrace ptrace
#endif

#if defined(KADB)
struct bkpt *tookhwbkpt(void);
#endif
static void execbkpt();

struct	bkpt *bkpthead;
extern debugging;
extern use_shlib;
extern struct r_debug *ld_debug_addr;
extern struct r_debug ld_debug_struct;
extern Elf32_Dyn *dynam_addr;
extern Elf32_Dyn dynam;
extern char *rtld_path;
extern address_invalid;

#define SYM_LOADED ((addr_t)&(ld_debug_struct.ldd_sym_loaded) - (addr_t)&ld_debug_struct)
#define IN_DEBUGGER ((addr_t)&(ld_debug_struct.ldd_in_debugger) - (addr_t)&ld_debug_struct)

int	datalen;
char	debugr;
int	loopcnt;

#define BPOUT	0
#define BPIN	1
#define BPSTEP	2

int	bpstate = BPOUT;

addr_t userpc;
#ifdef sparc
addr_t usernpc;
#endif

/*
 * A breakpoint instruction is in the extern "bpt", defined in
 * the machine-dependent sun.h, sparc.h, or whatever.
 */

getsig(sig)
	int sig;
{
	return (expr(0) ? expv : sig);
}

runpcs(runmode, execsig)
	int runmode;
	int execsig;
{
	int rc = 0;
	register struct bkpt *bkpt;
	int changed_pc = 0;

	if (hadaddress) {
#ifdef INSTR_ALIGN_ERROR
		if( dot & INSTR_ALIGN_MASK ) {
			error( INSTR_ALIGN_ERROR );
		}
#endif INSTR_ALIGN_ERROR

		userpc = (addr_t)dot;
	}
	while (--loopcnt >= 0) {
		if (runmode == PTRACE_SINGLESTEP) {
			ss_setup(userpc);
		} else {
			if ((bkpt = tookwp()) || (bkpt = bkptlookup(userpc))) {
				execbkpt(bkpt, execsig);
				execsig = 0;
			}
			setbp();
		}

#ifdef KADB
		(void) adb_ptrace(runmode, pid, (int)userpc,
			execsig, changed_pc);
		if (runmode != PTRACE_SINGLESTEP) {
			bpwait(runmode);
			chkerr();
		}
#else
		if (runmode == PTRACE_SINGLESTEP || !tookwp()) {
			(void) adb_ptrace(runmode, pid, (int)userpc,
				execsig, changed_pc);
			bpwait(runmode);
			chkerr();
		}
#endif /* KADB */
#ifndef	KADB
		if (pid && use_shlib && userpc == exec_entry) {
		    if ((bkpt = bkptlookup(userpc))) {
			scan_linkmap();
			execbkpt(bkpt, execsig);
			bkpt->flag = 0;
			setbp();
			(void) adb_ptrace(runmode, pid, (int)userpc,
					    execsig, changed_pc);
			bpwait(runmode);
		    }
		}
		chkerr();
#endif KADB
		execsig = 0; delbp();

		/* Clobber any apocryphal temp breakpoints.  Keep the
		 * one at pc, though, so printpc() can find the real
		 * instruction without reading the child. (adb only!)
		 */
		for (bkpt = bkpthead; bkpt; bkpt = bkpt->nxtbkpt) {
#if	!defined(KADB)
			if (bkpt->flag & BKPT_TEMP && bkpt->loc != userpc)
#else
			if (bkpt->flag & BKPT_TEMP)
#endif
				bkpt->flag = 0;
		}
#if defined(KADB) && defined(i386)
		/*
		 * x86 hardware breakpoints stop after the cause for the bkpt.
		 * Also, the semantics of tookwp() is not exactly what we need.
		 * These differences from sun4u make it desirable to handle
		 * hardware breakpoints differently, thus, the following code.
		 */
		if ((bkpt = tookhwbkpt())) {
			if ((bkpt->comm[0] == '\n' || /* HACK HERE */
			    command(bkpt->comm, ':')) && --bkpt->count) {
				loopcnt++;
			} else {
				bkpt->count = bkpt->initcnt;
				rc = 0;
			}
		} else
#endif
		if (signo == 0 &&
		    ((bkpt = tookwp()) || (bkpt = bkptlookup(userpc))) &&
		    (!(bkpt->flag & BKPT_TEMP))) {
			if ((bkpt->comm[0] == '\n' || /* HACK HERE */
			    command(bkpt->comm, ':')) && --bkpt->count) {
				if (!iswp(bkpt->type)) {
					execbkpt(bkpt, execsig);
					execsig = 0;
				}
				loopcnt++;
			} else {
				bkpt->count = bkpt->initcnt;
				rc = 1;
			}
		} else {
			execsig = signo;
			rc = 0;
		}
		if (interrupted)
			break;
	}
	if (((bkpt = bkptlookup(userpc)) && (bkpt->flag & BKPT_TEMP)) ||
	    tookwp() != NULL)
		rc = 0;		/* So this doesn't look like a breakpoint. */
	return (rc);
}

void
endpcs()
{
	register struct bkpt *bp;
#ifndef KADB
	extern void free_tmpmallocs();
#endif

	if (pid) {
		exitproc();

		userpc = (addr_t)filhdr.e_entry;


		for (bp = bkpthead; bp; bp = bp->nxtbkpt)
			if (bp->flag)
				bp->flag = BKPTSET;
	}
#if !defined(KADB)
	free_shlib_map_ranges(&txtmap);
	free_shlib_map_ranges(&datmap);
#endif
	address_invalid = 1;
	bpstate = BPOUT;
#ifndef KADB
	free_tmpmallocs();
#endif
}


/*
 * execute the instruction at a breakpoint, by deleting all breakpoints,
 * and then single stepping (called when we're about to continue from one).
 */
static void
execbkpt(bp, execsig)
	struct bkpt *bp;
	int execsig;
{
	int ssaddr = (int)bp->loc;

	ss_setup(userpc);	/* set all brkpts but one to step past */

	/*
	 * If this is a watchpoint, we need to step "userpc", since
	 * bp->loc is the data address being watched, not a PC.
	 */
	if (iswp(bp->type))
		ssaddr = (int)userpc;

	/* 
	 * This line will call bpwait and step one instruction. This
	 * is done to get past the break point at the current pc.
	 */
	(void) adb_ptrace(PTRACE_SINGLESTEP, pid, ssaddr, execsig, 0);
#if defined(KADB)
	bp->flag = BKPTSET;
	delbp();	/* to fix a kadb bug with multiple breakpoints */
#else
	bpwait(PTRACE_SINGLESTEP);
	bp->flag |= BKPTSET;
#endif
	chkerr();
}

#ifndef KADB
extern char	**environ;

static
doexec()
{
	char *argl[BUFSIZ/8];
	char args[BUFSIZ];
	char *p = args, **ap = argl, *filnam;

	*ap++ = symfil;
	do {
		if (rdc()=='\n')
			break;
		*ap = p;
		while (!isspace(lastc)) {
			*p++=lastc;
			(void) readchar();
		}
		*p++ = '\0';
		filnam = *ap + 1;
		if (**ap=='<') {
			(void) close(0);
			if (open(filnam, 0) < 0) {
				printf("%s: cannot open\n", filnam);
				exit(0);
			}
		} else if (**ap=='>') {
			(void) close(1);
			if (creat(filnam, 0666) < 0) {
				printf("%s: cannot create\n", filnam);
				exit(0);
			}
		} else {
	 		ap++;
			if (ap >= &argl[sizeof (argl)/sizeof (argl[0]) - 2]) {
				printf("too many arguments\n");
				exit(0);
			}
		}
	} while (lastc != '\n');
	*ap++ = 0;
	flushbuf();
	execve(symfil, argl, environ);
	perror(symfil);
}
#endif !KADB

void
delbp()
{
	register struct bkpt *bp;

	ptrace(PTRACE_CLRBKPT, pid, 0, 0);
	if (bpstate == BPOUT)
		return;
	for (bp = bkpthead; bp; bp = bp->nxtbkpt) {
		if (bp->flag && (bp->flag & BKPT_ERR) == 0 && !iswp(bp->type) &&
		    bp->type != BPDEFR)
			(void) writeproc(bp->loc, (char *)&bp->ins, SZBPT);
	}
	bpstate = BPOUT;
}

setbrkpt(bp)
	register struct bkpt *bp;
{
	if (bp->type == BPACCESS) {
		if (!(bp->flag & (BKPT_RDACTIVE|BKPT_WRACTIVE)))
			ptrace(PTRACE_SETAC, pid, bp->loc, bp->len, 0);
		else if (!(bp->flag & BKPT_WRACTIVE))
			ptrace(PTRACE_SETWR, pid, bp->loc, bp->len, 0);
	} else if (bp->type == BPWRITE) {
		if (!(bp->flag & BKPT_WRACTIVE))
			ptrace(PTRACE_SETWR, pid, bp->loc, bp->len, 0);
	} else if (bp->type == BPDBINS) {
		if (!(bp->flag & BKPT_EXACTIVE))
			ptrace(PTRACE_SETBPP, pid, bp->loc, bp->len, 0); 
#if defined(KADB)
	} else if (bp->type == BPX86IO) {
		ptrace(PTRACE_X86IO, pid, bp->loc, bp->len, 0);
	} else if (bp->type == BPDEFR) {
		return (0);
#endif
	} else {
		if (readproc(bp->loc, (char *)&bp->ins, SZBPT) != SZBPT
		    || writeproc(bp->loc, (char *)&bpt, SZBPT) != SZBPT) {
			bp->flag |= BKPT_ERR;   /* turn on err flag */
			if (use_shlib && address_invalid)
				return;
			prints("cannot set breakpoint: ");
			psymoff(bp->loc, ISYM, "\n");
		} else {
			bp->flag &= ~BKPT_ERR;  /* turn off err flag */
		}
	}
}

void
setbp()
{
	register struct bkpt *bp;

	if (bpstate == BPIN) {
		return;
	}
	for (bp = bkpthead; bp; bp = bp->nxtbkpt) {
		if (bp->flag) {
			setbrkpt(bp);
		}
	}
	bpstate = BPIN;
}

static void
setwp()
{
	register struct bkpt *bp;

	if (bpstate == BPIN)
		return;
	for (bp = bkpthead; bp; bp = bp->nxtbkpt) {
		if (bp->flag && iswp(bp->type)) {
			setbrkpt(bp);
		}
	}
}

static
unmarkwp()
{
	register struct bkpt *bp;

	for (bp = bkpthead; bp; bp = bp->nxtbkpt) {
		if (bp->flag && iswp(bp->type)) {
			bp->flag &=
				~(BKPT_EXACTIVE|BKPT_RDACTIVE|BKPT_WRACTIVE);
		}
	}
}

void
resetcounts()
{
	register struct bkpt *bp;

	for (bp = bkpthead; bp; bp = bp->nxtbkpt)
		bp->count = bp->initcnt;
}

/*
 * Called to set breakpoints up for executing a single step at addr
 */
void
ss_setup(addr)
	addr_t addr;
{
#ifdef KADB
	/*
	 * For kadb we need to set all the current breakpoints except
	 * for those at addr.  This is needed so that we can hit break
	 * points in interrupt and trap routines that we may run into
	 * during a single step.
	 */
	register struct bkpt *bp;

	for (bp = bkpthead; bp; bp = bp->nxtbkpt)
		if (bp->flag && bp->loc != addr)
			setbrkpt(bp);
	bpstate = BPSTEP;       /* force not BPOUT or BPIN */
#else /* !KADB */
	/*
	 * In the UNIX case, we simply delete all breakpoints
	 * before doing the single step.  Don't have to worry
	 * about asynchronous hardware events here.
	 */
	delbp();
	/*
	 * However, we have to keep all the watchpoints in place,
	 * except for the one that caused the stop.
	 */
	setwp();
#endif /* !KADB */
}

#ifndef KADB
/*
 * If the child is in another process group, ptrace will not always inform
 * adb (via "wait") when a signal typed at the terminal has occurred.
 * (Actually, if the controlling tty for the child is different from
 * the adb's controlling tty the notification fails to happen.)  Thus,
 * adb should catch the signal and if the child is in the same process
 * group, simply return (ptrace will inform adb of the signal via "wait").
 * If the child is in a different process group, adb should issue a "kill"
 * of that signal to the child.  (This will cause ptrace to "wake up" and
 * inform adb that the child received a signal.)  [Note that if the
 * child's process group is different than that of adb, the controlling
 * tty's process group is also different.]
 */

/* getpgid() in SVr4 is what getpgrp() was in SunOS4.0 and before */
#define	getpgrp getpgid

void sigint_handler()
{

	if (getpgrp(pid) != adb_pgrpid) {
		kill(pid, SIGINT);
	}
}

void sigquit_handler()
{

	if (getpgrp(pid) != adb_pgrpid) {
		kill(pid, SIGQUIT);
	}
}

void
bpwait(mode)
int mode;
{
	register unsigned w;
	extern char *corfil;
	int stat, pc_adjust = 0;
	extern adb_debug;
	extern char *corfil;
	void (*oldisig)();
	void (*oldqsig)();
        sigset_t new_mask, old_mask;

	tbia();
rewait:
	oldisig = (void (*)(int)) signal(SIGINT, sigint_handler);
	oldqsig = (void (*)(int)) signal(SIGQUIT, sigquit_handler);
	while ((w = proc_wait(pid, &stat)) != pid)
		if (w == -1 && errno != EINTR)
			break;

	/* Block SIGTTOU temporarily, since tcsetpgrp would generate one */
	sigprocmask(SIG_SETMASK, NULL, &new_mask);
	sigaddset(&new_mask, SIGTTOU);
	sigprocmask(SIG_SETMASK, &new_mask, &old_mask);

	/* Set foreground process group for terminal */
	if (adb_pgrpid > 0)
	    tcsetpgrp(0, adb_pgrpid);
	sigprocmask(SIG_SETMASK, &old_mask, NULL); /* restore signal mask */

	signal(SIGINT, oldisig);
	signal(SIGQUIT, oldqsig);
#ifdef DEBUG
	if (debugging >= 1) {
		prints("wait: ");
		prmode(mode);
	}
#endif
	if (w == -1) {
		pid = 0;
		resetcounts();
		errflg = "wait error: process disappeared";
		return;
	}
	if ((stat & WSTOPFLG) != WSTOPFLG) {
		sigcode = 0;
		if (signo = (stat & WSTOPFLG))
			sigprint(signo);
		if (stat & WNOWAIT) {
			printf(" - core dumped\n");
			(void) close(fcor);
			corfil = "core";
			pid = 0; 
			setcor();
		} else {
			pid = 0;
		}
		resetcounts();
		errflg = "process terminated";
	} else {
		signo = stat>>8;
		sigcode = (Prstatus.pr_lwp.pr_why == PR_SIGNALLED) ?
			Prstatus.pr_lwp.pr_info.si_code : TRAP_TRACE;

		/*
		 * On the sparc there is no need to get the registers
		 * again as the _bogus_ local ptrace has left them in
		 * the global struct.
		 */
		if (ptrace(PTRACE_GETREGS, pid, &Prstatus.pr_lwp.pr_reg, 0) == -1 &&
									errno) {
			perror(corfil);
		}
		core_to_regs();
		if (fpa_avail) {
			stat = ptrace(PTRACE_GETFPREGS, pid,&Prfpregs, 0);
			if (stat == -1 && errno) {
				perror(corfil);
			}
			db_fprdump(stat);
		}

		if (adb_debug)
			sigprint(signo);

		if (signo != SIGTRAP) {
			if (mode == PTRACE_CONT && sigpass[signo]) {
				int rtn;
				rtn = ptrace(PTRACE_CONT, pid,
							(int *) 1, signo);
				if (rtn == -1 && errno) {
					perror(symfil);
				}
				goto rewait;
			} /* else */
			sigprint(signo);
			unmarkwp();
		} else {
			struct bkpt *bp;
			signo = 0;
			/*
			 * if pc was advanced during execution of the
			 * breakpoint (probably by the size of the
			 * breakpoint instruction) push it back to the
			 * address of the instruction, so we will later
			 * recognize it as a tripped trap.
			 */
			switch (Prstatus.pr_lwp.pr_info.si_code) {
			case TRAP_BRKPT:
				db_printf(2, "bpwait: BREAKPOINT");
				pc_adjust = -SZBPT;
				/* FALLTHROUGH */
			case TRAP_TRACE:
				unmarkwp();
				dot = (addr_t)readreg(Reg_PC) + pc_adjust;
				break;
			case TRAP_RWATCH:
				if (!trapafter && (bp = tookwp()))
					bp->flag |= BKPT_RDACTIVE;
				dot = (addr_t)Prstatus.pr_lwp.pr_info.si_addr;
				break;
			case TRAP_WWATCH:
				if (!trapafter && (bp = tookwp()))
					bp->flag |= BKPT_WRACTIVE;
				dot = (addr_t)Prstatus.pr_lwp.pr_info.si_addr;
				break;
			case TRAP_XWATCH:
				if (!trapafter && (bp = tookwp()))
					bp->flag |= BKPT_EXACTIVE;
				dot = (addr_t)Prstatus.pr_lwp.pr_info.si_addr;
				break;
			}
		}
	}
	flushbuf();
	userpc = (addr_t) readreg(REG_PC);
	if (pc_adjust)
		writereg(REG_PC, userpc + pc_adjust);
}  /* end of NON-KADB version of bpwait */

#endif !KADB




#ifdef KADB
void
bpwait ( int mode )		/* at386 version */
{
	extern int istrap;
	extern int ishtrap;
	extern struct regs adb_regs;

	doswitch();	/* wait for something to happen */
	ptrace(PTRACE_GETREGS, pid, &adb_regs, 0, 0);
	signo = 0;
	regs_to_core( );
	flushbuf();
	userpc = (addr_t)readreg(REG_PC);
	if (istrap) {
		userpc += PCFUDGE;
		setreg ( REG_PC, userpc );
		ptrace ( PTRACE_SETREGS, pid, &adb_regs, 0, 0 );
		istrap = 0;		/* don't need this anymore; */
	} else if (ishtrap) {
		ishtrap = 0;
		setdr6(0);
	}
#ifdef DEBUG
	if (debugging >= 1) {
		printf("User PC is %X [fudge %D]\n", userpc, pcfudge);
	}
#endif

	tryabort(0);		/* set interrupt if user is trying to abort */
	ttysync();

}	/* end of at386 version of bpwait */

/*
 * Call this routine to go to debugger prompt.
 */
void
trap_to_debugger()
{
	asm("int	$20");
}

int	saved_GS;

/*
 * Save and restore the GS register for long jumps.
 */
void
set_GS(int new_GS)
{
	asm("pushl %gs");
	asm("popl  %eax");
	asm("movl  %eax,saved_GS");
	asm("movw 8(%ebp), %gs");
}

void
set_jmp_env()
{
	set_GS(KGSSEL);
}

void
restore_GS(int old_GS)
{
	asm("movl 8(%ebp), %eax");
	asm("movw %eax, %gs");
}

void
reset_jmp_env()
{
	restore_GS(saved_GS);
}

/*
 * Dummy routines that are used in sun4u/sparc for setting up the
 * kadb exection environment.  See comments in the sparc version
 * of this file for more details.
 */
void
set_kadb_dbg_env()
{}

void
clear_kadb_dbg_env()
{}

#endif KADB

#ifdef VFORK
nullsig()
{
}
#endif

void
setup()
{
	extern int adb_debug;

#ifndef KADB
	(void) close(fsym); fsym = -1;
	flushbuf();
	newsubtty();
	endpcs();
#ifndef VFORK
	if ((pid = fork()) == 0)
#else
	if ((pid = vfork()) == 0)
#endif
	{
		(void) ptrace(PTRACE_TRACEME, 0, 0, 0);
		signal(SIGINT,  sigint);
		signal(SIGQUIT, sigqit);
		closettyfd();
		doexec();
		_exit(0);
	}
	if (pid == -1)
		error("try again");

	bpwait(PTRACE_TRACEME);
	lp[0] = '\n'; lp[1] = 0;
	fsym = open(symfil, wtflag);
	if (fsym < 0)
		perror(symfil);
	if (errflg) {
		printf("%s: cannot execute\n", symfil);
		endpcs();
		error((char *)0);
	}
	if (use_shlib){
	    read_in_rtld(); /*userpc&pagemask*/
	}
#endif !KADB
	bpstate = BPOUT;
}

#ifdef DEBUG
static
prmode(mode)
int mode;
{
	register char *m;

	switch (mode) {
	case PTRACE_TRACEME:
		m = "PTRACE_TRACEME\n";
		break;
	case PTRACE_CONT:
		m = "PTRACE_CONT\n";
		break;
	case PTRACE_SINGLESTEP:
		m = "PTRACE_SINGLESTEP\n";
		break;
	case PTRACE_KILL:
		m = "PTRACE_KILL\n";
		break;
	default:
		printf("MODE[ %d ]\n", mode);
		return;
	}
	prints(m);
}
#endif DEBUG

#if defined(sparc) || defined(i386)

db_fprdump ( stat ) {
  int *fptr, rn, rnx;
  extern int adb_debug;

	if( adb_debug == 0 ) return;
#if defined(sparc)
	printf( "Dump of FP register area:\n" );
	printf( "ptrace returned %d; ", stat );

	if( stat < 0 ) {
	    perror( "adb -- ptrace( GETFPREGS )" );
	} else {
	    printf( "\n" );
	}

	for( rn = 0; rn < 32; rn += 4 ) {
	    printf( "Reg F%d:  ", rn );

	    fptr = (int *) &Prfpregs.pr_fr.pr_regs[rn];

	    printf( "%8X %8X %8X %8X\n", fptr[0], fptr[1], fptr[2], fptr[3] );
	}
	printf( "End of dump of FP register area\n" );
#elif defined(i386)
#endif
}
#endif sparc || i386
