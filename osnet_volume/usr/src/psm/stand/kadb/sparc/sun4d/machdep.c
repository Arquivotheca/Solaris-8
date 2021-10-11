/*
 * Copyright (c) 1991, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machdep.c	1.34	99/03/23 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include "allregs.h"
#include <sys/scb.h>
#include <sys/debug/debug.h>
#include <sys/debug/debugger.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/stack.h>
#include <sys/frame.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/openprom.h>

int scbsyncdone = 0;

int fake_bpt;			/* place for a fake breakpoint at startup */
jmp_buf debugregs;		/* context for debugger */
jmp_buf mainregs;		/* context for debuggee */
jmp_buf_ptr curregs;		/* pointer to saved context for each process */
struct allregs regsave;		/* temp save area--align to double */
struct scb *mon_tbr, *our_tbr;	/* storage for %tbr's */

extern char start[], estack[], etext[], edata[], end[];
extern struct scb *gettbr();
extern int errno;

#ifdef PARTIAL_ALIGN
int partial_align;
#endif
/*
 * Definitions for registers in jmp_buf
 */
#define	JB_PC	0
#define	JB_SP	1

void	kadbscbsync(void);
void	update_tt(trapvec *dst, trapvec *src);
static void mp_init(void);

void
startup()
{
	mon_tbr = gettbr();

	mp_init();

	/*
	 * Fix up old scb.
	 */
	kadbscbsync();
	spl13();		/* we can take nmi's now */
}

void
scbsync()
{
	kadbscbsync();
	scbsyncdone = 1;
}

void
kadbscbsync()
{
	register struct scb *tbr;
	extern trapvec tcode;
	extern int scbstop;

	tbr = gettbr();
	update_tt(&tbr->user_trap[ST_KADB_TRAP], &tcode);
	update_tt(&tbr->user_trap[ST_KADB_BREAKPOINT], &tcode);

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

/*
 * Miscellanous fault error handler
 */
fault(trap, trappc, trapnpc)
	register int trap;
	register int trappc;
	register int trapnpc;
{
	register int ondebug_stack;
	register u_int *pc;
	register u_int realpc;

	ondebug_stack = (getsp() > (int)etext && getsp() < (int)estack);

	/*
	 * Assume that nofault won't be non-NULL if we're not in kadb.
	 * ondebug_stack won't be true early on, and it's possible to
	 * get an innocuous data fault looking for kernel symbols then.
	 */
	if (trap == T_DATA_FAULT && nofault) {
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
		printf("abort jump: trap %x sp %x pc %x npc %x\n",
			trap, getsp(), trappc, trapnpc);
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

static jmp_buf_ptr saved_jb;
static jmp_buf jb;

/*
 * Peekc is so named to avoid a naming conflict
 * with adb which has a variable named peekc
 */
int
Peekc(addr)
	char *addr;
{
	u_char val;

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
	u_char intreg;
	int addr, t;
	int i;
	u_char interreg;
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
 * Setup a new context to run at routine using stack whose
 * top (end) is at sp.	Assumes that the current context
 * is to be initialized for mainregs and new context is
 * to be set up in debugregs.
 */
spawn(sp, routine)
	char *sp;
	func_t routine;
{
	char *fp;
	int res;
	extern void _exit();

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

void
traceback(sp)
	caddr_t sp;
{
	register u_int tospage;
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
	tospage = (u_int)btopr((u_int)sp);
	fp = (struct frame *)sp;
	printf("Begin traceback... sp = %x\n", sp);
	while (btopr((u_int)fp) == tospage) {
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

/*
 * XXX	This is badly broken- see bugid 1075606
 */

/* A utility used for stepping by adb_ptrace() */

int
in_prom(addr)
uint addr;
{
	return ((addr >= (unsigned)SUNMON_START) &&
		(addr <= (unsigned)SUNMON_END));
}

#include <vm/hat_srmmu.h>

int	va_xlate(caddr_t va, pa_t *ppa);
void	sparc_flush(caddr_t va);

/*
 * poketext() and update_tt() use physical addresses to update
 * code in write-protected areas.  Doing it this way means we
 * don't have to mess with mappings, or flush the TLB.  stphys36()
 * uses a swapa, which is synchronous, so the i-cache is updated
 * immediately, but we use "flush" as well to have the instruction
 * prefetch buffer flushed.
 */
poketext(addr, val)
	int *addr;
	int val;
{
	pa_t pa;

	if (!va_xlate((caddr_t)addr, &pa)) {
		errno = EFAULT;
		return (-1);
	}

	(void) stphys36(pa, val);
	sparc_flush((caddr_t)addr);

	errno = 0;
	return (0);
}

/*
 * Update a vector in the trap table.  See comment at poketext().
 */
void
update_tt(trapvec *dst, trapvec *src)
{
	pa_t pa;
	int i, mapped;
	u_int *src_up, *dst_up;

	mapped = va_xlate((caddr_t)dst, &pa);
	if (!mapped)
		prom_panic("update_tt: can't translate virt addr");

	src_up = (u_int *) src;
	dst_up = (u_int *) dst;
	for (i = 0; i < sizeof (trapvec) / sizeof (u_int); i++) {
		(void) stphys36(pa, *src_up);
		sparc_flush((caddr_t)dst_up);
		pa += sizeof (u_int);
		src_up++;
		dst_up++;
	}
}

int
va_xlate(caddr_t va, pa_t *ppa)
{
	pa_t pa;		/* translated physical address */
	u_int entry;		/* page table entry */
	int retval;		/* return value, 1 indicates va is mapped */

	/*
	 * try the most likely candidate first, is it a mapped
	 * page?
	 */
	entry = srmmu_probe_type(va, 0);
	if (PTE_ETYPE(entry) == MMU_ET_PTE) {
		pa = ((pa_t)(entry & 0xFFFFFF00) << 4) | ((u_int)va & 0xFFF);
		retval = 1;
		goto done;
	}

	/*
	 * let's see if it is mapped at all, if not don't waste any more
	 * time
	 */
	entry = srmmu_probe_type(va, 4);
	if (PTE_ETYPE(entry) != MMU_ET_PTE) {
		retval = 0;
		goto done;
	}

	entry = srmmu_probe_type(va, 1);
	if (PTE_ETYPE(entry) == MMU_ET_PTE) {
		pa = ((pa_t)(entry & 0xFFFFC000) << 4) |
		    ((u_int)va & 0x3FFFF);
		retval = 1;
		goto done;
	}

	entry = srmmu_probe_type(va, 2);
	if (PTE_ETYPE(entry) == MMU_ET_PTE) {
		pa = ((pa_t)(entry & 0xFFF00000) << 4) |
		    ((u_int)va & 0xFFFFFF);
		retval = 1;
		goto done;
	}

	entry = srmmu_probe_type(va, 3);
	if (PTE_ETYPE(entry) == MMU_ET_PTE) {
		pa = ((pa_t)(entry & 0xF0000000) << 4) | (u_int)va;
		retval = 1;
		goto done;
	}

	retval = 0;

done:
	if (retval == 1)
		*ppa = pa;

	return (retval);
}

/*
 * support for multiple processors (MP)
 */
extern u_int cur_cpuid;		/* current cpuid */
#define	NBSIZE	80		/* buffer size for getting name property */

typedef struct {
	int node_id;
	int cpu_id;
} auxcpu_t;

#define	MAXCPUS		20
auxcpu_t auxcpu[MAXCPUS];	/* which cpus are present */
int ncpus = 0;			/* number of cpus present */

lock_t	kadblock = 0;		/* lock used by kadb */

static void
drive_cpu_into_prom_idle(int cpu_id)
{
	int nodeid = auxcpu[cpu_id].node_id;

	prom_idlecpu((dnode_t)nodeid);
}

static void
grab_cpu_from_prom_idle(int cpu_id)
{
	int nodeid = auxcpu[cpu_id].node_id;

	prom_resumecpu((dnode_t)nodeid);
}

typedef	int (*child_func_t)(int nodeid, caddr_t client_data);

int
search_children(int nid, child_func_t fcn, caddr_t client_data)
{
	for (nid = (int)prom_childnode((dnode_t)nid); nid;
	    nid = (int)prom_nextnode((dnode_t)nid)) {
		int error = fcn(nid, client_data);
		if (error != 0) {
			return (error);
		};
	}
	return (0);
}

/*
 * Given the nodeid of a "cpu-unit" node, find the module thingie
 * (I don't know what to call it, this is all new to me), and get
 * the cpu-id property.
 */
static int
find_mod(int nodeid, caddr_t client_data)
{
	int cpu_id;
	char namebuf[NBSIZE];

	if (prom_getproplen((dnode_t)nodeid, "name") > NBSIZE) {
		return (0);	/* not a cpu */
	}

	prom_getprop((dnode_t)nodeid, "name", namebuf);

	if (strcmp(namebuf, "TI,TMS390Z55") == 0) {
		/* fall thru */
	} else if (strcmp(namebuf, "TI,TMS390Z50") == 0) {
		/* fall thru */
	} else if (strcmp(namebuf, "modi4v0m") == 0) {
		/* fall thru */
	} else {
		return (0);	/* not a cpu */
	}

	if (prom_getproplen((dnode_t)nodeid, "cpu-id") != sizeof (cpu_id)) {
		printf("kadb: bogus cpu-id property\n");
		return (0);	/* not a cpu */
	}

	prom_getprop((dnode_t)nodeid, "cpu-id", (caddr_t)&cpu_id);

	if (cpu_id >= MAXCPUS) {
		printf("kadb: cpu-id out of range, (%d > %d)\n",
			cpu_id, MAXCPUS - 1);
		return (0);	/* not a cpu */
	}

	if (auxcpu[cpu_id].node_id != 0) {
		printf("kadb: found duplicate cpu-id: %d\n", cpu_id);
		prom_panic("kadb");
	}

	auxcpu[cpu_id].node_id = nodeid;
	ncpus++;
	return (-1);	/* you can stop looking now! */
}

static int
find_cpu_units(int nodeid, caddr_t client_data)
{
	char namebuf[NBSIZE];

	for (nodeid = (int)prom_childnode((dnode_t)nodeid); nodeid;
	    nodeid = (int)prom_nextnode((dnode_t)nodeid)) {

		if (prom_getproplen((dnode_t)nodeid, "name") > NBSIZE)
			prom_panic("kadb: name property too big!");
		prom_getprop((dnode_t)nodeid, "name", namebuf);

		if (strcmp(namebuf, "cpu-unit") == 0) {
			int found = search_children(nodeid, find_mod, 0);
			if (found == 0) {
				printf("kadb: cpu-unit w/o a CPU!\n");
			}
		}
	}

	return (0);	/* keep looking for others! */
}

/*
 * Find out what cpus we have and set up our internal data.
 */
static void
mp_init()
{
	int rootnode = (int)prom_nextnode((dnode_t)0);
	int comma = 0;
	int i;

	for (i = 0; i < MAXCPUS; i++) {
		auxcpu[i].node_id = 0;
		auxcpu[i].cpu_id = i;
	}

	/*
	 * Poke around in the devinfo tree to find the cpuid
	 * of each cpu.  This code assumes that there are "cpu-unit"
	 * children of the root node, each with a "TI,TMS390Z55" child
	 * that has a property called "cpu-id".
	 */
	find_cpu_units(rootnode, 0);

	/*
	 * Print which cpus are present; mostly a sanity check.
	 */
	if (ncpus > 1) {
		printf("MP mode, CPUs: ");
		for (i = 0; i < MAXCPUS; i++) {
			if (auxcpu[i].node_id != 0) {
				printf((comma == 0) ? "%d" : ", %d", i);
				comma++;
			}
		}
		printf("\n");
	}
}

void
idle_other_cpus()
{
	int i;

	if (ncpus == 1)
		return;

	for (i = 0; i < MAXCPUS; i++) {
		if (i == cur_cpuid)
			continue;

		if (auxcpu[i].node_id != 0)
			drive_cpu_into_prom_idle(i);
	}
}

void
resume_other_cpus()
{
	int i;

	if (ncpus == 1)
		return;

	for (i = 0; i < MAXCPUS; i++) {
		if (i == cur_cpuid)
			continue;

		if (auxcpu[i].node_id != 0)
			grab_cpu_from_prom_idle(i);
	}
}

mach_fiximp()
{
	extern int icache_flush;

	icache_flush = 0;
}

void
setup_aux(void)
{
}

/* a stub function called from common code */
struct bkpt *
tookwp()
{
	return (NULL);
}
