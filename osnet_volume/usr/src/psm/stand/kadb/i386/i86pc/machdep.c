/*
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machdep.c	1.32	99/08/19 SMI"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/reboot.h>
#include <sys/sysmacros.h>
#include <sys/cpuvar.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/pte.h>
#include <sys/psw.h>
#include <sys/trap.h>
#include <sys/clock.h>
#include <sys/t_lock.h>
#include <sys/reg.h>
#include <sys/asm_linkage.h>
#include <sys/frame.h>
#include <sys/xc_levels.h>
#include <setjmp.h>

#include <sys/stack.h>
#include <sys/debug/debug.h>
#include <sys/debug/debugger.h>
#include <sys/bootconf.h>
#include <sys/promif.h>
#include <sys/psm_types.h>
#include <i386.h>
#include <adb.h>
#include "symtab.h"

/*
 * We want to use the kadb printf function here,
 * not the one provided with adb, nor the one
 * provided with the boot_syscalls interface!
 */
#undef printf
extern int printf(char *, ...);

extern int errno;

int cur_cpuid = -1;	/* The id of our current cpu. Initialized to -1 to  */
			/* indicate not currently in kadb */
int istrap = 0;
int ishtrap = 0;
static struct bkpt *hwbkpt = NULL;
void set_hwbkpt(int);

extern cons_polledio_t polled_io;

int fake_bpt;			/* place for a fake breakpoint at startup */
jmp_buf debugregs;		/* context for debugger */
jmp_buf mainregs;		/* context for debuggee */
jmp_buf_ptr curregs;		/* pointer to saved context for each process */
struct regs *regsave; 		/* temp save area--align to double */
struct regs *reg; 		/* temp save area--align to double */

extern char rstk[], etext[], edata[];
extern int estack;

/* physical memory access data */
static int kadb_phys_mode;
static uint64_t saved_paddr;
static char phys_buf[4*4096];
static char *phys_page;
extern int (*kadb_map)(caddr_t, uint64_t, uint64_t *);
extern int (**psm_notifyf)(int);
extern void delayl(int);

/*
 * Definitions for registers in jmp_buf
 */
#define	JB_PC	5
#define	JB_SP	4
#define	JB_FP	3
#define	STKFRAME 8	 /* return addr + old frame pointer */

static int step_expected = 0;
static int step_was_expected = 0;
static int saved_int_bit = 0;
static int saved_flags = 0;
static int doing_popf = 0;
static int doing_pushf = 0;

struct	regs *i_fparray[NCPU];	/* array for NCPU frame pointers */

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
 * Miscellanous fault error handler
 *
 * Return value is:
 *
 *	 0 - done
 *	 1 - single step expected
 *	-1 - we did nothing; pass it to the kernel trap handler
 */
fault(i_fp)
struct regs *i_fp;
{
	register int ondebug_stack;
	int	reason = 0;
	extern int first_time;
	extern int go2;
	extern int interactive;
	int sp;
#define	DO_FAULT_TRACE
#ifdef	DO_FAULT_TRACE
	extern void traceback(caddr_t);
#endif
	extern void fix_kadb_selectors(int);
	struct asym *asp;
	extern void set_gdt_entry(int, int, int);

	if (first_time) {

		/*
		 * We use a fake break point to get from main() to
		 * here before we call the user/debugged program.
		 * This is done so that our transition from user to
		 * debugger and back only occures in fault().
		 */
		first_time = 0;	/* this can only be done once */

		/* setup our initial reg values */
		reg = regsave = i_fp;
		i_fp->r_eip = go2;
		i_fp->r_ecx = 0;
		i_fp->r_ebx = (int)&bootops; /* boot properties root node */
		i_fp->r_edi = 0;
		i_fp->r_esi = 0;
		i_fp->r_eax = 0;
		i_fp->r_edx = 0;
		i_fp->r_ebp = 0;	/* so stack trace knows when to stop */

		/* the following enables deferred breakpoints for "kadb -d" */
		if ((asp = lookup("cpus")) != NULL)
			set_gdt_entry(KGSSEL, asp->s_value,
			    0x30b | (DATA_ACC2_S << 20) |
			    ((uint_t)KDATA_ACC1 << 24));

		if (interactive & RB_KRTLD) {
			(void) cmd(i_fp);
			if (dotrace) {
				scbstop = 1;
				dotrace = 0;
			}
		}
		/* XXX previous and next pages might be made invalid */
		phys_page = (char *)(((int)phys_buf + 0x2000) & 0xfffff000);
		return (step_expected);
	}

	/* kadb should use the normal kernel selectors */
	fix_kadb_selectors(i_fp->r_cs);

	/*
	 * Quick return to let kernel handle user-level debugging.
	 */
	if ((i_fp->r_trapno == T_BPTFLT || i_fp->r_trapno == T_SGLSTP) &&
	    (u_int)i_fp->r_eip < (u_int)USERLIMIT)
		return (-1);

	sp = (int)getsp();
	ondebug_stack = (sp > (int)&rstk && sp < (int)estack);

	i_fp->r_esp += 0x14;		/* real stack before excpt. */

	/*
	 * Assume that nofault won't be non-NULL if we're not in kadb.
	 * ondebug_stack won't be true early on, and it's possible to
	 * get an innocuous data fault looking for kernel symbols then.
	 */
	if ((i_fp->r_trapno == T_GPFLT ||
	    i_fp->r_trapno == T_PGFLT) && nofault) {
		jmp_buf_ptr sav = nofault;

		nofault = NULL;
		_longjmp(sav, 1);
		/*NOTREACHED*/
	}

	regsave = i_fp;

	/*
	 * Is this is a single step or a break point?
	 */
	if (i_fp->r_trapno == T_BPTFLT || i_fp->r_trapno == T_SGLSTP) {
		/*
		 * If this is single step make sure the trace bit is off.
		 * Hardware should do this for us - but it does not do it
		 * every time.
		 */
		if (i_fp->r_trapno == T_SGLSTP)
		{
			int rdr6;
			extern int kcmntrap;
			extern uchar_t dr_cpubkptused[NCPU];

			rdr6 = dr6();
			if (!step_expected && kcmntrap &&
			    !(((unsigned)cur_cpuid < NCPU ?
			    dr_cpubkptused[cur_cpuid] : 0xf) & rdr6))
				return (-1);	/* kernel should field it */
			if (rdr6 & 0xf) {	/* hardware break point */
				ishtrap = 1;
				set_hwbkpt(rdr6);
			}
			if (doing_popf) {
				i_fp->r_efl = saved_flags;
				doing_popf = 0;
			} else if (doing_pushf) {
				i_fp->r_efl &= ~0x100;
				i_fp->r_efl |= saved_int_bit;
				*(int *)(i_fp->r_esp) &= ~0x100;
				*(int *)(i_fp->r_esp) |= saved_int_bit;
				doing_pushf = 0;
			} else {
				i_fp->r_efl &= ~0x100;
				i_fp->r_efl |= saved_int_bit;
			}
		} else {
			/*
			 * We set the extern 'istrap' flag to alert the
			 * bpwait routine that the eip must be set back
			 * one byte
			 */
			if (Peekc(i_fp->r_eip + PCFUDGE) == 0xcc) {
				extern int kcmntrap;
				if (kcmntrap &&
				    (bkptlookup(i_fp->r_eip + PCFUDGE) == 0))
					return (-1);
				istrap = 1;
			} else {
				/*
				 * There was a kernel breakpoint hit on
				 * this processor, but it was removed before
				 * kadb got control for it.  Since the
				 * breakpoint has been removed, we do
				 * the cleanup, and just continue.
				 */
				i_fp->r_eip += PCFUDGE;
				step_expected = 0;
				return (step_expected);
			}
		}
		step_was_expected = step_expected;
		cmd(i_fp);
		return (step_expected);
	}

	/*
	 * Is this Programmed entry (int 20)
	 */
	if (i_fp->r_trapno == 20) {
		cmd(i_fp);
		return (step_expected);
	}

	/*
	 * If we are on the debugger stack and
	 * abort_jmp is set, do a longjmp to it.
	 */
	if (abort_jmp && ondebug_stack) {
#ifdef	DO_FAULT_TRACE
		traceback((caddr_t)i_fp->r_ebp);
#endif
		printf("abort jump: trap %x cr2 %x sp %x pc %x\n",
			i_fp->r_trapno, cr2(), getsp(), i_fp->r_eip);
		printf("etext %x estack %x edata %x nofault %x\n",
			etext, estack, edata, nofault);
		printf("eax %x ebx %x ecx %x edx %x\n",
			i_fp->r_eax, i_fp->r_ebx, i_fp->r_ecx, i_fp->r_edx);
		printf("esi %x edi %x ebp %x esp %x\n",
			i_fp->r_esi, i_fp->r_edi, i_fp->r_ebp, i_fp->r_esp);
		_longjmp(abort_jmp, 1);
		/*NOTREACHED*/
	}

	/*
	 * Check for "page fault while doing a single step".  If this occurs,
	 * kadb cannot effectively single step this instruction.  The way this
	 * is handled is similar to single stepping "iret", i.e., a message is
	 * printed, and we just continue.
	 */
	if ((i_fp->r_trapno == T_PGFLT) && step_expected) {
		if (bkptlookup(i_fp->r_eip) != 0) {
			printf("\nThe breakpoint at this instruction must "
			    "be removed ':d' before continuing.");
		}
		printf("\nThe current instruction cannot be single-stepped\n"
		    "because it causes a page fault.\n"
		    "You will have to type ':c' to continue from here.\n");
		i_fp->r_efl &= ~0x100;
		i_fp->r_efl |= saved_int_bit;
		cmd(i_fp);
		return (step_expected);
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

#ifdef	DO_FAULT_TRACE
	traceback((caddr_t)i_fp->r_ebp);
#endif
	printf("fault: trap: [%x] error: [%x] sp: [%x]"
	    " pc: [%x] fault-address(cr2): [%x]\n",
	    i_fp->r_trapno, i_fp->r_err, i_fp->r_esp, i_fp->r_eip, cr2());
	delayl(0xffffff);

#ifdef DEBUG
	ml_pause();
#endif
	cmd(i_fp);	/* error not resolved, enter debugger */

	return (step_expected);
}

void
nmifault(void)
{
	printf("\nNMI ignored by kadb.\n\n");
}

static jmp_buf_ptr saved_jb;
static jmp_buf jb;
extern int debugging;

void
restore_phys(void)
{
	uint64_t ignored;

	if (kadb_phys_mode == 0 || kadb_map == 0)
		return;

	(void) (*kadb_map)(phys_page, saved_paddr, &ignored);
}

int
check_phys(char **addr)
{
	int offset;
	struct asym *asp;
	uint64_t physaddr64;

	if (kadb_phys_mode == 0)
		return (0);
	else if (kadb_map == 0) {
		if ((asp = lookup("kadb_map")) != NULL)
			kadb_map = (int(*)(caddr_t, uint64_t, uint64_t *))
			    (asp->s_value);
		else
			return (1);
	}

	offset = (int)(*addr) & 0xfff;
	physaddr64 = (((uint64_t)phys_upper32) << 32) + (u_int)*addr;
	if ((*kadb_map)(phys_page, physaddr64, &saved_paddr) == 0)
		return (1);	/* failed */
	*addr = (char *)((int)phys_page | offset);
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
	u_char val;

	saved_jb = nofault;
	nofault = jb;
	errno = 0;
	if (!_setjmp(jb)) {
		if (check_phys(&addr))
			goto error;
		val = *addr;
		/* if we get here, it worked */
		nofault = saved_jb;
		restore_phys();
		return ((int)val);
	}
	/* a fault occured */
	restore_phys();
error:
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
		if (check_phys((char **)&addr))
			goto error;
		val = *addr;
		/* if we get here, it worked */
		nofault = saved_jb;
		restore_phys();
		return (val);
	}
	/* a fault occured */
	restore_phys();
error:
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
		if (check_phys((char **)&addr))
			goto error;
		val = *addr;
		/* if we get here, it worked */
		nofault = saved_jb;
		restore_phys();
		return (val);
	}
	/* a fault occured */
	restore_phys();
error:
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
		if (check_phys(&addr))
			goto error;
		*addr = val;
		restore_phys();
		/* if we get here, it worked */
		nofault = saved_jb;
		return (0);
	}
	restore_phys();
error:
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
		if (check_phys((char **)&addr))
			goto error;
		*addr = val;
		restore_phys();
		/* if we get here, it worked */
		nofault = saved_jb;
		return (0);
	}
	restore_phys();
error:
	/* a fault occured */
	nofault = saved_jb;
	errno = EFAULT;
	return (-1);
}

poketext(addr, val)
	long *addr;
	long val;
{
	return (pokel(addr, val));
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

/* allow reading physical memory */
rcopy(from, to, count)
	register char *from;
	register char *to;
	register int count;
{
	register int val;

	for (; count > 0; count--) {
		kadb_phys_mode = phys_address;
		if ((val = Peekc(from++)) == -1)
			goto err;
		kadb_phys_mode = 0;
		if (pokec(to++, val) == -1)
			goto err;
	}
	return (0);
err:
	errno = EFAULT;
	return (-1);
}

/* allow writing physical memory */
wcopy(from, to, count)
	register char *from;
	register char *to;
	register int count;
{
	register int val;

	for (; count > 0; count--) {
		kadb_phys_mode = 0;
		if ((val = Peekc(from++)) == -1)
			goto err;
		kadb_phys_mode = phys_address;
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
	extern void _exit();

	if (curregs != 0) {
		printf("bad call to spawn\n");
		_exit();
	}
	if ((res = _setjmp(mainregs)) == 0) {
		/*
		 * Setup top (null) stack frame.
		 */
		sp -= STKFRAME;
		((struct frame *)sp)->fr_savpc = 0;
		((struct frame *)sp)->fr_savfp = 0;
		/*
		 * Setup stack frame for routine with return to exit.
		 */
		fp = sp;
		sp -= STKFRAME;
		((struct frame *)sp)->fr_savpc = (int)_exit;
		((struct frame *)sp)->fr_savfp = 0;
		/*
		 * Setup new return stack frame with routine return value.
		 */
		fp = sp;
		sp -= STKFRAME;
		((struct frame *)sp)->fr_savpc = (int)routine;
		((struct frame *)sp)->fr_savfp = (int)fp;

		/* copy entire jump buffer to debugregs */
		bcopy((caddr_t)mainregs, (caddr_t)debugregs, sizeof (jmp_buf));
		debugregs[JB_FP] = (int)sp;	/* set sp */

		curregs = debugregs;

		_longjmp(debugregs, 1);		/* jump to new context */
		/*NOTREACHED*/
	}
}

void
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

extern	int	*xc_initted;
extern	void 	(*kxc_call)(int, int, int, int, int, void());
extern	void 	get_smothered();

/*
 * Since we want to do single stepping with interrupts disabled,
 * we need to be careful about a few of the instructions that mofify
 * the interrupt enable bit in the eflags.
 *
 * The function returns 1 when it identified one of the instructions it
 * emulates, it returns 2 for instructions that cannot be single-stepped,
 * otherwise it returns 0 meaning "did not emulate".
 *
 * Pushf and popf take special post-processing, and set global data to
 * allow for proper emulation.
 *
 * Iret cannot be single-stepped.  We return 2 for this instruction.
 */
emulated(struct regs *i_fp)
{
	unsigned char *eip = (unsigned char *)(i_fp->r_eip);

	if (*eip == 0xFA) {		/* cli */
		i_fp->r_eip += 1;
		i_fp->r_efl &= ~0x200;
		return (1);
	} else if (*eip == 0xFB) {	/* sti */
		i_fp->r_eip += 1;
		i_fp->r_efl |= 0x200;
		return (1);
	} else if (*eip == 0x9C) {	/* pushfl */
		doing_pushf = 1;
	} else if (*eip == 0x9D) {	/* popfl */
		saved_flags = *(int *)(i_fp->r_esp) & 0x00277FD7;
		*(int *)(i_fp->r_esp) &= ~0x200;
		doing_popf = 1;
	} else if (*eip == 0xCC || *eip == 0xCD || *eip == 0xCE) {
		printf("\nThe 'int $N' instruction(s) cannot be "
		    "single-stepped.\n");
		return (2);
	} else if (*eip == 0xCF) {	/* iret */
		printf("\nThe 'iret' instruction cannot be "
		    "single-stepped.\n");
		return (2);
	}

	return (0);
}

/*
 * Main interpreter command loop.
 */
void
cmd(i_fp)
struct regs *i_fp;
{
	int addr;
	int emul;
	static nsessions = 0;
	extern void goto_kernel();

	if (++nsessions != 1) {
		printf("kadb has been entered %d times (not once)\n",
		    nsessions);
		printf("Resetting counter\n");
		nsessions = 1;
	}

back_to_prompt:

	dorun = 0;

	/*
	 * See if the sp says that we are already on the debugger stack
	 */
	reg = regsave;
	addr = (int)getsp();
	if (addr > (int)&rstk && addr < (int)estack) {
		static int count = 0;
		if (count == 0)
			printf("Already in debugger!\n");
		++count;
		if ((count % 0x400000) == 0)
			printf(".");
		delayl(0xffffff);
		--nsessions;
		return;
	}

	/*
	 * On MP systems, other processors should idle in get_smothered().
	 * Don't bother smothering the other cpus when single stepping.
	 */
	if (!step_was_expected && xc_initted && *xc_initted) {
		goto_kernel(0, 0, 0, X_CALL_HIPRI, ~(1 << cur_cpuid),
		    get_smothered, kxc_call);
	}

	/*
	 * For PSMI v1.2, the PSMI module gets notified that the kernel
	 * debugger is stopping normal processing for a while.
	 */
	if (!step_was_expected && psm_notifyf && *psm_notifyf)
		(**psm_notifyf)(PSM_DEBUG_ENTER);

	step_was_expected = 0;

	do {
		static int count = 0;

		claim_console();

		doswitch();

		release_console();

		if (dorun == 0) {
			if (count == 0)
				printf("\nkadb: fatal error in function cmd(): "
				    "looping forever...\n");
			++count;
			if ((count % 0x400000) == 0)
				printf(".");
		} else
			count = 0;
	} while (dorun == 0);

	step_expected = ((i_fp->r_efl & 0x100) != 0);
	if (step_expected) {
		if ((emul = emulated(i_fp)) == 1) {
			/* complete and successful emulation */
			i_fp->r_efl &= ~0x100;	/* clear trace bit */
			step_was_expected = 1;
			goto back_to_prompt;
		} else if (emul == 2) {
			/* instruction cannot be single-stepped */
			i_fp->r_efl &= ~0x100;
			if (bkptlookup(i_fp->r_eip) != 0)
				printf("The breakpoint at this instruction "
				    "must be removed ':d' before "
				    "continuing.\n");
			printf("You will have to type ':c' to continue "
			    "from here.\n");
			step_was_expected = 1;
			goto back_to_prompt;
		} else {
			/* go ahead and do the single-step */
			saved_int_bit = (i_fp->r_efl & 0x200);
			i_fp->r_efl &= ~0x200;
		}
	} else if (psm_notifyf && *psm_notifyf)
		/* notify PSMI module that kadb is returning to the kernel */
		(**psm_notifyf)(PSM_DEBUG_EXIT);

	--nsessions;

	hwbkpt = NULL;
}


#ifdef	DO_FAULT_TRACE
void
traceback(sp)
	caddr_t sp;
{

typedef struct {
				struct frame sf;
				int fr_arg[6];
				} trace_frame;

	register trace_frame *fp, *oldfp;
	register greg_t pc;
	extern int getbp(void);

	if ((int)sp & (STACK_ALIGN-1))
		printf("traceback: misaligned sp = %x\n", sp);

	fp = (sp != NULL) ? (trace_frame *)sp : (trace_frame *)getbp();
	printf("Begin traceback... sp = %x\n", sp);
	pc = 0;

	for (;;) {
		if (fp == (trace_frame *)fp->sf.fr_savfp) {
			printf("FP loop at %x\n", fp);
			break;
		}
		tryabort(1);
		if (pc != 0)
			printf("fp=%x: %x(%x,%x,%x,%x,%x,%x)\n",
			    fp, pc,
			    fp->fr_arg[0], fp->fr_arg[1], fp->fr_arg[2],
			    fp->fr_arg[3], fp->fr_arg[4], fp->fr_arg[5]);
		oldfp = fp;
		pc = fp->sf.fr_savpc;
		fp = (trace_frame *)fp->sf.fr_savfp;
		if (fp == 0 || ((char *)fp - (char *)oldfp > 0x2000))
			break;
	}
	printf("End traceback...\n");
}
#endif

/* a stub function called from common code */
struct bkpt *
tookwp()
{
	return (NULL);
}

/*
 * x86 hardware breakpoints differ from sun4u watchpoints in that
 * x86 hardware breakpoints stop the processor after the breakpoint
 * condition as opposed to just prior.  Because of this difference,
 * another hook is created here.
 */
struct bkpt *
tookhwbkpt(void)
{
	return (hwbkpt);
}

/* search for matching hw bkpt, and stash it away for above function */
void
set_hwbkpt(int rdr6)
{
	extern int dr0(), dr1(), dr2(), dr3();
	extern struct bkpt *bkpthead;
	struct bkpt *bp;
	addr_t loc;

	if (rdr6 & 1) loc = dr0();
	else if (rdr6 & 2) loc = dr1();
	else if (rdr6 & 4) loc = dr2();
	else if (rdr6 & 8) loc = dr3();

	for (bp = bkpthead; bp; bp = bp->nxtbkpt) {
		if (iswp(bp->type) && bp->loc == loc) {
			hwbkpt = bp;
			return;
		}
	}

	printf("Hardware breakpoint hit, but it was not identified?\n");
	hwbkpt = NULL;
}

void
setup_aux(void)
{
	extern struct bootops *bopp;
	extern char *mmulist;
	static char mmubuf[3 * OBP_MAXDRVNAME];
	int plen;

	mmulist = mmubuf;
	if (((plen = BOP_GETPROPLEN(bopp, "mmu-modlist")) > 0) &&
	    (plen < (3 * OBP_MAXDRVNAME)) &&
	    (BOP_GETPROP(bopp, "mmu-modlist", mmubuf) != -1))
		return;
	strcpy(mmubuf, "mmu32");	/* default to mmu32 */
}
