/*
 * Copyright (c) 1989-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mach_4m.c	1.55	99/04/13 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/archsystm.h>
#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/module.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/privregs.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/memerr.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/pte.h>
#include <vm/page.h>
#include <vm/hat.h>
#include <vm/mhat.h>
#include <vm/seg.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/physaddr.h>
#include <sys/cmn_err.h>
#include <sys/async.h>
#include <sys/intreg.h>
#include <sys/bt.h>
#include <sys/aflt.h>
#include <sys/memctl.h>
#include <sys/ddi.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>

int	report_ce_console = 0;	/* don't print messages on console */

/*
 * when ce_int_ok is zero, interrupts for correctable errors are prevented
 */
int ce_int_ok = 1;

/*
 * when excessive correctable memory errors occur, ce interrupts are
 * disabled for a period of INT_OFF_TIME;  1800 secs = 30 minutes.
 */
#define	INT_OFF_TIME	1800
int ceint_off_time = INT_OFF_TIME;

int async_errq_full = 0;

extern void	mmu_print_sfsr();
extern void	sbusctl_getregs(u_int *afsr, u_int *afar0);
extern int	mmu_chk_wdreset(void);

void handle_aflt(u_int cpu_id, u_int type, u_int afsr, u_int afar0,
    u_int afar1);
void page_giveup(u_int pagenum, struct page *pp);
void enable_cei(void *);
void log_mem_err(u_int afsr, u_int afar0, u_int afar1);
void clr_dirint(int cpuix, int int_level);
void sun4m_l15_async_fault(void);
void log_mtos_err(u_int afsr, u_int afar0);
void log_ce_mem_err(u_int afsr, u_int afar0, u_int afar1,
	char *(*get_unum)());
void log_ue_mem_err(u_int afar0, u_int afar1, char *(*get_unum)());
void log_async_err(u_int afsr, u_int afar);

extern enum mc_type mc_type;
extern int viking;
extern int mxcc;

extern volatile u_int system_fatal;
extern struct async_flt sys_fatal_flt[NCPU];
extern struct async_flt a_flts[NCPU][MAX_AFLTS];
extern u_int a_head[NCPU];
extern u_int a_tail[NCPU];

extern int pokefault;

extern volatile u_int aflt_sync[NCPU];
extern int procset;

extern int ross_hw_workaround2;
extern int ross_hd_bug;

extern u_int sx_ctlr_present;
void *sx_aflt_fun;

#define	EFER_EE	0x1	/* Enables ECC Checking */
#define	EFER_EI	0x2	/* Enables Interrupt on CEs */
#define	EFAR1_PGNUM 0xFFFFF000	/* masked out 12 bit offset */
#define	EFDR_GENERATE_MODE	0x400
#define	EFAR0_PA	0x0000000F	/* PA<35:32>: physical address */
#define	EFAR0_MID_SHFT	(28)
#define	EFSR_ME		0x00010000	/* multiple errors */
#define	EFAR0_S		0x08000000	/* supervisor access */

#define	SX_FATAL_MSG1	"Async Fault due to SX accesses to D[V]RAM"
#define	SX_FATAL_MSG2	\
	"Async Fault due to illegal SX instructions from Supervisory Space"

/*
 * Memory error handling for sun4m
 */

static void
fix_ce_on_read(efsr, efar0, efar1)
	u_int	efsr,	efar0,	efar1;
{
	u_int	aligned_addr, i, n;
	u_int	size;

#ifdef lint
	efsr = efsr;
#endif
	/*
	 * currently, bit <35:32> are 0.  so we don't really need this one
	 * asi = efar0 & EFAR0_PA;
	 */

	size = (efar0 & EFAR0_SIZ) >> 8;
	switch (size) {
		case 4:
			/* 16 byte alighed */
			aligned_addr = efar1 & 0xFFFFFFF0;
			n = 2;
			break;
		case 5:
			/* 32 byte alighed */
			aligned_addr = efar1 & 0xFFFFFFE0;
			n = 4;
			break;
		default:
			/* 8 byte alighed */
			aligned_addr = efar1 & 0xFFFFFFF8;
			n = 1;
			break;
	}

	/*
	 * scrub n * 8 bytes (load double/store double)
	 *
	 * when trying to scrub 32 bytes from efar1, it's possible to have
	 * other fatal errors (UE, SE, TO) occur.  We don't do anything
	 * here to handle these fatal errors because another l15 async.
	 * fault will take place.
	 * It is also possible that another CE will occur (the second CE
	 * which causes ME bit set).  In this case, we have to reset the
	 * registers and ignore CE bit.
	 *
	 * Turn off the EFER_EE bit so that we don't take an uncorrectable
	 * error while scrubbing. This can't be handled properly because
	 * the fault address reported is a physical address, but the trap
	 * handler thinks it's virtual.
	 */
	memctl_set_enable(0, EFER_EE);
	for (i = 0; i < n; i++) {
		scrubphys(aligned_addr+i*8);
		/* check efsr */
		if ((*(u_int *)EFSR_VADDR) & EFSR_CE) {
			/* reset */
			*(u_int *)EFSR_VADDR = 0;
		}
	}
	memctl_set_enable(EFER_EE, 0);

}

int
fix_nc_ecc(efsr, efar0, efar1)
	u_int	efsr,	efar0,	efar1;
{
	/* Fix a non-correctable ECC error by "retiring" the page */

	u_int	pagenum;
	struct page    *pp;

	pagenum = (((efar0 & EFAR0_PA) << EFAR0_MID_SHFT) |
				((efar1 & EFAR1_PGNUM) >> 4));

	if (efsr & EFSR_ME) {
		cmn_err(CE_WARN, "Multiple ECC Errors\n");
		return (-1);
	}
	if (efar0 & EFAR0_S) {
		cmn_err(CE_WARN, "ECC error recovery: Supervisor mode\n");
		return (-1);
	}
	pp = page_numtopp(pagenum, SE_EXCL);
	if (pp == (page_t *)NULL) {
		cmn_err(CE_WARN, "Ecc error recovery: no page structure\n");
		return (-1);
	}
	if (pp->p_vnode == 0) {
		cmn_err(CE_WARN, "ECC error recovery: no vnode\n");
		return (-1);
	}

	(void) hat_pagesync(pp, HAT_SYNC_ZERORM);

	if (PP_ISMOD(pp)) {
		if (hat_kill_procs(pp, (caddr_t)efar0) != 0) {
			return (-1);
		}
	}

	page_giveup(pagenum, pp);
	return (0);
}

void
page_giveup(u_int pagenum, struct page *pp)
{
	(void) hat_pageunload(pp, HAT_FORCE_PGUNLOAD);
	/*LINTED*/
	VN_DISPOSE(pp, B_INVAL, -1, kcred);
	cmn_err(CE_WARN, "page %lx marked out of service.\n", ptob(pagenum));
}

/*ARGSUSED2*/
sun4m_handle_ebe(mmu_fsr, addr, type, rp, rw)
	unsigned type;
	caddr_t addr;
	u_int mmu_fsr;
	struct regs *rp;
	enum seg_rw rw;
{
	extern int pac_parity_chk_dis();
	extern struct memslot memslots[];

	if (mmu_fsr & MMU_SFSR_FATAL) {
		union ptpe ptpe;
		int bt;
		int busbits;
		/*
		 * If this trap was caused by a MBUS Uncorrectable
		 * error, it's an ECC uncorrectable error on read.
		 * This requires to print out the detailed info (
		 * including the SIMM number) before calling panic
		 */
		if (mmu_fsr & (MMU_SFSR_UC | MMU_SFSR_UD)) {
			u_int paddr;
			int parity_err;
			int cpuid = 0;
			int s;

			s = spl7();
			if (cache & CACHE_PAC)
				parity_err = pac_parity_chk_dis(mmu_fsr, 0);
			if (parity_err)
				printf("module parity error:\n");
			ptpe.ptpe_int = mmu_probe(addr, NULL);
			bt = impl_bustype(ptpe.pte.PhysicalPageNumber);
			if (pte_valid(&ptpe.pte)) {
				paddr = (ptpe.pte.PhysicalPageNumber <<
					MMU_PAGESHIFT)
					+ ((int)addr & MMU_PAGEOFFSET);
				busbits = (ptpe.pte.PhysicalPageNumber >>
					(32 - MMU_PAGESHIFT));
				/*
				 * uncorrectable ecc error on read
				 * Check to see if it is on a reference to
				 * memory which a driver wants to handle.
				 * If so, post it as an async error.
				 * It is up to the handler to cope with the
				 * bogus data that will have been read.
				 */
				if (bt == BT_NVRAM &&
				    memslots[PMEM_SLOT(paddr)].ms_func) {
					cpuid = getprocessorid();
					handle_aflt(cpuid, AFLT_MEM_IF,
					    EFSR_UE, 0, paddr);
					rp->r_pc = rp->r_npc;
					rp->r_npc = rp->r_npc + 4;
					splx(s);
					return (1);
				} else {
					printf("fatal system fault:\n");
					log_mem_err(EFSR_UE, busbits, paddr);
				}
			} else {
				printf("fatal system fault:\n");
				printf("addr %p is not valid\n", (void *)addr);
			}
			printf("Control Registers:\n");
			printf("\tsfsr = 0x%x, %s fault ", mmu_fsr,
				(rw == S_WRITE? "write": "read"));
			printf("at vaddr 0x%p\n", (void *)addr);
			panic("memory error");
			/* NOTREACHED */
		}
	}
	return (0);
}

/*
 * FIXME -- this asynchronous fault handling code will have
 * to be modified wherever necessary, so that it runs for 5.0MT.
 * It was originally implemented for 4.1PSMP.
 */
/*
 * a macro used by async() to
 * atomically set system_fatal and
 * log info about the fault,
 * to be printed later when we panic.
 */
#define	SYS_FATAL_FLT(TYPE) \
	if (!atomic_tas((int *)&system_fatal)) { \
		sys_fatal_flt[cpuid].aflt_type = TYPE; \
		sys_fatal_flt[cpuid].aflt_subtype = subtype; \
		sys_fatal_flt[cpuid].aflt_stat = afsr; \
		sys_fatal_flt[cpuid].aflt_addr0 = afar0; \
		sys_fatal_flt[cpuid].aflt_addr1 = afar1; \
	}

u_int	report_ce_log	= 0;
u_int	log_ce_error	= 0;
u_int	module_error	= 0;
volatile u_int	ecc_error	= 0;

extern	void	mmu_log_module_err();

/*
 * FIXME -- any locking mods needed for asynchronous fault handling???
 *  At least, I can think of one right now.  We need to grab all locks
 * before panicing.
 */
/*
 * Asynchronous fault handler.
 * This routine is called to handle a hard level 15 interrupt,
 * which is broadcasted to all CPUs.
 * All fatal failures are completely handled by this routine
 * (by panicing).  Since handling non-fatal failures would access
 * data structures which are not consistent at the time of this
 * interrupt, these non-fatal failures are handled later in a
 * soft interrupt at a lower level.
 */

void
sun4m_l15_async_fault(void)
{
	u_int sipr;		/* System Interrupt Pending Register */
	u_int afsr;		/* an Asynchronous Fault Status Register */
	u_int afar0;		/* first Asynchronous Fault Address Reg. */
				/* second Asynchronous Fault Address Reg. */
	u_int afar1 = 0;
	u_int aerr0 = 0;	/* MXCC error register <63:32> */
	u_int aerr1 = 0;	/* MXCC error register <31:0>  */
	int cpuid = 0;
	u_int subtype = 0;	/* module error fault subtype */
	u_int aflt_regs[4];
	int parity_err;
	int slot;
	struct memslot *mp;
	extern int nvsimm_present;
	extern struct memslot memslots[];
	int nvslot(u_int, u_int);
	u_int	reg;

	/*
	 * Get the CPU ID.  Note that we can't simply pull this
	 * out of the PERCPU area because, at the time of this
	 * interrupt the wrong PERCPU area may be mapped in, due
	 * to the fact that the system cache consistency code
	 * will change the context temporarly without also updating
	 * the PERCPU mapping.
	 */
	cpuid = getprocessorid();

	/*
	 * Here is where all CPUs synchronize.
	 * The synchronization master is CPU 0.  CPU 0 waits for
	 * each CPU that responded to reset its entry in
	 * aflt_sync.
	 */
	if (cpuid == 0) {
		register int ps = procset;
		register int i;
		for (i = 1; i < NCPU; i++)
			if (ps & (1<<i))
				while (aflt_sync[i] == 0)
					;
		for (i = 1; i < NCPU; i++) {
			if (ps & (1<<i))
				aflt_sync[i] = 0;
			flush_writebuffers();
		}
	} else {
		aflt_sync[cpuid] = 1;
		flush_writebuffers();
		while (aflt_sync[cpuid])
			;
	}

	/* read the System Interrupt Pending Register */
	sipr = *(u_int *)v_sipr_addr;

	/*
	 * If NONE of the interesting bits is set, then someone else
	 * took a watchdog and managed to clear his error. Dive back
	 * into the boot prom, permanently.
	 */
	if ((sipr & SIR_ASYNCFLT) == 0) {
		if (cpuid == 0)
			printf("Level 15 Error: Watchdog Reset\n");
		(void) prom_stopcpu((dnode_t)0);
	}

	/*
	 * Let cpu0 takes care of the fault
	 */
	if (cpuid == 0) {

	/*
	 * Handle ECC asynchronous faults first, because there is
	 * a minute possibility that this handler could cause
	 * further ECC faults (when storing non-fatal fault info
	 * in memory) before the first ECC fault is handled.
	 */

	/*
	 * Heisenberg's uncertainty principle as applied to
	 * mem I/F asynchronous faults:  Since the vac flush
	 * code that maintains consistency across various CPUs
	 * must be fast, it does not flush write buffers whenever
	 * it changes contexts temporarly.  A side effect is that
	 * it is not possible to tell which context is responsible
	 * for mem I/F faults.  Thus, if we ever wanted to do
	 * something other than panicing for timeout or bus errors,
	 * (for user accesses), we would still be forced to panic
	 * when the m-bus address does not have a page struct
	 * associated with it.
	 */

	/*
	 * Errors from the SX memory controller are reported asynchronously
	 * at level 15. When the error is due to a user process violating
	 * SX programming rules (ex writing an illegal SX opcode into the
	 * SX Instrucion Queue ) the error is reported as an ECC error in
	 * system interrupt pending register (SIPR) and the contents of
	 * the memory controller fault status register is 0. This type of
	 * error can be classified as an error from SX core. In the case of
	 * a memory parity error that occurs during a SX access to memory
	 * the error is also reported as an ECC memory error in the SIPR and in
	 * addition the memory controller fault status register will have the
	 * GE (Graphics Error) and UE (Uncorrectable error) or CE (Correctable
	 * Error) bits set.
	 * Since only user processes can cause interrupts from SX core
	 * they are terminated upon receipt of such errors. In the case of
	 * correctable/uncorrectble  memory errors during SX accesses to
	 * memory the normal course of action is taken with the warning
	 * messages displaying additional information that the error condition
	 * was created during SX accesses to memory.
	 */
		if (sipr & SIR_ECCERROR) {
			ecc_error = 1;
			/* read the ECC Memory Fault Registers */
			memctl_getregs(&afsr, &afar0, &afar1);

			/*
			 * to prevent another ECC/CE error occurs during the
			 * handler, disable EI before unload efsr, efar
			 * registers
			 * turn on EI bit before leaving this handler.
			 */
			memctl_set_enable(0, EFER_EI);

			/* unlock these registers */
			*(u_int *)EFSR_VADDR = 0;

			/* stall until write buffer not busy */
			memctl_getregs(&reg, &reg, &reg);

			/*
			 * Uncorrectable errors,
			 * and timeout errors are fatal to the system.
			 * One could argue that uncorrectable errors
			 * could simply be handled by killing off
			 * all processes which have mappings to the
			 * page affected, but this doesn't make
			 * much sence for ECC.  Besides, that
			 * approach might cause flakey behaviour
			 * when certain essential system daemons
			 * are killed off.
			 * Correctable errors are non-fatal. however
			 * we should handle CE on read case (scrub the
			 * location) here before invoking level 12
			 * to process the fault
			 */
/*
 * FIXME: curthread->t_nofault is per processor structure
 *	  this doesn't work properly in MP
 */
			if (curthread->t_nofault) {
				/*EMPTY*/;
			} else if ((mc_type == MC_MMC) && (afsr & EFSR_TO)) {
				SYS_FATAL_FLT(AFLT_MEM_IF)
			} else if (((mc_type == MC_EMC) ||
			    (mc_type == MC_SMC)) && (afsr & EFSR_SE)) {
				if (nvsimm_present &&
				    (slot = nvslot(afar0, afar1)) != -1) {
					mp = &memslots[slot];
					/*
					 * Ignore battery low access
					 */
					if ((u_int) mp->ms_fault_specific !=
					    afar1) {
						SYS_FATAL_FLT(AFLT_MEM_IF)
					}
				} else if (afsr & EFSR_GE) { /* SX errors */
					cmn_err(CE_WARN,
					    "Misreferenced Slot Error.\n");
					handle_aflt(cpuid, AFLT_SX_CORE, afsr,
					    afar0, afar1);
				} else {
					SYS_FATAL_FLT(AFLT_MEM_IF)
				}
			} else if (afsr & EFSR_UE) {
				handle_aflt(cpuid, AFLT_MEM_IF, afsr,
				    afar0, afar1);
				if (async_errq_full) {
					cmn_err(CE_CONT, "AFSR = 0x%x, "
					    "AFAR0 = 0x%x, AFAR1 = 0x%x\n",
						afsr, afar0, afar1);
					cmn_err(CE_PANIC, "Async Memory Fault");
				}
			} else if (afsr & EFSR_CE) {
				if (((afar0 & EFAR0_TYPE) >> 4) & 1)
					fix_ce_on_read(afsr, afar0, afar1);
				handle_aflt(cpuid, AFLT_MEM_IF, afsr,
				    afar0, afar1);
			} else if (sx_ctlr_present) {
				/*
				 * Graphics error from SMC CORE on SS-20.
				 * SX core does not latch any useful info
				 * such as MID, access (User/Supervisor)
				 * in the memory controller asynchronous
				 * fault address register. So, we don't
				 * check for user/supervisory access to
				 * do the right thing. Instead we predicate
				 * the fault handling on the fact that only
				 * user processes access the SX core.
				 */
				handle_aflt(cpuid, AFLT_SX_CORE, afsr,
				    afar0, afar1);
			} else {
				/*
				 * if none of the above (including SX type)
				 */
				handle_aflt(cpuid, AFLT_MEM_IF, afsr,
				    afar0, afar1);
			}
		}

		if (sipr & SIR_M2SWRITE) {

			/* gather MtoS error information */
			sbusctl_getregs(&afsr, &afar0);

			/* unlock these registers */
			*(u_int *)MTS_AFSR_VADDR = 0;

/*
 * FIXME: pokefault is defined in both machdep.c and rootnex.c
 *	fingure out what's going on.
 */
			if (pokefault == -1) {
				/*EMPTY*/;
			} else if (afsr & MTSAFSR_S) {
				if (afsr & (MTSAFSR_TO | MTSAFSR_BERR))
					handle_aflt(cpuid, AFLT_M_TO_S,
					    afsr, afar0, 0);
				else
					SYS_FATAL_FLT(AFLT_M_TO_S)
#ifdef	BUG_1130786_NOTNEEDED_IN_USER_MODE
			/*
			 * For user-mode Multiple msbus-to-sbus error,
			 * just send signal to user process is good enough.
			 * However, for kernel-mode driver access, ME should
			 * still panic from above check. MTSAFSR_S denotes
			 * supervisor mode access.
			 */
			} else if (afsr & MTSAFSR_ME) {
				SYS_FATAL_FLT(AFLT_M_TO_S);
#endif	BUG_1130786_NOTNEEDED_IN_USER_MODE
			} else {
				handle_aflt(cpuid, AFLT_M_TO_S, afsr, afar0, 0);
			}
		}

		if (sipr & SIR_MODERROR) {
			module_error = 1;
		}
	}

	if (cpuid == 0) {
		register int ps = procset;
		register int i;
		for (i = 1; i < NCPU; i++)
			if (ps & (1<<i))
				while (aflt_sync[i] == 0)
					;
		for (i = 1; i < NCPU; i++) {
			if (ps & (1<<i))
				aflt_sync[i] = 0;
			flush_writebuffers();
		}
	} else {
		aflt_sync[cpuid] = 1;
		flush_writebuffers();
		while (aflt_sync[cpuid])
			;
	}

	if (module_error) {

		/*
		 * If the mmu's "watchdog reset" bit is set,
		 * dive back into the prom.
		 */
		if (mmu_chk_wdreset() != 0)
			(void) prom_stopcpu((dnode_t)0);

		/*
		 * read the asynchronous fault registers for all
		 * the MMU/cache modules of this cpu.  note that
		 * this code assumes there is a maximum of two
		 * such modules, because this is a hardware limit
		 * for sun4m.
		 * ROSS:
		 *	mod0:	afsr --> aflt_regs[0]
		 *		afar --> aflt_regs[1]
		 *	mod1:	afsr --> aflt_regs[2] (-1 if mod1 not exist)
		 *		afar --> aflt_regs[3]
		 * VIKING only:
		 *	mod0:	mfsr --> aflt_regs[0]
		 *		mfar --> aflt_regs[1]
		 * VIKING/MXCC:
		 *	mod0:	mfsr --> aflt_regs[0]
		 *		mfar --> aflt_regs[1]
		 *		error register (<63:32>) --> aflt_regs[2]
		 *		error register (<31:0>) -->  aflt_regs[3]
		 */
		(void) mmu_getasyncflt((u_int *)aflt_regs);

		if (viking) {
			/*
			 * If there is a viking only module, the async
			 * fault (l15) will occur in the case of watchdog
			 * reset (which has been taken care of) or SB error.
			 * In the case of viking only system, a l15 mod error
			 * will be broadcated for SB error after SB trap
			 * currently, we panic the system when trap occurs
			 * ignore SB case for now.
			 */
			if (mxcc) {

				/*
				 * a store buffer error [SB; bit 15 of MFSR]
				 * will cause both a trap 0x2b and a broadcast
				 * l15 [module error] interrupt.  The trap
				 * will be taken first, but afterwards there
				 * will be a pending l15 interrupt waiting for
				 * this module. We may have to do something for
				 * SB error here. But right now, we just assume
				 * that trap handler will take care of the SB
				 * error.  ignore SB error for now.
				 */

				/*
				 * We check the AFSR for this module to see
				 * if it has a valid asynchronous fault. The
				 * Async fault information is kept in MXCC
				 * error register
				 */
				if (((afsr = aflt_regs[2]) != -1) &&
					(afsr & MXCC_ERR_EV)) {
					afar0 = aflt_regs[3];
					parity_err = pac_parity_chk_dis(
					    aflt_regs[0], afsr);
					if (curthread->t_nofault &&
						!parity_err) {
						/*EMPTY*/;
					} else {
						SYS_FATAL_FLT(AFLT_MODULE)
					}
				}
			}
		} else { /* ROSS case */

		/*
		 * Check the first (and possibly only) module for
		 * this CPU.  This module has the lowest MID
		 */

			if ((afsr = aflt_regs[0]) & AFSREG_AFV) {
				afar0 = aflt_regs[1];
				subtype = AFLT_LOMID;
				/*
				 * MODULE asynchronous faults are always
				 * fatal, unless we have nofault set,
				 * because we can't tell whether it was
				 * due to a supervisor access or not, and
				 * because even a user access might mean
				 * that the cache consistency/snooping
				 * could have been compromised.
				 */
				if (!curthread->t_nofault) {
					SYS_FATAL_FLT(AFLT_MODULE)
				}
			}

			/*
			 * If there is only one module for this CPU,
			 * mmu_getasyncflt will store a -1 in aflt_regs[2].
			 * If not -1 then we check the AFSR for this second
			 * module to see if it has a valid asynchronous
			 * fault.
			 */
			if (((afsr = aflt_regs[2]) != -1) &&
				(afsr & AFSREG_AFV)) {
				afar0 = aflt_regs[3];
				subtype = AFLT_HIMID;

				/*
				 * MODULE asynchronous faults are
				 * always fatal.
				 */
				if (!curthread->t_nofault) {
					SYS_FATAL_FLT(AFLT_MODULE)
				}
			}
		}
	}

	/*
	 * Here is where all CPUs synchronize.
	 * The synchronization master is CPU 0.  CPU 0 waits for
	 * each CPU that responded to reset its entry in
	 * aflt_sync.
	 */
	if (cpuid == 0) {
		register int ps = procset;
		register int i;
		for (i = 1; i < NCPU; i++)
			if (ps & (1<<i))
				while (aflt_sync[i] == 0)
					;
		for (i = 1; i < NCPU; i++) {
			if (ps & (1<<i))
				aflt_sync[i] = 0;
			flush_writebuffers();
		}
	} else {
		aflt_sync[cpuid] = 1;
		flush_writebuffers();
		while (aflt_sync[cpuid])
			;
	}

	/*
	 * Handle the nofault case by setting aflt_ignored so
	 * that the probe routine knows that something went
	 * wrong.  It is the responsibility of the probe routine
	 * to reset this variable.
	 */
/*
 * FIXME: pokefault is defined in both machdep.c and rootnex.c
 *	fingure out what's going on.
 */
	if (pokefault == -1) {
		pokefault = 1;
		if (ecc_error) {
			if (cpuid == 0) {
				ecc_error = 0;
				if (ce_int_ok) {
					/* unlock these registers */
					*(u_int *)EFSR_VADDR = 0;

					/* enable CE interrupt */
					memctl_set_enable(EFER_EI, 0);

					/* stall until write buffer not busy */
					memctl_getregs(&reg, &reg, &reg);
				}
			} else {
				while (ecc_error)
					;
			}
		}
		return;
	}

	/*
	 * If system_fatal is set, then CPU 0 takes care of
	 * panicing.  All other CPUs return to the boot prom
	 * for re-start recovery.
	 * If sytem_fatal is set, we don't need to turn on ECC EI bit
	 * because the system is going to panic anyway.
	 */
	if (cpuid != 0) {
		if (system_fatal) {
			/*
			 * synchronization point: let cpu0 panic
			 */
			while (system_fatal)
				;
			(void) prom_stopcpu((dnode_t)0);
		} else {
			while (ecc_error)
				;
		}
		return;
	}

	/* cpuid == 0 */
	if (system_fatal) {
		cmn_err(CE_CONT, "fatal system fault: sipr=%x\n", sipr);
		if (module_error) {
			int i = 0;
			for (i = 0; i < NCPU; i++) {
				if (sys_fatal_flt[i].aflt_type ==
				    AFLT_MODULE) {
					afsr = sys_fatal_flt[i].aflt_stat;
					afar0 = sys_fatal_flt[i].aflt_addr0;
					afar1 = sys_fatal_flt[i].aflt_addr1;
					aerr0 = sys_fatal_flt[i].aflt_err0;
					aerr1 = sys_fatal_flt[i].aflt_err1;
					cmn_err(CE_CONT,
					    "Async Fault from module %x: ", i);
					cmn_err(CE_CONT, "afsr %x afar=%x\n",
					    afsr, afar0);
					cmn_err(CE_CONT, "aerr0 %x aerr1=%x\n",
					    aerr0, aerr1);
					mmu_log_module_err(afsr, afar0, aerr0,
					    aerr1);
				}
			}
		} else {
			afsr = sys_fatal_flt[cpuid].aflt_stat;
			afar0 = sys_fatal_flt[cpuid].aflt_addr0;
			afar1 = sys_fatal_flt[cpuid].aflt_addr1;
			aerr0 = sys_fatal_flt[cpuid].aflt_err0;
			aerr1 = sys_fatal_flt[cpuid].aflt_err1;
		}

		switch (sys_fatal_flt[cpuid].aflt_type) {
		case AFLT_MODULE:
			break;
		case AFLT_MEM_IF:
			if ((afsr & EFSR_GE) && (sx_ctlr_present)) {
				cmn_err(CE_CONT, SX_FATAL_MSG1);
				cmn_err(CE_CONT, "\n");
			} else if ((afsr == 0) && (sx_ctlr_present)) {
				cmn_err(CE_CONT, SX_FATAL_MSG2);
				cmn_err(CE_CONT, "\n");
			} else
				cmn_err(CE_CONT, "Async Fault from memory: ");
			cmn_err(CE_CONT, "afsr=%x afar=%x %x\n",
			    afsr, afar0, afar1);
			log_mem_err(afsr, afar0, afar1);
			break;
		case AFLT_M_TO_S:
			cmn_err(CE_CONT, "Async Fault from M-to-S: ");
			cmn_err(CE_CONT, "afsr=%x afar=%x\n",
			    afsr, afar0);
			log_mtos_err(afsr, afar0);
			break;
		default:
			if (module_error == 0) {
				cmn_err(CE_CONT, "Unknown Fault type %d; "
				    "afsr=%x afar=%x %x\n",
				    sys_fatal_flt[cpuid].aflt_type, afsr,
				    afar0, afar1);
			}
			break;
		}
		cmn_err(CE_PANIC, "Fatal Asynchronous Fault\n");
		/* NOTREACHED */
		system_fatal = 0;
	} else if (ecc_error) {
		ecc_error = 0;
		if (ce_int_ok) {
			/* unlock these registers */
			*(u_int *)EFSR_VADDR = 0;

			/* enable CE interrupt */
			memctl_set_enable(EFER_EI, 0);

			/* stall until write buffer not busy */
			memctl_getregs(&reg, &reg, &reg);
		}
	}
}


/*
 * This routine is called by autovectoring a soft level 12
 * interrupt, which was sent by handle_aflt to process a
 * non-fatal asynchronous fault.
 */
sun4m_process_aflt()
{
	u_int tail;
	register proc_t *pp = ttoproc(curthread);
	u_int afsr;
	u_int afar0;
	u_int afar1;
	u_int cpuid = curthread->t_cpu->cpu_id;
	int ce = 0;
	int slot;
	register struct memslot *mp;
	struct ecc_handler_args eha;
	int (*func)(void *, void *);
	extern int nvsimm_present;
	extern struct memslot memslots[];
	int nvslot(u_int, u_int);

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
		tail = (a_tail[cpuid] + 1) % MAX_AFLTS;
		a_tail[cpuid] = tail;

		afsr = a_flts[cpuid][tail].aflt_stat;
		afar0 = a_flts[cpuid][tail].aflt_addr0;
		afar1 = a_flts[cpuid][tail].aflt_addr1;

		/*
		 * Process according to the type of write buffer
		 * fault.
		 */
		switch (a_flts[cpuid][tail].aflt_type) {

		case	AFLT_MEM_IF:
			/*
			 * If the error relates to a slot populated with an
			 * nvsimm board
			 */
			if (nvsimm_present &&
			    (slot = nvslot(afar0, afar1)) != -1) {
				mp = &memslots[slot];
				/*
				 * We use a local to get an atomic test and
				 * call
				 */
				func = mp->ms_func;
				if (func != NULL) {
					eha.e_uncorrectable =
					    (afsr & EFSR_UE);
					eha.e_addrhi = (afar0 & 0xf);
					eha.e_addrlo = afar1;
					if ((*func)(mp->ms_arg,
					    (void *) &eha) == AFLT_HANDLED)
						return (1);
				}
			}
			/* only correctable are non-fatal */
			if (afsr & EFSR_CE) {
				ce = 1;
				if (report_ce_log || report_ce_console)
					log_ce_error = 1;
			}
			log_mem_err(afsr, afar0, afar1);
			if (afsr & EFSR_UE) {
				if (fix_nc_ecc(afsr, afar0, afar1) == -1) {
					cmn_err(CE_CONT, "AFSR = 0x%x, "
					    "AFAR0 = 0x%x, AFAR1 = 0x%x\n",
						afsr, afar0, afar1);
					cmn_err(CE_PANIC, "Asynchronous fault");
				}
			}
			/*
			 * XXX - A future enhancement may be to
			 * "retire" pages which have memory which
			 * is going bad.  This could be done by
			 * defining a new I/F in vm_page.c which
			 * puts a page on a "retired" list.
			 * Some design issues to ressolve:
			 * 1. How do pages remain retired across
			 * boots?
			 * 2. Can we do anything about pages which
			 * don't have a corresponding page struct?
			 * - probably too much work.
			 * 3. What criteria do we use to decide to
			 * retire a page?  Any page which gets a
			 * correctable ECC error?  Any page which
			 * results in another ECC error when tested?
			 * 4. What limit on number of pages that
			 * can be retired?  System configurable?
			 */
			break;

		case    AFLT_SX_CORE:
			/*
			 * SMC (SS-20) graphics core errors and misreferenced
			 * slot errors. This code is only executed when
			 * the user has caused a misreferenced slot error or
			 * when SX programming rules are violated.
			 */
			if ((afsr & EFSR_GE) && (afsr & EFSR_SE)) {
				psignal(pp, SIGBUS);
				break;
			} else {
				/* CSTYLED */
				int (*sx_handler)(proc_t *) =
				    (int (*)(proc_t *))sx_aflt_fun;

				if (sx_handler != NULL) {
					(*sx_handler)(pp);
					break;
				}
			}
			break;

		case	AFLT_M_TO_S:
			cmn_err(CE_WARN, "M-to-S Asynchronous fault due to ");
			cmn_err(CE_CONT, "user write - non-fatal\n");
			log_mtos_err(afsr, afar0);
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
		if (!ce)
			cmn_err(CE_WARN,
				"AFSR = 0x%x, AFAR0 = 0x%x, AFAR1 = 0x%x\n",
				afsr, afar0, afar1);
		else {
			ce = 0;
			log_ce_error = 0;
		}
	}

	async_errq_full = 0;

	return (1);
}

/*
 * This routine is called by autovectoring a soft level 8 interrupt,
 * which is called to schedule a timeout to re-enable correctable memory
 * errors.
 */
void
sun4m_timeout_cei(void)
{
	if (ce_int_ok == 0)
		(void) timeout(enable_cei, 0, ceint_off_time * hz);
}

/*
 * This routine re-enables interrupt generation for correctable memory
 * errors.  It is scheduled from a timeout call from handle_aflt()
 * after an async fault queue overflow, or from log_ce_mem_err() when
 * to many errors have been reported.
 */
/*ARGSUSED*/
void
enable_cei(void *arg)
{
	u_int reg;

	ce_int_ok = 1;
	log_ce_error = 0;

	/* unlock these registers */
	*(u_int *)EFSR_VADDR = 0;

	/* enable CE interrupt */
	memctl_set_enable(EFER_EI, 0);

	/* stall until write buffer not busy */
	memctl_getregs(&reg, &reg, &reg);
}

/*
 * This routine is called by autovectoring a soft level 12
 * interrupt, which was sent by handle_aflt to process a
 * non-fatal asynchronous fault.
 */

/*
 * async() calls this routine to store info. for a particular
 * non-fatal asynchronous fault in this CPU's queue of
 * asynchronous faults to be handled, and to invoke a level-12
 * interrupt thread to process this fault.
 */
void
handle_aflt(
	u_int cpu_id,	/* CPU responsible for this fault */
	u_int type,	/* type of asynchronous fault */
	u_int afsr,	/* Asynchronous Fault Status Register */
	u_int afar0,	/* Asynchronous Fault Address Register 0 */
	u_int afar1)	/* Asynchronous Fault Address Register 1 */
{
	u_int head;

	/*
	 * Increment the head of the circular queue of fault
	 * structures by 1 (modulo MAX_AFLTS).
	 */
	head = (a_head[cpu_id] + 1) % MAX_AFLTS;
	/*
	 * if queue was full, discard entry and disable correctable
	 * error interrupt generation temporarily.
	 */
	if (head == a_tail[cpu_id]) {
		async_errq_full = 1;
		ce_int_ok = 0;
		log_ce_error = 1;
		send_dirint(cpu_id, AFLT_HANDLER_LEVEL);
		send_dirint(cpu_id, TIMEOUT_CEI_LEVEL);
		cmn_err(CE_CONT, "Excessive Asynchronous Faults: ");
		cmn_err(CE_CONT, "Possible Memory Deterioration\n");
		return;
	}
	a_head[cpu_id] = head;
	/*
	 * Store the asynchronous fault information supplied.
	 */
	a_flts[cpu_id][head].aflt_type = type;
	a_flts[cpu_id][head].aflt_stat = afsr;
	a_flts[cpu_id][head].aflt_addr0 = afar0;
	a_flts[cpu_id][head].aflt_addr1 = afar1;

	/*
	 * Send a soft directed interrupt at level AFLT_HANDLER_LEVEL
	 * to onesself to process this non-fatal asynchronous
	 * fault.
	 */
	send_dirint(cpu_id, AFLT_HANDLER_LEVEL);
}

void
sun4m_log_module_err(afsr, afar0)
	u_int afsr;	/* Asynchronous Fault Status Register */
	u_int afar0;	/* Asynchronous Fault Address Register */
{
	if (afsr & MMU_AFSR_BE)
		cmn_err(CE_WARN, "\tM-Bus Bus Error\n");
	if (afsr & MMU_AFSR_TO)
		cmn_err(CE_WARN, "\tM-Bus Timeout Error\n");
	if (afsr & MMU_AFSR_UC)
		cmn_err(CE_WARN, "\tM-Bus Uncorrectable Error\n");
	if (afsr & MMU_AFSR_AFV)
		cmn_err(CE_WARN, "\tPhysical Address = (space %x) %x\n",
		    (afsr&MMU_AFSR_AFA)>>MMU_AFSR_AFA_SHIFT, afar0);
}

char *nameof_siz[] = {
	"byte", "short", "word", "double", "quad", "line", "[siz=6?]",
	"[siz=7?]",
};

char *nameof_ssiz[] = {
	"word", "byte", "short", "[ssiz=3?]", "quad", "line", "[ssiz=6?]",
	"double",
};

void
log_mtos_err(u_int afsr, u_int afar0)
{
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
	cmn_err(CE_WARN, "\tRequested transaction: %s at (space %x) %x\n",
	    nameof_siz[(afsr & MTSAFSR_SIZ) >> MTSAFSR_SIZ_SHFT],
	    afsr & MTSAFSR_PA, afar0);
	cmn_err(CE_WARN, "\tSpecific cycle: %s xfer at %x:%x\n",
	    nameof_ssiz[(afsr & MTSAFSR_SSIZ) >> MTSAFSR_SSIZ_SHFT],
	    afsr & MTSAFSR_PA,
	    ((afar0 & ~0x1F) | ((afsr & MTSAFSR_SA) >> MTSAFSR_SA_SHFT)));
}

/*
 * This table used to determine which bit(s) is(are) bad when an ECC
 * error occurrs.  The array is indexed by the 8-bit syndrome which
 * comes from the ECC Memory Fault Status Register.  The entries
 * of this array have the following semantics:
 *
 *	00-63	The number of the bad bit, when only one bit is bad.
 *	64	ECC bit C0 is bad.
 *	65	ECC bit C1 is bad.
 *	66	ECC bit C2 is bad.
 *	67	ECC bit C3 is bad.
 *	68	ECC bit C4 is bad.
 *	69	ECC bit C5 is bad.
 *	70	ECC bit C6 is bad.
 *	71	ECC bit C7 is bad.
 *	72	Two bits are bad.
 *	73	Three bits are bad.
 *	74	Four bits are bad.
 *	75	More than Four bits are bad.
 *	76	NO bits are bad.
 * Based on "Galaxy Memory Subsystem SPECIFICATION" rev 0.6, pg. 28.
 */
char ecc_syndrome_tab[] =
{
76, 64, 65, 72, 66, 72, 72, 73, 67, 72, 72, 73, 72, 73, 73, 74,
68, 72, 72, 32, 72, 57, 75, 72, 72, 37, 49, 72, 40, 72, 72, 44,
69, 72, 72, 33, 72, 61,	 4, 72, 72, 75, 53, 72, 45, 72, 72, 41,
72,  0,	 1, 72, 10, 72, 72, 75, 15, 72, 72, 75, 72, 73, 73, 72,
70, 72, 72, 42, 72, 59, 39, 72, 72, 75, 51, 72, 34, 72, 72, 46,
72, 25, 29, 72, 27, 74, 72, 75, 31, 72, 74, 75, 72, 75, 75, 72,
72, 75, 36, 72,	 7, 72, 72, 54, 75, 72, 72, 62, 72, 48, 56, 72,
73, 72, 72, 75, 72, 75, 22, 72, 72, 18, 75, 72, 73, 72, 72, 75,
71, 72, 72, 47, 72, 63, 75, 72, 72,  6, 55, 72, 35, 72, 72, 43,
72,  5, 75, 72, 75, 72, 72, 50, 38, 72, 72, 58, 72, 52, 60, 72,
72, 17, 21, 72, 19, 74, 72, 75, 23, 72, 74, 75, 72, 75, 75, 72,
73, 72, 72, 75, 72, 75, 30, 72, 72, 26, 75, 72, 73, 72, 72, 75,
72,  8, 13, 72,	 2, 72, 72, 73,	 3, 72, 72, 73, 72, 75, 75, 72,
73, 72, 72, 73, 72, 75, 16, 72, 72, 20, 75, 72, 75, 72, 72, 75,
73, 72, 72, 73, 72, 75, 24, 72, 72, 28, 75, 72, 75, 72, 72, 75,
74, 12,	 9, 72, 14, 72, 72, 75, 11, 72, 72, 75, 72, 75, 75, 74,
};

#define	MAX_CE_ERROR	255
u_int	max_ce_err = MAX_CE_ERROR;


struct  ce_info {
	char    name[8];
	u_int   cnt;
	clock_t	timestamp;
};

#define	MAX_SIMM	256
struct  ce_info mem_ce_simm[MAX_SIMM];



void
log_ce_mem_err(
	u_int afsr,	/* ECC Memory Fault Status Register */
	u_int afar0,	/* ECC Memory Fault Address Register 0 */
	u_int afar1,	/* ECC Memory Fault Address Register 1 */
	char *(*get_unum)())
{
	int i, size, block, offset;
	u_int aligned_afar1;
	char *unum;
	u_int reg;

	u_int syn_code; /* Code from ecc_syndrome_tab */

	/*
	 * Use the 8-bit syndrome in the EFSR to index
	 * ecc_syndrome_tab to get code indicating which bit(s)
	 * is(are) bad.
	 */
	syn_code = ecc_syndrome_tab[((afsr & EFSR_SYND) >> EFSR_SYND_SHFT)];

	/*
	 * The EFAR contains the address of an 8-byte group. For 16 and 32
	 * bytes brust, the address is the start of the brust group. So
	 * added the proper offset according to the info given in the
	 * EFSR and the syndrome table.
	 */
	offset = 0;
	/*
	 * Which of the 8 byte group during 16 bytes and 32 bytes
	 * transfer
	 */
	block = (afsr & EFSR_DW) >> 4;

	size = (afar0 & EFAR0_SIZ) >> 8;
	switch (size) {
		case 4:
		case 5:   offset = block*8;
			break;
		default:  offset = 0; break;
	}

	/*
	 * For some reason, the EFAR sometimes contains a non-8 byte aligned
	 * address. We should ignore the lowest 3 bits
	 */
	aligned_afar1 = afar1 & 0xFFFFFFF8;

	if (syn_code < 72) {
	/*
	 * Syn_code contains the bit no. of the group that is bad.
	 */
		if (syn_code < 64)
			offset = offset + (7 - syn_code/8);
		else offset = offset + (7 - syn_code%8);

		if ((unum = (*get_unum)(aligned_afar1 + (u_int) offset,
			afar0 & EFAR0_PA)) == (char *)0)
			cmn_err(CE_WARN, "simm decode function failed\n");
	} else {
		for (i = 0; i < 8; i++) {
			if ((unum = (*get_unum)(aligned_afar1 + offset +
				(u_int) i, afar0 & EFAR0_PA)) == (char *)0)
				cmn_err(CE_WARN, "simm decode func failed\n");
		}
	}
	for (i = 0; i < MAX_SIMM; i++) {
		if (mem_ce_simm[i].name[0] == NULL) {
			(void) strcpy(mem_ce_simm[i].name, unum);
			mem_ce_simm[i].cnt++;
			mem_ce_simm[i].timestamp = lbolt;
			break;
		} else if (strcmp(unum, mem_ce_simm[i].name) == 0) {
			if (++mem_ce_simm[i].cnt > max_ce_err) {
				cmn_err(CE_WARN, "Multiple Softerrors: \n");
				cmn_err(CE_CONT, "Seen %x Corrected Softerrors",
					mem_ce_simm[i].cnt);
				cmn_err(CE_CONT, " from SIMM %s\n", unum);
				cmn_err(CE_CONT, "\tCONSIDER REPLACING ");
				cmn_err(CE_CONT, "THE SIMM.\n");
				mem_ce_simm[i].cnt = 0;
				log_ce_error = 1;

				/*
				 * if max errors occurs too fast then
				 * temporarily disable correctable error
				 * interrupt generation.
				 */
				if (lbolt - mem_ce_simm[i].timestamp < hz &&
						!async_errq_full) {
					int cpuid = 0;

					ce_int_ok = 0;

					/* disable ce interrupts */
					memctl_set_enable(0, EFER_EI);

					/* unlock these registers */
					*(u_int *)EFSR_VADDR = 0;

					/* stall for write buffer   */
					memctl_getregs(&reg, &reg, &reg);

					cpuid = getprocessorid();
					send_dirint(cpuid, TIMEOUT_CEI_LEVEL);
				}
				mem_ce_simm[i].timestamp = lbolt;
			}
			break;
		}
	}
	if (i >= MAX_SIMM)
		cmn_err(CE_WARN, "Softerror: mem_ce_simm[] out of space.\n");
	if (log_ce_error) {
		cmn_err(CE_WARN, "Softerror: ECC Memory Error Corrected.\n");
		if (afsr & EFSR_ME)
			cmn_err(CE_CONT, "\tMultiple Error Bit Set.\n");
		if (syn_code < 72)
			cmn_err(CE_CONT, "\tCorrected SIMM at: %s ", unum);
		else
			cmn_err(CE_CONT, "\tPossible Corrected SIMM at: %s ",
				unum);
		cmn_err(CE_CONT, "\toffset is %d", offset);
		if (syn_code < 64)
			cmn_err(CE_CONT, "\tBit %2d was corrected", syn_code);
		else if (syn_code < 72)
			cmn_err(CE_CONT, "\tECC Bit %2d was corrected",
				syn_code - 64);
		else {
		    switch (syn_code) {
			case 72:
				cmn_err(CE_CONT, "\tTwo bits were corrected");
				break;
			case 73:
				cmn_err(CE_CONT, "\tThree bits were corrected");
				break;
			case 74:
				cmn_err(CE_CONT, "\tFour bits were corrected");
				break;
			case 75:
				cmn_err(CE_CONT, "\tMore than Four bits ");
				cmn_err(CE_CONT, "were corrected");
				break;
			default:
				break;
		    }
		}
	}
}

void
log_ue_mem_err(
	u_int afar0,    /* ECC Memory Fault Address Register 0 */
	u_int afar1,    /* ECC Memory Fault Address Register 1 */
	char *(*get_unum)())
{
	char *unum;

	if ((unum = (*get_unum)(afar1, afar0 & EFAR0_PA)) == (char *)0)
		cmn_err(CE_WARN, "simm decode function failed\n");
	else
		cmn_err(CE_WARN, "Uncorrected SIMM at: %s\n", unum);
}

void
log_mem_err(
	u_int afsr,	/* Asynchronous Fault Status Register */
	u_int afar0,	/* Asynchronous Fault Address Register 0 */
	u_int afar1)	/* Asynchronous Fault Address Register 1 */
{
	char *(*get_unum) ();


	if ((mc_type == MC_MMC) && (afsr & EFSR_TO)) {
		cmn_err(CE_WARN, "\tTimeout on Write access to ");
		cmn_err(CE_CONT, "expansion memory.\n");
	}
	if (((mc_type == MC_EMC) || (mc_type == MC_SMC)) && (afsr & EFSR_SE)) {
		cmn_err(CE_WARN, "\tMisreferences Slot Error.\n");
	}
	if (afsr & EFSR_UE) {
		/*
		 * uncorrectable ecc on partial write, or on read
		 */
		cmn_err(CE_WARN, "\tUncorrectable ECC Memory Error.\n");
	}

	(void) prom_getprop(prom_nextnode(0), "get-unum", (caddr_t)&get_unum);
	if (get_unum == (char *(*)())0)
		cmn_err(CE_WARN, "no simm decode function available\n");
	else if (afsr & EFSR_UE)
		log_ue_mem_err(afar0, afar1, get_unum);
	else if (afsr & EFSR_CE)
		log_ce_mem_err(afsr, afar0, afar1, get_unum);

	if (!(afsr & EFSR_CE) || (log_ce_error))
		cmn_err(CE_CONT, "\tPhysical Address = (ASI:0x%x)0x%x\n",
		    afar0 & EFAR0_PA, afar1);
}

void
log_async_err(u_int afsr, u_int afar)
{
	/* XXX - change defines */
	if (afsr & MTSAFSR_LE)
		cmn_err(CE_WARN, "\tLate Error\n");
	if (afsr & MTSAFSR_BERR)
		cmn_err(CE_WARN, "\tBus Error\n");
	if (afsr & MTSAFSR_TO)
		cmn_err(CE_WARN, "\tTimeout\n");
	if (afsr & AFSREG_PERR)
		cmn_err(CE_WARN, "\tParity Error=%x\n", afsr & AFSREG_PERR);

	cmn_err(CE_WARN, "\tpa=%x\n", afar);
}

/*
 * Send a directed interrupt of specified level to a cpu.
 */

void
send_dirint(cpuix, int_level)
int	cpuix;			/* cpu to be interrupted */
int	int_level;		/* interrupt level */
{

	/*
	 * This is here to force the module write buffer to be flushed
	 * when the read access is done to the non-cacheable data
	 * Workwround for Ross hardware bug. Check for ross_hw_workaround2
	 * determines that this workaround is needed for Ross modules only.
	 */

	if (ross_hw_workaround2) {
		v_interrupt_addr[cpuix]->set_pend =
			1 << (int_level + IR_SOFT_SHIFT);
		ross_hd_bug = v_interrupt_addr[cpuix]->pend;
	} else {
		v_interrupt_addr[cpuix]->set_pend =
			1 << (int_level + IR_SOFT_SHIFT);
	}
}

/*
 * Clear a directed interrupt of specified level on a cpu.
 */

void
clr_dirint(
	int	cpuix,			/* cpu to be interrupted */
	int	int_level)		/* interrupt level */
{
	v_interrupt_addr[cpuix]->clr_pend = 1 << (int_level + IR_SOFT_SHIFT);
}

int
sun4m_impl_bustype(u_int pfn)
{
	register int bt;
	extern int mem_bus_type(u_int);

	switch (PTE_BUSTYPE(pfn)) {

	case 0:
		if ((mc_type == MC_EMC) || (mc_type == MC_SMC)) {
			if (pfn < EMC_MAX_PFN)
				bt = mem_bus_type(pfn);
			else
				bt = BT_OBIO;
		} else
			bt = BT_DRAM;
		break;

	case 0xe:
		bt = BT_SBUS;
		break;

	case 0x8:
	case 0x9:
	case 0xf:
		bt = (((pfn >> 16) & 0xFF) == 0xFD) ? BT_UNKNOWN : BT_OBIO;
		break;

	default:
		bt = BT_UNKNOWN;
		break;
	}
	return (bt);
}

/*
 * Returns slot number if addresses refer to nvram, else returns -1
 */
int
nvslot(u_int afar0, u_int afar1)
{
	extern struct memslot memslots[];
	int slot;

	if ((afar0 & 0xf) != 0)
		return (-1);

	slot = PMEM_SLOT(afar1);
	if (memslots[slot].ms_bustype == BT_NVRAM)
		return (slot);
	else
		return (-1);
}

/*
 * Almost all sun4m implementations work with a fully functional SPARC V8
 * processor that they were intended to .. however we allow an
 * implementation to override hardware multiply and divide for the sake
 * of legacy implementations like the "almost V8" Ross/CY605.
 *
 * This function expresses the default capabilities.
 */
/*ARGSUSED*/
int
sparcV8_get_hwcap_flags(int inkernel)
{
	return (AV_SPARC_HWMUL_32x32 | AV_SPARC_HWDIV_32x32);
}
