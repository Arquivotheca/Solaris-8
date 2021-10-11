/*
 * Copyright (c) 1990-1991, 1993-1994, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)vik.c	1.61	96/11/22 SMI"

#include <sys/types.h>
#include <sys/machparam.h>
#include <sys/module.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <vm/hat_srmmu.h>
#include <sys/async.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/stack.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/privregs.h>

/*
 * Support for modules based on the
 * TI VIKING memory management unit
 *
 * To enable this module, declare this near the top of module_conf.c:
 *
 *	extern void	vik_module_setup();
 *
 * and add this line to the module_info table in module_conf.c:
 *
 *	{ 0xF0000000, 0x00000000, vik_module_setup },
 */

extern void	vik_mmu_getasyncflt();
extern int	vik_mmu_chk_wdreset();
extern void	vik_cache_init();
extern int	mxcc_pac_parity_chk_dis();
extern void	vik_turn_cache_on();
extern void	vik_mmu_log_module_err();
extern void	mxcc_mmu_log_module_err();
extern int	vik_mmu_writepte();
extern int	vik_mp_mmu_writepte();
extern void	vik_mmu_writeptp();
extern int	vik_mmu_probe();
extern void	vik_window_overflow();
extern void	vik_window_underflow();
extern void	vik_pac_pageflush();
extern void	vik_mxcc_pageflush();
extern void	srmmu_tlbflush(int, caddr_t, u_int, u_int);

extern int vac;
extern int cache;
extern int use_cache;
extern int use_ec;
extern int use_mxcc_prefetch;
extern int use_multiple_cmds;
extern int use_mp;
extern int use_rdref_only;
extern int use_ic;
extern int use_dc;
extern int use_ec;
extern int use_store_buffer;
extern int use_table_walk;
extern int use_vik_prefetch;
extern int use_mix;
extern int do_pg_coloring;
extern int use_mix;
extern int use_page_coloring;

#define	MAX_NCTXS	(1 << 16)

#define	VIK_REV_1DOT2	1
#define	VIK_REV_2DOTX	2
#define	VIK_REV_3DOT0	3
#define	VIK_REV_3DOT5	4
#define	VOY_REV_1DOT0	8

extern int viking;
extern int mxcc;
u_int viking_setbits, viking_clrbits;
u_int mxcc_setbits, mxcc_clrbits;
u_int bpt_setbits, bpt_clrbits;
int vik_rev_level = 0;
extern int mxcc_cachesize;
extern int mxcc_linesize;
extern int mxcc_tagblockmask;

caddr_t vb63_fix_addr(struct regs *rp);
extern int viking_ncload_bug;
extern int viking_mfar_bug;
int viking_ptp2_bug;
extern int enable_mbit_wa;

static int vik_get_hwcap_flags(int);

static int getkureg(u_int *, u_int *, u_int, u_int *, int);
static int get_vik_rev_level(void);
extern void vik_mmu_flushrgn(caddr_t addr, u_int cxn);

static void vik_uncache_pt_page(caddr_t addr, u_int pfn);

int
vik_module_identify(u_int mcr)
{
	u_int psr = getpsr();

	/* 1.2 or 3.X */
	if (((psr >> 24) & 0xff) == 0x40)
		return (1);

	/* 2.X */
	if (((psr >> 24) & 0xff) == 0x41 &&
	    ((mcr >> 24) & 0xff) == 0x00)
		return (1);

	return (0);
}


/*
 * Variables to enable workaround for Viking bit flip
 * problem. enable_sm_wa must be set to 0 in /etc/system to disable the
 * workaround.  The code now checks for a Viking older than 3.5 as well
 * and will only enable the workaround on post 2.x/pre-3.5 processors.
 * Setting require_sm_wa to 1 overrides that check "just in case".
 */
int	enable_sm_wa = 1;
int	require_sm_wa = 0;

void
vik_cache_init()
{
	int revision = get_vik_rev_level();
	extern void vik_mxcc_init_asm(u_int, u_int);
	extern void vik_pac_init_asm(u_int, u_int);
	extern void vik_1137125_wa(void);

	/*
	 * Check revision level, and make vik_rev_level the lowest
	 * common denominator.  If we're a 1.2, also enable the
	 * ropte bug workaround.
	 */
	if (revision < vik_rev_level) {
		vik_rev_level = revision;
		if (revision == VIK_REV_1DOT2)
			cmn_err(CE_PANIC, "REV 1.2 not supported");
		if (revision <= VIK_REV_2DOTX) {
			viking_mfar_bug = 1;
			if (!mxcc && use_mix)
				viking_ncload_bug = 1;
		}
		if (revision < VOY_REV_1DOT0) {
			/* see vik_module_setup for more details */
			viking_ptp2_bug = 1;
		}
	}

	if (mxcc)
		vik_mxcc_init_asm(mxcc_clrbits, mxcc_setbits);
	vik_pac_init_asm(viking_clrbits, viking_setbits);

	if (((revision >= VIK_REV_3DOT0) && (revision < VIK_REV_3DOT5) &&
	    enable_sm_wa) || require_sm_wa)
		vik_1137125_wa();

	bpt_reg(bpt_setbits, bpt_clrbits);
}

void
vik_cache_on()
{
	/* caches were turned on in vik_pac_init() */
}

/* ARGSUSED */
static int
vik_mmu_ltic(svaddr, evaddr)
	register char  *svaddr;
	register char  *evaddr;
{
	return (-1);
}

/* ARGSUSED */
void
vik_module_setup(mcr)
	int	mcr;
{
	extern char *cache_mode;
	extern void srmmu_noop();

	viking = 1;
	cache |= (CACHE_PAC | CACHE_PTAG | CACHE_WRITEBACK | CACHE_IOCOHERENT);
	if (mcr & CPU_VIK_MB) {
		v_mmu_log_module_err = vik_mmu_log_module_err;
	} else {
		mxcc = 1;
		v_mmu_log_module_err = mxcc_mmu_log_module_err;
		v_pac_parity_chk_dis = mxcc_pac_parity_chk_dis;
	}
	v_mmu_probe = vik_mmu_probe;
	v_mmu_writepte = vik_mmu_writepte;
	v_mp_mmu_writepte = vik_mp_mmu_writepte;
	v_mmu_writeptp = vik_mmu_writeptp;
	v_mmu_getasyncflt = vik_mmu_getasyncflt;
	v_mmu_chk_wdreset = vik_mmu_chk_wdreset;
	v_mmu_ltic = vik_mmu_ltic;
	v_cache_init = vik_cache_init;
	v_turn_cache_on = vik_cache_on;

	v_window_overflow = vik_window_overflow;
	v_window_underflow = vik_window_underflow;
	v_mp_window_overflow = vik_window_overflow;
	v_mp_window_underflow = vik_window_underflow;

	/*
	 * Indicate if we want page coloring.
	 * Setup the cache initialization bits.
	 */
	if (mxcc) {
		if (use_page_coloring)
			do_pg_coloring = PG_COLORING_ON;

		if (use_cache && use_ec)
			mxcc_setbits |= MXCC_CE;
		if (use_rdref_only)
			mxcc_setbits |= MXCC_RC;
		else
			mxcc_clrbits |= MXCC_RC;
		cache_mode = "SuperSPARC/SuperCache";

		mxcc_tagblockmask = (mxcc_cachesize - 1) & ~(mxcc_linesize - 1);
		v_pac_pageflush = vik_mxcc_pageflush;
		v_uncache_pt_page = srmmu_noop;
	} else {
		cache_mode = "SuperSPARC";
		v_pac_pageflush = vik_pac_pageflush;
		v_uncache_pt_page = vik_uncache_pt_page;
	}

	viking_setbits = CPU_VIK_SE;
	if (use_cache) {
		if (use_ic)
			viking_setbits |= CPU_VIK_IE;
		if (use_dc)
			viking_setbits |= CPU_VIK_DE;
	}
	if (use_vik_prefetch)
		viking_setbits |= CPU_VIK_PF;
	else
		viking_clrbits |= CPU_VIK_PF;
	if (use_table_walk && use_ec && mxcc)
		viking_setbits |= CPU_VIK_TC;
	else
		viking_clrbits |= CPU_VIK_TC;
	if (use_store_buffer)
		viking_setbits |= CPU_VIK_SB;
	else
		viking_clrbits |= CPU_VIK_SB;

	if (use_mix)
		bpt_setbits = MBAR_MIX;
	else
		bpt_clrbits = MBAR_MIX;

	/*
	 * Use the maximum number of contexts available for Viking.
	 */
	nctxs = MAX_NCTXS;

	/*
	 * Check the viking version, and enable any workarounds.
	 */
	vik_rev_level = get_vik_rev_level();
	if (vik_rev_level == VIK_REV_1DOT2) {
		cmn_err(CE_PANIC, "Module Rev 1.2 is not supported");
	}
	if (vik_rev_level <= VIK_REV_2DOTX)
		viking_mfar_bug = 1;
	if (vik_rev_level <= VIK_REV_2DOTX && !mxcc && use_mix) {
		viking_ncload_bug = 1;
		msi_sync_mode();
	}

	/*
	 * Replace srmmu_mmu_flushrgn with a layer of indirection
	 * that checks whether or not the ptp2 workaround is needed.
	 */
	v_mmu_flushrgn = vik_mmu_flushrgn;
	if (vik_rev_level < VOY_REV_1DOT0) {
		/*
		 * SuperSPARC PTP2 bug:
		 * PTP2 is the second level page table pointer that is
		 * cached by Viking.  It is used to access tables of 4K
		 * PTEs.  A DEMAP REGION is used to flush the TLB of all
		 * entries matching VA 31:24, and can come from outside
		 * in systems that support demaps over the bus, or can be
		 * an internal TLB FLUSH instruction.
		 *
		 * TLB entries are all flushed correctly, but the PTP2 is
		 * not always invalidated.  PTP2 is only invalidated if
		 * VA 31:18 match, which is a stronger condition than
		 * REGION DEMAP, that being VA 31:24 match.
		 *
		 * It is possible that an OS remapping memory could issue
		 * a REGION flush, but the old PTP2 could later be used
		 * to fetch a PTE from the old page table.
		 *
		 * CONTEXT, SEGMENT, and PAGE demaps correctly invalidate
		 * PTP2.
		 */
		viking_ptp2_bug = 1;

		/*
		 * Bug 1220902 Viking mbit flip bug:
		 *
		 * This bug occurs in all SuperSparc I and SuperSparc +
		 * CPU modules and the workaround is therefore enabled
		 * for these processors only. The bug doesn't happen in
		 * SuperSparc II (aka Voyager) CPUs.  The bug causes the
		 * TLB modified bit to get set incorrectly under some
		 * conditions.
		 */
		enable_mbit_wa = 1;
	}


	{
		extern void (*level13_fasttrap_handler)(void);
		extern void vik_xcall_medpri(void);

		level13_fasttrap_handler = vik_xcall_medpri;
	}

	v_get_hwcap_flags = vik_get_hwcap_flags;
	isa_list = "sparcv8 sparcv8-fsmuld sparcv7 sparc";
}

char    *mod_err_type[] = {
	"[err=0] ",
	"Uncorrectable Error ",
	"Timeout Error ",
	"Bus Error ",
	"Undefined Error ",
	"[err=5] ",
	"[err=6] ",
	"[err=7] ",
};


void
vik_mmu_log_module_err(afsr, afar0)
	u_int afsr;	/* MFSR */
	u_int afar0;	/* MFAR */
{
	if (afsr & MMU_SFSR_SB)
		cmn_err(CE_CONT, "?Store Buffer Error");
	cmn_err(CE_CONT, "?mfsr 0x%x\n", afsr);
	if (afsr & MMU_SFSR_FAV)
		cmn_err(CE_CONT, "?\tFault Virtual Address = 0x%x\n", afar0);
}

void
mxcc_mmu_log_module_err(afsr, afar0, aerr0, aerr1)
	u_int afsr;	/* MFSR */
	u_int afar0;	/* MFAR */
	u_int aerr0;	/* MXCC error register <63:32> */
	u_int aerr1;	/* MXCC error register <31:0> */
{
	void vik_mmu_print_sfsr();

	(void) vik_mmu_print_sfsr(afsr);
	if (afsr & MMU_SFSR_FAV)
		cmn_err(CE_CONT, "?\tFault Virtual Address = 0x%x\n", afar0);

	cmn_err(CE_CONT, "?MXCC Error Register:\n");
	if (aerr0 & MXCC_ERR_ME)
		cmn_err(CE_CONT, "?\tMultiple Errors\n");
	if (aerr0 & MXCC_ERR_AE)
		cmn_err(CE_CONT, "?\tAsynchronous Error\n");
	if (aerr0 & MXCC_ERR_CC)
		cmn_err(CE_CONT, "?\tCache Consistency Error\n");
	/*
	 * XXXXXX: deal with pairty error later, just log it now
	 *	   ignore ERROR<7:0> on parity error
	 */
	if (aerr0 & MXCC_ERR_CP)
		cmn_err(CE_CONT, "?\tE$ Parity Error\n");

	if (aerr0 & MXCC_ERR_EV) {

#ifdef notdef
	cmn_err(CE_CONT, "?\tRequested transaction: %s%s at %x:%x\n",
		aerr0&MXCC_ERR_S ? "supv " : "user ",
		ccop_trans_type[(aerr0&MXCC_ERR_CCOP) >> MXCC_ERR_CCOP_SHFT],
			aerr0 & MXCC_ERR_PA, aerr1);
#endif notdef
		cmn_err(CE_CONT,
			"?\tRequested transaction: %s CCOP %x at %x:%x\n",
			aerr0&MXCC_ERR_S ? "supv " : "user ",
			aerr0&MXCC_ERR_CCOP, aerr0 & MXCC_ERR_PA, aerr1);
		cmn_err(CE_CONT, "?\tError type: %s\n",
		    mod_err_type[(aerr0 & MXCC_ERR_ERR) >> MXCC_ERR_ERR_SHFT]);
	}
}

void
vik_mmu_print_sfsr(sfsr)
u_int	sfsr;
{
	cmn_err(CE_CONT, "?MMU sfsr=%x:", sfsr);
	switch (sfsr & MMU_SFSR_FT_MASK) {
	case MMU_SFSR_FT_NO: cmn_err(CE_CONT, "? No Error"); break;
	case MMU_SFSR_FT_INV: cmn_err(CE_CONT, "? Invalid Address"); break;
	case MMU_SFSR_FT_PROT: cmn_err(CE_CONT, "? Protection Error"); break;
	case MMU_SFSR_FT_PRIV: cmn_err(CE_CONT, "? Privilege Violation"); break;
	case MMU_SFSR_FT_TRAN: cmn_err(CE_CONT, "? Translation Error"); break;
	case MMU_SFSR_FT_BUS: cmn_err(CE_CONT, "? Bus Access Error"); break;
	case MMU_SFSR_FT_INT: cmn_err(CE_CONT, "? Internal Error"); break;
	case MMU_SFSR_FT_RESV: cmn_err(CE_CONT, "? Reserved Error"); break;
	default: cmn_err(CE_CONT, "? Unknown Error"); break;
	}
	if (sfsr) {
		cmn_err(CE_CONT, "? on %s %s %s at level %d",
			    sfsr & MMU_SFSR_AT_SUPV ? "supv" : "user",
			    sfsr & MMU_SFSR_AT_INSTR ? "instr" : "data",
			    sfsr & MMU_SFSR_AT_STORE ? "store" : "fetch",
			    (sfsr & MMU_SFSR_LEVEL) >> MMU_SFSR_LEVEL_SHIFT);
		if (sfsr & MMU_SFSR_BE)
			cmn_err(CE_CONT, "?\n\tM-Bus Bus Error");
		if (sfsr & MMU_SFSR_TO)
			cmn_err(CE_CONT, "?\n\tM-Bus Timeout Error");
		if (sfsr & MMU_SFSR_UC)
			cmn_err(CE_CONT, "?\n\tM-Bus Uncorrectable Error");
		if (sfsr & MMU_SFSR_UD)
			cmn_err(CE_CONT, "?\n\tM-Bus Undefined Error");
		if (sfsr & MMU_SFSR_P)
			cmn_err(CE_CONT, "?\n\tParity Error");
		if (sfsr & MMU_SFSR_CS)
			cmn_err(CE_CONT, "?\n\tControl Space Sccess Error");
		if (sfsr & MMU_SFSR_SB)
			cmn_err(CE_CONT, "?\n\tStore Buffer Error");
	}
	cmn_err(CE_CONT, "?\n");
}


/*
 * Fix some chip bugs that happen on faults.
 */
void
vik_fixfault(rp, addrp, fsr)
	struct regs *rp;
	caddr_t *addrp;
	u_int fsr;
{
	u_int fault_type;

	/*
	 * Viking sometimes gets confused and latches the wrong mfar (fault
	 * address) register under certain circumstances.  There are four
	 * known cases.
	 *
	 *	mfar1 and mfar3 occur with the following code sequence:
	 *
	 *	{call}{ba}{jmpl}	dest
	 *	{st}{ld}{swap}{ldstub}	addr
	 *
	 *	In mfar1 this sequence is near the end of a page and occurs
	 *	in conjuction with the prefetcher doing a table-walk on the
	 *	next page.
	 *	In mfar3 this sequence is preceded by a st instruction that
	 *	also does a table-walk.
	 *	mfar2 only occurs on system with demaps, so 4m is not affected.
	 *
	 *	mfar4 occurs with:
	 *
	 *	{st}{ld}{swap}{ldstub}	addr1
	 *	{st}{ld}{swap}{ldstub}	addr
	 *	{call}{ba}{jmp}		dest
	 *
	 *	at the end of a page, with a gap between the delivery of the
	 *	first instruction and the other 2 instructions.
	 *
	 * In all these cases, the MFAR (fault address register) could
	 * latch the address of dest instead of addr if the "ld/st addr"
	 * gets a 1 cycle tlb fault such as prot or priv errors.  This could
	 * fool the kernel into thinking that the user was trying to write
	 * his own text and cause a core dump.
	 *
	 * Since the 4m cases all occur with MFAR equal to NPC or at the end
	 * of a page, we recompute the address when this happens on
	 * protection and privilege violations.
	 *
	 * See bugids 1096030 and 1117508 for more details.
	 */
	fault_type = X_FAULT_TYPE(fsr);
	if ((rp->r_npc == (int)*addrp || (rp->r_pc & 0xFE0) == 0xFE0) &&
	    (fault_type == FT_PROT_ERROR || fault_type == FT_PRIV_ERROR)) {
		caddr_t tmp = vb63_fix_addr(rp);

		if (tmp != (caddr_t)-1)
			*addrp = tmp;
	}
}

/*
 * Disassemble instruction at r_pc and return the effective address if
 * it's a memory access instruction, or -1 if it isn't.  Used to correct
 * MFAR when viking hits bug 63.
 */
caddr_t
vb63_fix_addr(struct regs *rp)
{
	u_int inst;
	u_int rs1, rs2;
	register u_int *rgs;		/* pointer to struct regs */
	register u_int *rw;		/* pointer to frame */
	u_int addr;
	int kpc;

	kpc = (u_int)rp->r_pc > KERNELBASE;
	if (kpc) {
		inst = *(u_int *)rp->r_pc;
		flush_windows();
	} else {
		if (fuiword32((void *)rp->r_pc, &inst) == -1)
			return ((caddr_t)-1);
		(void) flush_user_windows_to_stack(NULL);
	}

	/*
	 * Implement workaround only for memory operations (format == 3),
	 * since viking doesn't support coprocessor instructions.
	 */
	if (((inst >> 30) != 0x3) || (((inst >> 23) & 0x3) == 3))
		return ((caddr_t)-1);

	rgs = (u_int *)&rp->r_y;	/* globals and outs */
	rw = (u_int *)rp->r_sp;		/* ins and locals */

	/* generate first operand rs1 */
	if (getkureg(rgs, rw, (inst >> 14) & 0x1f, &rs1, kpc) != 0)
		return ((caddr_t)-1);
	/* check immediate bit and use immediate field or reg (rs2) */
	if ((inst >> 13) & 1) {
		register int imm;
		imm = inst & 0x1fff;	/* mask out immediate field */
		imm <<= 19;		/* sign extend it */
		imm >>= 19;
		addr = rs1 + imm;
	} else {
		if (getkureg(rgs, rw, inst & 0x1f, &rs2, kpc) != 0)
			return ((caddr_t)-1);
		addr = rs1 + rs2;
	}
	return ((caddr_t)addr);
}

/*
 * Register read support for vb63_fix_addr().
 */
static int
getkureg(u_int *rgs, u_int *rw, u_int reg, u_int *val, int kernelregs)
{
	if (reg == 0)
		*val = 0;
	else if (reg < 16)
		*val = rgs[reg];
	else {
		if (kernelregs)
			*val = rw[reg - 16];
		else if (fuword32(&rw[reg - 16], val) == -1)
			return (-1);
	}
	return (0);
}

/*
 * get viking revision level
 *
 * implements the following table:
 *
 *	Viking Rev	PSR version	MCR version	JTAG
 *	----------	-----------	-----------	----
 *
 *	1.2		0x40		0x00		0
 *	2.x		0x41		0x00		0
 *	3.0		0x40		0x01		>0
 *	3.5		0x40		0x04
 *	4.x		0x40		0x02
 *	5.x		0x40		0x03
 */
static int
get_vik_rev_level()
{
	int psr_version, mcr_version, version;

	psr_version = (getpsr() >> 24) & 0xf;
	mcr_version = (mmu_getcr() >> 24) & 0xf;

	if (psr_version)
		version = VIK_REV_2DOTX;
	else if (mcr_version >= 8)
		version = VOY_REV_1DOT0;
	else if (mcr_version > 1)
		version = VIK_REV_3DOT5;
	else if (mcr_version)
		version = VIK_REV_3DOT0;
	else
		version = VIK_REV_1DOT2;
	return (version);
}

/*
 * The UP version of the viking writepte function.
 *
 * This routine must workaround the following bug which occurs on mbus
 * mode vikings.
 *
 *  1.  Viking is executing a 2-instruction group and one of the
 *	instructions is a ld from non-cacheable memory.
 *
 *  2.  An interrupt arrives which causes the 2-instruction group
 *	to be broken and holds the pipe.
 *
 *  3.  Between 1-5 cycles after Viking receives the data from the
 *	non-cached load, an IO device issues a read request to main
 *	memory which hits in the Viking data cache. This forces Viking
 *	to supply the data to the IO and this is where the trouble
 *	starts.
 *
 *  4.  The Viking pipeline is first held because of the interrupt and
 *	because the non-cached load is in progress. The pipe should
 *	be allowed to advance when the data is returned but it can't
 *	because of the IO device read which hits in the Viking data
 *	cache. This confuses Viking and results in two errors:
 *
 *	- Viking supplies the non-cached data as the first
 *		doubleword of the IO data. This is wrong.
 *
 *	- Viking supplies the wrong half of the MBus doubleword
 *		as the non-cached data.
 *
 * The workaround employed here is to use mmu_readpte() to ld the ptes.
 * This function has been specially coded in assembly to force the
 * ld instruction into its own instruction group.
 */
vik_mmu_writepte(pte, value, addr, level, cxn, rmkeep)
	struct pte *pte;
	u_int value;
	caddr_t addr;
	int level;
	int cxn;
	u_int rmkeep;
{
	u_int old;

	ASSERT((rmkeep & ~PTE_RM_MASK) == 0);
	mmu_readpte(pte, (struct pte *)&old);
	value |= old & rmkeep;
	(void) swapl(value, (int *)pte);
	if (PTE_ETYPE(old) == MMU_ET_PTE && cxn != -1)
		srmmu_tlbflush(level, addr, cxn, FL_ALLCPUS);
	return (old & PTE_RM_MASK);
}

/*
 * The MP version of the viking writepte function.
 *
 * In order to avoid race conditions with other processors that might
 * be accessing the same pte, we first invalidate the pte in memory
 * before writing the new one.  An assumption here is that there will
 * be no problems caused if another processor table walks the invalid pte.
 *
 * Another assumption here is that vikings with the non-cached ld bug
 * will never be seen in mbus mode on mp systems.
 */
vik_mp_mmu_writepte(pte, value, addr, level, cxn, rmkeep)
	struct pte *pte;
	u_int value;
	caddr_t addr;
	int level;
	int cxn;
	u_int rmkeep;
{
	u_int *ipte, old;
	int nvalid, local, operms, nperms;
	extern u_int srmmu_perms[];

	ASSERT((rmkeep & ~PTE_RM_MASK) == 0);
	ipte = (u_int *)pte;
	old = *ipte;

	/*
	 * If new value matches old pte entry, we may still
	 * need to flush local TLB, since a previous permission
	 * upgrade done by another cpu did not flush all TLBs.
	 */
	if (old == value || ((((old ^ value) & ~PTE_RM_MASK) == 0) &&
	    rmkeep == PTE_RM_MASK)) {
		if (PTE_ETYPE(old) == MMU_ET_PTE && cxn != -1)
			srmmu_tlbflush(level, addr, cxn, FL_LOCALCPU);
		return (old & PTE_RM_MASK);
	}

	/*
	 * If the old pte is valid and it's in a valid context, we need to
	 * flush some tlbs in order to make it disappear.  If both the
	 * new and the old pte are valid, and the new pte's permissions are
	 * a superset of the old pte's permissions, we only need to flush
	 * the local processor's tlb since other processors will update
	 * their tlbs if they fault on the old pte's entry.  Otherwise,
	 * we must flush every processors's tlb.
	 */
	if (PTE_ETYPE(old) == MMU_ET_PTE && cxn != -1) {
		nvalid = PTE_ETYPE(value) == MMU_ET_PTE;
		operms = srmmu_perms[(old & PTE_PERMMASK) >> PTE_PERMSHIFT];
		nperms = srmmu_perms[(value & PTE_PERMMASK) >> PTE_PERMSHIFT];
		local = nvalid && (operms & nperms) == operms &&
			rmkeep == PTE_RM_MASK;
		do {
			old |= swapl(MMU_STD_INVALIDPTE, (int *)ipte);
			if (local) {
				srmmu_tlbflush(level, addr, cxn,
					FL_LOCALCPU);
			} else {
				XCALL_PROLOG;
				srmmu_tlbflush(level, addr, cxn,
					FL_ALLCPUS);
				XCALL_EPILOG;
			}
		} while (*ipte != MMU_STD_INVALIDPTE);
	}
	value |= old & rmkeep;
	(void) swapl(value, (int *)ipte);
	return (old & PTE_RM_MASK);
}

/*
 * The viking writeptp function.
 */
void
vik_mmu_writeptp(ptp, value, addr, level, cxn)
	struct ptp *ptp;
	u_int value;
	caddr_t addr;
	int level;
	int cxn;
{
	u_int old;

	old = swapl(value, (int *)ptp);
	if (PTE_ETYPE(old) == MMU_ET_PTP && cxn != -1) {
		XCALL_PROLOG;
		srmmu_tlbflush(level, addr, cxn, FL_ALLCPUS);
		XCALL_EPILOG;
	}
}

/* ARGSUSED */
static void
vik_uncache_pt_page(caddr_t addr, u_int pfn)
{
	extern void pac_pageflush();

	pac_pageflush(pfn);
}

/*
 * apply use_mxcc_prefetch and use_multiple_cmds to mxcc_setbits and
 * mxcc_clrbits for benefit of cache_init()'s vik_mxcc_init_asm().
 */
void
mxcc_knobs(void)
{
	if (use_mxcc_prefetch)
		mxcc_setbits |= MXCC_PF;
	else
		mxcc_clrbits |= MXCC_PF;

	if (use_multiple_cmds)
		mxcc_setbits |= MXCC_MC;
	else
		mxcc_clrbits |= MXCC_MC;
}

/*
 * Describe SPARC capabilities (performance hints)
 */
static int
vik_get_hwcap_flags(int inkernel)
{
	return (sparcV8_get_hwcap_flags(inkernel) | AV_SPARC_HWFSMULD);
}
