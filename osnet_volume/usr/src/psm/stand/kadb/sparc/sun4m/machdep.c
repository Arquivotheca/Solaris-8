/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machdep.c	1.36	99/10/22 SMI"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/enable.h>
#include <sys/scb.h>
#include <sys/privregs.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/clock.h>
#include <sys/intreg.h>
#include <sys/t_lock.h>
#include <sys/eeprom.h>
#include <sys/asm_linkage.h>
#include <sys/frame.h>
#include "allregs.h"
#include <sys/debug/debug.h>
#include <sys/debug/debugger.h>
#include <sys/bootconf.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/openprom.h>

extern int errno;

int istrap = 0;
int scbsyncdone = 0;

/*
 * The next group of variables and routines handle the
 * Open Boot Prom devinfo or property information.
 *
 * These machine-dependent quantities are set from the prom properties.
 * For the time being, set these to "large, safe" values.
 * XXX - does this all want to be packaged in a header file?
 * XXX	Yes.
 */
extern int vac;
extern int vac_size;
extern int vac_linesize;

/*
 * Properties tchotchkes
 */
#define	NEXT			prom_nextnode
#define	CHILD			prom_childnode
#define	GETPROP			prom_getprop
#define	GETPROPLEN		prom_getproplen

extern int getprop();
extern int debug_props;		/* Turn on to enable debugging message */

/*
 * For now hardcoding this stuff in.  Will setup to be taken from the
 * PROM in the future.  Values for Galaxy.
 */
#define	P_COUNTER_ADDR	0xFF1300000	/* Address of first proc register */
#define	P_INTREG_ADDR	0xFF1400000	/* for counters and interrupts */
#define	PROC_PAGES	(MMU_PAGESIZE * 4)	/* Size to map for proc regs */
#define	SYS_OFFSET	0x10000		/* Offset to system registers */

extern uint_t cur_cpuid;		/* current cpuid */

#define	NCPUS	4
uint_t cpu_nodeid[NCPUS];	/* array of nodeids */

lock_t	kadblock = 0;		/* lock used by kadb */

/*
 * Open proms give us romp as a variable
 */
extern union sunromvec *romp;

int fake_bpt;			/* place for a fake breakpoint at startup */
jmp_buf debugregs;		/* context for debugger */
jmp_buf mainregs;		/* context for debuggee */
jmp_buf_ptr curregs;		/* pointer to saved context for each process */
struct allregs regsave; 	/* temp save area--align to double */
struct scb *mon_tbr, *our_tbr;	/* storage for %tbr's */

#ifdef	VAC
int use_vac = 1;		/* variable to patch to use the cache */
#else
#define	use_vac 0
#endif	/* VAC */

int cache_ison = 0;		/* cache is being used */
int module_type = -1;		/* type of module being used */

extern char start[], estack[], etext[], edata[], end[];
extern int _exit();
extern struct scb *gettbr();

#ifdef PARTIAL_ALIGN
int partial_align;
#endif
/*
 * Definitions for registers in jmp_buf
 */
#define	JB_PC	0
#define	JB_SP	1

#define	CALL(func)	(*(int (*)())((int)(func) - (int)start + (int)real))
#define	RELOC(adr)	((adr) - (char *)start + real)
extern getpgmap(), setpgmap();

extern uint_t	getmcr(void);

/*
 * Startup code after relocation.
 */
startup()
{
	register int i;
	register int pg;
	uchar_t intreg;
	register int vaddr, pmeg;
	dnode_t nodeid;
	unsigned who;

	mon_tbr = gettbr();

	for (i = 0; i < NCPUS; i++)
		cpu_nodeid[i] = -1;

	nodeid = prom_nextnode((dnode_t)0); /* root node */
	for (nodeid = prom_childnode(nodeid);
	    (nodeid != OBP_NONODE) && (nodeid != OBP_BADNODE);
	    nodeid = prom_nextnode(nodeid)) {
		if ((prom_getproplen(nodeid, "mid") == sizeof (who)) &&
		    (prom_getprop(nodeid, "mid", (caddr_t)&who) != -1)) {

#if DEBUG
			/* Should be 8 and a */
			printf("mid: %X\n", who);
#endif

			/*
			 * A level 1 mbus module imples that this
			 * is a uni-processor machine and it sends
			 * out "f" as its module id.
			 */
			if (who == 15)
				who = 0; /* level-1 mbus module */
			else
				who &= 3;

			cpu_nodeid[who] = (int)nodeid;
#if DEBUG
			printf("nodeid for CPU: %d -> %X\n", who, nodeid);
#endif
		}
	}

	/*
	 * Fix up old scb.
	 */
	kadbscbsync();
	spl13();		/* we can take nmi's now */

	mmu_flushall();

#ifdef	VAC
	if (vac) {
		if (use_vac) {
			cache_init();
			cache_on();
			cache_ison = 1;
			/* printf("VAC ENABLED\n"); */
		} else {
			vac = 0;
			printf("VAC DISABLED\n");
		}
	}
#endif	/* VAC */
}

scbsync()
{
	kadbscbsync();
	scbsyncdone = 1;
}

kadbscbsync()
{
	register struct scb *tbr;
	register int otbr_pg;
	int level;
	extern trapvec tcode;

	tbr = gettbr();
	otbr_pg = getpgmap(tbr, &level);
	if (level != 3)
		prom_panic("tbr not l3 mapped");
	setpgmap(tbr, (otbr_pg & ~PG_PROT) | PG_KW, level);

	tbr->user_trap[ST_KADB_TRAP] = tcode;
	tbr->user_trap[ST_KADB_BREAKPOINT] = tcode;
	setpgmap(tbr, otbr_pg, level);
	if (scbstop) {
		/*
		 * We're running interactively. Trap into the debugger
		 * so the user can look around before continuing.
		 * We use trap ST_KADB_TRAP: "enter debugger"
		 */
		scbstop = 0;
		asm_trap(ST_KADB_TRAP);
	}
}

/*
 * Sys_trap trap handlers.
 */

/*
 * level15 (memory error) interrupt.
 */
level15()
{
	/*
	 * For now, the memory error regs are not mapped into the debugger,
	 * so we just print a message.
	 */
	printf("memory error\n");
}

int *saved_addr;	/* contains saved virtual address */

/*
 * Miscellanous fault error handler
 */
fault(trap, trappc, trapnpc)
	register int trap;
	register int trappc;
	register int trapnpc;
{
	register int ondebug_stack;
	register uint_t *pc;
	register uint_t realpc;

	ondebug_stack = (getsp() > (int)etext && getsp() < (int)estack);
	if (trap == T_DATA_FAULT && nofault && ondebug_stack) {
		jmp_buf_ptr sav = nofault;

		nofault = NULL;
		_longjmp(sav, 1);
		/*NOTREACHED*/
	}

	traceback((caddr_t)getsp());
	/*
	 * If we are on the debugger stack and
	 * abort_jmp is set, do a longjmp to it.
	 */
	if (abort_jmp && ondebug_stack) {
		printf("abort jump: trap %x sp %x pc %x npc %x addr %x\n",
			trap, getsp(), trappc, trapnpc, saved_addr);
		printf("etext %x estack %x edata %x nofault %x\n",
			etext, estack, edata, nofault);
		_longjmp(abort_jmp, 1);
		/*NOTREACHED*/
	}

	/*
	 * Ok, the user faulted while not in the
	 * debugger. Enter the main cmd loop
	 * so that the user can look around...
	 */
	/*
	 * There is a problem here since we really need to tell cmd()
	 * the current registers.  We would like to call cmd() in locore
	 * but the interface is not really set up to handle this (yet?)
	 */

	printf("fault and calling cmd: trap %x sp %x pc %x npc %x\n",
	    trap, getsp(), trappc, trapnpc);
	cmd();	/* error not resolved, enter debugger */
}
long trap_window[25];

static jmp_buf_ptr saved_jb;
static jmp_buf jb;
extern int debugging;


/*
 * Peekc is so named to avoid a naming conflict
 * with adb which has a variable named peekc
 */
int
Peekc(addr)
	char *addr;
{
	uchar_t val;

	saved_jb = nofault;
	nofault = jb;
	errno = 0;
	if (!_setjmp(jb)) {
		val = *addr;
		/* if we get here, it worked */
		nofault = saved_jb;
		return ((int)val);
	}
	/* a fault occured */
	nofault = saved_jb;
	errno = EFAULT;
	return (-1);
}

short
peek(addr)
	short *addr;
{
	short val;

	saved_jb = nofault;
	nofault = jb;
	errno = 0;
	if (!_setjmp(jb)) {
		val = *addr;
		/* if we get here, it worked */
		nofault = saved_jb;
		return (val);
	}
	/* a fault occured */
	nofault = saved_jb;
	errno = EFAULT;
	return (-1);
}
long
peekl(addr)
	long *addr;
{
	long val;

	saved_jb = nofault;
	nofault = jb;
	errno = 0;

	if (!_setjmp(jb)) {
		val = *addr;
		/* if we get here, it worked */
		nofault = saved_jb;
		return (val);
	}
	/* a fault occured */
	nofault = saved_jb;
	errno = EFAULT;
	return (-1);
}

int
pokec(addr, val)
	char *addr;
	char val;
{

	saved_jb = nofault;
	nofault = jb;
	errno = 0;
	if (!_setjmp(jb)) {
		*addr = val;
		/* if we get here, it worked */
		nofault = saved_jb;
		return (0);
	}
	/* a fault occured */
	nofault = saved_jb;
	errno = EFAULT;
	return (-1);
}

int
pokes(addr, val)
	short *addr;
	short val;
{
	saved_jb = nofault;
	nofault = jb;
	errno = 0;
	if (!_setjmp(jb)) {
		*addr = val;
		/* if we get here, it worked */
		nofault = saved_jb;
		return (0);
	}
	/* a fault occured */
	nofault = saved_jb;
	errno = EFAULT;
	return (-1);
}

int
pokel(addr, val)
	long *addr;
	long val;
{

	saved_jb = nofault;
	nofault = jb;
	errno = 0;
	if (!_setjmp(jb)) {
		*addr = val;
		/* if we get here, it worked */
		nofault = saved_jb;
		return (0);
	}
	/* a fault occured */
	nofault = saved_jb;
	errno = EFAULT;
	return (-1);
}

poketext(addr, val)
	int *addr;
	int val;
{
	int pg = 0;
	int sp;
	int level;

	sp = getsp();
	saved_addr = addr;

	pg = getpgmap((int)addr, &level);
	if (PTE_ETYPE(pg) != MMU_ET_PTE) {
		if (debugging > 2)
			printf("poketext: invalid page map %X at %X\n",
			    pg, addr);
		goto err;
	}
#ifndef sun4m
	/* XXX - is this necessary? (I guess so to help catch faults) */
	if ((pg & PGT_MASK) != PGT_OBMEM) {
		if (debugging > 2)
			printf("poketext: incorrect page type %X at %X\n",
			    pg, addr);
		goto err;
	}
#endif

	vac_pageflush(addr);
	if (btop((uint_t)(addr + sizeof (int) - 1)) != btop((uint_t)addr))
		vac_pageflush(addr + sizeof (int) - 1);

	if ((pg & PG_PROT) == PG_KR)
		setpgmap(addr, (pg & ~PG_PROT) | PG_KW, level);
	else if ((pg & PG_PROT) == PG_URKR)
		setpgmap(addr, (pg & ~PG_PROT) | PG_UW, level);
	/* otherwise it is already writeable */

#ifdef DEBUG
	/* XXX - Debugging info */
	if (addr == 0) {
		printf("poketext: addr: %X val: %X sp: %X\n", addr, val, sp);
		printf("poketext: saved_addr: %X\n", saved_addr);
		_exit();
	}
#endif

	*addr = val;		/* should be prepared to catch a fault here? */
	iflush(addr);		/* flush i-cache */

	/*
	 * Reset to page map to previous entry,
	 * but mark as modified
	 */
	vac_pageflush(addr);
	if (btop((uint_t)(addr + sizeof (int) - 1)) != btop((uint_t)addr))
		vac_pageflush(addr + sizeof (int) - 1);

	/* XXX - why not referenced also? */
	setpgmap(addr, pg | PTE_MOD(1), level);
	mmu_flushall();
	errno = 0;
	return (0);

err:
	errno = EFAULT;
	return (-1);
}

scopy(from, to, count)
	register char *from;
	register char *to;
	register int count;
{
	register int val;

	for (; count > 0; count--) {
		if ((val = Peekc(from++)) == -1)
			goto err;
		if (pokec(to++, val) == -1)
			goto err;
	}
	return (0);
err:
	errno = EFAULT;
	return (-1);
}

/*
 * Setup a new context to run at routine using stack whose
 * top (end) is at sp.  Assumes that the current context
 * is to be initialized for mainregs and new context is
 * to be set up in debugregs.
 */
spawn(sp, routine)
	char *sp;
	func_t routine;
{
	char *fp;
	int res;

	if (curregs != 0) {
		printf("bad call to spawn\n");
		_exit(1);
	}
	if ((res = _setjmp(mainregs)) == 0) {
		/*
		 * Setup top (null) window.
		 */
		sp -= WINDOWSIZE;
		((struct rwindow *)sp)->rw_rtn = 0;
		((struct rwindow *)sp)->rw_fp = 0;
		/*
		 * Setup window for routine with return to _exit.
		 */
		fp = sp;
		sp -= WINDOWSIZE;
		((struct rwindow *)sp)->rw_rtn = (int)_exit - 8;
		((struct rwindow *)sp)->rw_fp = (int)fp;
		/*
		 * Setup new return window with routine return value.
		 */
		fp = sp;
		sp -= WINDOWSIZE;
		((struct rwindow *)sp)->rw_rtn = (int)routine - 8;
		((struct rwindow *)sp)->rw_fp = (int)fp;
		/* copy entire jump buffer to debugregs */
		bcopy((caddr_t)mainregs, (caddr_t)debugregs, sizeof (jmp_buf));
		debugregs[JB_SP] = (int)sp;	/* set sp */
		curregs = debugregs;
		regsave.r_npc = (int)&fake_bpt;
		_longjmp(debugregs, 1);		/* jump to new context */
		/*NOTREACHED*/
	}
}

doswitch()
{
	int res;

	if ((res = _setjmp(curregs)) == 0) {
		/*
		 * Switch curregs to other descriptor
		 */
		if (curregs == mainregs) {
			curregs = debugregs;
		} else /* curregs == debugregs */ {
			curregs = mainregs;
		}
		_longjmp(curregs, 1);
		/*NOTREACHED*/
	}
	/*
	 * else continue on in new context
	 */
}


/*
 * Main interpreter command loop.
 */
cmd()
{
	int resetclk = 0;
	uchar_t intreg;
	int addr, t;
	int i;
	uchar_t interreg;
	int s;

	dorun = 0;
	i = 0;


	/*
	 * See if the sp says that we are already on the debugger stack
	 */
	reg = (struct regs *)&regsave;
	addr = getsp();

	if (addr > (int)etext && addr < (int)estack) {
		printf("Already in debugger!\n");
		return;
	}

	do {

		doswitch();
		if (dorun == 0)
			printf("cmd: nothing to do\n");

	} while (dorun == 0);
	/* we don't need to splx since we are returning to the caller */
	/* and will reset his/her state */

}

/*
 * Call into the monitor (hopefully)
 */
montrap()
{
	our_tbr = gettbr();
#ifdef debug
	printf("montrap: our_tbr = 0x%x, mon_tbr = 0x%x\n", our_tbr, mon_tbr);
#endif
	settbr(mon_tbr);
	(void) prom_enter_mon();
	settbr(our_tbr);
}

void
traceback(sp)
	caddr_t sp;
{
	register uint_t tospage;
	register struct frame *fp;
	static int done = 0;


#ifdef PARTIAL_ALIGN
	if (partial_align? ((int)sp & 0x3): ((int)sp & 0x7)) {
#else
	if ((int)sp & (STACK_ALIGN-1)) {
#endif PARTIAL_ALIGN
		printf("traceback: misaligned sp = %x\n", sp);
		return;
	}
	flush_windows();
	tospage = (uint_t)btopr((uint_t)sp);
	fp = (struct frame *)sp;
	printf("Begin traceback... sp = %x\n", sp);
	while (btopr((uint_t)fp) == tospage) {
		if (fp == fp->fr_savfp) {
			printf("FP loop at %x", fp);
			break;
		}
		printf("Called from %x, fp=%x, args=%x %x %x %x %x %x\n",
		    fp->fr_savpc, fp->fr_savfp,
		    fp->fr_arg[0], fp->fr_arg[1], fp->fr_arg[2],
		    fp->fr_arg[3], fp->fr_arg[4], fp->fr_arg[5]);
#ifdef notdef
		printf("\tl0-l7: %x, %x, %x, %x, %x, %x, %x, %x\n",
		    fp->fr_local[0], fp->fr_local[1],
		    fp->fr_local[2], fp->fr_local[3],
		    fp->fr_local[4], fp->fr_local[5],
		    fp->fr_local[6], fp->fr_local[7]);
#endif
		fp = fp->fr_savfp;
		if (fp == 0)
			break;
	}
	printf("End traceback...\n");
}

our_die_routine(retaddr)
	register caddr_t retaddr;
{
#ifdef NOTYET
	(*romp->op2_chain)(0, 0, retaddr+8);
	/* NOTREACHED */
#endif
}

/*
 * setpgmap:
 *  set the pte that maps virtual address `v' to `pte'.
 *  for success `v' must be mapped by a level-3 pte
 *  XXX - fix this so that we can look at monitors pages.
 */
setpgmap(v, pte, level)
caddr_t v;
uint_t pte;
int level;
{
	union ptpe ptpe;
	uint_t phys;
	int i, indx[4];

	ptpe.ptpe_int = mmu_getctp();
	ptpe.ptpe_int = ldphys(ptpe.ptp.PageTablePointer << MMU_STD_PTPSHIFT);
	indx[1] = MMU_L1_INDEX((uint_t)v);
	indx[2] = MMU_L2_INDEX((uint_t)v);
	indx[3] = MMU_L3_INDEX((uint_t)v);
	for (i = 1; i < level; i++) {
		phys = ptpe.ptp.PageTablePointer << MMU_STD_PTPSHIFT;
		phys += indx[i] * sizeof (struct pte);
		ptpe.ptpe_int = ldphys(phys);
	}
	phys = ptpe.ptp.PageTablePointer << MMU_STD_PTPSHIFT;
	phys += indx[i] * sizeof (struct pte);
	stphys(phys, pte);
	mmu_flushall();
	return (0);
}

/*
 * getpgmap:
 *  return the pte that maps virtual address `v'.
 *  for success `v' must be mapped by a level-3 pte
 */
getpgmap(v, level)
caddr_t v;
int *level;
{
	union ptpe ptpe;
	uint_t phys;
	int i, indx[4];

	ptpe.ptpe_int = mmu_getctp();
	ptpe.ptpe_int = ldphys(ptpe.ptp.PageTablePointer << MMU_STD_PTPSHIFT);
	indx[1] = MMU_L1_INDEX((uint_t)v);
	indx[2] = MMU_L2_INDEX((uint_t)v);
	indx[3] = MMU_L3_INDEX((uint_t)v);
	for (i = 1; i <= 3; i++) {
		phys = ptpe.ptp.PageTablePointer << MMU_STD_PTPSHIFT;
		phys += indx[i] * sizeof (struct pte);
		ptpe.ptpe_int = ldphys(phys);
		switch (ptpe.ptp.EntryType) {
		case MMU_ET_PTE:
			*level = i;
			return (ptpe.ptpe_int);
		case MMU_ET_PTP:
			break;
		default:
			return (0);
		}
	}
	return (0);
}

/*
 * XXX	This is badly broken- see bugid 1075606
 *
 *	The point is that V2/V3 PROMs are not constrained
 *	to this range of virtual addresses.
 */

/* A utility used for stepping by adb_ptrace() */

int
in_prom(uint_t addr)
{
	return ((addr >= (unsigned)SUNMON_START) &&
		(addr <= (unsigned)SUNMON_END));
}


void
idle_other_cpus()
{
	int i;

	if (cur_cpuid >= NCPUS)
		printf("cur_cpuid: %d is bogus\n", cur_cpuid);

	for (i = 0; i < NCPUS; i++) {
		if (i != cur_cpuid && cpu_nodeid[i] != -1)
			prom_idlecpu((dnode_t)cpu_nodeid[i]);
	}
}

void
resume_other_cpus()
{
	int i;

	if (cur_cpuid >= NCPUS)
		printf("cur_cpuid: %d is bogus\n", cur_cpuid);

	for (i = 0; i < NCPUS; i++) {
		if (i != cur_cpuid && cpu_nodeid[i] != -1)
			prom_resumecpu((dnode_t)cpu_nodeid[i]);
	}
}

mach_fiximp()
{
	uint_t mcr;
	extern int icache_flush;

	mcr = getmcr();

	/*
	 * Set the value of icache_flush.  This value is
	 * passed to the kernel linker so that it knows
	 * whether or not to iflush when relocating text.
	 * Because of a bug in the Ross605, the iflush
	 * instruction causes an illegal instruction
	 * trap therefore we don't iflush in that case.
	 */
	if (ross_module_identify(mcr))
		icache_flush = 0;
	else
		icache_flush = 1;
}

void
setup_aux(void)
{
	char *p;
	pstack_t *stk;
	dnode_t node;
	dnode_t sp[OBP_STACKDEPTH];
	char name[OBP_MAXDRVNAME];
	static char cpubuf[3 * OBP_MAXDRVNAME];
	extern char *cpulist;
	extern char *strcpy(char *, const char *);
	extern char *strcat(char *, const char *);

	/*
	 * Get properties for cpu module
	 */
	if (prom_is_openprom()) {
		stk = prom_stack_init(sp, sizeof (sp));
		node = prom_findnode_bydevtype(prom_rootnode(), "cpu", stk);
		if (node != OBP_NONODE && node != OBP_BADNODE) {
			if (prom_getprop(node, OBP_NAME, name) > 0) {
				strcpy(cpubuf, name);
				strcat(cpubuf, ":");
			}
			if (prom_getprop(node, OBP_COMPATIBLE, name) > 0) {
				strcpy(cpubuf, name);
				strcat(cpubuf, ":");
			}
		}
		prom_stack_fini(stk);
	}

	/* Legacy code can use cpu default module */
	strcat(cpubuf, "default");
	cpulist = cpubuf;
}

/* a stub function called from common code */
struct bkpt *
tookwp()
{
	return (NULL);
}
