/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)v7dep.c	1.43	99/11/20 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/vmparam.h>
#include <sys/systm.h>
#include <sys/stack.h>
#include <sys/frame.h>
#include <sys/proc.h>
#include <sys/ucontext.h>
#include <sys/cpuvar.h>
#include <sys/asm_linkage.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/bootconf.h>
#include <sys/promif.h>
#include <sys/archsystm.h>
#include <sys/fpu/fpusystm.h>
#include <sys/lockstat.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/mmu.h>
#include <sys/atomic.h>
#include <sys/copyops.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/machsystm.h>
#include <vm/as.h>
#include <vm/hat.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <sys/panic.h>
#include <sys/machpcb.h>

extern int fpdispr;

/* this belongs in <sys/fpu/fpusystm.h> */
extern void fp_installctx(klwp_id_t, fpregset_t *);

/*
 * Set floating-point registers.
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to set the registers of another lwp.
 */
void
setfpregs(klwp_t *lwp, fpregset_t *fp)
{
	struct machpcb *mpcb;
	fpregset_t *pfp;

	mpcb = lwptompcb(lwp);
	pfp = lwptofpu(lwp);

	if (fp->fpu_en) {
		if (!(pfp->fpu_en) && fpu_exists) {
			/*
			 * He's not currently using the FPU but wants to in his
			 * new context - arrange for this on return to userland.
			 */
			fp_installctx(lwp, pfp);
		}

		/*
		 * Load up a user's floating point context.
		 */
		if (fp->fpu_qcnt > MAXFPQ) 	/* plug security holes */
			fp->fpu_qcnt = MAXFPQ;

		fp->fpu_q_entrysize = sizeof (struct fq);
		(void) kcopy((caddr_t)fp, (caddr_t)pfp, sizeof (struct fpu));
		pfp->fpu_q = mpcb->mpcb_fpu_q;
		if (fp->fpu_qcnt)
			(void) kcopy((caddr_t)fp->fpu_q, (caddr_t)pfp->fpu_q,
				fp->fpu_qcnt * fp->fpu_q_entrysize);
		/* FSR ignores these bits on load, so they can not be set */
		pfp->fpu_fsr &= ~(FSR_QNE|FSR_FTT);

		kpreempt_disable();

		/*
		 * If not the current process then resume() will handle it
		 */
		if (lwp != ttolwp(curthread)) {
			/* force resume to reload fp regs */
			if (CPU->cpu_fpowner == lwp)
				CPU->cpu_fpowner = NULL;
			kpreempt_enable();
			return;
		}

		/*
		 * Load up FPU with new floating point context.
		 */
		if (fpu_exists) {
			if (fpdispr && ((getpsr() & PSR_EF) != PSR_EF))
				prom_printf("setfpregs with fp disabled!\n");
			fp_load(fp);
		}

		kpreempt_enable();
	} else {
		if (pfp->fpu_en) {
			/*
			 * Currently the lwp has floating point enabled.
			 * Disable floating point in the user's pcb and
			 * turn off FPU use in user PSR.
			 */
			pfp->fpu_en = 0;
			if (fpu_exists)
				lwptoregs(lwp)->r_psr &= ~PSR_EF;
			/* XXX: Deallocate FP context pointer */
		}
	}
}

/*
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to set the registers of another lwp.
 */
void
run_fpq(klwp_t *lwp, fpregset_t *fp)
{
	/*
	 * If the context being loaded up includes a floating queue,
	 * we need to simulate those instructions (since we can't reload
	 * the fpu) and pass the process any appropriate signals
	 */

	if (lwp == ttolwp(curthread) && CPU->cpu_fpowner == lwp) {
		if (fpu_exists) {
			if (fp->fpu_qcnt)
				fp_runq(lwp->lwp_regs);
		}
	}
}

/*
 * Get floating-point registers.
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to get the registers of another lwp.
 */
void
getfpregs(klwp_t *lwp, fpregset_t *fp)
{
	fpregset_t *pfp;
	extern int fpu_exists;

	pfp = lwptofpu(lwp);
	kpreempt_disable();
	if ((fp->fpu_en = pfp->fpu_en) != 0) {
		/*
		 * If we have an fpu and the current thread owns the fp
		 * context, flush fp registers into the pcb.
		 */
		if (fpu_exists && ttolwp(curthread) == lwp) {
			if (fpdispr && ((getpsr() & PSR_EF) != PSR_EF))
				prom_printf("getfpregs with fp disabled!\n");

			fp_fksave(pfp);
		}
		(void) kcopy((caddr_t)pfp, (caddr_t)fp, sizeof (struct fpu));
	} else {
		int i;
		for (i = 0; i < 32; i++)		/* Nan */
			fp->fpu_fr.fpu_regs[i] = (uint32_t)-1;
		fp->fpu_fsr = 0;
		fp->fpu_qcnt = 0;
	}
	kpreempt_enable();
}

/*
 * Set general registers.
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to set the registers of another lwp.
 */
void
setgregs(klwp_t *lwp, gregset_t rp)
{
	int current = (lwp == curthread->t_lwp);

	if (current)
		/* copy the args from the regs first */
		(void) save_syscall_args();

	/*
	 * pc and npc must be word aligned on sparc.
	 * We silently make it so to avoid a watchdog reset.
	 */
	rp[REG_PC] &= ~03;
	rp[REG_nPC] &= ~03;

	/*
	 * Only the condition-codes of the PSR can be modified.
	 */
	rp[REG_PSR] = (lwptoregs(lwp)->r_psr & ~PSL_USERMASK) |
		(rp[REG_PSR] & PSL_USERMASK);

	bcopy((caddr_t)rp, (caddr_t)lwp->lwp_regs, sizeof (gregset_t));

	if (current) {
		/*
		 * This was called from a system call, but we
		 * do not want to return via the shared window;
		 * restoring the CPU context changes everything.
		 * We set a flag telling syscall_trap to jump to sys_rtt.
		 */
		lwp->lwp_eosys = JUSTRETURN;
		curthread->t_post_sys = 1;
		lwptompcb(lwp)->mpcb_flags |= GOTO_SYS_RTT;
	}
}

/*
 * Return the general registers
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to get the registers of another lwp.
 */
void
getgregs(klwp_t *lwp, gregset_t rp)
{
	bcopy((caddr_t)lwp->lwp_regs, (caddr_t)rp, sizeof (gregset_t));
}

/*
 * Return the user-level PC.
 * If in a system call, return the address of the syscall trap.
 */
greg_t
getuserpc(void)
{
	return (lwptoregs(ttolwp(curthread))->r_pc);
}

/*
 * Set register windows.
 */
void
setgwins(klwp_t *lwp, gwindows_t *gwins)
{
	int i;
	struct machpcb *mpcb;

	mpcb = lwptompcb(lwp);
	mpcb->mpcb_wbcnt = 0;
	for (i = 0; i < gwins->wbcnt; i++) {
		mpcb->mpcb_spbuf[i] = (caddr_t)gwins->spbuf[i];
		bcopy((caddr_t)&gwins->wbuf[i],
			(caddr_t)&mpcb->mpcb_wbuf[i],
			sizeof (struct rwindow));
		mpcb->mpcb_wbcnt++;
	}
}

/*
 * Get register windows.
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to get the registers of another lwp.
 */
void
getgwins(klwp_t *lwp, gwindows_t *gwp)
{
	struct machpcb *mpcb = lwptompcb(lwp);
	int wbcnt = mpcb->mpcb_wbcnt;
	int i;

	ASSERT(wbcnt >= 0 && wbcnt <= SPARC_MAXREGWINDOW);
	gwp->wbcnt = wbcnt;
	for (i = 0; i < wbcnt; i++) {
		gwp->spbuf[i] = (greg_t *)mpcb->mpcb_spbuf[i];
		bcopy(&mpcb->mpcb_wbuf[i],
		    &(gwp->wbuf[i]), sizeof (struct rwindow));
	}
}

/*
 * For things that depend on register state being on the stack,
 * copy any register windows that get saved into the window buffer
 * (in the pcb) onto the stack.  This normally gets fixed up
 * before returning to a user program.  Callers of this routine
 * require this to happen immediately because a later kernel
 * operation depends on window state (like instruction simulation).
 */
int
flush_user_windows_to_stack(caddr_t *psp)
{
	int j, k;
	caddr_t sp;
	proc_t *p = ttoproc(curthread);
	struct machpcb *mpcb = lwptompcb(ttolwp(curthread));
	int mapped;
	int err;
	int error = 0;

	flush_user_windows();
	ASSERT(mpcb->mpcb_uwm == 0);
	j = mpcb->mpcb_wbcnt;
	while (j > 0) {
		sp = mpcb->mpcb_spbuf[--j];
		if (((int)sp & (STACK_ALIGN - 1)) != 0)
			continue;
		mapped = 0;
		if (p->p_warea)		/* watchpoints in effect */
			mapped = pr_mappage(sp, sizeof (struct rwindow),
				S_WRITE, 1);
		if (((err = default_xcopyout((caddr_t)&mpcb->mpcb_wbuf[j], sp,
		    sizeof (struct rwindow))) != 0)) {
			if (psp != NULL) {
				/*
				 * Determine the offending address.
				 * It may not be the stack pointer itself.
				 */
				uint_t *kaddr = (uint_t *)&mpcb->mpcb_wbuf[j];
				uint_t *uaddr = (uint_t *)sp;

				for (k = 0;
				    k < sizeof (struct rwindow) / sizeof (int);
				    k++, kaddr++, uaddr++) {
					if (default_suword32(uaddr, *kaddr))
						break;
				}

				/* can't happen? */
				if (k == sizeof (struct rwindow) / sizeof (int))
					uaddr = (uint_t *)sp;

				*psp = (caddr_t)uaddr;
			}
			error = err;
		} else {

			/*
			 * stack was aligned and copyout succeded;
			 * move other windows down.
			 */
			mpcb->mpcb_wbcnt--;
			for (k = j; k < mpcb->mpcb_wbcnt; k++) {
				mpcb->mpcb_spbuf[k] = mpcb->mpcb_spbuf[k+1];
				bcopy((caddr_t)&mpcb->mpcb_wbuf[k+1],
					(caddr_t)&mpcb->mpcb_wbuf[k],
					sizeof (struct rwindow));
			}
		}
		if (mapped)
			pr_unmappage(sp, sizeof (struct rwindow), S_WRITE, 1);
	}
	return (error);
}

int
copy_return_window(int dotwo)
{
	proc_t *p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);
	struct machpcb *mpcb = lwptompcb(lwp);
	caddr_t sp1;
	int map1 = 0;
	caddr_t sp2;
	int map2 = 0;

	(void) flush_user_windows_to_stack(NULL);
	if (mpcb->mpcb_rsp[0] == NULL) {
		sp1 = (caddr_t)lwptoregs(lwp)->r_sp;
		if (p->p_warea)		/* watchpoints in effect */
			map1 = pr_mappage(sp1, sizeof (struct rwindow),
				S_READ, 1);
		if ((default_copyin(sp1, (caddr_t)&mpcb->mpcb_rwin[0],
		    sizeof (struct rwindow)) == 0))
			mpcb->mpcb_rsp[0] = sp1;
	}
	mpcb->mpcb_rsp[1] = NULL;
	if (dotwo && mpcb->mpcb_rsp[0] != NULL &&
	    (sp2 = (caddr_t)mpcb->mpcb_rwin[0].rw_fp) != NULL) {
		if (p->p_warea)		/* watchpoints in effect */
			map2 = pr_mappage(sp2, sizeof (struct rwindow),
				S_READ, 1);
		if ((default_copyin(sp2, (caddr_t)&mpcb->mpcb_rwin[1],
		    sizeof (struct rwindow)) == 0))
			mpcb->mpcb_rsp[1] = sp2;
	}
	if (map2)
		pr_unmappage(sp2, sizeof (struct rwindow), S_READ, 1);
	if (map1)
		pr_unmappage(sp1, sizeof (struct rwindow), S_READ, 1);
	return (mpcb->mpcb_rsp[0] != NULL);
}

/*
 * Clear registers on exec(2).
 */
void
setregs(void)
{
	ulong_t entry;
	struct regs *rp;
	klwp_id_t lwp = ttolwp(curthread);
	struct machpcb *mpcb;
	proc_t *p = ttoproc(curthread);

	entry = (ulong_t)u.u_exdata.ux_entloc;

	/*
	 * Initialize user registers.
	 */
	(void) save_syscall_args();	/* copy args from registers first */
	rp = lwptoregs(lwp);
	rp->r_g1 = rp->r_g2 = rp->r_g3 = rp->r_g4 = rp->r_g5 =
	    rp->r_g6 = rp->r_g7 = rp->r_o0 = rp->r_o1 = rp->r_o2 =
	    rp->r_o3 = rp->r_o4 = rp->r_o5 = rp->r_o7 = 0;
	rp->r_psr = PSL_USER;
	rp->r_pc = entry;
	rp->r_npc = entry + 4;
	rp->r_y = 0;
	curthread->t_post_sys = 1;
	lwp->lwp_eosys = JUSTRETURN;
	lwp->lwp_pcb.pcb_trap0addr = NULL;	/* no trap 0 handler */
	/*
	 * Clear the fixalignment flag
	 */
	p->p_fixalignment = 0;

	/*
	 * Throw out old user windows, init window buf.
	 */
	trash_user_windows();

	mpcb = lwptompcb(lwp);
	mpcb->mpcb_flags |= GOTO_SYS_RTT;
	/*
	 * Here we initialize minimal fpu state.
	 * The rest is done at the first floating
	 * point instruction that a process executes.
	 */
	mpcb->mpcb_fpu.fpu_en = 0;
}

/*
 * Copy regs from parent to child.
 */
void
lwp_forkregs(klwp_t *lwp, klwp_t *clwp)
{
	kthread_t *t;
	struct machpcb *mpcb = lwptompcb(clwp);

	t = mpcb->mpcb_thread;
	/*
	 * Don't copy mpcb_frame since we hand-crafted it
	 * in thread_load().
	 */
	bcopy((caddr_t)lwp->lwp_regs, (caddr_t)clwp->lwp_regs,
		sizeof (struct machpcb) - REGOFF);
	mpcb->mpcb_thread = t;
	mpcb->mpcb_fpu.fpu_q = mpcb->mpcb_fpu_q;
}

/*
 * This function is unused on V7, but could be used to call fp_free,
 * if the V7 floating point code is changed to not use installctx.
 */
/*ARGSUSED*/
void
lwp_freeregs(klwp_t *lwp, int isexec)
{}

/*
 * Construct the execution environment for the user's signal
 * handler and arrange for control to be given to it on return
 * to userland.  The library code now calls setcontext() to
 * clean up after the signal handler, so sigret() is no longer
 * needed.
 */
int
sendsig(int sig, k_siginfo_t *sip, void	(*hdlr)())
{
	/*
	 * 'volatile' is needed to ensure that values are
	 * correct on the error return from on_fault().
	 */
	volatile int minstacksz; /* min stack required to catch signal */
	int newstack = 0;	/* if true, switching to altstack */
	label_t ljb;
	int *sp;
	struct regs *volatile rp;
	proc_t *volatile p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);
	int fpq_size = 0;
	struct sigframe {
		struct frame frwin;
		ucontext_t uc;
	};
	siginfo_t *sip_addr;
	struct sigframe *volatile fp;
	ucontext_t *volatile tuc = NULL;
	char *volatile xregs = NULL;
	volatile int xregs_size = 0;
	gwindows_t *volatile gwp = NULL;
	volatile int gwin_size = 0;
	fpregset_t *fpp;
	struct machpcb *mpcb;
	volatile int mapped = 0;
	volatile int map2 = 0;

	/*
	 * Make sure the current last user window has been flushed to
	 * the stack save area before we change the sp.
	 * Restore register window if a debugger modified it.
	 */
	(void) flush_user_windows_to_stack(NULL);
	if (lwp->lwp_pcb.pcb_xregstat != XREGNONE)
		xregrestore(lwp, 0);

	mpcb = lwptompcb(lwp);
	rp = lwptoregs(lwp);

	/*
	 * Clear the watchpoint return stack pointers.
	 */
	mpcb->mpcb_rsp[0] = NULL;
	mpcb->mpcb_rsp[1] = NULL;

	minstacksz = sizeof (struct sigframe);

	if (sip != NULL)
		minstacksz += sizeof (siginfo_t);

	/*
	 * These two fields are pointed to by ABI structures and may
	 * be of arbitrary length. Size them now so we know how big
	 * the signal frame has to be.
	 */
	fpp = lwptofpu(lwp);
	if (fpp->fpu_en) {
		fpq_size = fpp->fpu_q_entrysize * fpp->fpu_qcnt;
		minstacksz += fpq_size;
	}

	if (mpcb->mpcb_wbcnt != 0) {
		gwin_size = (mpcb->mpcb_wbcnt * sizeof (struct rwindow))
		    + SPARC_MAXREGWINDOW * sizeof (int *) + sizeof (int);
		minstacksz += gwin_size;
	}

	/*
	 * Extra registers, if support by this platform, may be of arbitrary
	 * length. Size them now so we know how big the signal frame has to be.
	 */
	xregs_size = xregs_getsize(p);
	minstacksz += xregs_size;

	/*
	 * Figure out whether we will be handling this signal on
	 * an alternate stack specified by the user. Then allocate
	 * and validate the stack requirements for the signal handler
	 * context. on_fault will catch any faults.
	 */
	newstack = (sigismember(&u.u_sigonstack, sig) &&
	    !(lwp->lwp_sigaltstack.ss_flags & (SS_ONSTACK|SS_DISABLE)));

	if (newstack != 0) {
		fp = (struct sigframe *)(SA((int)lwp->lwp_sigaltstack.ss_sp) +
		    SA((int)lwp->lwp_sigaltstack.ss_size) - STACK_ALIGN -
			SA(minstacksz));
	} else {
		fp = (struct sigframe *)((caddr_t)rp->r_sp - SA(minstacksz));
		/*
		 * Could call grow here, but stack growth now handled below
		 * in code protected by on_fault().
		 */
	}
	sp = (int *)((int)fp + sizeof (struct sigframe));

	/*
	 * Make sure process hasn't trashed its stack.
	 */
	if (((int)fp & (STACK_ALIGN - 1)) != 0 ||
	    (caddr_t)fp >= (caddr_t)KERNELBASE ||
	    (caddr_t)fp + SA(minstacksz) >= (caddr_t)KERNELBASE) {
#ifdef DEBUG
		printf("sendsig: bad signal stack pid=%d, sig=%d\n",
		    p->p_pid, sig);
		printf("sigsp = 0x%p, action = 0x%p, upc = 0x%lx\n",
		    (void *)fp, (void *)hdlr, rp->r_pc);

		if (((int)fp & (STACK_ALIGN - 1)) != 0)
		    printf("bad stack alignment\n");
		else
		    printf("fp above KERNELBASE\n");
#endif
		return (0);
	}

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)fp, SA(minstacksz), S_WRITE, 1);
	if (on_fault(&ljb))
		goto badstack;

	tuc = kmem_alloc(sizeof (ucontext_t), KM_SLEEP);
	xregs_clrptr(lwp, tuc);
	savecontext(tuc, lwp->lwp_sigoldmask);

	/*
	 * save extra register state if it exists
	 */
	if (xregs_size != 0) {
		xregs_setptr(lwp, tuc, (caddr_t)sp);
		xregs = kmem_alloc(xregs_size, KM_SLEEP);
		xregs_get(lwp, xregs);
		copyout_noerr(xregs, (caddr_t)sp, xregs_size);
		kmem_free(xregs, xregs_size);
		xregs = NULL;
		sp = (int *)((int)sp + xregs_size);
	}

	copyout_noerr((caddr_t)tuc, &fp->uc, sizeof (ucontext_t));
	kmem_free(tuc, sizeof (ucontext_t));
	tuc = NULL;

	if (sip != NULL) {
		uzero((caddr_t)sp, sizeof (siginfo_t));
		copyout_noerr((caddr_t)sip, (caddr_t)sp, sizeof (*sip));
		sip_addr = (siginfo_t *)sp;
		sp = (int *)((int)sp + sizeof (siginfo_t));

		if (sig == SIGPROF &&
		    curthread->t_rprof != NULL &&
		    curthread->t_rprof->rp_anystate) {
			/*
			 * We stand on our head to deal with
			 * the real time profiling signal.
			 * Fill in the stuff that doesn't fit
			 * in a normal k_siginfo structure.
			 */
			int i = sip->si_nsysarg;
			while (--i >= 0)
				suword32_noerr((uint_t *)
				    &sip_addr->si_sysarg[i],
				    (int)lwp->lwp_arg[i]);
			copyout_noerr((caddr_t)curthread->t_rprof->rp_state,
			    (caddr_t)sip_addr->si_mstate,
			    sizeof (curthread->t_rprof->rp_state));
		}
	} else
		sip_addr = (siginfo_t *)NULL;

	/*
	 * When flush_user_windows_to_stack() can't save all the
	 * windows to the stack, it puts them in the lwp's pcb.
	 */
	if (gwin_size != 0) {
		gwp = kmem_alloc(gwin_size, KM_SLEEP);
		getgwins(lwp, gwp);
		suword32_noerr(&fp->uc.uc_mcontext.gwins, (uint_t)sp);
		copyout_noerr((caddr_t)gwp, (caddr_t)sp, gwin_size);
		kmem_free(gwp, gwin_size);
		gwp = NULL;
		sp = (int *)((int)sp + gwin_size);
	} else
		suword32_noerr(&fp->uc.uc_mcontext.gwins, 0);

	if (fpq_size != 0) {
		struct fq *fqp = (struct fq *)sp;
		suword32_noerr(&fp->uc.uc_mcontext.fpregs.fpu_q, (uint_t)fqp);
		copyout_noerr((caddr_t)mpcb->mpcb_fpu_q, fqp,
			fpp->fpu_qcnt * fpp->fpu_q_entrysize);

		/*
		 * forget the fp queue so that the signal handler can run
		 * without being harrassed--it will do a setcontext that will
		 * re-establish the queue if there still is one
		 *
		 * NOTE: fp_runq() relies on the qcnt field being zeroed here
		 *	to terminate its processing of the queue after signal
		 *	delivery.
		 */
		mpcb->mpcb_fpu.fpu_qcnt = 0;
		sp = (int *)((int)sp + fpq_size);

		/* Also, syscall needs to know about this */
		mpcb->mpcb_flags |= FP_TRAPPED;

	} else {
		suword32_noerr(&fp->uc.uc_mcontext.fpregs.fpu_q, 0);
		suword8_noerr(&fp->uc.uc_mcontext.fpregs.fpu_qcnt, 0);
	}


	/*
	 * Since we flushed the user's windows and we are changing his
	 * stack pointer, the window that the user will return to will
	 * be restored from the save area in the frame we are setting up.
	 * We copy in save area for old stack pointer so that debuggers
	 * can do a proper stack backtrace from the signal handler.
	 */
	if (mpcb->mpcb_wbcnt == 0) {
		if (p->p_warea)
			map2 = pr_mappage((caddr_t)rp->r_sp,
				sizeof (struct rwindow), S_READ, 1);
		ucopy((caddr_t)rp->r_sp, &fp->frwin,
		    sizeof (struct rwindow));
	}

	lwp->lwp_oldcontext = (uintptr_t)&fp->uc;

	if (newstack != 0)
		lwp->lwp_sigaltstack.ss_flags |= SS_ONSTACK;

	no_fault();
	mpcb->mpcb_wbcnt = 0;		/* let user go on */

	if (map2)
		pr_unmappage((caddr_t)rp->r_sp, sizeof (struct rwindow),
			S_READ, 1);
	if (mapped)
		pr_unmappage((caddr_t)fp, SA(minstacksz), S_WRITE, 1);

	/*
	 * Set up user registers for execution of signal handler.
	 */
	rp->r_sp = (int)fp;
	rp->r_pc = (int)hdlr;
	rp->r_npc = (int)hdlr + 4;
	rp->r_o0 = sig;
	rp->r_o1 = (int)sip_addr;
	rp->r_o2 = (int)&fp->uc;
	/*
	 * Don't set lwp_eosys here.  sendsig() is called via psig() after
	 * lwp_eosys is handled, so setting it here would affect the next
	 * system call.
	 */
	return (1);

badstack:
	no_fault();
	if (map2)
		pr_unmappage((caddr_t)rp->r_sp, sizeof (struct rwindow),
			S_READ, 1);
	if (mapped)
		pr_unmappage((caddr_t)fp, SA(minstacksz), S_WRITE, 1);
	if (tuc)
		kmem_free(tuc, sizeof (ucontext_t));
	if (xregs)
		kmem_free(xregs, xregs_size);
	if (gwp)
		kmem_free(gwp, gwin_size);
#ifdef DEBUG
	printf("sendsig: bad signal stack pid=%d, sig=%d\n",
	    p->p_pid, sig);
	printf("on fault, sigsp = 0x%p, action = 0x%p, upc = 0x%lx\n",
	    (void *)fp, (void *)hdlr, rp->r_pc);
#endif
	return (0);
}

/*
 * load user registers into lwp
 */
void
lwp_load(klwp_t *lwp, gregset_t gregs)
{
	setgregs(lwp, gregs);
	lwptoregs(lwp)->r_psr = PSL_USER;
	lwp->lwp_eosys = JUSTRETURN;
	lwptot(lwp)->t_post_sys = 1;
}

/*
 * set syscall()'s return values for a lwp.
 */
void
lwp_setrval(klwp_t *lwp, int v1, int v2)
{
	struct regs *rp = lwptoregs(lwp);

	rp->r_psr &= ~PSR_C;
	rp->r_o0 = v1;
	rp->r_o1 = v2;
}

/*
 * set stack pointer for a lwp
 */
void
lwp_setsp(klwp_t *lwp, caddr_t sp)
{
	struct regs *rp = lwptoregs(lwp);
	rp->r_sp = (int)sp;
}

/*
 * Invalidate the saved user register windows in the pcb struct
 * for the current thread. They will no longer be preserved.
 */
void
lwp_clear_uwin(void)
{
	struct machpcb *m = lwptompcb(ttolwp(curthread));

	/*
	 * This has the effect of invalidating all (any) of the
	 * user level windows that are currently sitting in the
	 * kernel buffer.
	 */
	m->mpcb_wbcnt = 0;
}

/*
 * Returns whether the current lwp is cleaning windows.
 */
int
lwp_cleaningwins(klwp_t *lwp)
{
	return (lwptompcb(lwp)->mpcb_flags & CLEAN_WINDOWS);
}

/*
 * Start and end events on behalf of the lockstat driver.
 * Since the v7 locking and timing primitives don't provide
 * anything whizzy (like cas and %tick) we just do it in C.
 */
int
lockstat_event_start(uintptr_t lp, ls_pend_t *lpp)
{
	extern uint8_t ldstub(uint8_t *);

	if (ldstub((uint8_t *)&lpp->lp_mylock) == 0) {
		lpp->lp_lock = lp;
		lpp->lp_start_time = gethrtime();
		return (0);
	}
	return (-1);
}

hrtime_t
lockstat_event_end(ls_pend_t *lpp)
{
	hrtime_t start_time;

	start_time = lpp->lp_start_time;
	lpp->lp_lock = 0;
	membar_exit();
	lpp->lp_mylock = 0;
	return (gethrtime() - start_time);
}

/*
 * Simple C support for the miss cases of v7 atomics (cas32() et al).
 * We only gather contention stats for these locks.  We don't gather
 * hold times because we know the hold time is constant (i.e. it's
 * always the same fixed set of 10-20 instructions).
 */
void
atomic_lock_set(lock_t *lp)
{
	extern uint8_t ldstub(uint8_t *);
	int spin_count = 1;

	if (panicstr)
		return;

	if (ncpus == 1)
		panic("atomic_lock_set: %p lock held and only one CPU",
		    (void *)lp);

	while (LOCK_HELD((volatile lock_t *)lp) || ldstub((uint8_t *)lp))
		spin_count++;

	LOCKSTAT_RECORD(LS_SPIN_LOCK, lp, spin_count, 1);
}

/*
 * These functions are not used on this architecture, but are
 * declared in common/sys/copyops.h.  Definitions are provided here
 * but they should never be called.
 */
/* ARGSUSED */
int
default_fuword64(const void *addr, uint64_t *valuep)
{
	ASSERT(0);
	return (-1);
}

/* ARGSUSED */
int
default_suword64(void *addr, uint64_t value)
{
	ASSERT(0);
	return (-1);
}

/*
 * The panic code invokes panic_saveregs() to record the contents of a
 * regs structure into the specified panic_data structure for debuggers.
 */
void
panic_saveregs(panic_data_t *pdp, struct regs *rp)
{
	panic_nv_t *pnv = PANICNVGET(pdp);

	PANICNVADD(pnv, "psr", (uint32_t)rp->r_psr);
	PANICNVADD(pnv, "pc", (uint32_t)rp->r_pc);
	PANICNVADD(pnv, "npc", (uint32_t)rp->r_npc);
	PANICNVADD(pnv, "y", (uint32_t)rp->r_y);
	PANICNVADD(pnv, "g1", (uint32_t)rp->r_g1);
	PANICNVADD(pnv, "g2", (uint32_t)rp->r_g2);
	PANICNVADD(pnv, "g3", (uint32_t)rp->r_g3);
	PANICNVADD(pnv, "g4", (uint32_t)rp->r_g4);
	PANICNVADD(pnv, "g5", (uint32_t)rp->r_g5);
	PANICNVADD(pnv, "g6", (uint32_t)rp->r_g6);
	PANICNVADD(pnv, "g7", (uint32_t)rp->r_g7);
	PANICNVADD(pnv, "o0", (uint32_t)rp->r_o0);
	PANICNVADD(pnv, "o1", (uint32_t)rp->r_o1);
	PANICNVADD(pnv, "o2", (uint32_t)rp->r_o2);
	PANICNVADD(pnv, "o3", (uint32_t)rp->r_o3);
	PANICNVADD(pnv, "o4", (uint32_t)rp->r_o4);
	PANICNVADD(pnv, "o5", (uint32_t)rp->r_o5);
	PANICNVADD(pnv, "o6", (uint32_t)rp->r_o6);
	PANICNVADD(pnv, "o7", (uint32_t)rp->r_o7);

	PANICNVSET(pdp, pnv);
}

/*
 * Non-cached memory allocator.  Required on all v7 platforms.
 */
vmem_t	*knc_arena;	/* non-cached memory pool */
size_t	knc_limit;	/* memory usage limit */

void *
knc_alloc(vmem_t *vmp, size_t size, int flag)
{
	struct page *pp, *ppfirst;
	void *addr;
	struct seg kseg;

	if (vmem_size(knc_arena, VMEM_ALLOC | VMEM_FREE) + size > knc_limit) {
		cmn_err(CE_WARN, "knc_alloc: maximum size reached");
		return (NULL);
	}

	if ((addr = vmem_alloc(vmp, size, flag)) == NULL) {
		cmn_err(CE_WARN, "knc_alloc: no virtual memory");
		return (NULL);
	}

	if (page_resv(btop(size), flag & VM_KMFLAGS) == 0) {
		cmn_err(CE_WARN, "knc_alloc: no availrmem");
		vmem_free(vmp, addr, size);
		return (NULL);
	}

	kseg.s_as = &kas;
	ppfirst = pp = page_create_va(&kvp, (offset_t)addr, size,
	    PG_EXCL | ((flag & VM_NOSLEEP) ? 0 : PG_WAIT), &kseg, addr);

	if (ppfirst == NULL) {
		cmn_err(CE_WARN, "knc_alloc: no pages");
		page_unresv(btop(size));
		vmem_free(vmp, addr, size);
		return (NULL);
	}

	do {
		hat_pagecachectl(pp, HAT_UNCACHE);
		hat_memload(kas.a_hat, (caddr_t)pp->p_offset, pp,
		    PROT_ALL & ~PROT_USER, HAT_LOAD_LOCK);
		page_downgrade(pp);
		pp = pp->p_next;
	} while (pp != ppfirst);

	return (addr);
}

/*
 * Allocate from the system, aligned on a specific boundary.
 * The alignment, if non-zero, must be a power of 2.
 */
void *
kalloca(size_t size, size_t align, int nocache, int cansleep)
{
	size_t *addr, *raddr, rsize;
	size_t hdrsize = 2 * sizeof (size_t);	/* must be power of 2 */

	align = MAX(align, hdrsize);
	ASSERT((align & (align - 1)) == 0);
	rsize = P2ROUNDUP(size, align) + align + hdrsize;

	if (nocache && knc_arena != NULL)
		raddr = vmem_alloc(knc_arena, rsize,
		    cansleep ? VM_SLEEP : VM_NOSLEEP);
	else
		raddr = kmem_alloc(rsize, cansleep ? KM_SLEEP : KM_NOSLEEP);
	if (raddr == NULL)
		return (NULL);
	addr = (size_t *)P2ROUNDUP((uintptr_t)raddr + hdrsize, align);
	addr[-2] = (size_t)raddr;
	addr[-1] = rsize;

	return (addr);
}

void
kfreea(void *addr, int nocache)
{
	size_t *saddr = addr;

	if (nocache && knc_arena != NULL)
		vmem_free(knc_arena, (void *)saddr[-2], saddr[-1]);
	else
		kmem_free((void *)saddr[-2], saddr[-1]);
}
