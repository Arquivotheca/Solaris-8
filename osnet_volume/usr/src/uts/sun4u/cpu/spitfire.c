/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)spitfire.c	1.54	99/11/16 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/cpu.h>
#include <sys/elf_SPARC.h>
#include <vm/hat_sfmmu.h>
#include <sys/cpuvar.h>
#include <sys/spitregs.h>
#include <sys/async.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/dditypes.h>
#include <sys/prom_debug.h>
#include <sys/cpu_module.h>
#include <sys/vmsystm.h>
#include <sys/prom_plat.h>

/*
 * Support for spitfire modules
 */

extern void flush_ecache(uint64_t physaddr, size_t size);
extern uint64_t get_lsu(void);
extern void set_lsu(uint64_t ncc);
extern void clr_datapath(void);

void cpu_ce_error(struct regs *rp, u_long p_afar, u_long p_afsr,
	u_int p_afsr_high, u_int p_afar_high);
void cpu_async_error(struct regs *rp, u_long p_afar, u_long p_afsr,
	u_int p_afsr_high, u_int p_afar_high);

void cpu_read_paddr(struct async_flt *ecc, short verbose, short ce_err);

static void log_ce_err(struct async_flt *ecc, char *unum);
static int log_ue_err(struct async_flt *ecc, char *unum);
static void check_misc_err(uint64_t p_afsr, uint64_t p_afar);

static u_short ecc_gen(u_int high_bytes, u_int low_bytes);
static int clear_ecc(struct async_flt *ecc);
static void ue_check_cpus(void);
static u_int get_cpu_status(uint64_t arg);

/*
 * This table is used to determine which bit(s) is(are) bad when an ECC
 * error occurrs.  The array is indexed an 8-bit syndrome.  The entries
 * of this array have the following semantics:
 *
 *      00-63   The number of the bad bit, when only one bit is bad.
 *      64      ECC bit C0 is bad.
 *      65      ECC bit C1 is bad.
 *      66      ECC bit C2 is bad.
 *      67      ECC bit C3 is bad.
 *      68      ECC bit C4 is bad.
 *      69      ECC bit C5 is bad.
 *      70      ECC bit C6 is bad.
 *      71      ECC bit C7 is bad.
 *      72      Two bits are bad.
 *      73      Three bits are bad.
 *      74      Four bits are bad.
 *      75      More than Four bits are bad.
 *      76      NO bits are bad.
 * Based on "Galaxy Memory Subsystem SPECIFICATION" rev 0.6, pg. 28.
 */
static char ecc_syndrome_tab[] =
{
	76, 64, 65, 72, 66, 72, 72, 73, 67, 72, 72, 73, 72, 73, 73, 74,
	68, 72, 72, 32, 72, 57, 75, 72, 72, 37, 49, 72, 40, 72, 72, 44,
	69, 72, 72, 33, 72, 61,  4, 72, 72, 75, 53, 72, 45, 72, 72, 41,
	72,  0,  1, 72, 10, 72, 72, 75, 15, 72, 72, 75, 72, 73, 73, 72,
	70, 72, 72, 42, 72, 59, 39, 72, 72, 75, 51, 72, 34, 72, 72, 46,
	72, 25, 29, 72, 27, 74, 72, 75, 31, 72, 74, 75, 72, 75, 75, 72,
	72, 75, 36, 72,  7, 72, 72, 54, 75, 72, 72, 62, 72, 48, 56, 72,
	73, 72, 72, 75, 72, 75, 22, 72, 72, 18, 75, 72, 73, 72, 72, 75,
	71, 72, 72, 47, 72, 63, 75, 72, 72,  6, 55, 72, 35, 72, 72, 43,
	72,  5, 75, 72, 75, 72, 72, 50, 38, 72, 72, 58, 72, 52, 60, 72,
	72, 17, 21, 72, 19, 74, 72, 75, 23, 72, 74, 75, 72, 75, 75, 72,
	73, 72, 72, 75, 72, 75, 30, 72, 72, 26, 75, 72, 73, 72, 72, 75,
	72,  8, 13, 72,  2, 72, 72, 73,  3, 72, 72, 73, 72, 75, 75, 72,
	73, 72, 72, 73, 72, 75, 16, 72, 72, 20, 75, 72, 75, 72, 72, 75,
	73, 72, 72, 73, 72, 75, 24, 72, 72, 28, 75, 72, 75, 72, 72, 75,
	74, 12,  9, 72, 14, 72, 72, 75, 11, 72, 72, 75, 72, 75, 75, 74
};

#define	SYND_TBL_SIZE 256

/*
 * Hack for determining UDBH/UDBL, for later cpu-specific error reporting.
 * Cannot use bit 3 in afar, because it is a valid bit on a Sabre.
 */
#define	UDBL_REG	0x8000
#define	UDBL(synd)	((synd & UDBL_REG) >> 15)
#define	SYND(synd)	(synd & 0x7FFF)

/*
 * Maximum number of contexts for Spitfire.
 */
#define	MAX_NCTXS	(1 << 13)

/*
 * Save the cache bootup state for use when internal
 * caches are to be re-enabled after an error occurs.
 */
uint64_t	cache_boot_state = 0;

/*
 * Useful for hardware debugging.
 */
int	async_err_panic = 0;

/*
 * PA[31:0] represent Displacement in UPA configuration space.
*/
uint_t	root_phys_addr_lo_mask = 0xffffffff;

void
cpu_setup(void)
{
	extern int at_flags;
#if defined(__sparcv9) && defined(SF_ERRATA_57)
	extern caddr_t errata57_limit;
#endif

	cache |= (CACHE_VAC | CACHE_PTAG | CACHE_IOCOHERENT);

	at_flags = EF_SPARC_32PLUS | EF_SPARC_SUN_US1;

	/*
	 * save the cache bootup state.
	 */
	cache_boot_state = get_lsu() & (LSU_IC | LSU_DC);

	/*
	 * Use the maximum number of contexts available for Spitfire.
	 */
	nctxs = MAX_NCTXS;

	if (use_page_coloring) {
		do_pg_coloring = 1;
		if (use_virtual_coloring)
			do_virtual_coloring = 1;
	}
	isa_list =
#ifdef __sparcv9
		"sparcv9+vis sparcv9 "
#endif
		"sparcv8plus+vis sparcv8plus "
		"sparcv8 sparcv8-fsmuld sparcv7 sparc";

#ifdef __sparcv9
	/*
	 * On Spitfire, there's a hole in the address space
	 * that we must never map (the hardware only support 44-bits of
	 * virtual address).  Later CPUs are expected to have wider
	 * supported address ranges.
	 *
	 * See address map on p23 of the UltraSPARC 1 user's manual.
	 */
	hole_start = (caddr_t)0x80000000000ull;
	hole_end = (caddr_t)0xfffff80000000000ull;

	/*
	 * A spitfire call bug requires us to be a further 4Gbytes of
	 * firewall from the spec.
	 *
	 * See Spitfire Errata #21
	 */
	hole_start = (caddr_t)((uintptr_t)hole_start - (1ul << 32));
	hole_end = (caddr_t)((uintptr_t)hole_end + (1ul << 32));
#endif

#if defined(__sparcv9) && defined(SF_ERRATA_57)
	errata57_limit = (caddr_t)0x80000000ul;
#endif
}

void
fini_mondo(void)
{
}

void
syncfpu(void)
{
}

/*
 * Correctable ecc error trap handler
 */
/*ARGSUSED*/
void
cpu_ce_error(struct regs *rp, u_long p_afar, u_long p_afsr,
	u_int p_afsr_high, u_int p_afar_high)
{
	u_short sdbh, sdbl;
	u_short e_syndh, e_syndl;
	struct async_flt ecc;

#ifdef __sparcv9
	uint64_t t_afar = p_afar;
	uint64_t t_afsr = p_afsr;
#else
	uint64_t t_afar = (uint64_t)p_afar | ((uint64_t)p_afar_high << 32);
	uint64_t t_afsr = (uint64_t)p_afsr | ((uint64_t)p_afsr_high << 32);
#endif

	/*
	 * Note: the Spitfire data buffer error registers
	 * (upper and lower halves) are or'ed into the upper
	 * word of the afsr by ce_err().
	 */
	sdbh = (u_short)((t_afsr >> 33) & 0x3FF);
	sdbl = (u_short)((t_afsr >> 43) & 0x3FF);

	e_syndh = (u_char)(sdbh & (u_int)P_DER_E_SYND);
	e_syndl = (u_char)(sdbl & (u_int)P_DER_E_SYND);

	t_afsr &= S_AFSR_MASK;
	t_afar &= SABRE_AFAR_PA;	/* must use Sabre AFAR mask */

	/*
	 * Check for validity of the AFSR and the UDBs for this trap
	 */
	check_misc_err(t_afsr, t_afar);

	if (t_afsr & P_AFSR_UE) {
		cmn_err(CE_PANIC, "CPU%d CE Error Trap with Uncorrectables: "
		    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x "
		    "UDBH 0x%x UDBL 0x%x", CPU->cpu_id,
		    (uint32_t)(t_afsr >> 32), (uint32_t)t_afsr,
		    (uint32_t)(t_afar >> 32), (uint32_t)t_afar,
		    sdbh, sdbl);
		/* NOTREACHED */
	}
	if ((t_afsr & P_AFSR_CE) == 0) {
		cmn_err(CE_PANIC, "CPU%d CE Error Trap, CE not set in AFSR: "
		    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x "
		    "UDBH 0x%x UDBL 0x%x", CPU->cpu_id,
		    (uint32_t)(t_afsr >> 32), (uint32_t)t_afsr,
		    (uint32_t)(t_afar >> 32), (uint32_t)t_afar,
		    sdbh, sdbl);
		/* NOTREACHED */
	}
	if (((sdbh & P_DER_CE) == 0) && ((sdbl & P_DER_CE) == 0)) {
		cmn_err(CE_PANIC, "CPU%d CE Error Trap, CE not set in UDBs: "
		    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x "
		    "UDBH 0x%x UDBL 0x%x", CPU->cpu_id,
		    (uint32_t)(t_afsr >> 32), (uint32_t)t_afsr,
		    (uint32_t)(t_afar >> 32), (uint32_t)t_afar,
		    sdbh, sdbl);
		/* NOTREACHED */
	}

	if ((sdbh >> 8) & 1) {
		bzero(&ecc, sizeof (struct async_flt));
		ecc.flt_stat = t_afsr;
		ecc.flt_addr = t_afar;
		ecc.flt_status = ECC_C_TRAP;
		ecc.flt_bus_id = getprocessorid();
		ecc.flt_inst = CPU->cpu_id;
		ecc.flt_size = 3;	/* 8 byte alignment */
		ecc.flt_func = (afunc)log_ce_err;
		ecc.flt_in_memory =
			(pf_is_memory(ecc.flt_addr >> MMU_PAGESHIFT)) ? 1: 0;
		ecc.flt_synd = e_syndh;
		ce_error(&ecc);
	}
	if ((sdbl >> 8) & 1) {
		bzero(&ecc, sizeof (struct async_flt));
		ecc.flt_stat = t_afsr;
		ecc.flt_addr = t_afar | 0x8;	/* Sabres do not have a UDBL */
		ecc.flt_status = ECC_C_TRAP;
		ecc.flt_bus_id = getprocessorid();
		ecc.flt_inst = CPU->cpu_id;
		ecc.flt_size = 3;	/* 8 byte alignment */
		ecc.flt_func = (afunc)log_ce_err;
		ecc.flt_in_memory =
			(pf_is_memory(ecc.flt_addr >> MMU_PAGESHIFT)) ? 1: 0;
		ecc.flt_synd = e_syndl | UDBL_REG;
		ce_error(&ecc);
	}
}

/*
 * Cpu specific CE logging routine
 */
static void
log_ce_err(struct async_flt *ecc, char *unum)
{
	uint64_t t_afsr = ecc->flt_stat;
	uint64_t t_afar = ecc->flt_addr;
	uint32_t udbl = UDBL(ecc->flt_synd);
	uint32_t synd = SYND(ecc->flt_synd);

	if (!ce_verbose)
		return;

	cmn_err(CE_CONT, "CPU%d CE Error: AFSR 0x%08x.%08x "
	    "AFAR 0x%08x.%08x %s Syndrome 0x%x MemMod %s\n",
	    ecc->flt_inst, (uint32_t)(t_afsr >> 32), (uint32_t)t_afsr,
	    (uint32_t)(t_afar >> 32), (uint32_t)t_afar,
	    udbl?"UDBL":"UDBH", synd, unum);
}

/*
 * check for a valid ce syndrome, then call the
 * displacement flush scrubbing code, and then check the afsr to see if
 * the error was persistent or intermittent. Reread the afar/afsr to see
 * if the error was not scrubbed successfully, and is therefore sticky.
 */
void
cpu_ce_scrub_mem_err(struct async_flt *ecc)
{
	ASSERT(getpil() > LOCK_LEVEL);

	/* disable ECC error traps */
	set_error_enable(EER_ECC_DISABLE);

	scrubphys(ALIGN_64(ecc->flt_addr), cpunodes[CPU->cpu_id].ecache_size);

	/* clear any ECC errors from the scrub */
	if (clear_ecc(ecc) != 0) {
		ecc->flt_status |= ECC_PERSISTENT;

		/* did the scrub correct the CE? */
		cpu_read_paddr(ecc, 0, 1);

		/* clear any ECC errors from the verify */
		if (clear_ecc(ecc) != 0) {
			ecc->flt_status &= ~ECC_PERSISTENT;
			ecc->flt_status |= ECC_STICKY;
		}
	} else {
		ecc->flt_status |= ECC_INTERMITTENT;
	}

	/* enable ECC error traps */
	set_error_enable(EER_ENABLE);
}

/*
 * get the syndrome and unum, and then call the routines
 * to check the other cpus and iobuses, and then do the error logging.
 */
void
cpu_ce_log_err(struct async_flt *ecc)
{
	char unum[UNUM_NAMLEN];
	int len = 0;
	short syn_code;
	uint32_t synd = SYND(ecc->flt_synd);

	ASSERT(ecc->flt_func != NULL);

	/*
	 * Use the 8-bit syndrome to index the ecc_syndrome_tab to get
	 * the code indicating which bit(s) is(are) bad.
	 */
	if ((synd == 0) || (synd >= SYND_TBL_SIZE)) {
		uint64_t t_afsr = ecc->flt_stat;
		uint64_t t_afar = ecc->flt_addr;

		cmn_err(CE_CONT, "CE Error: AFSR 0x%08x.%08x AFAR 0x%08x.%08x "
		    "%s Bad Syndrome 0x%x Id %d Inst %d\n",
		    (uint32_t)(t_afsr >> 32), (uint32_t)t_afsr,
		    (uint32_t)(t_afar >> 32), (uint32_t)t_afar,
		    UDBL(ecc->flt_synd)?"UDBL":"UDBH", synd,
		    ecc->flt_bus_id, ecc->flt_inst);
		syn_code = -1;
	} else {
		syn_code = ecc_syndrome_tab[synd];
	}

	/* Get the unum string for logging purposes */
	if (ecc->flt_in_memory) {
		if (syn_code == -1) {
			(void) sprintf(unum, "%s", "cannot be decoded");
		} else if (prom_get_unum((int)syn_code, ALIGN_8(ecc->flt_addr),
			unum, UNUM_NAMLEN, &len) != 0) {
			(void) sprintf(unum, "%s", "prom_get_unum() failed");
			len = 0;
		} else if (len <= 1) {
			(void) sprintf(unum, "%s", "Invalid Syndrome");
			len = 0;
		}
	} else {
		(void) sprintf(unum, "%s", "Not Memory");
	}

	/* Call specific error logging routine */
	(void) (*ecc->flt_func)(ecc, unum);

	/*
	 * Count errors per unum.
	 * Non-memory errors are all counted via a special unum string.
	 */
	ce_count_unum(ecc->flt_status, len, unum);

	if (ce_verbose) {
		if ((syn_code >= 0) && (syn_code < 64)) {
			cmn_err(CE_CONT, "\tECC Data Bit %2d was corrected\n",
				syn_code);
		} else if ((syn_code >= 64) && (syn_code < 72)) {
			cmn_err(CE_CONT, "\tECC Check Bit %2d was corrected\n",
			    syn_code - 64);
		} else {
			switch (syn_code) {
			case 72:
				cmn_err(CE_CONT,
					"\tTwo ECC Bits were detected\n");
				break;
			case 73:
				cmn_err(CE_CONT,
					"\tThree ECC Bits were detected\n");
				break;
			case 74:
				cmn_err(CE_CONT,
					"\tFour ECC Bits were detected\n");
				break;
			case 75:
				cmn_err(CE_CONT,
					"\tMore than Four ECC Bits were "
					"detected\n");
				break;
			default:
				cmn_err(CE_CONT, "\tUnknown fault syndrome %d",
				    synd);
				break;
			}
		}
		ce_log_status(ecc->flt_status, unum);
	}

	/* Display entire cache line, if valid address */
	if ((ce_show_data) && (ecc->flt_addr != 0)) {
		read_ecc_data(ecc, 1, 1);
	}
}

/*
 * Access error trap handler for asynchronous cpu errors.  This routine
 * is called to handle a data or instruction access error.  All fatal
 * failures are completely handled by this routine (by panicing).  Since
 * handling non-fatal failures would access data structures which are not
 * consistent at the time of this interrupt, these non-fatal failures are
 * handled later in a soft interrupt at a lower level.
 */
/*ARGSUSED*/
void
cpu_async_error(struct regs *rp, u_long p_afar, u_long p_afsr,
	u_int p_afsr_high, u_int p_afar_high)
{
	u_short sdbh, sdbl;
	u_short e_syndh, e_syndl;
	u_int inst = CPU->cpu_id;
	struct async_flt ecc;
	int priv = 0, mult = 0;

#ifdef __sparcv9
	uint64_t t_afar = p_afar;
	uint64_t t_afsr = p_afsr;
#else
	uint64_t t_afar = (uint64_t)p_afar | ((uint64_t)p_afar_high << 32);
	uint64_t t_afsr = (uint64_t)p_afsr | ((uint64_t)p_afsr_high << 32);
#endif

	/*
	 * Note: the Spitfire data buffer error registers
	 * (upper and lower halves) are or'ed into the upper
	 * word of the afsr by async_err() if P_AFSR_UE is set.
	 */
	sdbh = (u_short)((t_afsr >> 33) & 0x3FF);
	sdbl = (u_short)((t_afsr >> 43) & 0x3FF);

	e_syndh = (u_char)(sdbh & (u_int)P_DER_E_SYND);
	e_syndl = (u_char)(sdbl & (u_int)P_DER_E_SYND);

	t_afsr &= S_AFSR_MASK;
	t_afar &= SABRE_AFAR_PA;	/* must use Sabre AFAR mask */

	if (async_err_panic) {
		if (t_afsr & P_AFSR_UE) {
			/*
			 * make sure we can panic without taking another UE
			 */
			clearphys(ALIGN_64(t_afar),
				cpunodes[CPU->cpu_id].ecache_size);
			(void) clear_ecc(&ecc);
		}
		cmn_err(CE_PANIC, "CPU%d Async Err: AFSR 0x%08x.%08x "
		    "AFAR 0x%08x.%08x UDBH 0x%x UDBL 0x%x",
		    inst, (uint32_t)(t_afsr >> 32), (uint32_t)t_afsr,
		    (uint32_t)(t_afar >> 32), (uint32_t)t_afar,
		    e_syndh, e_syndl);
		/* NOTREACHED */
	}

	/*
	 * Check for fatal async errors.
	 */
	check_misc_err(t_afsr, t_afar);

	/*
	 * handle the specific error
	 */
	bzero(&ecc, sizeof (struct async_flt));
	ecc.flt_stat = t_afsr;
	ecc.flt_addr = t_afar;
	ecc.flt_status = ECC_ID_TRAP;
	ecc.flt_bus_id = getprocessorid();
	ecc.flt_inst = inst;
	ecc.flt_func = (afunc)NULL;
	ecc.flt_proc = ttoproc(curthread);
	ecc.flt_pc = (caddr_t)rp->r_pc;
	ecc.flt_size = 3;	/* 8 byte alignment */

	/*
	 * UE error is currently always fatal even if priv bit is not set.
	 *
	 * Tip from kbn: if ME and 2 sdb syndromes, then 2 different addresses
	 * else if !ME and 2 sdb syndromes, then same address.
	 */
	if (t_afsr & P_AFSR_UE) {
		ecc.flt_func = (afunc)log_ue_err;
		ecc.flt_in_memory = (pf_is_memory(ecc.flt_addr >>
			MMU_PAGESHIFT)) ? 1: 0;

		/*
		 * clear out the UE so we can continue
		 */
		clearphys(ALIGN_64(ecc.flt_addr),
			cpunodes[CPU->cpu_id].ecache_size);
		(void) clear_ecc(&ecc);

		if (((sdbh & P_DER_UE) == 0) && ((sdbl & P_DER_UE) == 0)) {
			cmn_err(CE_PANIC, "CPU%d UE Error Trap, "
			    "UE not set in UDBs: "
			    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x "
			    "UDBH 0x%x UDBL 0x%x", CPU->cpu_id,
			    (uint32_t)(t_afsr >> 32), (uint32_t)t_afsr,
			    (uint32_t)(t_afar >> 32), (uint32_t)t_afar,
			    sdbh, sdbl);
			/* NOTREACHED */
		}

		if ((sdbh >> 9) & 1) {
			ecc.flt_synd = e_syndh;
			ue_error(&ecc);
		}
		if ((sdbl >> 9) & 1) {
			bzero(&ecc, sizeof (struct async_flt));
			ecc.flt_func = (afunc)log_ue_err;
			ecc.flt_in_memory = (pf_is_memory(ecc.flt_addr >>
				MMU_PAGESHIFT)) ? 1: 0;
			ecc.flt_stat = t_afsr;
			ecc.flt_addr = t_afar | 0x8;	/* no UDBL on Sabre */
			ecc.flt_status = ECC_ID_TRAP;
			ecc.flt_bus_id = getprocessorid();
			ecc.flt_inst = inst;
			ecc.flt_proc = ttoproc(curthread);
			ecc.flt_pc = (caddr_t)rp->r_pc;
			ecc.flt_size = 3;	/* 8 byte alignment */
			ecc.flt_synd = e_syndl | UDBL_REG;
			ue_error(&ecc);
		}
	}

	/*
	 * timeout and bus error handling
	 */
	if ((t_afsr & P_AFSR_TO) || (t_afsr & P_AFSR_BERR)) {
		if (curthread->t_lofault) {
			rp->r_g1 = FC_HWERR;
			rp->r_pc = curthread->t_lofault;
			rp->r_npc = curthread->t_lofault + 4;
		} else if (!(curthread->t_nofault)) {
			/*
			 * check for fatal timeout or bus errors
			 */
			if (t_afsr & (P_AFSR_ME|P_AFSR_PRIV)) {
				if (t_afsr & P_AFSR_ME)
					mult = 1;
				if (t_afsr & P_AFSR_PRIV)
					priv = 1;
				cmn_err(CE_PANIC, "CPU%d %s%s%s Error%s: "
				    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x",
				    ecc.flt_inst,
				    (mult) ? "Multiple " : "",
				    (priv) ? "Privileged " : "",
				    (t_afsr & P_AFSR_TO) ? "Timeout" : "Bus",
				    (mult) ? "s" : "",
				    (uint32_t)(t_afsr >> 32), (uint32_t)t_afsr,
				    (uint32_t)(t_afar >> 32), (uint32_t)t_afar);
				/* NOTREACHED */
			}
			ecc.flt_in_memory = 0;
			bto_error(&ecc);
		} else {
			pfn_t pfn = t_afar >> MMU_PAGESHIFT;
			ddi_nofault_data_t *nofault_data_p =
			    curthread->t_nofault;

			if ((nofault_data_p->op_type == PEEK_START) &&
			    (pfn == nofault_data_p->pfn)) {
				nofault_data_p->op_type = PEEK_FAULT;
				rp->r_pc = (uintptr_t)nofault_data_p->pc;
				rp->r_npc = (uintptr_t)nofault_data_p->pc + 4;
			} else {
				/*
				 * At this point we expect a device was trying
				 * a peek, but screwed up.  Print a warning
				 * message and drive on.
				 */
				cmn_err(CE_WARN, "CPU%d System Address "
				    "Bus Error: AFSR 0x%08x.%08x AFAR "
				    "0x%08x.%08x", inst,
				    (uint32_t)(t_afsr >> 32),
				    (uint32_t)t_afsr,
				    (uint32_t)(t_afar >> 32),
				    (uint32_t)t_afar);
			}
		}
	}

	/*
	 * reaching this point means the error was not fatal, so...
	 * flush the ecache,
	 * reset cache control to the bootup state,
	 * and re-enable errors
	 */
	flush_ecache(ecache_flushaddr, cpunodes[CPU->cpu_id].ecache_size * 2);
	set_lsu(get_lsu() | cache_boot_state);
	set_error_enable(EER_ENABLE);
}

static void
check_misc_err(uint64_t p_afsr, uint64_t p_afar)
{
	char *fatal_str = NULL;

	/*
	 * The ISAP and ETP errors are supposed to cause a POR
	 * from the system, so in theory we never, ever see these messages.
	 * IVUE, LDP, WP, and EDP are fatal because we have no address.
	 * So even if we kill the curthread, we can't be sure that we have
	 * killed everyone using tha data, and it could be updated incorrectly
	 * because we have a writeback cache. CP bit indicates a fatal error.
	 */
	if (p_afsr & P_AFSR_ISAP)
		fatal_str = "System Address Parity Error";
	else if (p_afsr & P_AFSR_ETP)
		fatal_str = "Ecache Tag Parity Error";
	else if (p_afsr & P_AFSR_IVUE)
		fatal_str = "Interrupt Vector Uncorrectable Error";
	else if (p_afsr & P_AFSR_LDP)
		fatal_str = "Ecache Load Data Parity Error";
	else if (p_afsr & P_AFSR_WP)
		fatal_str = "Ecache Writeback Data Parity Error";
	else if (p_afsr & P_AFSR_EDP)
		fatal_str = "Ecache SRAM Data Parity Error";
	else if (p_afsr & P_AFSR_CP)
		fatal_str = "Ecache Copyout Data Parity Error";
	if (fatal_str != NULL) {
		cmn_err(CE_PANIC, "CPU%d %s: "
		    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x",
		    CPU->cpu_id, fatal_str,
		    (uint32_t)(p_afsr >> 32), (uint32_t)p_afsr,
		    (uint32_t)(p_afar >> 32), (uint32_t)p_afar);
		/* NOTREACHED */
	}
}

/*
 * Cpu log_func for uncorrectable ecc errors
 */
static int
log_ue_err(struct async_flt *ecc, char *unum)
{
	uint64_t t_afsr = ecc->flt_stat;
	uint64_t t_afar = ecc->flt_addr;
	int priv = 0, mult = 0;

	if (t_afsr & P_AFSR_ME)
		mult = 1;
	if (t_afsr & P_AFSR_PRIV)
		priv = 1;

	cmn_err(CE_WARN, "CPU%d %s%sUE Error%s: "
		"AFSR 0x%08x.%08x AFAR 0x%08x.%08x %s Synd 0x%x MemMod %s",
		ecc->flt_inst, (mult) ? "Multiple " : "",
		(priv) ? "Privileged " : "", (mult) ? "s" : "",
		(uint32_t)(t_afsr >> 32), (uint32_t)t_afsr,
		(uint32_t)(t_afar >> 32), (uint32_t)t_afar,
		UDBL(ecc->flt_synd)?"UDBL":"UDBH", ecc->flt_synd, unum);

	return ((t_afsr & P_AFSR_PRIV) ? UE_FATAL : UE_USER_FATAL);
}

/*
 * Called from the common error handling code for UE event logging
 */
int
cpu_ue_log_err(struct async_flt *ecc, char *unum)
{
	int len = 0;

	ASSERT(ecc->flt_func != NULL);

	/* Get the unum string for logging purposes */
	if (ecc->flt_in_memory) {
		if (prom_get_unum(-1, ALIGN_8(ecc->flt_addr), unum,
		    UNUM_NAMLEN, &len) != 0) {
			(void) sprintf(unum, "%s", "prom_get_unum() failed");
		} else if (len <= 1) {
			(void) sprintf(unum, "%s", "Invalid Syndrome");
		}
	} else {
		(void) sprintf(unum, "%s", "Not Memory");
	}

	/* Check for CPU UE errors that don't cause a trap. */
	ue_check_cpus();

	/* Check for bus-related UE errors that don't cause an interrupt. */
	(void) ue_check_buses();

	/* Call specific error logging routine. */
	return ((*ecc->flt_func)(ecc, unum));
}

/*
 * Check all cpus for non-trapping UE-causing errors
 */
static void
ue_check_cpus(void)
{
	struct async_flt cecc;
	struct async_flt *pcecc = &cecc;
	int pix;

	for (pix = 0; pix < NCPU; pix++) {
		if (CPU_XCALL_READY(pix)) {
			xc_one(pix, (xcfunc_t *)get_cpu_status,
			    (uint64_t)pcecc, 0);
			if (pcecc->flt_stat & P_AFSR_CP) {
				uint64_t t_afar;
				uint64_t t_afsr;

				t_afsr = pcecc->flt_stat;
				t_afar = pcecc->flt_addr;
				cmn_err(CE_PANIC,
				    "CPU%d UE Error: Ecache Copyout on CPU%d: "
				    "AFSR 0x%08x.%08x AFAR 0x%08x.%08x",
				    CPU->cpu_id, pcecc->flt_inst,
				    (uint32_t)(t_afsr >> 32), (uint32_t)t_afsr,
				    (uint32_t)(t_afar >> 32), (uint32_t)t_afar);
				/* NOTREACHED */
			}
		}
	}
}

#ifdef DEBUG
#include <sys/spitregs.h>
int test_mp_cp = 0;
#endif

/*
 */
static u_int
get_cpu_status(uint64_t arg)
{
	struct async_flt *ecc = (struct async_flt *)arg;
	uint64_t afsr;
	uint64_t afar;
	u_int id = getprocessorid();
	u_int inst = CPU->cpu_id;

	/* Get afsr to later check for fatal cp bit.  */
	get_asyncflt(&afsr);
	get_asyncaddr(&afar);
	afar &= SABRE_AFAR_PA;
#ifdef DEBUG
	if (test_mp_cp)
		afsr |= P_AFSR_CP;
#endif
	ecc->flt_stat = afsr;
	ecc->flt_addr = afar;
	ecc->flt_inst = inst;
	ecc->flt_bus_id = id;
	return (0);
}

/*
 * Turn off all cpu error detection, normally only used for panics.
 */
void
cpu_disable_errors(void)
{
	xt_all(set_error_enable_tl1, EER_DISABLE, 0);
}

/*
 * Enable errors.
 */
void
cpu_enable_errors(void)
{
	xt_all(set_error_enable_tl1, EER_ENABLE, 0);
}

void
cpu_read_paddr(struct async_flt *ecc, short verbose, short ce_err)
{
	uint64_t aligned_addr = ALIGN_8(ecc->flt_addr);
	int i, loop = 1;
	u_short ecc_0;
	uint64_t paddr;
	uint64_t data;

	if (verbose)
		loop = 8;
	for (i = 0; i < loop; i++) {
		paddr = aligned_addr + (i * 8);
		data = lddphys(paddr);
		if (verbose) {
			if (ce_err) {
				ecc_0 = ecc_gen((uint32_t)(data>>32),
				(uint32_t)data);
				cmn_err(CE_CONT, "\tPaddr 0x%" PRIx64 ", "
				    "Data 0x%08x.%08x, ECC 0x%x\n", paddr,
				    (uint32_t)(data>>32), (uint32_t)data,
				    ecc_0);
			} else {
				cmn_err(CE_CONT, "\tPaddr 0x%" PRIx64 ", "
				    "Data 0x%08x.%08x\n", paddr,
				    (uint32_t)(data>>32), (uint32_t)data);
			}
		}
	}
}

static struct {		/* sec-ded-s4ed ecc code */
	u_int hi, lo;
} ecc_code[8] = {
	{ 0xee55de23U, 0x16161161U },
	{ 0x55eede93U, 0x61612212U },
	{ 0xbb557b8cU, 0x49494494U },
	{ 0x55bb7b6cU, 0x94948848U },
	{ 0x16161161U, 0xee55de23U },
	{ 0x61612212U, 0x55eede93U },
	{ 0x49494494U, 0xbb557b8cU },
	{ 0x94948848U, 0x55bb7b6cU }
};

static u_short
ecc_gen(u_int high_bytes, u_int low_bytes)
{
	int i, j;
	u_char checker, bit_mask;
	struct {
		u_int hi, lo;
	} hex_data, masked_data[8];

	hex_data.hi = high_bytes;
	hex_data.lo = low_bytes;

	/* mask out bits according to sec-ded-s4ed ecc code */
	for (i = 0; i < 8; i++) {
		masked_data[i].hi = hex_data.hi & ecc_code[i].hi;
		masked_data[i].lo = hex_data.lo & ecc_code[i].lo;
	}

	/*
	 * xor all bits in masked_data[i] to get bit_i of checker,
	 * where i = 0 to 7
	 */
	checker = 0;
	for (i = 0; i < 8; i++) {
		bit_mask = 1 << i;
		for (j = 0; j < 32; j++) {
			if (masked_data[i].lo & 1) checker ^= bit_mask;
			if (masked_data[i].hi & 1) checker ^= bit_mask;
			masked_data[i].hi >>= 1;
			masked_data[i].lo >>= 1;
		}
	}
	return (checker);
}

/*
 * Flush the entire ecache using displacement flush by reading through a
 * physical address range as large as the ecache.
 */
void
cpu_flush_ecache(void)
{
	flush_ecache(ecache_flushaddr, cpunodes[CPU->cpu_id].ecache_size * 2);
}

/*
 * read and display the data in the cache line where the
 * original ce error occurred.
 * This routine is mainly used for debugging new hardware.
 */
void
read_ecc_data(struct async_flt *ecc, short verbose, short ce_err)
{
	kpreempt_disable();
	/* disable ECC error traps */
	set_error_enable(EER_ECC_DISABLE);

	/*
	 * flush the ecache
	 * read the data
	 * check to see if an ECC error occured
	 */
	flush_ecache(ecache_flushaddr, cpunodes[CPU->cpu_id].ecache_size * 2);
	set_lsu(get_lsu() | cache_boot_state);
	cpu_read_paddr(ecc, verbose, ce_err);
	(void) clear_ecc(ecc);

	/* enable ECC error traps */
	set_error_enable(EER_ENABLE);
	kpreempt_enable();
}

/*
 * Clear any ecc error bits and check for persistence.
 * if ce_debug or ue_debug is set, report if any ecc errors have been detected.
 */
static int
clear_ecc(struct async_flt *ecc)
{
	uint64_t t_afsr;
	uint64_t t_afar;
	uint64_t udbh;
	uint64_t udbl;
	ushort_t udb;
	int persistent = 0;

	/*
	 * Get any ECC error info
	 */
	get_asyncflt(&t_afsr);
	get_asyncaddr(&t_afar);
	t_afar &= SABRE_AFAR_PA;
	get_udb_errors(&udbh, &udbl);

	if ((t_afsr & P_AFSR_UE) || (t_afsr & P_AFSR_CE)) {
		/*
		 * Clear any ECC errors
		 */
		clr_datapath();
		set_asyncflt(t_afsr);

		/*
		 * determine whether to check UDBH or UDBL for persistence
		 */
		if (ecc->flt_synd & UDBL_REG) {
			udb = (ushort_t)udbl;
			t_afar |= 0x8;
		} else {
			udb = (ushort_t)udbh;
		}

		if (ce_debug || ue_debug) {
			cmn_err(CE_CONT, "\tclear_ecc: AFSR 0x%08x.%08x "
			    "AFAR 0x%08x.%08x UDBH 0x%x UDBL 0x%x\n",
			    (uint32_t)(t_afsr >> 32), (uint32_t)t_afsr,
			    (uint32_t)(t_afar >> 32), (uint32_t)t_afar,
			    (uint32_t)udbh, (uint32_t)udbl);
		}

		/*
		 * if the fault addresses don't match, not persistent
		 */
		if (t_afar != ecc->flt_addr) {
			return (persistent);
		}

		/*
		 * check for UE persistence
		 * since all DIMMs in the bank are identified for a UE,
		 * there's no reason to check the syndrome
		 */
		if ((ecc->flt_stat & P_AFSR_UE) && (t_afsr & P_AFSR_UE)) {
			persistent = 1;
		}

		/*
		 * check for CE persistence
		 */
		if ((ecc->flt_stat & P_AFSR_CE) && (t_afsr & P_AFSR_CE)) {
			if ((udb & P_DER_E_SYND) ==
			    (ecc->flt_synd & P_DER_E_SYND)) {
				persistent = 1;
			}
		}
	}
	return (persistent);
}
