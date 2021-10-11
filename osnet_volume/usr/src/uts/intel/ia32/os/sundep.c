/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T   */
/*	All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/


#pragma ident	"@(#)sundep.c	1.107	99/11/20 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/class.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/archsystm.h>
#include <sys/vmparam.h>
#include <sys/prsystm.h>
#include <sys/reboot.h>
#include <sys/uadmin.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/session.h>
#include <sys/ucontext.h>
#include <sys/dnlc.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/debugreg.h>
#include <sys/thread.h>
#include <sys/vtrace.h>
#include <sys/consdev.h>
#include <sys/psw.h>
#include <sys/reg.h>
#include <sys/cpu.h>
#include <sys/stack.h>
#include <sys/swap.h>
#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <sys/exec.h>
#include <sys/acct.h>
#include <sys/core.h>
#include <sys/corectl.h>
#include <sys/modctl.h>
#include <sys/tuneable.h>
#include <c2/audit.h>
#include <sys/sunddi.h>
#include <sys/bootconf.h>
#include <sys/dumphdr.h>
#include <sys/promif.h>
#include <sys/systeminfo.h>

int is_coff_proc(proc_t *p);

/*
 * Compare the version of boot that boot says it is against
 * the version of boot the kernel expects.
 */
int
check_boot_version(int boots_version)
{
	if (boots_version == BO_VERSION)
		return (0);

	prom_printf("Wrong boot interface - kernel needs v%d found v%d\n",
	    BO_VERSION, boots_version);
	prom_panic("halting");
	/*NOTREACHED*/
}

/*
 * Count the number of available pages and the number of
 * chunks in the list of available memory.
 */
void
size_physavail(
	struct memlist	*physavail,
	pgcnt_t		*npages,
	int		*memblocks,
	pfn_t		last_pfn)
{
	uint64_t mmu_limited_physmax;

	*npages = 0;
	*memblocks = 0;
	mmu_limited_physmax = ptob((uint64_t)last_pfn + 1);
	for (; physavail; physavail = physavail->next) {
		if (physavail->address >= mmu_limited_physmax)
			continue;
		if (physavail->address + physavail->size < mmu_limited_physmax)
		    *npages += (pgcnt_t)btop(physavail->size);
		else
		    *npages += (pgcnt_t)
			btop((mmu_limited_physmax - physavail->address));
		(*memblocks)++;
	}
}

/*
 * Copy boot's physavail list deducting memory at "start"
 * for "size" bytes.
 */
int
copy_physavail(
	struct memlist	*src,
	struct memlist	**dstp,
	uint_t		start,
	uint_t		size)
{
	struct memlist *dst, *prev;
	uint_t end1;
	int deducted = 0;

	dst = *dstp;
	prev = dst;
	end1 = start + size;

	for (; src; src = src->next) {
		u_longlong_t addr, lsize, end2;

		addr = src->address;
		lsize = src->size;
		end2 = addr + lsize;

		if (start >= addr && end1 <= end2) {
			/* deducted range in this chunk */
			deducted = 1;
			if (start == addr) {
				/* abuts start of chunk */
				if (end1 == end2)
					/* is equal to the chunk */
					continue;
				dst->address = end1;
				dst->size = lsize - size;
			} else if (end1 == end2) {
				/* abuts end of chunk */
				dst->address = addr;
				dst->size = lsize - size;
			} else {
				/* in the middle of the chunk */
				dst->address = addr;
				dst->size = start - addr;
				dst->next = 0;
				if (prev == dst) {
					dst->prev = 0;
					dst++;
				} else {
					dst->prev = prev;
					prev->next = dst;
					dst++;
					prev++;
				}
				dst->address = end1;
				dst->size = end2 - end1;
			}
			dst->next = 0;
			if (prev == dst) {
				dst->prev = 0;
				dst++;
			} else {
			dst->prev = prev;
				prev->next = dst;
				dst++;
				prev++;
			}
		} else {
			dst->address = src->address;
			dst->size = src->size;
			dst->next = 0;
			if (prev == dst) {
				dst->prev = 0;
				dst++;
			} else {
				dst->prev = prev;
				prev->next = dst;
				dst++;
				prev++;
			}
		}
	}

	*dstp = dst;
	return (deducted);
}

/*
 * Find the page number of the highest installed physical
 * page and the number of pages installed (one cannot be
 * calculated from the other because memory isn't necessarily
 * contiguous).
 */
void
installed_top_size(
	struct memlist *list,	/* pointer to start of installed list */
	pfn_t *topp,		/* return ptr for top value */
	pgcnt_t *sumpagesp,	/* return ptr for sum of installed pages */
	pfn_t last_pfn)
{
	pfn_t top = 0;
	pgcnt_t sumpages = 0;
	pfn_t highp;		/* high page in a chunk */
	uint64_t mmu_limited_physmax;

	mmu_limited_physmax = ptob((uint64_t)last_pfn + 1);
	for (; list; list = list->next) {

		if (list->address >= mmu_limited_physmax)
			continue;

		highp = (list->address + list->size - 1) >> PAGESHIFT;
		if (highp > last_pfn)
			highp = last_pfn;
		if (top < highp)
			top = highp;
		if (list->address + list->size < mmu_limited_physmax)
		    sumpages += btop(list->size);
		else
		    sumpages += btop(mmu_limited_physmax - list->address);
	}

	*topp = top;
	*sumpagesp = sumpages;
}

/*
 * Copy a memory list.  Used in startup() to copy boot's
 * memory lists to the kernel.
 */
void
copy_memlist(
	struct memlist *src,
	struct memlist **dstp)
{
	struct memlist *dst, *prev;

	dst = *dstp;
	prev = dst;

	for (; src; src = src->next) {
		dst->address = src->address;
		dst->size = src->size;
		dst->next = 0;
		if (prev == dst) {
			dst->prev = 0;
			dst++;
		} else {
			dst->prev = prev;
			prev->next = dst;
			dst++;
			prev++;
		}
	}

	*dstp = dst;
}

/*
 * Kernel setup code, called from startup().
 */
void
kern_setup1(void)
{
	proc_t *pp;

	pp = &p0;

	proc_sched = pp;

	/*
	 * Initialize process 0 data structures
	 */
	pp->p_stat = SRUN;
	pp->p_flag = SLOAD | SSYS | SLOCK | SULOAD;

	pp->p_pidp = &pid0;
	pp->p_pgidp = &pid0;
	pp->p_sessp = &session0;
	pp->p_tlist = &t0;
	pid0.pid_pglink = pp;


	/*
	 * XXX - we asssume that the u-area is zeroed out except for
	 * ttolwp(curthread)->lwp_regs.
	 */
	u.u_cmask = (mode_t)CMASK;

	/*
	 * Set up default resource limits.
	 */
	bcopy(rlimits, u.u_rlimit, sizeof (struct rlimit64) * RLIM_NLIMITS);

	thread_init();		/* init thread_free list */
#ifdef LATER
	hrtinit();		/* init hires timer free list */
	itinit();		/* init interval timer free list */
#endif
	pid_init();		/* initialize pid (proc) table */

	init_pages_pp_maximum();
}

static struct  bootcode {
	char    letter;
	uint_t   bit;
} bootcode[] = {	/* See reboot.h */
	'a',	RB_ASKNAME,
	's',	RB_SINGLE,
	'i',	RB_INITNAME,
	'h',	RB_HALT,
	'b',	RB_NOBOOTRC,
	'd',	RB_DEBUG,
	'w',	RB_WRITABLE,
	'r',    RB_RECONFIG,
	'c',    RB_CONFIG,
	'v',	RB_VERBOSE,
	'f',	RB_FLUSHCACHE,
	'x',	RB_NOBOOTCLUSTER,
	0,	0,
};

char kern_bootargs[256];

/*
 * Parse the boot line to determine boot flags .
 */
void
bootflags(void)
{
	char *cp;
	int i;
	extern struct debugvec *dvec;
	extern char *initname;

	if (BOP_GETPROP(bootops, "boot-args", kern_bootargs) < 0) {
		cp = NULL;
		boothowto |= RB_ASKNAME;
	} else {
		cp = kern_bootargs;
		while (*cp && *cp != '-')
			cp++;

		if (*cp && *cp++ == '-')
			do {
				for (i = 0; bootcode[i].letter; i++) {
					if (*cp == bootcode[i].letter) {
						boothowto |= bootcode[i].bit;
						break;
					}
				}
				cp++;
			} while (bootcode[i].letter && *cp);
	}

	if (boothowto & RB_INITNAME) {
		/*
		 * XXX	This is a bit broken - shouldn't we
		 *	really be using the initpath[] above?
		 */
		while (*cp && *cp != ' ' && *cp != '\t')
			cp++;
		initname = cp;
	}

	if (boothowto & RB_HALT) {
		prom_printf("kernel halted by -h flag\n");
		prom_enter_mon();
	}

	/*
	 * If the boot flags say that kadb is there,
	 * test and see if it really is by peeking at DVEC.
	 * If is isn't, we turn off the RB_DEBUG flag else
	 * we call the debugger scbsync() routine.
	 */
	if ((boothowto & RB_DEBUG) != 0) {
		if (dvec == NULL || ddi_peeks((dev_info_t *)0,
		    (short *)dvec, (short *)0) != DDI_SUCCESS)
			boothowto &= ~RB_DEBUG;
	}
}

/*
 * Load a procedure into a thread.
 */
int
thread_load(
	kthread_t	*t,
	void		(*start)(),
	caddr_t		arg,
	size_t		len)
{
	caddr_t sp;
	size_t framesz;
	caddr_t argp;
	label_t ljb;
	long *p;
	extern void hat_map_kernel_stack_local(caddr_t, caddr_t);

	/*
	 * Push a "c" call frame onto the stack to represent
	 * the caller of "start".
	 */
	sp = t->t_stk;
	if (len != 0) {
		/*
		 * the object that arg points at is copied into the
		 * caller's frame.
		 */
		framesz = SA(len);
		sp -= framesz;
		if (sp < t->t_swap)	/* stack grows down */
			return (-1);
		argp = sp + SA(MINFRAME);
		if (on_fault(&ljb)) {
			no_fault();
			return (-1);
		}
		bcopy(arg, argp, len);
		no_fault();
		arg = (void *)argp;
	}
	/*
	 * Set up arguments (arg and len) on the caller's stack frame.
	 */
	p = (long *)sp;
	*--p = (long)len;
	*--p = (intptr_t)arg;
	*--p = (intptr_t)thread_exit;	/* threads that return, should exit */
	/*
	 * initialize thread to resume at (*start)().
	 */
	t->t_pc = (uintptr_t)start;
	t->t_sp = (uintptr_t)p;

	/*
	 * If necessary remap the kernel stack to node local pages.
	 */
	(void) hat_map_kernel_stack_local((caddr_t)t->t_stkbase,
		(caddr_t)t->t_stk);
	return (0);
}

/*
 * load user registers into lwp.
 */
void
lwp_load(klwp_t *lwp, gregset_t gregs)
{
	setgregs(lwp, gregs);
	lwptoregs(lwp)->r_efl = PSL_USER;
	lwp->lwp_eosys = JUSTRETURN;
	lwptot(lwp)->t_post_sys = 1;
}

/*
 * set syscall()'s return values for a lwp.
 */
void
lwp_setrval(klwp_t *lwp, int v1, int v2)
{
	lwptoregs(lwp)->r_efl &= ~PS_C;
	lwptoregs(lwp)->r_eax = v1;
	lwptoregs(lwp)->r_edx = v2;
}

/*
 * set syscall()'s return values for a lwp.
 */
void
lwp_setsp(klwp_t *lwp, caddr_t sp)
{
	lwptoregs(lwp)->r_uesp = (int)sp;
}

/*
 * Copy regs from parent to child.
 */
void
lwp_forkregs(klwp_t *lwp, klwp_t *clwp)
{
	bcopy(lwp->lwp_regs, clwp->lwp_regs, sizeof (struct regs));
}

/*
 * This function is unused on x86.
 */
/*ARGSUSED*/
void
lwp_freeregs(klwp_t *lwp, int isexec)
{}

/*
 * Clear registers on exec(2).
 */
void
setregs(void)
{
	ulong_t entry;
	struct regs *rp;
	klwp_t *lwp = ttolwp(curthread);

	entry = (ulong_t)u.u_exdata.ux_entloc;

	/*
	 * Initialize user registers.
	 * (Note: User stack pointer is already initialized by buildstack()
	 *	  or fastbuildstack()).
	 */
	rp = lwptoregs(lwp);
	rp->r_cs = USER_CS;
	rp->r_ds = rp->r_ss = rp->r_es = USER_DS;
	rp->r_gs = rp->r_fs = 0;
	rp->r_eip = entry;
	rp->r_efl = PSL_USER;	/* initial user EFLAGS */
	rp->r_eax = rp->r_ebx = rp->r_ecx = rp->r_edx = rp->r_edi =
	rp->r_esi = rp->r_ebp = 0;
	lwp->lwp_eosys = JUSTRETURN;
	curthread->t_post_sys = 1;

	/*
	 * Here we initialize minimal fpu state.
	 * The rest is done at the first floating
	 * point instruction that a process executes.
	 */
	lwp->lwp_pcb.pcb_fpu.fpu_flags = 0;
}

/*
 * Construct the execution environment for the user's signal
 * handler and arrange for control to be given to it on return
 * to userland.  The library code now calls setcontext() to
 * clean up after the signal handler, so sigret() is no longer
 * needed.
 *
 * i86:
 *	For COFF Binary Compatibility we assume that COFF executables use
 *	SVR532 libraries and therefore we need sigret() to clean up
 *	after signal handler.
 */

/* These structure defines what is pushd on the stack */

/* SVR4/ABI signal frame */
struct argpframe {
	void		(*retadr)();
	uint_t		signo;
	siginfo_t	*sip;
	ucontext_t	*ucp;
};

/* SVR532 compatible frame */
struct compat_frame {
	void		(*retadr)();
	uint_t		signo;
	gregset_t	gregs;
	char		*fpsp;
	char		*wsp;
};

int
sendsig(int sig, k_siginfo_t *sip, void (*hdlr)())
{
	/*
	 * 'volatile' is needed to ensure that values are
	 * correct on the error return from on_fault().
	 */
	volatile int minstacksz; /* min stack required to catch signal */
	int newstack = 0;	/* if true, switching to altstack */
	label_t ljb;
	volatile caddr_t sp;
	caddr_t fp;
	struct regs *regs;
	volatile greg_t upc;
	proc_t *volatile p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);
	ucontext_t *volatile tuc = NULL;
	ucontext_t *uc;
	siginfo_t *sip_addr;
	int	old_style = 0;	/* flag to indicate SVR532 signal frame */
	int setsegregs = 0; /* flag to set segment registers */
	volatile int mapped;

	regs = lwptoregs(lwp);
	upc = regs->r_eip;

	/*
	 * Determine if we need to generate SVR532 compatible
	 * signal frame.
	 */
	old_style = is_coff_proc(p);

	if (old_style) {
		minstacksz = sizeof (struct compat_frame) +
				sizeof (ucontext_t);
		sip = NULL;
	} else {
		minstacksz = sizeof (struct argpframe) +
				sizeof (ucontext_t);
	}

	if (sip != NULL)
		minstacksz += sizeof (siginfo_t);

	/*
	 * Figure out whether we will be handling this signal on
	 * an alternate stack specified by the user. Then allocate
	 * and validate the stack requirements for the signal handler
	 * context. on_fault will catch any faults.
	 */
	newstack = (sigismember(&u.u_sigonstack, sig) &&
	    !(lwp->lwp_sigaltstack.ss_flags & (SS_ONSTACK|SS_DISABLE)));

	/* check if we need to reload the selector registers */
	if (((regs->r_ss & 0xffff) != USER_DS) ||
	    ((regs->r_cs & 0xffff) != USER_CS))
		setsegregs++;

	if (newstack != 0) {
		sp = (caddr_t)(SA((uintptr_t)lwp->lwp_sigaltstack.ss_sp) +
		    SA(lwp->lwp_sigaltstack.ss_size) - STACK_ALIGN -
			SA(minstacksz));
	} else {
		/*
		 * If the stack segment selector is not the 386 user data
		 * data selector, convert the SS:SP to the equivalent 386
		 * virtual address and set the flag to load SS and DS with
		 * correct values after saving the current context.
		 */
		if ((regs->r_ss & 0xffff) != USER_DS) {
			char *dp;
			struct dscr *ldt = (struct dscr *)p->p_ldt;

			if (ldt == NULL)
				return (0); /* can't setup signal frame */
			dp = (char *)(ldt + seltoi(regs->r_ss));
			sp = (caddr_t)(regs->r_uesp & 0xFFFF);
			sp += (dp[7] << 24) | (*(int *)&dp[2] & 0x00FFFFFF);
			sp -= SA(minstacksz);
		} else
			sp = (caddr_t)regs->r_uesp - SA(minstacksz);
	}

	fp = sp + SA(minstacksz);

	/*
	 * Make sure process hasn't trashed its stack.
	 */
	if (((uintptr_t)sp & (STACK_ALIGN - 1)) != 0 ||
	    sp >= (caddr_t)USERLIMIT ||
	    fp >= (caddr_t)USERLIMIT) {
#ifdef DEBUG
		printf("sendsig: bad signal stack pid=%d, sig=%d\n",
		    p->p_pid, sig);
		printf("sigsp = 0x%p, action = 0x%p, upc = 0x%x\n",
		    (void *)sp, (void *)hdlr, upc);

		if (((uintptr_t)sp & (STACK_ALIGN - 1)) != 0)
		    printf("bad stack alignment\n");
		else
		    printf("sp above USERLIMIT\n");
#endif
		return (0);
	}

	mapped = 0;
	if (p->p_warea)
		mapped = pr_mappage((caddr_t)sp, SA(minstacksz), S_WRITE, 1);

	if (on_fault(&ljb))
		goto badstack;

	if (sip != NULL) {
		fp -= sizeof (siginfo_t);
		uzero((caddr_t)fp, sizeof (siginfo_t));
		copyout_noerr(sip, fp, sizeof (*sip));
		sip_addr = (siginfo_t *)fp;

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
				suword32_noerr(&(sip_addr->si_sysarg[i]),
				    (uint32_t)lwp->lwp_arg[i]);
			copyout_noerr(curthread->t_rprof->rp_state,
			    sip_addr->si_mstate,
			    sizeof (curthread->t_rprof->rp_state));
		}
	} else {
		sip_addr = (siginfo_t *)NULL;
	}

	/* save the current context on the user stack */
	fp -= sizeof (ucontext_t);
	uc = (ucontext_t *)fp;
	tuc = kmem_alloc(sizeof (*tuc), KM_SLEEP);
	savecontext(tuc, lwp->lwp_sigoldmask);
	copyout_noerr(tuc, uc, sizeof (*tuc));
	lwp->lwp_oldcontext = (uintptr_t)uc;

	if (newstack != 0)
		lwp->lwp_sigaltstack.ss_flags |= SS_ONSTACK;

	/*
	 * Set up user registers for execution of signal handler.
	 */
	if (old_style) { /* SVR532 compatible signal frame */
		struct compat_frame cframe;

		cframe.retadr = u.u_sigreturn;
		cframe.signo = sig;
		bcopy(tuc->uc_mcontext.gregs, cframe.gregs,
		    sizeof (gregset_t));
		cframe.fpsp = (char *)&uc->uc_mcontext.fpregs.fp_reg_set;
		cframe.wsp = (char *)&uc->uc_mcontext.fpregs.f_wregs[0];
		copyout_noerr(&cframe, sp, sizeof (cframe));
	} else {
		struct argpframe aframe;

		aframe.sip = sip_addr;
		aframe.ucp = uc;
		aframe.signo = sig;
		/* Shouldn't return via this; if they do, fault. */
		aframe.retadr = (void (*)())0xFFFFFFFF;
		copyout_noerr(&aframe, sp, sizeof (aframe));
	}

	kmem_free(tuc, sizeof (*tuc));
	tuc = NULL;

	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)sp, SA(minstacksz), S_WRITE, 1);

	regs->r_uesp = (greg_t)sp;
	regs->r_eip = (greg_t)hdlr;
	regs->r_efl = PSL_USER | (regs->r_efl & PS_IOPL);

	if (setsegregs) { /* we need to correct the segment registers */
		regs->r_cs = USER_CS;
		regs->r_ds = regs->r_es = regs->r_ss = USER_DS;
	}

	/*
	 * Don't set lwp_eosys here.  sendsig() is called via psig() after
	 * lwp_eosys is handled, so setting it here would affect the next
	 * system call.
	 */
	return (1);

badstack:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)sp, SA(minstacksz), S_WRITE, 1);
	if (tuc)
		kmem_free(tuc, sizeof (*tuc));
#ifdef DEBUG
	printf("sendsig: bad signal stack pid=%d, sig=%d\n",
	    p->p_pid, sig);
	printf("on fault, sigsp = 0x%p, action = 0x%p, upc = 0x%x\n",
	    (void *)sp, (void *)hdlr, upc);
#endif
	return (0);
}

/*
 * Restore user's context after execution of user signal handler
 * This code restores all registers to what they were at the time
 * signal occured. So any changes made to things like flags will
 * disappear.
 *
 * The saved context is assumed to be at esp+xxx address on the user's
 * stack. If user has mucked with his stack, he will suffer.
 * Called from the sig_clean.
 *
 * On entry, assume all registers are pushed.  r0ptr points to registers
 * on stack.
 * This function returns like other system calls.
 *
 * Note:
 *	It is assumed that this routine is needed only to support SVR532
 *	compatible signal handling mechanism. And the SVR4 compatible
 *	signal handling uses setcontext(2) system call to cleanup the
 *	signal frame.
 */

void
sigclean(int *r0ptr)		/* registers on stack */
{
	struct compat_frame *cframe;
	ucontext_t	uc, *ucp;

	/*
	 * The user's stack pointer currently points into compat_frame
	 * on the user stack.  Adjust it to the base of compat_frame.
	 */
	cframe = (struct compat_frame *)(r0ptr[UESP] - 2 * sizeof (int));

	ucp = (ucontext_t *)(cframe + 1);

	if (copyin(ucp, &uc, sizeof (ucontext_t))) {
		int v;
#ifdef C2_AUDIT
		if (audit_active)		/* audit core dump */
			audit_core_start(SIGSEGV);
#endif
		v = core(SIGSEGV);
#ifdef C2_AUDIT
		if (audit_active)		/* audit core dump */
			audit_core_finish((v)?CLD_KILLED:CLD_DUMPED);
#endif
		exit((v ? CLD_KILLED : CLD_DUMPED), SIGSEGV);
		return;
	}

	/*
	 * Old stack frame has gregs in a different place.
	 * Copy it into the ucontext structure.
	 */
	if (copyin((caddr_t)&cframe->gregs, (caddr_t)&uc.uc_mcontext.gregs,
	    sizeof (gregset_t))) {
		int v;
#ifdef C2_AUDIT
		if (audit_active)		/* audit core dump */
			audit_core_start(SIGSEGV);
#endif
		v = core(SIGSEGV);
#ifdef C2_AUDIT
		if (audit_active)		/* audit core dump */
			audit_core_finish((v)?CLD_KILLED:CLD_DUMPED);
#endif
		exit((v ? CLD_KILLED : CLD_DUMPED), SIGSEGV);
		return;
	}

	restorecontext(&uc);
}

/*
 * Determine if the process is from COFF executable (532 signal compatibility).
 * Returns true for COFF. THIS COULD BE A MACRO.
 * (Note: Currently we don't have support for running COFF binaries yet.)
 */

int
is_coff_proc(proc_t *p)
{
	extern short elfmagic;

#ifdef XXX_COFF
	extern short coffmagic;

	if (p->p_user.u_execid == coffmagic) /* if COFF then return TRUE */
		return (1);
	else
		return (0);
#else
	if (p->p_user.u_execid == elfmagic) /* if ELF then return FALSE */
		return (0);
	else
		return (1);
#endif XXX_COFF
}

#if !defined(lwp_getdatamodel)

/*
 * Return the datamodel of the given lwp.
 */
/*ARGSUSED*/
model_t
lwp_getdatamodel(klwp_t *lwp)
{
	return (DATAMODEL_ILP32);
}

#endif	/* !lwp_getdatamodel */

#if !defined(get_udatamodel)

model_t
get_udatamodel(void)
{
	return (lwp_getdatamodel(curthread->t_lwp));
}

#endif	/* !get_udatamodel */
