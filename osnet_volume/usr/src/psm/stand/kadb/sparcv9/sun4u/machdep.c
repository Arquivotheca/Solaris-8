/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machdep.c	1.13	99/05/25 SMI"

#include <sys/param.h>
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
#include <sys/stack.h>
#include "adb.h"
#include "allregs.h"
#include "sr_instruction.h"
#include <sys/debug/debug.h>
#include <sys/debug/debugger.h>
#include <sys/openprom.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/bootconf.h>

#include <sys/consdev.h>

#include <sys/cpu_sgnblk_defs.h>
#include <sys/cpuvar.h>

static void set_brkpt(caddr_t, int);
void scbsync(void);
int in_prom(uintptr_t addr);

extern void mp_init(void);
extern void set_prom_callback(void);
extern struct scb *gettba(void);
extern char *getsp();
extern void _exit(void);
extern int printf();
extern char *sprintf();		/* standalone lib uses char * */
extern int patch_brkpt(addr_t, char *);
extern int unpatch_brkpt(addr_t, char *);
extern void bzero(void *s, size_t n);

int wp_mask = 0xff;		/* watchpoint mask */
extern struct bkpt *bkpthead;
extern char estack[], etext[], edata[];
extern struct bootops *bootops;
extern int errno;
extern cons_polledio_t polled_io;
extern int elf64mode;

int fake_bpt;			/* place for a fake breakpoint at startup */
struct scb *mon_tba;

static void initialize_console(void);
static void claim_console(void);
static void release_console(void);


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

	initialize_console();
}


/*
 * Initialize the system console for kadb I/O
 */
static void
initialize_console(void)
{
	bzero(&polled_io, sizeof (cons_polledio_t));
}

/*
 * Claim the system console for kadb I/O
 */
static void
claim_console(void)
{
	unsigned long   (*func)();
	unsigned long   args[1];

	/*
	 * If a callback exists, tell the kernel that polled mode
	 * is being entered.
	 */
	if (polled_io.cons_polledio_enter != NULL) {

		/*
		 * The kernel_invoke function
		 * will adjust the stack pointer,
		 * stack frame, etc. if the kernel is 32 bit.
		 */
		func = (unsigned long(*)())
			polled_io.cons_polledio_enter;
		args[0] = (unsigned long)
			polled_io.cons_polledio_argument;
		kernel_invoke(func, 1, args);
	}
}

/*
 * Relase the system console for use by the kernel
 */
static void
release_console(void)
{
	unsigned long   (*func)();
	unsigned long   args[1];

	/*
	 * If a callback exists, tell the kernel that polled mode
	 * is being exited.
	 */
	if (polled_io.cons_polledio_exit != NULL) {

		/*
		 * The kernel_invoke function
		 * will adjust the stack pointer,
		 * stack frame, etc. if the kernel is 32 bit.
		 */
		func = (unsigned long(*)())
			polled_io.cons_polledio_exit;
		args[0] = (unsigned long)
			polled_io.cons_polledio_argument;
		kernel_invoke(func, 1, args);
	}
}

/*
 * Miscellanous fault/error handler, called from trap(). If
 * we took a trap while in the debugger, we longjmp back to
 * where we were
 */
fault(trap, trappc, trapnpc)
	register int trap;
	register long trappc;
	register long trapnpc;
{
	register int ondebug_stack;

	db_printf(4, "fault:  tt %lx, tpc %lx tnpc %lx nofault %lx",
				trap, trappc, trapnpc, nofault);

	ondebug_stack = (getsp() > etext && getsp() < estack);

	if ((trap == T_DATA_MMU_MISS || trap == FAST_DMMU_MISS_TT) &&
	    nofault && ondebug_stack) {
		jmp_buf_ptr sav = nofault;
		nofault = NULL;
		_longjmp(sav, 1);
		/*NOTREACHED*/
	}

	traceback((caddr_t)getsp());

	if (abort_jmp && ondebug_stack) {
		printf("abort jump: trap %llx sp %llx pc %llx npc %llx\n",
			trap, getsp(), trappc, trapnpc);
		printf("etext %llx estack %llx edata %llx nofault %x\n",
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

	printf("fault: calling cmd, trap %llx sp %llx pc %llx npc %llx\n",
	    trap, getsp(), trappc, trapnpc);
	cmd();	/* error not resolved, enter debugger */
	return (0);
}

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
peek32(addr)			/* Peek a word 32 bits in ILP32/LP64 */
	int *addr;
{
	int val;

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

int
poke32(addr, val)
	int *addr;
	int val;
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
	char *asp;	/* Sp adjusted for v9bias */


	db_printf(4, "spawn: sp=%X routine=%X", sp, routine);

	if (curregs != 0) {
		printf("bad call to spawn\n");
		_exit();
	}
	if (_setjmp(mainregs) == 0) {
		/*
		 * Setup top (null) window.
		 */
		sp -= STACK_BIAS;
		sp -= WINDOWSIZE64;
		asp = sp + STACK_BIAS;
		((struct rwindow64 *)asp)->rw_rtn = 0;
		((struct rwindow64 *)asp)->rw_fp = 0;

		/*
		 * Setup window for routine with return to exit.
		 */
		fp = sp;
		sp -= WINDOWSIZE64;
		asp = sp + STACK_BIAS;
		((struct rwindow64 *)asp)->rw_rtn = (long)_exit - 8;
		((struct rwindow64 *)asp)->rw_fp = (long)fp;

		/*
		 * Setup new return window with routine return value.
		 */
		fp = sp;
		sp -= WINDOWSIZE64;
		asp = sp + STACK_BIAS;
		((struct rwindow64 *)asp)->rw_rtn = (long)routine - 8;
		((struct rwindow64 *)asp)->rw_fp = (long)fp;

		/* copy entire jump buffer to debugregs */
		bcopy((caddr_t)mainregs, (caddr_t)debugregs, sizeof (jmp_buf));
		debugregs[JB_SP] = (long)sp;	/* set sp */

		curregs = debugregs;
		writereg(Reg_NPC, (uintptr_t)&fake_bpt); /* for -k option */
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
	if (_setjmp(curregs) == 0) {

		if (curregs == mainregs) {
			curregs = debugregs;
		} else {		/* curregs == debugregs */
			curregs = mainregs;
		}
		_longjmp(curregs, 1);
		/*NOTREACHED*/
	} /* else continue on in new context */
}


/*
 * Main interpreter command loop.
 */
void
cmd()
{
	char *sp;

	dorun = 0;

	/*
	 * Make sure we aren't already on the debugger stack; if we are,
	 * we took some sort of fault (e.g. unexpected TLB miss) inside
	 * the debugger itself. Since we can't repair that or go back to
	 * userland, just bail into the PROM.
	 */
	sp = getsp();

	if (sp > etext && sp < estack) {
		printf("cmd: fault inside debugger! pc=%lX, tt=%X\n",
			    readreg(Reg_PC), readreg(Reg_TT));
		_exit();
	}
	if (in_prom(readreg(Reg_PC))) {
		printf("kadb: Called from within the PROM, exiting...\n");
		_exit();
	}

	do {

		claim_console();

		doswitch();

		release_console();

		if (dorun == 0) {
			printf("cmd: nothing to do\n");
			_exit();
		}
	} while (dorun == 0);
}


static void
ss_delbp(struct bkpt *bp)
{
	if (bp->flag && (bp->flag & BKPT_ERR) == 0)
		(void) unpatch_brkpt(bp->loc, (char *)&bp->ins);
}


static void
ss_setbp(struct bkpt *bp)
{
	if (bp->flag) {
		if (bp->loc & INSTR_ALIGN_MASK ||
		    patch_brkpt(bp->loc, (char *)&bp->ins) < 0) {
			prints("single-stepper cannot set breakpoint: ");
			psymoff(bp->loc, ISYM, "\n");
			bp->flag |= BKPT_ERR;	/* turn on err flag */
		} else {
			bp->flag &= ~BKPT_ERR;	/* turn off err flag */
			db_printf(3, "ss_setbp: set breakpoint");
		}
	}
}


typedef enum {
	step_error,
	normal,
	emulated,
	bcond,
	bcond_annul,
	ba,
	ba_annul
} br_type;

/*
 * figure_step -- return the "branch-type" of the instruction at iaddr.
 * If it is a branch, set *targetp to be the branch's target address.
 * Emulate certain non-branch instructions.
 */
static br_type
figure_step(addr_t iaddr, addr_t *targetp)
{
	unsigned int instr;
	unsigned int annul, cond;
	long rs1, rs2, tstate;
	int br_offset;	/* Must be *signed* for the sign-extend to work */
	br_type brt;

	instr = (unsigned int) get(iaddr, ISP);
	if (errflg) {
	    errflg = 0;
	    return (step_error);
	}


	brt = normal;
	switch (X_OP(instr)) {	/* switch on main opcode */
	case SR_FMT2_OP:
		annul = X_ANNUL(instr);
		cond = X_COND(instr);

		switch (X_OP2(instr)) {
		case SR_BICC_OP:
		case SR_FBCC_OP:
			if (cond == SR_ALWAYS)	{ brt = ba; }
			else			{ brt = bcond; }
			brt = (br_type) (brt + annul);

			br_offset = SR_WA2BA(SR_SEX22(X_DISP22(instr)));
			*targetp = iaddr + br_offset;
			break;

		case SR_BPCC_OP:
		case SR_FBPCC_OP:
			if (cond == SR_ALWAYS)	{ brt = ba; }
			else			{ brt = bcond; }
			brt = (br_type) (brt + annul);

			br_offset = SR_WA2BA(SR_SEX19(X_DISP19(instr)));
			*targetp = iaddr + br_offset;
			break;

		case SR_BPR_OP:
			brt = (br_type) (bcond + annul);

			br_offset = SR_JOIN16(X_D16HI(instr), X_D16LO(instr));
			br_offset = SR_WA2BA(SR_SEX16(br_offset));
			*targetp = iaddr + br_offset;
			break;
		}
		break;

	case SR_FMT3a_OP:
		switch (X_OP3(instr)) {
		case SR_RDPR_OP:
			if (X_RS1(instr) != SR_PSTATE_PR)
				break;

			brt = emulated;

			tstate = readreg(Reg_TSTATE);
			tstate >>= TSTATE_PSTATE_SHIFT;
			tstate &= TSTATE_PSTATE_MASK;
			writereg(Reg_G0 + X_RD(instr), tstate);
			break;

		case SR_WRPR_OP:
			if (X_RD(instr) != SR_PSTATE_PR)
				break;

			brt = emulated;

			rs1 = readreg(Reg_G0 + X_RS1(instr));
			if (X_IMM(instr))
				rs2 = X_SIMM13(instr);
			else
				rs2 = readreg(Reg_G0 + X_RS2(instr));
			tstate = readreg(Reg_TSTATE);
			tstate &= ~(TSTATE_PSTATE_MASK << TSTATE_PSTATE_SHIFT);
			tstate |= ((rs1 ^ rs2) & TSTATE_PSTATE_MASK) <<
				    TSTATE_PSTATE_SHIFT;
			writereg(Reg_TSTATE, tstate);
			break;
		}
		break;

	default:		/* format three */
		break;
	} /* end switch on main opcode */

	return (brt);

} /* end figure_step */


/*
 * sswait -- set up the current processor for single step, then make
 * it happen.  Since SPARC v9 has no hardware support for single
 * stepping, we have to do it by setting breakpoints anywhere the
 * the CPU might execute next.
 */
void
sswait(void)
{
	struct bkpt	bk_npc, bk_pc8, bk_systrap;
	addr_t		npc, trg, pc;
	long		tstate;
	br_type		br;

	extern addr_t	systrap;	/* address of kernel's trap() */

	/*
	 * We emulate certain instructions rather than step over
	 * them.  See the comments below on PSTATE.IE for more on
	 * why.
	 */
	pc = (addr_t)readreg(Reg_PC);
	npc = (addr_t)readreg(Reg_NPC);

	/* figure_step sets trg to target, if it found a branch */
	br = figure_step(pc, &trg);
	if (br == emulated) {
		writereg(Reg_PC, npc);
		writereg(Reg_NPC, npc + 4);
		return;
	}

	/*
	 * For most instructions (including branches with the ANNUL
	 * bit clear, and CALL and JMPL instructions), PC points to
	 * the instruction to step over, and NPC points to the
	 * location where we should stop.  However, if PC points to
	 * a branch with the ANNUL bit set, the processor may ignore
	 * NPC.  In this case, we may need as many as two
	 * breakpoints in order to be sure that we're really
	 * single-stepping.
	 *
	 * Here's the full rule for where to put the breakpoints:
	 *	IF instr(PC) is unconditional branch, with ANNUL,
	 *		THEN put one at the branch target.
	 *	ELSE
	 *		put one at NPC.
	 *		IF instr(PC) is conditional branch with ANNUL bit,
	 *			THEN also put one at PC+8
	 */

	bk_npc.flag = 1;
	if (br == ba_annul)
		bk_npc.loc = trg;
	else {
		if (in_prom(npc))		/* can't set bkpt in there */
			npc = pc + 4;		/* catch it on return */

		bk_npc.loc = npc;
		if (br == bcond_annul && pc+8 != npc) {
			bk_pc8.flag = 1;
			bk_pc8.loc  = pc+8;
			ss_setbp(&bk_pc8);
		} else
			bk_pc8.flag = 0;
	}
	ss_setbp(&bk_npc);

	/*
	 * Put a breakpoint at trap() in the kernel, in case our
	 * single step instruction generates a trap.
	 */
	if (systrap && pc && pc != systrap) {
		bk_systrap.flag = 1;
		bk_systrap.loc = systrap;
		ss_setbp(&bk_systrap);
	} else
		bk_systrap.flag = 0;

	/*
	 * If an interrupt is pending during a single step, we may
	 * have a problem, since the interrupt handlers don't all go
	 * through trap(), and some of them rely on all CPUs to be
	 * responsive.  When stepping, the other CPUs are idle in
	 * kadb, not in the kernel, so deadlocks are possible.
	 *
	 * We work around this by forcing PSTATE.IE to be clear
	 * while stepping.  We've already emulated instructions
	 * which might read or write PSTATE.IE.
	 */
	tstate = readreg(Reg_TSTATE);
	writereg(Reg_TSTATE, tstate & ~TSTATE_IE);
	doswitch();
	tstate &= TSTATE_IE;
	tstate |= readreg(Reg_TSTATE) & ~TSTATE_IE;
	writereg(Reg_TSTATE, tstate);

	ss_delbp(&bk_npc);
	ss_delbp(&bk_pc8);
	ss_delbp(&bk_systrap);
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

void
traceback(sp)
	caddr_t sp;
{
	register uint_t tospage;
	register struct frame *fp;

#ifdef PARTIAL_ALIGN
	if (partial_align? ((int)sp & 0x3): ((int)sp & 0x7)) {
#else
	if ((uintptr_t)sp & (STACK_ALIGN-1)) {
#endif PARTIAL_ALIGN
		printf("traceback: misaligned sp = %llx\n", sp);
		return;
	}
	flush_windows();
	tospage = (uint_t)btopr((ulong_t)sp);
	fp = (struct frame *)sp;
	printf("Begin traceback... sp = %llx\n", sp);
	while (btopr((ulong_t)fp) == tospage) {
		struct frame *newfp;

		newfp = fp->fr_savfp;
		if (newfp != 0)
		    newfp = (struct frame *)((char *)newfp + STACK_BIAS);

		if (fp == newfp) {
			printf("FP loop at %llx", fp);
			break;
		}
		printf("Called from %llx, fp=%llx, args="
			"%llx %llx %llx %llx %llx %llx\n",
		    fp->fr_savpc, fp->fr_savfp,
		    fp->fr_arg[0], fp->fr_arg[1], fp->fr_arg[2],
		    fp->fr_arg[3], fp->fr_arg[4], fp->fr_arg[5]);
		fp = newfp;
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
in_prom(uintptr_t addr)
{
	return (addr >= (uintptr_t)mon_tba);
}

/*
 * Because we don't manage the MMU ourselves, we don't set breakpoints
 * by manipulating the MMU directly. Instead we obtain a mapping from
 * our own private buffer to the physical page underlying "kva" - we
 * then modify that, flush the i-cache and unmap.
 */
static void
set_brkpt(caddr_t kva, int val)
{
	int valid, mode;
	u_longlong_t physaddr;
	caddr_t addr;
	caddr_t page_addr;
	extern int pagesize;
	caddr_t bufptr;

	/*
	 * Map the physical page underlying kva to the breakpoint buffer
	 * we allocated at startup; then modify the location, and unmap.
	 */

	db_printf(1, "In setbrkpt %J %X\n", kva, val);

	page_addr = kva - ((uintptr_t)kva % pagesize);

	if (prom_translate_virt(page_addr, &valid, &physaddr,
	    &mode) != 0 || valid != -1) {
			prom_panic("do_bkpt: translate_virt() failure");
	}

	mode |= PROM_MMU_MODE_WRITE | PROM_MMU_MODE_CACHED;

	/* We need to make sure we do not create a virtual alias */
	/*
	 * To do this, we map the page only to the portion of
	 * bkptbuf so we have the same virtual color.  Noted
	 * we assume bkptbuf would have at least vac_size worth
	* of address space, which is at least 2 pages.
	 */

	addr = kva - ((uintptr_t)kva % vac_size);

	/*
	 * addr should cached-aligned with the brkptbuf
	 * So we should map the page at brkptbuf + (page_addr - addr)
	 */
	bufptr = bkptbuf + (uint_t)((uint_t)page_addr - (uint_t)addr);


	if (prom_map_phys(mode, pagesize, bufptr, physaddr) != 0) {
		prom_panic("do_bkpt: translate_virt() failure");
	}

	*(int *)(bufptr + (uintptr_t)kva % pagesize) = val;
	sf_iflush((int *)(bufptr + (uintptr_t)kva % pagesize));
	prom_unmap_virt(pagesize, bufptr);

	db_printf(1, "Leaving setbrkpt\n");
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

	if (readreg(Reg_TT) == T_PA_WATCHPOINT && (bp = wplookup(BPPHYS))) {
		return (bp);
	} else if (readreg(Reg_TT) == T_VA_WATCHPOINT &&
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
