/*
 * Copyright (c) 1991, 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)syscall.c	1.49	99/06/21 SMI"

#include <sys/param.h>
#include <sys/vmparam.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/user.h>
#include <sys/psw.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/modctl.h>
#include <sys/var.h>
#include <sys/inline.h>
#include <sys/syscall.h>
#include <sys/ucontext.h>
#include <sys/cpuvar.h>
#include <sys/siginfo.h>
#include <sys/trap.h>
#include <sys/vtrace.h>
#include <sys/sysinfo.h>
#include <sys/procfs.h>
#include <c2/audit.h>
#include <sys/modctl.h>
#include <sys/aio_impl.h>
#include <sys/tnf.h>
#include <sys/tnf_probe.h>
#include <sys/copyops.h>

#ifdef SYSCALLTRACE
int syscalltrace = 0;
static kmutex_t systrace_lock;		/* syscall tracing lock */
#else
#define	syscalltrace 0
#endif /* SYSCALLTRACE */

typedef	longlong_t (*llfcn_t)();	/* function returning long long */

int pre_syscall(void);
void post_syscall(long rval1, long rval2);
static krwlock_t *lock_syscall(uint_t code);
static void deferred_singlestep_trap(caddr_t);

/*
 * Arrange for the real time profiling signal to be dispatched.
 */
void
realsigprof(int sysnum, int error)
{
	proc_t *p;
	klwp_t *lwp;

	if (curthread->t_rprof->rp_anystate == 0)
		return;
	p = ttoproc(curthread);
	lwp = ttolwp(curthread);
	mutex_enter(&p->p_lock);
	if (sigismember(&p->p_ignore, SIGPROF) ||
	    sigismember(&curthread->t_hold, SIGPROF)) {
		mutex_exit(&p->p_lock);
		return;
	}
	lwp->lwp_siginfo.si_signo = SIGPROF;
	lwp->lwp_siginfo.si_code = PROF_SIG;
	lwp->lwp_siginfo.si_errno = error;
	hrt2ts(gethrtime(), &lwp->lwp_siginfo.si_tstamp);
	lwp->lwp_siginfo.si_syscall = sysnum;
	lwp->lwp_siginfo.si_nsysarg =
	    (sysnum > 0 && sysnum < NSYSCALL) ? sysent[sysnum].sy_narg : 0;
	lwp->lwp_siginfo.si_fault = lwp->lwp_lastfault;
	lwp->lwp_siginfo.si_faddr = lwp->lwp_lastfaddr;
	lwp->lwp_lastfault = 0;
	lwp->lwp_lastfaddr = NULL;
	sigtoproc(p, curthread, SIGPROF);
	mutex_exit(&p->p_lock);
	ASSERT(lwp->lwp_cursig == 0);
	if (issig(FORREAL))
		psig();
	mutex_enter(&p->p_lock);
	lwp->lwp_siginfo.si_signo = 0;
	bzero(curthread->t_rprof, sizeof (*curthread->t_rprof));
	mutex_exit(&p->p_lock);
}

/*
 * Special version of copyin() for benefit of watchpoints.
 * If watchpoints are active, don't make copying in of
 * system call arguments take a read watchpoint trap.
 */
int
copyin_args(int *sp, long *ap, uint_t nargs)
{
	int mapped = 0;
	int rc;

	nargs *= sizeof (int);

	if (curproc->p_warea)		/* watchpoints in effect */
		mapped = pr_mappage((caddr_t)sp, nargs, S_READ, 1);
	rc = default_copyin(sp, ap, nargs);
	if (mapped)
		pr_unmappage((caddr_t)sp, nargs, S_READ, 1);

	return (rc);
}

/*
 * Error handler for system calls where arg copy gets fault.
 */
static longlong_t
syscall_err()
{
	return (0);
}

#ifdef NOT_USING_i86pc_ml_VERSION
/*
 * Called from syscall() when a system call occurs.
 * 	Sets up the args and returns a pointer to the handler.
 */
llfcn_t
syscall_entry(int *argp)
{
	kthread_id_t t = curthread;
	klwp_t *lwp = ttolwp(t);
	struct regs *rp = lwptoregs(lwp);
	unsigned int code;
	struct sysent *callp;
	int	error = 0;
	uint_t	nargs;

	lwp->lwp_ru.sysc++;
	ASSERT(curthread->t_schedflag & TS_DONT_SWAP);
	CPU_STAT_ADDQ(CPU, cpu_sysinfo.syscall, 1);

	lwp->lwp_eosys = NORMALRETURN;	/* assume this will be normal */
	code = rp->r_eax;
	t->t_sysnum = (short)code;
	if (code >= NSYSCALL) {
		callp = &nosys_ent;
	} else {
		callp = &sysent[code];
	}

	/*
	 * Set lwp_ap to point to the args, even if none are needed for this
	 * system call.  This is for the loadable-syscall case where the
	 * number of args won't be known until the system call is loaded, and
	 * also maintains a non-NULL lwp_ap setup for get_syscall_args().
	 */
	lwp->lwp_ap = argp;		/* for get_syscall_args */

	if ((t->t_pre_sys | syscalltrace) != 0) {
		error = pre_syscall();
		/*
		 * Reset lwp_ap so that the args will be refetched if
		 * the lwp stopped for /proc purposes in pre_syscall().
		 */
		lwp->lwp_ap = argp;
		if (error) {
			(void) set_errno(error);
			return (syscall_err);	/* use dummy handler */
		}
	}

	/*
	 * Fetch the system call arguments.
	 * Note that for loadable system calls the number of arguments required
	 * may not be known at this point, and will be zero if the system call
	 * was never loaded.  Once the system call has been loaded, the number
	 * of args is not allowed to be changed.
	 */
	nargs = (uint_t)callp->sy_narg;
	if (nargs != 0) {
		int *sp = (int *)rp->r_uesp;

		ASSERT(nargs <= MAXSYSARGS);
		/*
		 * sp points to the return addr on the user's
		 * stack. bump it up to the actual args.
		 */
		sp++;
		if (copyin_args(sp, argp, nargs)) {
			(void) set_errno(EFAULT);
			return (syscall_err);	/* use dummy handler */
		}
	}
	/*
	 * Save arg count in case of loadable_syscall().
	 */
	argp[MAXSYSARGS] = nargs;
	return (callp->sy_callc);	/* return syscall handler for caller */
}
#endif	/* NOT_USING_i86pc_ml_VERSION */


void
syscall_exit(long rval1, long rval2)
{
	kthread_id_t t = curthread;
	klwp_t *lwp = ttolwp(t);
	struct regs *rp = lwptoregs(lwp);

	/*
	 * Handle signals and other post-call events if necessary.
	 */
	if ((t->t_post_sys_ast | syscalltrace) == 0) {
		/*
		 * Normal return.
		 * Clear error indication and set return values.
		 */
		rp->r_efl &= ~PS_C;	/* reset carry bit */
		rp->r_eax = rval1;
		rp->r_edx = rval2;
		ttolwp(t)->lwp_state = LWP_USER;
		t->t_sysnum = 0;
	} else {
		post_syscall(rval1, rval2);
	}
	t->t_sysnum = 0;		/* invalidate args */
}

int
pre_syscall()
{
	kthread_id_t t = curthread;
	unsigned code = t->t_sysnum;
	int error = 0;
	klwp_t *lwp = ttolwp(t);
	proc_t *p = ttoproc(t);
	int	repost = 0;

	t->t_pre_sys = 0;

	ASSERT(t->t_schedflag & TS_DONT_SWAP);

	if (t->t_proc_flag & TP_MSACCT) {
		(void) new_mstate(t, LMS_SYSTEM);	/* in syscall */
		repost = 1;
	}

	/*
	 * Make sure the thread is holding the latest credentials for the
	 * process.  The credentials in the process right now apply to this
	 * thread for the entire system call.
	 */
	if (t->t_cred != p->p_cred) {
		crfree(t->t_cred);
		t->t_cred = crgetcred();
	}

	/*
	 * From the proc(4) manual page:
	 * When entry to a system call is being traced, the traced process
	 * stops after having begun the call to the system but before the
	 * system call arguments have been fetched from the process.
	 */
	if (PTOU(p)->u_systrap) {
		if (prismember(&PTOU(p)->u_entrymask, code)) {
			mutex_enter(&p->p_lock);
			/*
			 * Recheck stop condition, now that lock is held.
			 */
			if (PTOU(p)->u_systrap &&
			    prismember(&PTOU(p)->u_entrymask, code)) {
				stop(PR_SYSENTRY, code);
			}
			mutex_exit(&p->p_lock);
		}
		repost = 1;
	}

	if (lwp->lwp_sysabort) {
		/*
		 * lwp_sysabort may have been set via /proc while the process
		 * was stopped on PR_SYSENTRY.  If so, abort the system call.
		 * Override any error from the copyin() of the arguments.
		 */
		lwp->lwp_sysabort = 0;
		t->t_pre_sys = 1;	/* repost anyway */
		error = EINTR;
	}

#ifdef C2_AUDIT
	if (audit_active) {	/* begin auditing for this syscall */
		error = audit_start(T_SYSCALL, code, error, lwp);
		repost = 1;
	}
#endif /* C2_AUDIT */

#ifndef NPROBE
	/* Kernel probe */
	if (tnf_tracing_active) {
		TNF_PROBE_1(syscall_start, "syscall thread", /* CSTYLED */,
			tnf_sysnum,	sysnum,		t->t_sysnum);
		t->t_post_sys = 1;	/* make sure post_syscall runs */
		repost = 1;
	}
#endif /* NPROBE */

#ifdef SYSCALLTRACE
	if (error == 0 && syscalltrace) {
		int i;
		long *ap;
		char *cp;
		char *sysname;
		struct sysent *callp;

		if (code >= NSYSCALL)
			callp = &nosys_ent;	/* nosys has no args */
		else
			callp = &sysent[code];
		(void) save_syscall_args();
		mutex_enter(&systrace_lock);
		printf("%d: ", p->p_pid);
		if (code >= NSYSCALL)
			printf("0x%x", code);
		else {
			sysname = mod_getsysname(code);
			printf("%s[0x%x]", sysname == NULL ? "NULL" :
			    sysname, code);
		}
		cp = "(";
		for (i = 0, ap = lwp->lwp_ap; i < callp->sy_narg; i++, ap++) {
			printf("%s%lx", cp, *ap);
			cp = ", ";
		}
		if (i)
			printf(")");
		printf(" %s id=0x%p\n", PTOU(p)->u_comm, curthread);
		mutex_exit(&systrace_lock);
	}
#endif /* SYSCALLTRACE */

	/*
	 * If there was a continuing reason for pre-syscall processing,
	 * set the t_pre_sys flag for the next system call.
	 */
	if (repost)
		t->t_pre_sys = 1;
	lwp->lwp_error = 0;	/* for old drivers */
	return (error);
}


/*
 * Post-syscall processing.  Perform abnormal system call completion
 * actions such as /proc tracing, profiling, signals, preemption, etc.
 *
 * This routine is called only if t_post_sys, t_sig_check, or t_astflag is set.
 * Any condition requiring pre-syscall handling must set one of these.
 * If the condition is persistent, this routine will repost t_post_sys.
 */
void
post_syscall(long rval1, long rval2)
{
	kthread_id_t t = curthread;
	klwp_t *lwp = ttolwp(t);
	proc_t *p = ttoproc(t);
	struct regs *rp = lwptoregs(lwp);
	uint_t	code = t->t_sysnum;
	uint_t	error = lwp->lwp_errno;
	int	repost = 0;

	t->t_post_sys = 0;

	/*
	 * Code can be zero if this is a new LWP returning after a fork(),
	 * other than the one which matches the one in the parent which called
	 * fork.  In these LWPs, skip most of post-syscall activity.
	 */
	if (code == 0)
		goto sig_check;
#ifdef C2_AUDIT
	if (audit_active) {	/* put out audit record for this syscall */
		rval_t	rval;

		/* XX64 -- truncation of 64-bit return values? */
		rval.r_val1 = (int)rval1;
		rval.r_val2 = (int)rval2;
		audit_finish(T_SYSCALL, code, error, &rval);
		repost = 1;
	}
#endif /* C2_AUDIT */

	if (lwp->lwp_eosys == NORMALRETURN) {

		if (error) {
			int sig;
#ifdef SYSCALLTRACE
			if (syscalltrace) {
				mutex_enter(&systrace_lock);
				printf("%d: error=%d, id 0x%p\n",
				    p->p_pid, error, curthread);
				mutex_exit(&systrace_lock);
			}
#endif /* SYSCALLTRACE */
			if (error == EINTR && t->t_activefd.a_stale)
				error = EBADF;
			if (error == EINTR &&
			    (sig = lwp->lwp_cursig) != 0 &&
			    sigismember(&PTOU(p)->u_sigrestart, sig) &&
			    PTOU(p)->u_signal[sig - 1] != SIG_DFL &&
			    PTOU(p)->u_signal[sig - 1] != SIG_IGN)
				error = ERESTART;
			rp->r_eax = error;
			rp->r_efl |= PS_C;	/* set carry bit */
		} else {
#ifdef SYSCALLTRACE
			if (syscalltrace) {
				mutex_enter(&systrace_lock);
				printf(
				    "%d: r_val1=0x%lx, r_val2=0x%lx, id 0x%p\n",
				    p->p_pid, rval1, rval2, curthread);
				mutex_exit(&systrace_lock);
			}
#endif /* SYSCALLTRACE */
			rp->r_efl &= ~PS_C;	/* reset carry bit */
			rp->r_eax = rval1;
			rp->r_edx = rval2;
		}
	}

	/*
	 * From the proc(4) manual page:
	 * When exit from a system call is being traced, the traced process
	 * stops on completion of the system call just prior to checking for
	 * signals and returning to user level.  At this point all return
	 * values have been stored into the traced process's saved registers.
	 */
	if (PTOU(p)->u_systrap) {
		if (prismember(&PTOU(p)->u_exitmask, code)) {
			mutex_enter(&p->p_lock);
			/*
			 * Recheck stop conditions now that p_lock is held.
			 */
			if (PTOU(p)->u_systrap &&
			    prismember(&PTOU(p)->u_exitmask, code)) {
				stop(PR_SYSEXIT, code);
			}
			mutex_exit(&p->p_lock);
		}
		repost = 1;
	}

	/*
	 * If we are the parent returning from a successful
	 * vfork, wait for the child to exec or exit.
	 * This code must be here and not in the bowels of the system
	 * so that /proc can intercept exit from vfork in a timely way.
	 */
	if (code == SYS_vfork && rp->r_edx == 0 && error == 0)
		vfwait((pid_t)rval1);

	/*
	 * If profiling is active, bill the current PC in user-land
	 * and keep reposting until profiling is disabled.
	 */
	if (p->p_prof.pr_scale) {
		if (lwp->lwp_oweupc)
			profil_tick(rp->r_eip);
		repost = 1;
	}

	t->t_sysnum = 0;		/* no longer in a system call */

sig_check:

#ifdef notdef		/* done in syscall entry */
	/*
	 * Reset flag for next time.
	 * We must do this after stopping on PR_SYSEXIT
	 * because /proc uses the information in lwp_eosys.
	 */
	lwp->lwp_eosys = NORMALRETURN;
#endif
	clear_stale_fd();

	if (t->t_astflag | t->t_sig_check) {
		/*
		 * Turn off the AST flag before checking all the conditions that
		 * may have caused an AST.  This flag is on whenever a signal or
		 * unusual condition should be handled after the next trap or
		 * syscall.
		 */
		astoff(t);
		t->t_sig_check = 0;

		/*
		 * for kaio requests that are on the per-process poll queue,
		 * aiop->aio_pollq, they're AIO_POLL bit is set, the kernel
		 * should copyout their result_t to user memory. by copying
		 * out the result_t, the user can poll on memory waiting
		 * for the kaio request to complete.
		 */
		if (p->p_aio)
			aio_cleanup(0);
		/*
		 * If this LWP was asked to hold, call holdlwp(), which will
		 * stop.  holdlwps() sets this up and calls pokelwps() which
		 * sets the AST flag.
		 *
		 * Also check TP_EXITLWP, since this is used by fresh new LWPs
		 * through lwp_rtt().  That flag is set if the lwp_create(2)
		 * syscall failed after creating the LWP.
		 */
		if (ISHOLD(p) || (t->t_proc_flag & TP_EXITLWP))
			holdlwp();

		/*
		 * All code that sets signals and makes ISSIG_PENDING
		 * evaluate true must set t_sig_check afterwards.
		 */
		if (ISSIG_PENDING(t, lwp, p)) {
			if (issig(FORREAL))
				psig();
			t->t_sig_check = 1;	/* recheck next time */
		}

		if (t->t_rprof != NULL && t->t_rprof->rp_anystate != 0) {
			realsigprof(code, error);
			t->t_sig_check = 1;	/* recheck next time */
		}

	}

	/*
	 * If a single-step trap occurred on a syscall (see trap())
	 * recognize it now.
	 */
	if (lwp->lwp_pcb.pcb_flags & DEBUG_PENDING)
		deferred_singlestep_trap((caddr_t)rp->r_eip);

#ifndef NPROBE
	/* Kernel probe */
	if (tnf_tracing_active) {
		TNF_PROBE_3(syscall_end, "syscall thread", /* CSTYLED */,
			tnf_long,	rval1,		rval1,
			tnf_long,	rval2,		rval2,
			tnf_long,	errno,		(long)error);
		repost = 1;
	}
#endif /* NPROBE */

	/*
	 * Set state to LWP_USER here so preempt won't give us a kernel
	 * priority if it occurs after this point.  Call CL_TRAPRET() to
	 * restore the user-level priority.
	 *
	 * It is important that no locks (other than spinlocks) be entered
	 * after this point before returning to user mode (unless lwp_state
	 * is set back to LWP_SYS).
	 *
	 * XXX Sampled times past this point are charged to the user.
	 */
	lwp->lwp_state = LWP_USER;
	if (t->t_proc_flag & TP_MSACCT) {
		(void) new_mstate(t, LMS_USER);
		repost = 1;
	}
	if (t->t_trapret) {
		t->t_trapret = 0;
		thread_lock(t);
		CL_TRAPRET(t);
		thread_unlock(t);
	}
	if (CPU->cpu_runrun)
		preempt();

	lwp->lwp_errno = 0;		/* clear error for next time */

	/*
	 * If there was a continuing reason for post-syscall processing,
	 * set the t_post_sys flag for the next system call.
	 */
	if (repost)
		t->t_post_sys = 1;
}

/*
 * Called from post_syscall() when a deferred singlestep is to be taken.
 */
static void
deferred_singlestep_trap(caddr_t pc)
{
	proc_t *p = ttoproc(curthread);
	klwp_id_t lwp = ttolwp(curthread);
	pcb_t *pcb = &lwp->lwp_pcb;
	uint_t fault = 0;
	k_siginfo_t siginfo;

	bzero(&siginfo, sizeof (siginfo));

	if (pcb->pcb_flags & WATCH_STEP)
		fault = undo_watch_step(&siginfo);
	else {	/* NORMAL_STEP or user set %efl */
		siginfo.si_signo = SIGTRAP;
		siginfo.si_code = TRAP_TRACE;
		siginfo.si_addr  = pc;
		fault = FLTTRACE;
	}
	pcb->pcb_flags &= ~(DEBUG_PENDING|NORMAL_STEP|WATCH_STEP);

	if (fault) {
		/*
		 * Remember the fault and fault adddress
		 * for real-time (SIGPROF) profiling.
		 */
		lwp->lwp_lastfault = fault;
		lwp->lwp_lastfaddr = siginfo.si_addr;
		/*
		 * If a debugger has declared this fault to be an
		 * event of interest, stop the lwp.  Otherwise just
		 * deliver the associated signal.
		 */
		if (prismember(&p->p_fltmask, fault) &&
		    stop_on_fault(fault, &siginfo) == 0)
			siginfo.si_signo = 0;
	}

	if (siginfo.si_signo)
		trapsig(&siginfo, 1);
}

/*
 * nonexistent system call-- signal lwp (may want to handle it)
 * flag error if lwp won't see signal immediately
 */
longlong_t
nosys()
{
	psignal(ttoproc(curthread), SIGSYS);
	return (set_errno(ENOSYS));
}


/*
 * Get the arguments to the current system call.
 *	lwp_ap should point at the args in lwp_arg, where they should be
 *	saved by save_syscall_args(), in pre_syscall() or post_syscall()
 *	before stopping for /proc.
 *
 * 	If called for a sleeping lwp, the syscall args might not have been
 *	saved, and in that case, the args on the kernel stack are only
 *	approximate (may have been changed by the handler).
 */
uint_t
get_syscall_args(klwp_t *lwp, long *argp, int *nargsp)
{
	kthread_id_t	t = lwptot(lwp);
	uint_t	code = t->t_sysnum;
	long	*ap;
	int	nargs;

	if (code != 0 && code < NSYSCALL) {
		nargs = sysent[code].sy_narg;
		ASSERT(nargs <= MAXSYSARGS);

		*nargsp = nargs;
		ap = lwp->lwp_ap;
		while (nargs-- > 0)
			*argp++ = *ap++;
	} else {
		*nargsp = 0;
	}
	return (code);
}

/*
 * Save_syscall_args - copy the users args prior to changing the stack or
 * stack pointer.  This is so /proc will be able to get a valid copy of the
 * args from the user stack even after the user stack has been changed.
 * Note that the kernel stack copy of the args may also have been changed by
 * a system call handler which takes C-style arguments.
 *
 * Note that this may be called by stop() from trap().  In that case t_sysnum
 * will be zero (syscall_exit clears it), so no args will be copied.
 */
int
save_syscall_args()
{
	kthread_id_t	t = curthread;
	klwp_t		*lwp = ttolwp(t);
	uint_t		code = t->t_sysnum;
	uint_t		nargs;

	if (lwp->lwp_ap == lwp->lwp_arg || code == 0)
		return (0);		/* args already saved or not needed */

	if (code >= NSYSCALL) {
		nargs = 0;		/* illegal syscall */
	} else {
		struct sysent *callp;

		callp = &sysent[code];
		nargs = callp->sy_narg;
		if (LOADABLE_SYSCALL(callp) && nargs == 0) {
			krwlock_t	*module_lock;

			/*
			 * Find out how many arguments the system
			 * call uses.
			 *
			 * We have the property that loaded syscalls
			 * never change the number of arguments they
			 * use after they've been loaded once.  This
			 * allows us to stop for /proc tracing without
			 * holding the module lock.
			 * /proc is assured that sy_narg is valid.
			 */
			module_lock = lock_syscall(code);
			nargs = callp->sy_narg;
			rw_exit(module_lock);
		}

		/*
		 * Fetch the system call arguments.
		 */
		if (nargs != 0) {
			struct regs *rp = lwptoregs(lwp);
			int *sp = (int *)rp->r_uesp;

			ASSERT(nargs <= MAXSYSARGS);
			/*
			 * sp points to the return addr on the user's
			 * stack. bump it up to the actual args.
			 */
			sp++;
			if (copyin_args(sp, lwp->lwp_arg, nargs))
				return (-1);
		}
	}
	lwp->lwp_ap = lwp->lwp_arg;
	/*
	 * It is unnecessary to set t_post_sys, since lwp_ap will be reset
	 * by syscall_entry().
	 */
	return (0);
}

/*
 * Call a system call which takes a pointer to the user args struct and
 * a pointer to the return values.  This is a bit slower than the standard
 * C arg-passing method in some cases.
 */
longlong_t
syscall_ap()
{
	uint_t	error;
	struct sysent *callp;
	rval_t	rval;
	klwp_t	*lwp = ttolwp(curthread);
	struct regs *rp = lwptoregs(lwp);

	callp = &sysent[curthread->t_sysnum];

	rval.r_val1 = 0;
	rval.r_val2 = rp->r_edx;
	lwp->lwp_error = 0;	/* for old drivers */
	error = (*(callp->sy_call))(lwp->lwp_ap, &rval);
	if (error)
		return ((longlong_t)set_errno(error));
	return (rval.r_vals);
}

/*
 * Load system call module.
 *	Returns with pointer to held read lock for module.
 */
static krwlock_t *
lock_syscall(uint_t code)
{
	krwlock_t	*module_lock;
	struct modctl	*modp;
	int		id;

	module_lock = sysent[code].sy_lock;

	for (;;) {
		if ((id = modload("sys", syscallnames[code])) == -1)
			break;

		/*
		 * If we loaded successfully at least once, the modctl
		 * will still be valid, so we try to grab it by filename.
		 * If this call fails, it's because the mod_filename
		 * was changed after the call to modload() (mod_hold_by_name()
		 * is the likely culprit).  We can safely just take
		 * another lap if this is the case;  the modload() will
		 * change the mod_filename back to one by which we can
		 * find the modctl.
		 */
		modp = mod_find_by_filename("sys", syscallnames[code]);

		if (modp == NULL)
			continue;

		mutex_enter(&mod_lock);

		if (!modp->mod_installed) {
			mutex_exit(&mod_lock);
			continue;
		}
		break;
	}

	rw_enter(module_lock, RW_READER);

	if (id != -1)
		mutex_exit(&mod_lock);

	return (module_lock);
}

/*
 * Loadable system call.
 *	If the system call isn't loaded, load it.
 *	If it is loaded, lock it against unloading while the syscall is
 *	in progress.
 *	A pointer to the handler to use is returned.
 */
llfcn_t
loadable_syscall_entry(long *ap)
{
	uint_t		code;
	uint_t		nargs;
	struct sysent	*callp;
	int		argc = ap[MAXSYSARGS];

	code = curthread->t_sysnum;
	callp = &sysent[code];

	/*
	 * Try to autoload the system call if necessary.
	 */
	(void) lock_syscall(code);

	/*
	 * Now that its loaded, make sure enough args were copied.
	 */
	if ((nargs = callp->sy_narg) > argc) {
		klwp_t 	*lwp = ttolwp(curthread);
		struct regs *rp = lwptoregs(lwp);
		int *sp = (int *)rp->r_uesp;

		ASSERT(nargs <= MAXSYSARGS);
		/*
		 * sp points to the return addr on the user's
		 * stack. bump it up to the actual args.
		 */
		sp += argc + 1;
		ap += argc;
		nargs -= argc;
		if (copyin_args(sp, ap, nargs)) {
			/*
			 * loadable_syscall_exit() will take care of dropping
			 * the lock taken in lock_syscall()
			 */
			(void) set_errno(EINVAL);
			return (syscall_err);
		}
	}
	THREAD_KPRI_RELEASE();		/* drop priority given by rw_enter */
	if (callp->sy_flags & SE_ARGC)
		return ((llfcn_t)callp->sy_call);
	return (syscall_ap);
}


void
loadable_syscall_exit()
{
	ASSERT(curthread->t_sysnum < NSYSCALL);	/* already checked */

	THREAD_KPRI_REQUEST();	/* regain priority from read lock */
	rw_exit(sysent[curthread->t_sysnum].sy_lock);
}


/*
 * Indirect syscall handled in libc on i386.
 */
longlong_t
indir()
{
	return (nosys());
}

/*
 * set_errno - set an error return from the current system call.
 *	This could be a macro.
 *	This returns the value it is passed, so that the caller can
 *	use tail-recursion-elimination and do return (set_errno(ERRNO));
 */
uint_t
set_errno(uint_t errno)
{
	ASSERT(errno != 0);		/* must not be used to clear errno */

	curthread->t_post_sys = 1;	/* have post_syscall do error return */
	return (ttolwp(curthread)->lwp_errno = errno);
}

/*
 * set_proc_pre_sys - Set pre-syscall processing for entire process.
 */
void
set_proc_pre_sys(proc_t *p)
{
	kthread_id_t	t;
	kthread_id_t	first;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = first = p->p_tlist;
	do {
		t->t_pre_sys = 1;
	} while ((t = t->t_forw) != first);
}

/*
 * set_proc_post_sys - Set post-syscall processing for entire process.
 */
void
set_proc_post_sys(proc_t *p)
{
	kthread_id_t	t;
	kthread_id_t	first;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = first = p->p_tlist;
	do {
		t->t_post_sys = 1;
	} while ((t = t->t_forw) != first);
}

/*
 * set_proc_sys - Set pre- and post-syscall processing for entire process.
 */
void
set_proc_sys(proc_t *p)
{
	kthread_id_t	t;
	kthread_id_t	first;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = first = p->p_tlist;
	do {
		t->t_pre_sys = 1;
		t->t_post_sys = 1;
	} while ((t = t->t_forw) != first);
}

/*
 * set_all_proc_sys - set pre- and post-syscall processing flags for all
 * user processes.
 *
 * This is needed when auditing, tracing, or other facilities which affect
 * all processes are turned on.
 */
void
set_all_proc_sys()
{
	kthread_id_t	t;
	kthread_id_t	first;

	mutex_enter(&pidlock);
	t = first = curthread;
	do {
		t->t_pre_sys = 1;
		t->t_post_sys = 1;
	} while ((t = t->t_next) != first);
	mutex_exit(&pidlock);
}

/*
 * set_proc_ast - Set asynchronous service trap (AST) flag for all
 * threads in process.
 */
void
set_proc_ast(proc_t *p)
{
	kthread_id_t	t;
	kthread_id_t	first;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = first = p->p_tlist;
	do {
		aston(t);
	} while ((t = t->t_forw) != first);
}
