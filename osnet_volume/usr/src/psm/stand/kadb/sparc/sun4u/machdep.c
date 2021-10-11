/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machdep.c	1.27	99/03/23 SMI"

#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/mmu.h>
#include <sys/scb.h>
#include <sys/privregs.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/t_lock.h>
#include <sys/asm_linkage.h>
#include <sys/frame.h>
#include "adb.h"
#include "allregs.h"
#include <sys/debug/debug.h>
#include <sys/debug/debugger.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/openprom.h>
#include <sys/bootconf.h>
#include <sys/prom_plat.h>
#ifdef	IOC
#include <sys/iocache.h>
#endif /* IOC */

#include <sys/cpu_sgnblk_defs.h>
#include <sys/cpuvar.h>

static void mp_init(void);
static void set_brkpt(caddr_t, int);
void switch_cpu(int);
caddr_t format_prom_callback(void);
void arm_prom_callback(void);
void set_prom_callback(void);
void scbsync(void);
void callb_format(caddr_t *arg);
void callb_arm(void);
void callb_cpu_change(int cpuid, kadb_cpu_attr_t what, int arg);
void reload_prom_callback(void);
void idle_other_cpus(void);
void resume_other_cpus(void);
void do_bkpt(caddr_t);
void xcall(int, func_t, u_int, u_int);
extern void save_cpu_state(u_int, u_int);
extern void restore_cpu_state(u_int, u_int);
extern void start_cmd_loop(u_int, u_int);
extern void kadb_idle_self(int);
extern void kadb_send_mondo(int, func_t, u_int, u_int, u_int, u_int);
extern struct scb *gettba(void);

int wp_mask = 0xff;		/* watchpoint mask */
extern struct bkpt *bkpthead;
extern char estack[], etext[], edata[];
extern struct bootops *bootops;
extern int errno;
int cur_cpuid;
int switched;
int to_cpu;
int clock_freq;
int Cpudelay;
u_int cpu_nodeid[NCPU];
u_int shadow_nodeid[NCPU];
lock_t kadblock = 0;		/* MP lock used by kadb */
int fake_bpt;			/* place for a fake breakpoint at startup */
struct scb *mon_tba;
struct scb *kern_tba;

struct allregs_v9 regsave;
struct allregs_v9 cpusave[NCPU];	/* per-CPU register save area */
v9_fpregset_t fpuregs;
v9_fpregset_t mpfpregs[NCPU];		/* per-CPU FP register save area */

#ifdef PARTIAL_ALIGN
int partial_align;
#endif

#define	JB_PC	0
#define	JB_SP	1
jmp_buf mainregs;		/* context for debuggee */
jmp_buf debugregs;		/* context for debugger */
jmp_buf_ptr curregs;		/* pointer to saved context for each process */
static jmp_buf jb;
static jmp_buf_ptr saved_jb;

int vac_size;
caddr_t bkptbuf;

/*
 * Startup code after relocation.
 */
void
startup()
{
	mon_tba = gettba();	/* save PROM's %tba */
	mp_init();
	set_prom_callback();

	/*
	 * Allocate space for breakpoint buffer; the buffer must be
	 * aligned on a vac-size boundary. The kernel va is then
	 * rounded down to a similar boundary, assuring the temp
	 * va and the address to be breakpointed are modulo cache
	 * size.
	 */
	if ((bkptbuf = prom_allocate_virt(vac_size,
	    vac_size)) == (caddr_t)-1)
		prom_panic("do_bkpt: can't allocate bkptbuf");
}

/*
 * Miscellanous fault/error handler, called from trap(). If
 * we took a trap while in the debugger, we longjmp back to
 * where we were
 */
fault(trap, trappc, trapnpc)
	register int trap;
	register int trappc;
	register int trapnpc;
{
	register int ondebug_stack;

	ondebug_stack = ((int)getsp() > (int)etext && (int)getsp() < (int)estack);

	if ((trap == T_DATA_MMU_MISS || trap == FAST_DMMU_MISS_TT) &&
	    nofault && ondebug_stack) {
		jmp_buf_ptr sav = nofault;

		nofault = NULL;
		_longjmp(sav, 1);
		/*NOTREACHED*/
	}

	traceback((caddr_t)getsp());

	if (abort_jmp && ondebug_stack) {
		printf("abort jump: trap %x sp %x pc %x npc %x\n",
			trap, getsp(), trappc, trapnpc);
		printf("etext %x estack %x edata %x nofault %x\n",
			etext, estack, edata, nofault);
		_longjmp(abort_jmp, 1);
		/*NOTREACHED*/
	}

	/*
	 * Ok, the user faulted while not in the debugger. Enter the
	 * main cmd loop so that the user can look around...
	 *
	 * There is a problem here since we really need to tell cmd()
	 * the current registers.  We would like to call cmd() in locore
	 * but the interface is not really set up to handle this (yet?)
	 */

	printf("fault: calling cmd, trap %x sp %x pc %x npc %x\n",
	    trap, getsp(), trappc, trapnpc);
	cmd();	/* error not resolved, enter debugger */
}

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

poketext(addr, val)
	int *addr;
	int val;
{

	saved_jb = nofault;
	nofault = jb;
	errno = 0;
	if (!_setjmp(jb)) {
		set_brkpt((caddr_t)addr, val);
		/*
		 * If we get here, it worked.
		 */
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

/*
 * Setup a new context to run at routine using stack whose
 * top (end) is at sp.  Assumes that the current context
 * is to be initialized for mainregs and new context is
 * to be set up in debugregs.
 */
void
spawn(sp, routine)
	char *sp;
	func_t routine;
{
	char *fp;
	int res;


	db_printf(4, "spawn: sp=%X routine=%X", sp, routine);

	if (curregs != 0) {
		printf("bad call to spawn\n");
		exit(1, 0);
	}
	if ((res = _setjmp(mainregs)) == 0) {
		/*
		 * Setup top (null) window.
		 */
		sp -= WINDOWSIZE;
		((struct rwindow *)sp)->rw_rtn = 0;
		((struct rwindow *)sp)->rw_fp = 0;
		/*
		 * Setup window for routine with return to exit.
		 */
		fp = sp;
		sp -= WINDOWSIZE;
		((struct rwindow *)sp)->rw_rtn = (int)exit - 8;
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

/*
 * Primitive context switch between debugger and debuggee.
 */
void
doswitch()
{
	int res;

	if ((res = _setjmp(curregs)) == 0) {

		if (curregs == mainregs) {
			curregs = debugregs;
		} else {		/* curregs == debugregs */
			curregs = mainregs;
		}
		_longjmp(curregs, 1);
		/*NOTREACHED*/
	} /* else continue on in new context */

	if ((curregs == mainregs) && switched) {
		switched = 0;
		switch_cpu(to_cpu);
	}
}


/*
 * Main interpreter command loop.
 */
void
cmd()
{
	int addr;

	dorun = 0;

	/*
	 * Make sure we aren't already on the debugger stack; if we are,
	 * we took some sort of fault (e.g. unexpected TLB miss) inside
	 * the debugger itself. Since we can't repair that or go back to
	 * userland, just bail into the PROM.
	 */
	reg = (struct regs *)&regsave;	/* XXX */
	addr = (int)getsp();

	if (addr > (int)etext && addr < (int)estack) {
		printf("cmd: fault inside debugger! pc=%X, tt=%X\n",
		    regsave.r_pc, regsave.r_tt);
		_exit();
	}
	if (in_prom(regsave.r_pc)) {
		printf("kadb: Called from within the PROM, exiting...\n");
		_exit();
	}

	do {
		doswitch();
		if (dorun == 0)
			printf("cmd: nothing to do\n");
	} while (dorun == 0);
}

/*
 * Called from mlsetup() in the kernel to handle kadb -d.
 */
void
scbsync(void)
{
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
 * Allows cpr to access the words which kadb defines to the prom.
 */
void
callb_format(caddr_t *arg)
{
	*arg = format_prom_callback();
}

/*
 * Allows cpr to arm the prom to use the words which kadb defines to the prom.
 */
void
callb_arm(void)
{
	arm_prom_callback();
}

/*
 * Called to notify kadb that a cpu is not accepting cross-traps.
 */
void
callb_cpu_change(int cpuid, kadb_cpu_attr_t what, int arg)
{
	/*
	 * If cpu's nodeid is valid, save and invalidate cpu's nodeid.
	 */

	switch (what) {
	case KADB_CPU_XCALL:
		if (arg == 0) {
			if ((cpuid >= 0) && (cpuid <= NCPU) &&
			    (cpu_nodeid[cpuid] != -1)) {
				shadow_nodeid[cpuid] = cpu_nodeid[cpuid];
				cpu_nodeid[cpuid] = -1;		/* invalidate */
			}
		} else {
			/*
			 * We assume that the (arg == 0) case was called for
			 * this cpuid earlier.  Restore nodeid that was saved.
			 */

			if ((cpuid >= 0) && (cpuid <= NCPU))
				cpu_nodeid[cpuid] = shadow_nodeid[cpuid];
		}
		break;
	default:
		break;
	}
}

/*
 * Call into the PROM monitor.
 */
void
montrap()
{
	struct scb *our_tba;

	our_tba = gettba();
	db_printf(8, "montrap: our_tba = 0x%x, mon_tba = 0x%x\n",
	    our_tba, mon_tba);
	settba(mon_tba);
	(void) prom_enter_mon();
	settba(our_tba);
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
		fp = fp->fr_savfp;
		if (fp == 0)
			break;
	}
	printf("End traceback...\n");
}


/*
 * Check whether the input address is within the PROM's virtual
 * address space.
 */
int
in_prom(caddr_t addr)
{
	return (addr >= (caddr_t)mon_tba);
}

/*
 * Called from the trap handler to idle all other CPUs before we
 * do anything.
 */
void
idle_other_cpus(void)
{
	int i;
	int cpuid = cur_cpuid;

	if (cpuid > NCPU)
		printf("cur_cpuid: %d is bogus\n", cur_cpuid);

	KADB_SGN_UPDATE_OBP();

	for (i = 0; i < NCPU; i++) {
		if (i != cpuid && cpu_nodeid[i] != -1) {
			/*
			 * Deliver a 'mondo' interrupt to the CPU we want to
			 * idle, passing the address of a routine that will
			 * set splhi, save the registers and idle until it
			 * receives another mondo telling it to wake up.
			 */
			db_printf(1, "cpu %d, idling cpu %d\n", cpuid, i);
			kadb_send_mondo(CPUID_TO_UPAID(i), (func_t)&save_cpu_state,
			    (u_int)&cpusave[i], (u_int)&mpfpregs[i],
			    (u_int)0, (u_int)0);
		}
	}
}

void
resume_other_cpus(void)
{
	int i;
	int cpuid = cur_cpuid;

	if (cpuid > NCPU)
		printf("cur_cpuid: %d is bogus\n", cur_cpuid);

	KADB_SGN_UPDATE_OS();

	for (i = 0; i < NCPU; i++) {
		if (i != cpuid && cpu_nodeid[i] != -1) {
			db_printf(1, "cpu %d resuming cpu %d\n", cpuid, i);
			kadb_send_mondo(CPUID_TO_UPAID(i), (func_t)&restore_cpu_state,
			    (u_int)&cpusave[i], (u_int)&mpfpregs[i],
			    (u_int)0, (u_int)0);
		}
	}
}

/*
 * Switch the "active" CPU (i.e. the one in kadb) to that specified by
 * the user.
 */
void
switch_cpu(int to)
{
	extern struct allregs_v9 adb_regs_v9;
	int otba = regsave.r_tba;

	if (cur_cpuid == to || cpu_nodeid[to] == -1) {
		printf("%d is not a valid CPU number\n", to);
		return;
	}
	db_printf(2, "switch_cpu: old %X, new %X\n", cur_cpuid, to);

	bcopy((caddr_t)&regsave, (caddr_t)&cpusave[cur_cpuid],
	    sizeof (regsave));
	bcopy((caddr_t)&fpuregs, (caddr_t)&mpfpregs[cur_cpuid],
	    sizeof (fpuregs));
	bcopy((caddr_t)&cpusave[to], (caddr_t)&regsave,
	    sizeof (regsave));
	bcopy((caddr_t)&mpfpregs[to], (caddr_t)&fpuregs,
	    sizeof (fpuregs));
	bcopy((caddr_t)&regsave, (caddr_t)&adb_regs_v9,
	    sizeof (regsave));

	regs_to_core();

	/*
	 * Make the "to" CPU active, and idle ourselves.
	 */
	cur_cpuid = to;
	reload_prom_callback();
	kadb_send_mondo(CPUID_TO_UPAID(to), (func_t)&start_cmd_loop,
	    (u_int)to, (u_int)0, (u_int)0, (u_int)0);
	kadb_idle_self(otba);
}

u_long saved_pc, saved_npc;
u_longlong_t saved_g1, saved_g2, saved_g3, saved_g4;
u_longlong_t saved_g5, saved_g6, saved_g7;
u_longlong_t saved_tstate;
int saved_tt, saved_pil;

char kadb_startup_hook[] = " ['] kadb_callback init-debugger-hook ";
char kadb_prom_hook[] = " ['] kadb_callback is debugger-hook ";

/*
 * Format the Forth word which tells the prom how to save state for
 * giving control to us.
 */
char *
format_prom_callback(void)
{
	static const char kadb_defer_word[] =
	    ": kadb_callback "
	    "  %%pc  h# %x  l! "
	    "  %%npc h# %x  l!"
	    "  %%g1 h# %x  x!"
	    "  %%g2 h# %x  x!"
	    "  %%g3 h# %x  x!"
	    "  %%g4 h# %x  x!"
	    "  %%g5 h# %x  x!"
	    "  %%g6 h# %x  x!"
	    "  %%g7 h# %x  x!"
	    "  1 %%tstate h# %x  x!"
	    "  1 %%tt h# %x  l!"
	    "  %%pil h# %x  l!"
	    "  h# %x   set-pc "
	    "    go "
	    "; ";
	static char prom_str[512];

	sprintf(prom_str, kadb_defer_word, &saved_pc, &saved_npc, &saved_g1,
	    &saved_g2, &saved_g3, &saved_g4, &saved_g5, &saved_g6,
	    &saved_g7, &saved_tstate, &saved_tt, &saved_pil, trap);

	return (prom_str);
}

/*
 * Inform the PROM of the address to jump to when it takes a breakpoint
 * trap.
 */
void
set_prom_callback(void)
{
	caddr_t str = format_prom_callback();

	prom_interpret(str, 0, 0, 0, 0, 0);
	prom_interpret(kadb_startup_hook, 0, 0, 0, 0, 0);
}

/*
 * For CPR.  Just arm the callback, since the required prom words have
 * already been prom_interpreted in the cpr boot program.
 */
void
arm_prom_callback(void)
{
	prom_interpret(kadb_startup_hook, 0, 0, 0, 0, 0);
}

/*
 * Reload the PROM's idea of "debugger-hook" for this CPU. The PROM
 * reinitializes the hook each time it is used, so we must re-arm it
 * every time a trap is taken.
 */
void
reload_prom_callback()
{
	prom_interpret(kadb_prom_hook, 0, 0, 0, 0, 0);
}

/*
 * Perform MP initialization. Walk the device tree and save the node IDs
 * of all CPUs in the system.
 */
void
mp_init()
{
	dnode_t nodeid;
	dnode_t sp[OBP_STACKDEPTH];
	pstack_t *stk;
	int upa_id, cpuid, i;
	extern caddr_t cpu_startup;


	for (i = 0; i < NCPU; i++)
		cpu_nodeid[i] = -1;

	stk = prom_stack_init(sp, sizeof (sp));
	for (nodeid = prom_findnode_bydevtype(prom_rootnode(), "cpu", stk);
	    nodeid != OBP_NONODE; nodeid = prom_nextnode(nodeid),
	    nodeid = prom_findnode_bydevtype(nodeid, "cpu", stk)) {
		if (prom_getprop(nodeid, "upa-portid",
		    (caddr_t)&upa_id) == -1) {
			prom_printf("cpu node %x without upa-portid prop\n",
			    nodeid);
			continue;
		}

		/*
		 * XXX - the following assumes the values are the same for
		 * all CPUs.
		 */
		if (prom_getprop(nodeid, "dcache-size",
		    (caddr_t)&vac_size) == -1) {
			prom_printf("can't get dcache-size for cpu 0x%x\n",
			    nodeid);
			vac_size = 2 * MMU_PAGESIZE;
		}
		if (prom_getprop(nodeid, "clock-frequency",
		    (caddr_t)&clock_freq) == -1) {
			prom_printf("can't get clock-freq for cpu 0x%x\n",
			    nodeid);
			clock_freq = 167;
		}
		Cpudelay = ((clock_freq + 500000) / 1000000) - 3;
		cpuid = UPAID_TO_CPUID(upa_id);
		if (!status_okay(nodeid, (char *)NULL, 0)) {
			prom_printf("kadb: bad status for cpu node %x\n",
			    cpuid);
			continue;
		}
		cpu_nodeid[cpuid] = (u_int)nodeid;
	}
	prom_stack_fini(stk);
}


/*
 * Because we don't manage the MMU ourselves, we don't set breakpoints
 * by manipulating the MMU directly. Instead we obtain a mapping from
 * our own private buffer to the physical page underlying "kva" - we
 * then modify that, flush the i-cache and unmap.
 */

/*
 * A long time ago someone changed so that even data writes would
 * use POKETEXT instead of POKEDATA. So here we have to deal with
 * this actually being in data space. See rev 1.16 of access.c
 */
static void
set_brkpt(caddr_t kva, int val)
{
	char *prop = "translations";
	int translen;
	dnode_t node;
	int valid, mode;
	u_longlong_t physaddr;
	caddr_t addr;
	caddr_t page_addr;
	extern int pagesize;
	int size;
	caddr_t bufptr;

	/*
	 * Map the physical page underlying kva to the breakpoint buffer
	 * we allocated at startup; then modify the location, and unmap.
	 */

	page_addr = kva - ((int)kva % pagesize);

	if (prom_translate_virt(page_addr, &valid, &physaddr,
	    &mode) != 0 || valid != -1) {
			prom_panic("do_bkpt: translate_virt() failure");
	}

	if (!(mode & PROM_MMU_MODE_WRITE))
		mode |= PROM_MMU_MODE_WRITE;
	if (!(mode & PROM_MMU_MODE_CACHED))
		mode |= PROM_MMU_MODE_CACHED;

	/* We need to make sure we do not create a virtual alias */
	/*
	 * To do this, we map the page only to the portion of
	 * bkptbuf so we have the same virtual color.  Noted
	 * we assume bkptbuf would have at least vac_size worth
	 * of address space, which is at least 2 pages.
	 */

	addr = kva - ((int)kva % vac_size);

	/*
	 * addr should cached-aligned with the brkptbuf
	 * So we should map the page at brkptbuf + (page_addr - addr)
	 */
	bufptr = bkptbuf + (u_int) ((u_int) page_addr - (u_int) addr);


	if (prom_map_phys(mode, pagesize, bufptr, physaddr) != 0) {
		prom_panic("do_bkpt: translate_virt() failure");
	}

	*(u_int *)(bufptr + (int)((u_int)kva % pagesize)) = val;

	sf_iflush((int *)(bufptr + (int)((int)kva % pagesize)));

	prom_unmap_virt(pagesize, bufptr);
}

/*
 * Look through all breakpoints for a watchpoint.
 */
struct bkpt *
wplookup(int type)
{
	register struct bkpt *bp;

	for (bp = bkpthead; bp; bp = bp->nxtbkpt) {
		if (bp->flag && (bp->type == type ||
		    (type == -1 && iswp(bp->type))))
			break;
	}
	return (bp);
}

/*
 * See if we entered the debugger via a watchpoint trap, and if so,
 * which one (virtual or physical). Return the associated breakpoint
 * so we can switch it off and single step over it before turning it
 * back on.
 */
struct bkpt *
tookwp()
{
	struct bkpt *bp = (struct bkpt *)0;

	if (regsave.r_tt == T_PA_WATCHPOINT && (bp = wplookup(BPPHYS))) {
		return (bp);
	} else if (regsave.r_tt == T_VA_WATCHPOINT &&
	    ((bp = wplookup(BPACCESS)) || (bp = wplookup(BPWRITE)))) {
		return (bp);
	}
	return (bp);
}

void
mach_fiximp()
{
	extern int icache_flush;

	icache_flush = 1;
}

/*
 * Borrowed from autoconf.c in the kernel - check to see if the nodeid's
 * status is valid.
 */
int
status_okay(int id, char *buf, int buflen)
{
	char status_buf[OBP_MAXPROPNAME];
	char *bufp = buf;
	int len = buflen;
	int proplen;
	static const char *status = "status";
	static const char *fail = "fail";
	int fail_len = (int)strlen(fail);

	/*
	 * Get the proplen ... if it's smaller than "fail",
	 * or doesn't exist ... then we don't care, since
	 * the value can't begin with the char string "fail".
	 *
	 * NB: proplen, if it's a string, includes the NULL in the
	 * the size of the property, and fail_len does not.
	 */
	proplen = prom_getproplen((dnode_t)id, (caddr_t)status);
	if (proplen <= fail_len)	/* nonexistant or uninteresting len */
		return (1);

	/*
	 * if a buffer was provided, use it
	 */
	if ((buf == (char *)NULL) || (buflen <= 0)) {
		bufp = status_buf;
		len = sizeof (status_buf);
	}
	*bufp = (char)0;

	/*
	 * Get the property into the buffer, to the extent of the buffer,
	 * and in case the buffer is smaller than the property size,
	 * NULL terminate the buffer. (This handles the case where
	 * a buffer was passed in and the caller wants to print the
	 * value, but the buffer was too small).
	 */
	(void) prom_bounded_getprop((dnode_t)id, (caddr_t)status,
	    (caddr_t)bufp, len);
	*(bufp + len - 1) = (char)0;

	/*
	 * If the value begins with the char string "fail",
	 * then it means the node is failed. We don't care
	 * about any other values. We assume the node is ok
	 * although it might be 'disabled'.
	 */
	if (strncmp(bufp, fail, fail_len) == 0)
		return (0);

	return (1);
}

void
setup_aux(void)
{
	pstack_t *stk;
	dnode_t node;
	dnode_t sp[OBP_STACKDEPTH];
	char name[OBP_MAXDRVNAME];
	static char cpubuf[2 * OBP_MAXDRVNAME];
	extern char *cpulist;

	stk = prom_stack_init(sp, sizeof (sp));
	node = prom_findnode_bydevtype(prom_rootnode(), "cpu", stk);
	if (node != OBP_NONODE && node != OBP_BADNODE) {
		if (prom_getprop(node, OBP_NAME, name) <= 0)
			prom_panic("no name in cpu node");
		strcpy(cpubuf, name);
		if (prom_getprop(node, OBP_COMPATIBLE, name) > 0) {
			strcat(cpubuf, ":");
			strcat(cpubuf, name);
		}
		cpulist = cpubuf;
	} else
		prom_panic("no cpu node");
	prom_stack_fini(stk);
}
