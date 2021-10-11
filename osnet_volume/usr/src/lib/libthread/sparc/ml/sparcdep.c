/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)sparcdep.c	1.66	98/08/12 SMI"

#ifdef TLS
#define	NOTHREAD
#endif

#include "libthread.h"
#ifdef __sparcv9
#include <sys/fsr.h>
#endif /* __sparcv9 */
#include <sys/reg.h>

#ifdef	TLS

intptr_t _threadoffset = (intptr_t)&thread;
/*
 * given a pointer to TLS find the pointer to struct thread.
 */
uthread_t *
_tlstot(char *tls)
{
	if (tls == NULL)
		_panic("tlstot, no TLS");
	return ((uthread_t *)(tls + (intptr_t)&_thread));
}

intptr_t
_t_tls(void)
{
	return ((intptr_t)&_thread);
}

#else

int _threadoffset = 0;

#endif /* TLS */

/* PROBE_SUPPORT begin */
extern void * _thread_probe_getfunc(void);
void * (*thr_probe_getfunc_addr)(void) = _thread_probe_getfunc;
/* PROBE_SUPPORT end */

/*
 * load a function call into a thread.
 */
int
_thread_call(uthread_t *t, void (*func)(), void *arg)
{
	struct rwindow *rwin;

	/*
	 * create a "c" call frame for "thread_start", the caller
	 * of "func".
	 */
	rwin = (struct rwindow *)(t->t_sp + STACK_BIAS);
	rwin->rw_in[0] = (greg_t)arg;
	rwin->rw_in[6] = 0;
	rwin->rw_in[7] = (greg_t)func;
	/*
	 * set thread to resume at thread_start()
	 */
	t->t_pc = (uintptr_t)_thread_start - 8;
	return (0);
}

void
_thread_ret(struct thread *t, void (*func)())
{
	t->t_pc = (uintptr_t)func - 8;
}

int
_lwp_exec(uthread_t *t, uintptr_t retpc, caddr_t sp, void (*func)(),
			int flags, lwpid_t *lwpidp)
{
	ucontext_t uc;
	int ret;

	ASSERT(t != NULL);
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_LWP_EXEC_START, "_lwpexec start");
	/*
	 * to avoid clearing the entire ucontext_t structure, only
	 * the following necessary fields are cleared.
	 */
	uc.uc_mcontext.fpregs.fpu_q = 0;
	uc.uc_mcontext.fpregs.fpu_en = 0;
	uc.uc_mcontext.gwins = 0;
	uc.uc_link = 0;

	/*
	 * clear extra register state information
	 */
	_xregs_clrptr(&uc);

	uc.uc_mcontext.gregs[REG_PC] = (greg_t)func;
	uc.uc_mcontext.gregs[REG_nPC] = (greg_t)func + sizeof (int);
	uc.uc_mcontext.gregs[REG_SP] = (greg_t)(sp);

	uc.uc_mcontext.gregs[REG_O7] = (greg_t)retpc - 8;
#ifdef TLS
	uc.uc_mcontext.gregs[REG_G7] = (greg_t)t->t_tls;
#else
	uc.uc_mcontext.gregs[REG_G7] = (greg_t)t;
#endif
	/*
	 * Push down the signal mask only for the ASLWP. This routine is
	 * called for two other types of threads: bound and aging. For both
	 * these threads, it would be incorrect to push down the mask:
	 *	- for bound threads: their calls to thr_sigsetmask() should
	 *	  also take advantage of fast masking - so the underlying LWP
	 *	  always has to have all signals unblocked.
	 *	- for aging threads: they pick up new threads to run and when
	 *		they do, they need to have all signals unblocked.
	 *
	 */
	if (flags & __LWP_ASLWP)
		uc.uc_sigmask = t->t_hold;
	else
		uc.uc_sigmask = _null_sigset;
	if (lwpidp == NULL)
		_panic("lwp_exec: NULL lwpidp");
	ret = _lwp_create(&uc, (unsigned long)flags, lwpidp);
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_LWP_EXEC_END, "_lwpexec end");
	return (ret);
}

#ifdef NO_SWAP_INSTRUCTION

/*
 * May not return if this is a segv caused due to the _mutex_unlock() reading
 * an unmapped waiter byte. If this is true, the routine advances the PC,
 * skipping over the segv instruction and does a setcontext(). Callable
 * only from the signal handler!
 */
void
__advance_pc_if_munlock_segv(int sig, siginfo_t *sip, ucontext_t *uap)
{
	/*
	 * __wrd is the label of the instruction in _mutex_unlock_asm() which
	 * reads the waiters byte. This read could seg-fault if the memory in
	 * which the lock is embedded is freed after clearing the lock but
	 * before reading the waiters byte. If this happens, advance the PC
	 * and move on, since there could not be any waiters if the memory was
	 * freed. On x86, _mutex_unlock_asm() uses the "xchgl" instruction to
	 * clear the lock and read the waiters bit atomically - hence not
	 * necessary to do this on x86. On sparc, the "swap" instruction is
	 * emulated on ss1s, ipcs, etc. i.e. on all sun4c systems, except ss2.
	 * Hence, on sparc, we do not use "swap" which would slow down mutex
	 * locking on such machines which emulate the instruction.
	 */

	if (sig == SIGSEGV && SI_FROMKERNEL(sip) &&
	    ((uap->uc_mcontext.gregs[REG_PC] == (greg_t)&__wrd) ||
	    (uap->uc_mcontext.gregs[REG_PC] == (greg_t)&__wrds))) {
		uap->uc_mcontext.gregs[REG_PC] =
		    uap->uc_mcontext.gregs[REG_nPC];
		uap->uc_mcontext.gregs[REG_nPC] += 4;
		setcontext(uap);
	}
}
#endif

void
_fpinherit(uthread_t *t)
{
#ifdef __sparcv9
	unsigned long fprs = 0;

	fprs = _getfprs();
	if (FPRS_FEF & fprs) {
		_copyfsr(&t->t_resumestate.rs_fsr);
		t->t_resumestate.rs_fpu_en = 0x01000000;
	}
#else
	psw_t psr = 0;

	psr = _getpsr();
	if (PSR_EF & psr) {
		_copyfsr(&t->t_resumestate.rs_fsr);
		t->t_resumestate.rs_fpu_en = 0x01000000;
	}
#endif /* __sparcv9 */
}

void
_get_resumestate(uthread_t *t, gregset_t rs)
{
	rs[REG_G2] = t->t_resumestate.rs_g2;
	rs[REG_G3] = t->t_resumestate.rs_g3;
#ifndef	__sparcv9
	rs[REG_G4] = t->t_resumestate.rs_g4;
#endif
	rs[REG_PC] = t->t_resumestate.rs_pc;
	rs[REG_SP] = t->t_resumestate.rs_sp;
	ASSERT(rs[REG_SP] != NULL);
}

void
_set_resumestate(uthread_t *t, gregset_t rs)
{
	t->t_resumestate.rs_g2 = rs[REG_G2];
	t->t_resumestate.rs_g3 = rs[REG_G3];
#ifndef	__sparcv9
	t->t_resumestate.rs_g4 = rs[REG_G4];
#endif
	t->t_resumestate.rs_pc = rs[REG_PC];
	t->t_resumestate.rs_sp = rs[REG_SP];
}
