
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)archdep.c	1.56	99/11/20 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/vmparam.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/reg.h>
#include <sys/frame.h>
#include <sys/proc.h>
#include <sys/psw.h>
#include <sys/siginfo.h>
#include <sys/cpuvar.h>
#include <sys/asm_linkage.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/bootconf.h>
#include <sys/archsystm.h>
#include <sys/debug.h>
#include <sys/elf.h>
#include <sys/spl.h>
#include <sys/time.h>
#include <sys/atomic.h>
#include <sys/lockstat.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/modctl.h>
#include <sys/kobj.h>
#include <sys/panic.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <sys/fp.h>

extern struct bootops *bootops;

/*
 * Advertised via /etc/system.
 * Does execution of coff binaries bring in /usr/lib/cbcp.
 */
int enable_cbcp = 1;

/*
 * Set floating-point registers.
 */
void
setfpregs(klwp_id_t lwp, fpregset_t *fp)
{
	struct pcb *pcb;
	fpregset_t *pfp;

	pcb = &lwp->lwp_pcb;
	pfp = &pcb->pcb_fpu.fpu_regs;

	if (pcb->pcb_fpu.fpu_flags & FPU_EN) {
		if (!(pcb->pcb_fpu.fpu_flags & FPU_VALID)) {
			/*
			 * FPU context is still active, release the
			 * ownership.
			 */
			fp_free(&pcb->pcb_fpu, 0);
		}
		(void) kcopy(fp, pfp, sizeof (struct fpu));
		pcb->pcb_fpu.fpu_flags |= FPU_VALID;
		/*
		 * If we are changing the fpu_flags in the current context,
		 * disable floating point (turn on CR0_TS bit) to track
		 * FPU_VALID after clearing any errors (frstor chokes
		 * otherwise)
		 */
		if (lwp == ttolwp(curthread)) {
			(void) fperr_reset();
			fpdisable();
		}
	}
}

/*
 * Get floating-point registers.  The u-block is mapped in here (not by
 * the caller).
 */
void
getfpregs(klwp_id_t lwp, fpregset_t *fp)
{
	register fpregset_t *pfp;
	register struct pcb *pcb;
	extern int fpu_exists;

	pcb = &lwp->lwp_pcb;
	pfp = &pcb->pcb_fpu.fpu_regs;

	kpreempt_disable();
	if (pcb->pcb_fpu.fpu_flags & FPU_EN) {
		/*
		 * If we have FPU hw and the thread's pcb doesn't have
		 * a valid FPU state then get the state from the hw.
		 */
		if (fpu_exists && ttolwp(curthread) == lwp &&
		    !(pcb->pcb_fpu.fpu_flags & FPU_VALID))
			fp_save(&pcb->pcb_fpu); /* get the current FPU state */
		(void) kcopy(pfp, fp, sizeof (struct fpu));
	}
	kpreempt_enable();
}

/*
 * Return the general registers
 */
void
getgregs(klwp_id_t lwp, gregset_t rp)
{
	register greg_t *reg;

	reg = (greg_t *)lwp->lwp_regs;
	bcopy(reg, rp, sizeof (gregset_t));
}

/*
 * Return the user-level PC.
 * If in a system call, return the address of the syscall trap.
 */
greg_t
getuserpc()
{
	greg_t eip = lwptoregs(ttolwp(curthread))->r_eip;
	if (curthread->t_sysnum)
		eip -= 7;	/* size of an lcall instruction */
	return (eip);
}

/*
 * Protect segment registers from non-user privilege levels and GDT selectors
 * other than FPESEL.  If the segment selector is non-null and not FPESEL, we
 * make sure that the TI is set to LDT and that the RPL is set to 3.  Since
 * struct regs stores each 16-bit segment register as a 32-bit greg_t, we
 * also explicitly zero the top 16 bits since they may be coming from the
 * user's address space via setcontext(2) or /proc.
 */
static greg_t
check_segreg(greg_t sr)
{
	sr &= 0xffff;		/* Make sure top-16 bits are always zero */

	if (sr != 0 && sr != FPESEL)
		sr |= 7;	/* TI=LDT and RPL=3 */

	return (sr);
}

/*
 * Set general registers.
 */
void
setgregs(klwp_id_t lwp, gregset_t rp)
{
	struct regs *reg = lwptoregs(lwp);

	/*
	 * Only certain bits of the EFL can be modified.
	 */
	rp[EFL] = (reg->r_efl & ~PSL_USERMASK) | (rp[EFL] & PSL_USERMASK);

	/*
	 * Copy saved registers from user stack.
	 */
	bcopy(rp, reg, sizeof (gregset_t));

	/*
	 * Check segment selector settings.
	 */
	reg->r_fs = check_segreg(reg->r_fs);
	reg->r_gs = check_segreg(reg->r_gs);
	reg->r_ss = check_segreg(reg->r_ss);
	reg->r_cs = check_segreg(reg->r_cs);
	reg->r_ds = check_segreg(reg->r_ds);
	reg->r_es = check_segreg(reg->r_es);
}

/*
 * Determine whether eip is likely to have an interrupt frame
 * on the stack.  We do this by comparing the address to the
 * range of addresses spanned by several well-known routines.
 */
extern void _interrupt();
extern void _sys_call();
extern void _cmntrap();

extern size_t _interrupt_size;
extern size_t _sys_call_size;
extern size_t _cmntrap_size;

#define	INTR_FRAME(eip)	\
	((eip) - (uintptr_t)_interrupt < _interrupt_size ||	\
	(eip) - (uintptr_t)_sys_call < _sys_call_size ||	\
	(eip) - (uintptr_t)_cmntrap < _cmntrap_size)

/*
 * Get a pc-only stacktrace.  Used for kmem_alloc() buffer ownership tracking.
 * Returns MIN(current stack depth, pcstack_limit).
 */
int
getpcstack(uintptr_t *pcstack, int pcstack_limit)
{
	struct frame *fp = (struct frame *)getfp();
	struct frame *nextfp, *minfp, *stacktop;
	int depth = 0;
	int is_intr = 0;
	int on_intr;
	uintptr_t pc;

	if ((on_intr = CPU->cpu_on_intr) != 0)
		stacktop = (struct frame *)CPU->cpu_intr_stack + SA(MINFRAME);
	else
		stacktop = (struct frame *)curthread->t_stk;
	minfp = fp;

	while (depth < pcstack_limit) {
		if (is_intr) {
			struct regs *rp = (struct regs *)fp;
			nextfp = (struct frame *)rp->r_ebp;
			pc = rp->r_eip;
		} else {
			nextfp = (struct frame *)fp->fr_savfp;
			pc = fp->fr_savpc;
		}
		if (nextfp <= minfp || nextfp >= stacktop) {
			if (on_intr) {
				/*
				 * Hop from interrupt stack to thread stack.
				 */
				stacktop = (struct frame *)curthread->t_stk;
				minfp = (struct frame *)curthread->t_stkbase;
				on_intr = 0;
				continue;
			}
			break;
		}
		pcstack[depth++] = pc;
		is_intr = INTR_FRAME(pc);
		fp = nextfp;
		minfp = fp;
	}
	return (depth);
}

/*
 * The following ELF header fields are defined as processor-specific
 * in the V8 ABI:
 *
 *	e_ident[EI_DATA]	encoding of the processor-specific
 *				data in the object file
 *	e_machine		processor identification
 *	e_flags			processor-specific flags associated
 *				with the file
 */

/*
 * The value of at_flags reflects a platform's cpu module support.
 * at_flags is used to check for allowing a binary to execute and
 * is passed as the value of the AT_FLAGS auxiliary vector.
 */
int at_flags = 0;

/*
 * Check the processor-specific fields of an ELF header.
 *
 * returns 1 if the fields are valid, 0 otherwise
 */
/*ARGSUSED2*/
int
elfheadcheck(
	unsigned char e_data,
	Elf32_Half e_machine,
	Elf32_Word e_flags)
{
	if ((e_data != ELFDATA2LSB) || (e_machine != EM_386))
		return (0);
	return (1);
}

/*
 *	sync_icache() - this is called
 *	in proc/fs/prusrio.c. x86 has an unified cache and therefore
 *	this is a nop.
 */
/* ARGSUSED */
void
sync_icache(caddr_t addr, uint_t len)
{
	/* Do nothing for now */
}

int
__ipltospl(int ipl)
{
	return (ipltospl(ipl));
}

/*
 * Start and end events on behalf of the lockstat driver.
 * Until we have a cheap, reliable, lock-free timing source
 * on *all* x86 machines we'll have to make do with a
 * software approximation based on lbolt.
 */
int
lockstat_event_start(uintptr_t lp, ls_pend_t *lpp)
{
	if (casptr(&lpp->lp_lock, NULL, (void *)lp) == NULL) {
		lpp->lp_start_time = (hrtime_t)lbolt;
		return (0);
	}
	return (-1);
}

hrtime_t
lockstat_event_end(ls_pend_t *lpp)
{
	clock_t ticks = lbolt - (clock_t)lpp->lp_start_time;

	lpp->lp_lock = 0;
	if (ticks == 0)
		return (0);
	return (TICK_TO_NSEC((hrtime_t)ticks));
}


/*
 * These functions are not used on this architecture, but are
 * declared in common/sys/copyops.h.  Definitions are provided here
 * but they should never be called.
 */
/*ARGSUSED*/
int
default_fuword64(const void *addr, uint64_t *valuep)
{
	ASSERT(0);
	return (-1);
}

/*ARGSUSED*/
int
default_suword64(void *addr, uint64_t value)
{
	ASSERT(0);
	return (-1);
}

/*
 * The panic code invokes panic_saveregs() to record the contents of a
 * regs structure into the specified panic_data structure for debuggers.
 */
void
panic_saveregs(panic_data_t *pdp, struct regs *rp)
{
	panic_nv_t *pnv = PANICNVGET(pdp);

	struct cregs	creg;

	PANICNVADD(pnv, "gs", (uint32_t)rp->r_gs);
	PANICNVADD(pnv, "fs", (uint32_t)rp->r_fs);
	PANICNVADD(pnv, "es", (uint32_t)rp->r_es);
	PANICNVADD(pnv, "ds", (uint32_t)rp->r_ds);
	PANICNVADD(pnv, "edi", (uint32_t)rp->r_edi);
	PANICNVADD(pnv, "esi", (uint32_t)rp->r_esi);
	PANICNVADD(pnv, "ebp", (uint32_t)rp->r_ebp);
	PANICNVADD(pnv, "esp", (uint32_t)rp->r_esp);
	PANICNVADD(pnv, "ebx", (uint32_t)rp->r_ebx);
	PANICNVADD(pnv, "edx", (uint32_t)rp->r_edx);
	PANICNVADD(pnv, "ecx", (uint32_t)rp->r_ecx);
	PANICNVADD(pnv, "eax", (uint32_t)rp->r_eax);
	PANICNVADD(pnv, "trapno", (uint32_t)rp->r_trapno);
	PANICNVADD(pnv, "err", (uint32_t)rp->r_err);
	PANICNVADD(pnv, "eip", (uint32_t)rp->r_eip);
	PANICNVADD(pnv, "cs", (uint32_t)rp->r_cs);
	PANICNVADD(pnv, "eflags", (uint32_t)rp->r_efl);
	PANICNVADD(pnv, "uesp", (uint32_t)rp->r_uesp);
	PANICNVADD(pnv, "ss", (uint32_t)rp->r_ss);

	getcregs(&creg);

	PANICNVADD(pnv, "gdt", creg.cr_gdt);
	PANICNVADD(pnv, "idt", creg.cr_idt);
	PANICNVADD(pnv, "ldt", creg.cr_ldt);
	PANICNVADD(pnv, "task", creg.cr_task);
	PANICNVADD(pnv, "cr0", creg.cr_cr0);
	PANICNVADD(pnv, "cr2", creg.cr_cr2);
	PANICNVADD(pnv, "cr3", creg.cr_cr3);
	if (creg.cr_cr4)
		PANICNVADD(pnv, "cr4", creg.cr_cr4);

	PANICNVSET(pdp, pnv);
}

/*
 * Given a return address (%eip), determine the likely number of arguments
 * that were pushed on the stack prior to its execution.  We do this by
 * expecting that a typical call sequence consists of pushing arguments on
 * the stack, executing a call instruction, and then performing an add
 * on %esp to restore it to the value prior to pushing the arguments for
 * the call.  We attempt to detect such an add, and divide the addend
 * by the size of a word to determine the number of pushed arguments.
 */
static ulong_t
argcount(uintptr_t eip)
{
	const uint8_t *ins = (const uint8_t *)eip;
	ulong_t n;

	enum {
		M_MODRM_ESP = 0xc4,	/* Mod/RM byte indicates %esp */
		M_ADD_IMM32 = 0x81,	/* ADD imm32 to r/m32 */
		M_ADD_IMM8  = 0x83	/* ADD imm8 to r/m32 */
	};

	if (eip < KERNELBASE || ins[1] != M_MODRM_ESP)
		return (0);

	switch (ins[0]) {
	case M_ADD_IMM32:
		n = ins[2] + (ins[3] << 8) + (ins[4] << 16) + (ins[5] << 24);
		break;

	case M_ADD_IMM8:
		n = ins[2];
		break;

	default:
		n = 0;
	}

	return (n / sizeof (long));
}

/*
 * Print a stack backtrace using the specified frame pointer.  We delay two
 * seconds before continuing, unless this is the panic traceback.  Note
 * that the frame for the starting stack pointer value is omitted because
 * the corresponding %eip is not known.
 */
void
traceback(caddr_t ebp)
{

	struct frame *stacktop = (struct frame *)curthread->t_stk;
	struct frame *prevfp = (struct frame *)KERNELBASE;
	struct frame *fp = (struct frame *)ebp;

	enum { TR_ARG_MAX = 8 };	/* Upper bound on printed args */
	char args[TR_ARG_MAX * 10];	/* TR_ARG_MAX * (8 digits + ", ") */

	int is_intr = 0;
	ulong_t off;
	char *sym;

	if (!panicstr)
		printf("traceback: %%ebp = %p\n", (void *)ebp);

	while (fp > prevfp && fp < stacktop) {
		struct frame *nextfp;
		uintptr_t eip;
		ulong_t argc;
		long *argv;

		/*
		 * If this is an interrupt frame (based on the last %eip),
		 * then fp is pointing at a regs structure saved to the stack.
		 */
		if (is_intr) {
			struct regs *rp = (struct regs *)fp;
			nextfp = (struct frame *)rp->r_ebp;
			eip = rp->r_eip;
		} else {
			nextfp = (struct frame *)fp->fr_savfp;
			eip = fp->fr_savpc;
		}

		if (nextfp <= fp || nextfp >= stacktop)
			break; /* Stop if we're outside of the expected range */

		if ((uintptr_t)nextfp & (STACK_ALIGN - 1)) {
			printf("  >> mis-aligned %%ebp = %p\n", (void *)nextfp);
			break;
		}

		argc = argcount(eip);
		argc = MIN(argc, TR_ARG_MAX);
		args[0] = '\0';

		if (argc != 0) {
			char *p = args;

			argv = (long *)((char *)nextfp + sizeof (struct frame));
			p += snprintf(p, sizeof (args), "%lx", *argv++);

			for (argc--; argc != 0; argc--) {
				p += snprintf(p, sizeof (args),
				    ", %lx", *argv++);
			}
		}

		if ((sym = kobj_getsymname(eip, &off)) != NULL) {
			printf("%08lx %s:%s+%lx (%s)\n", (uintptr_t)nextfp,
			    mod_containing_pc((caddr_t)eip), sym, off, args);
		} else {
			printf("%08lx %lx (%s)\n",
			    (uintptr_t)nextfp, eip, args);
		}

		is_intr = INTR_FRAME(eip);
		prevfp = fp;
		fp = nextfp;
	}

	if (!panicstr) {
		printf("end of traceback\n");
		DELAY(2 * MICROSEC);
	}
}

/*
 * Generate a stack backtrace from a saved register set.
 */
void
traceregs(struct regs *rp)
{
	traceback((caddr_t)rp->r_ebp);
}

/*
 * Generate a stack backtrace from the current location.
 */
void
tracedump(void)
{
	traceback((caddr_t)getfp());
}

void
exec_set_sp(size_t stksize)
{
	klwp_t *lwp = ttolwp(curthread);

	lwptoregs(lwp)->r_uesp = (uintptr_t)curproc->p_usrstack - stksize;
}
