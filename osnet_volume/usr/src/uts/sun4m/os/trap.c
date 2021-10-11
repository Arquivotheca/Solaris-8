/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)trap.c	1.145	99/10/22 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <sys/syscall.h>
#include <sys/cpuvar.h>
#include <sys/vm.h>
#include <sys/sysinfo.h>
#include <sys/fault.h>
#include <sys/frame.h>
#include <sys/stack.h>
#include <sys/aio_impl.h>
#include <sys/mmu.h>
#include <sys/pcb.h>
#include <sys/cpu.h>
#include <sys/machpcb.h>
#include <sys/pte.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/kmem.h>
#include <sys/vtrace.h>
#include <sys/x_call.h>
#include <sys/simulate.h>
#include <sys/prsystm.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/fpu/fpusystm.h>
#include <sys/cmn_err.h>
#include <sys/mutex_impl.h>
#include <sys/module.h>
#include <sys/tnf.h>
#include <sys/tnf_probe.h>
#include <sys/spl.h>
#include <sys/devaddr.h>
#include <sys/strlog.h>
#include <sys/panic.h>

#include <vm/hat.h>
#include <vm/hat_srmmu.h>

#include <vm/seg_kmem.h>
#include <vm/as.h>
#include <vm/seg.h>

#include <sys/procfs.h>

#include <sys/modctl.h>

#include <sys/reboot.h>
#include <sys/debug.h>
#include <sys/debug/debug.h>

#pragma weak vik_fixfault

extern int
mmu_handle_ebe(uint_t mmu_fsr, caddr_t addr, uint_t type, struct regs *rp,
	enum seg_rw rw);

#define	USER	0x10000		/* user-mode flag added to type */

#ifdef SYSCALLTRACE
int syscalltrace = 0;
#endif /* SYSCALLTRACE */

char	*trap_type[] = {
	"Zero",
	"Text fault",
	"Illegal instruction",
	"Privileged instruction",
	"Floating point unit disabled",
	"Window overflow",
	"Window underflow",
	"Memory address alignment",
	"Floating point exception",
	"Data fault",
	"Tag overflow",
	"Trap 0x0B",
	"Trap 0x0C",
	"Trap 0x0D",
	"Trap 0x0E",
	"Trap 0x0F",
	"Spurious interrupt",
	"Interrupt level 1",
	"Interrupt level 2",
	"Interrupt level 3",
	"Interrupt level 4",
	"Interrupt level 5",
	"Interrupt level 6",
	"Interrupt level 7",
	"Interrupt level 8",
	"Interrupt level 9",
	"Interrupt level A",
	"Interrupt level B",
	"Interrupt level C",
	"Interrupt level D",
	"Interrupt level E",
	"Interrupt level F",
	"AST",
};

#define	TRAP_TYPES	(sizeof (trap_type) / sizeof (trap_type[0]))

static int tudebug = 0;
static int tudebugbpt = 0;
static int tudebugfpe = 0;

static int alignfaults = 0;

#if defined(TRAPDEBUG) || defined(lint)
static int tdebug = 0;
static int lodebug = 0;
static int faultdebug = 0;
#else
#define	tdebug	0
#define	lodebug	0
#define	faultdebug	0
#endif defined(TRAPDEBUG) || defined(lint)

static int ppmapfault = 0;
static caddr_t lastppmapfault = (caddr_t)0;

static void showregs(unsigned, struct regs *, caddr_t, uint_t, enum seg_rw);
void (*hotpatch_safety_trap_handler)(caddr_t) = NULL;

struct trap_info {
	struct regs *trap_regs;
	uint_t trap_type;
	caddr_t trap_addr;
	uint_t trap_mmu_fsr;
	enum seg_rw trap_rw;
};

static int
die(uint_t type, struct regs *rp, caddr_t addr, uint_t mmu_fsr, enum seg_rw rw)
{
	char *trap_name = type < TRAP_TYPES ? trap_type[type] : "trap";
	struct trap_info ti;

	ti.trap_regs = rp;
	ti.trap_type = type;
	ti.trap_addr = addr;
	ti.trap_mmu_fsr = mmu_fsr;
	ti.trap_rw = rw;

	curthread->t_panic_trap = &ti;

	if (type == T_DATA_FAULT && addr < (caddr_t)KERNELBASE) {
		panic("BAD TRAP: type=%x (%s) rp=%p addr=%p mmu_fsr=%x rw=%x "
		    "occurred in module \"%s\" due to %s",
		    type, trap_name, (void *)rp, (void *)addr, mmu_fsr, rw,
		    mod_containing_pc((caddr_t)rp->r_pc),
		    addr < (caddr_t)PAGESIZE ?
		    "a NULL pointer dereference" :
		    "an illegal access to a user address");
	} else {
		panic("BAD TRAP: type=%x (%s) rp=%p addr=%p mmu_fsr=%x rw=%x",
		    type, trap_name, (void *)rp, (void *)addr, mmu_fsr, rw);
	}

	return (0);	/* avoid optimization of restore in call's delay slot */
}

void
mmu_print_sfsr(uint_t sfsr)
{
	printf("MMU sfsr=%x:", sfsr);
	switch (sfsr & MMU_SFSR_FT_MASK) {
	case MMU_SFSR_FT_NO:
		printf(" No Error");
		break;
	case MMU_SFSR_FT_INV:
		printf(" Invalid Address");
		break;
	case MMU_SFSR_FT_PROT:
		printf(" Protection Error");
		break;
	case MMU_SFSR_FT_PRIV:
		printf(" Privilege Violation");
		break;
	case MMU_SFSR_FT_TRAN:
		printf(" Translation Error");
		break;
	case MMU_SFSR_FT_BUS:
		printf(" Bus Access Error");
		break;
	case MMU_SFSR_FT_INT:
		printf(" Internal Error");
		break;
	case MMU_SFSR_FT_RESV:
		printf(" Reserved Error");
		break;
	default:
		printf(" Unknown Error");
		break;
	}
	if (sfsr) {
		printf(" on %s %s %s at level %d",
			(sfsr & MMU_SFSR_AT_SUPV)? "supv" : "user",
			(sfsr & MMU_SFSR_AT_INSTR)? "instr" : "data",
			(sfsr & MMU_SFSR_AT_STORE)? "store" : "fetch",
			(sfsr & MMU_SFSR_LEVEL) >> MMU_SFSR_LEVEL_SHIFT);
		if (sfsr & MMU_SFSR_BE)
			printf("\n\tM-Bus Bus Error");
		if (sfsr & MMU_SFSR_TO)
			printf("\n\tM-Bus Timeout Error");
		if (sfsr & MMU_SFSR_UC)
			printf("\n\tM-Bus Uncorrectable Error");
	}
	printf("\n");
}

#ifdef DEBUG
int trap_stale_tlb;
#endif DEBUG

static int
get_fault_type(uint_t type, struct regs *rp, caddr_t addr, uint_t mmu_fsr,
	enum seg_rw rw, enum fault_type *fault_type)
{
	int retval;
	union ptpe ptpe;
	uint_t probe_fsr;
	int mmu_fault_type;
	struct as *as;
	int level;
	struct ptbl *ptbl;
	kmutex_t *mtx;

	*fault_type = 0;

	/* Obtain the fault type field from the fault status register */
	mmu_fault_type = X_FAULT_TYPE(mmu_fsr);
	if ((uint_t)addr >= KERNELBASE)
		as = &kas;
	else
		as = curproc->p_as;

	if (mmu_fault_type == FT_TRANS_ERROR) {

		/*
		 * We do a probe here to test the validity of translation
		 * again. This is to protect against translation error
		 * that has been caused by transient bus error during
		 * h/w table walk.
		 */
		(void) srmmu_ptefind(as, addr, &level, &ptbl, &mtx,
			LK_PTBL_SHARED);
		ptpe.ptpe_int = mmu_probe((caddr_t)addr, &probe_fsr);
		unlock_ptbl(ptbl, mtx);

		if (ptpe.ptpe_int == 0) {
			mmu_fsr = probe_fsr;
			mmu_fault_type = X_FAULT_TYPE(mmu_fsr);
			if (mmu_fault_type != FT_TRANS_ERROR)
				mmu_fault_type = FT_NONE;
		} else {
			/*
			 * If the translation error no longer exists,
			 * we simply fall through the trap code and
			 * retry the fault instruction. If the transient
			 * translation error recorded here is not the
			 * reason that caused the current trap, when
			 * we retry the instruction, the real reason
			 * will be recorded in sfsr.
			 */
			mmu_fault_type = FT_NONE;
		}
	} else if ((mmu_fsr & MMU_SFSR_OW) && ((type & 0xf) == T_TEXT_FAULT)) {
		/*
		 * We got two text faults in a roll. The IU saved the pc
		 * of the first text fault in the trap window, while
		 * the sfsr records the cause of the second text fault.
		 * So we have to do a mmu probe on the saved pc to find out
		 * the real cause of the first fault.
		 */
		(void) srmmu_ptefind(as, (caddr_t)rp->r_pc, &level, &ptbl,
			&mtx, LK_PTBL_SHARED);

		ptpe.ptpe_int = mmu_probe((caddr_t)rp->r_pc, &probe_fsr);
		unlock_ptbl(ptbl, mtx);

		if (ptpe.ptpe_int == 0) {
			mmu_fsr = probe_fsr;
			mmu_fault_type = X_FAULT_TYPE(mmu_fsr);
		} else if (USERMODE(rp->r_psr) && pte_konly(&(ptpe.pte))) {
			mmu_fault_type = FT_PRIV_ERROR;
		} else if ((ptpe.pte.AccessPermissions == MMU_STD_SRUR) ||
		    (ptpe.pte.AccessPermissions == MMU_STD_SRWURW) ||
		    (ptpe.pte.AccessPermissions == MMU_STD_SRWUR)) {
			mmu_fault_type = FT_PROT_ERROR;
		} else {
			mmu_fault_type = FT_NONE;
		}
	} else if (addr >= (caddr_t)KERNELBASE &&
		mmu_fault_type == FT_INVALID_ADDR && !(type & USER)) {
		/*
		 * Re-check the address.
		 * If it's valid, we got here because of a pte
		 * that was temporarily invalidated by the ptesync
		 * code (see the module mp writepte functions)
		 * The check is done for kernel addresses only since user
		 * addresses always have a segment driver which will
		 * re-validate the pte.
		 */
		(void) srmmu_ptefind(&kas, addr, &level, &ptbl, &mtx,
			LK_PTBL_SHARED);
		ptpe.ptpe_int = mmu_probe(addr, &probe_fsr);
		unlock_ptbl(ptbl, mtx);

		if (ptpe.ptpe_int != 0)
			mmu_fault_type = FT_NONE;
	}

	if (mmu_fsr & MMU_SFSR_FATAL) {
		if (lodebug)
			showregs(type, rp, addr, mmu_fsr, rw);
		(void) die(type, rp, addr, mmu_fsr, rw);
		/*NOTREACHED*/
	}

	switch (mmu_fault_type) {
	case FT_INVALID_ADDR:
		if (tdebug)
			printf("Invalid address\n");
		*fault_type = F_INVAL;
		retval = FT_INVALID_ADDR;
		break;

	case FT_PROT_ERROR:
		if (tdebug)
			printf("Protection fault\n");

		if (rw == S_WRITE) {
			struct pte *mpte, xmpte;
			union ptpe tlb;
			int cxn;
			extern int static_ctx;

			mpte = srmmu_ptefind(as, addr, &level,
			    &ptbl, &mtx, LK_PTBL_SHARED);
			mmu_readpte(mpte, &xmpte);

			if (pte_valid(&xmpte)) {
				tlb.ptpe_int = mmu_probe(addr, &probe_fsr);
				if ((tlb.pte.AccessPermissions !=
				    xmpte.AccessPermissions) &&
				    (tlb.pte.PhysicalPageNumber ==
				    xmpte.PhysicalPageNumber)) {
					/*
					 * We are here because TLB was not
					 * flushed on purposely, we just need
					 * to flush the TLB here. Or it could
					 * be some other thread has resolved
					 * the fault for us. We just need to
					 * flush out stale tlbs.
					 */
					if (!static_ctx && as != &kas) {
						hat_enter(as->a_hat);
					}

					cxn = astosrmmu(as)->s_ctx;

					/*
					 * Flush only if a valid context exists.
					 */
					if (cxn != -1) {
						srmmu_tlbflush(level, addr, cxn,
						    FL_LOCALCPU);
					}

					if (!static_ctx && as != &kas) {
						hat_exit(as->a_hat);
					}
					unlock_ptbl(ptbl, mtx);

#ifdef DEBUG
					trap_stale_tlb++;
#endif DEBUG
					/*
					 * Reset fault type now so it
					 * would be like fault has been
					 * resolved.
					 */
					*fault_type = 0;
					retval = FT_NONE;
					break;
				}
			}
			unlock_ptbl(ptbl, mtx);
		}

		*fault_type = F_PROT;
		retval = mmu_fault_type;
		break;

	case FT_PRIV_ERROR:
		if (tdebug)
			printf("Priviledge fault\n");
		*fault_type = F_PROT;
		retval = mmu_fault_type;
		break;

	case FT_NONE:
		if (tdebug)
			printf("FT_NONE\n");
		if (lodebug)
			showregs(type, rp, addr, mmu_fsr, rw);
		retval = FT_NONE;
		break;

	case FT_TRANS_ERROR:
	case FT_ACC_BUSERR:
	case FT_INTERNAL:
	default:
		traceback((caddr_t)rp->r_sp);
		showregs(type, rp, addr, mmu_fsr, rw);
		cmn_err(CE_PANIC, "trap: unexpected MMU trap");
		/*NOTREACHED*/
	}
	return (retval);
}

/*
 * Called from the trap handler when a processor trap occurs.
 * Addr, mmu_fsr and rw only are passed for text and data faults.
 *
 * Note:  All user-level traps that might call stop() must exit
 * trap() by 'goto out' or by falling through.  This is so that
 * prdostep() can be called to enable hardware single stepping.
 */
/*VARARGS2*/
void
trap(uint_t type, struct regs *rp, caddr_t addr, uint_t mmu_fsr, enum seg_rw rw)
{
	proc_t *p = ttoproc(curthread);
	klwp_id_t lwp = ttolwp(curthread);
	struct machpcb *mpcb = NULL;
	uint_t lofault;
	faultcode_t pagefault(), res;
	enum fault_type fault_type;
	k_siginfo_t siginfo;
	uint_t fault = 0;
	int stepped;
	greg_t oldpc;
	int mstate;
	char *badaddr;
	int watchcode;
	int watchpage;
	extern int viking_mfar_bug;
	extern int unimpflush();

	CPU_STAT_ADDQ(CPU, cpu_sysinfo.trap, 1);

	ASSERT(curthread->t_schedflag & TS_DONT_SWAP);
	if (tdebug)
		showregs(type, rp, addr, mmu_fsr, rw);

	if (USERMODE(rp->r_psr)) {
		/*
		 * Set up the current cred to use during this trap. u_cred
		 * no longer exists.  t_cred is used instead.
		 * The current process credential applies to the thread for
		 * the entire trap.  If trapping from the kernel, this
		 * should already be set up.
		 */
		if (curthread->t_cred != p->p_cred) {
			crfree(curthread->t_cred);
			curthread->t_cred = crgetcred();
		}
		ASSERT(lwp != NULL);
		type |= USER;
		ASSERT(lwp->lwp_regs == rp);
		lwp->lwp_state = LWP_SYS;
		mpcb = lwptompcb(lwp);
#ifdef NPROBE
		if (curthread->t_proc_flag & TP_MSACCT)
#else
		if ((curthread->t_proc_flag & TP_MSACCT) || tnf_tracing_active)
#endif
		{
			switch (type) {
			case T_WIN_OVERFLOW + USER:
			case T_WIN_UNDERFLOW + USER:
			case T_SYS_RTT_PAGE + USER:
			case T_DATA_FAULT + USER:
				mstate = LMS_DFAULT;
				break;
			case T_TEXT_FAULT + USER:
				mstate = LMS_TFAULT;
				break;
			default:
				mstate = LMS_TRAP;
				break;
			}
			/* Kernel probe */
			TNF_PROBE_1(thread_state, "thread", /* CSTYLED */,
			    tnf_microstate, state, mstate);
			if (curthread->t_proc_flag & TP_MSACCT)
				mstate = new_mstate(curthread, mstate);
			else
				mstate = LMS_USER;
		} else {
			mstate = LMS_USER;
		}
		siginfo.si_signo = 0;
		stepped =
		    lwp->lwp_pcb.pcb_step != STEP_NONE &&
		    ((oldpc = rp->r_pc), prundostep()) &&
		    addr == (caddr_t)oldpc;
		/* this assignment must not precede call to prundostep() */
		oldpc = rp->r_pc;
	}

	TRACE_1(TR_FAC_TRAP, TR_C_TRAP_HANDLER_ENTER,
		"C_trap_handler_enter:type %x", type);

	/*
	 * Take any pending floating point exceptions now.
	 * If the floating point unit has an exception to handle,
	 * just return to user-level to let the signal handler run.
	 * The instruction that got us to trap() will be reexecuted on
	 * return from the signal handler and we will trap to here again.
	 * This is necessary to disambiguate simultaneous traps which
	 * happen when a floating-point exception is pending and a
	 * machine fault is incurred.
	 */
	if (type & USER) {
		/*
		 * FP_TRAPPED is set only by sendsig() when it copies
		 * out the floating-point queue for the signal handler.
		 * It is set there so we can test it here and in syscall().
		 */
		mpcb->mpcb_flags &= ~FP_TRAPPED;
		syncfpu();
		if (mpcb->mpcb_flags & FP_TRAPPED) {
			/*
			 * trap() has have been called recursively and may
			 * have stopped the process, so do single step
			 * support for /proc.
			 */
			mpcb->mpcb_flags &= ~FP_TRAPPED;
			goto out;
		}
	}

	switch (type) {

	default:
		/*
		 * Check for user software trap.
		 */
		if (type & USER) {
			if (tudebug)
				showregs(type, rp, (caddr_t)0, 0, S_OTHER);
			if (((type & ~USER) >= T_SOFTWARE_TRAP) ||
			    ((type & ~USER) & CP_BIT)) {
				bzero((caddr_t)&siginfo, sizeof (siginfo));
				siginfo.si_signo = SIGILL;
				siginfo.si_code  = ILL_ILLTRP;
				siginfo.si_addr  = (caddr_t)rp->r_pc;
				siginfo.si_trapno = type &~ USER;
				fault = FLTILL;
				break;
			}
		}
		(void) die(type, rp, addr, mmu_fsr, rw);
		/*NOTREACHED*/

	case T_ALIGNMENT:	/* supv alignment error */
		if (curthread->t_lofault && curthread->t_onfault) {
			label_t *ftmp;

			ftmp = curthread->t_onfault;
			curthread->t_onfault = NULL;
			curthread->t_lofault = 0;
			TRACE_0(TR_FAC_TRAP, TR_TRAP_END, "trap_end");
			longjmp(ftmp);
		}
		(void) die(type, rp, addr, mmu_fsr, rw);
		/*NOTREACHED*/

	case T_DATA_STORE:		/* store buffer exception */
	case T_DATA_STORE + USER:	/* store buffer exception */
		/*
		 * Check for module specific extended bus errors.
		 */
		if (mmu_fsr & MMU_SFSR_EBE)
			if (mmu_handle_ebe(mmu_fsr, addr, type, rp, rw))
				return;

		/*
		 * XXX FIXME:  This will just cause a panic.  We should
		 * attempt a graceful recovery!
		 */
		if (tdebug) {
			printf("T_DATA_STORE mmu_fsr %x ", mmu_fsr);
			showregs(type, rp, addr, mmu_fsr, rw);
		}
		/* XXX Was a goto die? */
		(void) die(type, rp, addr, mmu_fsr, rw);
		/*NOTREACHED*/

	case T_TEXT_ERROR:		/* system text access error */
	case T_DATA_ERROR:		/* system data access error */
	case T_TEXT_FAULT:		/* system text access exception */

		/*
		 * Check for module specific extended bus errors.
		 */
		if (mmu_fsr & MMU_SFSR_EBE)
			if (mmu_handle_ebe(mmu_fsr, addr, type, rp, rw))
				return;

		if (lodebug)
			showregs(type, rp, addr, mmu_fsr, rw);
		(void) die(type, rp, addr, mmu_fsr, rw);
		/*NOTREACHED*/

	case T_DATA_FAULT:		/* system data access exception */

		/* may have been expected by C (e.g. bus probe) */
		if (curthread->t_nofault) {
			label_t *ftmp;

			ftmp = curthread->t_nofault;
			curthread->t_nofault = 0;
			TRACE_0(TR_FAC_TRAP, TR_C_TRAP_HANDLER_EXIT,
				"C_trap_handler_exit");
			TRACE_0(TR_FAC_TRAP, TR_TRAP_END, "trap_end");
			longjmp(ftmp);
		}

		/* viking has some fault chip bugs */
		if (viking_mfar_bug)
			vik_fixfault(rp, &addr, mmu_fsr);

		/*
		 * Check for module specific extended bus errors.
		 */
		if (mmu_fsr & MMU_SFSR_EBE) {
			if (mmu_handle_ebe(mmu_fsr, addr, type, rp, rw))
				return;
			if ((mmu_fsr & (MMU_SFSR_TO | MMU_SFSR_BE)) ==
				(mmu_fsr & (MMU_SFSR_TO | MMU_SFSR_BE))) {

				/* Ignore timeouts/buserror if lofault set */

				if (curthread->t_lofault) {
					if (lodebug) {
						showregs(type, rp, addr,
						    mmu_fsr, rw);
						traceback((caddr_t)rp->r_sp);
					}
					rp->r_g1 = EFAULT;
					rp->r_pc = curthread->t_lofault;
					rp->r_npc = curthread->t_lofault + 4;
					goto cleanup;
				}
				printf("KERNEL DATA ACCESS TIMEOUT\n");
			}
		}

		if (get_fault_type(type, rp, addr, mmu_fsr,
		    rw, &fault_type) == FT_NONE)
			goto cleanup;

		ASSERT(fault_type == F_INVAL || fault_type == F_PROT);

		/*
		 * See if we can handle as pagefault. Save lofault
		 * across this. Here we assume that an address
		 * less than KERNELBASE is a user fault.
		 * We can do this as copy.s routines verify that the
		 * starting address is less than KERNELBASE before
		 * starting and because we know that we always have
		 * KERNELBASE mapped as invalid to serve as a "barrier".
		 * Because of SF9010 bug (see below), we must validate
		 * the bus error register when more than one bit is on.
		 */

		lofault = curthread->t_lofault;
		curthread->t_lofault = 0;

		if (curthread->t_proc_flag & TP_MSACCT)
			mstate = new_mstate(curthread, LMS_KFAULT);
		else
			mstate = LMS_SYSTEM;
		if (addr < (caddr_t)KERNELBASE) {
			if (lofault == 0)
				(void) die(type, rp, addr, mmu_fsr, rw);
			res = pagefault(addr, (int)fault_type, rw, 0);
			if (res == FC_NOMAP &&
			    addr < (caddr_t)USRSTACK &&
			    grow(addr))
				res = 0;
		} else {
			if (addr >= (caddr_t)PPMAPBASE &&
			    addr < (caddr_t)(PPMAPBASE + PPMAPSIZE)) {
				if (lastppmapfault != addr) {
					lastppmapfault = addr;
					ppmapfault++;
					if (curthread->t_proc_flag & TP_MSACCT)
						(void) new_mstate(curthread,
							mstate);
					return;
				}
			}

			res = pagefault(addr, (int)fault_type, rw, 1);
		}

		if (curthread->t_proc_flag & TP_MSACCT)
			(void) new_mstate(curthread, mstate);

		/*
		 * Restore lofault.  If we resolved the fault, exit.
		 * If we didn't and lofault wasn't set, die.
		 */
		curthread->t_lofault = lofault;
		if (res == 0)
			goto cleanup;

		if (lofault == 0)
			(void) die(type, rp, addr, mmu_fsr, rw);

		/*
		 * Cannot resolve fault.  Return to lofault.
		 */
		if (lodebug) {
			showregs(type, rp, addr, mmu_fsr, rw);
			traceback((caddr_t)rp->r_sp);
		}
		if (FC_CODE(res) == FC_OBJERR)
			res = FC_ERRNO(res);
		else
			res = EFAULT;
		rp->r_g1 = res;
		rp->r_pc = curthread->t_lofault;
		rp->r_npc = curthread->t_lofault + 4;
		goto cleanup;


	case T_WIN_OVERFLOW + USER:	/* need to load page for rwindow */
	case T_WIN_UNDERFLOW + USER:	/* need to load page for rwindow */
	case T_SYS_RTT_PAGE + USER:	/* need to load page for rwindow */
	case T_TEXT_ERROR + USER:	/* user text access error */
	case T_DATA_ERROR + USER:	/* user data access error */
	case T_DATA_FAULT + USER:	/* user data access exception */
	case T_TEXT_FAULT + USER:	/* user text access exception */

		/* viking has some fault chip bugs */
		if (viking_mfar_bug)
			vik_fixfault(rp, &addr, mmu_fsr);

		/*
		 * Check for module specific extended bus errors.
		 */
		if (mmu_fsr & MMU_SFSR_EBE) {

			/*
			 * If the fault is handled, we can just return.
			 * Otherwise the process must be sent a signal.
			 */
			if (mmu_handle_ebe(mmu_fsr, addr, type, rp, rw))
				return;

			res = FC_HWERR;
			goto badbus;
		}

		/*
		 * The SPARC processor prefetches instructions.
		 * The bus error register may also reflect faults
		 * that occur during prefetch in addition to the one
		 * that caused the current fault. For example:
		 *	st	[proterr]	! end of page
		 *	...			! invalid page
		 * will cause both SE_INVALID and SE_PROTERR.
		 * The gate array version of sparc (SF9010) has a bug
		 * which works as follows: When the chip does a prefetch
		 * to an invalid page the board loads garbage into the
		 * chip. If this garbage looks like a branch instruction
		 * a prefetch will be generated to some random address
		 * even though the branch is annulled. This can cause
		 * bits in the bus error register to be set. In this
		 * case we have to validate the bus error register bits.
		 * We only handle true SE_INVALID and SE_PROTERR faults.
		 * SE_MEMERR is given to memerr(), which will handle
		 * recoverable parity errors.
		 * All others cause the user to die.
		 */
		if (tdebug)
			printf(
			"T_DATA_FAULT | USER mmu_fsr %x X_FAULT_TYPE %x\n",
				mmu_fsr, (uint_t)X_FAULT_TYPE(mmu_fsr));

		if ((mmu_fsr & (MMU_SFSR_TO | MMU_SFSR_BE)) &&
			!(mmu_fsr & MMU_SFSR_UC)) {
			res = FC_HWERR;
			goto badbus;
		}

		if (stepped)
			fault_type = F_INVAL;
		else if (get_fault_type(type, rp, addr, mmu_fsr,
		    rw, &fault_type) == FT_NONE)
			goto out;

		ASSERT(fault_type == F_INVAL || fault_type == F_PROT);

		if (stepped) {
			res = FC_NOMAP;
		} else if (addr > (caddr_t)KERNELBASE &&
		    type == T_DATA_FAULT + USER &&
		    lwp->lwp_pcb.pcb_step != STEP_NONE) {
			/*
			 * While single-stepping on newer model sparc chips,
			 * we can get a text fault on a prefetch through
			 * nPC (and beyond) while also taking a data fault
			 * on a load or store instruction that is being
			 * single-stepped.  The resulting condition is a
			 * data fault with the offending address being the
			 * prefetch address, not the load or store address.
			 * To work around this problem, we emulate the
			 * load or store instruction with do_unaligned().
			 */
			res = FC_NOMAP;
			switch (do_unaligned(rp, &badaddr)) {
			case SIMU_SUCCESS:
				rp->r_pc = rp->r_npc;
				rp->r_npc += 4;
				stepped = 1;
				break;
			case SIMU_FAULT:
				addr = badaddr;
				break;
			}
		} else {
			caddr_t vaddr = addr;
			size_t sz;
			int ta;

			if (faultdebug) {
				char *fault_str;

				switch (rw) {
				case S_READ:
					fault_str = "read";
					break;
				case S_WRITE:
					fault_str = "write";
					break;
				case S_EXEC:
					fault_str = "exec";
					break;
				default:
					fault_str = "";
					break;
				}
				printf(
				    "user %s fault:  addr=0x%p mmu_fsr=0x%x\n",
				    fault_str, (void *)addr, mmu_fsr);
			}
			if (tdebug)
				printf(
				"trap pagefault: addr %p rw %x fault_type %x\n",
				    (void *)addr, rw, fault_type);

			ASSERT(!(curthread->t_flag & T_WATCHPT));
			watchpage = (p->p_warea != NULL &&
				type != T_WIN_OVERFLOW + USER &&
				type != T_WIN_UNDERFLOW + USER &&
				type != T_SYS_RTT_PAGE + USER &&
				pr_is_watchpage(addr, rw));

			if (!watchpage ||
			    (sz = instr_size(rp, &vaddr, rw)) <= 0)
				/* EMPTY */;
			else if ((watchcode = pr_is_watchpoint(&vaddr, &ta,
			    sz, NULL, rw)) != 0) {
				if (ta) {
					do_watch_step(vaddr, sz, rw,
						watchcode, rp->r_pc);
					fault_type = F_INVAL;
				} else {
					bzero((caddr_t)&siginfo,
						sizeof (siginfo));
					siginfo.si_signo = SIGTRAP;
					siginfo.si_code = watchcode;
					siginfo.si_addr = vaddr;
					siginfo.si_trapafter = 0;
					siginfo.si_pc = (caddr_t)rp->r_pc;
					fault = FLTWATCH;
					break;
				}
			} else {
				if (rw != S_EXEC &&
				    pr_watch_emul(rp, vaddr, rw))
					goto out;
				do_watch_step(vaddr, sz, rw, 0, 0);
				fault_type = F_INVAL;
			}

			if (p->p_warea != NULL &&
			    (type == T_WIN_OVERFLOW + USER ||
			    type == T_WIN_UNDERFLOW + USER ||
			    type == T_SYS_RTT_PAGE + USER)) {
				int dotwo = (type == T_WIN_UNDERFLOW + USER);
				if (copy_return_window(dotwo))
					goto out;
				fault_type = F_INVAL;
			}

			res = pagefault(addr, (int)fault_type, rw, 0);

			/*
			 * If pagefault succeed, ok.
			 * Otherwise grow the stack automatically.
			 */
			if (res == 0 ||
			    (res == FC_NOMAP &&
			    type != T_TEXT_ERROR + USER &&
			    type != T_TEXT_FAULT + USER &&
			    addr < (caddr_t)USRSTACK &&
			    grow(addr))) {
				lwp->lwp_lastfault = FLTPAGE;
				lwp->lwp_lastfaddr = addr;
				if (prismember(&p->p_fltmask, FLTPAGE)) {
					bzero(&siginfo, sizeof (siginfo));
					siginfo.si_addr = addr;
					(void) stop_on_fault(FLTPAGE, &siginfo);
				}
				goto out;
			}

			if (type == (T_TEXT_FAULT + USER) && res == FC_PROT)
				report_stack_exec(p, addr);

			if (tudebug)
				showregs(type, rp, addr, mmu_fsr, rw);
		}

		/*
		 * In the case where both pagefault and grow fail,
		 * set the code to the value provided by pagefault.
		 * We map all errors returned from pagefault() to SIGSEGV.
		 */
	badbus:
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_addr = addr;
		switch (FC_CODE(res)) {
		case FC_HWERR:
		case FC_NOSUPPORT:
			siginfo.si_signo = SIGBUS;
			siginfo.si_code = BUS_ADRERR;
			fault = FLTACCESS;
			break;
		case FC_ALIGN:
			siginfo.si_signo = SIGBUS;
			siginfo.si_code = BUS_ADRALN;
			fault = FLTACCESS;
			break;
		case FC_OBJERR:
			if ((siginfo.si_errno = FC_ERRNO(res)) != EINTR) {
				siginfo.si_signo = SIGBUS;
				siginfo.si_code = BUS_OBJERR;
				fault = FLTACCESS;
			}
			break;
		default:	/* FC_NOMAP or FC_PROT */
			siginfo.si_signo = SIGSEGV;
			siginfo.si_code =
			    (res == FC_NOMAP)? SEGV_MAPERR : SEGV_ACCERR;
			fault = FLTBOUNDS;
			/*
			 * If this is the culmination of a single-step,
			 * reset the addr, code, signal and fault to
			 * indicate a hardware trace trap.
			 */
			if (stepped) {
				pcb_t *pcb = &lwp->lwp_pcb;

				siginfo.si_signo = 0;
				fault = 0;
				if (pcb->pcb_step == STEP_WASACTIVE) {
					pcb->pcb_step = STEP_NONE;
					pcb->pcb_tracepc = NULL;
					oldpc = rp->r_pc - 4;
				}
				if (pcb->pcb_flags & NORMAL_STEP) {
					siginfo.si_code = TRAP_TRACE;
					siginfo.si_addr = (caddr_t)rp->r_pc;
					siginfo.si_signo = SIGTRAP;
					fault = FLTTRACE;
				}
				if (pcb->pcb_flags & WATCH_STEP)
					fault = undo_watch_step(&siginfo);
				pcb->pcb_flags &= ~(NORMAL_STEP|WATCH_STEP);
			}
			break;
		}
		break;

	case T_ALIGNMENT + USER:	/* user alignment error */
		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0, S_OTHER);
		/*
		 * If the user has to do unaligned references
		 * the ugly stuff gets done here.
		 */
		alignfaults++;
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		if (p->p_fixalignment) {
			if (do_unaligned(rp, &badaddr) == SIMU_SUCCESS) {
				rp->r_pc = rp->r_npc;
				rp->r_npc += 4;
				goto out;
			}
			siginfo.si_signo = SIGSEGV;
			siginfo.si_code = SEGV_MAPERR;
			siginfo.si_addr = badaddr;
			fault = FLTBOUNDS;
		} else {
			siginfo.si_signo = SIGBUS;
			siginfo.si_code = BUS_ADRALN;
			if (rp->r_pc & 3) {	/* offending address, if pc */
				siginfo.si_addr = (caddr_t)rp->r_pc;
			} else {
				if (calc_memaddr(rp, &badaddr) == SIMU_UNALIGN)
					siginfo.si_addr = badaddr;
				else
					siginfo.si_addr = (caddr_t)rp->r_pc;
			}
			fault = FLTACCESS;
		}
		break;

	case T_PRIV_INSTR + USER:	/* privileged instruction fault */
		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0, S_OTHER);
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_signo = SIGILL;
		siginfo.si_code = ILL_PRVOPC;
		siginfo.si_addr = (caddr_t)rp->r_pc;
		fault = FLTILL;
		break;

	case T_UNIMP_INSTR + USER:	/* illegal instruction fault */
		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0, S_OTHER);
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		/*
		 * Try to simulate the instruction.
		 */
		switch (simulate_unimp(rp, &badaddr)) {
		case SIMU_SUCCESS:
			/* skip the successfully simulated instruction */
			rp->r_pc = rp->r_npc;
			rp->r_npc += 4;
			goto out;
			/*NOTREACHED*/

		case SIMU_FAULT:
			siginfo.si_signo = SIGSEGV;
			siginfo.si_code = SEGV_MAPERR;
			siginfo.si_addr = badaddr;
			fault = FLTBOUNDS;
			break;

		case SIMU_DZERO:
			siginfo.si_signo = SIGFPE;
			siginfo.si_code = FPE_INTDIV;
			siginfo.si_addr = (caddr_t)rp->r_pc;
			fault = FLTIZDIV;
			break;

		case SIMU_UNALIGN:
			siginfo.si_signo = SIGBUS;
			siginfo.si_code = BUS_ADRALN;
			siginfo.si_addr = badaddr;
			fault = FLTACCESS;
			break;

		case SIMU_ILLEGAL:
		default:
			siginfo.si_signo = SIGILL;
			siginfo.si_code = ILL_ILLOPC;
			siginfo.si_addr = (caddr_t)rp->r_pc;
			fault = FLTILL;
			break;
		}
		break;

	case T_UNIMP_INSTR:	/* supv unimplemented instruction fault */
		flush_windows();
		if (simulate_unimp(rp, &badaddr) == SIMU_SUCCESS) {
			rp->r_pc = rp->r_npc;
			rp->r_npc += 4;
			goto cleanup;
		}
		(void) die(type, rp, addr, mmu_fsr, rw);
		/*NOTREACHED*/

	case T_IDIV0 + USER:		/* integer divide by zero */
	case T_DIV0 + USER:		/* integer divide by zero */
		if (tudebug && tudebugfpe)
			showregs(type, rp, (caddr_t)0, 0, S_OTHER);
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_signo = SIGFPE;
		siginfo.si_code = FPE_INTDIV;
		siginfo.si_addr = (caddr_t)rp->r_pc;
		fault = FLTIZDIV;
		break;

	case T_INT_OVERFLOW + USER:	/* integer overflow */
		if (tudebug && tudebugfpe)
			showregs(type, rp, (caddr_t)0, 0, S_OTHER);
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_signo = SIGFPE;
		siginfo.si_code  = FPE_INTOVF;
		siginfo.si_addr  = (caddr_t)rp->r_pc;
		fault = FLTIOVF;
		break;

	case T_FP_DISABLED:
		/* occurs when probing for the fpu and it is not found */
		if (rp->r_psr & PSR_EF) {
			fpu_exists = 0;
			rp->r_psr &= ~PSR_EF;
			rp->r_pc = rp->r_npc;
			rp->r_npc += 4;
			goto cleanup;
		}
		/* this shouldn't happen */
		(void) die(type, rp, addr, mmu_fsr, rw);
		/*NOTREACHED*/

	case T_FP_DISABLED + USER:	/* FPU disabled trap */
		if (tudebug && tudebugfpe)
			showregs(type, rp, addr, 0, S_OTHER);
		fp_disabled(rp);
		goto out;

	case T_FP_EXCEPTION + USER:	/* FPU arithmetic exception */
		if (tudebug && tudebugfpe)
			showregs(type, rp, addr, 0, S_OTHER);
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		oldpc = (greg_t)addr;
		siginfo.si_signo = SIGFPE;
		siginfo.si_code = mmu_fsr;
		siginfo.si_addr = addr;
		fault = FLTFPE;
		break;

	case T_BREAKPOINT + USER:	/* breakpoint trap (t 1) */
		if (tudebug && tudebugbpt)
			showregs(type, rp, (caddr_t)0, 0, S_OTHER);
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_signo = SIGTRAP;
		siginfo.si_code = TRAP_BRKPT;
		siginfo.si_addr = (caddr_t)rp->r_pc;
		fault = FLTBPT;
		break;

	case T_TAG_OVERFLOW + USER:	/* tag overflow (taddcctv, tsubcctv) */
		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0, S_OTHER);
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_signo = SIGEMT;
		siginfo.si_code = EMT_TAGOVF;
		siginfo.si_addr = (caddr_t)rp->r_pc;
		fault = FLTACCESS;
		break;

	case T_FLUSH_PCB + USER:	/* finish user window overflow */
		/*
		 * This trap is entered from sys_rtt in locore.s when, upon
		 * return to user is is found that there are user windows in
		 * pcb_wbuf. This happens because they could not be saved on
		 * the user stack, either because it wasn't resident or because
		 * it was misaligned.
		 */
	    {
		int error;
		caddr_t sp;

		error = flush_user_windows_to_stack(&sp);
		/*
		 * Possible errors:
		 *	error copying out
		 *	unaligned stack pointer
		 * The first is given to us as the return value
		 * from flush_user_windows_to_stack().  The second
		 * results in residual windows in the pcb.
		 */
		if (error != 0) {
			/*
			 * EINTR comes from a signal during copyout;
			 * we should not post another signal.
			 */
			if (error != EINTR) {
				/*
				 * Zap the process with a SIGSEGV - process
				 * may be managing its own stack growth by
				 * taking SIGSEGVs on a different signal stack.
				 */
				bzero((caddr_t)&siginfo, sizeof (siginfo));
				siginfo.si_signo = SIGSEGV;
				siginfo.si_code  = SEGV_MAPERR;
				siginfo.si_addr  = sp;
				fault = FLTBOUNDS;
			}
			break;
		} else if (mpcb->mpcb_wbcnt) {
			bzero((caddr_t)&siginfo, sizeof (siginfo));
			siginfo.si_signo = SIGILL;
			siginfo.si_code  = ILL_BADSTK;
			siginfo.si_addr  = (caddr_t)rp->r_pc;
			fault = FLTILL;
			break;
		}
	    }
		goto out;

	case T_AST + USER:		/* profiling or resched psuedo trap */
		break;

	case T_HOTPATCH_TRAP:
		if (hotpatch_safety_trap_handler == NULL)
			cmn_err(CE_PANIC,
			    "trap: spurious revectoring trap at 0x%p",
			    (void *)rp->r_pc);

		/* Pass it off to the hotpatch facility to handle */
		(*hotpatch_safety_trap_handler)((caddr_t)rp->r_pc);
		return;

	/*
	 * IFLUSH - unimplemented flush trap. This trap will only happen
	 * on modules which allow FLUSH instructions to cause a trap and
	 * only when such a trap is enabled (NOT disabled actually). This
	 * is used, at least, in the Ross 625 hyperSparc in order to
	 * perform a cross-call in order to flush other CPU ICHACHES. The
	 * default module handler returns a '0' which causes a die(),
	 * which is what locore used to do for this trap
	 */
	case T_UNIMP_FLUSH+USER:
	case T_UNIMP_FLUSH:
		switch (unimpflush()) {		/* not handled? */

			default:		/* bad trap if not handled */
			case IFLUSHNIMP:
				(void) die(type, rp, addr, mmu_fsr, rw);
				/*NOTREACHED*/

			case IFLUSHDONEIMP:	/* we handled the instruction */
				rp->r_pc = rp->r_npc;
				rp->r_npc += 4;
				/*FALLTHROUGH*/

			case IFLUSHREDOIMP:	/* need to redo the instr */
				if (type & USER)
					goto out;
				goto cleanup;
				/*NOTREACHED*/
		}
	}

	/*
	 * We can't get here from a system trap
	 */
	ASSERT(type & USER);

	if (fault) {
		/*
		 * Remember the fault and fault address
		 * for real-time (SIGPROF) profiling.
		 */
		lwp->lwp_lastfault = fault;
		lwp->lwp_lastfaddr = siginfo.si_addr;

		/*
		 * If a debugger has declared this fault to be an
		 * event of interest, stop the lwp.  Otherwise just
		 * deliver the associated signal.
		 */
		if (siginfo.si_signo != SIGKILL &&
		    prismember(&p->p_fltmask, fault) &&
		    stop_on_fault(fault, &siginfo) == 0)
			siginfo.si_signo = 0;
	}

	if (siginfo.si_signo)
		trapsig(&siginfo, oldpc == rp->r_pc);

	if (lwp->lwp_oweupc)
		profil_tick(rp->r_pc);

	if (curthread->t_astflag | curthread->t_sig_check) {
		/*
		 * Turn off the AST flag before checking all the conditions that
		 * may have caused an AST.  This flag is on whenever a signal or
		 * unusual condition should be handled after the next trap or
		 * syscall.
		 */
		astoff(curthread);
		curthread->t_sig_check = 0;

		mutex_enter(&p->p_lock);
		if (curthread->t_proc_flag & TP_CHANGEBIND) {
			timer_lwpbind();
			curthread->t_proc_flag &= ~TP_CHANGEBIND;
		}
		mutex_exit(&p->p_lock);

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
		if (ISHOLD(p))
			holdlwp();

		/*
		 * All code that sets signals and makes ISSIG evaluate true must
		 * set t_astflag afterwards.
		 */
		if (ISSIG_PENDING(curthread, lwp, p)) {
			if (issig(FORREAL))
				psig();
			curthread->t_sig_check = 1;
		}

		if (curthread->t_rprof != NULL) {
			realsigprof(0, 0);
			curthread->t_sig_check = 1;
		}
	}

out:	/* We can't get here from a system trap */
	ASSERT(type & USER);

	/*
	 * Restore register window if a debugger modified it.
	 * Set up to perform a single-step if a debugger requested it.
	 */
	if (lwp->lwp_pcb.pcb_xregstat != XREGNONE)
		xregrestore(lwp, 0);

	/*
	 * Set state to LWP_USER here so preempt won't give us a kernel
	 * priority if it occurs after this point.  Call CL_TRAPRET() to
	 * restore the user-level priority.
	 *
	 * It is important that no locks (other than spinlocks) be entered
	 * after this point before returning to user mode (unless lwp_state
	 * is set back to LWP_SYS).
	 */
	lwp->lwp_state = LWP_USER;
	if (curthread->t_trapret) {
		curthread->t_trapret = 0;
		thread_lock(curthread);
		CL_TRAPRET(curthread);
		thread_unlock(curthread);
	}
	if (CPU->cpu_runrun)
		preempt();
	if (lwp->lwp_pcb.pcb_step != STEP_NONE)
		prdostep();
	if (curthread->t_proc_flag & TP_MSACCT)
		(void) new_mstate(curthread, mstate);
	/* Kernel probe */
	TNF_PROBE_1(thread_state, "thread", /* CSTYLED */,
	    tnf_microstate, state, LMS_USER);

	TRACE_0(TR_FAC_TRAP, TR_C_TRAP_HANDLER_EXIT, "C_trap_handler_exit");

	return;

cleanup:	/* system traps end up here */
	ASSERT(!(type & USER));

	TRACE_0(TR_FAC_TRAP, TR_C_TRAP_HANDLER_EXIT, "C_trap_handler_exit");
}

/*
 * Patch non-zero to disable preemption of threads in the kernel.
 */
int IGNORE_KERNEL_PREEMPTION = 0;	/* XXX - delete this someday */

struct kpreempt_cnts {	/* kernel preemption statistics */
	int	kpc_idle;	/* executing idle thread */
	int	kpc_intr;	/* executing interrupt thread */
	int	kpc_clock;	/* executing clock thread */
	int	kpc_blocked;	/* thread has blocked preemption (t_preempt) */
	int	kpc_notonproc;	/* thread is surrendering processor */
	int	kpc_inswtch;	/* thread has ratified scheduling decision */
	int	kpc_prilevel;	/* processor interrupt level is too high */
	int	kpc_apreempt;	/* asynchronous preemption */
	int	kpc_spreempt;	/* synchronous preemption */
}	kpreempt_cnts;

/*
 * kernel preemption: forced rescheduling
 *	preempt the running kernel thread.
 */
void
kpreempt(int asyncspl)
{
	if (IGNORE_KERNEL_PREEMPTION) {
		aston(CPU->cpu_dispthread);
		return;
	}
	/*
	 * Check that conditions are right for kernel preemption
	 */
	do {
		if (curthread->t_preempt) {
			/*
			 * either a privileged thread (idle, panic, interrupt)
			 *	or will check when t_preempt is lowered
			 */
			if (curthread->t_pri < 0)
				kpreempt_cnts.kpc_idle++;
			else if (curthread->t_flag & T_INTR_THREAD) {
				kpreempt_cnts.kpc_intr++;
				if (curthread->t_pil == CLOCK_LEVEL)
					kpreempt_cnts.kpc_clock++;
			} else
				kpreempt_cnts.kpc_blocked++;
			aston(CPU->cpu_dispthread);
			return;
		}
		if (curthread->t_state != TS_ONPROC ||
		    curthread->t_disp_queue != &CPU->cpu_disp) {
			/* this thread will be calling swtch() shortly */
			kpreempt_cnts.kpc_notonproc++;
			if (CPU->cpu_thread != CPU->cpu_dispthread) {
				/* already in swtch(), force another */
				kpreempt_cnts.kpc_inswtch++;
				siron();
			}
			return;
		}

		if (((asyncspl != KPREEMPT_SYNC) ? spltoipl(asyncspl) :
		    getpil()) >= LOCK_LEVEL) {
			/*
			 * We can't preempt this thread if it is at
			 * a PIL > LOCK_LEVEL since it may be holding
			 * a spin lock (like sched_lock).
			 */
			siron();	/* check back later */
			kpreempt_cnts.kpc_prilevel++;
			return;
		}

		/*
		 * Take any pending fpu exceptions now.
		 */
		syncfpu();

		/*
		 * block preemption so we don't have multiple preemptions
		 * pending on the interrupt stack
		 */
		curthread->t_preempt++;
		if (asyncspl != KPREEMPT_SYNC) {
			splx(asyncspl);
			kpreempt_cnts.kpc_apreempt++;
		} else
			kpreempt_cnts.kpc_spreempt++;

		preempt();
		curthread->t_preempt--;
	} while (CPU->cpu_kprunrun);
}

#ifdef TRAPWINDOW
long trap_window[25];
#endif TRAPWINDOW

/*
 * Print out debugging info.
 */
void
showregs(uint_t type, struct regs *rp, caddr_t addr,
	uint_t mmu_fsr, enum seg_rw rw)
{
	int s;
	union ptpe ptpe;
	struct as *as, *uas;
	struct pte *pte;
	int level, ctx;

	s = spl7();
	type &= ~USER;
	printf("%s: ", u.u_comm);
	if (type < TRAP_TYPES)
		printf("%s\n", trap_type[type]);
	else switch (type) {
	case T_UNIMP_FLUSH:		/* IFLUSH */
		printf("unimplemented flush:\n");
		break;
	case T_SYSCALL:
		printf("syscall trap:\n");
		break;
	case T_BREAKPOINT:
		printf("breakpoint trap:\n");
		break;
	case T_DIV0:
		printf("zero divide trap:\n");
		break;
	case T_FLUSH_WINDOWS:
		printf("flush windows trap:\n");
		break;
	case T_SPURIOUS:
		printf("spurious interrupt:\n");
		break;
	case T_AST:
		printf("AST\n");
		break;
	case T_FLUSH_PCB:
		printf("FLUSH_PCB\n");
		break;
	default:
		if (type >= T_SOFTWARE_TRAP && type <= T_ESOFTWARE_TRAP)
			printf("software trap 0x%x\n", type - T_SOFTWARE_TRAP);
		else
			printf("bad trap = %d\n", type);
		break;
	}
	if (type == T_DATA_FAULT || type == T_TEXT_FAULT) {
		extern struct ctx *ctxs;

		uas = curproc->p_as;
		if ((uint_t)addr >= KERNELBASE)
			as = &kas;
		else
			as = curproc->p_as;

		pte = srmmu_ptefind_nolock(as, addr, &level);

		ptpe.ptpe_int = mmu_probe(addr, NULL);
		printf("%s %s fault at addr=0x%p, pme=0x%x\n",
		    (USERMODE(rp->r_psr)? "user": "kernel"),
		    (rw == S_WRITE? "write": "read"),
		    (void *)addr, *(uint_t *)&ptpe);
		mmu_print_sfsr(mmu_fsr);

		printf("pte addr = 0x%p, level = %d\n", (void *)pte, level);
		ctx = mmu_getctx();
		if (ctxs[ctx].c_as != uas) {
			printf("wrong ctx/as ctx %d, as 0x%p\n", ctx,
			    (void *)uas);
		}

		if (astosrmmu(uas)->s_ctx != ctx) {
			printf("wrong ctx/as 2 ctx %d, as 0x%x\n",
			    ctx, astosrmmu(uas)->s_ctx);
		}
	} else if (addr) {
		printf("addr=0x%p\n", (void *)addr);
	}
	printf("pid=%d, pc=0x%lx, sp=0x%lx, psr=0x%lx, context=%d\n",
	    (ttoproc(curthread) && ttoproc(curthread)->p_pidp) ?
	    (ttoproc(curthread)->p_pid) : 0, rp->r_pc, rp->r_sp,
	    rp->r_psr, mmu_getctx());
	if (USERMODE(rp->r_psr)) {
		printf("o0-o7: %lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx\n",
		    rp->r_o0, rp->r_o1, rp->r_o2, rp->r_o3,
		    rp->r_o4, rp->r_o5, rp->r_o6, rp->r_o7);
	}
	printf("g1-g7: %lx, %lx, %lx, %lx, %lx, %lx, %lx\n",
	    rp->r_g1, rp->r_g2, rp->r_g3,
	    rp->r_g4, rp->r_g5, rp->r_g6, rp->r_g7);
#ifdef TRAPWINDOW
	printf("trap_window: wim=%x\n", trap_window[24]);
	printf("o0-o7: %x, %x, %x, %x, %x, %x, %x, %x\n",
	    trap_window[0], trap_window[1], trap_window[2], trap_window[3],
	    trap_window[4], trap_window[5], trap_window[6], trap_window[7]);
	printf("l0-l7: %x, %x, %x, %x, %x, %x, %x, %x\n",
	    trap_window[8], trap_window[9], trap_window[10], trap_window[11],
	    trap_window[12], trap_window[13], trap_window[14], trap_window[15]);
	printf("i0-i7: %x, %x, %x, %x, %x, %x, %x, %x\n",
	    trap_window[16], trap_window[17], trap_window[18], trap_window[19],
	    trap_window[20], trap_window[21], trap_window[22], trap_window[23]);
#endif TRAPWINDOW
	if (vac) {
		XCALL_PROLOG
		vac_flush(panicbuf, PANICBUFSIZE);
		XCALL_EPILOG
	}
#ifndef SAS
	if (!panicstr && (tudebug > 1) && (boothowto & RB_DEBUG))
		debug_enter(NULL);
#endif SAS
	splx(s);
}

void
panic_showtrap(struct trap_info *tip)
{
	showregs(tip->trap_type, tip->trap_regs, tip->trap_addr,
	    tip->trap_mmu_fsr, tip->trap_rw);
}

void
panic_savetrap(panic_data_t *pdp, struct trap_info *tip)
{
	panic_nv_t *pnv;

	panic_saveregs(pdp, tip->trap_regs);
	pnv = PANICNVGET(pdp);

	PANICNVADD(pnv, "sfsr", (uint32_t)tip->trap_mmu_fsr);
	PANICNVADD(pnv, "sfar", (uint32_t)tip->trap_addr);

	PANICNVSET(pdp, pnv);
}
