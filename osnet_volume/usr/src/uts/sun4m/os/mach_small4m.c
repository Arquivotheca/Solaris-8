/*
 * Copyright (c) 1989-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mach_small4m.c	1.22	98/07/21 SMI"

#include <sys/machparam.h>
#include <sys/module.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/param.h>
#include <sys/memerr.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/pte.h>
#include <vm/page.h>
#include <vm/hat.h>
#include <vm/hat_srmmu.h>
#include <vm/seg.h>
#include <sys/vnode.h>
#include <sys/cmn_err.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/async.h>
#include <sys/intreg.h>
#include <sys/bt.h>
#include <sys/physaddr.h>
#include <sys/privregs.h>

void handle_aflt(u_int cpu_id, u_int type, u_int afsr, u_int afar0,
    u_int afar1);

int small_sun4m_parerr_reset(u_int addr);
int small_sun4m_parerr_recover(caddr_t addr, int perm);
void small_sun4m_log_sbus_err(u_int afsr, u_int afar);

extern	struct	async_flt a_flts[NCPU][MAX_AFLTS];
extern	u_int	a_head[NCPU];
extern	u_int	a_tail[NCPU];

extern	int	pokefault;

extern int tsunami;
#define	TSUNAMI_CONTROL_READ_BUG
#ifdef  TSUNAMI_CONTROL_READ_BUG
extern int tsunami_control_read_bug;
void tsu_cs_read_enter(void);
void tsu_cs_read_exit(void);
#pragma weak tsu_cs_read_enter
#pragma weak tsu_cs_read_exit
#endif

#define	PCR_PC		0x00020000
#define	TSUNAMI_PCR_PE	0x00001000
#define	SWIFT_PCR_PE	0x00040000

int small_sun4m_parity_enable = 1;

void
small_sun4m_memerr_init()
{
	u_int pcr;

	if (small_sun4m_parity_enable) {
		pcr = mmu_getcr();
		pcr |= (tsunami != 0) ? TSUNAMI_PCR_PE : SWIFT_PCR_PE;
		mmu_setcr(pcr);
	}
}

void
small_sun4m_memerr_disable()
{
	u_int pcr;

	if (small_sun4m_parity_enable) {
		pcr = mmu_getcr();
		pcr &= (tsunami != 0) ? ~TSUNAMI_PCR_PE : ~SWIFT_PCR_PE;
		mmu_setcr(pcr);
	}
}


#define	MMU_SFSR_PERR	0x00006000	/* Tsunami: Parity Error */


/*ARGSUSED2*/
small_sun4m_ebe_handler(reg, addr, type, rp)
	u_int reg;
	caddr_t addr;
	unsigned type;
	struct regs *rp;
{
	u_int ctx = mmu_getctx();
	u_int result = 0;
	u_int paddr, permanent;
	proc_t *p = ttoproc(curthread);

	paddr = va_to_pa(addr);

	if ((reg & MMU_SFSR_PERR) == 0) {

		/*
		 * If it's the result of a user process getting a timeout
		 * or bus error on the sbus, just return and let the process
		 * be sent a bus error signal.
		 */
		if (reg & (MMU_SFSR_TO | MMU_SFSR_BE))
			return (0);

		cmn_err(CE_PANIC,
			"Non-parity synchronous error: ctx=%x "
			"va=%p pa=%x", ctx, (void *)addr, paddr);

	} else if (X_FAULT_TYPE(reg) == FT_TRANS_ERROR) {

		/*
		 * Parity errors during table walks are unrecoverable.
		 */
		cmn_err(CE_PANIC, "Parity error during table walk.");

	} else if ((u_int) addr >= KERNELBASE) {

		/*
		 * Parity error in kernel address space.
		 */
		cmn_err(CE_PANIC,
			"Parity error in kernel space: ctx=%x, va=%p pa=%x",
			ctx, (void *)addr, paddr);
	}

	/*
	 * Here on parity errors in user space.  Determine if the
	 * the error is permanent and return non-zero if the process
	 * needs to be killed.
	 */
	cmn_err(CE_WARN,
	    "Synchronous parity error: pid=%d ctx=%x va=%p pa=%x",
	    p->p_pid, ctx, (void *)addr, paddr);
	cmn_err(CE_WARN, "Attempting recovery...");

	permanent = (small_sun4m_parerr_reset(paddr) == 0);
	cmn_err(CE_WARN, "Parity error at %x is %s.",
		paddr, permanent ? "permanent" : "transient");

	result = small_sun4m_parerr_recover(addr, permanent);
	cmn_err(CE_WARN, "System operation can continue.");

	return (result);
}


/*
 * Patterns to use to determine if a location has a hard or soft
 * parity error . The zero is also an end-of-list marker, as well
 *  as a pattern.
 */
static u_int parerr_patterns[] = {
	(u_int) 0xAAAAAAAA,	/* Alternating ones and zeroes */
	(u_int) 0x55555555,	/* Alternate the other way */
	(u_int) 0x01010101,	/* Walking ones ... */
	(u_int) 0x02020202,	/* ... four bytes at once ... */
	(u_int) 0x04040404,	/* ... from right to left */
	(u_int) 0x08080808,
	(u_int) 0x10101010,
	(u_int) 0x20202020,
	(u_int) 0x40404040,
	(u_int) 0x80808080,
	(u_int) 0x7f7f7f7f,	/* And now walking zeros, from left to right */
	(u_int) 0xbfbfbfbf,
	(u_int) 0xdfdfdfdf,
	(u_int) 0xefefefef,
	(u_int) 0xf7f7f7f7,
	(u_int) 0xfbfbfbfb,
	(u_int) 0xfdfdfdfd,
	(u_int) 0xfefefefe,
	(u_int) 0xffffffff,	/* All ones */
	(u_int) 0x00000000,	/* All zeroes -- must be last! */
};

/*
 * Reset a parity error so that we can continue operation.
 * Also, see if we get another parity error at the same location.
 * Return 0 if error reset, -1 if not.
 * %%% We need to test all the words in a cache line, if cache is on.
 */

int always_perm = 0;		/* for testing only */

int
small_sun4m_parerr_reset(u_int addr)
{
	int	retval = 1;
	u_int	*p, i, s;

	/*
	 * Test the word with successive patterns, to see if the parity
	 * error is a permanent problem.
	 */
	s = splhigh();
	small_sun4m_memerr_disable();
	for (p = parerr_patterns; *p; p++) {

		stphys(addr, *p);	/* store the pattern */
		i = ldphys(addr);

		if (i != *p || always_perm) {
			cmn_err(CE_WARN,
				"Parity error at %x with pattern %x", addr,
*p);
			retval = 0;
		}
	}
	small_sun4m_memerr_init();
	splx(s);
	return (retval);
}

/*
 * Recover from a parity error.  Returns 0 if successful, -1 if not.
 */

int
small_sun4m_parerr_recover(caddr_t addr, int perm)
{
	struct  page *pp;
	u_int   pagenum = va_to_pfn(addr);
	u_int	fatal = 0;
	u_int	result = 1;

	/*
	 * None of the following checks are expected to succeed
	 * since we should only get here as the result of a
	 * parity error on a user page.
	 */
	pp = page_numtopp(pagenum, SE_EXCL);
	if (pp == (page_t *)NULL) {
		cmn_err(CE_WARN, "parity error recovery: no page structure");
		fatal = 1;
	} else if (pp->p_vnode == 0) {
		cmn_err(CE_WARN, "parity error recovery: no vnode");
		fatal = 1;
	}
	if (fatal)
		cmn_err(CE_PANIC, "unrecoverable parity error");

	/*
	 * If the page has been modified, we can't recover.
	 */
	(void) hat_pagesync(pp, HAT_SYNC_ZERORM);
	if (PP_ISMOD(pp)) {
		cmn_err(CE_WARN,
			"Page %x was modified, error is unrecoverable.",
			pagenum);
		result = 0;
	}

	/*
	 * Destroy all the mappings to the page.  If the error is
	 * permanent, don't put it back on the free list.
	 */
	if (perm) {
		cmn_err(CE_WARN, "Page %x marked out of service.", pagenum);
		/*LINTED*/
		VN_DISPOSE(pp, B_INVAL, -1, kcred);
	} else {
		cmn_err(CE_WARN, "Page %x put back on free list.", pagenum);
		/*LINTED*/
		VN_DISPOSE(pp, B_INVAL, 0, kcred);
	}
	return (result);
}

#define	SYS_FATAL_FLT \
	cmn_err(small_sun4m_l15ints_fatal ? CE_PANIC : CE_WARN, \
	"asynchronous fault: AFSR=%x AFAR=%x", \
	afsr, afar);

int small_sun4m_l15ints_fatal = 1;	/* should only be zero for debugging */

void
small_sun4m_l15_async_fault()
{
	u_int sipr;	/* System Interrupt Pending Register */
	u_int afsr;	/* Asynchronous Fault Status Register */
	u_int afar;	/* Asynchronous Fault Address Reg. */
	u_int mfsr;	/* Memory Fault Status Reg. */
	u_int mfar;	/* Memory Fault Address Reg. */
	int cpuid = 0;

	/*
	 * Read the System Interrupt Pending Register.
	 */
	sipr = *(u_int *)v_sipr_addr;

	/*
	 * If NONE of the interesting bits is set, then panic.
	 */
	if ((sipr & SIR_MODERROR) == 0)
		cmn_err(CE_WARN, "unknown level-15 interrupt");

	/*
	 * Handle Parity asynchronous faults first, because there is
	 * a minute possibility that this handler could cause
	 * further Parity faults (when storing non-fatal fault info
	 * in memory) before the first parity fault is handled.
	 */
#define	TSU_MFSR_VADDR	((volatile u_int *)(v_sbusctl_addr + 0x20))
#define	TSU_MFAR_VADDR	((volatile u_int *)(v_sbusctl_addr + 0x24))
#define	MFSR_VADDR	((volatile u_int *)(v_sbusctl_addr + 0x50))
#define	MFAR_VADDR	((volatile u_int *)(v_sbusctl_addr + 0x54))
#define	AFSR_VADDR	((volatile u_int *)MTS_AFSR_VADDR)
#define	AFAR_VADDR	((volatile u_int *)MTS_AFAR_VADDR)
#define	MFSR_ERR	0x80000000

	if (tsunami) {
#ifdef	TSUNAMI_CONTROL_READ_BUG
		/*
		 * 2.2 tsunami's have a bug that can cause reads to the
		 * SBus and IOMMU control space during DVMA to hang the
		 * processor.  As a workaround, we must disable DVMA
		 * before reads to this space and enable DVMA after.
		 *
		 * WARNING: tsu_cs_read_enter() disables traps!
		 */
		if (tsunami_control_read_bug)
			tsu_cs_read_enter();
#endif
		mfar = *TSU_MFAR_VADDR;
		mfsr = *TSU_MFSR_VADDR;
		afar = *AFAR_VADDR;	/* inside tsunami_control_read_bug */
		afsr = *AFSR_VADDR;	/* inside tsunami_control_read_bug */
#ifdef	TSUNAMI_CONTROL_READ_BUG
		if (tsunami_control_read_bug)
			tsu_cs_read_exit();
#endif
	} else {
		mfar = *MFAR_VADDR;
		mfsr = *MFSR_VADDR;
		afar = *AFAR_VADDR;
		afsr = *AFSR_VADDR;
	}

	if (mfsr & MFSR_ERR)
		cmn_err(small_sun4m_l15ints_fatal ? CE_PANIC : CE_WARN,
			"asynchronous memory fault: MFSR=%x MFAR=%x",
			mfsr, mfar);

	if (afsr & MTSAFSR_ERR) {
		if (pokefault == -1) {
			/*EMPTY*/;
		} else if (afsr & MTSAFSR_S) {
			if (afsr & (MTSAFSR_TO | MTSAFSR_BERR)) {
				handle_aflt(cpuid, AFLT_M_TO_S,
					afsr, afar, 0);
			} else {
				SYS_FATAL_FLT
			}
#ifdef BUG_1223163_NOTNEEDED_IN_USER_MODE
		/*
		 * For user-mode Multiple cpu-to-sbus errors
		 * just send signal to user process is good enough
		 * However, for kernel-mode driver access, ME should
		 * still panic from above check. MTSAFSR_S denotes
		 * supervisor mode access.
		 */
		} else if (afsr & MTSAFSR_ME) {
			SYS_FATAL_FLT
#endif /* BUG_1223163_NOTNEEDED_IN_USER_MODE */
		} else {
			handle_aflt(cpuid, AFLT_M_TO_S, afsr, afar, 0);
		}
	}
}

/*
 * This routine is called by autovectoring a soft level 12
 * interrupt, which was sent by handle_aflt to process a
 * non-fatal asynchronous fault.
 */
small_sun4m_process_aflt()
{
	u_int tail;
	register proc_t *pp = ttoproc(curthread);
	u_int afsr;
	u_int afar;
	u_int cpuid = curthread->t_cpu->cpu_id;

	/*
	 * While there are asynchronous faults which have
	 * accumulated, process them.
	 * There shouldn't be a problem with the race condition
	 * where this loop has just encountered the exit
	 * condition where a_head == a_tail, but before process_aflt
	 * returns it is interrupted by a level 15 interrupt
	 * which stores another asynchronous fault to process.
	 * In this (rare) case, we will just take another level 13
	 * interrupt after returning from the current one.
	 */
	while (a_head[cpuid] != a_tail[cpuid]) {
		/*
		 * Point to the next asynchronous fault to process.
		 */
		tail = ++(a_tail[cpuid]) % MAX_AFLTS;
		a_tail[cpuid] = tail;

		afsr = a_flts[cpuid][tail].aflt_stat;
		afar = a_flts[cpuid][tail].aflt_addr0;

		/*
		 * Process according to the type of write buffer
		 * fault.
		 */
		switch (a_flts[cpuid][tail].aflt_type) {

		case	AFLT_M_TO_S:
			cmn_err(CE_WARN, "SBus Asynchronous fault due to ");
			cmn_err(CE_CONT, "user write - non-fatal\n");
			small_sun4m_log_sbus_err(afsr, afar);
			/*
			 * We want to kill off the process that
			 * performed the write that caused this
			 * asynchronous fault.  Since all asynchronous
			 * faults are handled here before we switch
			 * processes, we know which process is
			 * responsible.  The case where the cache
			 * flush code causes write buffers to fill
			 * (and async. fault later occurrs in the
			 * orig. context) is not a problem, because
			 * only the AFLT_MEM_IF case is affected
			 * since only main memory is cached in sun4m
			 */
			psignal(pp, SIGBUS);
			break;

		default:
			/*
			 * Since these are supposed to be non-fatal
			 * asynchronous faults, just ignore any
			 * bogus ones.
			 */
			break;
		}
		/*
		 * Nitty-Gritty details can be obtained from the
		 * asynchronous faults registers, and may be useful
		 * by an "expert."
		 */
		cmn_err(CE_WARN,
			"AFSR = 0x%x, AFAR = 0x%x\n", afsr, afar);
	}
	return (1);
}

void
small_sun4m_log_sbus_err(
	u_int afsr,	/* Asynchronous Fault Status Register */
	u_int afar)	/* Asynchronous Fault Address Register */
{
	extern	char	*nameof_ssiz[];

	if (afsr & MTSAFSR_ME)
		cmn_err(CE_WARN, "\tMultiple Errors\n");
	if (afsr & MTSAFSR_S)
		cmn_err(CE_WARN, "\tError during Supv mode cycle\n");
	else
		cmn_err(CE_WARN, "\tError during User mode cycle\n");
	if (afsr & MTSAFSR_LE)
		cmn_err(CE_WARN, "\tLate Error\n");
	if (afsr & MTSAFSR_TO)
		cmn_err(CE_WARN, "\tTimeout Error\n");
	if (afsr & MTSAFSR_BERR)
		cmn_err(CE_WARN, "\tBus Error\n");
	cmn_err(CE_WARN, "\tRequested transaction: %s at %x\n",
	    nameof_ssiz[(afsr & MTSAFSR_SIZ) >> MTSAFSR_SIZ_SHFT], afar);
}

/* XXX - this seems "module" dependent for small4m */

int
small_sun4m_impl_bustype(u_int pfn)
{
	int bt = BT_UNKNOWN;

	switch (pfn >> 16) {

		case 0:
			bt = BT_DRAM;
			break;

		case 1:	/* "sort of" obio */
		case 7: /* really SBus slot 4 .. */
			bt = BT_OBIO;
			break;

		case 2:
			if (tsunami)
				break;
			/*FALLTHROUGH*/
		case 3:
		case 4:
		case 5:
		case 6:
			bt = BT_SBUS;
			break;

		default:
			break;
	}
	return (bt);
}
